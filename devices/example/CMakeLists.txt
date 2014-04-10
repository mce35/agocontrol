#add a minimum required version
cmake_minimum_required (VERSION 2.6)

# only set these variables if you actually have the files
# set (my_device_schema "example.yaml")
# set (my_device_service "example.service")
# set (my_device_config "example.conf")

# include this after you set your device variables to
# automatically configure the files and install into
# their proper directories
include(${CMAKE_SOURCE_DIR}/cmake/CMake_Device_Common.cmake)


# select your source files
file (GLOB_RECURSE DEVICE_SOURCE_FILES *.cpp)

# set extra libraries you need to link to
# your binary
set (DEVICE_EXTRA_LIBRARIES
     agoclient
)

#set a device name. This will be the 'target'
set (DEVICE "example")

# add the executable
add_executable (${DEVICE} ${DEVICE_SOURCE_FILES})

# link all needed libraries
target_link_libraries (${DEVICE} ${DEVICE_EXTRA_LIBRARIES})

# create an install target for the device
install (TARGETS ${DEVICE} RUNTIME DESTINATION ${BINDIR})
