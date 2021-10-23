
configure_file( _source_me_generic.sh _source_me_generic.sh @ONLY)

# Merge script files into _integration_ko.sh
# Write to temporary so the real target only gets updated
# if its content has changed
set(integration_ko_tmp "${CMAKE_CURRENT_BINARY_DIR}/integration_ko.sh_tmp")
file(WRITE ${integration_ko_tmp} "")
foreach(f
        integration_main.sh.in util.sh integration_ko.sh)
  append_to_file(${integration_ko_tmp} ${f})
endforeach()
# Copy the temporary file to the final location
configure_file(${integration_ko_tmp} _integration_ko.sh @ONLY)

# Merge script files into _integration_fan.sh
set(integration_fan_tmp "${CMAKE_CURRENT_BINARY_DIR}/integration_fan.sh_tmp")
file(WRITE ${integration_fan_tmp} "")
foreach(f
        integration_main.sh.in util.sh integration_fan.sh)
  append_to_file(${integration_fan_tmp} ${f})
endforeach()
configure_file(${integration_fan_tmp} _integration_fan.sh @ONLY)


add_custom_target(target_SOURCE_SHELLSCRIPTS ALL
  COMMAND ${CMAKE_COMMAND} -E create_symlink _source_me_generic.sh SOURCE_ME.bash
  COMMAND ${CMAKE_COMMAND} -E create_symlink _source_me_generic.sh SOURCE_ME.zsh

  # keep those for backwards compatibility (SOURCE_ME.$shell should be used
  # in general as it automatically selects the correct backend)
  COMMAND ${CMAKE_COMMAND} -E create_symlink _integration_ko.sh integration_ko.bash
  COMMAND ${CMAKE_COMMAND} -E create_symlink _integration_ko.sh integration_ko.zsh

  COMMAND ${CMAKE_COMMAND} -E create_symlink _integration_fan.sh integration_fan.bash
  COMMAND ${CMAKE_COMMAND} -E create_symlink _integration_fan.sh integration_fan.zsh
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/_source_me_generic.sh"
    "${CMAKE_CURRENT_BINARY_DIR}/SOURCE_ME.bash"
    "${CMAKE_CURRENT_BINARY_DIR}/SOURCE_ME.zsh"

    "${CMAKE_CURRENT_BINARY_DIR}/_integration_ko.sh"
    "${CMAKE_CURRENT_BINARY_DIR}/integration_ko.bash"
    "${CMAKE_CURRENT_BINARY_DIR}/integration_ko.zsh"

    "${CMAKE_CURRENT_BINARY_DIR}/_integration_fan.sh"
    "${CMAKE_CURRENT_BINARY_DIR}/integration_fan.bash"
    "${CMAKE_CURRENT_BINARY_DIR}/integration_fan.zsh"

    DESTINATION ${shournal_install_dir_script}
)

