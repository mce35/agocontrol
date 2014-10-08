#ifndef AGOLOG_H
#define AGOLOG_H
/* Defines the following macros:
 *
 * AGO_TRACE
 * AGO_DEBUG
 * AGO_INFO
 * AGO_WARNING
 * AGO_ERROR
 * AGO_FATAL
 *
 * which shall be used as 
 * 	AGO_XXX() << "some logging data";
 *
 * If Boost.Log is available, that is used.
 * Else, we fall back to a simple hacky local
 * implementation.
 */

#define AGO_LOG_INSTANCE_NAME _default_logger_

#ifdef HAVE_BOOST_LOG
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/sources/severity_logger.hpp>

#define AGO_TRACE() BOOST_LOG_SEV(AGO_LOG_INSTANCE_NAME, boost::log::trivial::trace)
#define AGO_DEBUG() BOOST_LOG_SEV(AGO_LOG_INSTANCE_NAME, boost::log::trivial::debug)
#define AGO_INFO() BOOST_LOG_SEV(AGO_LOG_INSTANCE_NAME, boost::log::trivial::info)
#define AGO_WARNING() BOOST_LOG_SEV(AGO_LOG_INSTANCE_NAME, boost::log::trivial::warning)
#define AGO_ERROR() BOOST_LOG_SEV(AGO_LOG_INSTANCE_NAME, boost::log::trivial::error)
#define AGO_FATAL() BOOST_LOG_SEV(AGO_LOG_INSTANCE_NAME, boost::log::trivial::fatal)

#define AGO_LOGGER_IMPL  boost::log::sources::severity_logger_mt<boost::log::trivial::severity_level>

#else

/* Own hacky log solution.
 * This is inspired partly by the Boost implementation, at least for the
 * record stuff.
 */

#include <ostream>
#include <sstream>
#include <boost/date_time/posix_time/posix_time.hpp>

namespace agocontrol {
	namespace log {
		enum severity_level
		{
			trace,
			debug,
			info,
			warning,
			error,
			fatal
		};

		class record {
		private: 
			int state; // 0 = invalid, 1 = ready, 2 = used
			severity_level level;
			boost::posix_time::ptime timestamp;
			typedef std::stringstream stream_type;
			stream_type stream_;
		public:
			// Invalid record
			record()
				: state(0)
		  		, level(fatal)
			{}

			// Valid record
			record(severity_level level_)
				: state(1)
			   , level(level_)
				, timestamp(boost::posix_time::second_clock::local_time())
			{}

			record(const record &rec_)
				: state(rec_.state)
			   , level(rec_.level)
				, timestamp(rec_.timestamp)
			{}

			~record();

			/* indicates if we are valid for use or not */
			bool operator! () const {
				return state != 1;
			}

			std::stringstream & stream() 
			{
				// Only valid for one stream() operation
				state = 2;
				return stream_;
			}
		};

		class simple_logger {
		private:
			severity_level current_level;
		public:
			simple_logger()
				: current_level(debug) {}

			simple_logger(severity_level severity)
				: current_level(severity) {}

			void set_level(severity_level severity) {
				current_level = severity;
			}

			record open_record(severity_level severity) {
				if(severity > current_level) {
					return record(severity);
				}
				return record(); // Invalid record
			}
		};
	}
}

#define AGO_LOG_SEV(logger, severity) \
	for (::agocontrol::log::record rec_var = (logger).open_record(severity); !!rec_var;)\
		rec_var.stream()


#define AGO_TRACE() AGO_LOG_SEV(AGO_LOG_INSTANCE_NAME, ::agocontrol::log::trace)
#define AGO_DEBUG() AGO_LOG_SEV(AGO_LOG_INSTANCE_NAME, ::agocontrol::log::debug)
#define AGO_INFO() AGO_LOG_SEV(AGO_LOG_INSTANCE_NAME, ::agocontrol::log::info)
#define AGO_WARNING() AGO_LOG_SEV(AGO_LOG_INSTANCE_NAME, ::agocontrol::log::warning)
#define AGO_ERROR() AGO_LOG_SEV(AGO_LOG_INSTANCE_NAME, ::agocontrol::log::error)
#define AGO_FATAL() AGO_LOG_SEV(AGO_LOG_INSTANCE_NAME, ::agocontrol::log::fatal)

#define AGO_LOGGER_IMPL agocontrol::log::simple_logger

#endif


#endif
