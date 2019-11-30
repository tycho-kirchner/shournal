
include(GenerateExportHeader)


include_directories(
    ../common
    ../common/oscpp
    ../common/qsimplecfg
    ../common/qsqlthrow
    ../common/database
    )


add_library(libshournal-shellwatch SHARED    
    libshournal-shellwatch
    attached_bash
    attached_shell
    event_open
    event_other
    event_process
    shell_globals
    shell_logger
    shell_request_handler
    )

# to list exported symbols of the compiled .so:
#  nm -D libshournal-shellwatch.so | grep ' T '
GENERATE_EXPORT_HEADER(libshournal-shellwatch)
hide_static_lib_symbols(libshournal-shellwatch)

# manually set the name of the .so -> we need it later
set(libshournal_fullname "libshournal-shellwatch.so")
set_target_properties(libshournal-shellwatch PROPERTIES OUTPUT_NAME "${libshournal_fullname}")
set_target_properties(libshournal-shellwatch PROPERTIES PREFIX "")
set_target_properties(libshournal-shellwatch PROPERTIES SUFFIX "")
target_link_libraries(libshournal-shellwatch
    ${CMAKE_DL_LIBS} # dlsym
    lib_shournal_common_shared
    uuid
)


set(binary_dir_path_SOURCE_ME_bash "${CMAKE_CURRENT_BINARY_DIR}/SOURCE_ME.bash")
set(full_path_libshournal ${CMAKE_INSTALL_FULL_LIBDIR}/${PROJECT_NAME}/${libshournal_fullname})

# Generate a source'able helper file which prepares and enables the shell-integration
# by using the known path to libshournal-shellwatch and to the $shell_integration.sh
file(WRITE "${binary_dir_path_SOURCE_ME_bash}"
    "\
export SHOURNAL_PATH_LIB_SHELL_INTEGRATION='${full_path_libshournal}'\n\
source '${CMAKE_INSTALL_FULL_DATAROOTDIR}/${PROJECT_NAME}/bash_integration.sh'\n\
")

########################## Installation ##########################

install(TARGETS libshournal-shellwatch
     DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}/${PROJECT_NAME}
)

install(FILES
    ../../shell-integration-scripts/bash/bash_integration.sh
    "${binary_dir_path_SOURCE_ME_bash}"

    DESTINATION ${CMAKE_INSTALL_FULL_DATAROOTDIR}/${PROJECT_NAME}
    )




