
#include "fifocom.h"

#include <linux/limits.h>
#include <cassert>
#include <QJsonDocument>
#include <QJsonObject>

#include "stdiocpp.h"
#include "logger.h"
#include "cleanupresource.h"
#include "os.h"

FifoCom::FifoCom(int fifo) :
    m_fifofd(fifo)
{
    m_linebuf.reserve(PIPE_BUF);
}

/// Read a simple json message from our fifo containing only
/// of messsage-type (int >= 0) and data-field.
/// @param data: is filled with the data field of the message
/// @return the message type or -1.
int FifoCom::readJsonLine(QString &data)
{
    if(! readLineRaw()){
        return -1;
    }
    auto finallyClearLinebuf = finally([&] {
        m_linebuf.clear();
    });

    QJsonDocument d = QJsonDocument::fromJson(m_linebuf);
    QJsonObject rooObj = d.object();
    int messageType = rooObj.value("msgType").toInt(-1);
    if(messageType == -1){
        logWarning << "invalid fifo-message received (buggy client?):" << m_linebuf;
        return -1;
    }
    data = rooObj.value("data").toString();

    return messageType;
}


/// Buffered line read from non-blocking fifo.
/// push_back to our line buffer until we find a NEWLINE.
/// To be compliant with O_NONBLOCK, return false on EAGAIN.
/// @return if true, m_linebuf contains the read line.
bool FifoCom::readLineRaw()
{
    bool foundNewLine = false;
    if(m_bufIdx == 0){
        m_bufTmp.resize(PIPE_BUF);
        auto count = read(m_fifofd, m_bufTmp.data(), m_bufTmp.size());
        if(count == -1){
            if(errno == EAGAIN || errno == EWOULDBLOCK){
                return false;
            }
            throw os::ExcOs("read from fifo failed:");
        }
        m_bufTmp.resize(int(count));
    }

    for(; m_bufIdx < m_bufTmp.size(); m_bufIdx++){
        if(m_bufTmp[m_bufIdx] == '\n'){
            foundNewLine = true;
            m_bufIdx++;
            break;
        }
        m_linebuf.push_back(m_bufTmp[m_bufIdx]);
    }

    if(m_bufIdx >= m_bufTmp.size()){
        // whole buffer consumed
        m_bufIdx = 0;
    }
    if(! foundNewLine){
        // most likely the user sent a message greater than
        // PIPE_BUF. Get the rest on next call (do not clear
        // buffer)
        return false;
    }
    return true;
}


