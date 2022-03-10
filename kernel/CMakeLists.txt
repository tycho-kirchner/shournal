
# Find kernel headers
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
find_package(KernelHeaders REQUIRED)

set(common_prefix "../src/common")

include_directories(
    ${KERNELHEADERS_INCLUDE_DIRS}
    "${common_prefix}"
    )

# Add a dummy-library to satisfy IDE-intellisense -
# plain 'make' does the real work (see below) and file Kbuild.
# Also used for copying files to dkms-dir, so add all source-files here.
file(GLOB kernel_src
    "*.h"
    "*.c"
    "${common_prefix}/xxhash_common.h"
    "${common_prefix}/xxhash_common.c"
    "${common_prefix}/user_kernerl.h"
)
add_library(lib_shournalk_dummy
    EXCLUDE_FROM_ALL
    ${kernel_src}
)

set_target_properties(lib_shournalk_dummy PROPERTIES LANGUAGE C)

# avoid MODULE_LICENSE-warnings.
target_compile_definitions(lib_shournalk_dummy PRIVATE
  -D__KERNEL__
  -DMODULE
  -DCONFIG_MEMCG
)


if(EXISTS "/lib/modules/${KERNEL_RELEASE}/build")
    set(mod_buildir "/lib/modules/${KERNEL_RELEASE}/build")
else()
    set(mod_buildir "/lib/modules/${KERNEL_RELEASE}")
endif()

# maybe_todo: link time optimization produces weird warnings
# "-flto" # -fno-fat-lto-objects
set(k_extra_cflags "\
-I${CMAKE_CURRENT_SOURCE_DIR}/../src/common -I${CMAKE_CURRENT_SOURCE_DIR} \
"
)

if(CMAKE_BUILD_TYPE MATCHES Debug)
    set(k_extra_cflags
        "${k_extra_cflags} -DDEBUG"
    )
elseif(CMAKE_BUILD_TYPE MATCHES RelWithDebInfo)
    # profile
    set(k_extra_cflags
        "${k_extra_cflags} -O3 -DPROFILE"
    )
else()
    set(k_extra_cflags
        "${k_extra_cflags} -O3"
    )
endif()


# The kernel module is compiled from
# the dkms-directory, where the necessary source
# files are copied to beforehand.
# This has the advantage that the Kbuild file
# can be generated with the version and
# does not need to know about the files
# in ../src/common, in other words we compile
# the source code in the same way as
# a later user from the installed /usr/src/shournalk*.

set(dkms_dir "${CMAKE_BINARY_DIR}/dkms")

set(KBUILD_CMD $(MAKE)
    -C "${mod_buildir}"
    M=${CMAKE_CURRENT_BINARY_DIR}
    src=${dkms_dir}
    EXTRA_CFLAGS=${k_extra_cflags}
    modules
    shournal_cmake_build=true
)

# For development purposes, create the kernel module
# right here, else see below.
add_custom_target(shournalk ALL
    COMMAND ${KBUILD_CMD}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    VERBATIM
    DEPENDS shournalk_dep_file
)

# Copy required files to dkms dir
# Better be safe and copy the files on cmake and make.
file(COPY ${kernel_src} DESTINATION "${dkms_dir}" )
add_custom_command(
        OUTPUT shournalk_dep_file
        PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${kernel_src}
                "${dkms_dir}")


configure_file(Kbuild "${dkms_dir}/Makefile" @ONLY)
configure_file( "dkms.conf.in" "${dkms_dir}/dkms.conf" @ONLY)


install( DIRECTORY "${dkms_dir}/" DESTINATION "${shournal_install_dir_shournalk_src}" )





