

configure_file( _common.sh.in _common.sh )

add_custom_target(target_SOURCE_BASH ALL
                  COMMAND ${CMAKE_COMMAND} -E create_symlink
                  _source_me_generic.sh SOURCE_ME.bash
                  )

install(FILES
    _source_me_generic.sh
    "${CMAKE_CURRENT_BINARY_DIR}/SOURCE_ME.bash"
    "${CMAKE_CURRENT_BINARY_DIR}/_common.sh"
    DESTINATION ${shournal_install_dir_script}
    )

