
# get kernel release
execute_process(
        COMMAND uname -r
        OUTPUT_VARIABLE KERNEL_RELEASE
        OUTPUT_STRIP_TRAILING_WHITESPACE
)
string(REGEX REPLACE "-[^-]+$" "" KERNEL_RELEASE_NO_ARCH ${KERNEL_RELEASE})

# Find the headers
if(EXISTS /usr/src/linux-headers-${KERNEL_RELEASE_NO_ARCH}-common/include)
    # Most likely on Debian or similar
    set(KERNELHEADERS_INCLUDE_DIRS
            /usr/src/linux-headers-${KERNEL_RELEASE_NO_ARCH}-common/include
            /usr/src/linux-headers-${KERNEL_RELEASE_NO_ARCH}-common/arch/x86/include )
else()
    # Red Hat (?)
    find_path(KERNELHEADERS_DIR
        include/linux/user.h
        PATHS /usr/src/kernels/${KERNEL_RELEASE})
    if (KERNELHEADERS_DIR)
    set(KERNELHEADERS_INCLUDE_DIRS
            ${KERNELHEADERS_DIR}/include
            ${KERNELHEADERS_DIR}/arch/x86/include
            CACHE PATH "Kernel headers include dirs")
    endif()
endif()


message(STATUS "Kernel release: ${KERNEL_RELEASE}")
message(STATUS "Kernel headers: ${KERNELHEADERS_INCLUDE_DIRS}")



