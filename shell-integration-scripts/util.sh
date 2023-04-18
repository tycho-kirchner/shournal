

# don't call it directly, but use one of debug, info, warning, error functions
# $1: loglevel.
# all other args: is printed to stderr
_shournal_log_msg(){
    local loglevel=$1
    shift
    [ "$loglevel" -ge "$_shournal_shell_integration_log_level" ] &&
        >&2  printf "shournal $_SHOURNAL_SHELL_NAME integration - $*\n"
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


# returns true, if $1 starts with $2
_shournal_startswith() {
    case $1 in
        "$2"*) return 0;;
        *) return 1;;
    esac;
}

# returns true, if $1 ends with $2
_shournal_endswith() {
    case $1 in
        *"$2") return 0;;
        *) return 1;;
    esac;
}

# returns true, if $1 contains $2
_shournal_contains() {
    case $1 in
        *"$2"*) return 0;;
        *) return 1;;
    esac;
}

# Trim leading and trailing spaces
_shournal_trim(){
    echo -e "${1}" | sed -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//'
}

_shournal_is_clusterjob(){
    # Recent versions of the sun grid engine (SGE) use cgroups to manage
    # processes of a job. Once the main job-script finishes, all leftovers
    # are killed. Therefore, shournal cannot flush the events to the
    # database in background afterwards, so we fall back to foreground execution.
    # Usually SHOURNAL_ENABLE is expected to be part of the shell's rc,
    # e.g. ~/.bashrc, which we require for a "safe" re-execution.
    # A cluster-job should be: non-interactive and running within a login-shell.
    [ -n "${SHOURNAL_IS_CLUSTERJOB+x}" ] && return 0
    [ -z "${SGE_O_WORKDIR+x}"  -o  -z "${JOB_NAME+x}"  -o  -t 0  -o \
      -t 1  -o  -t 2  -o  -n "${SHOURNAL_NO_CLUSTER_JOB_DETECT+x}" ] && return 1
    _shournal_sh_is_interactive && return 1
    return 0
}

_shournal_clusterjob_reexec_ok(){
    # s. _shournal_is_clusterjob
    _shournal_is_clusterjob || return $?
    if ! _shournal_verbose_reexec_allowed; then
        _shournal_warn "cluster job detected, but we are not allowed to re-exec." \
                       "Please check your environment."
        return 1
    fi
    return 0
}

_shournal_is_subshell(){
    _shournal_refresh_current_pid || return $?
    [ $_shournal_current_pid -ne $$ ] && return 0
    return 1
}

_shournal_refresh_current_pid(){
    local pid ret=0
    read -d ' ' pid < /proc/self/stat || ret=$?
    if [ $ret -ne 0 ]; then
        _shournal_error "_shournal_refresh_current_pid:" \
            "failed to read from /proc/self/stat: $ret"
        return $ret
    fi
    _shournal_current_pid=$pid
    return 0
}

# Non-portable, shell specific functions:
case "$_SHOURNAL_SHELL_NAME" in
  'bash')

# returns 0, if all history settings were ok, else false.
# is verbose, if a setting is not ok.
_shournal_verbose_history_check(){
    # no history needed if running non-interactively
    [ -n "${BASH_EXECUTION_STRING+x}" ] && return 0
    local ret=0

    if ! [ -o history ]; then
        ret=1
        _shournal_error "bash history is off. Please enable it: set -o history"
    fi

    if [[ ${HISTSIZE-0} -lt 2 ]]; then
        ret=1
        _shournal_error "bash HISTSIZE is too small (or not set). Please set it at least to 2: HISTSIZE=2"
    fi

    if [[ ${HISTCONTROL-} == *"ignorespace"* || ${HISTCONTROL-} == *"ignoreboth"* ]]; then
        ret=1
        _shournal_error "Commands with spaces are set to be ignored from history. Please disable that, " \
                       "e.g. HISTCONTROL=ignoredups or HISTCONTROL=''"
    fi

    if [[ -n ${HISTIGNORE-} ]] ; then
        ret=1
        _shournal_error "HISTIGNORE is not empty. Please unset it: unset HISTIGNORE"
    fi
    return $ret
}

