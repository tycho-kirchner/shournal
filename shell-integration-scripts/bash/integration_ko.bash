# bash-integration for shournal - kernel-module backend.

SHOURNAL_PRINT_VERSIONS() {
    echo "shournal bash integration version: $_shournal_version"
    echo "shournal: $(shournal --version)"
    echo "shournal-run: $(shournal-run --version)"
}


SHOURNAL_ENABLE() {
    if [[ -z ${_shournal_version+x} ]]; then
        local common_path="$(dirname -- "$BASH_SOURCE")/_common.sh"
        if ! source "$common_path"; then
            >&2 echo "shournal shell integration: failed to source common file $common_path"
            return 1
        fi
    fi
    # 0: debug, 1: info, 2: warning, 3: error
    [[ -z ${_shournal_bash_integration_log_level+x} ]] && _shournal_bash_integration_log_level=2
    [[ -z ${_SHOURNAL_VERBOSITY+x} ]] && _SHOURNAL_VERBOSITY="warning"

    if ! shournal-run --shournalk-is-loaded; then
        _shournal_warn "Cannot enable the shell-integration -" \
                       "the required kernel module is not loaded."
        return 1
    fi

    if [[ -n ${_shournal_parent_launched_us_noninteractive+x} ]] ; then
        # avoid endless recursion. However, if during execution an interactive shell is launched, allow
        # for an "interactive" SHOURNAL_ENABLE. That applies to subprocesses. To handle multiple calls
        # of this function within this process, set it without exporting
        unset _shournal_parent_launched_us_noninteractive
        _shournal_parent_launched_us_noninteractive=true
        return 0
    fi

    if [[ -n ${_shournal_is_running+x} ]] ; then
        _shournal_debug "${FUNCNAME[0]}: current bash session is already observed"
        return 0
    fi

    local path_to_shournal=$(command -v shournal)
    if [[ -z ${path_to_shournal} ]] ; then
        _shournal_error "cannot enable shournal's bash integration - command <shournal> not found"
        return 1
    fi

    if ! shournal --validate-settings; then
        # informative mesg. should have been already printed by shournal
        _shournal_error "bash integration is *not* enabled"
        return 1
    fi

    local path_to_shournal_run=$(command -v shournal-run)
    if [[ -z ${path_to_shournal_run} ]] ; then
        _shournal_error "cannot enable shournal's bash integration - command <shournal-run> not found"
        return 1
    fi

    declare -a args_array
    while IFS= read -r -d '' line; do
        args_array+=("$line")
    done < /proc/$$/cmdline

    local cmd_path="$(readlink /proc/$$/exe)"

    # Until bash-version 5.0 ( commit d233b485e83c3a784b803fb894280773f16f2deb),
    # in eval.c:reader_loop the current_command_number was incremented before decode_prompt_string (ps0_prompt).
    # Take care of that by simply decrementing the command sequence in ps0.
    [ ${BASH_VERSINFO[0]} -lt 5 ] && _shournal_cmd_seq_hotfix=true ||
                                     _shournal_cmd_seq_hotfix=false

    # If an observed shell calls "exec bash" we end
    # up with an already existing fifo.
    # In almost all cases this is no problem, as the first time shournal-run is called
    # the pid is claimed and the old shournal-run process exits. However, in case of
    # sequence count 1 the previous and current fifo-paths collide, so just clean up
    # in any case.
    _shournal_detach_this_pid

    if [[ -n ${BASH_EXECUTION_STRING+x} ]]; then
        # The bash execution string is e.g. set when running bash -c 'echo foo', in which case we never get to
        # any prompt. Do do not preload our .so, but instead execute the whole command
        # within shournal.
        # Checking $BASH_EXECUTION_STRING seems to be more reliable than [[ $- == *i* ]], because
        # of commands like e.g. bash -i -c 'echo "wtf - is that interactive?"'
        _shournal_debug "${FUNCNAME[0]}: exec non-interactive $cmd_path ${args_array[@]}"
        _shournal_parent_launched_us_noninteractive=true exec \
            shournal-run --fork --verbosity "$_SHOURNAL_VERBOSITY" --exec-filename "$cmd_path" --exec "${args_array[@]}"
        # only get here on error
        return 1
    fi

    # Running interactively

    _shournal_verbose_history_check || return 1
    _shournal_session_uuid="$(shournal-run --make-session-uuid)" || return 1

    _shournal_enable
    return
}


SHOURNAL_DISABLE() {
    # if running non-interactively, disabling SHOURNAL does not have an effect.
    [[ -n ${_shournal_parent_launched_us_noninteractive+x} ]] && return 0
    [[ -z ${_shournal_is_running+x} ]] && return 1

    _shournal_remove_setup_error_file_if_exist
    _shournal_remove_prompts
    _shournal_detach_this_pid

    unset _shournal_is_running _shournal_last_cmd_seq \
          _shournal_session_uuid _shournal_cmd_seq_hotfix
}

