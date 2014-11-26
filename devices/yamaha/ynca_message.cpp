#include <boost/asio.hpp> 
#include "yamaha_device.h"

#include "agolog.h"

namespace asio = boost::asio;

YNCAMessage::YNCAMessage(const std::string &unit, const std::string &function,
        YNCAReplyMode replyMode, const std::string &value)

    : unit_(unit)
    , function_(function)
    , value_(value)
    , replyMode_(replyMode)
{
    assert(replyMode_ != SkipNextIfSameValue || isQuery());
}

YNCAMessage::YNCAMessage(const std::string &unit, const std::string &function,
        const std::string &value)
    : unit_(unit)
    , function_(function)
    , value_(value)
    , replyMode_(Matching)
{
}


YNCAMessage::YNCAMessage(const std::string &unit, const std::string &function,
        float value)
    : unit_(unit)
    , function_(function)
{
    std::stringstream str;
    str << value;
    value_ = str.str();
}

typedef asio::buffers_iterator<boost::asio::streambuf::const_buffers_type> buffer_iter;

YNCAMessage::YNCAMessage(asio::streambuf &buf, size_t num_bytes) {
    asio::streambuf::const_buffers_type bufs = buf.data();

    buffer_iter begin = asio::buffers_begin(bufs);
    buffer_iter end = begin + num_bytes;

    /* Format: "@unit:func=value\r\n" */
    std::string msg (begin, end);
    if(num_bytes < 5 || *begin != '@' || *(end-2) != '\r' || *(end-1) != '\n') {
        AGO_WARNING() << "Trying to construct YNCAMessage from '" << msg << "'";
        throw new std::runtime_error("Invalid data");
    }

    //AGO_DEBUG() << "Parsing YNCAMessage msg " << msg;
    size_t pos=1,
           npos = msg.find(':', pos);

    unit_ = msg.substr(pos, npos-pos);

    // We may get @RESTRICTED only
    if(npos != std::string::npos) {
        pos = npos + 1;

        npos = msg.find('=', pos);
        function_ = msg.substr(pos, npos-pos);
        pos = npos + 1;

        npos = msg.find('\r', pos);
        value_ = msg.substr(pos, npos-pos);

        assert(npos == num_bytes - 2);
    }

    buf.consume(num_bytes);
}

size_t YNCAMessage::write(boost::asio::streambuf &streambuf) const {
    size_t length = unit_.size() + function_.size() + value_.size() + 5;
    asio::mutable_buffer b = streambuf.prepare(length);

    char *p = asio::buffer_cast<char*>(b);
    size_t of = 1;
    *p = '@';

    of+= unit_.copy(p+of, unit_.size());
    *(p+of++) = ':';

    of+= function_.copy(p+of, function_.size());
    *(p+of++) = '=';

    of+= value_.copy(p+of, value_.size());
    *(p+of++) = '\r';
    *(p+of++) = '\n';

    assert(of == length);

    streambuf.commit(length);

    return length;
}

bool YNCAMessage::isSameFunction(const YNCAMessage &other) const {
    return unit_ == other.unit_ && function_ == other.function_;
}

std::ostream& operator<< (std::ostream& os, const YNCAMessage &msg) {
    os << "@" << msg.unit_ << ":" << msg.function_ << "=" << msg.value_;
    return os;
}

