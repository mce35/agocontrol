#include <signal.h>
#include <stdexcept>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/thread.hpp>

#include "agoapp.h"

#include "agoclient.h"

using namespace qpid::types;
using namespace qpid::messaging;

using namespace agocontrol::log;

namespace agocontrol {
/* Single instance of AgoApp which gets info on signals */
static AgoApp *app_instance;
static void static_sighandler(int signo) {
	if(app_instance != NULL)
		app_instance->sighandler(signo);
}



AgoApp::AgoApp(const char *appName_)
	: appName(appName_)
	, exit_signaled(false)
	, agoConnection(NULL)
{
	// Keep a "short" name too, trim leading 'Ago' from app name
	// This will be used for getConfigOption section names
	if(appName.find("Ago") == 0)
		appShortName = appName.substr(3);
	else
		appShortName = appName;

}

AgoApp::~AgoApp() {
	cleanup();
}

void AgoApp::setup() {
	setupLogging();
	setupAgoConnection();
	setupSignals();
	setupApp();
}

void AgoApp::setupLogging() {
	log_container::initDefault();

	std::string level_str = getConfigOption(appShortName.c_str(), "system", "log_level", "info");
	try {
		severity_level level = log_container::getLevel(level_str);
		log_container::setLevel(level);
	}catch(std::runtime_error &e) {
		throw ConfigurationError(e.what());
	}

	std::string method = getConfigOption(appShortName.c_str(), "system", "log_method", "console");

	if(method == "syslog") {
		const std::string facility_str = getConfigOption(appShortName.c_str(), "system", "syslog_facility", "local0");
		int facility = log_container::getFacility(facility_str);
		if(facility == -1) {
			throw ConfigurationError("Bad syslog facility '" + facility_str + "'");
		}

		log_container::setOutputSyslog(boost::to_lower_copy(appName), facility);
		AGO_DEBUG() << "Using syslog facility " << facility_str << ", level " << level_str;
	}
	else if(method == "console") {
		AGO_DEBUG() << "Using console log, level " << level_str;
	}
	else {
		throw ConfigurationError("Bad log_method '" + method + "'");
	}
}

void AgoApp::setupAgoConnection() {
	agoConnection = new AgoConnection(appShortName.c_str());
	agoConnection->addHandler(boost::bind(&AgoApp::commandHandler, this, _1));
	agoConnection->addEventHandler(boost::bind(&AgoApp::eventHandler, this, _1, _2));
}


void AgoApp::setupSignals() {
	struct sigaction sa;
	sa.sa_handler = static_sighandler;
	sa.sa_flags = 0;
//	sa.sa_flags = SA_RESTART;

	sigfillset(&sa.sa_mask);

	// static_sighandler calls global
	// static app_instance->sighandler
	app_instance = this;

	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGQUIT, &sa, NULL);
}

void AgoApp::sighandler(int signo) {
	switch(signo) {
	case SIGINT:
	case SIGQUIT:
		AGO_DEBUG() << "Exit signal catched, shutting down";
		signalExit();
		break;

	default:
		AGO_WARNING() << "Unmapped signal " << signo << " received";
	}
}

void AgoApp::signalExit() {
	exit_signaled = true;

	// Dispatch signalExitThr call in separate thread, for application
	// to do more serious shutdown procedures
	boost::thread t(boost::bind(&AgoApp::doShutdown, this));
	t.detach();
}

void AgoApp::doShutdown() {
	try {
		if(agoConnection)
			agoConnection->shutdown();

	} catch(std::exception &e) {
	}
}

void AgoApp::cleanup() {
	cleanupAgoConnection();
	cleanupApp();

	// Global signal mapper
	app_instance = NULL;
}

void AgoApp::cleanupAgoConnection() {
	if(agoConnection == NULL)
		return;

	delete agoConnection;
	agoConnection = NULL;
}

int AgoApp::main(int argc, const char **argv) {
	try {
		setup();
	}catch(StartupError &e) {
		return 1;
	}catch(ConfigurationError &e) {
		std::cerr << "Failed to start " << appName
			<< " due to configuration error: "
			<< e.what()
			<< std::endl;

		return 1;
	}

	AGO_INFO() << "Starting " << appName;

	int ret = this->appMain();

	if(ret == 0)
		AGO_INFO() << "Exiting " << appName << "(code " << ret << ")";
	else
		AGO_WARNING() << "Exiting " << appName << "(code " << ret << ")";

	return ret;
}


}
