#include <stdio.h>
#include <unistd.h>
#include <time.h>

#include <syslog.h>

#include <cstdlib>
#include <iostream>

#include <sstream>
#include <algorithm>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/locks.hpp>

#include "agoapp.h"

#ifndef SECURITYMAPFILE
#define SECURITYMAPFILE "maps/securitymap.json"
#endif

using namespace qpid::messaging;
using namespace qpid::types;
using namespace agocontrol;
using namespace std;
namespace pt = boost::posix_time;

enum TriggerStatus {
    Ok,
    OkInactiveZone,
    KoConfigInfoMissing,
    KoAlarmAlreadyRunning,
    KoInvalidConfig,
    KoAlarmFailed
};

enum ItemType {
    Device,
    Alarm
};

typedef struct TCurrentAlarm {
    std::string housemode;
    std::string zone;
} CurrentAlarm;

class AgoSecurity: public AgoApp {
private:
    std::string agocontroller;
    boost::thread securityThread;
    bool isSecurityThreadRunning;
    bool wantSecurityThreadRunning;
    qpid::types::Variant::Map securitymap;
    qpid::types::Variant::Map inventory;
    std::string alertControllerUuid;
    bool isAlarmActivated;
    CurrentAlarm currentAlarm;
    qpid::types::Variant::Map alertGateways;
    boost::mutex mutexAlertGateways;
    boost::mutex mutexContacts;
    std::string email;
    std::string phone;

    bool checkPin(std::string _pin);
    bool setPin(std::string _pin);
    void enableAlarm(std::string zone, std::string housemode, int16_t delay);
    void disableAlarm(std::string zone, std::string housemode);
    void triggerAlarms(std::string zone, std::string housemode);
    qpid::types::Variant::Map commandHandler(qpid::types::Variant::Map content);
    void eventHandler(std::string subject, qpid::types::Variant::Map content);
    TriggerStatus triggerZone(std::string zone, std::string housemode);
    void getHousemodeItems(std::string zone, std::string housemode, ItemType type, qpid::types::Variant::List* items);
    bool changeHousemode(std::string housemode);
    void refreshAlertGateways();
    void refreshDefaultContact();
    void sendAlarm(std::string zone, std::string uuid, std::string message, qpid::types::Variant::Map* content);

    void setupApp() ;

public:

    AGOAPP_CONSTRUCTOR_HEAD(AgoSecurity)
        , isSecurityThreadRunning(false)
        , wantSecurityThreadRunning(false)
        , isAlarmActivated(false)
        , email("")
        , phone("") {}
};

/**
 * Check pin code
 */
bool AgoSecurity::checkPin(std::string _pin)
{
    stringstream pins(getConfigOption("pin", "0815"));
    string pin;
    while (getline(pins, pin, ','))
    {
        if (_pin == pin) return true;
    }
    return false;
}

/**
 * Set pin codes
 */
bool AgoSecurity::setPin(std::string pin)
{
    return setConfigSectionOption("security", "pin", pin.c_str());
}

/**
 * Return list of alarms or devices for specified housemode/zone
 */
void AgoSecurity::getHousemodeItems(std::string zone, std::string housemode, ItemType type, qpid::types::Variant::List* items)
{
    if( items!=NULL )
    {
        if( !securitymap["config"].isVoid() )
        {
            qpid::types::Variant::Map config = securitymap["config"].asMap();
            for( qpid::types::Variant::Map::iterator it1=config.begin(); it1!=config.end(); it1++ )
            {
                //check housemode
                if( it1->first==housemode )
                {
                    //specified housemode found
                    qpid::types::Variant::List zones = it1->second.asList();
                    for( qpid::types::Variant::List::iterator it2=zones.begin(); it2!=zones.end(); it2++ )
                    {
                        //check zone
                        qpid::types::Variant::Map zoneMap = it2->asMap();
                        std::string zoneName = zoneMap["zone"].asString();
                        if( zoneName==zone )
                        {
                            //specified zone found, fill output list
                            if( type==Alarm )
                            {
                                if( !zoneMap["alarms"].isVoid() )
                                {
                                    *items = zoneMap["alarms"].asList();
                                }
                                else
                                {
                                    AGO_DEBUG() << "getHousemodeItems: invalid config, alarm list is missing!";
                                    return;
                                }
                            }
                            else
                            {
                                if( !zoneMap["devices"].isVoid() )
                                {
                                    *items = zoneMap["devices"].asList();
                                }
                                else
                                {
                                    AGO_DEBUG() << "getHousemodeItems: invalid config, device list is missing!";
                                    return;
                                }
                            }
                        }
                    }
                }
            }
        }
        else
        {
            AGO_DEBUG() << "getHousemodeItems: Invalid config file, config item is missing!";
        }
    }
    else
    {
        AGO_DEBUG() << "getHousemodeItems: Please give valid items object";
    }
}

