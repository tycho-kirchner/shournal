# bash-integration for shournal.
# You must provide a valid path for shournals libshournal-shellwatch.so
# before executing SHOURNAL_ENABLE, e.g.:
# SHOURNAL_PATH_LIB_SHELL_INTEGRATION="/usr/local/lib64/shournal/libshournal-shellwatch.so"

SHOURNAL_PRINT_VERSIONS() {
    echo "shournal bash integration version $_shournal_version"
    if [[ -n ${_shournal_is_running+x} ]] ; then
        _libshournal_print_version
    else
        echo "To see the version of shournal's shell-integration (libshournal-shellwatch.so) please SHOURNAL_ENABLE first"
    fi

}


SHOURNAL_ENABLE() {
    # To allow for SHOURNAL_ENABLE in bashrc, make sure to not enable again
    [[ -n ${_shournal_enable_just_called+x} ]] && return 0

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
    if [[ ! -u $path_to_shournal_run ]]; then
        _shournal_debug "shournal-run does not have the setuid-bit set." # but this might be a wrapper, so no warning
    fi

    declare -a args_array
    while IFS= read -r -d '' line; do
        args_array+=("$line")
    done < /proc/$$/cmdline

    local cmd_path="$(readlink /proc/$$/exe)"

    if [[ -n ${BASH_EXECUTION_STRING+x} ]]; then
        # The bash executions string is e.g. set when running bash -c 'echo foo', in which case we never get to
        # any prompt. Do do not preload our .so, but instead execute the whole command
        # within shournal.
        # Checking $BASH_EXECUTION_STRING seems to be more reliable than [[ $- == *i* ]], because
        # of commands like e.g. bash -i -c 'echo "wtf - is that interactive?"'
        _shournal_debug "${FUNCNAME[0]}: exec non-interactive $cmd_path ${args_array[@]}"
        _shournal_parent_launched_us_noninteractive=true exec \
            shournal --verbosity "$_SHOURNAL_LIB_SHELL_VERBOSITY" --exec-filename "$cmd_path" --exec "${args_array[@]}"
        # only get here on error
        return 1
    fi

    # Running interactively - use our .so
    # If it is not alreay loaded, "relaunch" this process (exec with same args)
    # within the "original" mount namespace and the .so preloaded.

    _shournal_verbose_history_check || return 1

    if _libshournal_is_loaded ; then
        # no need to preload/exec, if libshournal-shellwatch.so is already loaded
        _shournal_enable
        return
    fi

    if [[ ! -f ${SHOURNAL_PATH_LIB_SHELL_INTEGRATION-} ]]; then
        _shournal_error "Please provide a valid path for libshournal-shellwatch.so, e.g. " \
                        "(export SHOURNAL_PATH_LIB_SHELL_INTEGRATION='/usr/local/lib/shournal/libshournal-shellwatch.so')"

        return 1
    fi

    export _shournal_enable_just_called=true
    export LD_PRELOAD=${LD_PRELOAD-}":$SHOURNAL_PATH_LIB_SHELL_INTEGRATION"
    _shournal_debug "${FUNCNAME[0]}: calling preloaded: $cmd_path ${args_array[@]:1}"
    # Relaunch bash with shournals .so preloaded using the original arguments.
    exec shournal --verbosity "$_SHOURNAL_LIB_SHELL_VERBOSITY"  \
        --msenter-orig-mountspace --exec-filename "$cmd_path" --exec "${args_array[@]}"
    # only get here on error
    return 1
}


