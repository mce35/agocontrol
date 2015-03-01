cmake_minimum_required (VERSION 2.6)

set (AGOSYSTEM_SOURCE_FILES agosystem.cpp)
IF (FREEBSD)
    list(APPEND AGOSYSTEM_SOURCE_FILES processinfo_freebsd.cpp)
ELSE()
    list(APPEND AGOSYSTEM_SOURCE_FILES processinfo_linux.cpp)
ENDIF()

set (SYSTEM_LIBRARIES
    agoclient
	${Boost_PROGRAM_DATE_TIME_LIBRARY}
)

IF (FREEBSD)
    list(APPEND SYSTEM_LIBRARIES kvm)
ELSE()
    list(APPEND SYSTEM_LIBRARIES procps)
ENDIF()

# add the executable
add_executable (agosystem ${AGOSYSTEM_SOURCE_FILES})
target_link_libraries (agosystem ${SYSTEM_LIBRARIES})

install (TARGETS agosystem RUNTIME DESTINATION ${BINDIR})
