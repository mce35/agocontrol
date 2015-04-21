cmake_minimum_required (VERSION 2.6)

macro (add_pyonvif)
    file (RELATIVE_PATH _relPath "${CMAKE_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
    foreach (_src ${ARGN})
        if (_relPath)
            list (APPEND SRCS "${_relPath}/${_src}")
        else()
            list (APPEND SRCS "${_src}")
        endif()
    endforeach()
    if (_relPath)
        # propagate SRCS to parent directory
        set (ONVIF_LIB ${ONVIF_LIB} PARENT_SCOPE)
    endif()
endmacro()

macro (add_wsdl)
    file (RELATIVE_PATH _relPath "${CMAKE_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
    foreach (_src ${ARGN})
        if (_relPath)
            list (APPEND SRCS "${_relPath}/${_src}")
        else()
            list (APPEND SRCS "${_src}")
        endif()
    endforeach()
    if (_relPath)
        # propagate SRCS to parent directory
        set (ONVIF_WSDL ${ONVIF_WSDL} PARENT_SCOPE)
    endif()
endmacro()

add_subdirectory(wsdl)
add_subdirectory(pyonvif)

message("========================")
message(STATUS "${ONVIF_LIB}")
message(STATUS "${ONVIF_WSDL}")
message("========================")

install (PROGRAMS agoonvif.py DESTINATION ${BINDIR})
#onvif lib
install (DIRECTORY DESTINATION ${PYTHON2DIST}/onvif)
install (PROGRAMS ${ONVIF_LIB} DESTINATION ${PYTHON2DIST}/onvif)
#wsdl
install (DIRECTORY DESTINATION /etc/onvif/wsdl)
install (PROGRAMS ${ONVIF_WSDL} DESTINATION /etc/wsdl/onvif)
