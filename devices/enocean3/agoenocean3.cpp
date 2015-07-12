/*
   Copyright (C) 2009 Harald Klein <hari@vt100.at>

   This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

   See the GNU General Public License for more details.

*/

#include <iostream>
#include <sstream>
#include <uuid/uuid.h>
#include <stdlib.h>

#include <unistd.h>
#include <pthread.h>
#include <stdio.h>

#include "agoapp.h"
#include "esp3.h"

#ifndef DEVICEMAPFILE
#define DEVICEMAPFILE "/maps/enocean3.json"
#endif

using namespace std;
using namespace qpid::types;
using namespace agocontrol;

class AgoEnocean3: public AgoApp {
private:
    esp3::ESP3 *myESP3;
    bool learnmode;
    qpid::types::Variant::Map devicemap;
    void setupApp();
    qpid::types::Variant::Map commandHandler(qpid::types::Variant::Map content);
public:
    AGOAPP_CONSTRUCTOR_HEAD(AgoEnocean3)
        , learnmode(false)
        {}

    void _handler(esp3::Notification const* message);
};

void handler(esp3::Notification const* _message, void *context) {
    AgoEnocean3 *inst = static_cast<AgoEnocean3*>(context);
    inst->_handler(_message);
}

void AgoEnocean3::_handler(esp3::Notification const* message) {
    AGO_DEBUG() << "Received Enocean Notification type: " << message->getType() << " ID: " << std::hex << message->getId();
    stringstream id;
    id << std::hex << message->getId();
	if (message->getType() == "Switch") {
        const esp3::SwitchNotification *switchNotif = (const esp3::SwitchNotification *)message;
        if (switchNotif->getIsPressed()) AGO_DEBUG() << "isPressed() == true";
        AGO_DEBUG() << "Rocker id: " << switchNotif->getRockerId();
        if (learnmode) {
            learnmode=false;
            int max;
            if (switchNotif->getRockerId() > 2) { // this is a two paddle switch - beware, user has to press the second paddle in learn mode for this detection to work
                max = 4;
            } else max = 2;
                
            for (int i=1; i<= max; i++) {
                stringstream internalid;
                internalid << id.str() << "-" << i;
                devicemap[internalid.str()]="remoteswitch";
                agoConnection->addDevice(internalid.str().c_str(), "remoteswitch");
            }
            variantMapToJSONFile(devicemap, getConfigPath(DEVICEMAPFILE));
        } else {
            if (switchNotif->getIsPressed()) {
                stringstream internalid;
                internalid << id.str() << "-" << switchNotif->getRockerId();
                agoConnection->emitEvent(internalid.str().c_str(), "event.device.statechanged", 255, "");
            } else {
                for (qpid::types::Variant::Map::iterator it = devicemap.begin(); it!=devicemap.end(); it++) {
                    AGO_DEBUG() << "matching id: " << it->first;
                    if (it->first.find(id.str()) != std::string::npos) {
                        agoConnection->emitEvent(it->first.c_str(), "event.device.statechanged", 0, "");
                    }
                }
            }
        }
	}
}

qpid::types::Variant::Map AgoEnocean3::commandHandler(qpid::types::Variant::Map content) {
    std::string internalid = content["internalid"].asString();
    if (internalid == "enoceancontroller") {
        if (content["command"] == "teachframe") {
            checkMsgParameter(content, "channel", VAR_INT32);
            // this is optional - checkMsgParameter(content, "profile", VAR_STRING);
            int channel = content["channel"];
            std::string profile = content["profile"];
            if (profile == "central command dimming") {
                myESP3->fourbsCentralCommandDimTeachin(channel);
            } else {
                myESP3->fourbsCentralCommandSwitchTeachin(channel);
            }
            return responseSuccess();
        } else if (content["command"] == "setlearnmode") {
            checkMsgParameter(content, "mode");
            if (content["mode"]=="start") {
                learnmode = true;
            } else {
                learnmode = false;
            }
            return responseSuccess();
        } else if (content["command"] == "setidbase") {
            return responseError(RESPONSE_ERR_INTERNAL, "set id base not yet implemented");
        }
    } else {
        int rid = 0; rid = atol(internalid.c_str());
        if (content["command"] == "on") {
            if (agoConnection->getDeviceType(internalid.c_str())=="dimmer") {
                myESP3->fourbsCentralCommandDimLevel(rid,0x64,1);
            } else {
                myESP3->fourbsCentralCommandSwitchOn(rid);
            }
            return responseSuccess();
        } else if (content["command"] == "off") {
            if (agoConnection->getDeviceType(internalid.c_str())=="dimmer") {
                myESP3->fourbsCentralCommandDimOff(rid);
            } else {
                myESP3->fourbsCentralCommandSwitchOff(rid);
            }
            return responseSuccess();
        } else if (content["command"] == "setlevel") {
            checkMsgParameter(content, "level", VAR_INT32);
            uint8_t level = 0;
            level = content["level"];
            myESP3->fourbsCentralCommandDimLevel(rid,level,1);
            return responseSuccess();
        }
    }
    return responseUnknownCommand();
}

void AgoEnocean3::setupApp() {
    std::string devicefile;
    devicefile=getConfigOption("device", "/dev/ttyAMA0");
    myESP3 = new esp3::ESP3(devicefile);
    if (!myESP3->init()) {
        AGO_FATAL() << "cannot initalize enocean ESP3 protocol on device " << devicefile;
        throw StartupError();
    }
    myESP3->setHandler(handler, this);

    addCommandHandler();
    agoConnection->addDevice("enoceancontroller", "enoceancontroller");

    stringstream dimmers(getConfigOption("dimmers", "1"));
    string dimmer;
    while (getline(dimmers, dimmer, ',')) {
        agoConnection->addDevice(dimmer.c_str(), "dimmer");
        AGO_DEBUG() << "adding rid " << dimmer << " as dimmer";
    } 
    stringstream switches(getConfigOption("switches", "20"));
    string switchdevice;
    while (getline(switches, switchdevice, ',')) {
        agoConnection->addDevice(switchdevice.c_str(), "switch");
        AGO_DEBUG() << "adding rid " << switchdevice << " as switch";
    } 

    devicemap = jsonFileToVariantMap(getConfigPath(DEVICEMAPFILE));
    for (qpid::types::Variant::Map::iterator it = devicemap.begin(); it!=devicemap.end(); it++) {
        AGO_DEBUG() << "Found id in devicemap file: " << it->first;
        agoConnection->addDevice(it->first.c_str(), it->second.asString().c_str());
    }
}

AGOAPP_ENTRY_POINT(AgoEnocean3);
