
# bash- and zsh-integration for shournal - kernel-module backend.


SHOURNAL_ENABLE() {
    local tmpdir

    if [ -z "${_shournal_version+x}" ]; then
        local common_path="$(dirname -- "$_shournal_this_scriptname")/_common.sh"
        if ! . "$common_path"; then
            >&2 echo "shournal shell integration: failed to source common file $common_path"
            return 1
        fi
    fi
    # 0: debug, 1: info, 2: warning, 3: error
    [ -z "${_shournal_shell_integration_log_level+x}" ] && _shournal_shell_integration_log_level=2
    [ -z "${_SHOURNAL_VERBOSITY+x}" ] && _SHOURNAL_VERBOSITY="warning"

    local path_to_shournal=$(command -v shournal)
    if [ -z "$path_to_shournal" ] ; then
        _shournal_error "cannot enable shournal's shell integration - command <shournal> not found"
        return 1
    fi

    local path_to_shournal_run=$(command -v shournal-run)
    if [ -z "$path_to_shournal_run" ] ; then
        _shournal_error "cannot enable shournal's shell integration - command <shournal-run> not found"
        return 1
    fi

    if ! shournal-run --shournalk-is-loaded; then
        _shournal_warn "Cannot enable the shell-integration -" \
                       "the required kernel module is not loaded."
        return 1
    fi

    if [ -n "${_shournal_parent_launched_us_noninteractive+x}" ] ; then
        # avoid endless recursion. However, if during execution an interactive shell is launched, allow
        # for an "interactive" SHOURNAL_ENABLE. That applies to subprocesses. To handle multiple calls
        # of this function within this process, set it without exporting
        unset _shournal_parent_launched_us_noninteractive
        _shournal_parent_launched_us_noninteractive=true
        return 0
    fi

    if [ -n "${_shournal_is_running+x}" ] ; then
        _shournal_debug "SHOURNAL_ENABLE: current session is already observed"
        return 0
    fi

    if ! shournal --validate-settings; then
        # informative mesg. should have been already printed by shournal
        _shournal_error "shell integration is *not* enabled"
        return 1
    fi

    declare -a args_array
    while IFS= read -r -d '' line; do
        args_array+=("$line")
    done < /proc/$$/cmdline

    local cmd_path="$(readlink /proc/$$/exe)"

    [ -n "${TMPDIR+x}" ] && tmpdir="$TMPDIR" || tmpdir=/tmp
    _shournal_fifo_basepath="$tmpdir/shournal-fifo-$USER-$$"
    _shournal_setup_error_path_current_pid="/dev/shm/shournal-setup-error-$USER-$$"


    # If an observed shell calls "exec bash" we end
    # up with an already existing fifo.
    # In almost all cases this is no problem, as the first time shournal-run is called
    # the pid is claimed and the old shournal-run process exits. However, in case of
    # sequence count 1 the previous and current fifo-paths collide, so just clean up
    # in any case.
    _shournal_detach_this_pid

    if [ -n "${_shournal_shell_exec_string+x}" ]; then
        _shournal_debug "SHOURNAL_ENABLE: exec non-interactive $cmd_path ${args_array[@]}"
        _shournal_parent_launched_us_noninteractive=true exec \
            shournal-run --fork --verbosity "$_SHOURNAL_VERBOSITY" --exec-filename "$cmd_path" --exec "${args_array[@]}"
        # only get here on error
        return 1
    fi

    # Running interactively

    _shournal_verbose_history_check || return 1
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

    return
}


SHOURNAL_DISABLE() {
    # if running non-interactively, disabling SHOURNAL does not have an effect.
    [ -n "${_shournal_parent_launched_us_noninteractive+x}" ] && return 0
    [ -z "${_shournal_is_running+x}" ] && return 1

    _shournal_remove_setup_error_file_if_exist
    _shournal_remove_prompts
    _shournal_detach_this_pid

    unset _shournal_is_running _shournal_last_cmd_seq \
          _shournal_session_uuid _shournal_cmd_seq_hotfix
    # TODO unset more ...
}

# $1: pass one of dbg, info, warning, critical
SHOURNAL_SET_VERBOSITY(){
    case "$1" in
    "dbg")
        _shournal_shell_integration_log_level=0
        ;;
    "info")
        _shournal_shell_integration_log_level=1
        ;;
    "warning")
        _shournal_shell_integration_log_level=2
        ;;
    "critical")
        _shournal_shell_integration_log_level=3
        ;;
    *)
        _shournal_warn "Bad verbosity passed. Pass one of dbg, info, warning, critical"
        return 1
        ;;
    esac

    _SHOURNAL_VERBOSITY="$1"
}

