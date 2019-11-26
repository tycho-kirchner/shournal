
#include <cassert>

#include <unordered_set>
#include <QLinkedList>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QResource>
#include <QTemporaryFile>

#include "command_printer_html.h"
#include "command_query_iterator.h"
#include "logger.h"
#include "util.h"
#include "cleanupresource.h"
#include "stupidinject.h"
#include "qresource_helper.h"

using qresource_helper::data_safe;


void CommandPrinterHtml::printCommandInfosEvtlRestore(std::unique_ptr<CommandQueryIterator> &cmdIter)
{
    Q_INIT_RESOURCE(htmlexportres);
    QResource indexHtmlResource("://index.html");
    QByteArray html_content = data_safe(indexHtmlResource);

    QTextStream outstream(&m_outputFile);

    StupidInject inject;

    QTemporaryFile tmpCmdDataFile;
    tmpCmdDataFile.open();

    FileReadInfoSet_t readFileIdSet;

    inject.addInjection( "<script src=\"SAMPLE_DATA.js\"></script>",
                         [this, &cmdIter, &tmpCmdDataFile, &readFileIdSet](QTextStream& outstream){
        // json performance much better-> embed into html
        outstream << R"(<script id="commandJSON" type="application/json">)";

        outstream << "[";

        // commands are sorted by start-date, but the last started command
        // may end before the second to last (and so on).
        // So keep track of the final command end date
        QDateTime finalCommandEndDate = QDateTime::fromTime_t(0);
        const auto queryDate = QDateTime::currentDateTime();
        bool isFirst = true;
        while(cmdIter->next()){
            this->addScriptsToReadFilesSet((cmdIter->value().fileReadInfos), readFileIdSet);
            if(isFirst){
                isFirst = false;
            } else {
                // sepearte json objects by comma:
                outstream << ",";
            }

            this->processSingleCommand(outstream, cmdIter->value(), finalCommandEndDate,
                                       tmpCmdDataFile);
        }

        outstream << "]";

        outstream << "</script>\n";

        // store the rest as plain js:
        outstream << "<script>\n";

        outstream << "const ORIGINAL_QUERY = '" << m_queryString << "';\n";
        outstream << "const ORIGINAL_QUERY_DATE_STR = '"
                  << queryDate.toString(Conversions::dateIsoFormatWithMilliseconds()) << "';\n";

        outstream << "const CMD_FINAL_ENDDATE_STR = '"
                  << finalCommandEndDate.toString(Conversions::dateIsoFormatWithMilliseconds()) << "';\n";
        outstream << "</script>\n";
    });

    inject.addInjection("<script src=\"main.js\"></script>",
                        [this, &tmpCmdDataFile, &readFileIdSet](QTextStream& outstream){
        // currently the whole js is compiled into a single main.js file,
        // which we inject here for convenience.
        QResource mainJSResource("://main.js");
        QByteArray mainJsContent = data_safe(mainJSResource);

        outstream << "<script>";

        outstream << mainJsContent.data();

        outstream << "</script>\n";


        // write the cmd-data of the tempfile to html
        assert(tmpCmdDataFile.seek(0));

        QByteArray line;
        uint linecounter = 0;
        while(! (line = tmpCmdDataFile.readLine()).isEmpty()){
            // pop \n
            line.resize(line.size() - 1);
            outstream << "<script id=\"commandDataJSON" << linecounter
                      << R"(" type="application/json">)";
            outstream << line;

            outstream << "</script>\n";
            ++linecounter;
        }

        outstream << "\n<script>\n";

        // write the command-statistics arrays by giving the
        // indeces of the respective commands in the original
        // commands-array
        m_cmdStats.eval();
        writeStatistics(outstream);

        // finally write read files:
        writeReadFileContentsToHtml(outstream, readFileIdSet);

        outstream << "\n</script>\n";


    });

    inject.stream(html_content, outstream);
}


void CommandPrinterHtml::processSingleCommand(QTextStream& outstream,
                                              CommandInfo& cmd,
                                              QDateTime& finalCommandEndDate,
                                              QTemporaryFile &tmpCmdDataFile){

    m_cmdStats.collectCmd(cmd);

    if(cmd.startTime.msecsTo(cmd.endTime) < 1){
        // for the plot we need at least one millisecond time difference to draw a rect:
        cmd.endTime = cmd.endTime.addMSecs(1);
    }
    finalCommandEndDate = std::max(finalCommandEndDate, cmd.endTime);

    // to speed up loading of the html document (especially useful
    // for more than 2000 entries), we 'split' up the command
    // data, to first (quickly) render the session time-line and
    // load the rest afterwards. For the timeline and command-list, only id, start/end-date, uuid and
    // text are necessary.
    writeCmdStartup(cmd, outstream);

    // write the 'rest' to tempfile, line by line
    writeCmdData(cmd, tmpCmdDataFile);

}

void CommandPrinterHtml::writeCmdStartup(const CommandInfo &cmd, QTextStream &outstream)
{
    QJsonObject jsonCmdStartup;

    CmdJsonWriteCfg cmdJsonStartup(false);
    cmdJsonStartup.maxCountRFiles = m_maxCountRfiles;
    cmdJsonStartup.maxCountWFiles = m_maxCountWfiles;
    cmdJsonStartup.idInDb = true;
    cmdJsonStartup.startEndTime = true;
    cmdJsonStartup.sessionInfo = true;
    cmdJsonStartup.text = true;
    cmd.write(jsonCmdStartup, m_writeDatesWithMillisec, cmdJsonStartup);

    outstream << QJsonDocument(jsonCmdStartup).toJson(QJsonDocument::Compact);
}

