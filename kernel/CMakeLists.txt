
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


if(EXISTS "/lib/modules/${CMAKE_SYSTEM_VERSION}/build")
    set(mod_buildir "/lib/modules/${CMAKE_SYSTEM_VERSION}/build")
else()
    set(mod_buildir "/lib/modules/${CMAKE_SYSTEM_VERSION}")
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

set(KBUILD_CMD $(MAKE)
    -C "${mod_buildir}"
    M=${CMAKE_CURRENT_BINARY_DIR}
    src=${CMAKE_CURRENT_SOURCE_DIR}
    EXTRA_CFLAGS=${k_extra_cflags}
    modules
    shournal_cmake_build=true
    shournal_version=${shournal_version}
)

# For development purposes, create the kernel module
# right here, else see below.
add_custom_target(shournalk ALL
    COMMAND ${KBUILD_CMD}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    VERBATIM
)

set(dkms_dir "${CMAKE_BINARY_DIR}/dkms")

# Copy required files to dkms dir
# Better be safe and copy the files on cmake and make.
file(COPY ${kernel_src} DESTINATION "${dkms_dir}" )
add_custom_command(
        TARGET shournalk POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
                ${kernel_src}
                "${dkms_dir}")


# copy-and-rename workaround (not really a 'configure_file')
configure_file(Kbuild "${dkms_dir}/Makefile" COPYONLY)
configure_file( "dkms.conf.in" "${dkms_dir}/dkms.conf")


install( DIRECTORY "${dkms_dir}/" DESTINATION "${shournal_install_dir_shournalk_src}" )