_shournal_get_current_cmd_bash(){
    declare -n ret=$1
    local cmd
    # history output is e.g.
    # " 6989  echo foo"
    # so strip the leading " 6989  "
    cmd="$(HISTTIMEFORMAT='' history 1)"
    [[ "$cmd" =~ ([[:space:]]*[0-9]+[[:space:]]*)(.*) ]]
    ret="${BASH_REMATCH[2]}"
    return 0
}

_shournal_print_current_cmd(){
    local cmd_str
    _shournal_get_current_cmd_bash cmd_str
    printf '%s\n' "$cmd_str"
}

_shournal_refresh_current_pid(){
    _shournal_current_pid=$BASHPID
    return 0
}

_shournal_sh_is_interactive(){
    [[ $- == *i* ]] && return 0
    return 1
}


# Return true, if we are allowed to reexec, which we do in case
# of non-interactive ssh commands (for the fanotify backend), where the
# BASH_EXECUTION_STRING is always set, or in case of cluster jobs, which
# are usually invoked as login_shell.
# Re-exec is not allowed, if the a command
# within the -c '..' arg was already executed
# (it would be executed twice otherwise).
_shournal_verbose_reexec_allowed(){
    local i sourced_from_bashrc
    if [[ -z ${BASH_EXECUTION_STRING+x} ]] && ! shopt -q login_shell; then
        return 1
    fi
    # Only consider this a running cluster job, if we are sourced from .bashrc.
    # FIXME: this is not robust, bash -c 'echo foo; source ~/.bashrc'
    # should _not_ be allowed.
    if [[ -z ${BASH_SOURCE+x} ]]; then
        _shournal_error "BASH_SOURCE is not set. Something is seriously" \
            "wrong here, aborting..."
        return 1
    fi
    sourced_from_bashrc=false
    for ((i=1; i<${#BASH_SOURCE[@]}; i++)); do
        if [[ "${BASH_SOURCE[i]##*/}" == .bashrc ]]; then
            sourced_from_bashrc=true
            break
        fi
    done

    if [[ $sourced_from_bashrc == false ]]; then
        _shournal_warn "The command was considered for re-execution, but" \
                       "we require to be sourced from .bashrc for SHOURNAL_ENABLE." \
                       "Alternatively, invoke »shournal -e« directly."
        return 1
    fi
    return 0
}

;;   # END_OF bash
    'zsh')

_shournal_verbose_history_check(){
    # no history needed if running non-interactively
    [ -n "${ZSH_EXECUTION_STRING+x}" ] && return 0

    # While in bash we retrieve the command-string
    # from history, in zsh it is directly passed
    # to our preexec_function, which seems to work
    # regardless of history options like e.g.
    # $ setopt HIST_IGNORE_SPACE
    return 0
}


_shournal_print_current_cmd(){
    printf '%s\n' "$history[$HISTCMD]"
}

_shournal_sh_is_interactive(){
    [[ -o interactive ]] && return 0
    return 1
}

_shournal_verbose_reexec_allowed(){
    local toplevel_contex
    if [[ -z ${ZSH_EXECUTION_STRING+x} && ! -o login ]]; then
        return 1
    fi
    zmodload zsh/parameter
    toplevel_context="${zsh_eval_context[1]}"
    case "$toplevel_context" in
    file) :;;
    cmdarg)
        _shournal_warn "eval-toplevel-context $toplevel_context not allowed"
        return 1;;
    *)
        _shournal_warn "unhandled eval-toplevel-context $toplevel_context." \
                       "Please report if you" \
                       "think that SHOURNAL_ENABLE should be possible here."
        return 1;;
    esac
    return 0
}

;;  # END_OF zsh
  *)
    echo "shournal shell integration: something is seriously wrong, " \
         "_SHOURNAL_SHELL_NAME is not correctly setup" >&2
;;
esac
