
# shell-integration for shournal - fanotify backend.


_shournal_run_backend='shournal-run-fanotify'


_shournal_enable(){
    if [ -n "${_shournal_is_running+x}" ] ; then
        _shournal_warn_on '! _libshournal_is_loaded'
        _shournal_debug "_shournal_enable: current session is already observed"
        return 0
    fi

    if ! _libshournal_is_loaded ; then
        if [ -z "${_shournal_enable_just_called+x}" ]; then
            _shournal_exec_ldpreloaded_shell
        else
            _shournal_warn "Something went wrong during preloading of " \
                           "libshournal-shellwatch.so, the shell integration " \
                           "is _not_ enabled."
            unset _shournal_enable_just_called
        fi
        # only get here on error
        return 1
    fi
    _shournal_debug "_shournal_enable: about to enable..."
    unset _shournal_enable_just_called

    if [ -n "${LD_PRELOAD+x}" ] ; then
        # note: processes launched by this shell will not be observed by the preloaded library!
        LD_PRELOAD=${LD_PRELOAD//:$SHOURNAL_PATH_LIB_SHELL_INTEGRATION/}
    fi

    _libshournal_enable || return 1
    _shournal_remove_prompts
    _shournal_add_prompts
    _shournal_is_running=true
    return 0
}


_shournal_disable(){
    local exitcode=$1

    # if running non-interactively, disabling SHOURNAL does not have an effect.
    [ -n "${_shournal_parent_launched_us_noninteractive+x}" ] && return 0
    [ -z "${_shournal_is_running+x}" ] && return 0

    _shournal_debug "_shournal_disable: about to disable..."

    # In case we were called in a sequence, e.g.
    # $ (exit 123); SHOURNAL_DISABLE
    # we should cleanup first (otherwise the command was lost).
    # Be careful to avoid endless shutdown recursion here
    # ( _shournal_postexec may call SHOURNAL_DISABLE in case of erros)
    if [ -z "${_shournal_during_shutdown+x}" ]; then
        _shournal_during_shutdown=true
        _shournal_postexec "$exitcode"
    fi
    _libshournal_disable || _shournal_warn "_libshournal_disable failed"
    _shournal_remove_prompts

    unset _shournal_during_shutdown
    unset _shournal_is_running
    return 0
}

_shournal_set_verbosity(){
    local ret=0
    # for libshournal-shellwatch.so
    export _SHOURNAL_LIB_SHELL_VERBOSITY="$1"
    if _libshournal_is_loaded; then
        _libshournal_update_verbosity || ret=$?
    fi
    return $ret
}

_shournal_print_versions(){
    echo "shournal-run-fanotify: $(shournal-run-fanotify --version)"
    if _libshournal_is_loaded ; then
        _libshournal_print_version || _shournal_warn "printing version failed."
    else
        echo "To see the version of shournal's shell-integration "\
             " (libshournal-shellwatch.so) please SHOURNAL_ENABLE first"
    fi
    return 0
}


# If our libshournal-shellwatch.so is not loaded yet, do so
# by "relaunching" this process (exec with same args)
# within the "original" mount namespace.
_shournal_exec_ldpreloaded_shell(){
    declare -a args_array
    local cmd_path

    if ! [ -f "${SHOURNAL_PATH_LIB_SHELL_INTEGRATION-}" ]; then
        _shournal_error "Please provide a valid path for libshournal-shellwatch.so, e.g. " \
                        "export SHOURNAL_PATH_LIB_SHELL_INTEGRATION=" \
                        "'/usr/local/lib/shournal/libshournal-shellwatch.so'"
        return 1
    fi
    while IFS= read -r -d '' line; do
            args_array+=("$line")
    done < /proc/$$/cmdline
    cmd_path="$(readlink /proc/$$/exe)"
    export _shournal_enable_just_called=true
    export LD_PRELOAD=${LD_PRELOAD-}":$SHOURNAL_PATH_LIB_SHELL_INTEGRATION"
    _shournal_debug "_shournal_exec_ldpreloaded_shell: calling preloaded " \
                    "$cmd_path ${args_array[@]:1}"
    # Relaunch the shell with shournals .so preloaded using the original arguments.
    exec shournal --verbosity "$_SHOURNAL_VERBOSITY"  \
        --backend-filename 'shournal-run-fanotify' \
        --msenter-orig-mountspace \
        --exec-filename "$cmd_path" --exec -- "${args_array[@]}"
    # only get here on error
    return 1

}


_shournal_preexec_generic(){
    _libshournal_prepare_cmd || :
}


_shournal_postexec_generic(){
    local cmd_str="$1"
    local exitcode="$2"

    _shournal_debug "_shournal_postexec"

    # user might modify history settings at any time, so better be safe:
    if ! _shournal_verbose_history_check; then
        _shournal_warn "history settings were modified after the shell " \
                       "integration was turned on. " \
                       "Turning the shell integration off..."
        # Be careful to avoid endless shutdown recursion here.
        if [ -z "${_shournal_during_shutdown+x}" ]; then
            _shournal_during_shutdown=true
            SHOURNAL_DISABLE
        fi
        return 1
    fi
    export _SHOURNAL_LAST_COMMAND="$cmd_str"
    export _SHOURNAL_LAST_RETURN_VALUE=$exitcode

    # cleanup may fail regularily, this function may also be executed
    # when no command ran before (e.g. when hitting enter on a blank line in bash)
    _libshournal_cleanup_cmd || :

    unset _SHOURNAL_LAST_COMMAND
    unset _SHOURNAL_LAST_RETURN_VALUE

    return 0
}

# «Send» messages to libshournal-shellwatch.so by exporting a
# variable and triggering a dummy-close event.
_shournal_trigger_update(){
    local desired_state="$1"
    local trigger_response
    local ret

    export _LIBSHOURNAL_TRIGGER="$desired_state";

    # Note: our .so detects this special (non-)filename and
    # writes its response to an unnamed tmp file.
    ret=0
    read -d '' trigger_response < '_///shournal_trigger_response///_' || ret=$?
    unset _LIBSHOURNAL_TRIGGER

    if [ $ret -ne 0 ]; then
        _shournal_debug "_shournal_trigger_update: failed to read " \
                        "trigger_response"
        return 1
    fi
    [ "$trigger_response" = ok ] && return 0 || return 1
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
    _shournal_trigger_update 3
}

_libshournal_print_version(){
    _shournal_trigger_update 4
}

_libshournal_update_verbosity(){
    _shournal_trigger_update 5
}

_libshournal_is_loaded(){
    local word_arr
    local pathname
    local IFS

    unset IFS
    if [ -n "${ZSH_VERSION+x}" ]; then
        setopt LOCAL_OPTIONS
        setopt sh_word_split
    fi

    while IFS="" read -r row || [ -n "$row" ]; do
        word_arr=($row)
        # see man 5 proc section /proc/[pid]/maps
        # word_arr[5] usually contains the pathname but may also be blank.
        [ "${#word_arr[@]}" -lt 6 ] && continue
        # portable array index access (zsh is one-based)
        pathname="${word_arr[@]:5:1}"
        _shournal_endswith "$pathname" libshournal-shellwatch.so && return 0
    done < "/proc/$$/maps"
    return 1
}



# The following non-portable, shell specific functions _must_ be set
# for each supported shell:
# _shournal_add_prompts
# _shournal_remove_prompts
# _shournal_postexec
case "$_SHOURNAL_SHELL_NAME" in
  'bash')

