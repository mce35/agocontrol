#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <syslog.h>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/date_time/local_time/local_time.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

#include "agoapp.h"

#ifndef SECURITYMAPFILE
#define SECURITYMAPFILE "maps/securitymap.json"
#endif

#ifndef RECORDINGSDIR
#define RECORDINGSDIR "recordings/"
#endif

using namespace qpid::messaging;
using namespace qpid::types;
using namespace agocontrol;
using namespace std;
using namespace cv;
namespace pt = boost::posix_time;
namespace fs = boost::filesystem;

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
    pthread_mutex_t alertGatewaysMutex;
    pthread_mutex_t contactsMutex;
    pthread_mutex_t securitymapMutex;
    std::string email;
    std::string phone;
    bool stopProcess;
    void setupApp();
    void cleanupApp();

    //security
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

    //timelapse
    bool stopTimelapses;
    std::map<std::string, boost::thread*> timelapseThreads;
    void getEmptyTimelapse(qpid::types::Variant::Map* timelapse);
    void timelapseFunction(qpid::types::Variant::Map timelapse);
    void restartTimelapses();
    void launchTimelapses();
    void launchTimelapse(qpid::types::Variant::Map& timelapse);

public:

    AGOAPP_CONSTRUCTOR_HEAD(AgoSecurity)
        , isSecurityThreadRunning(false)
        , wantSecurityThreadRunning(false)
        , isAlarmActivated(false)
        , email("")
        , phone("")
        , stopProcess(false)
        , stopTimelapses(false) {}
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
    pthread_mutex_lock(&securitymapMutex);
    securitymap["housemode"] = housemode;
    agoConnection->setGlobalVariable("housemode", housemode);

    //send event that housemode changed
    Variant::Map eventcontent;
    eventcontent["housemode"]= housemode;
    agoConnection->emitEvent("securitycontroller", "event.security.housemodechanged", eventcontent);
    pthread_mutex_unlock(&securitymapMutex);

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
 * Send an alarm
 */
