
# shell-integration for shournal - kernel-module backend.


_shournal_run_backend='shournal-run'

_shournal_enable(){
    local tmpdir
    local ret=0
    local cmd_str

    if [ -n "${_shournal_is_running+x}" ] ; then
        # maybe_todo: check that our prompts are still there.
        _shournal_debug "_shournal_enable: current session is already observed"
        return 0
    fi

    if ! "$_shournal_run_backend" --shournalk-is-loaded; then
        _shournal_warn "Cannot enable the shell-integration -" \
                       "the required kernel module is not loaded."
        return 1
    fi

    if [ -e '/dev/shm' ]; then
        tmpdir='/dev/shm'
    else
        [ -n "${TMPDIR+x}" ] && tmpdir="$TMPDIR" || tmpdir=/tmp
    fi
    _shournal_fifo_basepath="$tmpdir/shournal-fifo-$USER-$$"
    _shournal_setup_error_path_current_pid="$tmpdir/shournal-setup-error-$USER-$$"

    # If an observed shell calls "exec bash" we end
    # up with an already existing fifo.
    # In almost all cases this is no problem, as the first time shournal-run is called
    # the pid is claimed and the old shournal-run process exits. However, in case of
    # sequence count 1 the previous and current fifo-paths collide, so just clean up
    # in any case.
    _shournal_detach_this_pid 0

    # May be necessary in cases where setup fails but exec was called, so
    # we couldn't clean up previously.
    _shournal_remove_setup_error_file_if_exist

    if [ -n "${_shournal_shell_exec_string+x}" ]; then
        # invoked via sh -c
        cmd_str=""
        # FIXME: also collect /proc/$$/exe ?
        while IFS= read -r -d '' line; do
            [ -z "$cmd_str" ] && cmd_str="$line" ||
                                 cmd_str="$cmd_str $line"
        done < /proc/$$/cmdline

        _shournal_preexec_generic 1 "$cmd_str" || return $?
        _shournal_is_running=true
    else
        SHOURNAL_SESSION_ID="$(shournal-run --make-session-uuid)" || return $?
        export SHOURNAL_SESSION_ID
        export SHOURNAL_CMD_COUNTER=0

        # Usually removing prompts should not be necessary here,
        # however, if a user exports PS0/PROMPT_COMMAND
        # and starts a new bash-session, we need to get rid of the existing commands.
        _shournal_remove_prompts || return $?
        _shournal_add_prompts || return $?
        _shournal_is_running=true
    fi
    return 0
}

_shournal_disable(){
    # Note that there are at least three cases how we can get here:
    # • User-invoked SHOURNAL_DISABLE
    # • Error during pre/postexec
    # • exit trap
    local exitcode="$1"

    _shournal_debug "_shournal_disable: about to disable" \
                    "with exitcode $exitcode"

    if [ -z "${_shournal_is_running+x}" ]; then
        _shournal_debug "_shournal_disable: not running"
        return 0
    fi

    _shournal_remove_setup_error_file_if_exist
    _shournal_remove_prompts
    _shournal_detach_this_pid "$exitcode"

    unset _shournal_is_running \
          _shournal_fifo_basepath _shournal_setup_error_path_current_pid
    return 0
}

_shournal_set_verbosity(){
    :
}

_shournal_print_versions(){
    echo "shournal-run: $(shournal-run --version)"
}


_shournal_send_msg(){
    # send json string to last started shournal.
    # for the different message types (msgType), see enum FIFO_MSG in c++.
    local fifofd="$1"
    local msg_type="$2"
    local msg_data="$3"
    local fdpath
    local ret=0

    # simple json string type-data: { "msgType":0, "data":"stuff" }
    local full_msg="{\"msgType\":$msg_type,\"data\":\"$msg_data\"}"
    _shournal_debug "_shournal_send_msg: sending message: $full_msg"
    # See _shournal_run_finalize for the rationale of using a fd instead of fifopath.
    # We may run in a subshell, so do not use $$.
    _shournal_refresh_current_pid || return $?
    fdpath="/proc/$_shournal_current_pid/fd/$fifofd"
    echo "$full_msg" > "$fdpath" || ret=$?
    if [ $ret -ne 0 ]; then
        _shournal_error "_shournal_send_msg: failed to write to $fdpath: $ret"
        return $ret
    fi
    return 0
}

_shournal_send_ret_val(){
    _shournal_send_msg "$1" 0 "$2"
}


_shournal_send_unmark_pid(){
    _shournal_send_msg "$1" 1 "$2"
}

