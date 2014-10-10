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

#ifdef HAVE_BOOST_LOG
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/sources/severity_logger.hpp>

#define AGO_TRACE() BOOST_LOG_SEV(AGO_GET_LOGGER, boost::log::trivial::trace)
#define AGO_DEBUG() BOOST_LOG_SEV(AGO_GET_LOGGER, boost::log::trivial::debug)
#define AGO_INFO() BOOST_LOG_SEV(AGO_GET_LOGGER, boost::log::trivial::info)
#define AGO_WARNING() BOOST_LOG_SEV(AGO_GET_LOGGER, boost::log::trivial::warning)
#define AGO_ERROR() BOOST_LOG_SEV(AGO_GET_LOGGER, boost::log::trivial::error)
#define AGO_FATAL() BOOST_LOG_SEV(AGO_GET_LOGGER, boost::log::trivial::fatal)

#define AGO_LOGGER_IMPL  boost::log::sources::severity_logger_mt<boost::log::trivial::severity_level>

#else

/* Own hacky log solution.
 * This is inspired partly by the Boost implementation, at least for the
 * record stuff.
 */

#include <ostream>
#include <sstream>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/shared_ptr.hpp>

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

		class simple_logger;
		class record {
		friend class simple_logger;
		friend class record_pump;
		friend class console_sink;
		friend class syslog_sink;

		private: 
			enum record_state { invalid, open, closed };
			record_state state;
			severity_level level;
			boost::posix_time::ptime timestamp;

			typedef std::stringstream stream_type;
			stream_type stream_;

		public:
			// Invalid record
			record()
				: state(invalid)
		  		, level(fatal)
			{}

			// Valid record
			record(severity_level level_)
				: state(open)
			   , level(level_)
				, timestamp(boost::posix_time::second_clock::local_time())
			{}

			record(const record &rec_)
				: state(rec_.state)
			   , level(rec_.level)
				, timestamp(rec_.timestamp)
			{}

			/* indicates if we are valid for use or not */
			bool operator! () const {
				return state != open;
			}
		};

		class record_pump {
		private:
			simple_logger &logger;
			record &rec;
		public:
			record_pump(simple_logger &logger_, record &rec_)
				: logger(logger_)
				, rec(rec_)
			{}

			~record_pump();

			std::stringstream & stream() 
			{
				// Only valid for one stream() operation
				assert(rec.state == record::open);
				rec.state = record::closed;
				return rec.stream_;
			}
		};

		class log_sink {
		public:
			log_sink(){}
			virtual void output_record(record &rec) = 0;
			virtual ~log_sink() {}
		};

		class console_sink: public log_sink {
		private:
			boost::mutex sink_mutex;

		public:
			console_sink(){}
			void output_record(record &rec);
		};

		class syslog_sink: public log_sink {
		private:
			boost::mutex sink_mutex;
			char ident[255];
		public:
			syslog_sink(std::string const &ident);
			void output_record(record &rec);
		};

		class simple_logger {
		friend class record_pump;

		private:
			severity_level current_level;
			boost::shared_ptr<log_sink> sink;

		protected:
			void push_record(record &rec) {
				sink->output_record(rec);
			}

		public:
			simple_logger()
				: current_level(debug)
				, sink( boost::shared_ptr<log_sink>(new console_sink()) )
			{}

			simple_logger(severity_level severity)
				: current_level(severity) {}

			void set_level(severity_level severity) {
				current_level = severity;
			}

			void set_sink(boost::shared_ptr<log_sink> sink_) {
				sink = sink_;
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

#define AGO_LOGGER_IMPL agocontrol::log::simple_logger

#define AGO_LOG_SEV(logger, severity) \
	for (::agocontrol::log::record rec_var = (logger).open_record(severity); !!rec_var;)\
		::agocontrol::log::record_pump((logger), rec_var).stream()

#define AGO_TRACE() AGO_LOG_SEV(AGO_GET_LOGGER, ::agocontrol::log::trace)
#define AGO_DEBUG() AGO_LOG_SEV(AGO_GET_LOGGER, ::agocontrol::log::debug)
#define AGO_INFO() AGO_LOG_SEV(AGO_GET_LOGGER, ::agocontrol::log::info)
#define AGO_WARNING() AGO_LOG_SEV(AGO_GET_LOGGER, ::agocontrol::log::warning)
#define AGO_ERROR() AGO_LOG_SEV(AGO_GET_LOGGER, ::agocontrol::log::error)
#define AGO_FATAL() AGO_LOG_SEV(AGO_GET_LOGGER, ::agocontrol::log::fatal)

#endif

#define AGO_GET_LOGGER (::agocontrol::log::log_container::get())
namespace agocontrol {
	namespace log {
		class log_container {
		public:
			static AGO_LOGGER_IMPL &get();
			static void init_default();
		};
	}
}

#endif
