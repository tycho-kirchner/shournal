
# get kernel release
execute_process(
        COMMAND uname -r
        OUTPUT_VARIABLE KERNEL_RELEASE
        OUTPUT_STRIP_TRAILING_WHITESPACE
)
string(REGEX REPLACE "-[^-]+$" "" KERNEL_RELEASE_NO_ARCH ${KERNEL_RELEASE})

# Find the headers
foreach(header_path
        /usr/src/linux-headers-${KERNEL_RELEASE_NO_ARCH}-common # Debian
        /usr/src/linux-${KERNEL_RELEASE_NO_ARCH}/include)       # Opensuse
    if(EXISTS "${header_path}")
        set(KERNELHEADERS_DIR "${header_path}")
        break()
    endif()
endforeach()

if(NOT (KERNELHEADERS_DIR))
    # Red Hat (?)
    find_path(KERNELHEADERS_DIR
        include/linux/user.h
        PATHS /usr/src/kernels/${KERNEL_RELEASE})
endif()


message(STATUS "Kernel release: ${KERNEL_RELEASE}")

if (KERNELHEADERS_DIR)
set(KERNELHEADERS_INCLUDE_DIRS
        ${KERNELHEADERS_DIR}/include
        ${KERNELHEADERS_DIR}/arch/x86/include)
    message(STATUS "Kernel headers: ${KERNELHEADERS_INCLUDE_DIRS}")
else()
    message(WARNING "Unable to find kernel headers!")
endif()