void CommandPrinterHtml::writeCmdData(const CommandInfo &cmd,
                                      QTemporaryFile &tmpCmdDataFile)
{
    CmdJsonWriteCfg cmdJsonData(true);
    cmdJsonData.maxCountRFiles = m_maxCountRfiles;
    cmdJsonData.maxCountWFiles = m_maxCountWfiles;

    cmdJsonData.idInDb = false;
    cmdJsonData.startEndTime = false;
    cmdJsonData.sessionInfo = false;
    cmdJsonData.text = false;

    QJsonObject jsonCmdData;
    cmd.write(jsonCmdData, m_writeDatesWithMillisec, cmdJsonData);

    // since we may restrict the number of read/written files (to not generate
    // huge html-files), store the real number in any case:
    jsonCmdData["fileReadEvents_length"] = cmd.fileReadInfos.length();
    jsonCmdData["fileWriteEvents_length"] = cmd.fileWriteInfos.length();

    if(tmpCmdDataFile.write(QJsonDocument(jsonCmdData).toJson(QJsonDocument::Compact)) == -1){
        throw QExcIo("Failed to write cmdData to tmpfile: " +  tmpCmdDataFile.errorString());
    }
    tmpCmdDataFile.write("\n");
}


void CommandPrinterHtml::addScriptsToReadFilesSet(const FileReadInfos &infos,
                                                  FileReadInfoSet_t &set)
{
    int counter = 0;
    for(const auto& info : infos){
        if(info.isStoredToDisk){
            // don't check mimetype here, to avoid performing it multiple times
            // for the same script-id
            set.insert(info.idInDb);
        }
        ++counter;
        if(counter > m_maxCountRfiles){
            break;
        }
    }
}

void CommandPrinterHtml::writeReadFileContentsToHtml(QTextStream &outstream,
                                                     FileReadInfoSet_t &readFileIdSet)
{
    // uniquely store each script in the html file
    outstream << "const readFileContentMap = new Map([\n";
    auto autoCloseNewMap = finally([&outstream] {  outstream << "]);\n"; });

    for(const auto& id_ : readFileIdSet) {
        // javascript Maps can take 2d arrays in the constructor.
        // Each array entry has the format [key, value].
        outstream << "[" << id_ << ",";
        auto autoCloseBracket = finally([&outstream] { outstream << "],\n"; });
        QFileThrow f(m_storedFiles.mkPathStringToStoredReadFile(id_));
        try {
            f.open(QFile::OpenModeFlag::ReadOnly);
            auto mtype = m_mimedb.mimeTypeForData(&f);
            if(! mtype.inherits("text/plain")){
                outstream << "null"; // don't use 'undefined' here!
                continue;
            }
            outstream << "\"";
            auto autoSetQuote = finally([&outstream] { outstream << "\""; });
            writeFileToStream(f, outstream);

        } catch (const QExcIo& e) {
            logWarning << qtr("Error writing read file with id %1 to html: %2")
                          .arg(id_).arg(e.descrip());
        }
    }
}

void CommandPrinterHtml::writeFileToStream(QFileThrow &f, QTextStream &outstream)
{
    const int BUFSIZE = 9000; // MUST be divisible by 3, so we create no padding '='
                              // between base64-chunks (;
    char buf[BUFSIZE];
    qint64 readCount;
    while(  (readCount = f.read(buf, BUFSIZE)) > 0 ){
        // we could be writing anything here to the js file - KISS, and use base64
        outstream << QByteArray::fromRawData(buf, int(readCount)).toBase64();
    }
}

void CommandPrinterHtml::writeStatistics(QTextStream &outstream)
{
    {
        QJsonArray jsonMostFileMods;
        for(const auto& e : m_cmdStats.cmdsWithMostFileMods()){
            QJsonObject o;
            o["idx"] = e.idx;
            o["countOfFileMods"] = e.countOfFileMods;
            jsonMostFileMods.append(o);
        }
        outstream << "const mostFileMods = "
                  << QJsonDocument(jsonMostFileMods).toJson(QJsonDocument::Compact) << "\n";
    }

    {
        QJsonArray jsonSessionsMostCmds;
        for(const auto & e : m_cmdStats.sessionMostCmds()){
            QJsonObject o;
            o["idxFirstCmd"] = e.idx;
            o["countOfCommands"] = e.cmdCount;
            jsonSessionsMostCmds.append(o);
        }
        outstream << "const sessionsMostCmds = "
                  << QJsonDocument(jsonSessionsMostCmds).toJson(QJsonDocument::Compact) << "\n";
    }

    {
        QJsonArray json;
        for(const auto & e : m_cmdStats.cwdCmdCounts()){
            QJsonObject o;
            o["workingDir"] = e.workingDir;
            o["countOfCommands"] = e.cmdCount;
            json.append(o);
        }
        outstream << "const cwdCmdCounts = "
                  << QJsonDocument(json).toJson(QJsonDocument::Compact) << "\n";
    }

    {
        QJsonArray json;
        for(const auto & e : m_cmdStats.dirIoCounts()){
            QJsonObject o;
            o["dir"] = e.dir;
            o["readCount"] = e.readCount;
            o["writeCount"] = e.writeCount;
            json.append(o);
        }
        outstream << "const dirIoCounts = "
                  << QJsonDocument(json).toJson(QJsonDocument::Compact) << "\n";
    }



}


