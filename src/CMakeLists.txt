

include_directories(
    ../extern
    ../kernel
    common/
    common/database
    common/oscpp
    common/oscpp
    common/qoptargparse
    common/qsimplecfg
    common/util
    )

add_subdirectory("common")
add_subdirectory("shournal")

if(${SHOURNAL_EDITION} MATCHES "full|docker|ko")
    add_subdirectory("shournal-run")
endif()
if(${SHOURNAL_EDITION} MATCHES "full|docker|fanotify")
    add_subdirectory("shournal-run-fanotify")
    add_subdirectory("shell-integration-fanotify")
endif()

