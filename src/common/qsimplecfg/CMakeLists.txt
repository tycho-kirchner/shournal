
include_directories(
    ../
)


SET(qsimplecfg_files
    cfg
    section
    exccfg
    ../exccommon
    ../cflock
    ../conversions
    )

add_library(lib_qsimplecfg
    ${qsimplecfg_files}
 )

add_library(lib_qsimplecfg_shared
    ${qsimplecfg_files}
    )
set_property(TARGET lib_qsimplecfg_shared PROPERTY POSITION_INDEPENDENT_CODE ON)
hide_static_lib_symbols(lib_qsimplecfg_shared)

target_link_libraries(lib_qsimplecfg PUBLIC
    lib_util
    lib_orderedmap
    lib_qformattedstream
)

target_link_libraries(lib_qsimplecfg_shared
    lib_util_shared
    lib_orderedmap_shared
    lib_qformattedstream_shared
    )
