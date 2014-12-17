#include <boost/asio.hpp> 
#include <boost/foreach.hpp> 

#include "agoapp.h"
#include "yamaha_device.h"

using namespace agocontrol;

class AgoYamaha: public AgoApp {
private:
    void setupApp();
    void cleanupApp();
    qpid::types::Variant::Map commandHandler(qpid::types::Variant::Map command);

    std::list<YamahaDevice *> devices;

public:
    AGOAPP_CONSTRUCTOR(AgoYamaha);
};

qpid::types::Variant::Map AgoYamaha::commandHandler(qpid::types::Variant::Map command) {
    qpid::types::Variant::Map returnval;
    std::string internalid = command["internalid"].asString();

    // Find device
    // XX Ugly
    YamahaDevice *dev = NULL;
    BOOST_FOREACH(YamahaDevice *i, devices) {
        if(i->endpoint().address().to_string() == internalid) {
            dev = i;
            break;
        }
    }
    if(!dev) {
        AGO_WARNING() << "Got command for unknown device " << internalid;
            returnval["result"] = 1;
            returnval["error"] = "Parameter level of type float/double is required";
        return returnval;
    }

    if (command["command"] == "on") {
        AGO_INFO() << "Switch " << internalid << " ON";
        dev->powerOn();
    } else if (command["command"] == "off") {
        AGO_INFO() << "Switch " << internalid << " OFF";
        dev->powerOff();
    } else if (command["command"] == "mute") {
        AGO_INFO() << "Muting " << internalid;
        dev->mute();
    } else if (command["command"] == "unmute") {
        AGO_INFO() << "Unmuting " << internalid;
        dev->unmute();
    } else if (command["command"] == "mutetoggle") {
        AGO_INFO() << "Toggling mute on " << internalid;
        dev->muteToggle();
    } else if (command["command"] == "vol+") {
        AGO_INFO() << "Increasing volume on " << internalid;
        // XXX: Dangerous if we queue up alot which cannot raech!
        dev->volIncr();
    } else if (command["command"] == "vol-") {
        AGO_INFO() << "Decreasing volume on " << internalid;
        dev->volDecr();
    } else if (command["command"] == "setlevel") {
        if(command["level"].isVoid() ||
                (command["level"].getType() != qpid::types::VAR_FLOAT &&
                 command["level"].getType() != qpid::types::VAR_DOUBLE)) {

            returnval["result"] = 1;
            returnval["error"] = "Parameter level of type float/double is required";
            return returnval;
        }

        float level = command["level"].asFloat();

        if(level > -10)
            // XXX: Temp protect
            level = -10;

        AGO_INFO() << "Setting volume on " << internalid << " to " << level;
        dev->volSet(level);
    }

    returnval["result"] = 0;
    return returnval;
}

void AgoYamaha::setupApp() {
    // TODO: upnp auto-discovery
    // TODO: configuration file...
    // TODO: value callback in YamahaDevice
    devices.push_back(
            new YamahaDevice(ioService(), ip::address::from_string("172.28.4.130"))
        );

    agoConnection->addDevice("172.28.4.130", "avreceiver");
    addCommandHandler();
}

void AgoYamaha::cleanupApp() {
    BOOST_FOREACH(YamahaDevice *device, devices) {
        device->shutdown();
    }
}

AGOAPP_ENTRY_POINT(AgoYamaha);


