#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/CompilerOutputter.h>
#include <cppunit/BriefTestProgressListener.h>
#include <cppunit/TestResult.h>

int main( int argc, char **argv)
{
    CppUnit::TextUi::TestRunner runner;
    CppUnit::TestFactoryRegistry &registry = CppUnit::TestFactoryRegistry::getRegistry();

    runner.addTest( registry.makeTest() );

    runner.setOutputter( new CppUnit::CompilerOutputter( &runner.result(), std::cerr ) );
    CppUnit::BriefTestProgressListener listener;
    runner.eventManager().addListener(&listener);

    return runner.run() ? 0 : 1;
}

