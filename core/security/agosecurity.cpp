#include <stdio.h>
#include <unistd.h>
#include <time.h>

#include <syslog.h>

#include <cstdlib>
#include <iostream>

#include <sstream>
#include <algorithm>
#include <boost/date_time/posix_time/posix_time_types.hpp>

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

    void setupApp() ;

public:

    AGOAPP_CONSTRUCTOR_HEAD(AgoSecurity)
        , isSecurityThreadRunning(false)
        , wantSecurityThreadRunning(false)
        , isAlarmActivated(false) {}
};

/**
 * Check pin code
 */
bool AgoSecurity::checkPin(std::string _pin)
{
    stringstream pins(getConfigOption("pin", "0815"));
    string pin;
    while (getline(pins, pin, ',')) {
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
        //alarm has been canceled by user
        AGO_DEBUG() << "Alarm thread cancelled";
        agoConnection->emitEvent("securitycontroller", "event.security.alarmcanceled", content);
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

        if( uuid=="email" )
        {
            //virtual email device
            content["command"] = "sendmail";
            content["uuid"] = alertControllerUuid;
            content["to"] = "TODO";
            content["subject"] = "Agocontrol security";
            content["body"] = message + "[" + zone + "]";
        }
        else if( uuid=="sms" )
        {
            //virtual sms device
            content["command"] = "sendsms";
            content["uuid"] = alertControllerUuid;
            content["to"] = "TODO";
            content["text"] = message + "[" + zone + "]";
        }
        else if( uuid=="push" )
        {
            //virtual push device
            content["command"] = "sendpush";
            content["uuid"] = alertControllerUuid;
            content["message"] = message + "[" + zone + "]";
        }
        else if( uuid=="twitter" )
        {
            //virtual twitter device
            content["command"] = "sendtweet";
            content["uuid"] = alertControllerUuid;
            content["tweet"] = message + "[" + zone + "]";
        }
        else
        {
            //it's a real device
            content["command"] = "off";
            content["uuid"] = uuid;
        }
        agoConnection->sendMessageReply("", content);
        AGO_DEBUG() << "cancelAlarm: disable alarm uuid='" << uuid << "' " << content;
    }
}

/**
 * Send alarms
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

        if( uuid=="email" )
        {
            //virtual email device
            content["command"] = "sendmail";
            content["uuid"] = alertControllerUuid;
            content["to"] = "tanguy.bonneau@gmail.com";
            content["subject"] = "Agocontrol security";
            content["body"] = message + "[" + zone + "]";
        }
        else if( uuid=="sms" )
        {
            //virtual sms device
            content["command"] = "sendsms";
            content["uuid"] = alertControllerUuid;
            content["to"] = "0660677086";
            content["text"] = message + "[" + zone + "]";
        }
        else if( uuid=="push" )
        {
            //virtual push device
            content["command"] = "sendpush";
            content["uuid"] = alertControllerUuid;
            content["message"] = message + "[" + zone + "]";
        }
        else if( uuid=="twitter" )
        {
            //virtual twitter device
            content["command"] = "sendtweet";
            content["uuid"] = alertControllerUuid;
            content["tweet"] = message + "[" + zone + "]";
        }
        else
        {
            //it's a real device
            content["command"] = "on";
            content["uuid"] = uuid;
        }
        agoConnection->sendMessageReply("", content);
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
        //AGO_DEBUG() << " -> config=" << config;
        for( qpid::types::Variant::Map::iterator it1=config.begin(); it1!=config.end(); it1++ )
        {
            //check housemode
            //AGO_DEBUG() << " -> check housemode: " << it1->first << "==" << housemode;
            if( it1->first==housemode )
            {
                //specified housemode found
                qpid::types::Variant::List zones = it1->second.asList();
                //AGO_DEBUG() << " -> zones: " << zones;
                for( qpid::types::Variant::List::iterator it2=zones.begin(); it2!=zones.end(); it2++ )
                {
                    //check zone
                    qpid::types::Variant::Map zoneMap = it2->asMap();
                    std::string zoneName = zoneMap["zone"].asString();
                    int16_t delay = zoneMap["delay"].asInt16();
                    //check delay (if <0 it means inactive) and if it's current zone
                    //AGO_DEBUG() << " -> check zone: " << zoneName << "==" << zone << " delay=" << delay;
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
            AGO_WARNING() << "Missing housemode in config file.";
        }

        AGO_TRACE() << "event received: " << subject << " - " << content;
        //get device uuid
        if( !content["uuid"].isVoid() && !content["level"].isVoid() )
        {
            string uuid = content["uuid"].asString();
            int64_t level = content["level"].asInt64();
            AGO_DEBUG() << "trigger device uuid=" << uuid << " level=" << level;

            if( level==0 )
            {
                //sensor disabled
                AGO_DEBUG() << "Sensor disabled. Stop";
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
                                AGO_DEBUG() << "housemode[" << housemode << "] is triggered in zone[" << zoneName << "]";
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
}

/**
 * Command handler
 */
