#include "command_printer_human.h"

#include <sys/ioctl.h>
#include <cstdio>
#include <unistd.h>

#include <QDebug>
#include <QDir>
#include <QStandardPaths>
#include <QJsonDocument>

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
#include "qoutstream.h"
#include "cmd_stats.h"
#include "commandinfo.h"


void CommandPrinterHuman::printCommandInfosEvtlRestore(std::unique_ptr<CommandQueryIterator> &cmdIter)
{
    if( cmdIter->computeSize() == 0){
        QOut() << qtr("No results found matching the query.\n");
        return;
    }
    if(! m_outputFile.isOpen()){
        m_outputFile.open(QFile::OpenModeFlag::WriteOnly);
    }

    struct winsize termWinSize{};
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &termWinSize);

    QFormattedStream s(&m_outputFile);

    s.setMaxLineWidth((termWinSize.ws_col > 5) ? termWinSize.ws_col : 80 );

    CmdStats cmdStats;
    while(cmdIter->next()){
        cmdStats.collectCmd(cmdIter->value());
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

        printWriteInfos(s, cmd.fileWriteInfos);
        printReadInfos(s, cmd);
    }

    cmdStats.eval();

    // statistics with fewer elements is boring..
    const int MIN_STATS_COUNT = 4;
    if(cmdStats.cmdsWithMostFileMods().size() > MIN_STATS_COUNT){
        s.setLineStart(m_indentlvl0);
        s << qtr("\nCommands with most file modifications:\n");
        s.setLineStart(m_indentlvl1);
        for(const auto& e : cmdStats.cmdsWithMostFileMods()){
            s << qtr("cmd-id %1 modified %2 file(s) - %3\n")
                 .arg(e.idInDb).arg(e.countOfFileMods).arg(e.cmdTxt);
        }

    }

    if(cmdStats.sessionMostCmds().size() > MIN_STATS_COUNT){
        s.setLineStart(m_indentlvl0);
        s << qtr("\nSessions with most commands:\n");
        s.setLineStart(m_indentlvl1);
        for(const auto& e : cmdStats.sessionMostCmds()){
            s << qtr("session-uuid %1 - %2 command(s)\n")
                 .arg(e.cmdUuid.toBase64().data()).arg(e.cmdCount);
        }
    }

    if(cmdStats.cwdCmdCounts().size() > MIN_STATS_COUNT){
        s.setLineStart(m_indentlvl0);
        s << qtr("\nWorking directories with most commands:\n");
        s.setLineStart(m_indentlvl1);
        for(const auto& e : cmdStats.cwdCmdCounts()){
            s << qtr("%1 command(s) at %2\n")
                 .arg(e.cmdCount).arg(e.workingDir);
        }
    }

    if(cmdStats.dirIoCounts().size() > MIN_STATS_COUNT){
        s.setLineStart(m_indentlvl0);
        s << qtr("\nDirectories with most input/output-activity:\n");
        s.setLineStart(m_indentlvl1);
        for(const auto& e : cmdStats.dirIoCounts()){
            s << qtr("Total %1 (%2 written, %3 read) files at %4\n")
                 .arg(e.writeCount + e.readCount).arg(e.writeCount)
                 .arg(e.readCount).arg(e.dir);
        }
    }

    if(m_countOfRestoredFiles > 0){
        s.setLineStart(m_indentlvl0);
        s << qtr("%1 file(s) restored at %2").arg(m_countOfRestoredFiles)
             .arg(m_restoreDir.absolutePath()) << "\n";
    }
}


void
CommandPrinterHuman::printReadFileEventEvtlRestore(QFormattedStream& s,
                                                   const FileReadInfo& readInfo,
                                                   const QString &cmdIdStr){
    s.setLineStart(m_indentlvl2);
    s << readInfo.path  + QDir::separator() + readInfo.name
      << "(" + m_userStrConv.bytesToHuman(readInfo.size) + ")" << "id" << QString::number(readInfo.idInDb) <<  + "\n";
    if(! readInfo.isStoredToDisk){
        // since shournal 2.1 it is possible to log only meta-information about
        // read files without storing them in the read files dir.
        return;
    }
    if(m_restoreReadFiles){
        createRestoreTopleveDirIfNeeded();
    }

    bool printFileContentSuccess {false};
    QFileThrow f(m_storedFiles.mkPathStringToStoredReadFile(readInfo));
    try {
        f.open(QFile::OpenModeFlag::ReadOnly);
        auto mtype = m_mimedb.mimeTypeForData(&f);
        s.setLineStart(m_indentlvl3);
        if(! mtype.inherits("text/plain")){
            s << qtr("Not printing content (mimetype %1)").arg(mtype.name()) << "\n";
            return;
        }
        printReadFile(s, f);
        printFileContentSuccess = true;

        if(m_restoreReadFiles){
            restoreReadFile_safe(readInfo, cmdIdStr, f);
        }
    } catch (const QExcIo& e) {
        if(printFileContentSuccess){
            logWarning << e.what();
        } else {
            logWarning << qtr("Error while printing read file '%1' with id %2: %3")
                          .arg(readInfo.name).arg(readInfo.idInDb).arg(e.descrip());
        }
        return;
    }
}


void CommandPrinterHuman::printReadFile(QFormattedStream &s, QFile &f)
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

void CommandPrinterHuman::printWriteInfos(QFormattedStream &s, const FileWriteInfos &fileWriteInfos)
{
    if(! fileWriteInfos.isEmpty()){
        s.setLineStart(m_indentlvl1);
        s << qtr("%1 written file(s):\n").arg(fileWriteInfos.size());
        s.setLineStart(m_indentlvl2);
        int counter = 0;
        for(const auto& f : fileWriteInfos){
            if(counter >= m_maxCountWfiles){
                if(counter > 0){
                    s << qtr("... and %1 more files.\n")
                         .arg(fileWriteInfos.size() - m_maxCountWfiles);
                }
                break;
            }
            s << f.path  + QDir::separator() + f.name
              << "(" + m_userStrConv.bytesToHuman(f.size) + ")"
              << qtr("Hash:") << ((f.hash.isNull()) ? "-" : QString::number(f.hash.value()))
              << "\n";
            ++counter;
        }
    }
}

void CommandPrinterHuman::printReadInfos(QFormattedStream &s, const CommandInfo &cmd)
{
    s.setLineStart(m_indentlvl1);
    if(! cmd.fileReadInfos.isEmpty()){
        s << qtr("%1 read file(s):\n").arg(cmd.fileReadInfos.size());
        const QString cmdIdStr = QString::number(cmd.idInDb);
        int counter = 0;
        for(const auto & f : cmd.fileReadInfos){
            if(counter >= m_maxCountRfiles){
                if(counter > 0){
                    s << qtr("... and %1 more files.\n")
                            .arg(cmd.fileReadInfos.size() - m_maxCountRfiles);
                }
                break;
            }
            printReadFileEventEvtlRestore(s, f, cmdIdStr);
            ++counter;
        }
    }

}



/// Do not print more than that number of lines for each read file
void CommandPrinterHuman::setMaxCountOfReadFileLines(int maxCountOfReadFileLines)
{
    m_maxCountOfReadFileLines = maxCountOfReadFileLines;
}
