
include_directories(
    ../
)

add_library(lib_qoptargparse
    qoptargparse
    qoptarg
    qoptsqlarg
    qoptvarlenarg
    qoptargtrigger
    excoptargparse
    ../exccommon
    ../cpp_exit
    ../user_str_conversions
 )


target_link_libraries(lib_qoptargparse PUBLIC
    lib_util
)
