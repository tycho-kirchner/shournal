
configure_file( _common.sh.in _common.sh @ONLY)
configure_file(integration_ko.sh.in integration_ko.sh @ONLY)


add_custom_target(target_SOURCE_SHELLSCRIPTS ALL
  COMMAND ${CMAKE_COMMAND} -E create_symlink _source_me_generic.sh SOURCE_ME.bash
  COMMAND ${CMAKE_COMMAND} -E create_symlink _source_me_generic.sh SOURCE_ME.zsh

  COMMAND ${CMAKE_COMMAND} -E create_symlink integration_ko.sh integration_ko.bash
  COMMAND ${CMAKE_COMMAND} -E create_symlink integration_ko.sh integration_ko.zsh
                  )

install(FILES
    _source_me_generic.sh
    "${CMAKE_CURRENT_BINARY_DIR}/SOURCE_ME.bash"
    "${CMAKE_CURRENT_BINARY_DIR}/SOURCE_ME.zsh"
    "${CMAKE_CURRENT_BINARY_DIR}/_common.sh"
    "${CMAKE_CURRENT_BINARY_DIR}/integration_ko.sh"
    "${CMAKE_CURRENT_BINARY_DIR}/integration_ko.bash"
    "${CMAKE_CURRENT_BINARY_DIR}/integration_ko.zsh"
    DESTINATION ${shournal_install_dir_script}
    )

