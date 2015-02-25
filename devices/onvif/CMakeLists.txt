cmake_minimum_required (VERSION 2.8)

SET_PROPERTY(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
SET_PROPERTY(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache) 

file (GLOB_RECURSE DEVICE_SOURCE_FILES "*.cpp" "*.c")
# message (STATUS "files: ${DEVICE_SOURCE_FILES}")
set (DEVICE_EXTRA_LIBRARIES
     agoclient
     crypto
     ssl
)

set (DEVICE "agoonvif")

ADD_DEFINITIONS( -DWITH_OPENSSL )

# add the executable
add_executable (${DEVICE} ${DEVICE_SOURCE_FILES})
target_link_libraries (${DEVICE} ${DEVICE_EXTRA_LIBRARIES})

install (TARGETS ${DEVICE} RUNTIME DESTINATION ${BINDIR})
