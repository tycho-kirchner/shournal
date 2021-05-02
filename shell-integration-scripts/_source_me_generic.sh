# Select which shell-integration to be sourced based on config
# and availability. If a config file at shournal's user cfg-dir or
# at /etc... exists, use the backend
# configured there, else use the ko-backend or, if not found, the fanotify-backend.

__shournal_eprint(){
    >&2 printf "shournal-backend-selection: $*\n"
}

__shournal_cmd_exists(){
     [ -x "$(command -v "$1")" ]
}

__shournal_select_backend(){
    local this_shell
    local backend_name_ko
    local backend_name_fan
    local backend_name_selected
    local backend_config_file
    local IFS=
    local scriptname

    if [ -n "$BASH_VERSION" ]; then
       this_shell='bash'
       scriptname="$BASH_SOURCE"
    # Once zsh-support is ready:
    # elif [ -n "$ZSH_VERSION" ]; then
    #    this_shell='zsh'
    #    scriptname="$0"
    else
        __shournal_eprint "called from unsupported shell [currently only bash is supported]"
        return 1
    fi
    backend_name_ko="integration_ko.$this_shell"
    backend_name_fan="integration_fan.$this_shell"

    for p in "$HOME/.config/shournal/backend" \
             "/etc/shournal.d/backend"; do
        if test -f "$p"; then
            backend_config_file="$p"
            break
        fi
    done

    if [ -n "$backend_config_file" ]; then
        read -r backend_name_selected < "$backend_config_file"
        if [ "$backend_name_selected" = 'ko' ]; then
            backend_name_selected="$backend_name_ko"
        elif [ "$backend_name_selected" = 'fanotify' ]; then
            backend_name_selected="$backend_name_fan"
        else
            backend_name_selected=""
            __shournal_eprint "Unsupported backend $backend_name_selected set in " \
                              "$backend_config_file. Supported options: [fanotify, ko]. " \
                              "Using defaults..."
        fi
    fi

    if [ -z "$backend_name_selected" ]; then
        if __shournal_cmd_exists 'shournal-run'; then
            backend_name_selected="$backend_name_ko"
        elif __shournal_cmd_exists 'shournal-run-fanotify'; then
            backend_name_selected="$backend_name_fan"
        else
            __shournal_eprint "Commands shournal-run and shournal-run-fanotify " \
                              "were not found in PATH."
            return 1
        fi
    fi

    . "$(dirname -- "$scriptname")/$backend_name_selected"
}

__shournal_select_backend
__shournal_select_backend_return=$?

unset __shournal_eprint
unset __shournal_cmd_exists
unset __shournal_select_backend

return $__shournal_select_backend_return
