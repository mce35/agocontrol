#include <boost/bind.hpp>
#include <boost/make_shared.hpp>

#include "agolog.h"
#include "yamaha_device.h"

namespace asio = boost::asio;

YamahaDeviceBase::YamahaDeviceBase(asio::io_service &io_service,
        const ip::address &addr, unsigned short port)
    : io_service(io_service)
    , timer(io_service)
    , do_exit(false)
    , connected(false)
    , sendTimer(io_service)
    , sendState(IDLE)
{
    deviceEndpoint = ip::tcp::endpoint(addr, port);
}

void YamahaDeviceBase::send(const YNCAMessage &msg) {
    if(!sendQueue.empty()) {
        // If previous msg was SkipNextIfSameValue, the next must be identical
        assert(sendQueue.back().replyMode() != YNCAMessage::SkipNextIfSameValue ||
            (msg.isSameFunction(sendQueue.back()) && !msg.isQuery()));
    }
    sendQueue.push(msg);
    io_service.post(boost::bind(&YamahaDeviceBase::processSendQueue, this));
}

void YamahaDeviceBase::queryAndSend(const YNCAMessage &msg) {
    assert(!msg.isQuery());
    send(msg.asPreQuery());
    send(msg);
}

void YamahaDeviceBase::connect() {
    io_service.post(boost::bind(&YamahaDeviceBase::connect_, this));
}

void YamahaDeviceBase::shutdown() {
    io_service.post(boost::bind(&YamahaDeviceBase::shutdown_, this));
}

const ip::tcp::endpoint YamahaDeviceBase::endpoint() const {
    return deviceEndpoint;
}








void YamahaDeviceBase::connect_() {
    AGO_DEBUG() << "[" << deviceEndpoint << "] Connecting";

    timer.cancel();
    sendTimer.cancel();
    sendState = IDLE;
    sendBuf.consume(sendBuf.size());
    recvBuf.consume(recvBuf.size());

    socket = boost::make_shared<ip::tcp::socket>(boost::ref(io_service));

    socket->async_connect(
            deviceEndpoint,
            boost::bind(&YamahaDeviceBase::onConnect, this, _1));
    timer.expires_from_now(boost::posix_time::seconds(5));
    timer.async_wait(boost::bind(&YamahaDeviceBase::onTimeout, this, _1));
}

void YamahaDeviceBase::disconnect() {
    AGO_DEBUG() << "[" << deviceEndpoint << "] Disconnecting";

    connected = false;
    if(socket.get()) {
        boost::system::error_code ec;
        // Ignore errors
        socket->shutdown(ip::tcp::socket::shutdown_both, ec);
        socket->close(ec);
        socket.reset();
    }

    timer.cancel();
    sendTimer.cancel();
}

void YamahaDeviceBase::reconnect() {
    if(do_exit)
        return;

    AGO_DEBUG() << "[" << deviceEndpoint << "] Reconnecting";
    disconnect();

    connect_();
}

void YamahaDeviceBase::shutdown_() {
    AGO_INFO() << "[" << deviceEndpoint << "] Closing";
    do_exit = true;
    disconnect();
}

void YamahaDeviceBase::processSendQueue() {
    if(!socket.get() || !connected || sendState != IDLE)
        return;

    if(sendQueue.empty()) {
        maybeSetIdle();
        return;
    }

    // else, abort any keep-alive..
    sendTimer.cancel();

    assert(sendState == IDLE);
    sendState = SENDING;
    
    const YNCAMessage& msg(sendQueue.front());
    assert(sendBuf.size() == 0);
    msg.write(sendBuf);

    AGO_DEBUG() << "[" << deviceEndpoint << "] Sending " << msg;

    asio::async_write(*socket,
            sendBuf,
            boost::bind(&YamahaDeviceBase::onWrite, this, _1, _2));
}

void YamahaDeviceBase::maybeSetIdle() {
    if(sendState == IDLE) {
        sendTimer.expires_from_now(boost::posix_time::seconds(30));
        sendTimer.async_wait(boost::bind(&YamahaDeviceBase::onSendTimer, this, _1));
    }
}


void YamahaDeviceBase::onTimeout(const boost::system::error_code& error) {
    if(error) return;
    AGO_TRACE() << "[" << deviceEndpoint << "] Timer timeout";

    if(connected)
        reconnect();
    else
        // From retry-timer
        connect_();
}

