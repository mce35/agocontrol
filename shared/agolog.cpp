#include "agolog.h"

#include <boost/date_time/posix_time/posix_time.hpp>

namespace agocontrol {
namespace log {

const char* str_level[] = {
	"TRACE",
	"DEBUG",
	"INFO",
	"WARNING",
	"ERROR", 
	"FATAL"
};


record::~record(){
	if(state != 2)
		return;

	// TODO: Custom output target
	// TODO: output mutex
	std::cout
		<< boost::posix_time::to_simple_string(timestamp)
		<< " [" << str_level[level] << "] "
		<< stream_.str()
		<< std::endl;
}

}
}
