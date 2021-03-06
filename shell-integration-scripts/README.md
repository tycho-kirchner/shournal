
# Shell integration for shournal

## TL;DR
After setup, put `SHOURNAL_ENABLE` into your shell's rc (e.g. .bashrc)
and log all configured data and meta-data (file events, etc.),
*without further ado*.

* [Bash integration](./bash)

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


Further options are currently <br>
`SHOURNAL_DISABLE` (disable the observation) and <br>
`SHOURNAL_SET_VERBOSITY` to change the default verbostiy ("dbg, info, warning, critical").
For dbg, shournal must have been compiled with debugging symbols. A verbosity higher than
*warning* is not recommended.

## Requirements
The shell must be
[supported by shournal](#supported-shells). If the *fanotify* backend
is used, the shell must further be linked dynamically against (g)libc
(default case, can be tested e.g. with<br>
`file $(which bash) | grep "dynamically linked"` )


## Updates
If the shell-integration is running while shournal is updated, it becomes necessary,
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
  within the the interactive shell-session or a script.
* For further limitations please also read the general
  [README](/../../).


## Supported shells

Currently only
[bash](./bash) is supported.
If you want your favorite shell to be integrated as well,
please open an issue. Or, even better, send
a pull-request.
