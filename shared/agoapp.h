#ifndef AGOAPP_H
#define AGOAPP_H

#include "agoclient.h"
#include <boost/asio/io_service.hpp>
#include <boost/thread.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/variables_map.hpp>

/**
 * Provides boilerplate code for writing an AGO application.
 *
 * Each application needs to implement a class
 * which extends the AgoApp class. A helpful macro for the constructor
 * can be used:
 *
 * 	class MyApp: public AgoApp {
 * 		AGOAPP_CONSTRUCTOR(MyApp);
 *
 * 		// App main code goes here
 * 		int appMain();
 * 	}
 *
 * It must then add a simple main entry point:
 * 	APP_ENTRY_POINT(MyAgoApp);
 *
 */


/* This helps with a class constructor */
#define AGOAPP_CONSTRUCTOR_HEAD(class_name) class_name() : AgoApp(#class_name)
#define AGOAPP_CONSTRUCTOR(class_name) AGOAPP_CONSTRUCTOR_HEAD(class_name) {};

/* This defines the applications main() function */
#define AGOAPP_ENTRY_POINT(app_class_name) \
    int main(int argc, const char**argv) {		\
        app_class_name instance;					\
        return instance.main(argc, argv);		\
    }

namespace agocontrol {

    class TestAgoConfig;
    class AgoApp {
    friend class TestAgoConfig;

    private :
        const std::string appName;
        std::string appShortName;

        bool exit_signaled;
        boost::thread ioThread;
        boost::asio::io_service ioService_;
        std::auto_ptr<boost::asio::io_service::work> ioWork;

        int parseCommandLine(int argc, const char **argv);

    protected:
        /* This can be overriden from the applications constructor.
         * Preferably, don't!*/
        std::string appConfigSection;

        /**
         * Any parsed command line parameters are placed in this map-like object
         */
        boost::program_options::variables_map cli_vars;

        void signalExit();

        AgoConnection *agoConnection;

        /* Obtain a reference to the ioService for async operations.
         *
         * By doing this, we also tell AgoApp base that we want to have an IO
         * thread running for the duration of the program.
         *
         * If this is never called during setup() phase, we will not start the IO thread.
         * However, if called at least once, we can call it whenever again later.
         */
        boost::asio::io_service& ioService();

        /* Application should implement this function if it like to present
         * any extra command line options to the user.
         */
        virtual void appCmdLineOptions(boost::program_options::options_description &options) {}

        /* Setup will be called prior to appMain being called. This
         * normally sets up logging, an AgoConnection, signal handling,
         * and finally any app specific setup (setupApp).
         *
         * Please keep setupXX() functions in the order called!
         */
        virtual void setup();
        void setupLogging();
        void setupAgoConnection();
        void setupSignals();
        /* App specific init can be done in this */
        virtual void setupApp() { };
        void setupIoThread();

        /* After appMain has returned, we will call cleanup to release
         * any resources */
        void cleanup();
        void cleanupAgoConnection();
        virtual void cleanupApp() { };
        void cleanupIoThread();

        bool isExitSignaled() { return exit_signaled; }

        /* Shortcut to register the commandHandler with agoConnection.
         * Should be called from setupApp */
        void addCommandHandler();

        /* Shortcut to register the eventHandler with agoConnection.
         * Should be called from setupApp */
        void addEventHandler();

        /* Command handler registered with the agoConncetion; override! */
        virtual qpid::types::Variant::Map commandHandler(qpid::types::Variant::Map content) {
            return qpid::types::Variant::Map();
        }

        /* Event handler registered with the agoConncetion; override! */
        virtual void eventHandler(std::string subject, qpid::types::Variant::Map content) {}

        /**
         * This is called from a separate thread when the app is
         * to shutdown, triggered via SIGINT and SIGQUIT.
         *
         * The base impl calls agoConnection->close().
         */
        virtual void doShutdown();

