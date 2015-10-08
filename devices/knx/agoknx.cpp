/*
   Copyright (C) 2012 Harald Klein <hari@vt100.at>

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
#include <stdint.h>
#include <time.h>
#include <boost/date_time/posix_time/posix_time_types.hpp>

#include <tinyxml2.h>

#include <eibclient.h>
#include "Telegram.h"

#include "agoapp.h"

#ifndef KNXDEVICEMAPFILE
#define KNXDEVICEMAPFILE "maps/knx.json"
#endif

#ifndef ETSGAEXPORTMAPFILE
#define ETSGAEXPORTMAPFILE "maps/knx_etsgaexport.json"
#endif

using namespace qpid::messaging;
using namespace qpid::types;
using namespace tinyxml2;
using namespace std;
using namespace agocontrol;
namespace fs = ::boost::filesystem;
namespace pt = boost::posix_time;

class AgoKnx: public AgoApp {
private:
    int polldelay;

    Variant::Map deviceMap;

    std::string eibdurl;
    std::string time_ga;
    std::string date_ga;

    EIBConnection *eibcon;
    pthread_mutex_t mutexCon;
    boost::thread *listenerThread;

    qpid::types::Variant::Map commandHandler(qpid::types::Variant::Map content);
    void eventHandler(std::string subject, qpid::types::Variant::Map content);
    void sendDate();
    void sendTime();
    bool sendShortData(std::string dest, int data);
    bool sendCharData(std::string dest, int data);
    bool sendFloatData(std::string dest, float data);

    void setupApp();
    void cleanupApp();

    bool loadDevicesXML(fs::path &filename, Variant::Map& _deviceMap);
    void reportDevices(Variant::Map devicemap);
    string uuidFromGA(Variant::Map devicemap, string ga);
    string typeFromGA(Variant::Map device, string ga);

    void *listener();
public:
    AGOAPP_CONSTRUCTOR_HEAD(AgoKnx)
        , polldelay(0)
        {}
};

/**
 * parses the device XML file and creates a qpid::types::Variant::Map with the data
 */
bool AgoKnx::loadDevicesXML(fs::path &filename, Variant::Map& _deviceMap) {
    XMLDocument devicesFile;
    int returncode;

    AGO_DEBUG() << "trying to open device file: " << filename;
    returncode = devicesFile.LoadFile(filename.c_str());
    if (returncode != XML_NO_ERROR) {
        AGO_ERROR() << "error loading XML file, code: " << returncode;
        return false;
    }

    AGO_TRACE() << "parsing file";
    XMLHandle docHandle(&devicesFile);
    XMLElement* device = docHandle.FirstChildElement( "devices" ).FirstChild().ToElement();
    if (device) {
        XMLElement *nextdevice = device;
        while (nextdevice != NULL) {
            Variant::Map content;

            AGO_TRACE() << "node: " << nextdevice->Attribute("uuid") << " type: " << nextdevice->Attribute("type");

            content["devicetype"] = nextdevice->Attribute("type");
            XMLElement *ga = nextdevice->FirstChildElement( "ga" );
            if (ga) {
                XMLElement *nextga = ga;
                while (nextga != NULL) {
                    AGO_DEBUG() << "GA: " << nextga->GetText() << " type: " << nextga->Attribute("type");
                    string type = nextga->Attribute("type");
/*
                    if (type=="onoffstatus" || type=="levelstatus") {
                        AGO_DEBUG() << "Requesting current status: " << nextga->GetText();
                        Telegram *tg = new Telegram();
                        eibaddr_t dest;
                        dest = Telegram::stringtogaddr(nextga->GetText());
                        tg->setGroupAddress(dest);
                        tg->setType(EIBREAD);
                        tg->sendTo(eibcon);
                    }
*/
                    content[nextga->Attribute("type")]=nextga->GetText();
                    nextga = nextga->NextSiblingElement();
                }
            }
            _deviceMap[nextdevice->Attribute("uuid")] = content;
            nextdevice = nextdevice->NextSiblingElement();
        }
    }
    return true;
}

