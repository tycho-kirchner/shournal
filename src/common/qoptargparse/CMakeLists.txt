
add_library(lib_qoptargparse
    excoptargparse
    qoptargparse
    qoptarg
    qoptsqlarg
    qoptvarlenarg
    qoptargtrigger
 )

target_link_libraries(lib_qoptargparse PUBLIC
    Qt5::Core
    lib_util
)
