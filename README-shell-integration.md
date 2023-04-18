
# Shell integration for shournal



## Basic setup (interactive)
After installation, to start observing your *interactive* shell-sessions
append the following to your shell's rc: <br>
**~/.bashrc** <br>
~~~
HISTCONTROL=ignoredups:erasedups # NOT ALLOWED: ignorespace,ignoreboth
source /usr/share/shournal/SOURCE_ME.bash
SHOURNAL_ENABLE
~~~

**~/.zshrc** <br>
~~~
source /usr/share/shournal/SOURCE_ME.zsh
SHOURNAL_ENABLE
~~~

Launch a new shell afterwards and check whether it's working:
~~~
$ echo foo > bar
$ shournal --query --wfile bar
cmd-id 66075 $?=0 2021-11-02 14:23 $ echo foo > bar
Working directory: /home/tycho
session-uuid 3hIZtDwhEey5WPDVv9W/Cw==
  1 written file:
     /home/tycho/bar (4 bytes) Hash: 8087352826690557229
$ # or just look into the history:
$ shournal --query --history 3
# ...
~~~


The shell-integration injects code into
`PROMPT_COMMAND`, `PS0` and `PS1` (bash) or the `preexec/precmd_functions`
(zsh), so please do not overwrite those after having enabled shournal.
Further basic history functionality must be available, e.g. in bash
HISTCONTROL must not ignore commands with leading spaces (see above).
shournal's shell integration checks the typical variables and gives
hints, if there is need for action.

Other commands include <br>
`SHOURNAL_DISABLE` to disable the observation <br>
`SHOURNAL_PRINT_VERSIONS` to print the version of each component <br>
`SHOURNAL_SET_VERBOSITY` to change the default verbosity ("dbg, info,
warning, critical"). For dbg, shournal must have been compiled with
debugging symbols. A verbosity higher than *warning* is not recommended.



## Advanced setup (non-interactive)
To also observe non-interactive commands executed via ssh
~~~
ssh localhost echo foo
~~~
or the *Sun Grid Engine* (SGE) the following setup is recommended:
<br> <br>
**bash** <br>
Put the following near the **beginning** of your bashrc:
~~~
if [[ -n ${SGE_O_WORKDIR+x} || (
     -n ${BASH_EXECUTION_STRING+x} &&
    ( -n ${SSH_CLIENT+x} || -n ${SSH_TTY+x} )
    ) ]]; then
    source /usr/share/shournal/SOURCE_ME.bash
    SHOURNAL_ENABLE
fi
~~~
In particular that code has to run before the sourcing of ~/.bashrc
stops due to a negative interactive-check. For example, some distributions
place the following near the top of the bashrc:
~~~
case $- in
    *i*) ;;
      *) return;;
esac
~~~

**zsh** <br>
Put the following into ~/.zprofile
~~~
if [[ -n ${SGE_O_WORKDIR+x} || (
    -n ${ZSH_EXECUTION_STRING+x} &&
    ( -n ${SSH_CLIENT+x} || -n ${SSH_TTY+x} )
    ) ]]; then
    source /usr/share/shournal/SOURCE_ME.zsh
    SHOURNAL_ENABLE
fi
~~~
Note that depending on your server environment, this requires zsh to be
executed as login shell, e.g. <br>
`ssh HOST zsh -l -c command`. Alternatively
you may use ~/.zshenv but beware that this file is always sourced, also
during `zsh -c ':'` invocations on the interactive command-line, so at least
an additional check for <br>
`[ $SHLVL -eq 1 ]` is recommended.

For cluster software systems other than SGE, you may
`export SHOURNAL_IS_CLUSTERJOB=true`, before `SHOURNAL_ENABLE`, if and
only if the shell is about to execute a cluster job. Note
that in this case, shournal performs a re-execution of the current
command and only returns control flow after flushing the database, because
cluster software systems tend to kill background processes, once the
main job script finished. To totally disable cluster job detection,
set `SHOURNAL_NO_CLUSTER_JOB_DETECT=true` before `SHOURNAL_ENABLE`.