/**
 * Set housemode
 */
bool AgoSecurity::changeHousemode(std::string housemode)
{
    //update security map
    AGO_INFO() << "Setting housemode: " << housemode;
    securitymap["housemode"] = housemode;
    agoConnection->setGlobalVariable("housemode", housemode);

    //send event that housemode changed
    Variant::Map eventcontent;
    eventcontent["housemode"]= housemode;
    agoConnection->emitEvent("securitycontroller", "event.security.housemodechanged", eventcontent);

    //finally save changes to config file
    return variantMapToJSONFile(securitymap, getConfigPath(SECURITYMAPFILE));
}

/**
 * Enable alarm (threaded)
 * Handle countdown if necessary, send intruderalert event and send zone/housemode associated alarms
 */
void AgoSecurity::enableAlarm(std::string zone, std::string housemode, int16_t delay)
{
    //init
    Variant::Map content;
    Variant::Map countdownstartedeventcontent;
    countdownstartedeventcontent["delay"] = delay;
    countdownstartedeventcontent["zone"] = zone;

    //send countdown started event
    agoConnection->emitEvent("securitycontroller", "event.security.countdown.started", countdownstartedeventcontent);

    //run delay
    try
    {
        content["zone"] = zone;
        AGO_INFO() << "Alarm triggered: zone=" << zone << " housemode=" << housemode << " delay=" << delay;
        while (wantSecurityThreadRunning && delay-- > 0)
        {
            Variant::Map countdowneventcontent;
            countdowneventcontent["delay"] = delay;
            countdowneventcontent["zone"] = zone;
            AGO_TRACE() << "enableAlarm: countdown=" << delay;
            agoConnection->emitEvent("securitycontroller", "event.security.countdown", countdowneventcontent);
            boost::this_thread::sleep(pt::seconds(1));
        }

        //countdown expired
        AGO_INFO() << "Countdown expired for zone=" << zone << " housemode=" << housemode << " , sending 'intruder alert' event";
        //send event
        agoConnection->emitEvent("securitycontroller", "event.security.intruderalert", content);
        //and send alarms
        triggerAlarms(zone, housemode);
    }
    catch(boost::thread_interrupted &e)
    {
        //alarm has been cancelled by user
        AGO_DEBUG() << "Alarm thread cancelled";
        agoConnection->emitEvent("securitycontroller", "event.security.alarmcancelled", content);

        //finally change housemode to default one
        //this is implemented to toggle automatically to another housemode that disable alarm (but it's not mandatory)
        if( !securitymap["defaultHousemode"].isVoid() )
        {
            if( !changeHousemode(securitymap["defaultHousemode"].asString()) )
            {
                AGO_ERROR() << "Unable to write config file saving default housemode";
            }
        }
        else
        {
            AGO_DEBUG() << "enableAlarm: no default housemode, so current housemode is not changed";
        }
    }

    isSecurityThreadRunning = false;
    wantSecurityThreadRunning = false;
    AGO_DEBUG() << "Alarm thread exited";
}

/**
 * Disable current running alarm and stop enabled devices
 */