SHOURNAL_PRINT_VERSIONS() {
    echo "shournal $_shournal_this_shell integration version: $_shournal_version"
    echo "shournal: $(shournal --version)"
    echo "shournal-run: $(shournal-run --version)"
}




######################## PRIVATE ########################


# don't call it directly, but use one of debug, info, warning, error functions
# $1: loglevel.
# all other args: is printed to stderr
_shournal_log_msg(){
    local loglevel=$1
    shift
    [ "$loglevel" -ge "$_shournal_shell_integration_log_level" ] &&
        >&2  printf "shournal $_shournal_this_shell integration - $*\n"
}

_shournal_error() {
     _shournal_log_msg 3 "ERROR: $*"
}

_shournal_warn(){
    _shournal_log_msg 2 "warning: $*"
}

_shournal_info(){
        _shournal_log_msg 1 "info: $*"
}

_shournal_debug(){
        _shournal_log_msg 0 "debug: $*"
}

_shournal_warn_on(){
    eval "$1" && _shournal_warn "$1"
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
    # See shournal's ps1 for the rationale of using a fd instead of fifopath.
    # For bash we need BASHPID, not $$ because
    # bash's PS0 and PS1 are executed in subshells
    [ "$_shournal_this_shell" = 'bash' ] && pid=$BASHPID || pid=$$
    echo "$full_msg" > /proc/$pid/fd/$fifofd
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
    local reset_nomatch=false
    local ret=0

    if [ "$_shournal_this_shell" = 'zsh' ] && [[ -o nomatch ]]; then
        # supress nomatch error messages (and aborts)
        # TODO: just setopt LOCAL_OPTIONS ??
        reset_nomatch=true
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
    $reset_nomatch && setopt nomatch
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
_shournal_preexec(){
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
    _shournal_debug "_shournal_preexec: using fifo at $fifopath"
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
    shournal-run --verbosity "$_SHOURNAL_VERBOSITY" --pid $$ --fork \
        --close-fds --fifoname "$fifopath" \
        --shell-session-uuid "$_shournal_session_uuid" \
        --cmd-string "$cmd_str"
    if [ $? -ne 0 ]; then
        # only debug here - there should already be two warnings - one from
        # shournal-run or bash not able to execute and (likely) one afterwards
        # from the prompt.
        _shournal_debug "_shournal_preexec: shournal-run setup failed"
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
_shournal_postexec(){
    local current_seq="$1"
    local exitcode="$2"
    local previous_seq=$((current_seq - 1))
    local fifopath
    local fifopath_ret

    fifopath="$_shournal_fifo_basepath-$previous_seq"
    _shournal_debug "_shournal_postexec: using fifo at $fifopath"
    _shournal_run_finalize "$fifopath" "$exitcode"

    return $exitcode
}



# Setup non-portable stuff for
# each supported shell. The following variables _must_ be set:
# _shournal_this_shell (name of the current shell)
# _shournal_shell_exec_string - if and only if the command is executed non-interactively
#
# The following functions _must_be set:
# _shournal_verbose_history_check (retruns 0 on success)
# _shournal_add_prompts
# _shournal_remove_prompts
#
# During the prompts _shournal_preexec and _shournal_postexec must be
# called respectively.

# TODO: remove _shournal_this_scriptname once ready.
if [ -n "${BASH_VERSION+x}" ]; then
_shournal_this_shell='bash'
_shournal_this_scriptname="$BASH_SOURCE"

# Until bash-version 5.0 ( commit d233b485e83c3a784b803fb894280773f16f2deb),
# in eval.c:reader_loop the current_command_number was incremented before decode_prompt_string (ps0_prompt).
# Take care of that by simply decrementing the command sequence in ps0.
[ ${BASH_VERSINFO[0]} -lt 5 ] && _shournal_cmd_seq_hotfix=true ||
                                 _shournal_cmd_seq_hotfix=false

# The bash execution string is e.g. set when running bash -c 'echo foo', in which case we never get to
# any prompt. Simply execute the whole command
# within shournal.
# Checking $BASH_EXECUTION_STRING seems to be more reliable than [[ $- == *i* ]], because
# of commands like e.g. bash -i -c 'echo "wtf - is that interactive?"'
[ -n "${BASH_EXECUTION_STRING+x}" ] &&
    _shournal_shell_exec_string="$BASH_EXECUTION_STRING"

# returns 0, if all history settings were ok, else false.
# is verbose, if a setting is not ok.
_shournal_verbose_history_check(){
    # no history needed if running non-interactively
    [ -n "${BASH_EXECUTION_STRING+x}" ] && return 0
    local success=true

    if ! [ -o history ]; then
        success=false
        _shournal_error "bash history is off. Please enable it: set -o history"
    fi

    if [[ ${HISTSIZE-0} -lt 2 ]]; then
        success=false
        _shournal_error "bash HISTSIZE is too small (or not set). Please set it at least to 2: HISTSIZE=2"
    fi

    if [[ ${HISTCONTROL-} == *"ignorespace"* || ${HISTCONTROL-} == *"ignoreboth"* ]]; then
        success=false
        _shournal_error "Commands with spaces are set to be ignored from history. Please disable that, " \
                       "e.g. HISTCONTROL=ignoredups or HISTCONTROL=''"
    fi

    if [[ -n ${HISTIGNORE-} ]] ; then
        success=false
        _shournal_error "HISTIGNORE is not empty. Please unset it: unset HISTIGNORE"
    fi

    $success
    return $?
}

_shournal_add_prompts(){
    [ -z "${PS0+x}" ] && PS0=''
    [ -z "${PS1+x}" ] && PS1=''
    [ -z "${PROMPT_COMMAND+x}" ] && PROMPT_COMMAND=''

    PS0="$PS0"'`_shournal_preexec_bash \#`'
    PS1="$PS1"'`_shournal_postexec_bash \#`'
    PROMPT_COMMAND=$'_shournal_prompt_bash\n'"$PROMPT_COMMAND"
}

_shournal_remove_prompts(){
    [ -n "${PS0+x}" ] && PS0=${PS0//'`_shournal_preexec_bash \#`'/}
    [ -n "${PS1+x}" ] && PS1=${PS1//'`_shournal_postexec_bash \#`'/}
    [ -n "${PROMPT_COMMAND+x}" ] &&
        PROMPT_COMMAND=${PROMPT_COMMAND//_shournal_prompt_bash$'\n'/}
}
## _____ End of must-override functions and variables _____ ##

_shournal_get_current_cmd_bash(){
    declare -n ret=$1
    # history output is e.g.
    # " 6989  echo foo"
    # so strip the leading " 6989  "
    local cmd="$(HISTTIMEFORMAT='' history 1)"
    [[ "$cmd" =~ ([[:space:]]*[0-9]+[[:space:]]*)(.*) ]]
    ret="${BASH_REMATCH[2]}"
    return 0
}

_shournal_preexec_bash(){
    local current_seq="$1"
    local cmd_str
    $_shournal_cmd_seq_hotfix && current_seq=$((current_seq - 1))
    _shournal_get_current_cmd_bash cmd_str
    _shournal_preexec "$current_seq" "$cmd_str"
}

_shournal_postexec_bash(){
    local exitcode=$?
    local current_seq="$1"
    _shournal_postexec "$current_seq" "$exitcode"
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


#endif BASH

elif [ -n "${ZSH_VERSION+x}" ]; then
_shournal_this_shell='zsh'
_shournal_this_scriptname="$0"

[ -n "${ZSH_EXECUTION_STRING+x}" ] &&
    _shournal_shell_exec_string="$ZSH_EXECUTION_STRING"


_shournal_verbose_history_check(){
    # no history needed if running non-interactively
    [ -n "${ZSH_EXECUTION_STRING+x}" ] && return 0

    # TODO: check ignore space, etc.:
    # man zshoptions | grep HIST_IGNORE

    return 0
}

_shournal_add_prompts(){
    preexec_functions+=(_shournal_preexec_zsh)
    precmd_functions+=(_shournal_postexec_zsh)
    [ -z "${_shournal_zsh_cmdseq+x}" ] && _shournal_zsh_cmdseq=0
    return 0
}

_shournal_remove_prompts(){
    preexec_functions[$preexec_functions[(i)_shournal_preexec_zsh]]=()
    precmd_functions[$precmd_functions[(i)_shournal_postexec_zsh]]=()
    return 0
}

## _____ End of must-override functions and variables _____ ##


_shournal_preexec_zsh(){
    # maybe_todo: use $2 or $3 for expanded aliases instead of $1
    local cmd_str="$1"
    _shournal_preexec $_shournal_zsh_cmdseq "$cmd_str"
    _shournal_zsh_cmdseq=$((_shournal_zsh_cmdseq+1))
    return 0
}

_shournal_postexec_zsh(){
    local exitcode=$?
    _shournal_postexec $_shournal_zsh_cmdseq $exitcode
    return 0
}


#endif ZSH

else
    echo "shournal shell integration: sourced from unsupported shell - " \
         "currently only bash and zsh are supported." >&2
    return 1
fi


