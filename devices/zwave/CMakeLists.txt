cmake_minimum_required (VERSION 2.6)

file (GLOB_RECURSE DEVICE_SOURCE_FILES *.cpp)

set (DEVICE_EXTRA_LIBRARIES
     tinyxml2
     openzwave
     agoclient
     pthread
)

IF (FREEBSD)
	list(APPEND DEVICE_EXTRA_LIBRARIES usb)
	IF(NOT EXISTS "/usr/include/iconv.h")
		# Pre 10.0 FreeBSD without globla iconv
		list(APPEND DEVICE_EXTRA_LIBRARIES iconv)
	ENDIF ()
ELSE (FREEBSD)
	list(APPEND DEVICE_EXTRA_LIBRARIES udev)
ENDIF (FREEBSD)

include_directories("${LOCALDIR}/include/openzwave")
include_directories("${LOCALDIR}/include/openzwave/value_classes")
include_directories("${LOCALDIR}/include/openzwave/platform")
include_directories("${LOCALDIR}/include/openzwave/platform/unix")

set (DEVICE "agozwave")

# add the executable
add_executable (${DEVICE} ${DEVICE_SOURCE_FILES})
target_link_libraries (${DEVICE} ${DEVICE_EXTRA_LIBRARIES})

install (TARGETS ${DEVICE} RUNTIME DESTINATION ${BINDIR})
install (DIRECTORY DESTINATION ${CONFDIR}/ozw)
