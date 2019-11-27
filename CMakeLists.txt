cmake_minimum_required(VERSION 3.0.2)


# version applies to all released files: shournal, shournal-run, libshournal-shellwatch.so
# and bash_integration.sh
set(shournal_version "2.2")

cmake_policy( SET CMP0048 NEW )
project(shournal VERSION ${shournal_version} LANGUAGES CXX)


if(CMAKE_COMPILER_IS_GNUCXX)
    if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 5.0)
        message(FATAL_ERROR "GCC version must be at least 5.0!")
    endif()
else()
    message(WARNING "You are using an unsupported compiler. Compilation was only tested with GCC.")
endif()

if ( CMAKE_COMPILER_IS_GNUCXX )
    set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wunused-result")
endif()


add_definitions( -DSHOURNAL_VERSION="${shournal_version}" )


if (NOT EXISTS ${CMAKE_BINARY_DIR}/CMakeCache.txt)
  if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
  endif()
endif()


IF(CMAKE_BUILD_TYPE MATCHES Release)
    ADD_DEFINITIONS( -DQT_NO_DEBUG_OUTPUT=1)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -s")
ENDIF()


IF (NOT DEFINED MSENTER_GROUPNAME)
    SET(MSENTER_GROUPNAME "shournalmsenter")
ENDIF()
add_definitions( -DSHOURNAL_MSENTERGROUP="${MSENTER_GROUPNAME}")


macro(use_cxx11)
  if (CMAKE_VERSION VERSION_LESS "3.1")
    if (CMAKE_COMPILER_IS_GNUCXX)
      set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++11")
    endif ()
  else ()
    set (CMAKE_CXX_STANDARD 11)
  endif ()
endmacro(use_cxx11)

use_cxx11()


if (CMAKE_VERSION VERSION_LESS "3.3")
    IF (NOT  CMAKE_COMPILER_IS_GNUCXX AND
        NOT "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        message("Warning - cannot hide all symbols of libshournal.so.")
        message("Please upgrade cmake or use clang/gcc")
    ENDIF()
else()
    # cmake policy: allow for hidden symbols in static libs
    cmake_policy( SET CMP0063 NEW )
    set(CMAKE_POLICY_DEFAULT_CMP0063 NEW)
endif()


function(hide_static_lib_symbols staticLib)
    if (CMAKE_VERSION VERSION_LESS "3.3")
        IF ( CMAKE_COMPILER_IS_GNUCXX OR
            "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        target_compile_options(${staticLib} PRIVATE "-fvisibility=hidden")
        ENDIF()
    else()
        set_target_properties(${staticLib} PROPERTIES CXX_VISIBILITY_PRESET hidden)
    endif()
    set_target_properties(${staticLib} PROPERTIES CMAKE_VISIBILITY_INLINES_HIDDEN 1)
endfunction(hide_static_lib_symbols)

# Below code could be used to strip *all* symbols, however, we do it only
# for the shared lib to allow for meaningful stacktraces in shournal and shournal-run
# CMP0063 NEW allows hiding symbols also in static libraries
# If cmake is too old, try to use compiler optinons directly or
# print warning, if that also fails.
# if (CMAKE_VERSION VERSION_LESS "3.3")
#     IF (CMAKE_COMPILER_IS_GNUCXX OR
#         "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
#         SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")
#         set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)
#     else()
#         message("Warning - cannot hide all symbols of libshournal.so.")
#         message("Please upgrade cmake or use clang/gcc")
#     ENDIF()
# else()
#     cmake_policy( SET CMP0063 NEW )
#     set(CMAKE_POLICY_DEFAULT_CMP0063 NEW)
#     set(CMAKE_CXX_VISIBILITY_PRESET hidden)
#     set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)
# endif()

function(set_version_in_bash_integration)
    # look in the bash integration, if version matches. If not, replace it and write to file
    set(pathBashIntegration "${CMAKE_CURRENT_SOURCE_DIR}/shell-integration-scripts/bash/bash_integration.sh")
    FILE(READ "${pathBashIntegration}" bashIntegration NEWLINE_CONSUME)
    # find out current bash-version
    string(REGEX MATCH  "_shournal_version=([\\.0-9]*)" currentBashVersion "${bashIntegration}")
    set(currentBashVersion "${CMAKE_MATCH_1}")

    if(NOT "${currentBashVersion}" STREQUAL "${shournal_version}")
        message("setting bash-integration version from ${currentBashVersion} to ${shournal_version}")
        STRING(REGEX REPLACE "_shournal_version=[\\.0-9]*"
                             "_shournal_version=${shournal_version}"
                             bashIntegration "${bashIntegration}" )
        FILE(WRITE "${pathBashIntegration}" "${bashIntegration}")
    endif()
endfunction(set_version_in_bash_integration)

set_version_in_bash_integration()

include(GNUInstallDirs)


set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
# qt resource files (.qrc):
set(CMAKE_AUTORCC ON)



find_package(Qt5Core)
find_package(Qt5Sql)
find_package(Qt5Network)


include_directories(extern/tsl-ordered-map)

add_subdirectory("extern/tsl-ordered-map")
set(XXHASH_BUNDLED_MODE ON)
add_subdirectory(extern/xxHash/cmake_unofficial EXCLUDE_FROM_ALL)

add_subdirectory("src")


# Turn on tests with 'cmake -Dtest=ON'.
# To run the tests enter directory "test" within the build directory
# and enter "ctest".
option(test "Build all tests." OFF)

if (test)
   add_subdirectory("test")
endif()







