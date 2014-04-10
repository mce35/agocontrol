IF (DEFINED my_device_service)
    string(REGEX REPLACE "\.in$" "" ${my_device_service_out} ${my_device_service})
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/${my_device_service}"
        "${CMAKE_CURRENT_BINARY_DIR}/${my_device_service_out}"
        @ONLY
    )
    install (FILES ${CMAKE_CURRENT_BINARY_DIR}/${my_device_service_out} DESTINATION /lib/systemd/system)
    set(my_device_service)
    set(my_device_service_out)
ENDIF (DEFINED my_device_service)

IF (DEFINED my_device_config)
    string(REGEX REPLACE "\.in$" "" ${my_device_config_out} ${my_device_config})
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/${my_device_config}"
        "${CMAKE_CURRENT_BINARY_DIR}/${my_device_config_out}"
        @ONLY
    )
    install (FILES ${CMAKE_CURRENT_BINARY_DIR}/${my_device_config_out} DESTINATION ${CONFDIR}/conf.d)
    set(my_device_config)
    set(my_device_config_out)
ENDIF (DEFINED my_device_config)

IF (DEFINED my_device_schema)
    install (FILES ${CMAKE_CURRENT_SOURCE_DIR}/${my_device_schema} DESTINATION ${CONFDIR}/schema.d)
    set(my_device_schema)
ENDIF (DEFINED my_device_schema)
