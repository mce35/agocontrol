/*
   Copyright (C) 2014 Jimmy Rentz <rentzjam@gmail.com>

   This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

   See the GNU General Public License for more details.

   curl helpers from User Galik on http://www.cplusplus.com/user/Galik/ - http://www.cplusplus.com/forum/unices/45878/

NOTE: This is based on the agocontrol webcam device.
*/

#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <uuid/uuid.h>
#include <stdlib.h>

#include <unistd.h>
#include <pthread.h>
#include <stdio.h>

#include <curl/curl.h>

#include "agoapp.h"

#include "../webcam/base64.h"
#include "zoneminderclient.h"

using namespace std;
using namespace agocontrol;

class AgoZmcam: public AgoApp {
private:
    ZoneminderClient zoneminderClient;

    void setupApp();
    qpid::types::Variant::Map commandHandler(qpid::types::Variant::Map content);
public:
    AGOAPP_CONSTRUCTOR(AgoZmcam);
};

qpid::types::Variant::Map AgoZmcam::commandHandler(qpid::types::Variant::Map content) 
{
    int monitorId = content["internalid"].asInt32();
    if (content["command"] == "getvideoframe") 
    {
        ostringstream tmpostr;
        if (zoneminderClient.getVideoFrame(monitorId, tmpostr))
        {
            std::string s;
            s = tmpostr.str();	
            qpid::types::Variant::Map returnval;
            returnval["image"]  = base64_encode(reinterpret_cast<const unsigned char*>(s.c_str()), s.length());
            return responseSuccess(returnval);
        } 
        else
        {
            AGO_ERROR() << "commandHandler [getvideoframe] - Could not get video frame for monitor " << monitorId << endl;
            return responseError(RESPONSE_ERR_INTERNAL, "Cannot fetch video frame from monitor");
        }
    }
    else if (content["command"] == "triggeralarm")
    {
        if (zoneminderClient.setMonitorAlert(monitorId,
                    content["duration"].asUint32(),
                    content["importance"].asInt32(),
                    content["cause"].asString(),
                    content["description"].asString(),
                    content["detail"].asString()))
            return responseSuccess();
        else
        {
            AGO_ERROR() << "commandHandler [triggeralarm] - Could not trigger camera alarm for monitor " << monitorId << endl;
            return responseError(RESPONSE_ERR_INTERNAL, "Cannot camera alarm for monitor");
        }
    }
    else if (content["command"] == "clearalarm")
    {
        if (zoneminderClient.clearMonitorAlert(monitorId))
            return responseSuccess();
        else
        {
            AGO_ERROR() << "commandHandler [clearalarm] - Could not clear alarm for monitor " << monitorId << endl;
            return responseError(RESPONSE_ERR_INTERNAL, "Cannot clear alarm for monitor");
        }
    }
    return responseUnknownCommand();
}

void AgoZmcam::setupApp() {
    string temp;

    temp = getConfigOption("authtype", "");

    curl_global_init(CURL_GLOBAL_ALL);

    transform(temp.begin(), temp.end(), temp.begin(), ::tolower);

    if (temp != "hash" && temp != "plain"  && temp != "none")
    {
        AGO_FATAL() << "doInitialize - Invalid zmcam auth type of " << temp << " specified.  Only hash, plain and none are supported." << endl;
        throw StartupError();
    }

    ZM_AUTH_TYPE authType;

    if (temp == "hash")
        authType = ZM_AUTH_HASH;
    else if (temp == "plain")
        authType = ZM_AUTH_PLAIN;
    else
        authType = ZM_AUTH_NONE;

    temp = getConfigOption("hashauthuselocaladdress", "n");

    bool hashAuthUseLocalAddress;

    transform(temp.begin(), temp.end(), temp.begin(), ::tolower);

    if (temp[0] == 'y' || temp[0] == '1' || temp[0] == 't')
        hashAuthUseLocalAddress = true;
    else
        hashAuthUseLocalAddress = false;

    if (!zoneminderClient.create(getConfigOption("webprotocal", "http"), 
                getConfigOption("server", ""), 
                stringToUint(getConfigOption("webport", "80")), 
                authType,
                hashAuthUseLocalAddress,
                getConfigOption("username", ""),
                getConfigOption("password", ""),
                getConfigOption("authhashscret", ""),
                stringToUint(getConfigOption("triggerport", "6802"))))
    {
        AGO_FATAL() << "cannot create Zoneminder client";
        throw StartupError();
    }

    stringstream devices(getConfigOption("monitors", ""));

    string device;
    while (getline(devices, device, ','))
        agoConnection->addDevice(device.c_str(), "camera");

    addCommandHandler();
}

AGOAPP_ENTRY_POINT(AgoZmcam);
