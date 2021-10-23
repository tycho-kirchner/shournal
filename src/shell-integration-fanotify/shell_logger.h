#pragma once

#ifndef __FILENAME__
#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif

/// The shell logger prints to stderr,
/// and sends messages via socket to the external
/// shournal process, where they are written to file.
/// In case the socket is closed, the messages are buffered.
namespace shell_logger {

void setup();

void flushBufferdMessages();

}

#ifndef NDEBUG
void __shell_earlydbg(const char* file, int line, const char *format, ...);
#define shell_earlydbg(format, args...) __shell_earlydbg(__FILENAME__, __LINE__, format, ## args)
#else
#define shell_earlydbg(format, args...)
#endif

