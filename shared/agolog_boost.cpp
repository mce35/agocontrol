/**
 * Boost.Log based logging code, this mainly contains functions to set logging
 * level and sinks.
 */

#include <boost/shared_ptr.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/log/sinks.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/attributes.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/support/date_time.hpp>

#include <boost/utility/empty_deleter.hpp>

#include "agolog.h"

namespace agocontrol {
namespace log {

namespace logging = boost::log;
namespace sinks = boost::log::sinks;
namespace expr = boost::log::expressions;
namespace attrs = boost::log::attributes;


BOOST_LOG_ATTRIBUTE_KEYWORD(severity, "Severity", severity_level);

// The formatting logic for the severities
template< typename CharT, typename TraitsT >
inline std::basic_ostream< CharT, TraitsT >& operator<< (
		std::basic_ostream< CharT, TraitsT >& strm,
		severity_level lvl)
{
	if (static_cast< std::size_t >(lvl) < AGOLOG_NUM_SEVERITY_LEVELS)
		strm << severity_level_str[lvl];
	else
		strm << static_cast< int >(lvl);

	return strm;
}


void log_container::init_default() {
	logging::add_common_attributes();
	set_level(agocontrol::log::debug);

	boost::shared_ptr< logging::core > core = logging::core::get();

	// Create a backend and attach a couple of streams to it
	boost::shared_ptr< sinks::text_ostream_backend > backend =
		boost::make_shared< sinks::text_ostream_backend >();

	backend->add_stream(
			boost::shared_ptr< std::ostream >(&std::clog, boost::empty_deleter())
			);

	// Enable auto-flushing after each log record written
	backend->auto_flush(true);

	// Wrap it into the frontend and register in the core.
	// The backend requires synchronization in the frontend.
	typedef sinks::synchronous_sink< sinks::text_ostream_backend > sink_t;
	boost::shared_ptr< sink_t > sink(new sink_t(backend));

	sink->set_formatter
		(
			expr::stream
				<< expr::format_date_time< boost::posix_time::ptime >("TimeStamp", "%Y-%m-%d %H:%M:%S.%f")
				<< " [" << expr::attr< attrs::current_thread_id::value_type > ("ThreadID") << "] "
				<< " [" << agocontrol::log::severity << "] "
				<< expr::smessage
		);

	core->add_sink(sink);
}

void log_container::set_level(severity_level lvl) {
	boost::log::core::get()->set_filter
	(
		agocontrol::log::severity >= lvl
	);
}

}
}
