
include_directories(
    ../
)


SET(oscpp_files
    os
    excos
    osutil
    oscaps
    fdentries
    )

add_library(oscpp_lib
    ${oscpp_files}
    )


add_library(oscpp_lib_shared
    ${oscpp_files}
    )

set_property(TARGET oscpp_lib_shared PROPERTY POSITION_INDEPENDENT_CODE ON)
hide_static_lib_symbols(oscpp_lib_shared)

target_link_libraries(oscpp_lib PUBLIC
    Qt5::Core
    ${CMAKE_DL_LIBS}
    lib_util
    lib_translation
    cap

)

target_link_libraries(oscpp_lib_shared
    Qt5::Core
    ${CMAKE_DL_LIBS}
    lib_util_shared
    lib_translation_shared
    cap
    )