_shournal_run_finalize(){
    local fifopath="$1"
    local exitcode="$2"
    local _shournal_fifofd

    # We race with the fifo-removal of a potential previous
    # _shournal_postexec_generic and the removal shournal-run performs. To
    # avoid creating a file (instead of writing to the fifo), whose
    # event pollutes shournal's history, first open a descriptor
    # read only and on success write to it later using /proc/PID/fd/$_shournal_fifofd
    # This also protects us from a potential deadlock. shournal-run first deletes
    # the fifo and then closes it. If it is deleted after we opened it readonly,
    # reopen and writing to it does *not* block.
    if ! { exec {_shournal_fifofd}<"$fifopath"; } 2>/dev/null; then
        _shournal_debug "_shournal_run_finalize: opening fifopath \"$fifopath\" failed."
        return 0
    fi
    _shournal_send_ret_val $_shournal_fifofd $exitcode
    _shournal_send_unmark_pid $_shournal_fifofd $$
    rm "$fifopath" 2>/dev/null
    exec {_shournal_fifofd}<&-
}


# Find a fifo (if any) that was created by this shell previously and
# instruct the belonging shournal-run process to stop
# observing this pid.
_shournal_detach_this_pid(){
    local exitcode="$1"
    local fifopath
    local ret=0

    if [ "$_SHOURNAL_SHELL_NAME" = 'zsh' ]; then
        # supress nomatch error messages (and aborts)
        setopt LOCAL_OPTIONS
        unsetopt nomatch
    fi

    # use globbing to ignore the sequence number
    set -- "$_shournal_fifo_basepath"*
    # Note: in case of no results $1 is _not_ empty, so check
    # for existence.
    if [ -e "$1" ] ; then
        if [ $# -eq 1 ]; then
            fifopath=${1%%$'\n'*} # should not be necessary
            _shournal_debug "_shournal_detach_this_pid at $fifopath"
            _shournal_run_finalize "$fifopath" "$exitcode" || ret=$?
        else
            _shournal_error "_shournal_detach_this_pid: unexpected fifos $@"
            ret=1
        fi
    fi
    return $ret
}

_shournal_remove_setup_error_file_if_exist(){
    test -e "$_shournal_setup_error_path_current_pid" &&
         rm "$_shournal_setup_error_path_current_pid"
    return 0
}


# preexec is run before a valid command (but not when ENTER or Ctrl+C is hit).
# We launch a shournal-run process and wait for it to setup and
# fork into background. Note that the command sequence counter is not
# incremented yet
_shournal_preexec_generic(){
    local current_seq="$1"
    local cmd_str="$2"
    local fifopath
    local args_array

    if ! _shournal_verbose_history_check; then
        _shournal_warn "history settings were modified after the shell integration was turned on. " \
                       "Please correct that or call SHOURNAL_DISABLE " \
                       "to get rid of this message."
        return 1
    fi
    fifopath="$_shournal_fifo_basepath-$current_seq"
    _shournal_debug "_shournal_preexec_generic: using fifo at $fifopath"
    _shournal_warn_on "[ -e \"$fifopath\" ]"

    args_array=(
        --verbosity "$_SHOURNAL_VERBOSITY" --pid $$ --fork
        --close-fds --fifoname "$fifopath"
        --cmd-string "$cmd_str"
    )

    [ -n "${SHOURNAL_SESSION_ID+x}" ] &&
        args_array+=(--shell-session-uuid "$SHOURNAL_SESSION_ID")

    # Argument --close-fds is important here for the following reasons:
    # * We may run within a subshell which waits for redirected stdout to
    #   close (deadlock otherwise).
    # * We have created a custom redirection, e.g. with
    #   exec 3> foo; echo "test" >&3;
    #   exec 3>&-; # closes 3
    #   In this case **without closing** within shournal-run the close event would be lost,
    #   as the final __fput is reached during shournal-run exit().
    # Argument --fork: shournal forks itself into background once setup
    # is ready, so we can just wait here.
    if ! shournal-run "${args_array[@]}"; then
        # only debug here - there should already be two warnings - one from
        # shournal-run or bash not able to execute and (likely) one afterwards
        # from the prompt.
        _shournal_debug "_shournal_preexec_generic: shournal-run setup failed"
        # maybe_todo: If we successfully call "exec" immediatly afterwards,
        # we leak this file. This however should happen rarely.
        echo 1 > "$_shournal_setup_error_path_current_pid"
        return 1
    fi
    return 0
}


# postexec is run after any command, but eventually also after hitting ENTER
# or Ctrl (other than PS0). However, the command sequence counter
# SHOURNAL_CMD_COUNTER is only incremented in case of valid commands.
# So we try to open the fifo-path according to the current counter.
# If open succeeds, write the last exit-code to the fifo and unmark this process.
_shournal_postexec_generic(){
    local current_seq="$1"
    local exitcode="$2"
    local fifopath

    fifopath="$_shournal_fifo_basepath-$current_seq"
    _shournal_debug "_shournal_postexec_generic: using fifo at $fifopath"
    _shournal_run_finalize "$fifopath" "$exitcode"

    return $exitcode
}


# The following non-portable, shell specific functions _must_ be set
# for each supported shell:
# _shournal_add_prompts
# _shournal_remove_prompts
#
# During the prompts _shournal_preexec_generic and
# _shournal_postexec_generic must be
# called respectively.

case "$_SHOURNAL_SHELL_NAME" in
  'bash')
