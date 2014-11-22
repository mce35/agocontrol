#include <sys/stat.h>
#include <fstream>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include <boost/algorithm/string/join.hpp>
#include <boost/filesystem/fstream.hpp>

#include "agoapp.h"
#include "agoconfig.h"

namespace fs = boost::filesystem;
namespace agocontrol {

class TestAgoConfig : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE( TestAgoConfig ) ;
    CPPUNIT_TEST( testBasicGet );
    CPPUNIT_TEST( testFallbackGet );
    CPPUNIT_TEST( testSimulatedAppGet );
    CPPUNIT_TEST( testBasicSet );
    CPPUNIT_TEST( testSetError );
    CPPUNIT_TEST( testAppSet );
    CPPUNIT_TEST( testAppGet );
    CPPUNIT_TEST_SUITE_END();

    ConfigNameList empty;
    std::list<std::string>::const_iterator i;

    fs::path tempDir;
public:

    void setUp();
    void tearDown();

    void testBasicGet();
    void testFallbackGet();
    void testSimulatedAppGet();
    void testBasicSet();
    void testSetError();

    void testAppGet();
    void testAppSet();
};

CPPUNIT_TEST_SUITE_REGISTRATION(TestAgoConfig);

// Dummy app which does nothing
class DummyUnitTestApp: public AgoApp {
public:
    AGOAPP_CONSTRUCTOR_HEAD(DummyUnitTestApp) {
        appConfigSection = "test";
    }
};

#define ASSERT_STR(expected, actual) \
    CPPUNIT_ASSERT_EQUAL(std::string(expected), (actual))
#define ASSERT_NOSTR(actual) \
    CPPUNIT_ASSERT_EQUAL(std::string(), (actual))

void TestAgoConfig::setUp() {
    tempDir = fs::temp_directory_path() / fs::unique_path();
    CPPUNIT_ASSERT(fs::create_directory(tempDir));
    CPPUNIT_ASSERT(fs::create_directory(tempDir / "conf.d"));
   
    // XXX: try to find conf/ directory in agocontrol src root
    // assumes we're in unittest/shared/test_agoconfig.cpp
    fs::path src(__FILE__);
    fs::path src_conf_dir = src.parent_path().parent_path().parent_path() / "conf";

    // Copy lens and system.conf from src dir
    fs::path lens = src_conf_dir / "agocontrol.aug";
    CPPUNIT_ASSERT_MESSAGE(
            std::string("Failed to find lens at ") + lens.string(),
            fs::exists(lens));

    AgoClientInternal::_reset();
    AgoClientInternal::setConfigDir(tempDir);

    fs::copy_file(lens, tempDir / "agocontrol.aug");
    fs::copy_file(src_conf_dir / "conf.d" / "system.conf", getConfigPath("conf.d/system.conf"));

    // Create dumm test file
    fs::fstream fs(getConfigPath("conf.d/test.conf"), std::fstream::out);
    fs << "[test]\n"
        "existing_key=value\n"
        "[system]\n"
        "password=testpwd\n";
    fs.close();

    // Important: init augeas AFTER all files are created
    CPPUNIT_ASSERT(augeas_init());
}

void TestAgoConfig::tearDown() {
    //fs::remove_all(tempDir);
}

void TestAgoConfig::testBasicGet() {
    ASSERT_NOSTR( getConfigSectionOption("test", "key", ""));
    ASSERT_NOSTR(getConfigSectionOption("test", "key", NULL));
    ASSERT_STR("fallback", getConfigSectionOption("test", "key", "fallback"));

    ASSERT_STR("localhost", getConfigSectionOption("system", "broker", NULL));
    ASSERT_STR("value", getConfigSectionOption("test", "existing_key", NULL));
}

void TestAgoConfig::testFallbackGet() {
    ConfigNameList section("test");
    section.add("system");

    ASSERT_NOSTR(getConfigSectionOption(section, "key", ""));

    // default-value fallback
    ASSERT_STR("fallback", getConfigSectionOption(section, "nonexisting", "fallback"));

    // Overriden in test.conf
    ASSERT_STR("testpwd", getConfigSectionOption(section, "password", NULL));

    // from system.conf
    ASSERT_STR("localhost", getConfigSectionOption(section, "broker", NULL));

    // From test.conf
    ASSERT_STR("value", getConfigSectionOption(section, "existing_key", NULL));
}

