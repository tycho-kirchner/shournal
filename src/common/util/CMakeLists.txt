

add_library(lib_util
    cleanupresource.h
    compat.h
    compareoperator.cpp
    conversions.cpp
    cpp_exit.cpp
    exccommon.cpp
    qoutstream.cpp
    qformattedstream.cpp
    strlight.cpp
    strlight_util.cpp
    staticinitializer.h
    sys_ioprio.h
    translation.cpp
    util.cpp
    util_performance.cpp
    )


target_link_libraries(lib_util PUBLIC
    Qt5::Core
    uuid
    )
