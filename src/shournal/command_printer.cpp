
#include <sys/ioctl.h>
#include <cstdio>
#include <unistd.h>

#include <QDebug>
#include <QDir>
#include <QStandardPaths>

#include "command_printer.h"
#include "qformattedstream.h"
#include "util.h"
#include "qfilethrow.h"
#include "db_controller.h"
#include "logger.h"
#include "file_query_helper.h"
#include "excos.h"
#include "os.h"
#include "translation.h"

using translation::TrSnippets;

CommandPrinter::CommandPrinter() :
    m_restoreDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation) +
                 QDir::separator() +
                 TrSnippets::instance().shournalRestore + "-" + os::getUserName<QString>())
{}

void CommandPrinter::createRestoreTopleveDirIfNeeded()
{
    if(m_countOfRestoredFiles == 0){
        // initially create restore dir
        if(! m_restoreDir.mkpath(m_restoreDir.absolutePath())){
            throw QExcIo(qtr("Failed to the create top-level read-files restore directory at %1")
                                 .arg(m_restoreDir.absolutePath()), false);
        }
    }
}

void CommandPrinter::restoreReadFile_safe(const FileReadInfo &readInfo, const QString &cmdIdStr)
{
    QFileThrow f(m_storedFiles.mkPathStringToStoredReadFile(readInfo));
    f.open(QFile::ReadOnly);
    restoreReadFile_safe(readInfo, cmdIdStr, f);
}


void CommandPrinter::restoreReadFile_safe(const FileReadInfo &readInfo, const QString &cmdIdStr,
                                  const QFile &openReadFile)
{  
    QDir fullDirPath(m_restoreDir.absoluteFilePath(qtr("command-id-") + cmdIdStr) + QDir::separator() +
                  readInfo.path);

    const QString failMsg(qtr("Failed to restore read file with id %1:").arg(readInfo.idInDb));
    try {
        if(! fullDirPath.mkpath(fullDirPath.absolutePath())){
            throw QExcIo(qtr("Failed to create the read-files restore directory for command-id %1")
                                 .arg(cmdIdStr));
        }
        m_storedFiles.restoreReadFileAtDIr(readInfo, fullDirPath, openReadFile);
        ++m_countOfRestoredFiles;
    } catch (const os::ExcOs& e) {
        logWarning << failMsg << e.what();
    } catch(const QExcIo& e){
         logWarning << failMsg << e.descrip();
    }
}


void CommandPrinter::setRestoreDir(const QDir &restoreDir)
{
    m_restoreDir = restoreDir;
}





void CommandPrinter::setRestoreReadFiles(bool restoreReadFiles)
{
    m_restoreReadFiles = restoreReadFiles;
}
