
include_directories(
    ../
)

SET(qsqlthrow_files
    qexcdatabase
    qsqlquerythrow
    )

add_library(lib_qsqlthrow
    ${qsqlthrow_files}
    )

target_link_libraries(lib_qsqlthrow PUBLIC
    lib_util
    Qt5::Sql
)


