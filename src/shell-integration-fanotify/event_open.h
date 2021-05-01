
#pragma once

#include <sys/stat.h>

namespace event_open {

int handleOpen(const char *pathname, int flags, mode_t mode, bool largeFile );

}

