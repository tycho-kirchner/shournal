
add_library(lib_qoptargparse
    excoptargparse.cpp
    qoptargparse.cpp
    qoptarg.cpp
    qoptsqlarg.cpp
    qoptvarlenarg.cpp
    qoptargtrigger.cpp
 )

target_link_libraries(lib_qoptargparse PUBLIC
    Qt5::Core
    lib_util
)