## Prerequisites of the fanotify backend
If the *fanotify* backend is used, please ensure the following:
* The shell must be linked dynamically against (g)libc (default case,
  can be tested e.g. with <br>
  `file $(which bash) | grep "dynamically linked"` ).
* Sourcing of SOURCE_ME.$shell must be within the shell's rc-file.
* `SHOURNAL_ENABLE` should be within the shell's rc, because
  on the very first enable the shell is re-executed, purging all non-exported
  variables.
* For non-interactive commands `SHOURNAL_ENABLE` must be called
  before the actual execution begins.

Note that the kernel module backend does not have those prerequisites
and should be preferred in most cases.



## Updates
If the shell-integration is running while shournal is updated, it is recommended,
to restart your shell. A more elegant way than logout-login might be to `exec` your $shell.



## FAQ
* **How to obtain the value of variables?**. <br>
  If shell-variables are used within a command, shournal's reports might
  not seem to be very helpful. However, the shell-integration assigns
  each shell-session a unique identifier (uuid).
  In the likely case that the variable was
  assigned *during that session*, you might be able to obtain its value.
  This of course only works, if SHOURNAL_ENABLE was called, *before*
  a variable was assigned. Example: <br>
  `shournal --query --shell-session-id 'L/932KZTEemRB/dOGB9LOA==' | grep var_name`
* **What about new, nested shell-sessions**? <br>
  By *new shell-sessions* it is meant to call e.g. `bash` within an already
  running bash-process. What happens next really depends on whether the
  shell is itself **observed** by shournal or not (e.g. whether
  `SHOURNAL_ENABLE` is within the .bashrc or not). On calling
  `SHOURNAL_ENABLE` file-events are then considered to belong to the
  new shell-session and are no longer reported to the original
  observation-process of the caller. If a **non-observed** shell
  is a called, shournal's later report will not be very helpful: all
  file-modifications caused by that process will yield the plain
  shell-command (and not individual commands possibly entered
  within the new shell session).



## Limitations
* File-operations (redirections) which spread over **multiple** command-sequences
  within the **interactive shell** might lead to surprising (*kernel module backend*)
  or incorrect (*fanotify backend*) results. <br>
  Example:
  ~~~
  $ exec 3> /tmp/foo  # open fd 3
  $ echo "test" >&3
  $ exec 3>&-  # close fd 3.
  ~~~
  In case of the *kernel module backend* as usual the close event is
  tracked, however `shournal -q -wf /tmp/foo` prints only the command
  `exec 3>&-`. By using the shell-session uuid it should be possible
  to reconstruct those cases. <br>
  In case of the *fanotify backend* the close-event is lost.
* **Additional limitations of the fanotify-backend**: <br>
  Filesystem-events of asynchronously launched processes, which close the inherited
  shournal-socket, might be lost, because an external shournal-run process
  waits until all instances of that socket are closed.
  Steps to reproduce: In an *observed* shell-session enter <br>
  `bash -c 'eval "exec $_SHOURNAL_SOCKET_NB>&-"; sleep 1; echo foo > bar' &` <br>
  Note that e.g. in *Python* processes launched via its
  *subprocess*-module do not inherit file descriptors by default.
  There seems to be no general solution to this problem, but in most cases
  there should be some mechanism to wait for the processes to finish,
  within the interactive shell-session or a script.
* For further limitations please also read the general
  [README](/../../).



## Motivation

For a general introduction about the data and meta-data *shournal* stores
please visit the general [README](/../../).

Having to type *shournal* before every single command one wants
to observe can be tiresome. Another typing-overhead would be
introduced by using pipes or redirections.
Consider the following **broken** example:

    shournal --exec echo hi > foo    # Don't do this.

As many shell users know the redirection applies to the whole command,
while shournal itself only observes "echo hi". The file modification event ('hi'
  written to 'foo')
is hence **not** tracked by shournal.
To actually observe such a command
one must rather type

    shournal --exec sh -c 'echo hi > foo'

That's annoying, right?

Therefore before observing one or multiple commands,
`source` the respective integration-file within your shell's rc
(e.g. .bashrc) and type

    SHOURNAL_ENABLE

That's (almost) all. Forget about *shournal* until needed
( e.g. you want to know how a certain file was created).