void AgoSecurity::disableAlarm(std::string zone, std::string housemode)
{
    AGO_INFO() << "Disabling alarm";
    Variant::Map content;
    qpid::types::Variant::List alarms;

    //first of all update flags
    isAlarmActivated = false;

    //and send events
    agoConnection->emitEvent("securitycontroller", "event.security.alarmstopped", content);

    //change housemode to default one
    //this is implemented to toggle automatically to another housemode that disable alarm (but it's not mandatory)
    if( !securitymap["defaultHousemode"].isVoid() )
    {
        if( !changeHousemode(securitymap["defaultHousemode"].asString()) )
        {
            AGO_ERROR() << "Unable to write config file saving default housemode";
        }
    }
    else
    {
        AGO_DEBUG() << "disableAlarm: no default housemode, so current housemode is not changed";
    }

    //then stop alarms
    getHousemodeItems(zone, housemode, Alarm, &alarms);
    for( qpid::types::Variant::List::iterator it=alarms.begin(); it!=alarms.end(); it++ )
    {
        qpid::types::Variant::Map content;
        std::string uuid = *it;

        //get message to send
        std::string message = "";
        if( !securitymap["disarmedMessage"].isVoid() )
        {
            message = securitymap["disarmedMessage"].asString();
        }
        if( message.length()==0 )
        {
            message = "Alarm disarmed";
        }

        //send event
        sendAlarm(zone, uuid, message, &content);
        AGO_DEBUG() << "cancelAlarm: disable alarm uuid='" << uuid << "' " << content;
    }
}

/**
 * Trigger alarms
 */
void AgoSecurity::triggerAlarms(std::string zone, std::string housemode)
{
    //get alarms
    qpid::types::Variant::List alarms;
    getHousemodeItems(zone, housemode, Alarm, &alarms);

    //send event for each alarm
    for( qpid::types::Variant::List::iterator it=alarms.begin(); it!=alarms.end(); it++ )
    {
        qpid::types::Variant::Map content;
        std::string uuid = *it;

        //get message to send
        std::string message = "";
        if( !securitymap["armedMessage"].isVoid() )
        {
            message = securitymap["armedMessage"].asString();
        }
        if( message.length()==0 )
        {
            message = "Alarm armed";
        }

        //send event
        sendAlarm(zone, uuid, message, &content);
        AGO_DEBUG() << "triggerAlarms: trigger device uuid='" << uuid << "' " << content;
    }
}

/**
 * Trigger zone enabling alarm
 */
TriggerStatus AgoSecurity::triggerZone(std::string zone, std::string housemode)
{
    if( !securitymap["config"].isVoid() )
    {
        qpid::types::Variant::Map config = securitymap["config"].asMap();
        AGO_TRACE() << " -> config=" << config;
        for( qpid::types::Variant::Map::iterator it1=config.begin(); it1!=config.end(); it1++ )
        {
            //check housemode
            AGO_TRACE() << " -> check housemode: " << it1->first << "==" << housemode;
            if( it1->first==housemode )
            {
                //specified housemode found
                qpid::types::Variant::List zones = it1->second.asList();
                AGO_TRACE() << " -> zones: " << zones;
                for( qpid::types::Variant::List::iterator it2=zones.begin(); it2!=zones.end(); it2++ )
                {
                    //check zone
                    qpid::types::Variant::Map zoneMap = it2->asMap();
                    std::string zoneName = zoneMap["zone"].asString();
                    int16_t delay = zoneMap["delay"].asInt16();
                    //check delay (if <0 it means inactive) and if it's current zone
                    AGO_TRACE() << " -> check zone: " << zoneName << "==" << zone << " delay=" << delay;
                    if( zoneName==zone )
                    {
                        //specified zone found
                        
                        //check delay
                        if( delay<0 )
                        {
                            //this zone is inactive
                            return OkInactiveZone;
                        }
                        
                        //check if there is already an alarm thread running
                        if( isSecurityThreadRunning==false )
                        {
                            //no alarm thread is running yet
                            try
                            {
                                //fill current alarm
                                currentAlarm.housemode = housemode;
                                currentAlarm.zone = zone;

                                // XXX: Does not handle multiple zone alarms.
                                wantSecurityThreadRunning = true;
                                isAlarmActivated = true;
                                securityThread = boost::thread(boost::bind(&AgoSecurity::enableAlarm, this, zone, housemode, delay));
                                isSecurityThreadRunning = true;
                                return Ok;
                            }
                            catch(std::exception& e)
                            {
                                AGO_FATAL() << "Failed to start alarm thread! " << e.what();
                                return KoAlarmFailed;
                            }
                        }
                        else
                        {
                            //alarm thread already running
                            AGO_DEBUG() << "Alarm thread is already running";
                            return KoAlarmAlreadyRunning;
                        }

                        //stop statement here for this housemode
                        break;
                    }
                }
            }
        }

        //no housemode/zone found
        AGO_ERROR() << "Specified housemode/zone '" << housemode << "/" << zone << "' doesn't exist";
        return KoConfigInfoMissing;
    }
    else
    {
        //invalid config
        return KoInvalidConfig;
    }
}