# $1: pass one of dbg, info, warning, critical
SHOURNAL_SET_VERBOSITY(){
    case "$1" in
    "dbg")
        _shournal_bash_integration_log_level=0
        ;;
    "info")
        _shournal_bash_integration_log_level=1
        ;;
    "warning")
        _shournal_bash_integration_log_level=2
        ;;
    "critical")
        _shournal_bash_integration_log_level=3
        ;;
    *)
        _shournal_warn "Bad verbosity passed. Pass one of dbg, info, warning, critical"
        return 1
        ;;
    esac

    _SHOURNAL_VERBOSITY="$1"
}


######################## PRIVATE ########################


_shournal_enable(){
    _shournal_debug "${FUNCNAME[0]}"

    # May be necessary in cases where setup fails but exec was called, so
    # we couldn't clean up previously.
    _shournal_remove_setup_error_file_if_exist

    [[ -z ${PS0+x} ]] && PS0=''
    [[ -z ${PS1+x} ]] && PS1=''
    [[ -z ${PROMPT_COMMAND+x} ]] && PROMPT_COMMAND=''
    # Usually removing prompts should not be necessary here,
    # however, if a user exports PS0/PS1/PROMPT_COMMAND
    # and starts a new bash-session, we need to get rid of the existing commands.
    _shournal_remove_prompts
    _shournal_add_prompts
    _shournal_is_running=true
}


# Find a fifo (if any) that was created by this shell previously and
# instruct the belonging shournal-run process to stop
# observing this pid.
_shournal_detach_this_pid(){
    local fifo_pid_path
    local fifopath
    _shournal_fifopath_current_pid fifo_pid_path || return
    # use globbing to ignore the sequence number
    fifopath=$(compgen -G "$fifo_pid_path*")
    if [ $? -eq 0 ]; then
        fifopath=${fifopath%%$'\n'*} # should not be necessary
        _shournal_debug "${FUNCNAME[0]}: finalizing fifo $fifopath..."
        _shournal_run_finalize "$fifopath" 0
    fi
}


_shournal_run_finalize(){
    local fifopath="$1"
    local exitcode="$2"
    local _shournal_fifofd

    # We race with the fifo-removal of a potential previous
    # PS1 and the removal shournal-run performs. To
    # avoid creating a file (instead of writing to the fifo), whose
    # event pollutes shournal's history, first open a descriptor
    # read only and on success write to it later using /proc/$BASHPID/fd/$_shournal_fifofd
    # This also protects us from a potential deadlock. shournal-run first deletes
    # the fifo and then closes it. If it is deleted after we opened it readonly,
    # reopen and writing to it does *not* block.
    if ! { exec {_shournal_fifofd}<"$fifopath"; } 2>/dev/null; then
        _shournal_debug "${FUNCNAME[0]}: opening fifopath \"$fifopath\" failed."
        return 0
    fi
    _shournal_send_ret_val $_shournal_fifofd $exitcode
    _shournal_send_unmark_pid $_shournal_fifofd $$
    rm "$fifopath" 2>/dev/null
    exec {_shournal_fifofd}<&-
}

# PS0 is run before a valid command (but not when ENTER or Ctrl+C is hit).
# We launch a shournal-run process and wait for it to setup and
# fork into background. Note that the current command sequence \# is not
# incremented yet
_shournal_ps0(){
    local current_seq="$1"
    local fifopath
    local cmd_str
    local last_pid
    local setup_err_path

    $_shournal_cmd_seq_hotfix && current_seq=$((current_seq - 1))

    if ! _shournal_verbose_history_check; then
        _shournal_warn "history settings were modified after the bash integration was turned on. " \
                       "Please correct that or call SHOURNAL_DISABLE " \
                       "to get rid of this message."
        return 1
    fi
    _shournal_fifopath_from_seq $current_seq fifopath || return 1
    _shournal_debug "${FUNCNAME[0]}: using fifo at $fifopath"
    _shournal_warn_on "[[ -e $fifopath ]]"

    _shournal_get_current_cmd cmd_str
    # Argument --close-fds is important here for the following reasons:
    # * We run within a subshell which waits for redirected stdout to
    #   close (deadlock otherwise).
    # * We have created a custom redirection, e.g. with
    #   exec 3> foo; echo "test" >&3;
    #   exec 3>&-; # closes 3
    #   In this case **without closing** within shournal-run the close event would be lost,
    #   as the final __fput is reached during shournal-run exit().
    shournal-run --verbosity "$_SHOURNAL_VERBOSITY" --pid $$ --fork \
        --close-fds --fifoname "$fifopath" \
        --shell-session-uuid "$_shournal_session_uuid" \
        --cmd-string "$cmd_str" &
    last_pid=$!
    # wait for shournal setup (another shournal-fork performs the event processing)
    wait $last_pid
    if [ $? -ne 0 ]; then
        # only debug here - there should already be two warnings - one from
        # shournal-run or bash not able to execute and (likely) one afterwards
        # from _shournal_prompt.
        _shournal_debug "${FUNCNAME[0]}: shournal-run setup failed"
        _shournal_setup_error_path_current_pid setup_err_path
        # maybe_todo: If we successfully call "exec" immediatly afterwards,
        # we leak this file. This however should happen rarely.
        echo 1 > "$setup_err_path"
        return 1
    fi
    return 0
}

