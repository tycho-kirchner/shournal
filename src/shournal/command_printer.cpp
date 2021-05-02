
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

static QString buildRestorePath(){
    return
      pathJoinFilename(
        QStandardPaths::writableLocation(QStandardPaths::TempLocation),
        TrSnippets::instance().shournalRestore + "-" + os::getUserName<QString>()
        );
}


CommandPrinter::CommandPrinter() :
    m_restoreDir(buildRestorePath())
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
    QDir fullDirPath(
           pathJoinFilename(m_restoreDir.absoluteFilePath(qtr("command-id-") + cmdIdStr)
                            ,readInfo.path));

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

/// Do not output statistics, if less than 'val' entries
void CommandPrinter::setMinCountOfStats(int val)
{
    m_minCountOfStats = val;
}

void CommandPrinter::setMaxCountRfiles(int maxCountRfiles)
{
    m_maxCountRfiles = maxCountRfiles;
}

CmdStats &CommandPrinter::cmdStats()
{
    return m_cmdStats;
}

void CommandPrinter::setMaxCountWfiles(int maxCountWfiles)
{
    m_maxCountWfiles = maxCountWfiles;
}

void CommandPrinter::setQueryString(const QString &queryString)
{
    m_queryString = queryString;
}


void CommandPrinter::setRestoreDir(const QDir &restoreDir)
{
    m_restoreDir = restoreDir;
}

QFileThrow &CommandPrinter::outputFile()
{
    return m_outputFile;
}


void CommandPrinter::setRestoreReadFiles(bool restoreReadFiles)
{
    m_restoreReadFiles = restoreReadFiles;
}
