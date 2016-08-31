cmake_minimum_required (VERSION 2.6)
project (agocontrol)


# The version number.
set (AGOCONTROL_VERSION 0.1)

# FreeBSD have some differences in building, mainly 
# that non-base libs/includes are located in /usr/local.
IF (CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
	set (FREEBSD YES)
	set (LOCALDIR /usr/local)
	include_directories("${LOCALDIR}/include")
	LINK_DIRECTORIES(${LOCALDIR}/lib)
ELSE (CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
	set (LOCALDIR /usr)
ENDIF (CMAKE_SYSTEM_NAME MATCHES "FreeBSD")


# some configurable values
IF (FREEBSD)
	set (BINDIR ${LOCALDIR}/bin CACHE STRING "Directory to store Binaries")
	set (ETCDIR ${LOCALDIR}/etc CACHE STRING "Directory for system-wide settings")
	set (LIBDIR /usr/lib CACHE STRING "Directory for libraries")
	set (CONFDIR ${ETCDIR}/agocontrol CACHE STRING "Directory for agocontrol settings")
	set (INCDIR ${LOCALDIR}/include/agocontrol CACHE STRING "Directory for include files")
	# XXX: Not sure what "variable" data is, but only .sql files are installed currently,
	# and those should be stored in share.
	set (DATADIR ${LOCALDIR}/share/agocontrol CACHE STRING "Directory for variable data")
	set (LOCALSTATEDIR ${LOCALDIR}/var/agocontrol CACHE STRING "Directory for state info")
	set (HTMLDIR ${LOCALDIR}/share/agocontrol/html CACHE STRING "Directory for rpc HTML files")
	set (AUGEASLENSEDIR ${LOCALDIR}/share/augeas/lenses CACHE STRING "Directory for augeas lenses")
ELSE (FREEBSD)
	set (BINDIR /opt/agocontrol/bin CACHE STRING "Directory to store Binaries")
	set (ETCDIR /etc CACHE STRING "Directory for system-wide settings")
	set (LIBDIR /usr/lib CACHE STRING "Directory for libraries")
	set (CONFDIR ${ETCDIR}/opt/agocontrol CACHE STRING "Directory for agocontrol settings")
	set (INCDIR /usr/include/agocontrol CACHE STRING "Directory for include files")
	set (DATADIR /var/opt/agocontrol CACHE STRING "Directory for variable data")
	set (LOCALSTATEDIR /var/opt/agocontrol CACHE STRING "Directory for state info")
	set (HTMLDIR /opt/agocontrol/html CACHE STRING "Directory for rpc HTML files")
	set (AUGEASLENSEDIR /usr/share/augeas/lenses CACHE STRING "Directory for augeas lenses")
	set (PYTHON2DIST /usr/lib/python2.7/dist-packages CACHE STRING "Directory for Python 2.7 packages")
	set (PYTHON3DIST /usr/lib/python3/dist-packages CACHE STRING "Directory for Python 3 packages")
	set (CMAKE_EXE_LINKER_FLAGS "-Wl,-z,now")
ENDIF (FREEBSD)


# configure a header file to pass some of the CMake settings
# to the source code
configure_file (
    "${PROJECT_SOURCE_DIR}/version.h.in"
    "${PROJECT_BINARY_DIR}/version.h"
    @ONLY
)
 
set_property( DIRECTORY
	APPEND PROPERTY
	COMPILE_OPTIONS
	-Wall -DBOOST_LOG_DYN_LINK)

set_property( DIRECTORY
	APPEND PROPERTY
	COMPILE_OPTIONS
	-DBOOST_FILESYSTEM_VERSION=3
	-DBOOST_FILESYSTEM_NO_DEPRECATED)

# add the binary tree to the search path for include files
# so that we will find version.h and other configured files
include_directories("${PROJECT_BINARY_DIR}")
include_directories("${PROJECT_BINARY_DIR}/shared")
include_directories("${PROJECT_SOURCE_DIR}/shared")

# fix augeas bug
include_directories("${LOCALDIR}/include/libxml2")

set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

# JSONCpp is used by all components (via agoclient.h)
# Note that jsoncpp is usually installed under .../include/jsoncpp, not directly into
# include. But this is not always true, i.e. Brew on Mac does not install it there.
# Do not include 'jsoncpp/' in C++ include directives!
set (JSONCPP_ROOT_DIR "${LOCALDIR}/lib")
set (JSONCPP_INCLUDE_DIR "${LOCALDIR}/include/jsoncpp")

find_package (JsonCpp REQUIRED)
if (JSONCPP_FOUND)
	include_directories(${JSONCPP_INCLUDE_DIRS})
endif (JSONCPP_FOUND)

FIND_PACKAGE(Boost COMPONENTS system filesystem thread program_options date_time regex REQUIRED)

FIND_PACKAGE(Boost COMPONENTS log)
IF (Boost_LOG_FOUND)
	ADD_DEFINITIONS(-DHAVE_BOOST_LOG)
ELSE()
	FIND_PACKAGE(Boost COMPONENTS REQUIRED date_time)
ENDIF()

FIND_PACKAGE(CppUnit)
IF (CPPUNIT_FOUND)
    message(STATUS "CppUnit found, building unit-tests by default")
    set(BUILD_TESTS_DEFAULT ON)
ELSE()
    message(STATUS "CppUnit not found, NOT building unit-tests by default")
    set(BUILD_TESTS_DEFAULT OFF)
ENDIF()

set (ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${LIBDIR}")
set (LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${LIBDIR}")
set (RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${BINDIR}")


# add sub directories
OPTION(BUILD_SHARED "Build the agoclient shared library" ON)
if (BUILD_SHARED)
  add_subdirectory (shared) 
endif()

OPTION(BUILD_CORE "Build the CORE" ON)
if (BUILD_CORE)
  add_subdirectory (core) 
  install (PROGRAMS gateways/agomeloware.py DESTINATION ${BINDIR})
  install (FILES data/inventory.sql data/datalogger.sql DESTINATION ${DATADIR})
endif()

OPTION(BUILD_GATEWAYS "Build the gateways" ON)
if (BUILD_GATEWAYS)
  add_subdirectory (gateways) 
endif()

OPTION(BUILD_DEVICES "Build the devices" ON)
if (BUILD_DEVICES)
  add_subdirectory (devices) 
endif()

OPTION(BUILD_CONF "Build the configs" ON)
if (BUILD_CONF)
  add_subdirectory (conf) 
endif()

OPTION(BUILD_SCRIPTS "Build the scripts" ON)
if (BUILD_SCRIPTS)
  add_subdirectory (scripts)
endif()

OPTION(BUILD_TESTS "Build the unittests" ${BUILD_TESTS_DEFAULT})
if (BUILD_TESTS)
  enable_testing()
  add_subdirectory (unittest)
endif()