        /**
         * Read a config option from the configuration subsystem.
         *
         * The system is based on per-app configuration files, which has sections
         * and options.
         *
         * The simplest, and most common, usage only specifies the option and
         * defaultValue. This looks in the applications own configuration file, in a
         * section named after the app.
         * Some apps might want to use a custom section too.
         * Edge-case apps might want to look in other files, by specifying value for app.
         *
         * It is posible to look at multiple sections, for example for fallback if it's
         * not present in the main section. To do this, pass an explicit
         * ExtraConfigNameList object as section/app. This will look first at the apps
         * primary section/app, then at any sections/apps specified in the ExtraConfigNameList.
         * To completely override the probed sections/apps, use a plain ConfigNameList.
         *
         * Note that a regular char* or std::string can be passed instead of
         * ConfigNameList objects, it will create an implicit ConfigNameList.
         *
         * Arguments:
         *  option -- The name of the option to retreive
         *
         *  defaultValue -- If the option can not be found in any of the specified
         *      sections, fall back to this value.
         *
         *  section -- A ConfigNameList section to look for the option in.
         *      Defaults to the applications shorted name.
         *
         *  app -- A ConfigNameList identifying the configuration storage unit to look in.
         *      If omited, it defaults to the section(s).
         *
         * Lookup order:
         *  For each specified app, we look at each specified section/option. First non-empty
         *  value wins.
         *
         * Returns:
         *  The value found.
         *  If not found, defaultValue is passed through unmodified.
         */
        std::string getConfigOption(const char *option, const char *defaultValue, const ConfigNameList &section=BLANK_CONFIG_NAME_LIST, const ConfigNameList &app = BLANK_CONFIG_NAME_LIST) {
            ConfigNameList section_(section, appConfigSection);
            ConfigNameList app_(app, section_);
            return getConfigSectionOption(section_, option, defaultValue, app_);
        }
        std::string getConfigOption(const char *option, std::string &defaultValue, const ConfigNameList &section=BLANK_CONFIG_NAME_LIST, const ConfigNameList &app = BLANK_CONFIG_NAME_LIST) {
            ConfigNameList section_(section, appConfigSection);
            ConfigNameList app_(app, section_);
            return getConfigSectionOption(section_, option, defaultValue, app_);
        }
        boost::filesystem::path getConfigOption(const char *option, const boost::filesystem::path &defaultValue, const ConfigNameList &section=BLANK_CONFIG_NAME_LIST, const ConfigNameList &app = BLANK_CONFIG_NAME_LIST) {
            ConfigNameList section_(section, appConfigSection);
            ConfigNameList app_(app, section_);
            return getConfigSectionOption(section_, option, defaultValue, app_);
        }

        /**
         * Write a config option to the configuration subsystem.
         *
         * The system is based on per-app configuration files, which has sections
         * and options.
         *
         * This is a low-level implementation, please try to use AgoApp's instance version
         * of setConfigOption instead.
         *
         * Arguments:
         *  section -- A string section in which to store the option in.
         *
         *  option -- The name of the option to set
         *
         *  value -- The value to write
         *
         *  app -- A string identifying the configuration storage unit to store to.
         *      If omited, it defaults to the section.
         *
         * Returns:
         *  true if sucesfully stored, false otherwise.
         *  Please refer to the error log for failure indication.
         */
        bool setConfigOption(const char *option, const char* value, const char *section=NULL, const char *app=NULL) {
            if(section == NULL) section = appConfigSection.c_str();
            if(app == NULL)     app = section;
            return setConfigSectionOption(section, option, value, app);
        }
        bool setConfigOption(const char *option, const float value, const char *section=NULL, const char *app=NULL) {
            if(section == NULL) section = appConfigSection.c_str();
            if(app == NULL)     app = section;
            return setConfigSectionOption(section, option, value, app);
        }
        bool setConfigOption(const char *option, const int value, const char *section=NULL, const char *app=NULL) {
            if(section == NULL) section = appConfigSection.c_str();
            if(app == NULL)     app = section;
            return setConfigSectionOption(section, option, value, app);
        }
        bool setConfigOption(const char *option, const bool value, const char *section=NULL, const char *app=NULL) {
            if(section == NULL) section = appConfigSection.c_str();
            if(app == NULL)     app = section;
            return setConfigSectionOption(section, option, value, app);
        }
    public:
        AgoApp(const char *appName);
        virtual ~AgoApp() {} ;

        /**
         * Main method, call this from application main entry point (generally this
         * is done using the AGOAPP_ENTRY_POINT macro).
         */
        int main(int argc, const char **argv);

        /**
         * By default this calls agoConnection->run(), and blocks until shutdown signal
         * arrives.
         * This can be overridden if non-standard behavior is required.
         */
        virtual int appMain();

        // Not really public
        void sighandler(int signo);
    };

    class ConfigurationError: public std::runtime_error {
    public:
        ConfigurationError(const std::string &what)
            : runtime_error(what) {}
        ConfigurationError(const char *what)
            : runtime_error(std::string(what)) {}
    };

    /* This can be raised from any setup method, to indicate setup failure.
     * Application should log actual error itself!
     */
    class StartupError: public std::runtime_error {
    public:
        StartupError(): runtime_error("") {};
    };


}/* namespace agocontrol */

#endif
