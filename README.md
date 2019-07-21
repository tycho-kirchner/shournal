

# shournal

## A (file-) journal for your shell

Call a command and recursively monitor which files
it modifies in an efficient manner.

shournal stores file properties, to find out later,
how a given file was modified/created.

It can collect scripts or other *read* files during execution to
regain valuable information beyond the plain command.


shournal can be integrated pretty tight into the shell
(currently only bash). Using it there is no need to type
`shournal --exec $cmd` all the time - enable it once and forget about it,
until needed.
For more details please refer to the
[shell-integration-scripts](shell-integration-scripts/README.md).

In contrast to ptrace-based solutions (e.g. strace),
shournal does *not* slow down the observed process(es) and
consumes only a little amount of your precious cpu-time.
See also [Technology](#technology).

Please note that shournal is no reliable auditing-solution -
it is for people who want to log/reproduce *their own work*.
Advanced users can easily circumvent it.

shournal runs only on GNU/Linux.


## Examples

* Create a file and ask shournal, how it was created:
  ```
  $ shournal --exec sh -c 'echo hi > foo'
  $ shournal --query --wfile foo
  Command id 1 returned 0 - 14.05.19 10:19 : sh -c echo hi > foo
    Written file(s):
       /home/user/foo (3 bytes) Hash: 15349503233279147316

  ```
* shournal can be configured, to store specific read files, like shell-scripts,
  within it's database. An unmodified file occupies space only once.
  ```
  $ shournal -e ./test.sh
  $ shournal -q --history 1
  Command id 2 returned 0 - 14.05.19 14:01 : ./test.sh
    Read file(s):
       /home/user/test.sh (213 bytes) id 1
  ```



## FAQ
* **Does shournal track file rename/move operations?**
  No, but most often it should not be a problem. Using the
  `--wfile` commandline-query-option, shournal finds the stored command
  by content (size, hash) and mtime, not by its name.
  For the name, `--wname` can be used.
  More concrete:
  ```
  shournal --exec sh -c 'echo foo > bar; mv bar bar_old'
  ```
  Querying for bar_old by content (`--wfile`-option) yields exactly
  the given command, however, `--wname bar_old` does **not** work
  (`--wname bar` of course works). To use the bar_old *file name*
  (and not content) as basis for a successful query, in this case
  `--command-text -like '%bar_old%'` can be used.
* **What happens to an appended file?** How to get a "modification history"?
  Please read above rename/move-text first.
  Appending to a file is currently handled as if a new one was created -
  only the last command, which modified a given file can be found with
  good certainty (by file **content**).
  However, querying by path/file**name** works.
  If the file was appended *and* renamed, things get more complicated.
* **To track written files, they are hashed**. Doesn't this take very long
  for huge files?
  No, because per default only certain parts of the file are hashed.
* **What does the following message mean and how to get rid of it?**:
  `fanotify_mark: failed to add path /foobar ... Permission denied `.

  This message might be printed on executing a command with shournal.
  Most probably the administrator mounted a filesystem object for which you don't have
  permissions, thus you cannot *monitor* file events.
  In this case you cannot perform file operations at this path
  anyway, so it should be safe to silence this warning by adding the
  path within the config-file in section `[mounts]`. If you want to ignore all
  fanotify_mark permission errors, you can set the flag in section
  `[mounts]`:
  ```
  [mounts]
  ignore_no_permission = true
  ```


## Configuration
shournal stores a self-documenting config-file typically at
~/.config/shournal
which is created on first run. It can be edited either directly with
a plain text editor or via `--edit-cfg`.
For completeness, the most important points are listed here as well.
* Usually only file-write-events for specific paths are of interest.
  Put each path into a separate line, all paths being enclosed
  by triple quotes:
  ```
  include_paths = '''
    /home/me
    /media
  '''
  ```
  Each exclude_path should be a sub-path of an include path.
* While currently all write-events occurring at the given paths are stored,
  file read-events can be controlled in more detail. As already mentioned,
  while for write-events only a few properties (filename, size, hash,...)
  are saved, a read file matching the rule-set is stored as a whole
  within shournal's database.
  Files are only stored, if the configured max. file-size, file extension
  (.sh) and mimetype (application/x-shellscript) matches.
  To find a mimetype for a given file
  you should use `shournal --print-mime test.sh`.
  The correspondence of mimetype and file extension
  is explained in more detail within the config-file.
  Further, at your wish, read files are only stored if *you* have write permission for them
  (not only read) - often system-provided scripts (owned by root) are not of particular
  interest.

  shournal will not store more read files per command, than max_count_of_files.
  Matching files coming first have precedence.


## Disk-space - get rid of obsolete file-events
Depending on the file-activity of the observed commands, shournal's
database sooner or later grows. When you feel that enough time
has passed and want to get rid of old events, this can be done by e.g.
`shournal --delete --older-than 1y`
which deletes all commands (and file-events) older than one year.
More options are available, see also
`shournal --delete --help`


## Remote file-systems
* File modifications on a remote server, mounted e.g. via NFS or sshfs, can be observed,
  since we only monitor processes running on the local machine (fanotify can otherwise not monitor
  events which *another* Kernel causes on the same NFS-storage).
* For sshfs it is necessary, to add ```-o allow_root``` to the sshfs-options,
  otherwise permission errors during ```fanotify_mark``` are raised.
  See also: https://serverfault.com/a/188896


## Installation
(side note: below qt libraries are not for graphical purposes (: )

* Install gcc >= 5.0. Other compilers might work but are untested.
* Install cmake >=2.8.12 and make
* for safe generation of uuids it is recommend to install uuidd (uuid-runtime)
* install qt-dev, uuid-dev, qt-sqlite-driver, Qt version >= 5.6.
  *With a little effort, shournal could be changed to
  support Qt version >= 5.3. Please open an issue, if that would
  be helpful to you.*

  *Packages lists*:

  Debian/Ubuntu:
  `apt-get install g++ cmake make qtbase5-dev libqt5sql5-sqlite
  uuid-dev libcap-dev uuid-runtime`

  Opensuse:
  `zypper install gcc-c++ cmake make libqt5-qtbase-devel libQt5Sql5-sqlite
  libuuid-devel libcap-devel uuidd`

  CentOS (note: CentOS 7 as of July 2019 only ships with gcc 4.8
  -> compile gcc >= 5.0 yourself):
  `yum install gcc-c++ cmake make qt5-qtbase-devel libuuid-devel
  libcap-devel uuidd`

* In the source-tree-directory, enter the following commands to compile and install:
  ```
  mkdir -p build
  cd build
  cmake  .. # defaults to -DCMAKE_BUILD_TYPE=Release
  make
  # as root:
  make install

  ```
* Depending on your distribution, additional steps might be necessary to
  enable the (recommended) uuidd-daemon. If systemd is in use, one may need to:
  ```
  systemctl enable uuidd
  systemctl start uuidd
  ```

* if you plan on using the shell-integration, you'll need the location of
  `libshournal-shellwatch.so` which is typically within /usr/local/lib/shournal
* Add a group to your system, which is primarily needed for the shell-integration:

  ```groupadd shournalmsenter```

  However, *do not add any users to that group*. It is part of a permission check, where root adopts that gid (within shournal).
  If you don't like the default group name, you can specify your own: at build time pass the following to cmake:

  ```-DMSENTER_GROUPNAME=$your_group_name```

* Two executables were built: shournal and shournal-run. The latter is a setuid program, it has to be owned by root while having the setuid-bit set.
  Typing `make install` as root does this automatically, to do so manually, enter as root:
  ```
  chown root shournal-run
  chmod u+s  shournal-run
  ```


## Technology
The linux-kernel's *fanotify* (not to be confused with the more popular
*inotify*) is able to observe file-events for whole
mountpoints. By unsharing the mount-namespace it is possible to monitor only
the file-events of a given process and its children (but see [Caveats](##Caveats)).
See also `man mount_namespaces` and fanotify.


## Security
shournal-run is a so called "setuid"-program: whenever a regular user calls it, it runs
with root-permissions in the first place. As soon as possible, it runs effectively with user
permissions though.
It must be setuid for two reaons:
* fanotify, the filesystem-changes api requires root for initializing, because it is in
  principle able, to **forbid** a process to access a file. shournal does not make use
  of this feature so this is not a real security concern.
* unsharing the *mount namespace* requires root, because setuid-programs *could* still refer
  to seemingly obsolete mounts. This means that under awkward circumstances an unmount-event,
  which has security-relevant consequences (e.g. mounting a new file to /etc/shadow) might not
  propagate to processes which run in other mount namespaces.
  To make sure mount-propagation applies, **all mounts, which carry setuid-binaries
  or files they refer to, should be mounted *shared* **, or no (security-relevant)
  mount/unmount events should occur, after the first shournal-process started.
  Shared mounts are the default in all recent distributions I know of.
  See also
  man 7 mount_namespaces and
  "shared subtrees"
  https://www.kernel.org/doc/Documentation/filesystems/sharedsubtree.txt



## Limitations
The file observation only works, if the process does not unshare the mount-namespace itself,
e.g. monitoring a program started via *flatpak* fails.

Processes can communicate via IPC (inter-process-communication).
If the observed process A instructs the **not** observed process B
via IPC to modify a file, the filesystem-event is not registered by shournal.

Currently files may be reported by shournal as written, even though
nothing was actually written to them, in case they were opened with
*write*-permissions. By using the file content (hash) you should
be able to cover those cases.

The provided timestamp is determined shortly after a modified file was
closed. Note that it is possible that some other process has
written to it in between. This however is only a
problem, if that other process was itself **not** observed.

To cache write-events efficiently during execution, they are put into a device-inode-hashtable. Note that the kernel might reuse them.
If you copy a file to a non-observed directory, delete it at the original location and the inode is reused during execution
of the observed process, the filesystem-event is lost. Note that copying it to a observed location is fine though,
because copying a file is itself a file-modification-event.

For further limitations please visit the fanotify manpage.

## Known Issues
* on NFS-storages: file events are lost, if the user does not have
  read-permissions while a file is closed.
  Steps to reproduce:
  - open a file readable for you on a NFS storage
  - chmod it 000
  - close it --> the event is lost




# License
The whole project is licensed under the GPL, v3 or later
(see LICENSE file for details) **except** the libraries within
`extern/`: please refer to the licenses within their
respective directories.



Copyleft (C) 2019, Tycho Kirchner
