cmake_minimum_required (VERSION 2.6)

file (GLOB INIT_FILES *.in)

foreach (infile ${INIT_FILES})

    string(REGEX REPLACE ".in$" "" outfile ${infile})
    string(REGEX REPLACE ".*/" "" outfile ${outfile})


    configure_file(
        "${infile}"
        "${CMAKE_CURRENT_BINARY_DIR}/${outfile}"
        @ONLY
    )

endforeach (infile)

add_subdirectory (systemd)
add_subdirectory (conf.d)
add_subdirectory (schema.d)
add_subdirectory (sysvinit)

install (FILES security-limits.conf DESTINATION ${ETCDIR}/security/limits.d RENAME agocontrol.conf)
install (FILES sysctl.conf DESTINATION ${ETCDIR}/sysctl.d RENAME agocontrol.conf)
install (FILES rpc_cert.pem DESTINATION ${CONFDIR}/rpc)
install (FILES agocontrol.aug DESTINATION ${AUGEASLENSEDIR})
