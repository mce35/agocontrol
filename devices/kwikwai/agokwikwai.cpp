/*
   Copyright (C) 2012 Harald Klein <hari@vt100.at>

   This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

   See the GNU General Public License for more details.

*/

#include <iostream>
#include <string.h>

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

#include "agoclient.h"

#include "kwikwai.h"


using namespace std;
using namespace agocontrol;

kwikwai::Kwikwai *myKwikwai;

qpid::types::Variant::Map commandHandler(qpid::types::Variant::Map content) {
    qpid::types::Variant::Map returnval;
    std::string internalid = content["internalid"].asString();
    if (internalid == "hdmicec") {
        if (content["command"] == "alloff" ) {
            myKwikwai->cecSend("FF:36");
            returnval["result"] = 0;
        }
    } else if (internalid == "tv") {
        if (content["command"] == "on" ) {
            myKwikwai->cecSend("F0:04");
            returnval["result"] = 0;
        } else if (content["command"] == "off" ) {
            myKwikwai->cecSend("F0:36");
            returnval["result"] = 0;
        }
    }
    return returnval;
}

int main(int argc, char **argv) {
    std::string hostname;
    std::string port;

    hostname=getConfigOption("kwikwai", "host", "kwikwai.local");
    port=getConfigOption("kwikwai", "port", "9090");

    kwikwai::Kwikwai _myKwikwai(hostname.c_str(), port.c_str());
    myKwikwai = &_myKwikwai;
    AGO_INFO() << "Version: " << myKwikwai->getVersion();
    AgoConnection agoConnection = AgoConnection("kwikwai");		

    agoConnection.addDevice("hdmicec", "hdmicec");
    agoConnection.addDevice("tv", "tv");
    agoConnection.addHandler(commandHandler);

    agoConnection.run();
}