/**
 * announces our devices in the devicemap to the resolver
 */
void AgoKnx::reportDevices(Variant::Map devicemap) {
    for (Variant::Map::const_iterator it = devicemap.begin(); it != devicemap.end(); ++it) {
        Variant::Map device;
        Variant::Map content;
        Message event;

        device = it->second.asMap();
        agoConnection->addDevice(it->first.c_str(), device["devicetype"].asString().c_str(), true);
    }
}

/**
 * looks up the uuid for a specific GA - this is needed to match incoming telegrams to the right device
 */
string AgoKnx::uuidFromGA(Variant::Map devicemap, string ga) {
    for (Variant::Map::const_iterator it = devicemap.begin(); it != devicemap.end(); ++it) {
        Variant::Map device;

        device = it->second.asMap();
        for (Variant::Map::const_iterator itd = device.begin(); itd != device.end(); itd++) {
            if (itd->second.asString() == ga) {
                // AGO_TRACE() << "GA " << itd->second.asString() << " belongs to " << itd->first;
                return(it->first);
            }
        }
    }	
    return("");
}

/**
 * looks up the type for a specific GA - this is needed to match incoming telegrams to the right event type
 */
string AgoKnx::typeFromGA(Variant::Map device, string ga) {
    for (Variant::Map::const_iterator itd = device.begin(); itd != device.end(); itd++) {
        if (itd->second.asString() == ga) {
            // AGO_TRACE() << "GA " << itd->second.asString() << " belongs to " << itd->first;
            return(itd->first);
        }
    }
    return("");
}
/**
 * thread to poll the knx bus for incoming telegrams
 */
void *AgoKnx::listener() {
    int received = 0;

    AGO_TRACE() << "starting listener thread";
    while(!isExitSignaled()) {
        string uuid;
        pthread_mutex_lock (&mutexCon);
        received=EIB_Poll_Complete(eibcon);
        pthread_mutex_unlock (&mutexCon);
        switch(received) {
            case(-1): 
                AGO_WARNING() << "cannot poll bus";
                try {
                    //boost::this_thread::sleep(pt::seconds(3)); FIXME: check why boost sleep interferes with EIB_Poll_complete, causing delays on status feedback
                    sleep(3);
                } catch(boost::thread_interrupted &e) {
                    AGO_DEBUG() << "listener thread cancelled";
                    break;
                }
                AGO_INFO() << "reconnecting to eibd"; 
                pthread_mutex_lock (&mutexCon);
                EIBClose(eibcon);
                eibcon = EIBSocketURL(eibdurl.c_str());
                if (!eibcon) {
                    pthread_mutex_unlock (&mutexCon);
                    AGO_FATAL() << "cannot reconnect to eibd";
                    signalExit();
                } else {
                    if (EIBOpen_GroupSocket (eibcon, 0) == -1) {
                        AGO_FATAL() << "cannot reconnect to eibd";
                        pthread_mutex_unlock (&mutexCon);
                        signalExit();
                    } else {
                        pthread_mutex_unlock (&mutexCon);
                        AGO_INFO() << "reconnect to eibd succeeded"; 
                    }
                }
                break;
                ;;
            case(0)	:
                try {
                    //boost::this_thread::sleep(pt::milliseconds(polldelay)); FIXME: check why boost sleep interferes with EIB_Poll_complete, causing delays on status feedback
                    usleep(polldelay);
                } catch(boost::thread_interrupted &e) {
                    AGO_DEBUG() << "listener thread cancelled";
                }
                break;
                ;;
            default:
                Telegram tl;
                pthread_mutex_lock (&mutexCon);
                tl.receivefrom(eibcon);
                pthread_mutex_unlock (&mutexCon);
                AGO_DEBUG() << "received telegram from: " << Telegram::paddrtostring(tl.getSrcAddress()) << " to: " 
                    << Telegram::gaddrtostring(tl.getGroupAddress()) << " type: " << tl.decodeType() << " shortdata: "
                    << tl.getShortUserData();
                uuid = uuidFromGA(deviceMap, Telegram::gaddrtostring(tl.getGroupAddress()));
                if (uuid != "") {
                    string type = typeFromGA(deviceMap[uuid].asMap(),Telegram::gaddrtostring(tl.getGroupAddress()));
                    if (type != "") {
                        AGO_DEBUG() << "handling telegram, GA from telegram belongs to: " << uuid << " - type: " << type;
                        if(type == "onoff" || type == "onoffstatus") { 
                            agoConnection->emitEvent(uuid.c_str(), "event.device.statechanged", tl.getShortUserData()==1 ? 255 : 0, "");
                        } else if (type == "setlevel" || type == "levelstatus") {
                            int data = tl.getUIntData(); 
                            agoConnection->emitEvent(uuid.c_str(), "event.device.statechanged", data*100/255, "");
                        } else if (type == "temperature") {
                            agoConnection->emitEvent(uuid.c_str(), "event.environment.temperaturechanged", tl.getFloatData(), "degC");
                        } else if (type == "brightness") {
                            agoConnection->emitEvent(uuid.c_str(), "event.environment.brightnesschanged", tl.getFloatData(), "lux");
                        } else if (type == "energy") {
                            agoConnection->emitEvent(uuid.c_str(), "event.environment.energychanged", tl.getFloatData(), "A");
                        } else if (type == "power") {
                            agoConnection->emitEvent(uuid.c_str(), "event.environment.powerchanged", tl.getFloatData(), "kW");
                        } else if (type == "energyusage") {
                            unsigned char buffer[4];
                            if (tl.getUserData(buffer,4) == 4) {
                                AGO_DEBUG() << "USER DATA: " << std::hex << buffer[0] << " " << buffer[1] << " " << buffer[2] << buffer[3];
                            }
                            // event.setSubject("event.environment.powerchanged");
                        } else if (type == "binary") {
                            agoConnection->emitEvent(uuid.c_str(), "event.security.sensortriggered", tl.getShortUserData()==1 ? 255 : 0, "");
                        }
                    }
                }
                break;
                ;;
        }

    }

    return NULL;
}

