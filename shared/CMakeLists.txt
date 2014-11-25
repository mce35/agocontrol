cmake_minimum_required (VERSION 2.6)

configure_file (
    "${CMAKE_CURRENT_SOURCE_DIR}/agoclient/_directories.py.in"
    "${CMAKE_CURRENT_BINARY_DIR}/agoclient/_directories.py"
    @ONLY
)

set(LIBAGOCLIENT_SOURCE_FILES
	agoclient.cpp
	agolog.cpp
	agoapp.cpp
	agoconfig.cpp
	base64.cpp
)

set(PYTHON_AGOCLIENT_SOURCE_FILES
    agoapp.py
    agoconnection.py
    config.py
    _directories.py
    __init__.py
    _logging.py
)

IF (Boost_LOG_FOUND)
	list(APPEND LIBAGOCLIENT_SOURCE_FILES agolog_boost.cpp)
else()
	list(APPEND LIBAGOCLIENT_SOURCE_FILES agolog_basic.cpp)
endif()

add_library (agoclient SHARED ${LIBAGOCLIENT_SOURCE_FILES})
set_target_properties (agoclient PROPERTIES VERSION 1 SOVERSION 1)

set (AGOCLIENT_LIBRARIES
    qpidmessaging
    qpidtypes
    uuid
    augeas
	 ${Boost_SYSTEM_LIBRARY}
	 ${Boost_FILESYSTEM_LIBRARY}
	 ${Boost_THREAD_LIBRARY}
	 ${Boost_PROGRAM_OPTIONS_LIBRARY}
    ${JSONCPP_LIBRARIES}
)


IF (Boost_LOG_FOUND)
	LIST(APPEND AGOCLIENT_LIBRARIES ${Boost_LOG_LIBRARY})
ELSE()
	LIST(APPEND AGOCLIENT_LIBRARIES ${Boost_DATE_TIME_LIBRARY})
ENDIF()

target_link_libraries (agoclient ${AGOCLIENT_LIBRARIES})

set_property(
	TARGET agoclient
	PROPERTY COMPILE_DEFINITIONS
	DEFAULT_CONFDIR=${CONFDIR}
	DEFAULT_LOCALSTATEDIR=${LOCALSTATEDIR}
)

set_property(
	TARGET agoclient
	APPEND PROPERTY
	COMPILE_OPTIONS
	-Werror)


# Install instructions for this target
install (TARGETS agoclient LIBRARY DESTINATION ${LIBDIR})
install (DIRECTORY DESTINATION ${PYTHON2DIST}/agoclient)
install (PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/agoclient/agoapp.py DESTINATION ${PYTHON2DIST}/agoclient)
install (PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/agoclient/agoconnection.py DESTINATION ${PYTHON2DIST}/agoclient)
install (PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/agoclient/config.py DESTINATION ${PYTHON2DIST}/agoclient)
install (PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/agoclient/_directories.py DESTINATION ${PYTHON2DIST}/agoclient)
install (PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/agoclient/__init__.py DESTINATION ${PYTHON2DIST}/agoclient)
install (PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/agoclient/_logging.py DESTINATION ${PYTHON2DIST}/agoclient)
install (DIRECTORY DESTINATION ${CONFDIR}/uuidmap)

