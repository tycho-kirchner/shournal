

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


;;  # END_OF zsh
  *)
    echo "shournal shell integration: something is seriously wrong, " \
         "_SHOURNAL_SHELL_NAME is not correctly setup" >&2
;;
esac