qpid::types::Variant::Map AgoSecurity::commandHandler(qpid::types::Variant::Map content)
{
    AGO_TRACE() << "handling command: " << content;
    qpid::types::Variant::Map returndata;
    qpid::types::Variant::Map returnval;
    qpid::types::Variant::Map returncode;

    std::string internalid = content["internalid"].asString();

    if (internalid == "securitycontroller")
    {
        if (content["command"] == "sethousemode")
        {
            // TODO: handle delay
            if (content["housemode"].asString() != "")
            {
                if (checkPin(content["pin"].asString()))
                {
                    if( changeHousemode(content["housemode"].asString()) )
                    {
                        returncode["code"] = "success";
                        returncode["message"] = "Housemode changed";
                        returnval["result"] = returncode;
                    }
                    else
                    {
                        AGO_ERROR() << "Command 'sethousemode': Cannot write securitymap";
                        returndata["housemode"] = securitymap["housemode"].asString();
                        returncode["data"] = returndata;
                        returncode["code"] = "error.security.housemodechange";
                        returncode["message"] = "Cannot write config file";
                        returnval["error"] = returncode;
                    }
                }
                else
                {
                    AGO_ERROR() << "Command 'sethousemode': invalid pin";
                    returndata["housemode"] = securitymap["housemode"].asString();
                    returncode["data"] = returndata;
                    returncode["code"] = "error.security.invalidpin";
                    returncode["message"] = "Invalid pin specified";
                    returnval["error"] = returncode;
                }
            }
            else
            {
                AGO_WARNING() << "Command 'sethousemode': parameter is missing";
                returndata["housemode"] = securitymap["housemode"].asString();
                returncode["data"] = returndata;
                returncode["code"] = "error.parameter.missing";
                returncode["message"] = "Parameter is missing";
                returnval["error"] = returncode;
            }
        }
        else if (content["command"] == "gethousemode")
        {
            if (!(securitymap["housemode"].isVoid()))
            {
                returndata["housemode"]= securitymap["housemode"];
                returncode["data"] = returndata;
                returncode["code"]="success";
                returnval["result"]=returncode;
            }
            else
            {
                AGO_WARNING() << "Command 'gethousemode': no housemode set";
                returncode["code"] = "error.security.housemodenotset";
                returncode["message"] = "No housemode set";
                returnval["error"] = returncode;
            }
        }
        else if (content["command"] == "triggerzone")
        {
            if( !content["zone"].isVoid() )
            {
                std::string zone = content["zone"].asString();
                std::string housemode = securitymap["housemode"];
                TriggerStatus status = triggerZone(zone, housemode);

                switch(status)
                {
                    case Ok:
                    case OkInactiveZone:
                        returncode["code"]="success";
                        returnval["result"] = returncode;
                        break;

                    case KoAlarmFailed:
                        AGO_ERROR() << "Command 'triggerzone': fail to start alarm thread";
                        returncode["code"]="error.security.alarmthreadfailed";
                        returncode["message"]="Failed to start alarm thread";
                        returnval["error"] = returncode;
                        break;

                    case KoAlarmAlreadyRunning:
                        returncode["code"]="success";
                        returncode["message"]="Alarm thread is already running";
                        returnval["result"] = returncode;
                        break;

                    case KoConfigInfoMissing:
                    case KoInvalidConfig:
                        AGO_ERROR() << "Command 'triggerzone': invalid configuration file content";
                        returncode["code"]="error.security.invalidconfig";
                        returncode["message"]="Invalid config";
                        returnval["error"] = returncode;
                        break;

                    default:
                        AGO_ERROR() << "Command 'triggerzone': unknown error";
                        returncode["code"]="error.security.unknown";
                        returncode["message"]="Unknown state";
                        returnval["error"] = returncode;
                        break;
                };
            }
        }
        else if (content["command"] == "cancelalarm")
        {
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
                            returncode["code"]="success";
                            returncode["message"]="Alarm cancelled";
                            returnval["result"] = returncode;
                        }
                        catch(std::exception& e)
                        {
                            AGO_ERROR() << "Command 'cancelalarm': cannot cancel alarm thread!";
                            returncode["message"]="Cannot cancel alarm thread";
                            returncode["code"]="error.security.alarmthreadcancelfailed";
                            returnval["error"] = returncode;
                        }
                    }
                    else
                    {
                        //thread is not running, delay is over and alarm is surely screaming
                        disableAlarm(currentAlarm.zone, currentAlarm.housemode);
                        AGO_INFO() << "Command 'cancelalarm': alarm disabled";
                        returncode["code"] = "success";
                        returncode["message"] = "Alarm disabled";
                        returnval["result"] = returncode;
                    }
                }
                else
                {
                    AGO_ERROR() << "Command 'cancelalarm': no alarm is running :S";
                    returncode["message"]="No alarm running";
                    returncode["code"]="error.security.alarmthreadcancelfailed";
                    returnval["error"] = returncode;
                }
            }
            else
            {
                AGO_ERROR() << "Command 'cancelalarm': invalid pin specified";
                returncode["code"]="error.security.invalidpin";
                returncode["message"]="Invalid pin specified";
                returnval["error"] = returncode;
            }
        }
        else if( content["command"]=="getconfig" )
        {
            qpid::types::Variant::Map returndata;
            if( !securitymap["config"].isVoid() )
            {
                returndata["config"] = securitymap["config"].asMap();
            }
            else
            {
                qpid::types::Variant::Map empty;
                securitymap["config"] = empty;
            }
            returndata["armedMessage"] = "";
            if( !securitymap["armedMessage"].isVoid() )
            {
                returndata["armedMessage"] = securitymap["armedMessage"].asString();
            }
            returndata["disarmedMessage"] = "";
            if( !securitymap["disarmedMessage"].isVoid() )
            {
                returndata["disarmedMessage"] = securitymap["disarmedMessage"].asString();
            }
            returndata["defaultHousemode"] = "";
            if( !securitymap["defaultHousemode"].isVoid() )
            {
                returndata["defaultHousemode"] = securitymap["defaultHousemode"].asString();
            }
            returndata["housemode"] = "";
            if( !securitymap["housemode"].isVoid() )
            {
                returndata["housemode"] = securitymap["housemode"].asString();
            }
            returncode["data"] = returndata;
            returncode["code"]="success";
            returnval["result"]=returncode;
        }
        else if( content["command"]=="setconfig" )
        {
            if( !content["config"].isVoid() && !content["armedMessage"].isVoid() && !content["disarmedMessage"].isVoid() && !content["defaultHousemode"].isVoid() && !content["pin"].isVoid() )
            {
                if( checkPin(content["pin"].asString()) )
                {
                    qpid::types::Variant::Map newconfig = content["config"].asMap();
                    securitymap["config"] = newconfig;
                    securitymap["armedMessage"] = content["armedMessage"].asString();
                    securitymap["disarmedMessage"] = content["disarmedMessage"].asString();
                    securitymap["defaultHousemode"] = content["defaultHousemode"].asString();
                    if (variantMapToJSONFile(securitymap, getConfigPath(SECURITYMAPFILE)))
                    {
                        returncode["code"]="success";
                        returnval["result"] = returncode;
                    }
                    else
                    {
                        AGO_ERROR() << "Command 'setconfig': cannot save securitymap";
                        returncode["message"] = "cannot save securitymap";
                        returncode["code"] = "error.security.setzones";
                        returnval["error"] = returncode;
                    }
                }
                else
                {
                    //invalid pin
                    AGO_ERROR() << "Command 'setconfig': invalid pin";
                    returncode["message"] = "Invalid pin specified";
                    returncode["code"] = "error.security.invalidpin";
                    returnval["error"] = returncode;
                }
            }
            else
            {
                //parameter is missing
                AGO_ERROR() << "Command 'setconfig': missing parameter";
                returncode["message"]="Missing parameter";
                returncode["code"]="error.parameter.missing";
                returnval["error"] = returncode;
            }
        }
        else if( content["command"]=="checkpin" )
        {
            if( !content["pin"].isVoid() )
            {
                if( checkPin(content["pin"].asString() ) )
                {
                    returncode["code"]="success";
                    returnval["result"] = returncode;
                }
                else
                {
                    AGO_WARNING() << "Command 'checkpin': invalid pin";
                    returncode["message"] = "Invalid pin specified";
                    returncode["code"] = "error.security.invalidpin";
                    returnval["error"] = returncode;
                }
            }
            else
            {
                AGO_ERROR() << "Command 'checkpin': parameter is missing";
                returncode["message"]="Missing parameter";
                returncode["code"]="error.parameter.missing";
                returnval["error"] = returncode;
            }
        }
        else if( content["command"]=="setpin" )
        {
            if( !content["pin"].isVoid() && !content["newpin"].isVoid() )
            {
                //check pin
                if (checkPin(content["pin"].asString()))
                {
                    if( setPin(content["newpin"].asString()) )
                    {
                        returncode["code"]="success";
                        returnval["result"] = returncode;
                    }   
                    else
                    {
                        AGO_ERROR() << "Command 'setpin': unable to save pin";
                        returncode["message"] = "Unable to save new pin code";
                        returncode["code"] = "error.security.setpin";
                        returnval["error"] = returncode;
                    }
                }
                else
                {
                    //wrong pin specified
                    AGO_WARNING() << "Command 'setpin': invalid pin";
                    returncode["message"] = "Invalid pin specified";
                    returncode["code"] = "error.security.invalidpin";
                    returnval["error"] = returncode;
                }
            }
            else
            {
                AGO_ERROR() << "Command 'setpin': missing parameter";
                returncode["message"]="missing parameter";
                returncode["code"]="error.parameter.missing";
                returnval["error"] = returncode;
            }
        }
        else if( content["command"]=="getalarmstate" )
        {
            returncode["code"] = "success";
            returncode["data"] = isAlarmActivated;
            returnval["result"] = returncode;
        }
        else
        {
            AGO_ERROR() << "Unknown command received '" << content["command"] << "'";
            returncode["code"]="error.command.invalid";
            returnval["error"] = returncode;
        }
    }
    else
    {
        returncode["code"]="error.device.invalid";
        returnval["error"] = returncode;
    }
    return returnval;
}

