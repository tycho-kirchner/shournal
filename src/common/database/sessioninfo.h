#pragma once

#include <QString>

struct SessionInfo
{
    QByteArray uuid;
    QString    comment;

    bool operator==(const SessionInfo& rhs) const;
};

