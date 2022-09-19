cmake_minimum_required(VERSION 3.6)

if (NOT (CMAKE_VERSION VERSION_LESS "3.20"))
    cmake_policy( SET CMP0115 OLD )
endif()

# version applies to all released files: shournal, shournal-run, libshournal-shellwatch.so
# and shell-integration-scripts (e.g. integration_ko.bash)
set(shournal_version "2.9")

cmake_policy( SET CMP0048 NEW )
project(shournal VERSION ${shournal_version} LANGUAGES CXX C)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
find_package(ShournalUtil REQUIRED)

if (NOT DEFINED MSENTER_GROUPNAME)
    set(MSENTER_GROUPNAME "shournalmsenter")
endif()
add_definitions( -DSHOURNAL_MSENTERGROUP="${MSENTER_GROUPNAME}")

# No need to make configurable - user can override in /etc/shournal.d/kgroup
# (or, not recommended, use a custom rule in /etc/udev/rules.d).
# DO NOT CHANGE, it is documented in the README.
set(GROUPNAME_SHOURNALK "shournalk")


# Inside docker no kernel module may be installed,
# but we default to using the host's kernel-module.
# When only shournal-run-fanotify is desired, no need to
# compile shournal-run.
# -DSHOURNAL_EDITION={full, docker, ko, fanotify}
if(NOT DEFINED SHOURNAL_EDITION)
    set(SHOURNAL_EDITION "full")
endif()


if(NOT ${SHOURNAL_EDITION} MATCHES "full|docker|ko|fanotify")
     message( FATAL_ERROR "invalid SHOURNAL_EDITION passed: ${SHOURNAL_EDITION}" )
endif()

set (CMAKE_CXX_STANDARD 11)

if(CMAKE_COMPILER_IS_GNUCXX)
    if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 5.0)
        message(FATAL_ERROR "GCC version must be at least 5.0!")
    endif()
else()
    message(WARNING "You are using an unsupported compiler. Compilation was only tested with GCC.")
endif()

if ( CMAKE_COMPILER_IS_GNUCXX )
    set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wunused-result -Werror=return-type")
endif()


add_definitions( -DSHOURNAL_VERSION="${shournal_version}" )


if (NOT EXISTS ${CMAKE_BINARY_DIR}/CMakeCache.txt)
  if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
  endif()
endif()


IF(CMAKE_BUILD_TYPE MATCHES Release)
    ADD_DEFINITIONS( -DQT_NO_DEBUG_OUTPUT=1)
    SET(CMAKE_AR  "gcc-ar")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -s")
ENDIF()

# Profile purposes
IF(CMAKE_BUILD_TYPE MATCHES RelWithDebInfo)
    ADD_DEFINITIONS( -DQT_NO_DEBUG_OUTPUT=1)
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 -g -DNDEBUG")
ENDIF()

# Meaningful stacktraces:
SET (CMAKE_ENABLE_EXPORTS TRUE)


# cmake policy: allow for hidden symbols in static libs
cmake_policy( SET CMP0063 NEW )
set(CMAKE_POLICY_DEFAULT_CMP0063 NEW)

function(hide_static_lib_symbols staticLib)    
    set_target_properties(${staticLib} PROPERTIES CXX_VISIBILITY_PRESET hidden)
    set_target_properties(${staticLib} PROPERTIES CMAKE_VISIBILITY_INLINES_HIDDEN 1)
endfunction(hide_static_lib_symbols)

# append the content of f2 to f1
function(append_to_file f1 f2)
  file(READ ${f2} CONTENTS)
  file(APPEND ${f1} "${CONTENTS}")
endfunction()


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


include(GNUInstallDirs)
set(shournal_install_dir_script ${CMAKE_INSTALL_FULL_DATAROOTDIR}/${PROJECT_NAME})
set(shournal_install_dir_lib ${CMAKE_INSTALL_FULL_LIBDIR}/${PROJECT_NAME})
set(shournal_install_dir_shournalk_src /usr/src/shournalk-${shournal_version})

set(libshournal_fullname "libshournal-shellwatch.so")
set(full_path_libshournal ${shournal_install_dir_lib}/${libshournal_fullname})


set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
# qt resource files (.qrc):
set(CMAKE_AUTORCC ON)


find_package(Qt5 COMPONENTS Core Sql Network REQUIRED)

include_directories(
    extern/tsl-ordered-map
    extern/folly
    extern/xxHash
    )


add_subdirectory("extern/tsl-ordered-map")
set(XXHASH_BUNDLED_MODE ON)
add_subdirectory(extern/xxHash/cmake_unofficial EXCLUDE_FROM_ALL)

