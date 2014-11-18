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

    class AgoApp {
    private :
        const std::string appName;
        std::string appShortName;

        bool exit_signaled;
        boost::thread ioThread;
        boost::asio::io_service ioService_;
        std::auto_ptr<boost::asio::io_service::work> ioWork;

        int parseCommandLine(int argc, const char **argv);

    protected:
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
