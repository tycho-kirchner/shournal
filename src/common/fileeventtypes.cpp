

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "fileeventtypes.h"
#include "os.h"
#include "logger.h"

#include "settings.h"
#include "stdiocpp.h"
#include "util_performance.h"
#include "cleanupresource.h"

using stdiocpp::QExcStdio;

const size_t FILE_BUF_SIZE = 1024*1024*5;

/// Read null-terminated c-string from file into buf
static void freadCstring(FILE* file, char* buf){
    int c;
    int pathIdx = 0;
    while((c=stdiocpp::fgetc_unlocked(file)) != EOF){
        buf[pathIdx++] = char(c);
        if(c == '\0'){
            break;
        }
    }
    if(pathIdx == 0){
        throw QExcProgramming("EOF reached without a final null-terminator");
    }
}


FileWriteEvents::FileWriteEvents(const char *parentTempDir) :
    m_file(nullptr)
{
    auto path = std::string(parentTempDir) + "/writtenFiles";

    m_file = stdiocpp::fopen(strDataAccess(path), "w+b");
    // warning: number of bytes seems to have no effect on
    // bufsize? What worked anyway was
    // char* buf = (char*)malloc(FILE_BUF_SIZE);   
    if(setvbuf(m_file, nullptr, _IOFBF, FILE_BUF_SIZE) != 0 ){
        throw QExcIo("setvbuf failed");
    }
    // valgrind likes initialized memory
    memset(&m_eventTmp, 0, sizeof (m_eventTmp));
    memset(&m_currentEvent, 0, sizeof (m_currentEvent));

}


FileWriteEvents::~FileWriteEvents()
{
    if(m_file != nullptr){
        if(ferror(m_file) != 0){
            logCritical << "IO-Error occurred on processing file-write-events. flags:"
                        << m_file->_flags;
        }
        try {
             stdiocpp::fclose(m_file);
        } catch (const std::exception& ex) {
            logCritical << ex.what();
        }
    }
}


void FileWriteEvents::write(const StrLight &path,
                            const struct stat &st, HashValue hash)
{
    m_eventTmp.mtime = st.st_mtime;
    m_eventTmp.size = st.st_size;

    m_eventTmp.hash = (hash.isNull()) ? 0  : hash.value();
    m_eventTmp.hashIsNull = hash.isNull();
    stdiocpp::fwrite_unlocked(&m_eventTmp , sizeof(m_eventTmp), 1, m_file );
    // string length varies, use the null-terminator
    // on later read (size + 1)
    stdiocpp::fwrite_unlocked(path.c_str(), path.size() + 1, 1, m_file );
}

FileWriteEvent *FileWriteEvents::read()
{
    if(! m_readAllowed){
        throw QExcProgramming("please call fseekToBegin first");
    }

    // FileWriteEventInternal and FileWriteEvent begin with the same structure
    if(stdiocpp::fread_unlocked(&m_currentEvent, sizeof (FileWriteEventInternal), 1, m_file) != 1){
        return nullptr;
    }
    freadCstring(m_file, m_currentEvent.fullPath);
    return &m_currentEvent;
}


void FileWriteEvents::clear()
{
    stdiocpp::ftruncate_unlocked(m_file);
}

void FileWriteEvents::fseekToBegin()
{
    stdiocpp::fseek(m_file, 0, SEEK_SET);
    m_readAllowed = true;
}


/////////////////////////////////////////////////////////////


FileReadEvents::FileReadEvents(const char *parentTempDir) :
    m_file(nullptr),
    m_tmpDirFd(os::open(parentTempDir, O_RDONLY | O_DIRECTORY)),
    m_tmpDirPath(parentTempDir),
    m_storedFilesCounter(0)
{
    auto path = std::string(parentTempDir) + "/readFiles";

    m_file = stdiocpp::fopen(strDataAccess(path), "w+b");
    // warning: number of bytes seems to have no effect on
    // bufsize? What worked anyway was
    // char* buf = (char*)malloc(FILE_BUF_SIZE);
    if(setvbuf(m_file, nullptr, _IOFBF, FILE_BUF_SIZE) != 0 ){
        throw QExcIo("setvbuf failed");
    }
    // valgrind likes initialized memory
    memset(&m_eventTmp, 0, sizeof (m_eventTmp));
    memset(&m_currentEvent, 0, sizeof (m_currentEvent));

}