add_subdirectory("src")
add_subdirectory("shell-integration-scripts")
add_subdirectory("install")

# Kernel module
if(${SHOURNAL_EDITION} MATCHES "full|ko")
    add_subdirectory("kernel")
endif()

# Turn on tests with 'cmake -Dtest=ON'.
# To run the tests enter directory "test" within the build directory
# and enter "ctest".
option(test "Build all tests." OFF)

if (test)
   add_subdirectory("test")
endif()

# install license
install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
    RENAME copyright # following Lintian
    DESTINATION ${CMAKE_INSTALL_FULL_DOCDIR}
    )

############## Package creation using 'cpack' ##############

# generic
set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_VERSION ${shournal_version})
set(CPACK_PACKAGE_CONTACT "Tycho Kirchner <tychokirchner@mail.de>")
SET(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
# If CPACK_INSTALL_PREFIX is not set, let it default to CMAKE_INSTALL_PREFIX
# see also: https://stackoverflow.com/a/7363073/7015849
# set(CPACK_SET_DESTDIR true)

set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
   "File-journal for your shell"
)

set(SHOURNAL_CONFLICTS_LIST
    "shournal" "shournal-docker"
    "shournal-ko" "shournal-fanotify"
)

if(${SHOURNAL_EDITION} STREQUAL "full")
    set(CPACK_PACKAGE_NAME "shournal")
    set(edition_description "full suite (all backends)")
elseif(${SHOURNAL_EDITION} STREQUAL "ko")
    set(CPACK_PACKAGE_NAME "shournal-ko")
    set(edition_description "kernel backend only (fanotify backend not included)")
elseif(${SHOURNAL_EDITION} STREQUAL "fanotify")
    set(CPACK_PACKAGE_NAME "shournal-fanotify")
    set(edition_description "fanotify backend only (no kernel module included)")
elseif(${SHOURNAL_EDITION} STREQUAL "docker")
    set(CPACK_PACKAGE_NAME "shournal-docker")
    set(edition_description "docker-version to be installed inside containers")
else()
    message( FATAL_ERROR "invalid cpack COMPONENT: ${SHOURNAL_EDITION}" )
endif()

list(REMOVE_ITEM SHOURNAL_CONFLICTS_LIST
    "${CPACK_PACKAGE_NAME}"
    )
JOIN("${SHOURNAL_CONFLICTS_LIST}" ", " SHOURNAL_CONFLICTS)

# CPACK_DEBIAN_PACKAGE_DESCRIPTION requires newlines
# be indented by one space. For the sake of simplicity:
# No new lines here:
set(CPACK_PACKAGE_DESCRIPTION
    "Integrated tool to increase the reproducibility \
of your work on the shell: what did you do when and \
where and what files were modified/read. This package \
provides the ${edition_description}.")

# deb specific
# set(CPACK_GENERATOR "DEB")

set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "${CPACK_PACKAGE_DESCRIPTION}")
execute_process(COMMAND dpkg --print-architecture OUTPUT_VARIABLE DEB_ARCH
    OUTPUT_STRIP_TRAILING_WHITESPACE)
set(CPACK_DEBIAN_FILE_NAME
    ${CPACK_PACKAGE_NAME}_${shournal_version}_${DEB_ARCH}.deb)
set(CPACK_DEBIAN_PACKAGE_CONFLICTS "${SHOURNAL_CONFLICTS}")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/tycho-kirchner/shournal")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.2), libstdc++6 (>= 5.0), libgcc1, \
libqt5core5a (>= 5.6), libqt5network5, libqt5sql5-sqlite, libcap2, uuid-runtime"
)
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")

# generate the postinst based on the groupname
set(debPostinstPath "${CMAKE_BINARY_DIR}/debian/postinst")
set(debPrermPath "${CMAKE_BINARY_DIR}/debian/prerm")

set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
    "${debPostinstPath}"
    "${debPrermPath}"
    )
set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION TRUE)

if(${SHOURNAL_EDITION} MATCHES "full|ko")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS
        "${CPACK_DEBIAN_PACKAGE_DEPENDS}, dkms")
    append_to_file( "${debPostinstPath}" ${CMAKE_BINARY_DIR}/install/postinst-dkms )
    append_to_file( "${debPrermPath}" ${CMAKE_BINARY_DIR}/install/prerm-dkms )
endif()

# call it *after* setting above variables, otherwise
# generic .gz's are generated.
include(CPack)