# Disable the shell-integration in case of setup-errors, to avoid
# spamming the user. Setup may in particular fail in cases where
# shournal is updated while the kernel module of the old version
# is still active.
# Note that other than _shournal_ps0 and _shournal_ps1 this
# function is executed in the *parent shell*.
_shournal_prompt(){
    local ret=$?
    local setup_err_path

    _shournal_setup_error_path_current_pid setup_err_path
    if test -e "$setup_err_path"; then
        _shournal_warn "Disabling the shell-integration due to previous setup-erros..."
        SHOURNAL_DISABLE
    fi
    return $ret
}

# PS1 is run after any command, but also after hitting ENTER or Ctrl (other than PS0).
# However, the command sequence \# ($1) was only incremented in case of valid commands. So we try
# to open the fifo-path using the last (decremented) \#. If open succeeds, write the last exit-code
# to the fifo and unmark this process.
_shournal_ps1(){
    local exitcode=$?
    local current_seq="$1"
    local previous_seq=$((current_seq - 1))
    local fifopath
    local fifopath_ret

    _shournal_fifopath_from_seq $previous_seq fifopath
    if [ $? -ne 0 ]; then
        _shournal_debug "${FUNCNAME[0]}: fifopath_from_seq failed for seq $previous_seq"
        return 1
    fi
    _shournal_debug "${FUNCNAME[0]}: using fifo at $fifopath"

    _shournal_run_finalize "$fifopath" "$exitcode"

    return $exitcode
}

_shournal_send_msg(){
    # send json string to last started shournal.
    # for the different message types (msgType), see enum FIFO_MSG in c++.
    local fifofd="$1"
    local msg_type="$2"
    local msg_data="$3"
    # simple json string type-data: { "msgType":0, "data":"stuff" }
    local full_msg="{\"msgType\":$msg_type,\"data\":\"$msg_data\"}"
    _shournal_debug "${FUNCNAME[0]}: sending message: $full_msg"
    # see shournal's ps1 for the rationale of using a fd instead of fifopath
    # BASHPID is different from $$ in subshells!
    echo "$full_msg" > /proc/$BASHPID/fd/$fifofd
}

_shournal_send_ret_val(){
    _shournal_send_msg "$1" 0 "$2"
}

_shournal_send_unmark_pid(){
    _shournal_send_msg "$1" 1 "$2"
}

_shournal_add_prompts(){
    PS0="$PS0"'`_shournal_ps0 \#`'
    PS1="$PS1"'`_shournal_ps1 \#`'
    PROMPT_COMMAND=$'_shournal_prompt\n'"$PROMPT_COMMAND"
}

_shournal_remove_prompts(){
    PS0=${PS0//'`_shournal_ps0 \#`'/}
    PS1=${PS1//'`_shournal_ps1 \#`'/}
    PROMPT_COMMAND=${PROMPT_COMMAND//_shournal_prompt$'\n'/}
}

_shournal_setup_error_path_current_pid(){
    declare -n ret=$1
    ret="/dev/shm/shournal-setup-error-$USER-$$"
}

_shournal_remove_setup_error_file_if_exist(){
    local setup_err_path
    _shournal_setup_error_path_current_pid setup_err_path
    test -e "$setup_err_path" && rm "$setup_err_path"
}

_shournal_fifopath_current_pid(){
    declare -n ret=$1
    local tmpdir
    [[ -n "$TMPDIR" ]] && tmpdir="$TMPDIR" || tmpdir=/tmp
    ret="$tmpdir/shournal-fifo-$USER-$$"
}

_shournal_fifopath_from_seq(){
    local seq="$1"
    declare -n ret=$2
    local basepath
    _shournal_fifopath_current_pid basepath || return
    ret="$basepath-$seq"
    return 0
}


_shournal_get_current_cmd(){
    declare -n ret=$1
    # history output is e.g.
    # " 6989  echo foo"
    # so strip the leading " 6989  "
    local cmd="$(HISTTIMEFORMAT='' history 1)"
    [[ "$cmd" =~ ([[:space:]]*[0-9]+[[:space:]]*)(.*) ]]
    ret="${BASH_REMATCH[2]}"
}


# returns 0, if all history settings were ok, else false.
# is verbose, if a setting is not ok.
_shournal_verbose_history_check(){
    # no history needed if running non-interactively
    [[ -n ${BASH_EXECUTION_STRING+x} ]] && return 0
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


# don't call it directly, but use one of debug, info, warning, error functions
# $1: logleve.
# all other args: is printed to stderr
_shournal_log_msg(){
    local loglevel=$1
    shift
    [[ $loglevel -ge $_shournal_bash_integration_log_level ]] &&
        >&2  printf "shournal bash integration - $*\n"
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



