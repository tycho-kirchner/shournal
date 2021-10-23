#pragma once


namespace shell_request_handler  {

/// ENABLE: shell observation enabled
/// DISABLE: shell observation disabled
/// PREPARE_CMD: prepare observing the next command-sequence
/// CLEANUP_CMD: stop monitoring the command-sequence and send command-info to external shournal
/// PRINT_VERSION: print the version of *this* shared library
/// UPDATE_VERBOSITY: update the verbosity from environment
/// TRIGGER_UNSET:  The trigger is not set in the environment
/// TRIGGER_MALFORMED: The trigger is set in the environment but malformed
enum class ShellRequest {
                         // To be used by the shell integration-scripts
                         ENABLE, DISABLE,
                         PREPARE_CMD, CLEANUP_CMD,
                         PRINT_VERSION,
                         UPDATE_VERBOSITY,

                         // Internal use in this shared library
                         TRIGGER_UNSET,
                         TRIGGER_MALFORMED,
                         ENUM_END};

ShellRequest checkForTriggerAndHandle(bool *success);
bool handlePrepareCmd();

} // namespace shell_request_handler

