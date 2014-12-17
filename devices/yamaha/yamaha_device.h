#ifndef YAMAHA_DEVICE_H
#define YAMAHA_DEVICE_H

#include <queue>
#include <boost/asio.hpp> 

namespace ip = boost::asio::ip;

class YNCAMessage {
public:
    enum YNCAReplyMode {
        /* Used when we expect a reply message with identical unit/function.
         * Note that we do not get replies for PUT operation if the value already
         * matches! */
        Matching,

        /* Used when we expect reply, for example @MAIN:BASIC=? returns multiple "real"
         * values but nothing named MAIN:BASIC */
        NoReply,

        /* Used when the next queued message shall be dropped,
         * if the reply value matches the value we was about to send */
        SkipNextIfSameValue
    };

private:
    std::string unit_;
    std::string function_;
    std::string value_;
    YNCAReplyMode replyMode_;

public:

    YNCAMessage(const std::string &unit, const std::string &function, YNCAReplyMode replyMode, const std::string &value = "?");
    YNCAMessage(const std::string &unit, const std::string &function, const std::string &value = "?");
    YNCAMessage(const std::string &unit, const std::string &function, float value);
    YNCAMessage(boost::asio::streambuf &streambuf, size_t num_bytes);

    const std::string& unit() const { return unit_; };
    const std::string& function() const { return function_; };
    const std::string& value() const { return value_; };

    bool isQuery() const { return value_ == "?"; }

    YNCAMessage asPreQuery() const {
        return YNCAMessage(unit_, function_, SkipNextIfSameValue);
    }

    YNCAReplyMode replyMode() { return replyMode_; };
    bool isSameFunction(const YNCAMessage &other) const;

    size_t write(boost::asio::streambuf &streambuf) const;

    friend std::ostream& operator<< (std::ostream& os, const YNCAMessage &list) ;
};

class YamahaDeviceBase {
private:
    ip::tcp::endpoint deviceEndpoint;
    boost::asio::io_service &io_service;
    boost::asio::deadline_timer timer;
    bool do_exit;
    bool connected;

    boost::shared_ptr<ip::tcp::socket> socket;

    boost::asio::streambuf sendBuf;
    boost::asio::streambuf recvBuf;

    std::queue<YNCAMessage> sendQueue;
    boost::asio::deadline_timer sendTimer;
    enum {
        IDLE,
        SENDING,
        SENT,
        RECEIVED
    } sendState;

    void processSendQueue();
    void maybeSetIdle();
    void read();
    void connect_();
    void disconnect();
    void reconnect();
    void shutdown_();

    void onTimeout(const boost::system::error_code& error);
    void onConnect(const boost::system::error_code& error);
    void onWrite(const boost::system::error_code& error, size_t bytes_transferred);
    void onSendTimer(const boost::system::error_code& error);
    void onRead(const boost::system::error_code& error, size_t bytes_transferred);

    virtual void onCommandReply(const YNCAMessage &req, const YNCAMessage &rep);
    virtual void onAutoFeedback(const YNCAMessage &msg);

public:
    YamahaDeviceBase(boost::asio::io_service &io_service, const ip::address &addr, unsigned short port = 50000);

    const ip::tcp::endpoint endpoint() const;

    /* Explicitly connect */
    void connect();

    /* Shutdown and invalidate the client */
    void shutdown();

    /* Send a GET or PUT message. If the PUT message has an explicit value,
     * you MUST use queryAndSend instead!
     *
     * Non-explicit PUTs (such as @MAIN:VOL=Up) should be sent here however.
     */
    void send(const YNCAMessage &msg);

    /* Send a PUT message, but prepend with a GET making the PUT optional */
    void queryAndSend(const YNCAMessage &msg);
};

class YamahaDevice : public YamahaDeviceBase {
private:
    std::map<std::string, std::string> data;

    std::string dataKey(const YNCAMessage &msg);

public:
    YamahaDevice(boost::asio::io_service &io_service, const ip::address &addr, unsigned short port = 50000);

    void powerOn();
    void powerOff();
    void mute();
    void unmute();
    void muteToggle();
    void volIncr();
    void volDecr();
    void volSet(float level);

    void onCommandReply(const YNCAMessage &req, const YNCAMessage &rep);
    void onAutoFeedback(const YNCAMessage &msg);
};


#endif
