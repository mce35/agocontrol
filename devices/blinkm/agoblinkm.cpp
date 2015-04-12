#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include <stdlib.h>
#include <sstream>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

#include "agoapp.h"

using namespace qpid::types;
using namespace std;
using namespace agocontrol;

class AgoBlinkm: public AgoApp {
private:
    string devicefile;

    bool i2ccommand(const char *device, int i2caddr, int command, size_t size, __u8  *buf);
    qpid::types::Variant::Map  commandHandler(qpid::types::Variant::Map content);
    void setupApp();
public:
    AGOAPP_CONSTRUCTOR(AgoBlinkm);
};

bool AgoBlinkm::i2ccommand(const char *device, int i2caddr, int command, size_t size, __u8  *buf) {
    int file = open(device, O_RDWR);
    if (file < 0) {
        AGO_ERROR() << "cannot open " << file << " - error: " << file;
        return false;
    }
    else
        AGO_DEBUG() << "device open succeeded"  << device;

    if (ioctl(file, I2C_SLAVE, i2caddr) < 0) {
        AGO_ERROR() << "cannot open i2c slave: 0x" << std::hex << i2caddr;
        return false;
    }
    else
        AGO_DEBUG() << "open i2c slave succeeded: 0x" << std::hex << i2caddr;
    int result = i2c_smbus_write_i2c_block_data(file, command, size,buf);
    AGO_DEBUG() << "result: " << result;

    return true;
}

qpid::types::Variant::Map  AgoBlinkm::commandHandler(qpid::types::Variant::Map content) {
    qpid::types::Variant::Map returnval;
    int i2caddr = atoi(content["internalid"].asString().c_str());
    __u8 buf[10];
    if (content["command"] == "on" ) {
        buf[0]=0xff;
        buf[1]=0xff;
        buf[2]=0xff;
        if (i2ccommand(devicefile.c_str(),i2caddr,0x63,3,buf))
        {
            agoConnection->emitEvent(content["internalid"].asString().c_str(), "event.device.statechanged", "255", "");
            return responseSuccess();
        } else return responseFailed("Cannot write i2c command");
    } else if (content["command"] == "off") {
        buf[0]=0x0;
        buf[1]=0x0;
        buf[2]=0x0;
        if (i2ccommand(devicefile.c_str(),i2caddr,0x63,3,buf))
        {
            agoConnection->emitEvent(content["internalid"].asString().c_str(), "event.device.statechanged", "0", "");
            return responseSuccess();
        } else return responseFailed("Cannot write i2c command");
    } else if (content["command"] == "setlevel") {
        checkMsgParameter(content, "level", VAR_INT32);
        buf[0] = atoi(content["level"].asString().c_str()) * 255 / 100;
        buf[1] = atoi(content["level"].asString().c_str()) * 255 / 100;
        buf[2] = atoi(content["level"].asString().c_str()) * 255 / 100;
        if (i2ccommand(devicefile.c_str(),i2caddr,0x63,3,buf))
        {
            agoConnection->emitEvent(content["internalid"].asString().c_str(), "event.device.statechanged", content["level"].asString().c_str(), "");
            return responseSuccess();
        } else return responseFailed("Cannot write i2c command");
    } else if (content["command"] == "setcolor") {
        checkMsgParameter(content, "red", VAR_INT32);
        checkMsgParameter(content, "green", VAR_INT32);
        checkMsgParameter(content, "blue", VAR_INT32);
        int red = 0;
        int green = 0;
        int blue = 0;
        red = content["red"];
        green = content["green"];
        blue = content["blue"];
        buf[0] = red * 255 / 100;
        buf[1] = green * 255 / 100;
        buf[2] = blue * 255 / 100;
        if (i2ccommand(devicefile.c_str(),i2caddr,0x63,3,buf))
        {
            return responseSuccess();
        } else return responseFailed("Cannot write i2c command");
    }
    return responseUnknownCommand();
}

void AgoBlinkm::setupApp() {
    devicefile=getConfigOption("bus", "/dev/i2c-0");
    stringstream devices(getConfigOption("devices", "9")); // read blinkm addr from config, default to addr 9

    string device;
    while (getline(devices, device, ',')) {
        agoConnection->addDevice(device.c_str(), "dimmerrgb");
        i2ccommand(devicefile.c_str(),atoi(device.c_str()),0x6f,0,NULL); // stop script on blinkm
    } 
    addCommandHandler();
}

AGOAPP_ENTRY_POINT(AgoBlinkm);
