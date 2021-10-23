# Select which shell-integration to be sourced based on config
# and availability.
# The backend is chosen in the following order:
# • variable SHOURNAL_BACKEND
# • config file at shournal's user cfg-dir
# • config file at /etc...
# Else default to the ko-backend or, if not found, the fanotify-backend.

__shournal_eprint(){
    >&2 printf "shournal-backend-selection: $*\n"
}

__shournal_cmd_exists(){
     [ -x "$(command -v "$1")" ]
}

__shournal_select_backend(){
    local scriptname="$1"
    local this_shell="$2"
    local backend_name_ko
    local backend_name_fan
    local backend_name_selected
    local backend_config_file
    local backend_origin

    backend_name_ko="integration_ko.$this_shell"
    backend_name_fan="integration_fan.$this_shell"

    if [ -n "${SHOURNAL_BACKEND+x}" ]; then
        backend_name_selected="$SHOURNAL_BACKEND"
        backend_origin="variable SHOURNAL_BACKEND"
    else
        for p in "$HOME/.config/shournal/backend" \
                 "/etc/shournal.d/backend"; do
            if test -f "$p"; then
                backend_config_file="$p"
                break
            fi
        done
    fi

    if [ -n "$backend_config_file" ]; then
        read -r backend_name_selected < "$backend_config_file"
        backend_origin="$backend_config_file"
    fi

    if [ "$backend_name_selected" = 'ko' ]; then
            backend_name_selected="$backend_name_ko"
        elif [ "$backend_name_selected" = 'fanotify' ]; then
            backend_name_selected="$backend_name_fan"
        else
            backend_name_selected=""
            __shournal_eprint "Unsupported backend $backend_name_selected set in " \
                              "$backend_origin. Supported options: [fanotify, ko]. " \
                              "Using defaults..."
    fi

    if [ -z "$backend_name_selected" ]; then
        if __shournal_cmd_exists 'shournal-run'; then
            backend_name_selected="$backend_name_ko"
        elif __shournal_cmd_exists 'shournal-run-fanotify'; then
            backend_name_selected="$backend_name_fan"
        else
            __shournal_eprint "Error: commands shournal-run and " \
                              "shournal-run-fanotify were not found in PATH."
            return 1
        fi
    fi
    . "$(dirname -- "$scriptname")/$backend_name_selected"
}

__shournal_select_backend_return=0
if [ -n "${BASH_VERSION+x}" ]; then
    __shournal_select_backend "$BASH_SOURCE" bash ||
        __shournal_select_backend_return=$?
elif [ -n "${ZSH_VERSION+x}" ]; then
    # This has to be in global scope, $0 is different within function.
    __shournal_select_backend "$0" zsh ||
        __shournal_select_backend_return=$?
else
    __shournal_eprint "called from unsupported shell [currently only bash is supported]"
    __shournal_select_backend_return=1
fi

unset __shournal_eprint
unset __shournal_cmd_exists
unset __shournal_select_backend

return $__shournal_select_backend_return
