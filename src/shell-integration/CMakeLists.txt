
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

# libshournal already begins with <lib> so remove prefix.
set_target_properties(libshournal-shellwatch PROPERTIES PREFIX "")
install(TARGETS libshournal-shellwatch
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/${PROJECT_NAME}
    )

target_link_libraries(libshournal-shellwatch
    ${CMAKE_DL_LIBS} # dlsym
    lib_shournal_common_shared
    uuid
)


