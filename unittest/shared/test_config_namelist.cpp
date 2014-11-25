#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include <boost/algorithm/string/join.hpp>

#include "agoconfig.h"

namespace agocontrol {

class TestConfigNameList : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE( TestConfigNameList ) ;
    CPPUNIT_TEST( testBasicConfigNameList );
    CPPUNIT_TEST( testFallbackConfigNameList );
    CPPUNIT_TEST( testExtraConfigNameList );
    CPPUNIT_TEST( testAppConfigNameListUsage );
    CPPUNIT_TEST_SUITE_END();

    ConfigNameList empty;
    std::list<std::string>::const_iterator i;

public:
    void testBasicConfigNameList();
    void testFallbackConfigNameList();
    void testExtraConfigNameList();
    void testAppConfigNameListUsage();
};

const std::string join_names(const ConfigNameList &c) {
    return boost::algorithm::join(c.names(), ",");
}
#define ASSERT_STRLIST(expected, cnl) \
    CPPUNIT_ASSERT_EQUAL(std::string((expected)), join_names((cnl)));

CPPUNIT_TEST_SUITE_REGISTRATION(TestConfigNameList);

void TestConfigNameList::testBasicConfigNameList() {
    CPPUNIT_ASSERT(BLANK_CONFIG_NAME_LIST.empty());

    CPPUNIT_ASSERT(empty.empty());
    CPPUNIT_ASSERT(empty.names().begin() == empty.names().end());

    /* This is the simplest way to use it; a single value. */
    ConfigNameList simple("aname");
    ASSERT_STRLIST("aname", simple);

    /* Another can be added */
    simple.add("another");

    ASSERT_STRLIST("aname,another", simple);
}

void TestConfigNameList::testFallbackConfigNameList() {
    /* Test fallback value; if first list is empty, we use the fallback
     * This implicitly creates a ConfigNameList() from the string parameter */
    ConfigNameList c(empty, "primary");

    ASSERT_STRLIST("primary", c);

    /* If list is NOT empty, we shall not use the fallback */
    ConfigNameList c2(c, "should not be used");

    ASSERT_STRLIST("primary", c2);
}

void TestConfigNameList::testExtraConfigNameList() {
    /* Test extra value; we shall always add this after the first one */
    ConfigNameList c(empty, ExtraConfigNameList("primary"));

    ASSERT_STRLIST("primary", c);

    /* *Extra*ConfigNameList shall always be added at the end */
    ConfigNameList c2(c, ExtraConfigNameList("extra"));

    ASSERT_STRLIST("primary,extra", c2);
}

void TestConfigNameList::testAppConfigNameListUsage() {
    /* This tests how the AgoApp uses the ConfigNameLists */

    /* No section/app set */
    ConfigNameList section = BLANK_CONFIG_NAME_LIST;
    ConfigNameList app = BLANK_CONFIG_NAME_LIST;

    ConfigNameList section_(section, "appname");
    ConfigNameList app_(app, section_);

    CPPUNIT_ASSERT(section.empty());
    ASSERT_STRLIST("appname", section_);

    CPPUNIT_ASSERT(app.empty());
    ASSERT_STRLIST("appname", app_);

    /* Section set explicitly */
    section = "system";
    app = BLANK_CONFIG_NAME_LIST;

    section_ = ConfigNameList(section, "appname");
    app_ = ConfigNameList(app, section_);

    ASSERT_STRLIST("system", section);

    ASSERT_STRLIST("system", section_);

    CPPUNIT_ASSERT(app.empty());
    ASSERT_STRLIST("system", app_);

    /* Section set as extra; this should use the appname primarly but fall back on
     * the extras */
    section = ExtraConfigNameList("system");
    app = BLANK_CONFIG_NAME_LIST;

    section_ = ConfigNameList(section, "appname");
    app_ = ConfigNameList(app, section_);

    ASSERT_STRLIST("system", section);
    //CPPUNIT_ASSERT(section.isExtra());
    ASSERT_STRLIST("appname,system", section_);

    CPPUNIT_ASSERT(app.empty());
    CPPUNIT_ASSERT(!app_.empty());
    ASSERT_STRLIST("appname,system", app_);


    /* Section set with multiple explicits */
    section = ConfigNameList("app").add("system");
    ASSERT_STRLIST("app,system", section);
    app = BLANK_CONFIG_NAME_LIST;

    section_ = ConfigNameList(section, "appname");
    app_ = ConfigNameList(app, section_);

    ASSERT_STRLIST("app,system", section_);
    ASSERT_STRLIST("app,system", app_);
}

}; /* namespace agocontrol */
