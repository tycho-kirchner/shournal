
SET(qsimplecfg_files
    cfg
    section
    exccfg
    )

add_library(lib_qsimplecfg
    ${qsimplecfg_files}  
 )

target_link_libraries(lib_qsimplecfg PUBLIC
    Qt5::Core
    lib_util
    oscpp_lib
    lib_orderedmap
)