FileReadEvents::~FileReadEvents()
{
    close(m_tmpDirFd);
    if(m_file != nullptr){
        if(ferror(m_file) != 0){
            logCritical << "IO-Error occurred on processing file-read-events. flags:"
                        << m_file->_flags;
        }
        try {
             stdiocpp::fclose(m_file);
        } catch (const std::exception& ex) {
            logCritical << ex.what();
        }
    }
}


/// @param fd: the file descriptor of this read event
/// @param store: if true, store the file within the cached dir
void FileReadEvents::write(const StrLight &path,
                                const struct stat &st, HashValue hash, int fd, bool store)
{
    m_eventTmp.mtime = st.st_mtime;
    m_eventTmp.size = st.st_size;
    m_eventTmp.mode = st.st_mode;
    m_eventTmp.hash = (hash.isNull()) ? 0  : hash.value();
    m_eventTmp.hashIsNull = hash.isNull();
    m_eventTmp.file_content_id = (store) ? storeFile(fd, st) : -1;
    stdiocpp::fwrite_unlocked(&m_eventTmp , sizeof(m_eventTmp), 1, m_file );
    // string length varies, use the null-terminator
    // on later read (size + 1)
    stdiocpp::fwrite_unlocked(path.c_str(), path.size() + 1, 1, m_file );
}

/// @return nullptr on EOF, else a pointer to the next event
FileReadEvent *FileReadEvents::read()
{
    if(! m_readAllowed){
        throw QExcProgramming("please call fseekToBegin first");
    }
    // FileWriteEventInternal and FileWriteEvent begin with the same structure
    if(stdiocpp::fread_unlocked(&m_currentEvent, sizeof (FileReadEventInternal), 1, m_file) != 1){
        return nullptr;
    }
    freadCstring(m_file, m_currentEvent.fullPath);
    return &m_currentEvent;

}

void FileReadEvents::fseekToBegin()
{
    stdiocpp::fseek(m_file, 0, SEEK_SET);
    m_readAllowed = true;
}


void FileReadEvents::clear()
{
    stdiocpp::ftruncate_unlocked(m_file);
    for(uint i=0; i < m_storedFilesCounter; i++){
        auto storeName = makeStoredFilename(i);
        try {
            os::unlinkat(m_tmpDirFd, strDataAccess(storeName), 0);
        } catch (const os::ExcOs & ex) {
            // If shournal's db and the tempdir are on the same filesystem
            // the file may already be moved away, so ignore ENOENT
            if(ex.errorNumber() != ENOENT){
                throw ;
            }
        }
    }
    m_storedFilesCounter = 0;
}

QByteArray FileReadEvents::makeStoredFilepath(const FileReadEvent &e)
{
    assert(e.file_content_id != -1);
    return m_tmpDirPath + '/' + makeStoredFilename(e.file_content_id) ;
}

uint FileReadEvents::getStoredFilesCounter() const {
    return m_storedFilesCounter;
}

const QByteArray &FileReadEvents::getTmpDirPath() const    {
    return m_tmpDirPath;
}


QByteArray FileReadEvents::makeStoredFilename(int number)
{
    auto storeName = "readfile" + QByteArray::number(number);
    return storeName;
}

/// Store fd in our cached directory
/// @return the numeric id of the cached file
uint FileReadEvents::storeFile(int fd, const struct stat &st)
{
    auto storeName = makeStoredFilename(m_storedFilesCounter);
    int storeFd = os::openat(m_tmpDirFd, storeName, O_WRONLY | O_CREAT | O_EXCL);
    auto closeStoreFd = finally([&storeFd] { close(storeFd); });
    os::sendfile(storeFd, fd, st.st_size);
    auto oldCounter = m_storedFilesCounter;
    ++m_storedFilesCounter;
    return oldCounter;
}