void AgoKnx::eventHandler(std::string subject, qpid::types::Variant::Map content) {
    if (subject == "event.environment.timechanged") {
        // send time/date every hour
        if (content["minute"].asInt32() == 0) {
            sendDate();
            sendTime();
        }
    }
}

void AgoKnx::sendDate() {
    uint8_t datebytes[3];   
    time_t now = time(NULL);
    struct tm *lt = localtime(&now);
    datebytes[0] = lt->tm_mday;
    datebytes[1] = lt->tm_mon + 1;
    datebytes[2] = lt->tm_year - 100;
    Telegram *tg_date = new Telegram();
    tg_date->setUserData(datebytes,3);
    tg_date->setGroupAddress(Telegram::stringtogaddr(date_ga));
    pthread_mutex_lock (&mutexCon);
    AGO_TRACE() << "sending telegram";
    tg_date->sendTo(eibcon);
    pthread_mutex_unlock (&mutexCon);
}


void AgoKnx::sendTime() {
    uint8_t timebytes[3];   
    time_t now = time(NULL);
    struct tm *lt = localtime(&now);
    timebytes[0]=((lt->tm_wday?lt->tm_wday:7)<<5) + lt->tm_hour;
    timebytes[1]= lt->tm_min;
    timebytes[2] = lt->tm_sec;
    Telegram *tg_time = new Telegram();
    tg_time->setUserData(timebytes,3);
    tg_time->setGroupAddress(Telegram::stringtogaddr(time_ga));
    pthread_mutex_lock (&mutexCon);
    AGO_TRACE() << "sending telegram";
    tg_time->sendTo(eibcon);
    pthread_mutex_unlock (&mutexCon);
}

