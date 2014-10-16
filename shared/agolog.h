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
 *
 * Else, we fall back to a simple local implementation.
 *
 * In either case, the log_container interface should be used to
 * control the actual implementation.
 */
/* Severity level definition is the same for both implementations */
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
#define AGOLOG_NUM_SEVERITY_LEVELS 6
	}
}

#ifdef HAVE_BOOST_LOG
# include <agolog_boost.h>
#else
# include "agolog_basic.h"
#endif

namespace agocontrol {
	namespace log {
		extern const char* const severity_level_str[];

// Global static singleton for obtaining default logger instance,
// and initing the logger.
#define AGO_GET_LOGGER (::agocontrol::log::log_container::get())

		class log_container {
		public:
			static AGO_LOGGER_IMPL &get();
			static void init_default();
			static void set_level(severity_level lvl);
		};
	}
}

#endif
