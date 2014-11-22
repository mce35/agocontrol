cmake_minimum_required (VERSION 2.6)

FIND_PACKAGE(CppUnit REQUIRED)

set(TEST_SOURCE_FILES
    runner.cpp
	shared/test_agoconfig.cpp
	shared/test_config_namelist.cpp
)

set (TEST_LIBRARIES
    agoclient
    ${CPPUNIT_LIBRARIES}
)

add_executable (testrunner ${TEST_SOURCE_FILES})
target_link_libraries (testrunner ${TEST_LIBRARIES})

add_test( test testrunner )

add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose
    DEPENDS testrunner )

