cmake_minimum_required (VERSION 2.6)

set_property( DIRECTORY
	APPEND PROPERTY
	COMPILE_OPTIONS
	-Werror)

OPTION(BUILD_GATEWAY_ImperiHome "Build ImperiHome gateway" ON)
if (BUILD_GATEWAY_ImperiHome)
  add_subdirectory (ImperiHome)
endif()

