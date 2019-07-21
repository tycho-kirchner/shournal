
#include <sys/ioctl.h>
#include <cstdio>
#include <unistd.h>

#include <QDebug>
#include <QDir>
#include <QStandardPaths>

#include "pretty_print.h"
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

PrettyPrint::PrettyPrint() :
    m_indentlvl0(""),
    m_indentlvl1("  "),
    m_indentlvl2("     "),
    m_indentlvl3("          "),
    m_maxCountOfReadFileLines(8),
    m_restoreReadFiles(false),
    m_countOfRestoredFiles(0),
    m_restoreDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation) +
                 QDir::separator() +
                 TrSnippets::instance().shournalRestore + "-" + os::getUserName<QString>())
{}

void PrettyPrint::printCommandInfosEvtlRestore(std::unique_ptr<CommandQueryIterator> &cmdIter)
{    
    struct winsize termWinSize;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &termWinSize);

    QFormattedStream s(stdout);

    s.setMaxLineWidth((termWinSize.ws_col > 5) ? termWinSize.ws_col : 80 );

    while(cmdIter->next()){
        s.setLineStart(m_indentlvl0);
        auto & cmd = cmdIter->value();
        s << qtr("cmd-id %1:").arg(cmd.idInDb);
        if(cmd.returnVal != CommandInfo::INVALID_RETURN_VAL){
            s << qtr("$?:") << QString::number(cmd.returnVal);
        }
        s << cmd.startTime.toString( Qt::DefaultLocaleShortDate) << "-"
          << cmd.endTime.toString( Qt::DefaultLocaleShortDate) << ": "
          << cmd.text << "\n";
        if(! cmd.sessionInfo.uuid.isNull()){
            s << qtr("session-uuid") << cmd.sessionInfo.uuid.toBase64() << "\n";
        }

        s.setLineStart(m_indentlvl1);
        if(! cmd.fileWriteInfos.isEmpty()){
            s << qtr("Written file(s):\n");
        }
        s.setLineStart(m_indentlvl2);
        for(const auto& f : cmd.fileWriteInfos){
            s << f.path  + QDir::separator() + f.name
              << "(" + m_userStrConv.bytesToHuman(f.size) + ")"
              << qtr("Hash:") << ((f.hash.isNull()) ? "-" : QString::number(f.hash.value()))
              << "\n";
        }
        s.setLineStart(m_indentlvl1);
        if(! cmd.fileReadInfos.isEmpty()){
            s << qtr("Read file(s):\n");
        }
        const QString cmdIdStr = QString::number(cmd.idInDb);
        for(const auto & f : cmd.fileReadInfos){
            printReadFileEventEvtlRestore(s, f, cmdIdStr);
        }
    }
    s.setLineStart(m_indentlvl0);
    if(m_countOfRestoredFiles > 0){
        s << qtr("%1 file(s) restored at %2").arg(m_countOfRestoredFiles)
             .arg(m_restoreDir.absolutePath()) << "\n";
    }
}


void PrettyPrint::printReadFileEventEvtlRestore(QFormattedStream& s, const FileReadInfo& readInfo,
                                           const QString &cmdIdStr){
    s.setLineStart(m_indentlvl2);
    s << readInfo.path  + QDir::separator() + readInfo.name
      << "(" + m_userStrConv.bytesToHuman(readInfo.size) + ")" << "id" << QString::number(readInfo.idInDb) <<  + "\n";

    QFileThrow f(StoredFiles::getReadFilesDir() + QDir::separator() + QString::number(readInfo.idInDb));

    try {
        f.open(QFile::OpenModeFlag::ReadOnly);
        auto mtype = m_mimedb.mimeTypeForData(&f);
        s.setLineStart(m_indentlvl3);
        if(! mtype.inherits("text/plain")){
            s << qtr("Not printing content (mimetype %1)").arg(mtype.name()) << "\n";
            return;
        }
        printReadFile(s, f);

        if(m_restoreReadFiles){
            restoreReadFile(readInfo, cmdIdStr, f);
        }

    } catch (const QExcIo& e) {
        logWarning << qtr("Error while printing read file %1 with id %2: %3")
                      .arg(readInfo.name).arg(readInfo.idInDb).arg(e.descrip());
        return;
    }
}

void PrettyPrint::restoreReadFile(const FileReadInfo &readInfo, const QString &cmdIdStr,
                                  const QFile &openReadFile)
{
    if(m_countOfRestoredFiles == 0){
        // initially create restore dir
        if(! m_restoreDir.mkpath(m_restoreDir.absolutePath())){
            throw QExcIo(qtr("Failed to the create read-files restore directory at %1")
                                 .arg(m_restoreDir.absolutePath()));
        }
    }
    QDir fullDirPath(m_restoreDir.absoluteFilePath(qtr("command-id-") + cmdIdStr) + QDir::separator() +
                  readInfo.path);

    if(! fullDirPath.mkpath(fullDirPath.absolutePath())){
        throw QExcIo(qtr("Failed to create the read-files restore directory for command-id %1")
                             .arg(cmdIdStr));
    }

    const QString failMsg(qtr("Failed to restore read file with id %1:").arg(readInfo.idInDb));
    try {
        m_storedFiles.restoreReadFileAtDIr(readInfo, fullDirPath, openReadFile);
        ++m_countOfRestoredFiles;
    } catch (const os::ExcOs& e) {
        logWarning << failMsg << e.what();
    } catch(const QExcIo& e){
         logWarning << failMsg << e.descrip();
    }
}

void PrettyPrint::printReadFile(QFormattedStream &s, QFile &f)
{
    QTextStream fstream(&f);
    int nLinesPrinted = 0;
    while(! fstream.atEnd()){
        QString line = fstream.readLine();
        if(line.isEmpty()){
            continue;
        }
        s << line << "\n";
        if(++nLinesPrinted >= m_maxCountOfReadFileLines){
            s << "...\n";
            break;
        }
    }
}

void PrettyPrint::setRestoreDir(const QDir &restoreDir)
{
    m_restoreDir = restoreDir;
}

void PrettyPrint::setRestoreReadFiles(bool restoreReadFiles)
{
    m_restoreReadFiles = restoreReadFiles;
}

/// Do not print more than that number of lines for each read file
void PrettyPrint::setMaxCountOfReadFileLines(int maxCountOfReadFileLines)
{
    m_maxCountOfReadFileLines = maxCountOfReadFileLines;
}