bool AgoKnx::sendShortData(std::string dest, int data) {
    Telegram *tg = new Telegram();
    tg->setGroupAddress(Telegram::stringtogaddr(dest));
    tg->setShortUserData(data > 0 ? 1 : 0);
    AGO_TRACE() << "sending value " << data << " as short data telegram to " << dest;
    pthread_mutex_lock (&mutexCon);
    bool result = tg->sendTo(eibcon);
    pthread_mutex_unlock (&mutexCon);
    AGO_DEBUG() << "Result: " << result;
    return result;
}

bool AgoKnx::sendCharData(std::string dest, int data) {
    Telegram *tg = new Telegram();
    tg->setGroupAddress(Telegram::stringtogaddr(dest));
    tg->setDataFromChar(data);
    AGO_TRACE() << "sending value " << data << " as char data telegram to " << dest;
    pthread_mutex_lock (&mutexCon);
    bool result = tg->sendTo(eibcon);
    pthread_mutex_unlock (&mutexCon);
    AGO_DEBUG() << "Result: " << result;
    return result;
}

bool AgoKnx::sendFloatData(std::string dest, float data) {
    Telegram *tg = new Telegram();
    tg->setGroupAddress(Telegram::stringtogaddr(dest));
    tg->setDataFromFloat(data);
    AGO_TRACE() << "sending value " << data << " as float data telegram to " << dest;
    pthread_mutex_lock (&mutexCon);
    bool result = tg->sendTo(eibcon);
    pthread_mutex_unlock (&mutexCon);
    AGO_DEBUG() << "Result: " << result;
    return result;
}


