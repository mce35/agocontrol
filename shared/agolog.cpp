#include <boost/date_time/posix_time/posix_time.hpp>
#include <stdexcept>

// for LOG_xxx defines
#include <syslog.h>

#include "agolog.h"

namespace agocontrol {
namespace log {

static AGO_LOGGER_IMPL default_inst;
typedef std::map<std::string, int> str_int_map;

static str_int_map syslog_facilities;
static std::vector<std::string> syslog_facility_names;
static std::vector<std::string> log_levels;

/* Static global accessor */
AGO_LOGGER_IMPL & log_container::get() {
    return default_inst;
}

void init_static() {
    if(log_levels.empty()) {
        // Same order as severity_level
        log_levels.push_back("TRACE");
        log_levels.push_back("DEBUG");
        log_levels.push_back("INFO");
        log_levels.push_back("WARNING");
        log_levels.push_back("ERROR");
        log_levels.push_back("FATAL");
        assert(log_levels.size() == AGOLOG_NUM_SEVERITY_LEVELS);
    }

    if(syslog_facilities.empty()) {
        syslog_facilities["auth"] = 		LOG_AUTH;
        syslog_facilities["authpriv"] =	LOG_AUTHPRIV;
#ifdef LOG_CONSOLE
        syslog_facilities["console"] =	LOG_CONSOLE;
#endif
        syslog_facilities["cron"] =		LOG_CRON;
        syslog_facilities["daemon"] =		LOG_DAEMON;
        syslog_facilities["ftp"] =			LOG_FTP;
        syslog_facilities["kern"] =		LOG_KERN;
        syslog_facilities["lpr"] =			LOG_LPR;
        syslog_facilities["mail"] =		LOG_MAIL;
        syslog_facilities["news"] =		LOG_NEWS;
#ifdef LOG_NTP
        syslog_facilities["ntp"] =			LOG_NTP;
#endif
#ifdef LOG_SECURITY
        syslog_facilities["security"] =	LOG_SECURITY;
#endif
        syslog_facilities["syslog"] =		LOG_SYSLOG;
        syslog_facilities["user"] =		LOG_USER;
        syslog_facilities["uucp"] =		LOG_UUCP;
        syslog_facilities["local0"] =		LOG_LOCAL0;
        syslog_facilities["local1"] =		LOG_LOCAL1;
        syslog_facilities["local2"] =		LOG_LOCAL2;
        syslog_facilities["local3"] =		LOG_LOCAL3;
        syslog_facilities["local4"] =		LOG_LOCAL4;
        syslog_facilities["local5"] =		LOG_LOCAL5;
        syslog_facilities["local6"] =		LOG_LOCAL6;
        syslog_facilities["local7"] =		LOG_LOCAL7;

        for(str_int_map::iterator i = syslog_facilities.begin(); i != syslog_facilities.end(); i++) {
            syslog_facility_names.push_back(i->first);
        }
    }
}

int log_container::getFacility(const std::string &facility) {
    init_static();

    str_int_map::iterator it;
    it = syslog_facilities.find(facility);
    if(it == syslog_facilities.end()) {
        return -1;
    }

    return it->second;
}

const std::vector<std::string>& log_container::getSyslogFacilities() {
    init_static();
    return syslog_facility_names;
}
severity_level log_container::getLevel(const std::string &level) {
    init_static();
    std::string ulevel(boost::to_upper_copy(level));

    for(size_t n=0; n < log_levels.size(); n++) {
        if(ulevel == log_levels[n]) {
            return static_cast<severity_level>(n);
        }
    }

    throw std::runtime_error("Invalid log level '" + level + "'");
}

const std::string& log_container::getLevel(severity_level level) {
    init_static();
    if(level >= log_levels.size()) {
        std::stringstream ss;
        ss << "Invalid log level " << level;
        throw std::runtime_error(ss.str());
    }

    return log_levels[level];
}

const std::vector<std::string>& log_container::getLevels() {
    init_static();
    return log_levels;
}

}/* namespace log */
}/* namespace agocontrol */
