
include_directories(
    ../common
    ../common/qoptargparse
    ../common/database
    ../common/qsimplecfg
    ../common/oscpp
    ../common/database
    ../common/qsqlthrow
    )


add_executable(shournal-run
    shournal-run.cpp # main
    fanotify_controller
    filewatcher
    mount_controller    
    msenter
    orig_mountspace_process
    )

target_link_libraries(shournal-run
    lib_shournal_common
    pthread
    uuid
    cap # capabilites
    )


install(
    TARGETS shournal-run
    RUNTIME DESTINATION bin
    PERMISSIONS SETUID
                OWNER_READ OWNER_WRITE OWNER_EXECUTE
                GROUP_READ GROUP_EXECUTE
                WORLD_READ WORLD_EXECUTE
)
