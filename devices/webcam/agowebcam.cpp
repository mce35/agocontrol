/*
   Copyright (C) 2013 Harald Klein <hari@vt100.at>

   This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

   See the GNU General Public License for more details.

   curl helpers from User Galik on http://www.cplusplus.com/user/Galik/ - http://www.cplusplus.com/forum/unices/45878/

*/

#include <iostream>
#include <sstream>
#include <fstream>
#include <uuid/uuid.h>
#include <stdlib.h>

#include <unistd.h>
#include <pthread.h>
#include <stdio.h>

#include <curl/curl.h>

#include "agoapp.h"
#include "base64.h"

using namespace std;
using namespace agocontrol;

class AgoWebcam: public AgoApp {
private:
    void setupApp();
    qpid::types::Variant::Map commandHandler(qpid::types::Variant::Map content);

    CURLcode curl_read(const std::string& url, std::ostream& os, long timeout);
public:
    AGOAPP_CONSTRUCTOR(AgoWebcam);
};

// callback function writes data to a std::ostream
static size_t data_write(void* buf, size_t size, size_t nmemb, void* userp) {
    if(userp) {
        std::ostream& os = *static_cast<std::ostream*>(userp);
        std::streamsize len = size * nmemb;
        if(os.write(static_cast<char*>(buf), len))
            return len;
    }

    return 0;
}

CURLcode AgoWebcam::curl_read(const std::string& url, std::ostream& os, long timeout = 30) {
    CURLcode code(CURLE_FAILED_INIT);
    CURL* curl = curl_easy_init();

    if(curl)
    {
        if(CURLE_OK == (code = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &data_write))
                && CURLE_OK == (code = curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L))
                && CURLE_OK == (code = curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L))
                && CURLE_OK == (code = curl_easy_setopt(curl, CURLOPT_FILE, &os))
                && CURLE_OK == (code = curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout))
                && CURLE_OK == (code = curl_easy_setopt(curl, CURLOPT_URL, url.c_str())))
        {
            code = curl_easy_perform(curl);
        }
        curl_easy_cleanup(curl);
    }
    return code;
}

qpid::types::Variant::Map AgoWebcam::commandHandler(qpid::types::Variant::Map content) {
    std::string internalid = content["internalid"].asString();
    if (content["command"] == "getvideoframe") {
        ostringstream tmpostr;
        if(CURLE_OK == curl_read(internalid, tmpostr, 2)) {
            std::string s;
            s = tmpostr.str();  
            qpid::types::Variant::Map returnval;
            returnval["image"]  = base64_encode(reinterpret_cast<const unsigned char*>(s.c_str()), s.length());
            return responseSuccess(returnval);
        } else {
            return responseError(RESPONSE_ERR_INTERNAL, "Cannot fetch webcam image");
        }
    }
    return responseUnknownCommand();
}

void AgoWebcam::setupApp() {
    curl_global_init(CURL_GLOBAL_ALL);

    addCommandHandler();

    stringstream devices(getConfigOption("devices", "http://192.168.80.65/axis-cgi/jpg/image.cgi"));
    string device;
    while (getline(devices, device, ',')) {
        if (device.find("rtsp://") != string::npos) {
            agoConnection->addDevice(device.c_str(), "onvifnvt"); // this is a helper for agoonvif for situations where the device can't be discovered (e.g. multicast issues)
        } else {
            agoConnection->addDevice(device.c_str(), "camera");
        }
    } 
}

AGOAPP_ENTRY_POINT(AgoWebcam);
