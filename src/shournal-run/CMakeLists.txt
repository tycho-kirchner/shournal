

add_executable(shournal-run
    shournal-run.cpp
    fifocom.cpp
    filewatcher_shournalk.cpp
    mark_helper.cpp
    shournalk_ctrl.c
)

target_link_libraries(shournal-run
    Qt5::Core
    Qt5::Sql
    lib_shournal_common
    pthread
)


install(
    TARGETS shournal-run
    RUNTIME DESTINATION bin
    PERMISSIONS
                OWNER_READ OWNER_WRITE OWNER_EXECUTE
                GROUP_READ GROUP_EXECUTE
                WORLD_READ WORLD_EXECUTE
)