SHOURNAL_DISABLE() {
    export _SHOURNAL_LAST_RETURN_VALUE=$?
    # if running non-interactively, disabling SHOURNAL does not have an effect.
    [[ -n ${_shournal_parent_launched_us_noninteractive+x} ]] && return 0
    [[ -z ${_shournal_is_running+x} ]] && return 1

    # remove prompts.
    PROMPT_COMMAND=${PROMPT_COMMAND//_shournal_prompt_start$'\n'/}
    PROMPT_COMMAND=${PROMPT_COMMAND//$'\n'_shournal_prompt_stop/}

    # if this function was called within a sequence, cleanup first, to also know
    # the command
    _libshournal_cleanup_cmd
    _libshournal_disable

    unset _shournal_is_running
    unset _SHOURNAL_LAST_RETURN_VALUE
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

    export _SHOURNAL_LIB_SHELL_VERBOSITY=$1
}


######################## PRIVATE ########################

# Do *not* touch the next line. The version is updated automatically on build from cmake according
# to the version set there
_shournal_version=2.2

# 0: debug, 1: info, 2: warning, 3: error
[[ -z ${_shournal_bash_integration_log_level+x} ]] && _shournal_bash_integration_log_level=2
[[ -z ${_SHOURNAL_LIB_SHELL_VERBOSITY+x} ]] && export _SHOURNAL_LIB_SHELL_VERBOSITY="warning"


_shournal_enable(){
    _shournal_debug "${FUNCNAME[0]}"
    export _SHOURNAL_SHELL_NAME="bash"

    unset _shournal_enable_just_called

    if [[ -n ${LD_PRELOAD-} ]] ; then
        # note: processes launched by this shell will not be observed by the preloaded library!
        LD_PRELOAD=${LD_PRELOAD//:$SHOURNAL_PATH_LIB_SHELL_INTEGRATION/}
    fi

    # Usually this should not be necessary, however, if a user exports the PROMPT_COMMAND
    # and starts a bash-session, we need to take care of the existing _shournal_prompts.
    [[ -z ${PROMPT_COMMAND+x} ]] && PROMPT_COMMAND=''
    PROMPT_COMMAND=${PROMPT_COMMAND//_shournal_prompt_start$'\n'/}
    PROMPT_COMMAND=${PROMPT_COMMAND//$'\n'_shournal_prompt_stop/}

    # Prepend and append dummy functions to $PROMPT_COMMAND, which is used
    # to determine, when a command-sequence has ended.
    PROMPT_COMMAND=$'_shournal_prompt_start\n'"$PROMPT_COMMAND"
    PROMPT_COMMAND+=$'\n_shournal_prompt_stop'

    _libshournal_enable
    _shournal_is_running=true

}


_shournal_prompt_start(){
    export _SHOURNAL_LAST_RETURN_VALUE=$?
    _libshournal_cleanup_cmd
    return $_SHOURNAL_LAST_RETURN_VALUE
}

# executed right BEFORE the next command can be entered
_shournal_prompt_stop() {
    _shournal_debug "${FUNCNAME[0]}"
    # user might modify history settings at any time, so better be safe:
    if ! _shournal_verbose_history_check; then
        _shournal_warn "history settings were modified after the bash integration was turned on. " \
                       "Turning the bash integration off..."
        SHOURNAL_DISABLE
        return $_SHOURNAL_LAST_RETURN_VALUE
    fi

    _libshournal_prepare_cmd
    return $_SHOURNAL_LAST_RETURN_VALUE

}

_libshournal_enable(){
    _shournal_trigger_update 0
}

_libshournal_disable(){
    _shournal_trigger_update 1
}

_libshournal_prepare_cmd(){
    _shournal_trigger_update 2
}

_libshournal_cleanup_cmd(){
    export _SHOURNAL_LAST_COMMAND="$(_shournal_get_current_cmd)"
    _shournal_trigger_update 3
    unset _SHOURNAL_LAST_COMMAND
}

_libshournal_print_version(){
    _shournal_trigger_update 4
}



_libshournal_is_loaded(){
    _shournal_trigger_update 5
    # _LIBSHOURNAL_TRIGGER is unset by libshournal, if  it's loaded
    [[ -z ${_LIBSHOURNAL_TRIGGER+x} ]] && return 0
    unset _LIBSHOURNAL_TRIGGER
    return 1
}


# $1: desired state
_shournal_trigger_update(){
    # trigger is unset immediately in our .so, if active
    export _LIBSHOURNAL_TRIGGER=$1
    echo '' > /dev/null # close event is recorded in our .so
}

_shournal_get_current_cmd(){
    # history output is e.g.
    # " 6989  echo foo"
    # so strip the leading " 6989  "
    local cmd="$(HISTTIMEFORMAT='' history 1)"
    [[ "$cmd" =~ ([[:space:]]*[0-9]+[[:space:]]*)(.*) ]]; echo "${BASH_REMATCH[2]}"
}


# returns 0, if all history settings were ok, else false.
# is verbose, if a setting is not ok.
_shournal_verbose_history_check(){
    # no history needed if running non-interactively
    [[ -n ${BASH_EXECUTION_STRING+x} ]] && return 0
    local success=true

    if ! [ -o history ]; then
        success=false
        _shournal_warn "bash history is off. Please enable it: set -o history"
    fi

    if [[ ${HISTSIZE-0} -lt 2 ]]; then
        success=false
        _shournal_warn "bash HISTSIZE is too small (or not set). Please set it at least to 2: HISTSIZE=2"
    fi

    if [[ ${HISTCONTROL-} == *"ignorespace"* || ${HISTCONTROL-} == *"ignoreboth"* ]]; then
        success=false
        _shournal_warn "Commands with spaces are set to be ignored from history. Please disable that, " \
                       "e.g. HISTCONTROL=ignoredups or HISTCONTROL=''"
    fi

    if [[ -n ${HISTIGNORE-} ]] ; then
        success=false
        _shournal_warn "HISTIGNORE is not empty. Please unset it: unset HISTIGNORE"
    fi

    $success
    return $?
}


# don' call it directly, but use one of debug, info, warning, error functions
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


if [[ -n ${_shournal_enable_just_called+x} ]] ; then
    # a parent proccess has called SHOURNAL_ENABLE, which executed bash.
    # libshournal-shellwatch.so should now be loaded (dynamically) and the
    # observation can begin
    if ! _libshournal_is_loaded ; then
        _shournal_error "Although _'shournal_enable_just_called' is set, libshournal-shellwatch.so seems " \
                        "to be not loaded (bug?)."
        return
    fi
    _shournal_enable
fi

