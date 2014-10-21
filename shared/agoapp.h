#ifndef AGOAPP_H
#define AGOAPP_H

#include "agoclient.h"

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
#define AGOAPP_CONSTRUCTOR(class_name) class_name() : AgoApp(#class_name) {};

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

		void signalExit();

	protected:
		AgoConnection *agoConnection;

		/* Setup will be called prior to appMain being called. This
		 * normally sets up logging, an AgoConnection, signal handling,
		 * and finally any app specific setup (setupApp).
		 */
		virtual void setup();
		void setupLogging();
		void setupAgoConnection();
		void setupSignals();

		/* App specific init can be done in this */
		virtual void setupApp() { };

		/* After appMain has returned, we will call cleanup to release
		 * any resources */
		void cleanup();
		void cleanupAgoConnection();
		virtual void cleanupApp() { };

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
		~AgoApp();

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
}

#endif
