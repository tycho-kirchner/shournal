#pragma once

#include "attached_shell.h"

/// We read the env-variable set in bash's PS0
/// in order to prepare the observation of the next command
/// sequence. We cannot do this easily from within a function
/// called in PS0, because that is run in a subshell.
/// Another possibility is to run a signal handler but there we must
/// again be careful not to interfere with custom handlers of the
/// user.
/// counter=0
/// trap_handler(){
///     counter=$((counter+1))
///     echo "hi from trap_handler: $counter: $(history 1)" >&2
/// }
/// trap trap_handler SIGRTMIN
/// PS0='$(echo "sending signal... "; kill -SIGRTMIN  $$; )'
class AttachedBash : public AttachedShell
{
public:
    AttachedBash();

    void handleEnable() override;

    bool cmdCounterJustIncremented() override;

private:
    int m_lastSeq;
};

