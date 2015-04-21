cmake_minimum_required (VERSION 2.6)

set(ONVIF_LIB_PATH "onvif-0.1.2")

find_program(PYTHON "python")
if (PYTHON)
    set(SETUP_PY    "${ONVIF_LIB_PATH}/setup.py")
    set(OUTPUT      "${CMAKE_CURRENT_BINARY_DIR}/build/timestamp")

    add_custom_command(OUTPUT ${OUTPUT}
                       COMMAND ${PYTHON} ${SETUP_PY} build
                       COMMAND ${CMAKE_COMMAND} -E touch ${OUTPUT})

    add_custom_target(target ALL DEPENDS ${OUTPUT})

    install(CODE "execute_process(COMMAND ${PYTHON} ${SETUP_PY} install)")
endif()

install (PROGRAMS agoonvif.py DESTINATION ${BINDIR})
