#include "agolog.h"

#include <boost/date_time/posix_time/posix_time.hpp>
#ifdef HAVE_BOOST_LOG
#else
#endif

namespace agocontrol {
namespace log {

static AGO_LOGGER_IMPL default_inst;

/* Static global accessor */
AGO_LOGGER_IMPL & log_container::get() {
	return default_inst;
}

const char* const severity_level_str[] = {
	"TRACE",
	"DEBUG",
	"INFO",
	"WARNING",
	"ERROR", 
	"FATAL"
};

}
}