/**
 * Send an alarm
 */
void AgoSecurity::sendAlarm(std::string zone, std::string uuid, std::string message, qpid::types::Variant::Map* content)
{
    bool found = false;
    bool send = true;
    AGO_TRACE() << "sendAlarm() BEGIN";

    {
       boost::unique_lock<boost::mutex> lock(mutexAlertGateways);
       for( qpid::types::Variant::Map::iterator it=alertGateways.begin(); it!=alertGateways.end(); it++ )
       {
          if( it->first==uuid )
          {
             //gateway found
             found = true;
             std::string gatewayType = it->second.asString();
             if( gatewayType=="smsgateway" )
             {
                boost::unique_lock<boost::mutex> lock(mutexContacts);
                if( phone.size()>0 )
                {
                   (*content)["command"] = "sendsms";
                   (*content)["uuid"] = uuid;
                   (*content)["to"] = phone;
                   (*content)["text"] = message + "[" + zone + "]";
                }
                else
                {
                   AGO_WARNING() << "Trying to send alert to undefined phone number. You must configure default one in system config";
                   send = false;
                }
             }
             else if( gatewayType=="smtpgateway" )
             {
                boost::unique_lock<boost::mutex> lock(mutexContacts);
                if( phone.size()>0 )
                {
                   (*content)["command"] = "sendmail";
                   (*content)["uuid"] = uuid;
                   (*content)["to"] = email;
                   (*content)["subject"] = "Agocontrol security";
                   (*content)["body"] = message + "[" + zone + "]";
                }
                else
                {
                   AGO_WARNING() << "Trying to send alert to undefined email address. You must configure default one in system config";
                   send = false;
                }
             }
             else if( gatewayType=="twittergateway" )
             {
                (*content)["command"] = "sendtweet";
                (*content)["uuid"] = uuid;
                (*content)["tweet"] = message + "[" + zone + "]";
             }
             else if( gatewayType=="pushgateway" )
             {
                (*content)["command"] = "sendpush";
                (*content)["uuid"] = uuid;
                (*content)["message"] = message + "[" + zone + "]";
             }
          }
       }
    }

    if( !found )
    {
        //switch type device
        (*content)["command"] = "on";
        (*content)["uuid"] = uuid;
    }

    if( send )
    {
        agoConnection->sendMessageReply("", *content);
    }
    AGO_TRACE() << "sendAlarm() END";
}

/**
 * Refresh default contact informations (mail + phone number)
 */
void AgoSecurity::refreshDefaultContact()
{
    boost::unique_lock<boost::mutex> lock(mutexContacts);
    std::string oldEmail = email;
    std::string oldPhone = phone;
    email = getConfigOption("email", "", "system", "system");
    phone = getConfigOption("phone", "", "system", "system");
    AGO_DEBUG() << "read email=" << email;
    AGO_DEBUG() << "read phone=" << phone;
    if( oldEmail!=email )
    {
        AGO_DEBUG() << "Default email changed (now " << email << ")";
    }
    if( oldPhone!=phone )
    {
        AGO_DEBUG() << "Default phone number changed (now " << phone << ")";
    }
}

/**
 * Refresh alert gateways
 */