# We use _SHOURNAL_SHELL_NAME as a dummy variable in order to increment
# SHOURNAL_CMD_COUNTER without printing anything in PS0. First increment,
# then execute shournal. Otherwise a SIGINT may abort PS0 execution,
# preventing the increment.
_shournal_ps0='${_SHOURNAL_SHELL_NAME:((++SHOURNAL_CMD_COUNTER)):0}'\
'$(_shournal_preexec $SHOURNAL_CMD_COUNTER)'
_shournal_prompt_command=$'_shournal_postexec\n'


_shournal_add_prompts(){
    [ -z "${PS0+x}" ] && PS0=''
    [ -z "${PROMPT_COMMAND+x}" ] && PROMPT_COMMAND=''
    PS0+="$_shournal_ps0"
    PROMPT_COMMAND="${_shournal_prompt_command}${PROMPT_COMMAND}"
    return 0
}

_shournal_remove_prompts(){
    [ -n "${PS0+x}" ] && PS0=${PS0//"$_shournal_ps0"/}
    [ -n "${PROMPT_COMMAND+x}" ] &&
        PROMPT_COMMAND=${PROMPT_COMMAND//"$_shournal_prompt_command"/}
    return 0
}
## _____ End of must-override functions and variables _____ ##


_shournal_preexec(){
    local current_seq="$1"
    local cmd_str
    if [[ -z "${PROMPT_COMMAND+x}" || "$PROMPT_COMMAND" != *"$_shournal_prompt_command"* ]]; then
        _shournal_error "_shournal_preexec: Invalid PROMPT_COMMAND. Apparently" \
            "PROMPT_COMMAND was modified after SHOURNAL_ENABLE" \
            "was called. This is often caused by double-sourcing the bashrc, e.g. from" \
            "~/.profile or .bash_profile."
        echo 1 > "$_shournal_setup_error_path_current_pid"
        return 1
    fi

    _shournal_get_current_cmd_bash cmd_str
    _shournal_preexec_generic "$current_seq" "$cmd_str"
}

# Disable the shell-integration in case of setup-errors, to avoid
# spamming the user. Setup may in particular fail in cases where
# shournal is updated while the kernel module of the old version
# is still active.
# Note that other than _shournal_preexec this
# function is executed in the *parent shell*.
_shournal_postexec(){
    local ret=$?
    _shournal_postexec_generic "$SHOURNAL_CMD_COUNTER" "$ret" || :
    if test -e "$_shournal_setup_error_path_current_pid"; then
        _shournal_warn "Disabling the shell-integration due to previous setup-erros..."
        SHOURNAL_DISABLE
    fi
    return $ret
}


;; # END_OF bash _______________________________________________________

'zsh')


_shournal_add_prompts(){
    preexec_functions+=(_shournal_preexec)
    precmd_functions+=(_shournal_postexec)
    return 0
}

_shournal_remove_prompts(){
    preexec_functions[$preexec_functions[(i)_shournal_preexec]]=()
    precmd_functions[$precmd_functions[(i)_shournal_postexec]]=()
    return 0
}

## _____ End of must-override functions and variables _____ ##


_shournal_preexec(){
    # maybe_todo: use $2 or $3 for expanded aliases instead of $1
    local cmd_str="$1"
    ((++SHOURNAL_CMD_COUNTER))
    _shournal_preexec_generic $SHOURNAL_CMD_COUNTER "$cmd_str" || return $?
    return 0
}

_shournal_postexec(){
    local exitcode=$?
    _shournal_postexec_generic $SHOURNAL_CMD_COUNTER $exitcode || return $?
    return 0
}


;; # END_OF zsh ________________________________________________________
  *)
    echo "shournal shell integration: sourced from unsupported shell - " \
         "currently only bash and zsh are supported." >&2
    return 1
;;
esac