void AgoSecurity::setupApp()
{
    securitymap = jsonFileToVariantMap(getConfigPath(SECURITYMAPFILE));

    AGO_TRACE() << "Loaded securitymap: " << securitymap;
    std::string housemode = securitymap["housemode"];
    AGO_DEBUG() << "Current house mode: " << housemode;
    agoConnection->setGlobalVariable("housemode", housemode);

    //get alert controller uuid
    qpid::types::Variant::Map inventory = agoConnection->getInventory();
    if( inventory.size()>0 && !inventory["devices"].isVoid() )
    {
        qpid::types::Variant::Map devices = inventory["devices"].asMap();
        for( qpid::types::Variant::Map::iterator it=devices.begin(); it!=devices.end(); it++ )
        {
            std::string uuid = it->first;
            if( !it->second.isVoid() )
            {
                qpid::types::Variant::Map infos = it->second.asMap();
                //AGO_DEBUG() << infos;
                if( !infos["devicetype"].isVoid() && infos["devicetype"].asString()=="alertcontroller" )
                {
                    alertControllerUuid = uuid;
                    break;
                }
            }
        }
    }
    AGO_DEBUG() << "Found alertController: " << alertControllerUuid;

    //finalize
    agoConnection->addDevice("securitycontroller", "securitycontroller");
    addCommandHandler();
    addEventHandler();
}

AGOAPP_ENTRY_POINT(AgoSecurity);

