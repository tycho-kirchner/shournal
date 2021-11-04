
# shell-integration for shournal - fanotify backend.


_shournal_run_backend='shournal-run-fanotify'


_shournal_enable(){
    local ret=0
    if [ -n "${_shournal_is_running+x}" ] ; then
        _shournal_warn_on '! _libshournal_is_loaded'
        _shournal_debug "_shournal_enable: current session is already observed"
        return 0
    fi

    if [ -n "${_shournal_shell_exec_string+x}" ]; then
        _shournal_handle_exec_string || ret=$?
        return $ret
    fi

    # This shell was _not_ invoked with the sh -c '...' option,
    # so clear our flag unconditionally:
    unset _shournal_parent_launched_us_noninteractive

    if ! _libshournal_is_loaded ; then
        if [ -n "${_shournal_enable_just_called+x}" ]; then
            _shournal_warn "Something went wrong during preloading of " \
                           "libshournal-shellwatch.so, the shell integration " \
                           "is _not_ enabled."
            unset _shournal_enable_just_called
            return 1
        fi
        _shournal_interactive_exec_allowed || return $?
        _shournal_exec_ldpreloaded_shell
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

    _shournal_debug "_shournal_disable: about to disable" \
                    "with exitcode $exitcode"

    if [ -n "${_shournal_shell_exec_string+x}" ]; then
        if [ -n "${_shournal_parent_launched_us_noninteractive+x}" ]; then
             _shournal_warn "SHOURNAL_DISABLE called, but the fanotify-based" \
                       "shell integration cannot be disabled for the" \
                       "'$_SHOURNAL_SHELL_NAME -c' invocation. Please use the" \
                       "kernel backend if this is a strong requirement."
            return 1
        else
            _shournal_debug "_shournal_parent_launched_us_noninteractive is not" \
                            "set, likely disable was called without a" \
                            "prior enable."
            return 0
        fi
    fi

    if [ -z "${_shournal_is_running+x}" ]; then
        _shournal_debug "_shournal_disable: not running"
        return 0
    fi

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


# Check if it is _ok_ to call exec.
# The shell is running interactive (no *_EXECUTION_STRING)
# and _libshournal is not (pre-)loaded yet. To do so we
# (currently) have to call exec. While doing so from
# .shrc is fine, loading from interactive shell is ok (non-exported
# variables are lost) we want to exclude the case where
# commands are called in a row after SHOURNAL_ENABLE e.g.
# $ SHOURNAL_ENABLE; important-command
# because those are lost.
_shournal_interactive_exec_allowed(){
    local current_cmd
    current_cmd="$(_shournal_print_current_cmd)"
    current_cmd="$(_shournal_trim "$current_cmd")"
    if [ -z "$current_cmd" ]; then
        _shournal_debug "_shournal_enable exec granted, history " \
                        "is empty (likely called from .shrc)"
        return 0
    fi

    if ! _shournal_endswith "$current_cmd" 'SHOURNAL_ENABLE'; then
        _shournal_warn "Command after SHOURNAL_ENABLE detected but" \
                       "we have to call exec first to enable the" \
                       "fanotify based shell integration." \
                       "Please ENABLE as separate command" \
                       "or switch backend. Command was '$current_cmd'"
        return 1
    fi
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
        echo "To see the version of shournal's shell-integration " \
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
    local IFS; unset IFS

    if ! [ -f "${SHOURNAL_PATH_LIB_SHELL_INTEGRATION-}" ]; then
        _shournal_error "Please provide a valid path for libshournal-shellwatch.so, e.g. " \
                        "export SHOURNAL_PATH_LIB_SHELL_INTEGRATION=" \
                        "'/usr/local/lib/shournal/libshournal-shellwatch.so'"
        return 1
    fi
    cmd_path="$(readlink /proc/$$/exe)"
    while IFS= read -r -d '' line; do
        args_array+=("$line")
    done < /proc/$$/cmdline
    export _shournal_enable_just_called=true
    export LD_PRELOAD=${LD_PRELOAD-}":$SHOURNAL_PATH_LIB_SHELL_INTEGRATION"
    _shournal_debug "_shournal_exec_ldpreloaded_shell: calling preloaded " \
                    "$cmd_path ${args_array[@]:1}"
    # Relaunch the shell with shournals .so preloaded using the original arguments.
    exec shournal --verbosity "$_SHOURNAL_VERBOSITY"  \
        --backend-filename "$_shournal_run_backend" \
        --msenter-orig-mountspace \
        --exec-filename "$cmd_path" --exec -- "${args_array[@]}"
    # only get here on error
    return 1

}

# Handle the sh -c '...' case. No PS0 (bash) or preexec_functions (zsh)
_shournal_handle_exec_string(){
    local cmd_trimmed
    local cmd_path
    declare -a args_array
    local IFS; unset IFS

    # In *this* backend we simply re-exec ourselves
    # with shournal and monitor the whole command sequence (SHOURNAL_DISABLE not
    # possible). Note that technically it would be possible to
    # move the original shell-binary somewhere else, execute a shournal-fake-shell
    # instead and invoke the original shell preloaded, so
    # we could allow for flexible enabling/disabling. This however would
    # be somewhat involved and possibly require one-time setup by the user.
    # On the other hand the ko-based shell integration does offer this
    # flexibility, so here we just ensure correctness:
    # We may only re-exec, if no command was executed before we were enabled,
    # otherwise it would be executed twice! Therefore we allow only two
    # cases: SHOURNAL_ENABLE as first command of the invocation e.g.
    # *sh -c 'SHOURNAL_ENABLE; ...' or when called from .shrc (which
    # must have been sourced before the command invocation starts.


    if [ -n "${_shournal_parent_launched_us_noninteractive+x}" ] ; then
        _shournal_debug "_shournal_handle_exec_string: we are (likely) already observed," \
                        "_shournal_parent_launched_us_noninteractive is set." \
                        "NOT performing re-exec"
        return 0
    fi

    cmd_trimmed="$(_shournal_trim "$_shournal_shell_exec_string")"
    if ! _shournal_startswith "$cmd_trimmed" 'SHOURNAL_ENABLE' &&
       ! _shournal_verbose_reexec_allowed; then
        _shournal_warn "we were enabled _during_ the $_SHOURNAL_SHELL_NAME -c" \
                       "invocation, however, the fanotify backend only supports" \
                       "enabling _before_ or at the beginning of the invocation." \
                       "Either switch to the kernel backend or" \
                       "put SHOURNAL_ENABLE into your shell's rc or at the" \
                       "beginning of the invocation. It may also be" \
                       "possible to directly call the command with 'shournal -e ...'"
        return 1
    fi

    cmd_path="$(readlink /proc/$$/exe)"
    while IFS= read -r -d '' line; do
        args_array+=("$line")
    done < /proc/$$/cmdline

    _shournal_debug "_shournal_handle_exec_string: exec non-interactive" \
                    "$cmd_path ${args_array[@]}"

    # arg --fork: do not wait writing to the database. Otherwise
    # it blocks of course.
    _shournal_parent_launched_us_noninteractive=true exec \
        shournal --backend-filename "$_shournal_run_backend" \
        --verbosity "$_SHOURNAL_VERBOSITY" \
        --exec-filename "$cmd_path" --exec --fork -- "${args_array[@]}"
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
    export _SHOURNAL_SHELL_PID=$$

    # Note: our .so detects this special (non-)filename and
    # writes its response to an unnamed tmp file.
    ret=0
    read -d '' trigger_response < '_///shournal_trigger_response///_' || ret=$?
    unset _LIBSHOURNAL_TRIGGER
    unset _SHOURNAL_SHELL_PID

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
    local IFS; unset IFS

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
# _shournal_verbose_reexec_allowed
case "$_SHOURNAL_SHELL_NAME" in
  'bash')
export _LIBSHOURNAL_SEQ_COUNTER=1
_shournal_ps0='${_SHOURNAL_SHELL_NAME:((_LIBSHOURNAL_SEQ_COUNTER++)):0}$(:)'

_shournal_add_prompts(){
    [ -z "${PS0+x}" ] && PS0=''
    [ -z "${PROMPT_COMMAND+x}" ] && PROMPT_COMMAND=''

    # Allright, what happens here? We use _SHOURNAL_SHELL_NAME as a dummy
    # variable in order to increment _LIBSHOURNAL_SEQ_COUNTER without printing
    # anything. Then we fork to notify libshournal-shellwatch.so that
    # we're about to execute a command.
    PS0="$PS0""$_shournal_ps0"
    PROMPT_COMMAND=$'_shournal_postexec\n'"$PROMPT_COMMAND"
    # no _shournal_preexec for bash, see below ...
    return 0
}

_shournal_remove_prompts(){
    [ -n "${PS0+x}" ] && PS0=${PS0//"$_shournal_ps0"/}
    [ -n "${PROMPT_COMMAND+x}" ] &&
        PROMPT_COMMAND=${PROMPT_COMMAND//_shournal_postexec$'\n'/}
    return 0
}

# Return true in case bash -c '...' may re-exec,
# otherwise report the error and return false.
# Re-exec is e.g. not allowed, if the a command
# within the -c '..' arg was already executed
# (it would be executed twice otherwise).
_shournal_verbose_reexec_allowed(){
    # FIXME: this is not robust, bash -c 'echo foo; source ~/.bashrc'
    # should _not_ be allowed.
    [ "${BASH_SOURCE[-1]}" = "$HOME/.bashrc" ] && return 0
    _shournal_warn "not called from ~/.bashrc but ${BASH_SOURCE[-1]}"
    return 1
}

## _____ End of must-override functions and variables _____ ##


# _shournal_preexec(){
#   For bash preexec is not implemented here but in
#   in an interplay of above PS0 and libshournal-shellwatch.so.
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

_shournal_verbose_reexec_allowed(){
    zmodload zsh/parameter
    local toplevel_context="${zsh_eval_context[1]}"
    case "$toplevel_context" in
    file) return 0;;
    cmdarg)
        _shournal_warn "eval-toplevel-context $toplevel_context not allowed"
        return 1;;
    *)
        _shournal_warn "unhandled eval-toplevel-context $toplevel_context." \
                       "Please report if you" \
                       "think that SHOURNAL_ENABLE should be possible here."
        return 1;;
    esac
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
        _shournal_error "Although _'shournal_enable_just_called' is set, " \
                        "libshournal-shellwatch.so seems " \
                        "to be not loaded (bug?)."
        unset _shournal_enable_just_called
        return 1
    fi
    _shournal_enable
fi



