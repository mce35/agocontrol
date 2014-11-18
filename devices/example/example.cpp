#include "agoapp.h"

using namespace agocontrol;

class AgoExample: public AgoApp {
private:
    void setupApp();
    qpid::types::Variant::Map commandHandler(qpid::types::Variant::Map command);
public:
    AGOAPP_CONSTRUCTOR(AgoExample);
};

qpid::types::Variant::Map AgoExample::commandHandler(qpid::types::Variant::Map command) {
    qpid::types::Variant::Map returnval;
    std::string internalid = command["internalid"].asString();
    if (command["command"] == "on") {
        AGO_DEBUG() << "Switch " << internalid << " ON";
        agoConnection->emitEvent(internalid.c_str(), "event.device.statechanged", "255", "");
        returnval["result"] = 0;

    } else if (command["command"] == "off") {
        AGO_DEBUG() << "Switch " << internalid << " OFF";
        agoConnection->emitEvent(internalid.c_str(), "event.device.statechanged", "0", "");
        returnval["result"] = 0;
    }	
    return returnval;
}

void AgoExample::setupApp() {
    AGO_INFO() << "example device starting up";

    // do some device specific setup
    if (false) {
        AGO_FATAL() << "setup failed";
        throw StartupError();
    }

    // add some devices
    agoConnection->addDevice("123", "dimmer");
    agoConnection->addDevice("124", "switch");

    // add our command handler
    addCommandHandler();
}

AGOAPP_ENTRY_POINT(AgoExample);