void AgoSecurity::sendAlarm(std::string zone, std::string uuid, std::string message, qpid::types::Variant::Map* content)
{
    bool found = false;
    bool send = true;
    AGO_DEBUG() << "sendAlarm() BEGIN";

    pthread_mutex_lock(&alertGatewaysMutex);
    for( qpid::types::Variant::Map::iterator it=alertGateways.begin(); it!=alertGateways.end(); it++ )
    {
        if( it->first==uuid )
        {
            //gateway found
            found = true;
            std::string gatewayType = it->second.asString();
            if( gatewayType=="smsgateway" )
            {
                pthread_mutex_lock(&contactsMutex);
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
                pthread_mutex_unlock(&contactsMutex);
            }
            else if( gatewayType=="smtpgateway" )
            {
                pthread_mutex_lock(&contactsMutex);
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
                pthread_mutex_unlock(&contactsMutex);
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
    pthread_mutex_unlock(&alertGatewaysMutex);

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
    AGO_DEBUG() << "sendAlarm() END";
}

/**
 * Refresh default contact informations (mail + phone number)
 */
void AgoSecurity::refreshDefaultContact()
{
    pthread_mutex_lock(&contactsMutex);
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
    pthread_mutex_unlock(&contactsMutex);
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
            pthread_mutex_lock(&alertGatewaysMutex);

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

            pthread_mutex_unlock(&alertGatewaysMutex);
        }
    }
    AGO_TRACE() << "Found alert gateways: " << alertGateways;
}

/**
 * Timelapse function (threaded)
 */
void AgoSecurity::timelapseFunction(qpid::types::Variant::Map timelapse)
{
    AGO_DEBUG() << "Timelapse started: " << timelapse;

    //init video reader
    VideoCapture capture(timelapse["uri"].asString());
    if( !capture.isOpened() )
    {
        //TODO send error?
        AGO_ERROR() << "Timelapse: unable to capture uri";
        return;
    }
    double srcWidth = capture.get(CV_CAP_PROP_FRAME_WIDTH);
    double srcHeight = capture.get(CV_CAP_PROP_FRAME_HEIGHT);

    //init video writer
    bool fileOk = false;
    int inc = 0;
    fs::path filepath;
    while( !fileOk )
    {
        std::string name = timelapse["name"].asString();
        stringstream filename;
        filename << RECORDINGSDIR;
        filename << "timelapse_";
        if( name.length()==0 )
        {
            filename << "noname_";
        }
        else
        {
            filename << name << "_";
        }
        filename << pt::second_clock::local_time().date().year() << pt::second_clock::local_time().date().month() << pt::second_clock::local_time().date().day();
        if( inc>0 )
        {
            filename << "_" << inc;
        }
        filename << ".avi";
        filepath = ensureParentDirExists(getLocalStatePath(filename.str()));
        if( fs::exists(filepath) )
        {
            //file already exists
            inc++;
        }
        else
        {
            fileOk = true;
        }
    }
    AGO_DEBUG() << "Record into '" << filepath.c_str() << "'";
    string codec = timelapse["codec"].asString();
    int fourcc = CV_FOURCC('F', 'M', 'P', '4');
    if( codec.length()==4 )
    {
        fourcc = CV_FOURCC(codec[0], codec[1], codec[2], codec[3]);
    }
    int fps = 24;
    Size resolution(srcWidth, srcHeight);
    VideoWriter recorder(filepath.c_str(), fourcc, fps, resolution);
    if( !recorder.isOpened() )
    {
        //TODO send error?
        AGO_ERROR() << "Timelapse: unable to open recorder";
        return;
    }

    try
    {
        Mat edges;
        int now = (int)(time(NULL));
        int last = 0;
        while( !stopProcess && !stopTimelapses )
        {
            //capture frame
            Mat frame;
            capture >> frame;

            //add current time
            stringstream stream;
            stream << pt::second_clock::local_time().date().year() << "/" << (int)pt::second_clock::local_time().date().month() << "/" << pt::second_clock::local_time().date().day();
            stream << " " << pt::second_clock::local_time().time_of_day();
            stream << " - " << timelapse["name"].asString();
            string text = stream.str();
            putText(frame, text.c_str(), Point(20,20), FONT_HERSHEY_SIMPLEX, 0.5, Scalar(0,0,0), 4, CV_AA);
            putText(frame, text.c_str(), Point(20,20), FONT_HERSHEY_SIMPLEX, 0.5, Scalar(255,255,255), 1, CV_AA);
    
            if( now!=last )
            {
                //TODO handle recording fps using timelapse["fps"] param. For now it records at 1 fps
                recorder << frame;
                last = now;
            }
    
            now = (int)(time(NULL));
        }
    }
    catch(boost::thread_interrupted &e)
    {
        AGO_DEBUG() << "Timelapse thread interrupted";
    }
    
    //close all
    capture.release();
    recorder.release();
    
    AGO_DEBUG() << "Timelapse stopped: " << timelapse;
}

/**
 * Get empty timelapse map
 */
void AgoSecurity::getEmptyTimelapse(qpid::types::Variant::Map* timelapse)
{
    (*timelapse)["name"] = "";
    (*timelapse)["uri"] = "";
    (*timelapse)["outputFps"] = 0;
    (*timelapse)["outputCodec"] = "";
}

/**
 * Restart timelapses
 */
void AgoSecurity::restartTimelapses()
{
    //stop current timelapses
    stopTimelapses = true;

    //wait for threads stop
    for( std::map<std::string, boost::thread*>::iterator it=timelapseThreads.begin(); it!=timelapseThreads.end(); it++ )
    {
        (it->second)->join();
    }

    //then restart them all
    stopTimelapses = false;
    launchTimelapses();
}

/**
 * Launch all timelapses
 */
void AgoSecurity::launchTimelapses()
{
    qpid::types::Variant::List timelapses = securitymap["timelapses"].asList();
    for( qpid::types::Variant::List::iterator it=timelapses.begin(); it!=timelapses.end(); it++ )
    {
        qpid::types::Variant::Map timelapse = it->asMap();
        launchTimelapse(timelapse);
    }
}

/**
 * Launch specified timelapse
 */
void AgoSecurity::launchTimelapse(qpid::types::Variant::Map& timelapse)
{
    AGO_DEBUG() << "Launch timelapse: " << timelapse;
    boost::thread* thread = new boost::thread(boost::bind(&AgoSecurity::timelapseFunction, this, timelapse));
    string uri = timelapse["uri"].asString();
    timelapseThreads[uri] = thread;
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

        //midnight, create new timelapse for new day
        if( content["hour"].asInt8()==0 && content["minute"].asInt8()==0 )
        {
            restartTimelapses();
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
                pthread_mutex_lock(&securitymapMutex);
                qpid::types::Variant::Map newconfig = content["config"].asMap();
                securitymap["config"] = newconfig;
                securitymap["armedMessage"] = content["armedMessage"].asString();
                securitymap["disarmedMessage"] = content["disarmedMessage"].asString();
                securitymap["defaultHousemode"] = content["defaultHousemode"].asString();
                pthread_mutex_unlock(&securitymapMutex);

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
        else if( content["command"]=="addtimelapse" )
        {
            checkMsgParameter(content, "uri", VAR_STRING);
            checkMsgParameter(content, "name", VAR_STRING);
            checkMsgParameter(content, "fps", VAR_INT32);
            checkMsgParameter(content, "codec", VAR_STRING);

            //check if timelapse already exists or not
            pthread_mutex_lock(&securitymapMutex);
            string uri = content["uri"].asString();
            qpid::types::Variant::List timelapses = securitymap["timelapses"].asList();
            for( qpid::types::Variant::List::iterator it=timelapses.begin(); it!=timelapses.end(); it++ )
            {
                qpid::types::Variant::Map timelapse = it->asMap();
                if( !timelapse["uri"].isVoid() )
                {
                    string timelapseUri = timelapse["uri"].asString();
                    if( timelapseUri==uri )
                    {
                        //uri already exists, stop here
                        pthread_mutex_unlock(&securitymapMutex);
                        return responseSuccess("Timelapse already exists");
                    }
                }
            }

            //fill new timelapse
            qpid::types::Variant::Map timelapse;
            getEmptyTimelapse(&timelapse);
            timelapse["uri"] = content["uri"].asString();
            timelapse["name"] = content["name"].asString();
            timelapse["codec"] = content["codec"].asString();
            timelapse["fps"] = content["fps"].asInt32();

            //and save it
            timelapses.push_back(timelapse);
            securitymap["timelapses"] = timelapses;
            pthread_mutex_unlock(&securitymapMutex);
            if( variantMapToJSONFile(securitymap, getConfigPath(SECURITYMAPFILE)) )
            {
                AGO_DEBUG() << "Command 'addtimelapse': timelapse added " << timelapse;

                //and finally launch timelapse thread
                launchTimelapse(timelapse);

                return responseSuccess("Timelapse added");
            }
            else
            {
                AGO_ERROR() << "Command 'addtimelapse': cannot save securitymap";
                return responseError("error.security.addtimelapse", "Cannot save config");
            }
        }
        else if( content["command"]=="removetimelapse" )
        {
            bool found = false;

            checkMsgParameter(content, "uri", VAR_STRING);
            
            //search and destroy specified timelapse
            pthread_mutex_lock(&securitymapMutex);
            string uri = content["uri"].asString();
            qpid::types::Variant::List timelapses = securitymap["timelapses"].asList();
            for( qpid::types::Variant::List::iterator it=timelapses.begin(); it!=timelapses.end(); it++ )
            {
                qpid::types::Variant::Map timelapse = it->asMap();
                if( !timelapse["uri"].isVoid() )
                {
                    string timelapseUri = timelapse["uri"].asString();
                    if( timelapseUri==uri )
                    {
                        //timelapse found
                        found = true;
                        
                        //stop running timelapse thread
                        timelapseThreads[uri]->interrupt();

                        //and remove it from config
                        timelapses.erase(it);
                        securitymap["timelapses"] = timelapses;
                        break;
                    }
                }
            }
            pthread_mutex_unlock(&securitymapMutex);

            if( found )
            {
                if( variantMapToJSONFile(securitymap, getConfigPath(SECURITYMAPFILE)) )
                {
                    AGO_DEBUG() << "Command 'removetimelapse': timelapse remove";
                    return responseSuccess("Timelapse removed");
                }
                else
                {
                    AGO_ERROR() << "Command 'removetimelapse': cannot save securitymap";
                    return responseError("error.security.removetimelapse", "Cannot save config");
                }
            }
            else
            {
                return responseError("error.security.removetimelapse", "Specified timelapse was not found");
            }
        }
        else if( content["command"]=="gettimelapses" )
        {
            //TODO
        }
        else if( content["command"]=="addmotion" )
        {
            checkMsgParameter(content, "uri", VAR_STRING);
            checkMsgParameter(content, "name", VAR_STRING);
            checkMsgParameter(content, "fps", VAR_INT32);
            checkMsgParameter(content, "codec", VAR_STRING);

            //check if timelapse already exists or not
            pthread_mutex_lock(&securitymapMutex);
            string uri = content["uri"].asString();
            qpid::types::Variant::List timelapses = securitymap["timelapses"].asList();
            for( qpid::types::Variant::List::iterator it=timelapses.begin(); it!=timelapses.end(); it++ )
            {
                qpid::types::Variant::Map timelapse = it->asMap();
                if( !timelapse["uri"].isVoid() )
                {
                    string timelapseUri = timelapse["uri"].asString();
                    if( timelapseUri==uri )
                    {
                        //uri already exists, stop here
                        pthread_mutex_unlock(&securitymapMutex);
                        return responseSuccess("Timelapse already exists");
                    }
                }
            }

            //fill new timelapse
            qpid::types::Variant::Map timelapse;
            getEmptyTimelapse(&timelapse);
            timelapse["uri"] = content["uri"].asString();
            timelapse["name"] = content["name"].asString();
            timelapse["codec"] = content["codec"].asString();
            timelapse["fps"] = content["fps"].asInt32();

            //and save it
            timelapses.push_back(timelapse);
            securitymap["timelapses"] = timelapses;
            pthread_mutex_unlock(&securitymapMutex);
            if( variantMapToJSONFile(securitymap, getConfigPath(SECURITYMAPFILE)) )
            {
                AGO_DEBUG() << "Command 'addtimelapse': timelapse added " << timelapse;

                //and finally launch timelapse thread
                launchTimelapse(timelapse);

                return responseSuccess("Timelapse added");
            }
            else
            {
                AGO_ERROR() << "Command 'addtimelapse': cannot save securitymap";
                return responseError("error.security.addtimelapse", "Cannot save config");
            }
        }
        else if( content["command"]=="removemotion" )
        {
            //TODO
        }
        else if( content["command"]=="getmotions" )
        {
            //TODO
        }
        else if( content["command"]=="getrecordingsconfig" )
        {
            //TODO
        }
        else if( content["command"]=="setrecordingsconfig" )
        {
            //TODO
        }
        else if( content["command"]=="getrecordings" )
        {
            //TODO
        }

        return responseUnknownCommand();
    }

    // We have no devices registered but our own
    throw std::logic_error("Should not go here");
}

void AgoSecurity::setupApp()
{
    //init
    pthread_mutex_init(&alertGatewaysMutex, NULL);
    pthread_mutex_init(&contactsMutex, NULL);
    pthread_mutex_init(&securitymapMutex, NULL);

    //load config
    securitymap = jsonFileToVariantMap(getConfigPath(SECURITYMAPFILE));
    //add missing sections if necessary
    if( securitymap["timelapses"].isVoid() )
    {
        qpid::types::Variant::List timelapses;
        securitymap["timelapses"] = timelapses;
        variantMapToJSONFile(securitymap, getConfigPath(SECURITYMAPFILE));
    }
    AGO_DEBUG() << "Loaded securitymap: " << securitymap;
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

    //launch timelapse threads
    launchTimelapses();
}

void AgoSecurity::cleanupApp()
{
    //stop processes
    stopProcess = true;

    //wait for timelapse threads stop
    for( std::map<std::string, boost::thread*>::iterator it=timelapseThreads.begin(); it!=timelapseThreads.end(); it++ )
    {
        (it->second)->join();
    }
}

AGOAPP_ENTRY_POINT(AgoSecurity);

