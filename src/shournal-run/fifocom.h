#pragma once

#include <QString>
#include "qfilethrow.h"

/// Fifo-communication. Parse (json)
/// messages sent to a given shournal-run instance
class FifoCom
{
public:
    FifoCom(int fifo);

    int readJsonLine(QString& data);

private:
    bool readLineRaw();

    int m_fifofd;
    QByteArray m_bufTmp;
    int m_bufIdx{0};
    QByteArray m_linebuf;

};

