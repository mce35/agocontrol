#include <boost/bind.hpp>
#include <boost/make_shared.hpp>

#include "agolog.h"
#include "yamaha_device.h"

namespace asio = boost::asio;

YamahaDevice::YamahaDevice(asio::io_service &io_service,
        const ip::address &addr, unsigned short port)
    : YamahaDeviceBase(io_service, addr, port)
{
    send(YNCAMessage("MAIN", "BASIC", YNCAMessage::NoReply));
    connect();
}

void YamahaDevice::onCommandReply(const YNCAMessage &req, const YNCAMessage &rep) {
    AGO_DEBUG() << "[" << endpoint() << "] Recv command response:" << rep<<
        " (for request " << req << ")";
    data[dataKey(req)] = req.value();
}

void YamahaDevice::onAutoFeedback(const YNCAMessage &msg) {
    AGO_DEBUG() << "[" << endpoint() << "] Recv auto-feedback: " << msg;
    data[dataKey(msg)] = msg.value();
}

std::string YamahaDevice::dataKey(const YNCAMessage &msg) {
    return msg.unit() + ":" + msg.function();
}

void YamahaDevice::powerOn() {
    queryAndSend(YNCAMessage("MAIN", "PWR", "On"));
}
void YamahaDevice::powerOff() {
    queryAndSend(YNCAMessage("MAIN", "PWR", "Off"));
}
void YamahaDevice::mute() {
    queryAndSend(YNCAMessage("MAIN", "MUTE", "On"));
}
void YamahaDevice::unmute() {
    queryAndSend(YNCAMessage("MAIN", "MUTE", "Off"));
}
void YamahaDevice::muteToggle() {
    send(YNCAMessage("MAIN", "MUTE", "On/Off"));
}
void YamahaDevice::volIncr() {
    send(YNCAMessage("MAIN", "VOL", "Up"));
}
void YamahaDevice::volDecr() {
    send(YNCAMessage("MAIN", "VOL", "Down"));
}
void YamahaDevice::volSet(float level) {
    queryAndSend(YNCAMessage("MAIN", "VOL", level));
}
