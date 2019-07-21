#pragma once

#include "nullable_value.h"

#include <string>

namespace pidcontrol {

    std::string parseCmdlineOfPID(pid_t pid);

    NullableValue<uid_t> parseRealUidOf(int procDirFd);

} // namespace pidcontrol



