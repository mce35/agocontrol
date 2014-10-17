#include "agoclient.h"

using namespace agocontrol;
AgoConnection *agoConnection;

qpid::types::Variant::Map commandHandler(qpid::types::Variant::Map command) {
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

int main(int argc, char **argv) {
	agoConnection = new AgoConnection("example");
	AGO_INFO() << "example device starting up";
	agoConnection->addDevice("123", "dimmer");
	agoConnection->addDevice("124", "switch");
	agoConnection->addHandler(commandHandler);
	agoConnection->run();
}