void YamahaDeviceBase::onConnect(const boost::system::error_code& error) {
    timer.cancel();
    if(error) {
        if(do_exit) return;

        AGO_WARNING() << "[" << deviceEndpoint << "] "
            << "Failed to connect: "
            << (error == asio::error::operation_aborted ? 
                "Connection timeout" : error.message());

        timer.expires_from_now(boost::posix_time::seconds(10));
        timer.async_wait(boost::bind(&YamahaDeviceBase::onTimeout, this, _1));
        return;
    }

    AGO_INFO() << "[" << deviceEndpoint << "] Connected";
    connected = true;

    processSendQueue();
    read();
}

void YamahaDeviceBase::onWrite(const boost::system::error_code& error, size_t bytes_transferred) {
    if(error) {
        AGO_WARNING() << "[" << deviceEndpoint << "] Failed to write: " << error.message();
        reconnect();
        return;
    }else{
        assert(sendBuf.size() == 0);
        assert(sendState == SENDING);
        sendState = SENT;
//        AGO_DEBUG() << bytes_transferred << " bytes written";
    }

    // Response timer
    sendTimer.expires_from_now(boost::posix_time::seconds(5));
    sendTimer.async_wait(boost::bind(&YamahaDeviceBase::onSendTimer, this, _1));
}

void YamahaDeviceBase::onSendTimer(const boost::system::error_code& error) {
    if(error)
        return;

    switch(sendState) {
        case IDLE:
            // Send keep-alive message
            AGO_DEBUG() << "Sending keepalive-query";
            send(YNCAMessage("SYS", "MODELNAME"));
            return;
        case SENT:
            // Send timeout
            AGO_WARNING() << "[" << deviceEndpoint << "] Timeout waiting for command response. Resending";
            sendState = IDLE;
            processSendQueue();
            break;
        case RECEIVED:
            // Message received and we have now delayed.
            sendState = IDLE;
            processSendQueue();
            break;
        default:
            assert(!*"Invalid state");
    }
}

void YamahaDeviceBase::read() {
    asio::async_read_until(*socket,
            recvBuf,
            "\r\n",
            boost::bind(&YamahaDeviceBase::onRead, this, _1, _2));
}

void YamahaDeviceBase::onRead(const boost::system::error_code& error, size_t bytes_transferred) {
    if(error) {
        if(error == boost::system::errc::operation_canceled)
            return;

        if(error == asio::error::eof)
            AGO_INFO() << "[" << deviceEndpoint << "] Connection closed (EOF)";
        else
            AGO_WARNING() << "[" << deviceEndpoint << "] Failed to read: " << error.message();

        reconnect();
        return;
    }

//    AGO_DEBUG() << bytes_transferred << " bytes recieved";

    YNCAMessage msg(recvBuf, bytes_transferred);
    bool handled = false;
    if(sendState == SENT) {
        // Match command response
        YNCAMessage req = sendQueue.front();
        bool noReply = req.replyMode() == YNCAMessage::NoReply;
        if(noReply || msg.isSameFunction(req)) {
            // Matching message
            sendQueue.pop();
            sendTimer.cancel();
            sendState = RECEIVED;

            switch(req.replyMode()) {
            case YNCAMessage::SkipNextIfSameValue:
                assert(sendQueue.size() > 0);
                assert(req.isQuery());
                assert(msg.isSameFunction(sendQueue.front()));

                if(msg.value() == sendQueue.front().value()) {
                    AGO_TRACE() << "Skipping next message, value is correct";
                    sendQueue.pop();
                }

                /* DROP THROUGH */
            case YNCAMessage::NoReply:
                /* Let handled=false trigger auto-feedback below */
                break;
            default:
                handled = true;
                onCommandReply(req, msg);
                break;
            }
            
            // Protocol dictates we delay at least 100ms between commands
            // XXX: Does not really seem necesary.
            sendTimer.expires_from_now(boost::posix_time::milliseconds(100));
            sendTimer.async_wait(boost::bind(&YamahaDeviceBase::onSendTimer, this, _1));
        }
    }
    
    if(!handled) {
        onAutoFeedback(msg);
        maybeSetIdle();
    }

    read();
}

void YamahaDeviceBase::onCommandReply(const YNCAMessage &req, const YNCAMessage &rep) {
    AGO_DEBUG() << "[" << deviceEndpoint << "] Recv command response:" << rep<<
        " (for request " << req << ")";
}
void YamahaDeviceBase::onAutoFeedback(const YNCAMessage &msg) {
    AGO_DEBUG() << "[" << deviceEndpoint << "] Recv auto-feedback: " << msg;
}

