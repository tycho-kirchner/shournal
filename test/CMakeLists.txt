

include_directories(
    ../kernel
    ../src/common
    ../src/common/qsimplecfg
    ../src/common/oscpp
    ../src/common/util
    ../src/common/qsqlthrow
    )

enable_testing()


find_package(Qt5Test REQUIRED)

add_definitions( -DSHOURNALTEST_SQLITE_v_2_2="${CMAKE_CURRENT_SOURCE_DIR}/sqlite_sample_db_v2_2")

add_executable(runTests
    main.cpp
    autotest.h
    test_cfg.cpp
    test_pathtree.cpp
    test_db_controller.cpp
    test_cxxhash.cpp
    test_fileeventhandler.cpp
    test_fdcommunication.cpp
    test_osutil.cpp
    test_qformattedstream.cpp
    test_qoptargparse.cpp
    test_util.cpp
    integration_test_shell.cpp
    helper_for_test.cpp
)

add_test(NAME tests COMMAND runTests)

target_link_libraries(runTests
    Qt5::Test
    lib_shournal_common
    )


# run tests post build:
# add_custom_command( TARGET runTests
#      COMMENT "Run tests"
#      POST_BUILD
#      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
#      COMMAND runTests
# )

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