void AgoSecurity::refreshAlertGateways()
{
    //get alert controller uuid
    qpid::types::Variant::Map inventory = agoConnection->getInventory();
    qpid::types::Variant::List gatewayTypes;
    
    if( inventory.size()>0 )
    {
        //get usernotification category devicetypes
        if( !inventory["schema"].isVoid() )
        {
            qpid::types::Variant::Map schema = inventory["schema"].asMap();
            if( !schema["categories"].isVoid() )
            {
                qpid::types::Variant::Map categories = schema["categories"].asMap();
                if( !categories["usernotification"].isVoid() )
                {
                    qpid::types::Variant::Map usernotification = categories["usernotification"].asMap();
                    if( !usernotification["devicetypes"].isVoid() )
                    {
                        gatewayTypes = usernotification["devicetypes"].asList();
                    }
                }
            }
        }

        //get available alert gateway uuids
        if( !inventory["devices"].isVoid() )
        {
            boost::unique_lock<boost::mutex> lock(mutexAlertGateways);

            //clear list
            alertGateways.clear();

            //fill with fresh data
            qpid::types::Variant::Map devices = inventory["devices"].asMap();
            for( qpid::types::Variant::Map::iterator it1=devices.begin(); it1!=devices.end(); it1++ )
            {
                std::string uuid = it1->first;
                if( !it1->second.isVoid() )
                {
                    qpid::types::Variant::Map deviceInfos = it1->second.asMap();
                    for( qpid::types::Variant::List::iterator it2=gatewayTypes.begin(); it2!=gatewayTypes.end(); it2++ )
                    {
                        if( !deviceInfos["devicetype"].isVoid() && deviceInfos["devicetype"].asString()==(*it2) )
                        {
                            AGO_TRACE() << "Found alert " << (*it2) << " with uuid " << uuid << " [name=" << deviceInfos["name"].asString() << "]";
                            alertGateways[uuid] = (*it2);
                        }
                    }
                }
            }
        }
    }
    AGO_TRACE() << "Found alert gateways: " << alertGateways;
}

/**
 * Event handler
 */
void AgoSecurity::eventHandler(std::string subject, qpid::types::Variant::Map content)
{
    bool alarmTriggered = false;
    std::string housemode = "";
    if( securitymap["config"].isVoid() )
    {
        //nothing configured, exit right now
        AGO_DEBUG() << "no config";
        return;
    }

    if( subject=="event.device.statechanged" || subject=="event.security.sensortriggered" )
    {
        //get current housemode
        if( !securitymap["housemode"].isVoid() )
        {
            housemode = securitymap["housemode"].asString();
        }
        else
        {
            //invalid config file
            AGO_ERROR() << "Missing housemode in config file.";
            return;
        }

        AGO_TRACE() << "event received: " << subject << " - " << content;
        //get device uuid
        if( !content["uuid"].isVoid() && !content["level"].isVoid() )
        {
            string uuid = content["uuid"].asString();
            int64_t level = content["level"].asInt64();

            if( level==0 )
            {
                //sensor disabled, nothing to do
                AGO_TRACE() << "Disabled sensor event, event dropped";
                return;
            }
            else if( isAlarmActivated )
            {
                //alarm already running, nothing else to do
                AGO_TRACE() << "Alarm already running, event dropped";
                return;
            }

            //device stated changed, check if it's a monitored one
            qpid::types::Variant::Map config = securitymap["config"].asMap();
            for( qpid::types::Variant::Map::iterator it1=config.begin(); it1!=config.end(); it1++ )
            {
                //check housemode
                std::string hm = it1->first;
                //AGO_TRACE() << " - housemode=" << housemode;
                if( hm==housemode )
                {
                    qpid::types::Variant::List zones = it1->second.asList();
                    for( qpid::types::Variant::List::iterator it2=zones.begin(); it2!=zones.end(); it2++ )
                    {
                        qpid::types::Variant::Map zoneMap = it2->asMap();
                        std::string zoneName = zoneMap["zone"].asString();
                        //AGO_TRACE() << "   - zone=" << zoneName;
                        qpid::types::Variant::List devices = zoneMap["devices"].asList();
                        for( qpid::types::Variant::List::iterator it3=devices.begin(); it3!=devices.end(); it3++ )
                        {
                            //AGO_TRACE() << "     - device=" << *it3;
                            if( (*it3)==uuid )
                            {
                                //zone is triggered
                                AGO_DEBUG() << "housemode[" << housemode << "] is triggered in zone[" << zoneName << "] by device [" << uuid << "]";
                                triggerZone(zoneName, housemode);

                                //trigger alarm only once
                                alarmTriggered = true;
                                break;
                            }
                        }

                        if( alarmTriggered )
                        {
                            //trigger alarm only once
                            break;
                        }
                    }
                }

                if( alarmTriggered )
                {
                    //trigger alarm only once
                    break;
                }
            }
        }
        else
        {
            AGO_DEBUG() << "No uuid for event.device.statechanged " << content;
        }
    }
    else if( subject=="event.environment.timechanged" && !content["minute"].isVoid() && !content["hour"].isVoid() )
    {
        //refresh gateway list every 5 minutes
        if( content["minute"].asInt8()%5==0 )
        {
            AGO_DEBUG() << "Timechanged: get inventory";
            refreshAlertGateways();
            refreshDefaultContact();
        }
    }
}