qpid::types::Variant::Map AgoKnx::commandHandler(qpid::types::Variant::Map content) {
    std::string internalid = content["internalid"].asString();
    AGO_TRACE() << "received command " << content["command"] << " for device " << internalid;

    if (internalid == "knxcontroller")
    {
        qpid::types::Variant::Map returnData;

        if (content["command"] == "adddevice")
        {
            checkMsgParameter(content, "devicemap", VAR_MAP);
            // checkMsgParameter(content, "devicetype", VAR_STRING);

            qpid::types::Variant::Map newdevice = content["devicemap"].asMap();
            AGO_TRACE() << "adding knx device: " << newdevice;

            std::string deviceuuid;
            if(content.count("uuid"))
                deviceuuid = content["uuid"].asString();
            else
                deviceuuid = generateUuid();

            deviceMap[deviceuuid] = newdevice;
            agoConnection->addDevice(deviceuuid.c_str(), newdevice["devicetype"].asString().c_str(), true);
            if (variantMapToJSONFile(deviceMap, getConfigPath(KNXDEVICEMAPFILE)))
            {
                returnData["device"] = deviceuuid;
                return responseSuccess(returnData);
            }
            return responseFailed("Failed to write knx device map file");
        }
        else if (content["command"] == "getdevice")
        {
            checkMsgParameter(content, "device", VAR_STRING);
            std::string device = content["device"].asString();

            AGO_TRACE() << "getdevice request: " << device;
            if (!deviceMap.count(device)) 
                return responseError(RESPONSE_ERR_NOT_FOUND, "Device not found");

            returnData["devicemap"] = deviceMap[device].asMap();
            returnData["device"] = device;

            return responseSuccess(returnData);
        }
        else if (content["command"] == "getdevices")
        {
            /*qpid::types::Variant::Map devicelist;
            for (Variant::Map::const_iterator it = devicemap.begin(); it != devicemap.end(); ++it) {
                Variant::Map device;
                device = it->second.asMap();
                devicelist.push_back(device);
            }*/
            returnData["devices"] = deviceMap;
            return responseSuccess(returnData);
        }
        else if (content["command"] == "deldevice")
        {
            checkMsgParameter(content, "device", VAR_STRING);
            
            std::string device = content["device"].asString();
            AGO_TRACE() << "deldevice request:" << device;
            qpid::types::Variant::Map::iterator it = deviceMap.find(device);
            if (it != deviceMap.end())
            {
                AGO_DEBUG() << "removing ago device" << device;
                agoConnection->removeDevice(it->first.c_str());
                deviceMap.erase(it);
                if (!variantMapToJSONFile(deviceMap, getConfigPath(KNXDEVICEMAPFILE)))
                {
                    return responseFailed("Failed to write knx device map file");
                }
            }
            return responseSuccess();

        } 
        else if (content["command"] == "uploadfile")
        {
            checkMsgParameter(content, "filepath", VAR_STRING);

            XMLDocument etsExport;
            std::string etsdata = content["filepath"].asString();
            AGO_TRACE() << "parse ets export request:" << etsdata;
            /*
            if (etsExport.Parse(etsdata.c_str()) != XML_NO_ERROR)
                return responseFailed("Failed to parse XML input data");
            */
            int returncode = etsExport.LoadFile(etsdata.c_str());
            if (returncode != XML_NO_ERROR) {
                AGO_ERROR() << "error loading XML file '" << etsdata << "', code: " << returncode;
                return responseFailed("Failed to parse XML input data");
            }

            XMLHandle docHandle(&etsExport);
            XMLElement* groupRange = docHandle.FirstChildElement("GroupAddress-Export").FirstChild().ToElement();
            if (groupRange) {
                XMLElement *nextRange = groupRange;
                qpid::types::Variant::Map rangeMap;
                while (nextRange != NULL) {
                    AGO_TRACE() << "node: " << nextRange->Attribute("Name");
                    XMLElement *middleRange = nextRange->FirstChildElement( "GroupRange" );
                    if (middleRange)
                    {
                        XMLElement *nextMiddleRange = middleRange;
                        qpid::types::Variant::Map middleMap;
                        while (nextMiddleRange != NULL)
                        {
                            AGO_TRACE() << "middle: " << nextMiddleRange->Attribute("Name");
                            XMLElement *groupAddress = nextMiddleRange->FirstChildElement("GroupAddress");
                            if (groupAddress)
                            {
                                XMLElement *nextGroupAddress = groupAddress;
                                qpid::types::Variant::Map groupMap;
                                while (nextGroupAddress != NULL)
                                {
                                    AGO_TRACE() << "Group: " << nextGroupAddress->Attribute("Name") << " Address: " << nextGroupAddress->Attribute("Address");
                                    groupMap[nextGroupAddress->Attribute("Name")]=nextGroupAddress->Attribute("Address");
                                    nextGroupAddress = nextGroupAddress->NextSiblingElement();
                                }
                                middleMap[nextMiddleRange->Attribute("Name")]=groupMap;

                            }
                            nextMiddleRange = nextMiddleRange->NextSiblingElement();

                        }
                        rangeMap[nextRange->Attribute("Name")]=middleMap;
                    }
                    nextRange = nextRange->NextSiblingElement();
                }
                returnData["groupmap"]=rangeMap;
                variantMapToJSONFile(rangeMap, getConfigPath(ETSGAEXPORTMAPFILE));
            } else 
                return responseFailed("No 'GroupAddress-Export' tag found");

            return responseSuccess(returnData);

        }
        else if (content["command"] == "getgacontent")
        {
            returnData["groupmap"] = jsonFileToVariantMap(getConfigPath(ETSGAEXPORTMAPFILE));
            return responseSuccess(returnData);
        }
        return responseUnknownCommand();
    }

    qpid::types::Variant::Map::const_iterator it = deviceMap.find(internalid);
    qpid::types::Variant::Map device;
    if (it != deviceMap.end()) {
        device=it->second.asMap();
    } else {
       return responseError(RESPONSE_ERR_INTERNAL, "Device not found in internal deviceMap");
    }
    
    bool result;
    if (content["command"] == "on") {
        if (device["devicetype"]=="drapes") {
            result = sendShortData(device["onoff"],0);
        } else {
            result = sendShortData(device["onoff"],1);
        }
    } else if (content["command"] == "off") {
        if (device["devicetype"]=="drapes") {
            result = sendShortData(device["onoff"],1);
        } else {
            result = sendShortData(device["onoff"],0);
        }
    } else if (content["command"] == "stop") {
        result = sendShortData(device["stop"],1);
    } else if (content["command"] == "push") {
        result = sendShortData(device["push"],0);
    } else if (content["command"] == "setlevel") {
        checkMsgParameter(content, "level", VAR_INT32);
        result = sendCharData(device["setlevel"],atoi(content["level"].asString().c_str())*255/100);
    } else if (content["command"] == "settemperature") {
        checkMsgParameter(content, "temperature");
        result = sendFloatData(device["settemperature"],content["temperature"]);
    } else if (content["command"] == "setcolor") {
        checkMsgParameter(content, "red");
        checkMsgParameter(content, "green");
        checkMsgParameter(content, "blue");
        result = sendCharData(device["red"],atoi(content["red"].asString().c_str()));
        result &= sendCharData(device["green"],atoi(content["green"].asString().c_str()));
        result &= sendCharData(device["blue"],atoi(content["blue"].asString().c_str()));
    } else {
        return responseUnknownCommand();
    }

    if (result)
    {
        return responseSuccess();
    }
    return responseError(RESPONSE_ERR_INTERNAL, "Cannot send KNX Telegram");
}

