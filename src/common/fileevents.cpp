#include "fileevents.h"

#include <sys/stat.h>
#include <cassert>

#include "stdiocpp.h"
#include "strlight.h"
#include "os.h"
#include "osutil.h"
#include "logger.h"
#include "user_kernerl.h"




int FileEvent::flags() const
{
    return m_close_event.flags;
}

uint64_t FileEvent::mtime() const
{
    return m_close_event.mtime;
}

size_t FileEvent::size() const
{
    return m_close_event.size;
}

uint64_t FileEvent::mode() const
{
    return m_close_event.mode;
}

HashValue FileEvent::hash() const
{
    return (m_close_event.hash_is_null)
            ? HashValue()
            : HashValue(m_close_event.hash);
}

off_t FileEvent::fileContentSize() const
{
    return m_close_event.bytes;
}

off_t FileEvent::fileContentStart() const
{
    return m_fileContentStart;
}

const char *FileEvent::path() const
{
    return m_path.data();
}

FILE *FileEvent::file() const
{
    return m_file;
}

void FileEvent::setPath(const char *path)
{
    m_path = path;
}


/////////////////////////////////////////////////////////////////////////////////////



/// Read null-terminated c-string from file into buf, return len
static int freadCstring(FILE* file, char* buf){
    int c;
    int pathIdx = 0;
    while((c=stdiocpp::fgetc_unlocked(file)) != EOF){
        buf[pathIdx++] = char(c);
        if(c == '\0'){
            break;
        }
    }
    if(pathIdx == 0){
        throw QExcIo(QString("EOF reached without expected null-terminator for file %1")
                     .arg(osutil::findPathOfFd<QByteArray>(fileno(file)).constData()));
    }
    return pathIdx;
}

bool FileEvents::isReadEvent(int flags)
{
    switch (flags) {
    case O_RDONLY:
    case O_RDWR: return true;
    default: return false;
    }
}

bool FileEvents::isWriteEvent(int flags)
{
    switch (flags) {
    case O_WRONLY:
    case O_RDWR: return true;
    default: return false;
    }
}

FileEvents::FileEvents() :
    m_wbuf_lastReadDir(PATH_MAX, '\0'),
    m_wbuf_lastWrittenDir(PATH_MAX, '\0')
{

}

/// Write the file-event to the logfile. If storefd != -1 and
/// the file has a st_size greater than zero, the whole file is copied
/// as well.
void FileEvents::write(int flags, const StrLight &path,
                       const struct stat &st, HashValue hash, int storefd)
{
    bool isREvent =  isReadEvent(flags);

    m_eventTmp.flags = flags;
    m_eventTmp.mtime = st.st_mtime;
    m_eventTmp.size = st.st_size;
    m_eventTmp.mode = st.st_mode;

    m_eventTmp.hash = (hash.isNull()) ? 0  : hash.value();
    m_eventTmp.hash_is_null = hash.isNull();

    m_eventTmp.bytes = (storefd == -1) ? 0 : st.st_size;
    auto oldOffset = stdiocpp::ftell(m_file);
    stdiocpp::fwrite_unlocked(&m_eventTmp , sizeof(m_eventTmp), 1, m_file );

    if(m_eventTmp.bytes > 0){
        assert(os::ltell(storefd) == 0);
        int targetfd = fileno_unlocked(m_file);
        // kernel-copy has no idea of our buffer - flush it
        stdiocpp::fflush(m_file);
        auto sent = os::sendfile(targetfd, storefd, m_eventTmp.bytes);
        stdiocpp::fseek(m_file, os::ltell(targetfd), SEEK_SET);

        if(sent != off_t(m_eventTmp.bytes)){
            // should happpen very rarely - seek back and correct file size
            logInfo << qtr("Could only collect %1 of %2 bytes for file %3")
                       .arg(sent).arg(m_eventTmp.bytes)
                       .arg(osutil::findPathOfFd<QByteArray>(storefd).constData());

            m_eventTmp.bytes = sent;
            stdiocpp::fseek(m_file, oldOffset, SEEK_SET);
            stdiocpp::fwrite_unlocked(&m_eventTmp , sizeof(m_eventTmp), 1, m_file );
            stdiocpp::fseek(m_file, 0, SEEK_END);
        }
        if(isREvent){
            m_rStoredFilesCount++;
        }
        if(isWriteEvent(flags)){
            m_wStoredFilesCount++;
        }
    }

     writeFilenameToFile(path, isREvent);

     if(isREvent){
         m_rEventCount++;
     } else {
         m_wEventCount++;
     }
}

