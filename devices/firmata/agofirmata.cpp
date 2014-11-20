#include <iostream>
#include <stdlib.h>
#include <sstream>

#include "agoapp.h"
#include "firmata.h"

using namespace std;
using namespace agocontrol;

class AgoFirmata: public AgoApp {
private:
    Firmata* f;

    qpid::types::Variant::Map commandHandler(qpid::types::Variant::Map content);
    void setupApp();
    void cleanupApp();
public:
    AGOAPP_CONSTRUCTOR(AgoFirmata);
};

qpid::types::Variant::Map AgoFirmata::commandHandler(qpid::types::Variant::Map content) {
    qpid::types::Variant::Map returnval;
    int pin = atoi(content["internalid"].asString().c_str());
    if (content["command"] == "on" ) {
        f->writeDigitalPin(pin,ARDUINO_HIGH);
        // TODO: send proper status events
    } else if (content["command"] == "off") {
        f->writeDigitalPin(pin,ARDUINO_LOW);
    }
    returnval["result"] = 0; // TODO: determine proper result code
    return returnval;
}


void AgoFirmata::setupApp() {
    string devicefile=getConfigOption("device", "/dev/ttyUSB2");
    stringstream outputs(getConfigOption("outputs", "2")); // read digital out pins from config, default to pin 2 only

    f = new Firmata();
    if (f->openPort(devicefile.c_str()) != 0) {
        AGO_FATAL() << "cannot open device: " << devicefile;
        f->destroy();
        throw StartupError();
    }

    AGO_INFO() << "Firmata version: " <<  f->getFirmwareVersion();

    string output;
    while (getline(outputs, output, ',')) {
        f->setPinMode(atoi(output.c_str()), FIRMATA_OUTPUT);
        agoConnection->addDevice(output.c_str(), "switch");
        AGO_INFO() << "adding DIGITAL out pin as switch: " << output;
    } 
    addCommandHandler();
}

void AgoFirmata::cleanupApp() {
    f->destroy();
}

AGOAPP_ENTRY_POINT(AgoFirmata);
