#include "sessioninfo.h"



bool SessionInfo::operator==(const SessionInfo &rhs) const
{
    return uuid == rhs.uuid &&
            comment == rhs.comment;
}
