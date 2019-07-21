#include <cassert>

#include "storedfiles.h"
#include "db_connection.h"
#include "util.h"
#include "qfilethrow.h"
#include "os.h"

const QString& StoredFiles::getReadFilesDir()
{
    static const QString path = db_connection::getDatabaseDir() + "/readFiles";
    return path ;
}

/// creates path of stored files if not exist
/// @return the created path
/// @throws QExcIo
const QString& StoredFiles::mkpath()
{
    const auto & p = getReadFilesDir();
    if( ! QDir(p).mkpath(p)){
        throw QExcIo(qtr("Failed to the create directory for the stored read files at %1")
                             .arg(p));
    }
    return p;
}

StoredFiles::StoredFiles()
{
    const auto & path = getReadFilesDir();
    m_readFilesDir.setPath(path);
    this->mkpath();
}

bool StoredFiles::deleteReadFile(const QString &fname)
{
    return m_readFilesDir.remove(fname);
}

/// @throws QExcIo
void StoredFiles::addReadFile(const QString &fname, const QByteArray &data)
{
    const QString fPath = m_readFilesDir.absoluteFilePath(fname);
    QFileThrow f(fPath);
    try {
        f.open(QFile::OpenModeFlag::WriteOnly | QFile::OpenModeFlag::Truncate);
        f.write(data);
    } catch (const QExcIo&) {
        f.remove();
        throw ;
    }
}


/// @param info: the read file already loaded from the database
/// @param dir: the directory where to restore it (warning: override without confirmation)
/// @param openReadFileInDb: the for reading opened file corresponding to the info-database-entry.
void StoredFiles::restoreReadFileAtDIr(const FileReadInfo &info, const QDir& dir,
                                             const QFile &openReadFileInDb)
{
    assert(openReadFileInDb.isOpen());
    const QString filePath = dir.absoluteFilePath(info.name);
    QFileThrow dstFile(filePath);
    dstFile.open(QFile::OpenModeFlag::WriteOnly);
    os::sendfile(dstFile.handle(), openReadFileInDb.handle(), static_cast<size_t>(info.size));
    os::fchmod(dstFile.handle(), info.mode);
}


/// @overload
void StoredFiles::restoreReadFileAtDIr(const FileReadInfo &info, const QDir &dir)
{
    QFileThrow f(m_readFilesDir.absoluteFilePath(QString::number(info.idInDb)));
    f.open(QFile::OpenModeFlag::ReadOnly);
    restoreReadFileAtDIr(info, dir, f);
}
