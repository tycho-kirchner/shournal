
# Bash integration for shournal

For a general introduction to shournal's shell-integration
please visit the
[README](../../shell-integration-scripts)
of the above directory.


## Setup
It is assumed that shournal is already installed, as described in the
general [README](/../../) and that shournal's bash-integration functions
were already made available by sourcing the SOURCE_ME.bash
**at the end of ~/.bashrc** (below path may differ depending on
  distribution and installation mode):   
`source /usr/share/shournal/SOURCE_ME.bash`  
**Note:** due to technical reasons the sourcing **must** be
performed in the bashrc and not within a terminal session.

Before successfully calling `SHOURNAL_ENABLE` you may
need to modify bash's history settings - in  most
cases it should be sufficient to change `HISTCONTROL` to something
other than `ignorespace` or `ignoreboth`, for example:

`HISTCONTROL="ignoredups"`

Check, if *bash* is correctly observed:  
```
$ SHOURNAL_ENABLE
$ echo foo > bar
$ shournal --query --wfile bar
# should output above command, etc.
```

## Notes
* **Never** modify the PROMPT_COMMAND after enabling shournal
  (unless you know what you are doing (; )
* Background for shournal's bash-integration to forbid e.g. `ignorespace`
  is that otherwise potentially wrong commands could be stored within
  shournal's own history (the last command *not* beginning with
  a space in this case).  
  Besides `HISTCONTROL` also
  other variables like `HISTIGNORE` or `HISTSIZE` must be set appropriately.
  shournal's bash integration checks those variables and gives hints, if
  there is need for action.
