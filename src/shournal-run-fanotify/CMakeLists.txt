
# This program is somewhat performance-critical,
# so disable exports. FIXME: benchmark it
IF(CMAKE_BUILD_TYPE MATCHES Release)
    SET (CMAKE_ENABLE_EXPORTS FALSE)
ENDIF()



include_directories(
    ../common
    ../common/qoptargparse
    ../common/database
    ../common/qsimplecfg
    ../common/oscpp
    ../common/database
    ../common/qsqlthrow
    ../../
    )


add_executable(shournal-run-fanotify
    shournal-run-fanotify.cpp
    fanotify_controller.cpp
    filewatcher_fan.cpp
    mount_controller.cpp
    msenter.cpp
    orig_mountspace_process.cpp
    )

target_link_libraries(shournal-run-fanotify
    lib_shournal_common
    pthread
    uuid
    cap # capabilites
    )


install(
    TARGETS shournal-run-fanotify
    RUNTIME DESTINATION bin
    PERMISSIONS SETUID
                OWNER_READ OWNER_WRITE OWNER_EXECUTE
                GROUP_READ GROUP_EXECUTE
                WORLD_READ WORLD_EXECUTE
)
