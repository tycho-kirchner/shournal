
# Join a list of strings using seperator sep
# and store the output in result.
function(JOIN vals sep result)
  string (REGEX REPLACE "([^\\]|^);" "\\1${sep}" _tmp_str "${vals}")
  string (REGEX REPLACE "[\\](.)" "\\1" _tmp_str "${_tmp_str}")
  set (${result} "${_tmp_str}" PARENT_SCOPE)
endfunction()