/**
 * Command handler
 */
qpid::types::Variant::Map AgoSecurity::commandHandler(qpid::types::Variant::Map content)
{
    AGO_TRACE() << "handling command: " << content;
    qpid::types::Variant::Map returnData;

    std::string internalid = content["internalid"].asString();
    if (internalid == "securitycontroller")
    {
        if (content["command"] == "sethousemode")
        {
            checkMsgParameter(content, "housemode", VAR_STRING);
            checkMsgParameter(content, "pin", VAR_STRING);

            if (checkPin(content["pin"].asString()))
            {
                if( changeHousemode(content["housemode"].asString()) )
                {
                    return responseSuccess("Housemode changed");
                }
                else
                {
                    AGO_ERROR() << "Command 'sethousemode': Cannot write securitymap";
                    return responseFailed("Cannot write config file");
                }
            }
            else
            {
                AGO_ERROR() << "Command 'sethousemode': invalid pin";
                returnData["housemode"] = securitymap["housemode"].asString();
                return responseError("error.security.invalidpin", "Invalid pin specified", returnData);
            }
        }
        else if (content["command"] == "gethousemode")
        {
            if (!(securitymap["housemode"].isVoid()))
            {
                returnData["housemode"]= securitymap["housemode"];
                return responseSuccess(returnData);
            }
            else
            {
                AGO_WARNING() << "Command 'gethousemode': no housemode set";
                return responseError("error.security.housemodenotset", "No housemode set", returnData);
            }
        }
        else if (content["command"] == "triggerzone")
        {
            checkMsgParameter(content, "zone", VAR_STRING);

            std::string zone = content["zone"].asString();
            std::string housemode = securitymap["housemode"];
            TriggerStatus status = triggerZone(zone, housemode);

            switch(status)
            {
                case Ok:
                case OkInactiveZone:
                    return responseSuccess();
                    break;

                case KoAlarmFailed:
                    AGO_ERROR() << "Command 'triggerzone': fail to start alarm thread";
                    return responseError("error.security.alarmthreadfailed", "Failed to start alarm thread");
                    break;

                case KoAlarmAlreadyRunning:
                    return responseSuccess("Alarm thread is already running");
                    break;

                case KoConfigInfoMissing:
                case KoInvalidConfig:
                    AGO_ERROR() << "Command 'triggerzone': invalid configuration file content";
                    return responseError("error.security.invalidconfig", "Invalid config");
                    break;

                default:
                    AGO_ERROR() << "Command 'triggerzone': unknown error";
                    return responseError("error.security.unknown", "Unknown state");
                    break;
            }
        }
        else if (content["command"] == "cancelalarm")
        {
            checkMsgParameter(content, "pin", VAR_STRING);

            if (checkPin(content["pin"].asString()))
            {
                if( isAlarmActivated )
                {
                    if( isSecurityThreadRunning )
                    {
                        //thread is running, cancel it, it will cancel alarm
                        wantSecurityThreadRunning = false;
                        try
                        {
                            securityThread.interrupt();
                            isSecurityThreadRunning = false;
                            AGO_INFO() << "Command 'cancelalarm': alarm cancelled";
                            return responseSuccess("Alarm cancelled");
                        }
                        catch(std::exception& e)
                        {
                            AGO_ERROR() << "Command 'cancelalarm': cannot cancel alarm thread!";
                            return responseError("error.security.alarmthreadcancelfailed", "Cannot cancel alarm thread");
                        }
                    }
                    else
                    {
                        //thread is not running, delay is over and alarm is surely screaming
                        disableAlarm(currentAlarm.zone, currentAlarm.housemode);
                        AGO_INFO() << "Command 'cancelalarm': alarm disabled";
                        return responseSuccess("Alarm disabled");
                    }
                }
                else
                {
                    AGO_ERROR() << "Command 'cancelalarm': no alarm is running :S";
                    return responseError("error.security.alarmthreadcancelfailed", "No alarm running");
                }
            }
            else
            {
                AGO_ERROR() << "Command 'cancelalarm': invalid pin specified";
                return responseError("error.security.invalidpin", "Invalid pin specified");
            }
        }
        else if( content["command"]=="getconfig" )
        {
            if( !securitymap["config"].isVoid() )
            {
                returnData["config"] = securitymap["config"].asMap();
            }
            else
            {
                qpid::types::Variant::Map empty;
                securitymap["config"] = empty;
            }
            returnData["armedMessage"] = "";
            if( !securitymap["armedMessage"].isVoid() )
            {
                returnData["armedMessage"] = securitymap["armedMessage"].asString();
            }
            returnData["disarmedMessage"] = "";
            if( !securitymap["disarmedMessage"].isVoid() )
            {
                returnData["disarmedMessage"] = securitymap["disarmedMessage"].asString();
            }
            returnData["defaultHousemode"] = "";
            if( !securitymap["defaultHousemode"].isVoid() )
            {
                returnData["defaultHousemode"] = securitymap["defaultHousemode"].asString();
            }
            returnData["housemode"] = "";
            if( !securitymap["housemode"].isVoid() )
            {
                returnData["housemode"] = securitymap["housemode"].asString();
            }
            return responseSuccess(returnData);
        }
        else if( content["command"]=="setconfig" )
        {
            checkMsgParameter(content, "config", VAR_MAP);
            checkMsgParameter(content, "armedMessage", VAR_STRING, true);
            checkMsgParameter(content, "disarmedMessage", VAR_STRING, true);
            checkMsgParameter(content, "defaultHousemode", VAR_STRING);
            checkMsgParameter(content, "pin", VAR_STRING);

            if( checkPin(content["pin"].asString()) )
            {
                qpid::types::Variant::Map newconfig = content["config"].asMap();
                securitymap["config"] = newconfig;
                securitymap["armedMessage"] = content["armedMessage"].asString();
                securitymap["disarmedMessage"] = content["disarmedMessage"].asString();
                securitymap["defaultHousemode"] = content["defaultHousemode"].asString();
                if (variantMapToJSONFile(securitymap, getConfigPath(SECURITYMAPFILE)))
                {
                    return responseSuccess();
                }
                else
                {
                    AGO_ERROR() << "Command 'setconfig': cannot save securitymap";
                    return responseError("error.security.setzones", "Cannot save securitymap");
                }
            }
            else
            {
                //invalid pin
                AGO_ERROR() << "Command 'setconfig': invalid pin";
                return responseError("error.security.invalidpin", "Invalid pin specified");
            }
        }
        else if( content["command"]=="checkpin" )
        {
            checkMsgParameter(content, "pin", VAR_STRING);
            if( checkPin(content["pin"].asString() ) )
            {
                return responseSuccess();
            }
            else
            {
                AGO_WARNING() << "Command 'checkpin': invalid pin";
                return responseError("error.security.invalidpin", "Invalid pin specified");
            }
        }
        else if( content["command"]=="setpin" )
        {
            checkMsgParameter(content, "pin", VAR_STRING);
            checkMsgParameter(content, "newpin", VAR_STRING);
            //check pin
            if (checkPin(content["pin"].asString()))
            {
                if( setPin(content["newpin"].asString()) )
                {
                    return responseSuccess();
                }   
                else
                {
                    AGO_ERROR() << "Command 'setpin': unable to save pin";
                    return responseError("error.security.setpin", "Unable to save new pin code");
                }
            }
            else
            {
                //wrong pin specified
                AGO_WARNING() << "Command 'setpin': invalid pin";
                return responseError("error.security.invalidpin", "Invalid pin specified");
            }
        }
        else if( content["command"]=="getalarmstate" )
        {
            returnData["alarmactivated"] = isAlarmActivated;
            return responseSuccess(returnData);
        }

        return responseUnknownCommand();
    }

    // We have no devices registered but our own
    throw std::logic_error("Should not go here");
}

void AgoSecurity::setupApp()
{
    //load config
    securitymap = jsonFileToVariantMap(getConfigPath(SECURITYMAPFILE));

    AGO_TRACE() << "Loaded securitymap: " << securitymap;
    std::string housemode = securitymap["housemode"];
    AGO_DEBUG() << "Current house mode: " << housemode;
    agoConnection->setGlobalVariable("housemode", housemode);

    //get available alert gateways
    refreshAlertGateways();
    refreshDefaultContact();

    //finalize
    agoConnection->addDevice("securitycontroller", "securitycontroller");
    addCommandHandler();
    addEventHandler();
}

AGOAPP_ENTRY_POINT(AgoSecurity);

