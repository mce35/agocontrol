#ifndef AGOLOG_H
#define AGOLOG_H
#include <vector>

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

#define AGO_DEFAULT_LEVEL ::agocontrol::log::info

#ifdef HAVE_BOOST_LOG
# include <agolog_boost.h>
#else
# include "agolog_basic.h"
#endif

// Global static singleton for obtaining default logger instance,
// and initing the logger.
#define AGO_GET_LOGGER (::agocontrol::log::log_container::get())

// It is not safe to pass a null char* to a stringstream,
// use this macro to replace null char* with "(null)"
#define LOG_STR_OR_NULL(v)  (v != NULL ? v : "(null)")


namespace agocontrol {
namespace log {
class log_container {
public:
    static AGO_LOGGER_IMPL &get();
    /**
     * Inits the logger with default settings (log level INFO, console output)
     */
    static void initDefault();

    /**
     * Changes the severity setting of the logger
     */
    static void setCurrentLevel(severity_level lvl);

    /**
     * Changes to console output
     */
    static void setOutputConsole();

    /**
     * Changes to Syslog output.
     */
    static void setOutputSyslog(const std::string &ident, int facility);

    static int getFacility(const std::string &facility);

    /* Returns a vector of all supported syslog facitliy names */
    static const std::vector<std::string>& getSyslogFacilities() ;

    /* Translate level string to internal level. Returns -1 on invalid value */
    static severity_level getLevel(const std::string &level);

    /* Translate internal level to string. Throws exception on invalid value */
    static const std::string & getLevel(severity_level lvl);

    /* Returns a vector with all levels, indexed identical to severity_level */
    static const std::vector<std::string> & getLevels();

};

} /* namespace log */
} /* namespace agocontrol */

#endif
