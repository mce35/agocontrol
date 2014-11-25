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

add_test( cpp-test testrunner )

add_test( python-test python -m unittest discover -s ../shared/ )

# Shortcut to build and run all tests
add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose
    DEPENDS testrunner )

# Shortcut to build and run only cpp tests
add_custom_target(cpp-check
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose -R cpp-test
    DEPENDS testrunner )

# Shortcut to run only python tests
add_custom_target(python-check
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose -R python-test)
