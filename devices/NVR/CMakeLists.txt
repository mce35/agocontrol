cmake_minimum_required (VERSION 2.6)

file (GLOB_RECURSE DEVICE_SOURCE_FILES *.cpp)
set(DEVICE_SOURCE_FILES
    nvr.cpp
    frameprovider.cpp
)

set (VIDEO_LIBRARIES
    agoclient
    pthread
    opencv_core
    opencv_imgproc
    opencv_objdetect
    opencv_highgui
)

# add the executable
add_executable (agonvr ${DEVICE_SOURCE_FILES})
target_link_libraries (agonvr ${VIDEO_LIBRARIES})

install (TARGETS agonvr RUNTIME DESTINATION ${BINDIR})
