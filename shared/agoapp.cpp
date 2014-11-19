#include <signal.h>
#include <stdexcept>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/algorithm/string/join.hpp>



#include "agoapp.h"

#include "agoclient.h"
#include "options_validator.hpp"

using namespace qpid::types;
using namespace qpid::messaging;

using namespace agocontrol::log;
using namespace boost_options_validator;
namespace po = boost::program_options;
namespace fs = boost::filesystem;

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

    boost::to_lower(appShortName);

    // Default to same config section as shortname, but this may be custom
    // if the original app did not use something "matching"
    appConfigSection = appShortName;
}

int AgoApp::parseCommandLine(int argc, const char **argv) {
    po::options_description options;
    options.add_options()
        ("help,h", "produce this help message")
        ("log-level", po::value<std::string>(),
         (std::string("Log level. Valid values are one of\n ") +
          boost::algorithm::join(log_container::getLevels(), ", ")).c_str())
        ("log-method", po::value<std::string>(),
         "Where to log. Valid values are one of\n console, syslog")
        ("log-syslog-facility", po::value<std::string>(),
         (std::string("Which syslog facility to log to. Valid values are on of\n ") +
          boost::algorithm::join(log_container::getSyslogFacilities(), ", ")).c_str())
        ("debug,d", po::bool_switch(),
         "Shortcut to set console logging with level DEBUG")
        ("trace,t", po::bool_switch(),
         "Shortcut to set console logging with level TRACE")
        ("config-dir", po::value<std::string>(),
         "Directory with configuration files")
        ("state-dir", po::value<std::string>(),
         "Directory with local state files")
        ;

    appCmdLineOptions(options);
    try {
        po::variables_map &vm (cli_vars);
        po::store(po::parse_command_line(argc, argv, options), vm);
        po::notify(vm);

        if(vm.count("config-dir")) {
            std::string dir = vm["config-dir"].as<std::string>();
            try {
                AgoClientInternal::setConfigDir(dir);
            } catch(const fs::filesystem_error& error) {
                std::cout << "Could not use " << dir << " as config-dir: "
                    << error.code().message()
                    << std::endl;
                return 1;
            }
        }

        if(vm.count("state-dir")) {
            std::string dir = vm["state-dir"].as<std::string>();
            try {
                AgoClientInternal::setLocalStateDir(dir);
            } catch(const fs::filesystem_error& error) {
                std::cout << "Could not use " << dir << " as state-dir: "
                    << error.code().message()
                    << std::endl;
                return 1;
            }
        }

        // Init dirs before anything else, so we at least show correct in help output
        if (vm.count("help")) {
            initDirectorys();
            std::cout << "usage: " << argv[0] << std::endl
                << options << std::endl
                << std::endl
                << "Paths:" << std::endl
                << "  Default config dir: "
                <<  BOOST_PP_STRINGIZE(DEFAULT_CONFDIR) << std::endl
                << "  Default state dir : "
                <<  BOOST_PP_STRINGIZE(DEFAULT_LOCALSTATEDIR) << std::endl
                << "  Active config dir : "
                << getConfigPath().string() << std::endl
                << "  Active state dir  : "
                << getLocalStatePath().string() << std::endl
                << std::endl
                << "System configuration file      : "
                << getConfigPath("conf.d/system.conf").string() << std::endl
                << "App-specific configuration file: "
                << getConfigPath("conf.d/" + appShortName + ".conf").string() << std::endl
                ;
            return 1;
        }

        options_validator<std::string>(vm, "log-method")
            ("console")("syslog")
            .validate();

        options_validator<std::string>(vm, "log-syslog-facility")
            (log_container::getSyslogFacilities())
            .validate();

        options_validator<std::string>(vm, "log-level")
            (log_container::getLevels())
            .validate();
    }
    catch(std::exception& e)
    {
        std::cout << "Failed to parse command line: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}


void AgoApp::setup() {
    setupLogging();
    setupAgoConnection();
    setupSignals();
    setupApp();
    setupIoThread();
}

void AgoApp::cleanup() {
    cleanupApp();
    cleanupAgoConnection();
    cleanupIoThread();

    // Global signal mapper
    app_instance = NULL;
}

void AgoApp::setupLogging() {
    log_container::initDefault();

    std::string level_str;
    if(cli_vars["debug"].as<bool>())
        level_str = "DEBUG";
    else if(cli_vars["trace"].as<bool>())
        level_str = "TRACE";
    else if(cli_vars.count("log-level"))
        level_str = cli_vars["log-level"].as<std::string>();
    else
        level_str = getConfigOption(appShortName.c_str(), "system", "log_level", "info");

    try {
        severity_level level = log_container::getLevel(level_str);
        log_container::setCurrentLevel(level);
    }catch(std::runtime_error &e) {
        throw ConfigurationError(e.what());
    }

    std::string method;
    if(cli_vars["debug"].as<bool>() || cli_vars["trace"].as<bool>())
        method = "console";
    else if(cli_vars.count("log-method"))
        method = cli_vars["log-method"].as<std::string>();
    else
        method = getConfigOption(appShortName.c_str(), "system", "log_method", "console");

    if(method == "syslog") {
        std::string facility_str;
        if(cli_vars.count("log-syslog-facility"))
            facility_str = cli_vars["log-syslog-facility"].as<std::string>();
        else
            facility_str = getConfigOption(appShortName.c_str(), "system", "syslog_facility", "local0");

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
}

void AgoApp::cleanupAgoConnection() {
    if(agoConnection == NULL)
        return;

    delete agoConnection;
    agoConnection = NULL;
}

void AgoApp::addCommandHandler() {
    agoConnection->addHandler(boost::bind(&AgoApp::commandHandler, this, _1));
}

void AgoApp::addEventHandler() {
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



boost::asio::io_service& AgoApp::ioService() {
    if(!ioWork.get()) {
        // Enqueue work on the IO service to indicate we want it running
        ioWork = std::auto_ptr<boost::asio::io_service::work>(
                new boost::asio::io_service::work(ioService_)
                );
    }

    return ioService_;
}

void AgoApp::setupIoThread() {
    if(!ioWork.get()) {
        // No work job enqueued, skipping
        AGO_TRACE() << "Skipping IO thread, nothing scheduled";
        return;
    }

    // This thread will run until we're out of work.
    AGO_TRACE() << "Starting IO thread";
    ioThread = boost::thread(boost::bind(&boost::asio::io_service::run, &ioService_));
}

void AgoApp::cleanupIoThread(){
    if(!ioThread.joinable()) {
        AGO_TRACE() << "No IO thread alive";
        return;
    }

    AGO_TRACE() << "Resetting work & joining IO thread, waiting for it to exit";
    ioWork.reset();
    ioThread.join();
    AGO_TRACE() << "IO thread dead";
}




int AgoApp::appMain() {
    agoConnection->run();
    return 0;
}

int AgoApp::main(int argc, const char **argv) {
    try {
        if(parseCommandLine(argc, argv) != 0)
            return 1;
        setup();
    }catch(StartupError &e) {
        cleanup();
        return 1;
    }catch(ConfigurationError &e) {
        std::cerr << "Failed to start " << appName
            << " due to configuration error: "
            << e.what()
            << std::endl;

        cleanup();
        return 1;
    }

    AGO_INFO() << "Starting " << appName;

    int ret = this->appMain();

    AGO_DEBUG() << "Shutting down " << appName;

    cleanup();

    if(ret == 0)
        AGO_INFO() << "Exiting " << appName << "(code " << ret << ")";
    else
        AGO_WARNING() << "Exiting " << appName << "(code " << ret << ")";

    return ret;
}


}/*namespace agocontrol */
