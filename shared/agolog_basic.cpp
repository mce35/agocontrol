/**
 * Basic logging implementation, when Boost.Log is not available.
 */

#include <boost/thread/locks.hpp>
#include <syslog.h>
#include "agolog.h"

namespace agocontrol {
namespace log {

// TODO: Decide on levels. On FreeBSD at least, the LOG_INFO is normally
// not logged, only notice..
int sev2syslog[] = {
	LOG_DEBUG,	// trace
	LOG_INFO,	// debug
	LOG_NOTICE, // info 
	LOG_WARNING,
	LOG_ERR,
	LOG_CRIT
};

void console_sink::output_record(record &rec) {
	std::stringstream out;
	const char * lvl;
	if(rec.level < AGOLOG_NUM_SEVERITY_LEVELS)
		lvl = severity_level_str[rec.level];
	else
		lvl = "????";
			
	out
		<< boost::posix_time::to_simple_string(rec.timestamp)
		<< " [" << lvl << "] "
		<< rec.stream_.str()
		<< std::endl;

	boost::lock_guard<boost::mutex> lock(sink_mutex);
	std::cout << out.str();
}


syslog_sink::syslog_sink(std::string const &ident) {
	strncpy(this->ident, ident.c_str(), sizeof(this->ident));
	openlog(this->ident, LOG_PID, LOG_DAEMON);
}

void syslog_sink::output_record(record &rec) {
	syslog(sev2syslog[rec.level], "%s", rec.stream_.str().c_str());
}


record_pump::~record_pump()
{
	assert(rec.state == record::closed);
	logger.push_record(rec);
}


void log_container::init_default() {
	set_level(::agocontrol::log::debug);
	//get().set_sink( boost::shared_ptr<log_sink>(new syslog_sink("agotest")) );
}

void log_container::set_level(severity_level lvl) {
	get().set_level(lvl);
}


}
}
