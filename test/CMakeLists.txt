

include_directories(
    ../src/common
    ../src/common/qsimplecfg
    ../src/common/oscpp
    ../src/common/qsqlthrow
    )

enable_testing()



find_package(Qt5Test REQUIRED)

add_executable(runTests
    main
    autotest
    test_cfg
    test_pathtree
    test_db_controller
    test_cxxhash
    test_fileeventhandler
    test_fdcommunication
    test_osutil
    test_qformattedstream
    test_qoptargparse
    test_util
    integration_test_shell
    helper_for_test

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

