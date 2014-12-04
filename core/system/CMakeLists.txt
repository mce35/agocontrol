cmake_minimum_required (VERSION 2.6)

file (GLOB_RECURSE AGOSYSTEM_SOURCE_FILES *.cpp)
set (SYSTEM_LIBRARIES
    agoclient
	${Boost_PROGRAM_DATE_TIME_LIBRARY}
    procps
)

# add the executable
add_executable (agosystem ${AGOSYSTEM_SOURCE_FILES})
target_link_libraries (agosystem ${SYSTEM_LIBRARIES})

install (TARGETS agosystem RUNTIME DESTINATION ${BINDIR})
