#include "agolog.h"

#ifdef HAVE_BOOST_LOG

#include <boost/log/expressions.hpp>

#else

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread/locks.hpp>
#include <syslog.h>
#endif

namespace agocontrol {
namespace log {

/* Static global accessor */
static AGO_LOGGER_IMPL default_inst;
AGO_LOGGER_IMPL & log_container::get() {
	return default_inst;
}

/* Implementation-specific below */
#ifndef HAVE_BOOST_LOG
const char* str_level[] = {
	"TRACE",
	"DEBUG",
	"INFO",
	"WARNING",
	"ERROR", 
	"FATAL"
};

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
	out
		<< boost::posix_time::to_simple_string(rec.timestamp)
		<< " [" << str_level[rec.level] << "] "
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
	default_inst.set_level(::agocontrol::log::debug);

	//default_inst.set_sink( boost::shared_ptr<log_sink>(new syslog_sink("agotest")) );
}


#else


void log_container::init_default() {
	boost::log::add_common_attributes();
	boost::log::core::get()->set_filter
	(
		boost::log::trivial::severity >= boost::log::trivial::info
	);
}

#endif


}
}
