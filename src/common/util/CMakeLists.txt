

add_library(lib_util
    cleanupresource
    compareoperator
    conversions
    cpp_exit
    exccommon
    qoutstream
    qformattedstream
    strlight
    strlight_util
    staticinitializer
    sys_ioprio.h
    translation
    util
    util_performance
    )


target_link_libraries(lib_util PUBLIC
    Qt5::Core
    uuid
    )
