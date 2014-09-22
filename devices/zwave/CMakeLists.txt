cmake_minimum_required (VERSION 2.6)

file (GLOB_RECURSE DEVICE_SOURCE_FILES *.cpp)

set (DEVICE_EXTRA_LIBRARIES
     openzwave
     agoclient
     pthread
)

IF (FREEBSD)
	list(APPEND DEVICE_EXTRA_LIBRARIES usb iconv)
ELSE (FREEBSD)
	list(APPEND DEVICE_EXTRA_LIBRARIES udev)
ENDIF (FREEBSD)

include_directories("/usr/include/openzwave")
include_directories("/usr/include/openzwave/value_classes")
include_directories("/usr/include/openzwave/platform")
include_directories("/usr/include/openzwave/platform/unix")

# Johanhack
include_directories("/usr/home/johan/dev/openzwave-inst/usr/local/include")
include_directories("/usr/home/johan/dev/openzwave-inst/usr/local/include/openzwave")
include_directories("/usr/home/johan/dev/openzwave-inst/usr/local/include/openzwave/value_classes")
include_directories("/usr/home/johan/dev/openzwave-inst/usr/local/include/openzwave/platform")
include_directories("/usr/home/johan/dev/openzwave-inst/usr/local/include/openzwave/platform/unix")
LINK_DIRECTORIES(/usr/home/johan/dev/openzwave-inst/usr/local/lib)

set (DEVICE "agozwave")

# add the executable
add_executable (${DEVICE} ${DEVICE_SOURCE_FILES})
target_link_libraries (${DEVICE} ${DEVICE_EXTRA_LIBRARIES})

install (TARGETS ${DEVICE} RUNTIME DESTINATION ${BINDIR})
install (DIRECTORY DESTINATION ${CONFDIR}/ozw)
