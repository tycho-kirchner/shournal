#pragma once

/// The shell logger prints to stderr,
/// and sends messages via socket to the external
/// shournal process, where they are written to file.
/// In case the socket is closed, the messages are buffered.
namespace shell_logger {

void setup();

void flushBufferdMessages();
}
