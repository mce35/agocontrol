cmake_minimum_required (VERSION 2.6)

file (GLOB_RECURSE DEVICE_SOURCE_FILES *.cpp)

set(DEVICE_SOURCE_FILES
    agoMySensors.cpp
    MySensors.cpp
    serialib.cpp
)

set (DEVICE_EXTRA_LIBRARIES
    agoclient
    ${Boost_SYSTEM_LIBRARY}
    pthread
)

set (DEVICE "agoMySensors")

# add the executable
add_executable (${DEVICE} ${DEVICE_SOURCE_FILES})
target_link_libraries (${DEVICE} ${DEVICE_EXTRA_LIBRARIES})

install (TARGETS ${DEVICE} RUNTIME DESTINATION ${BINDIR})
