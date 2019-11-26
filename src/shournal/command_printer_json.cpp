
#include <QJsonObject>
#include <QJsonDocument>

#include "command_printer_json.h"
#include "command_query_iterator.h"
#include "logger.h"
#include "util.h"


void CommandPrinterJson::printCommandInfosEvtlRestore(std::unique_ptr<CommandQueryIterator> &cmdIter)
{
    QTextStream outstream(&m_outputFile);
    {
        QJsonObject header;
        header["pathToReadFiles"] = StoredFiles::getReadFilesDir();
        QJsonDocument doc(header);
        outstream << "HEADER:" << doc.toJson(QJsonDocument::Compact) << "\n";
    }

    while(cmdIter->next()){
        QJsonObject cmdObject;
        cmdIter->value().write(cmdObject);
        QJsonDocument doc(cmdObject);
        outstream << "COMMAND:" << doc.toJson(QJsonDocument::Compact) << "\n";

        if(! m_restoreReadFiles){
            continue;
        }
        for(const auto& readInfo : cmdIter->value().fileReadInfos){
            if(readInfo.isStoredToDisk){
                createRestoreTopleveDirIfNeeded();
                restoreReadFile_safe(readInfo, QString::number(cmdIter->value().idInDb));
            }
        }
    }

    {
        QJsonObject footer;
        footer["restorePath"] =QJsonValue::fromVariant(
                    (m_countOfRestoredFiles == 0) ? QVariant() : m_restoreDir.absolutePath() );
        footer["countOfRestoredFiles"] = m_countOfRestoredFiles;
        QJsonDocument doc(footer);
        outstream << "FOOTER:" << doc.toJson(QJsonDocument::Compact) << "\n";
    }
}
