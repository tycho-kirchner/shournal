
# Bash integration for shournal

For a general introduction to shournal's shell-integration
please visit the
[README](../../shell-integration-scripts)
of the above directory.


## Setup
* Install shournal (obviously)
* You'll need the location of shournal's shell integration shared library
 (libshournal-shellwatch.so) which is typically at `/usr/local/lib/shournal`
* Download the [bash_integration.sh](./[bash_integration.sh]) (see above) and
  put it into your favorite scripts-directory
* Add the following **to the end** of your .bashrc:
  ```
  HISTCONTROL="ignoredups" # or anything other than "ignorespace" or "ignoreboth"
  export SHOURNAL_PATH_LIB_SHELL_INTEGRATION="/PATH_TO/libshournal-shellwatch.so"                                                                                              
  source /PATH_TO/bash_integration.sh                                                                                                                                                        
  # SHOURNAL_ENABLE could also be called later, during the shell session:
  SHOURNAL_ENABLE
  ```
  **Note**: For testing purposes you might be tempted to enter above commands
  manually within a shell-session. This does not work (straightforward).
  For a quick test the following works:
  ```
  $ export SHOURNAL_PATH_LIB_SHELL_INTEGRATION="/PATH_TO/libshournal-shellwatch.so"
  $ LD_PRELOAD="$SHOURNAL_PATH_LIB_SHELL_INTEGRATION" bash
  $ source /PATH_TO/bash_integration.sh
  $ SHOURNAL_ENABLE
  ```

* Check, if *bash* is correctly observed:  
  ```
  $ echo foo > bar
  $ shournal --query --wfile bar
  # should output above command, etc.
  ```

## Notes
* As seen in *Setup*, the history-control of bash is modified. Background
  is that shournal shall track *all* commands, which is currently
  only possible, if all commands are saved to the history.
  Besides `HISTCONTROL` also
  other variables like `HISTIGNORE` or `HISTSIZE` must be set appropriately.
  shournal's shell integration checks those variables and gives hints, if
  there is need for action.
* **Never** modify the PROMPT_COMMAND after enabling shournal
  (unless you know what you are doing (; )