_shournal_add_prompts(){
    [ -z "${PROMPT_COMMAND+x}" ] && PROMPT_COMMAND=''
    PROMPT_COMMAND=$'_shournal_postexec\n'"$PROMPT_COMMAND"
    # no _shournal_preexec for bash, see below ...
    return 0
}

_shournal_remove_prompts(){
    [ -n "${PROMPT_COMMAND+x}" ] &&
        PROMPT_COMMAND=${PROMPT_COMMAND//_shournal_postexec$'\n'/}
    return 0
}
## _____ End of must-override functions and variables _____ ##


# _shournal_preexec(){
#   For bash preexec is not implemented here but in
#   libshournal-shellwatch.so, because the DEBUG trap is
#   somewhat unreliable and PS0 can only run commands in
#   a subshell which would require additional care (more IPC).
# }

_shournal_postexec(){
    local exitcode=$?
    local cmd_str
    [ -n "${1+x}" ] && exitcode="$1"
    _shournal_get_current_cmd_bash cmd_str
    _shournal_postexec_generic "$cmd_str" "$exitcode"
    return $exitcode
}


;; # END_OF bash _______________________________________________________

'zsh')

_shournal_add_prompts(){
    preexec_functions+=(_shournal_preexec)
    precmd_functions+=(_shournal_postexec)
    return 0
}

_shournal_remove_prompts(){
    unset _shournal_zsh_last_cmd

    preexec_functions[$preexec_functions[(i)_shournal_preexec]]=()
    precmd_functions[$precmd_functions[(i)_shournal_postexec]]=()
    return 0
}

## _____ End of must-override functions and variables _____ ##


_shournal_preexec(){
    # maybe_todo: use $2 or $3 for expanded aliases instead of $1
    _shournal_zsh_last_cmd="$1"
    _shournal_preexec_generic

    return 0
}

_shournal_postexec(){
    local exitcode=$?
    [ -n "${1+x}" ] && exitcode="$1"
    _shournal_postexec_generic "$_shournal_zsh_last_cmd" $exitcode
    return 0
}


;; # END_OF zsh ________________________________________________________
  *)
    echo "shournal shell integration: something is seriously wrong, " \
        "_SHOURNAL_SHELL_NAME is not correctly setup" >&2
    return 1
;;
esac


if [ -n "${_shournal_enable_just_called+x}" ] ; then
    # A parent process has called SHOURNAL_ENABLE and exec'd itself
    # again with the same arguments and our libshournal-shellwatch.so
    # preloaded. Let the tracking begin ...
    if ! _libshournal_is_loaded ; then
        _shournal_error "Although _'shournal_enable_just_called' is set, "\
                        "libshournal-shellwatch.so seems " \
                        "to be not loaded (bug?)."
        unset _shournal_enable_just_called
        return 1
    fi
    _shournal_enable
fi



