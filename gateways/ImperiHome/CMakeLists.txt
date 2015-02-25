cmake_minimum_required (VERSION 2.6)

IF(COMMAND cmake_policy)
    cmake_policy(SET CMP0009 NEW)
ENDIF(COMMAND cmake_policy)

set (IMPERIHOME_LIBRARIES
    agoclient
    pthread
)

IF (NOT FREEBSD)
	list(APPEND IMPERIHOME_LIBRARIES dl )
ENDIF (NOT FREEBSD)

# add the executable
add_executable (agoImperiHome agoImperiHome.cpp ../../core/rpc/mongoose.c)
target_link_libraries (agoImperiHome ${IMPERIHOME_LIBRARIES})

install (TARGETS agoImperiHome RUNTIME DESTINATION ${BINDIR})

