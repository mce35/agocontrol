/**
 * Boost.Log based logging code, this mainly contains functions to set logging
 * level and sinks.
 */

#define BOOST_LOG_USE_NATIVE_SYSLOG

#include <boost/shared_ptr.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/log/sinks.hpp>
#include <boost/log/sinks/syslog_backend.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/attributes.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/support/date_time.hpp>

#include <boost/utility/empty_deleter.hpp>

#include "agolog.h"

namespace agocontrol {
namespace log {

static bool inited = false;
static std::string syslog_ident;

namespace logging = boost::log;
namespace sinks = boost::log::sinks;
namespace keywords = boost::log::keywords;
namespace expr = boost::log::expressions;
namespace attrs = boost::log::attributes;


BOOST_LOG_ATTRIBUTE_KEYWORD(severity, "Severity", severity_level);

// The formatting logic for the severities
template< typename CharT, typename TraitsT >
inline std::basic_ostream< CharT, TraitsT >& operator<< (
        std::basic_ostream< CharT, TraitsT >& strm,
        severity_level lvl)
{
    strm << log_container::getLevel(lvl);
    return strm;
}


void log_container::initDefault() {
    if(inited)
        return;
    inited = true;

    logging::add_common_attributes();

    setOutputConsole();
    setCurrentLevel(AGO_DEFAULT_LEVEL);
}

void log_container::setCurrentLevel(severity_level lvl) {
    boost::log::core::get()->set_filter
        (
         ::agocontrol::log::severity >= lvl
        );
}


void log_container::setOutputConsole() {
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
         <<" [" << std::setw(7) << severity << std::setw(0) << "] "
         << expr::smessage
        );

    core->remove_all_sinks();
    core->add_sink(sink);
}

void log_container::setOutputSyslog(const std::string &ident, int facility) {
    boost::shared_ptr< logging::core > core = logging::core::get();

    // The ident string passed into the syslog_backend
    // must be keept allocated. syslog_ident is static
    syslog_ident = ident;

    // Create a backend and attach a couple of streams to it
    boost::shared_ptr< sinks::syslog_backend > backend(
            new sinks::syslog_backend(
                keywords::facility = sinks::syslog::make_facility(facility),
                keywords::ident = syslog_ident,
                keywords::use_impl = sinks::syslog::native
                )
            );

    sinks::syslog::custom_severity_mapping <severity_level> mapping("Severity");
    mapping[trace] = sinks::syslog::debug;
    mapping[debug] = sinks::syslog::debug;
    mapping[info] = sinks::syslog::info;
    mapping[warning] = sinks::syslog::warning;
    mapping[error] = sinks::syslog::error;
    mapping[fatal] = sinks::syslog::critical;

    // Wrap it into the frontend and register in the core.
    // The backend requires synchronization in the frontend.
    typedef sinks::synchronous_sink< sinks::syslog_backend > sink_t;
    boost::shared_ptr< sink_t > sink(new sink_t(backend));

    /*
       sink->set_formatter
       (
       expr::stream
    //			<< expr::format_date_time< boost::posix_time::ptime >("TimeStamp", "%Y-%m-%d %H:%M:%S.%f")
    //			<< " [" << expr::attr< attrs::current_thread_id::value_type > ("ThreadID") << "] "
    << " [" << agocontrol::log::severity << "] "
    << expr::smessage
    );
    */

    core->remove_all_sinks();
    core->add_sink(sink);
}

}/* namespace log */
}/* namespace agocontrol */