void FileEvents::incrementDropCount(int eventType)
{
    switch (eventType) {
    case O_RDONLY: m_rDroppedCount++; break;
    case O_WRONLY: m_wDroppedCount++; break;
    default: throw QExcProgramming("bad event type: " +
                                   QString::number(eventType));
    }
}

void FileEvents::clear()
{
    stdiocpp::ftruncate_unlocked(m_file);
    m_rStoredFilesCount = 0;
    m_wStoredFilesCount = 0;
    m_rEventCount = 0;
    m_wEventCount = 0;
}



FileEvent *FileEvents::read()
{
    if(stdiocpp::fread_unlocked(&m_fileEvent.m_close_event,
                                sizeof(shournalk_close_event), 1, m_file) != 1){
        return nullptr;
    }
    if(m_fileEvent.fileContentSize() > 0){
        // remember offset where file content begins, the caller may use this
        m_fileEvent.m_fileContentStart = stdiocpp::ftell(m_file);
        stdiocpp::fseek(m_file, m_fileEvent.fileContentSize(), SEEK_CUR);
    }
    auto filename_len = freadCstring(m_file, m_pathTmp);

    // If last and current directory-path is equal, the producer
    // may have omitted it after the first time,
    // for read- and write-events respectively. In this case,
    // the path does not start with a '/'
    auto& lastDir = (isWriteEvent(m_fileEvent.m_close_event.flags))
            ? m_rbuf_lastWrittenDir : m_rbuf_lastReadDir;
    if(m_pathTmp[0] == '/'){
        m_fileEvent.m_path = QByteArray(m_pathTmp, filename_len);
        lastDir = splitAbsPath<QByteArray>(m_fileEvent.m_path).first;
    } else {
        // use dir-path from last time and append current filename
        m_fileEvent.m_path = pathJoinFilename(
                                lastDir,
                                QByteArray::fromRawData(m_pathTmp, filename_len));
    }
    return &m_fileEvent;
}

FILE *FileEvents::file() const
{
    return m_file;
}

void FileEvents::setFile(FILE *file)
{
    m_fileEvent.m_file = file;
    m_file = file;
}

uint FileEvents::wEventCount() const
{
    return m_wEventCount;
}

uint FileEvents::wDroppedCount() const
{
    return m_wDroppedCount;
}


uint FileEvents::rEventCount() const
{
    return m_rEventCount;
}

uint FileEvents::rDroppedCount() const
{
    return m_rDroppedCount;
}


uint FileEvents::rStoredFilesCount() const
{
    return m_rStoredFilesCount;
}

uint FileEvents::wStoredFilesCount() const
{
    return m_wStoredFilesCount;
}


void FileEvents::writeFilenameToFile(const StrLight &path, bool isREvent)
{
    auto & lastDir = (isREvent) ? m_wbuf_lastReadDir : m_wbuf_lastWrittenDir;
    const char* filename;
    size_t len_with_nul;
    int slashIdx = path.lastIndexOf('/');
    if(unlikely(slashIdx < 0)){
        throw QExcProgramming(QString("Invalid path %1").arg(path.c_str()));
    }

    if(unlikely(slashIdx == 0)){
        // a file written to the root directory.
        // KISS and always write full path.
        // size + 1 -> include nul.
        stdiocpp::fwrite_unlocked(path.c_str(), path.size()+ 1, 1, m_file );
        lastDir.resize(0); // invalidate last cached dir
        return;
    }

    if(slashIdx == int(lastDir.size())  &&
       memcmp(lastDir.constData(), path.constData(), slashIdx) == 0){
        // optimization: don't write full path but only filename (this
        // is later handled when reading back for read- and write-events
        // respectively).
        filename = path.c_str() + slashIdx + 1;
        len_with_nul = path.size() - slashIdx; // including nul
    } else {
        // write full path and remeber current directory for next time
        filename = path.c_str();
        len_with_nul = path.size() + 1; // including nul
        lastDir.resize(slashIdx);
        memcpy(lastDir.data(), path.c_str(), slashIdx);
    }
    stdiocpp::fwrite_unlocked(filename, len_with_nul, 1, m_file );
}






