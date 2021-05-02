

SET(oscpp_files
    cflock
    excos
    fdentries
    os
    osutil
    oscaps

    )

add_library(oscpp_lib
    ${oscpp_files}
    )

target_link_libraries(oscpp_lib PUBLIC
    Qt5::Core
    ${CMAKE_DL_LIBS}
    lib_util
    cap
)


