
# The files here are only required for installation and
# have no direct relation to source-code.

configure_file( postinst.in "${CMAKE_BINARY_DIR}/debian/postinst" )
configure_file( prerm.in "${CMAKE_BINARY_DIR}/debian/prerm" )

# For the kernelmodule:
configure_file( "90-shournaladd.rules.in" "90-shournaladd.rules")
configure_file( shournalk-load.conf shournalk.conf )
configure_file( postinst-dkms.in postinst-dkms )
configure_file( prerm-dkms.in prerm-dkms )

if(${SHOURNAL_EDITION} MATCHES "full|ko")

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/90-shournaladd.rules"
    DESTINATION "/lib/udev/rules.d"
    )

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/shournalk.conf"
    DESTINATION "/usr/lib/modules-load.d"
    )

endif() # ${SHOURNAL_EDITION} MATCHES "full"
