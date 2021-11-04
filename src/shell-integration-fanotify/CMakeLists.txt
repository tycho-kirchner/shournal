
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
# Do not change the name libshournal-shellwatch.so, it is hardcoded into the shell-integration-scripts.
set_target_properties(libshournal-shellwatch PROPERTIES OUTPUT_NAME "${libshournal_fullname}")
set_target_properties(libshournal-shellwatch PROPERTIES PREFIX "")
set_target_properties(libshournal-shellwatch PROPERTIES SUFFIX "")
target_link_libraries(libshournal-shellwatch
    ${CMAKE_DL_LIBS} # dlsym
    # Using the static lib_shournal in our shared library exposes
    # all symbols (not a good idea in a LD_PRELOAD-lib).
    # Hiding them requires either a --version-script
    # or all libraries being compiled with -fvisibility=hidden. The former
    # approach seems more elegant.
    # See also https://stackoverflow.com/a/22110050/7015849
    "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/libshellwatch.version"
    lib_shournal_common
    uuid
)


########################## Installation ##########################

install(TARGETS libshournal-shellwatch
     DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}/${PROJECT_NAME}
)





