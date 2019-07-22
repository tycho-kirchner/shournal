
# Shell integration for shournal

## TL;DR
After setup, put `SHOURNAL_ENABLE` into your shell's rc (e.g. .bashrc)
and log all configured file events, *without further ado*.  

* [Bash integration](https://github.com/tycho-kirchner/shournal/tree/master/shell-integration-scripts/bash)

## Requirements
You are using a shell which is linked dynamically against (g)libc
(default case) and supported by shournal (see below).


## Motivation

Having to type shournal before every single command one wants
to observe can be tiresome. Another typing-overhead is introduced by using
pipes or redirections. Consider the following example:

    shournal --exec echo hi > foo

As most shell users know the redirection applies to the whole command,
while shournal itself only observes "echo hi". The file modification event ('hi'
  written to 'foo')
is hence **not** tracked by shournal.
 To actually observe such a command
one must rather type

    shournal --exec sh -c 'echo hi > foo'

That's annoying, right?

Therefore before observing one or multiple commands,
`source` the respective integration-file within your shell's rc (e.g. .bashrc)
and type

    SHOURNAL_ENABLE

That's (almost) all. Forget about shournal until needed ( e.g. you want to know how
  a certain file was created).


Further options are currently  
`SHOURNAL_DISABLE` (disable the observation) and  
`SHOURNAL_SET_VERBOSITY` to change the default verbostiy ("dbg, info, warning, critical").
For dbg, shournal must have been compiled with debugging symbols. A verbosity higher than
warning is not recommended.

## Updates
If the shell-integration is running while shournal is updated, it becomes necessary,
to restart your shell. A more elegant way than logout-login might be to `exec` your $shell.


## FAQ
* **Obtaining the value of variables**.  
  If shell-variables are used within a command, shournal's reports might
  not seem to be very helpful. However, the shell-integration assigns
  each shell-session a unique identifier (uuid).
  In the likely case that the variable was
  assigned *during that session*, you might be able to obtain its value.
  This of course only works, if SHOURNAL_ENABLE was called, *before*
  a variable was assigned. Example:  
  `shournal --query --shell-session-id 'L/932KZTEemRB/dOGB9LOA==' | grep var_name`
* **What about new shell-sessions**?  
  By *new shell-sessions* it is meant to call e.g. `bash` within an already
  running bash-process. What happens next really depends on whether the
  shell is itself "observed" by shournal or not (e.g. whether
  `SHOURNAL_ENABLE` is within the .bashrc or not). On calling
  `SHOURNAL_ENABLE` the original mount-namespace is joined so file-events
  are no longer reported to the the shournal-observation-process
  belonging to the initial bash-process. If a non-observed shell
  is a called, shournal's later report will not be very helpful: all
  file-modifications caused by that process will yield the plain
  shell-command (and not individual commands possibly entered
  within the new shell session).


## Technology
The shell-integration works by using the well known `LD_PRELOAD`-trick
to inject code into a shell-process. In particular the library-calls
for `open` and `exec` are masked. When `open` is called, a
directory-file-descriptor of another mount-namespace is used to
perform the open-call, so the event can be tracked by fanotify.
When `exec` is called, instead of the original program,
shournal-run is executed in the first place which enters a
mount-namespace common to the whole *command sequence*.
The so executed program is **not** using shournal's LD_PRELOAD'ed
library any more, so the observation also works for
statically linked executables.  
The observed shell communicates with an external shournal-run-process
via a socket. To not interfere with the file-descriptors, the shell creates,
this socket is the highest allowed (free) descriptor (typically 1023).
The observation of external processes continues until all instances
of this descriptor, which is inherited to subprocesses, are closed.
Note that there are corner-cases, where this does not work (see limitations
below).

## Limitations
* File-operations (redirections) which spread over **multiple** command-sequences
  within the **interactive shell** are currently not tracked (reliably).  
  Example:
  ```
  $ exec 3> /tmp/foo  # open fd 3.
  $ echo "test" >&3
  $ exec 3>&- # close event not tracked
  ```
  This will probably be fixed in future versions.
* Filesystem-events of asynchronously launched processes, which close the inherited
  shournal-socket, might be lost, because an external shournal-run process
  waits until all instances of that socket are closed.
  Steps to reproduce: In an *observed* shell-session enter  
  `bash -c 'eval "exec $_SHOURNAL_SOCKET_NB>&-"; sleep 1; echo foo > bar' &`  
  Note that e.g. in *Python* processes launched via its
  *subprocess*-module do not inherit file descriptors by default.
  There seems to be no general solution to this problem, but in most cases
  there should be some mechanism to wait for the processes to finish,
  within the the interactive shell-session or a script.
* For further limitations please also read the general
  [README](https://github.com/tycho-kirchner/shournal).


## Supported shells

Currently only
[bash](https://github.com/tycho-kirchner/shournal/tree/master/shell-integration-scripts/bash) is supported.
If you want your favorite shell to be integrated as well,
please open an issue (don't forget to donate (: ) . Or, even better, send
a pull-request.
