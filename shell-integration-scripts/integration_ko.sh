
# shell-integration for shournal - kernel-module backend.


_shournal_run_backend='shournal-run'

_shournal_enable(){
    local tmpdir

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
    _shournal_detach_this_pid

    _shournal_session_uuid="$(shournal-run --make-session-uuid)" || return 1

    # May be necessary in cases where setup fails but exec was called, so
    # we couldn't clean up previously.
    _shournal_remove_setup_error_file_if_exist


    # Usually removing prompts should not be necessary here,
    # however, if a user exports PS0/PS1/PROMPT_COMMAND
    # and starts a new bash-session, we need to get rid of the existing commands.
    _shournal_remove_prompts
    _shournal_add_prompts
    _shournal_is_running=true

    return 0
}


_shournal_disable(){
    _shournal_remove_setup_error_file_if_exist
    _shournal_remove_prompts
    _shournal_detach_this_pid

    unset _shournal_is_running _shournal_last_cmd_seq \
          _shournal_session_uuid _shournal_cmd_seq_hotfix \
          _shournal_fifo_basepath _shournal_setup_error_path_current_pid
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
    local pid

    # simple json string type-data: { "msgType":0, "data":"stuff" }
    local full_msg="{\"msgType\":$msg_type,\"data\":\"$msg_data\"}"
    _shournal_debug "_shournal_send_msg: sending message: $full_msg"
    # See _shournal_run_finalize for the rationale of using a fd instead of fifopath.
    # For bash we need BASHPID, not $$ because
    # bash's PS0 and PS1 are executed in subshells
    [ "$_SHOURNAL_SHELL_NAME" = 'bash' ] && pid=$BASHPID || pid=$$
    echo "$full_msg" > "/proc/$pid/fd/$fifofd"
    _shournal_debug "_shournal_send_msg DONE"
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
    # PS1 and the removal shournal-run performs. To
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
            _shournal_run_finalize "$fifopath" 0
            ret=$?
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
}


# preexec is run before a valid command (but not when ENTER or Ctrl+C is hit).
# We launch a shournal-run process and wait for it to setup and
# fork into background. Note that the command sequence counter is not
# incremented yet
_shournal_preexec_generic(){
    local current_seq="$1"
    local cmd_str="$2"
    local fifopath

    if ! _shournal_verbose_history_check; then
        _shournal_warn "history settings were modified after the shell integration was turned on. " \
                       "Please correct that or call SHOURNAL_DISABLE " \
                       "to get rid of this message."
        return 1
    fi
    fifopath="$_shournal_fifo_basepath-$current_seq"
    _shournal_debug "_shournal_preexec_generic: using fifo at $fifopath"
    _shournal_warn_on "[ -e \"$fifopath\" ]"


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
    if ! shournal-run --verbosity "$_SHOURNAL_VERBOSITY" --pid $$ --fork \
        --close-fds --fifoname "$fifopath" \
        --shell-session-uuid "$_shournal_session_uuid" \
        --cmd-string "$cmd_str"; then
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


# postexec is run after any command, but also after hitting ENTER or Ctrl (other than PS0).
# However, the command sequence \# ($1) was only incremented in case of valid commands. So we try
# to open the fifo-path using the last (decremented) \#. If open succeeds, write the last exit-code
# to the fifo and unmark this process.
_shournal_postexec_generic(){
    local current_seq="$1"
    local exitcode="$2"
    local previous_seq=$((current_seq - 1))
    local fifopath

    fifopath="$_shournal_fifo_basepath-$previous_seq"
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
# Until bash-version 5.0 ( commit d233b485e83c3a784b803fb894280773f16f2deb),
# in eval.c:reader_loop the current_command_number was incremented before decode_prompt_string (ps0_prompt).
# Take care of that by simply decrementing the command sequence in ps0.
[ ${BASH_VERSINFO[0]} -lt 5 ] && _shournal_cmd_seq_hotfix=true ||
                                 _shournal_cmd_seq_hotfix=false

_shournal_add_prompts(){
    [ -z "${PS0+x}" ] && PS0=''
    [ -z "${PS1+x}" ] && PS1=''
    [ -z "${PROMPT_COMMAND+x}" ] && PROMPT_COMMAND=''

    PS0="$PS0"'`_shournal_preexec \#`'
    PS1="$PS1"'`_shournal_postexec \#`'
    PROMPT_COMMAND=$'_shournal_prompt_bash\n'"$PROMPT_COMMAND"
    return 0
}

_shournal_remove_prompts(){
    [ -n "${PS0+x}" ] && PS0=${PS0//'`_shournal_preexec \#`'/}
    [ -n "${PS1+x}" ] && PS1=${PS1//'`_shournal_postexec \#`'/}
    [ -n "${PROMPT_COMMAND+x}" ] &&
        PROMPT_COMMAND=${PROMPT_COMMAND//_shournal_prompt_bash$'\n'/}
    return 0
}
## _____ End of must-override functions and variables _____ ##


_shournal_preexec(){
    local current_seq="$1"
    local cmd_str
    $_shournal_cmd_seq_hotfix && current_seq=$((current_seq - 1))
    _shournal_get_current_cmd_bash cmd_str
    _shournal_preexec_generic "$current_seq" "$cmd_str"
}

_shournal_postexec(){
    local exitcode=$?
    local current_seq="$1"
    _shournal_postexec_generic "$current_seq" "$exitcode"
    return $exitcode

}

# Disable the shell-integration in case of setup-errors, to avoid
# spamming the user. Setup may in particular fail in cases where
# shournal is updated while the kernel module of the old version
# is still active.
# Note that other than PS0 and PS1 this
# function is executed in the *parent shell*.
_shournal_prompt_bash(){
    local ret=$?
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
    [ -z "${_shournal_zsh_cmdseq+x}" ] && _shournal_zsh_cmdseq=0
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
    _shournal_preexec_generic $_shournal_zsh_cmdseq "$cmd_str"
    _shournal_zsh_cmdseq=$((_shournal_zsh_cmdseq+1))
    return 0
}

_shournal_postexec(){
    local exitcode=$?
    _shournal_postexec_generic $_shournal_zsh_cmdseq $exitcode
    return 0
}


;; # END_OF zsh ________________________________________________________
  *)
    echo "shournal shell integration: sourced from unsupported shell - " \
         "currently only bash and zsh are supported." >&2
    return 1
;;
esac



