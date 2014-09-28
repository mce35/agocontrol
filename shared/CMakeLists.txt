cmake_minimum_required (VERSION 2.6)

configure_file (
    "${CMAKE_CURRENT_SOURCE_DIR}/agoclient.py.in"
    "${CMAKE_CURRENT_BINARY_DIR}/agoclient.py"
    @ONLY
)

file(GLOB_RECURSE LIBAGOCLIENT_SOURCE_FILES *.cpp)

add_library (agoclient SHARED ${LIBAGOCLIENT_SOURCE_FILES})
set_target_properties (agoclient PROPERTIES VERSION 1 SOVERSION 1)

set (AGOCLIENT_LIBRARIES
    qpidmessaging
    qpidtypes
    uuid
    augeas
    ${JSONCPP_LIBRARIES}
)

target_link_libraries (agoclient ${AGOCLIENT_LIBRARIES})

set_property(
	TARGET agoclient
	PROPERTY COMPILE_DEFINITIONS
	DEFAULT_CONFDIR=${CONFDIR}
	DEFAULT_LOCALSTATEDIR=${LOCALSTATEDIR}
)

# Install instructions for this target
install (TARGETS agoclient LIBRARY DESTINATION ${LIBDIR})
install (PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/agoclient.py DESTINATION ${BINDIR})
install (DIRECTORY DESTINATION ${CONFDIR}/uuidmap)

