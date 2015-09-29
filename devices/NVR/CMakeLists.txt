cmake_minimum_required (VERSION 2.6)

file (GLOB_RECURSE DEVICE_SOURCE_FILES *.cpp)
set(DEVICE_SOURCE_FILES
    agovideo.cpp
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
add_executable (agovideo ${DEVICE_SOURCE_FILES})
target_link_libraries (agovideo ${VIDEO_LIBRARIES})

install (TARGETS agovideo RUNTIME DESTINATION ${BINDIR})