void AgoKnx::setupApp() {
    fs::path devicesFile;

    // parse config
    eibdurl=getConfigOption("url", "ip:127.0.0.1");
    polldelay=atoi(getConfigOption("polldelay", "5000").c_str());
    devicesFile=getConfigOption("devicesfile", getConfigPath("/knx/devices.xml"));


    AGO_INFO() << "connecting to eibd"; 
    eibcon = EIBSocketURL(eibdurl.c_str());
    if (!eibcon) {
        AGO_FATAL() << "can't connect to eibd url:" << eibdurl;
        throw StartupError();
    }

    if (EIBOpen_GroupSocket (eibcon, 0) == -1)
    {
        EIBClose(eibcon);
        AGO_FATAL() << "can't open EIB Group Socket";
        throw StartupError();
    }

    addCommandHandler();
    if (getConfigOption("sendtime", "0")!="0") {
        time_ga = getConfigOption("timega", "3/3/3");
        date_ga = getConfigOption("datega", "3/3/4");
        addEventHandler();
    }

    agoConnection->addDevice("knxcontroller", "knxcontroller");

    // check if old XML file exists and convert it to a json map
    if (fs::exists(devicesFile)) {
        AGO_DEBUG() << "Found XML config file, converting to json map";
        // load xml file into map
        if (!loadDevicesXML(devicesFile, deviceMap)) {
            AGO_FATAL() << "can't load device xml";
            throw StartupError();
        }
        // write json map
        AGO_DEBUG() << "Writing json map into " << getConfigPath(KNXDEVICEMAPFILE);
        variantMapToJSONFile(deviceMap, getConfigPath(KNXDEVICEMAPFILE));
        AGO_INFO() << "XML devices file has been converted to a json map. Renaming old file.";
        fs::rename(devicesFile, fs::path(devicesFile.string() + ".converted"));
    } else {
        AGO_DEBUG() << "Loading json device map";
        deviceMap = jsonFileToVariantMap(getConfigPath(KNXDEVICEMAPFILE));
    }

    // announce devices to resolver
    reportDevices(deviceMap);

    pthread_mutex_init(&mutexCon,NULL);

    AGO_DEBUG() << "Spawning thread for KNX listener";
    listenerThread = new boost::thread(boost::bind(&AgoKnx::listener, this));
}

void AgoKnx::cleanupApp() {
    AGO_TRACE() << "waiting for listener thread to stop";
    listenerThread->interrupt();
    listenerThread->join();
    AGO_DEBUG() << "closing eibd connection";
    EIBClose(eibcon);
}

AGOAPP_ENTRY_POINT(AgoKnx);