void TestAgoConfig::testSimulatedAppGet() {
    ConfigNameList section("test");
    section.add("system");

    ConfigNameList app("test");

    ASSERT_NOSTR(getConfigSectionOption(section, "key", "", app));

    // default-value fallback
    ASSERT_STR("fallback", getConfigSectionOption(section, "nonexisting", "fallback", app));

    // Overriden in test.conf
    ASSERT_STR("testpwd", getConfigSectionOption(section, "password", NULL, app));

    // from system.conf; will not be found since we only look in app test
    ASSERT_NOSTR(getConfigSectionOption(section, "broker", NULL, app));

    // From test.conf
    ASSERT_STR("value", getConfigSectionOption(section, "existing_key", NULL, app));

    // Add system to app, and make sure we now can read broker
    app.add("system");
    ASSERT_STR("localhost", getConfigSectionOption(section, "broker", NULL));
}

void TestAgoConfig::testBasicSet() {
    ASSERT_STR("value", getConfigSectionOption("test", "existing_key", NULL));
    ASSERT_NOSTR(getConfigSectionOption("test", "new_key", NULL));

    CPPUNIT_ASSERT(setConfigSectionOption("test", "new_key", "val"));
    ASSERT_STR("val", getConfigSectionOption("test", "new_key", NULL));

    // Test blank app, should be same as NULL
    CPPUNIT_ASSERT(setConfigSectionOption("test", "new_key", "val2", ""));
    ASSERT_STR("val2", getConfigSectionOption("test", "new_key", NULL));

    // New file
    ASSERT_NOSTR(getConfigSectionOption("test", "new_key", NULL, "new_file"));
    CPPUNIT_ASSERT(setConfigSectionOption("test", "new_key", "val", "new_file"));
    ASSERT_STR("val", getConfigSectionOption("test", "new_key", NULL, "new_file"));
}

void TestAgoConfig::testSetError() {
    // Make file non-writeable and make sure we handle it properly
    // In augeas 1.3.0 it segfaults, we've got workarounds to avoid that.
    // https://github.com/hercules-team/augeas/issues/178
    chmod(getConfigPath("conf.d/test.conf").c_str(), 0);

    CPPUNIT_ASSERT(! setConfigSectionOption("test", "new_key", "val"));

    ASSERT_NOSTR(getConfigSectionOption("test", "new_key", NULL));

    CPPUNIT_ASSERT(! setConfigSectionOption("test", "new_key", "val", "//bad/path"));
}

void TestAgoConfig::testAppGet() {
    DummyUnitTestApp app;
    ASSERT_STR("value", app.getConfigOption("existing_key", NULL));
    ASSERT_NOSTR(app.getConfigOption("nonexisting_key", NULL));

    // This should go directly to system.conf, only
    ASSERT_STR("localhost", app.getConfigOption("broker", NULL, "system"));
    ASSERT_NOSTR(app.getConfigOption("nonexisting_key", NULL, "system"));
    ASSERT_NOSTR(app.getConfigOption("existing_key", NULL, "system"));

    // This shall fall back on system.conf
    ASSERT_STR("localhost", app.getConfigOption("broker", NULL, ExtraConfigNameList("system")));
    ASSERT_NOSTR(app.getConfigOption("nonexisting_key", NULL, ExtraConfigNameList("system")));
    ASSERT_STR("value", app.getConfigOption("existing_key", NULL, ExtraConfigNameList("system")));
}

void TestAgoConfig::testAppSet() {
    DummyUnitTestApp app;
    ASSERT_NOSTR(app.getConfigOption("nonexisting_key", NULL));
    CPPUNIT_ASSERT(app.setConfigOption("nonexisting_key", "val"));
    ASSERT_STR("val", app.getConfigOption("nonexisting_key", NULL));

    // This should go directly to system.conf
    ASSERT_STR("localhost", app.getConfigOption("broker", NULL, "system"));
    CPPUNIT_ASSERT(app.setConfigOption("broker", "remotehost", "system"));
    ASSERT_STR("remotehost", app.getConfigOption("broker", NULL, "system"));

    // Make sure we can set custom app + section
    ASSERT_NOSTR(app.getConfigOption("broker", NULL, "section", "system"));
    CPPUNIT_ASSERT(app.setConfigOption("broker", "remotehost", "section", "system"));
    ASSERT_STR("remotehost", app.getConfigOption("broker", NULL, "section", "system"));
}

}; /* namespace agocontrol */
