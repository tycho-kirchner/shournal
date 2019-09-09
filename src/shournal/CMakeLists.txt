

include_directories(
    ../common
    ../common/qoptargparse
    ../common/database
    ../common/qsimplecfg
    ../common/oscpp
    ../common/database
    ../common/qsqlthrow
    ../../extern/xxHash
    )


add_executable(${PROJECT_NAME}
    shournal.cpp # main
    argcontrol_dbdelete
    argcontrol_dbquery
    command_printer
    command_printer_human
    command_printer_json
    )

target_link_libraries(${PROJECT_NAME}
    lib_shournal_common
    )

install(
    TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
    PERMISSIONS
                OWNER_READ OWNER_WRITE OWNER_EXECUTE
                GROUP_READ GROUP_EXECUTE
                WORLD_READ WORLD_EXECUTE
)
