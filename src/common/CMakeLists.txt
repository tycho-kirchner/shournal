

include_directories(
    oscpp
    qsqlthrow
    qsimplecfg
)

add_subdirectory(qoptargparse)
add_subdirectory(qsimplecfg)
add_subdirectory(oscpp)
add_subdirectory(qsqlthrow)


#########################################

add_library(lib_util
    util
    util_performance
    )

add_library(lib_util_shared
    util
    util_performance
    )

set_property(TARGET lib_util_shared PROPERTY POSITION_INDEPENDENT_CODE ON)
hide_static_lib_symbols(lib_util_shared)

target_link_libraries(lib_util PUBLIC
    Qt5::Core
    uuid
    )

target_link_libraries(lib_util_shared
    Qt5::Core
    uuid
    )

#########################################

add_library(lib_translation
    translation
    )

add_library(lib_translation_shared
    translation
    )

set_property(TARGET lib_translation_shared PROPERTY POSITION_INDEPENDENT_CODE ON)
hide_static_lib_symbols(lib_translation_shared)

target_link_libraries(lib_translation
    Qt5::Core
    )

target_link_libraries(lib_translation_shared
    Qt5::Core
    )

#########################################

add_library(lib_qformattedstream
    qformattedstream
    )

target_link_libraries(lib_qformattedstream PUBLIC
    Qt5::Core
    )

add_library(lib_qformattedstream_shared
    qformattedstream
    )

set_property(TARGET lib_qformattedstream_shared PROPERTY POSITION_INDEPENDENT_CODE ON)
hide_static_lib_symbols(lib_qformattedstream_shared)

target_link_libraries(lib_qformattedstream_shared
    Qt5::Core
    )

#########################################

SET(lib_shournal_common_files
    app
    cflock
    cleanupresource
    console_dialog
    cxxhash
    cpp_exit
    exccommon
    fdcommunication
    fileeventhandler
    fileeventtypes
    generic_container
    groupcontrol
    hashcontrol
    hashmeta
    idmapentry
    interrupt_handler
    logger
    limited_priority_queue
    pidcontrol
    pathtree
    qfddummydevice
    qfilethrow
    qoutstream
    qresource_helper
    settings
    stdiocpp
    strlight
    strlight_util
    stupidinject
    socket_message
    staticinitializer
    subprocess
    conversions
    )

set(database_files
    database/db_connection
    database/db_controller
    database/sqlite_database_scheme
    database/commandinfo
    database/sessioninfo
    database/fileinfos
    database/compareoperator
    database/sqlquery
    database/file_query_helper
    database/query_columns
    database/db_conversions
    database/sqlite_database_scheme_updates
    database/storedfiles
    database/db_globals
    database/command_query_iterator
    )



add_library(lib_shournal_common
    ${database_files}
    ${lib_shournal_common_files}

    )

add_library(lib_shournal_common_shared
    ${lib_shournal_common_files}
    )


target_link_libraries(lib_shournal_common PUBLIC
    Qt5::Sql
    Qt5::Network
    lib_translation
    lib_qsimplecfg
    oscpp_lib
    lib_qoptargparse
    lib_qformattedstream
    xxhash
    lib_qsqlthrow
    uuid
)

target_link_libraries(lib_shournal_common_shared
    lib_translation_shared
    lib_qsimplecfg_shared
    oscpp_lib_shared
    uuid
    )
