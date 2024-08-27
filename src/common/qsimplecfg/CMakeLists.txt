

add_library(lib_qsimplecfg
  cfg.cpp
  section.cpp
  exccfg.cpp
 )

target_link_libraries(lib_qsimplecfg PUBLIC
    Qt5::Core
    lib_util
    oscpp_lib
    lib_orderedmap
)


