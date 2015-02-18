/*
   Copyright (C) 2013 Harald Klein <hari@vt100.at>

   This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

   See the GNU General Public License for more details.

*/

#include <iostream>
#include <sstream>
#include <string.h>

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

#include <limits.h>
#include <float.h>


#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include "agoapp.h"

#include <openzwave/Options.h>
#include <openzwave/Manager.h>
#include <openzwave/Driver.h>
#include <openzwave/Node.h>
#include <openzwave/Group.h>
#include <openzwave/Notification.h>
#include <openzwave/platform/Log.h>
#include <openzwave/value_classes/ValueStore.h>
#include <openzwave/value_classes/Value.h>
#include <openzwave/value_classes/ValueBool.h>

#include "ZWApi.h"
#include "ZWaveNode.h"

#define CONFIG_BASE_PATH "/etc/openzwave/"
#define CONFIG_MANUFACTURER_SPECIFIC CONFIG_BASE_PATH "manufacturer_specific.xml"

using namespace std;
using namespace agocontrol;
using namespace OpenZWave;

static pthread_mutex_t g_criticalSection;
static pthread_cond_t  initCond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t initMutex = PTHREAD_MUTEX_INITIALIZER;

class AgoZwave: public AgoApp {
private:
    bool polling;
    int unitsystem;
    uint32 g_homeId;
    bool g_initFailed;

    typedef struct {
        uint32			m_homeId;
        uint8			m_nodeId;
        bool			m_polled;
        list<ValueID>	m_values;
    } NodeInfo;
    
    list<NodeInfo*> g_nodes;
    map<ValueID, qpid::types::Variant> valueCache;
    ZWaveNodes devices;

    const char *controllerErrorStr(Driver::ControllerError err);
    NodeInfo* getNodeInfo(Notification const* _notification);
    NodeInfo* getNodeInfo(uint32 homeId, uint8 nodeId);
    ValueID* getValueID(int nodeid, int instance, string label);
    qpid::types::Variant::Map getCommandClassConfigurationParameter(OpenZWave::ValueID* valueID);
    qpid::types::Variant::Map getCommandClassWakeUpParameter(OpenZWave::ValueID* valueID);
    bool setCommandClassParameter(uint32 homeId, uint8 nodeId, uint8 commandClassId, uint8 index, string newValue);
    void requestAllNodeConfigParameters();

    qpid::types::Variant::Map commandHandler(qpid::types::Variant::Map content);
    void setupApp();
    void cleanupApp();
    string getHRCommandClassId(uint8_t commandClassId);
public:
    AGOAPP_CONSTRUCTOR_HEAD(AgoZwave)
        , polling(false)
        , unitsystem(0)
        , g_homeId(0)
        , g_initFailed(false)
     //   , initCond(PTHREAD_COND_INITIALIZER)
     //   , initMutex(PTHREAD_MUTEX_INITIALIZER)
        {}


    void _OnNotification (Notification const* _notification);
    void _controller_update(Driver::ControllerState state,  Driver::ControllerError err);
};

void controller_update(Driver::ControllerState state,  Driver::ControllerError err, void *context) {
    AgoZwave *inst = static_cast<AgoZwave*>(context);
    if (inst != NULL) inst->_controller_update(state, err); 
}

void on_notification(Notification const* _notification, void *context) {
    AgoZwave *inst = static_cast<AgoZwave*>(context);
    inst->_OnNotification(_notification);
}

class MyLog : public i_LogImpl {
    virtual void Write( LogLevel _level, uint8 const _nodeId, char const* _format, va_list _args );
    virtual void QueueDump();
    virtual void QueueClear();
    virtual void SetLoggingState(OpenZWave::LogLevel, OpenZWave::LogLevel, OpenZWave::LogLevel);
    virtual void SetLogFileName(const string&);
};

void MyLog::QueueDump() {
}
void MyLog::QueueClear() {
}
void MyLog::SetLoggingState(OpenZWave::LogLevel, OpenZWave::LogLevel, OpenZWave::LogLevel) {
}
void MyLog::SetLogFileName(const string&) {
}
void MyLog::Write( LogLevel _level, uint8 const _nodeId, char const* _format, va_list _args ) {
    char lineBuf[1024] = {};
    if( _format != NULL && _format[0] != '\0' )
    {
        va_list saveargs;
        va_copy( saveargs, _args );
        vsnprintf( lineBuf, sizeof(lineBuf), _format, _args );
        va_end( saveargs );
    }
    if (_level == LogLevel_StreamDetail) AGO_TRACE() << "OZW " <<string(lineBuf);
    else if (_level == LogLevel_Debug) AGO_DEBUG() << "OZW " << string(lineBuf);
    else if (_level == LogLevel_Detail) AGO_DEBUG() << "OZW " << string(lineBuf);
    else if (_level == LogLevel_Info) AGO_INFO() << "OZW " << string(lineBuf);
    else if (_level == LogLevel_Alert) AGO_WARNING() << "OZW " << string(lineBuf);
    else if (_level == LogLevel_Warning) AGO_WARNING() << "OZW " << string(lineBuf);
    else if (_level == LogLevel_Error) AGO_ERROR() << "OZW " << string(lineBuf);
    else if (_level == LogLevel_Fatal) AGO_FATAL() << "OZW " << string(lineBuf);
    else if (_level == LogLevel_Always) AGO_FATAL() << "OZW " << string(lineBuf);
    else AGO_FATAL() << string(lineBuf);
}

const char *AgoZwave::controllerErrorStr (Driver::ControllerError err) {
    switch (err) {
        case Driver::ControllerError_None:
            return "None";
        case Driver::ControllerError_ButtonNotFound:
            return "Button Not Found";
        case Driver::ControllerError_NodeNotFound:
            return "Node Not Found";
        case Driver::ControllerError_NotBridge:
            return "Not a Bridge";
        case Driver::ControllerError_NotPrimary:
            return "Not Primary Controller";
        case Driver::ControllerError_IsPrimary:
            return "Is Primary Controller";
        case Driver::ControllerError_NotSUC:
            return "Not Static Update Controller";
        case Driver::ControllerError_NotSecondary:
            return "Not Secondary Controller";
        case Driver::ControllerError_NotFound:
            return "Not Found";
        case Driver::ControllerError_Busy:
            return "Controller Busy";
        case Driver::ControllerError_Failed:
            return "Failed";
        case Driver::ControllerError_Disabled:
            return "Disabled";
        case Driver::ControllerError_Overflow:
            return "Overflow";
        default:
            return "Unknown error";
    }
}

void AgoZwave::_controller_update(Driver::ControllerState state,  Driver::ControllerError err) {
    qpid::types::Variant::Map eventmap;
    eventmap["statecode"]=state;
    switch(state) {
        case Driver::ControllerState_Normal:
            AGO_INFO() << "controller state update: no command in progress";
            eventmap["state"]="normal";
            eventmap["description"]="Normal: No command in progress";
            agoConnection->emitEvent("zwavecontroller", "event.zwave.controllerstate", eventmap);
            // nothing to do
            break;
        case Driver::ControllerState_Waiting:
            AGO_INFO() << "controller state update: waiting for user action";
            eventmap["state"]="awaitaction";
            eventmap["description"]="Waiting for user action";
            agoConnection->emitEvent("zwavecontroller", "event.zwave.controllerstate", eventmap);
            // waiting for user action
            break;
        case Driver::ControllerState_Cancel:
            AGO_INFO() << "controller state update: command was cancelled";
            eventmap["state"]="cancel";
            eventmap["description"]="Command was cancelled";
            agoConnection->emitEvent("zwavecontroller", "event.zwave.controllerstate", eventmap);
            break;
        case Driver::ControllerState_Error:
            AGO_ERROR() << "controller state update: command returned error";
            eventmap["state"]="error";
            eventmap["description"]="Command returned error";
            eventmap["error"] = err;
            eventmap["errorstring"] = controllerErrorStr(err);
            agoConnection->emitEvent("zwavecontroller", "event.zwave.controllerstate", eventmap);
            break;
        case Driver::ControllerState_Sleeping:
            AGO_INFO() << "controller state update: device went to sleep";
            eventmap["state"]="sleep";
            eventmap["description"]="Device went to sleep";
            agoConnection->emitEvent("zwavecontroller", "event.zwave.controllerstate", eventmap);
            break;

        case Driver::ControllerState_InProgress:
            AGO_INFO() << "controller state update: communicating with other device";
            eventmap["state"]="inprogress";
            eventmap["description"]="Communication in progress";
            agoConnection->emitEvent("zwavecontroller", "event.zwave.controllerstate", eventmap);
            // communicating with device
            break;
        case Driver::ControllerState_Completed:
            AGO_INFO() << "controller state update: command has completed successfully";
            eventmap["state"]="success";
            eventmap["description"]="Command completed";
            agoConnection->emitEvent("zwavecontroller", "event.zwave.controllerstate", eventmap);
            break;
        case Driver::ControllerState_Failed:
            AGO_ERROR() << "controller state update: command has failed";
            eventmap["state"]="failed";
            eventmap["description"]="Command failed";
            agoConnection->emitEvent("zwavecontroller", "event.zwave.controllerstate", eventmap);
            // houston..
            break;
        case Driver::ControllerState_NodeOK:
            AGO_INFO() << "controller state update: node ok";
            eventmap["state"]="nodeok";
            eventmap["description"]="Node OK";
            agoConnection->emitEvent("zwavecontroller", "event.zwave.controllerstate", eventmap);
            break;
        case Driver::ControllerState_NodeFailed:
            AGO_ERROR() << "controller state update: node failed";
            eventmap["state"]="nodefailed";
            eventmap["description"]="Node failed";
            agoConnection->emitEvent("zwavecontroller", "event.zwave.controllerstate", eventmap);
            break;
        default:
            AGO_INFO() << "controller state update: unknown response";
            eventmap["state"]="unknown";
            eventmap["description"]="Unknown response";
            agoConnection->emitEvent("zwavecontroller", "event.zwave.controllerstate", eventmap);
            break;
    }
    if (err != Driver::ControllerError_None)  {
        AGO_ERROR() << "Controller error: " << controllerErrorStr(err);
    }
}

/**
 * Return Human Readable CommandClassId
 */
string AgoZwave::getHRCommandClassId(uint8_t commandClassId)
{
    string output;
    switch(commandClassId)
    {
        case COMMAND_CLASS_MARK:
            output="COMMAND_CLASS_MARK";
            break;
        case COMMAND_CLASS_BASIC:
            output="COMMAND_CLASS_BASIC";
            break;
        case COMMAND_CLASS_VERSION:
            output="COMMAND_CLASS_VERSION";
            break;
        case COMMAND_CLASS_BATTERY:
            output="COMMAND_CLASS_BATTERY";
            break;
        case COMMAND_CLASS_WAKE_UP:
            output="COMMAND_CLASS_WAKE_UP";
            break;
        case COMMAND_CLASS_CONTROLLER_REPLICATION:
            output="COMMAND_CLASS_CONTROLLER_REPLICATION";
            break;
        case COMMAND_CLASS_SWITCH_MULTILEVEL:
            output="COMMAND_CLASS_SWITCH_MULTILEVEL";
            break;
        case COMMAND_CLASS_SWITCH_ALL:
            output="COMMAND_CLASS_SWITCH_ALL";
            break;
        case COMMAND_CLASS_SENSOR_BINARY:
            output="COMMAND_CLASS_SENSOR_BINARY";
            break;
        case COMMAND_CLASS_SENSOR_MULTILEVEL:
            output="COMMAND_CLASS_SENSOR_MULTILEVEL";
            break;
        case COMMAND_CLASS_SENSOR_ALARM:
            output="COMMAND_CLASS_SENSOR_ALARM";
            break;
        case COMMAND_CLASS_ALARM:
            output="COMMAND_CLASS_ALARM";
            break;
        case COMMAND_CLASS_MULTI_CMD:
            output="COMMAND_CLASS_MULTI_CMD";
            break;
        case COMMAND_CLASS_CLIMATE_CONTROL_SCHEDULE:
            output="COMMAND_CLASS_CLIMATE_CONTROL_SCHEDULE";
            break;
        case COMMAND_CLASS_CLOCK:
            output="COMMAND_CLASS_CLOCK";
            break;
        case COMMAND_CLASS_ASSOCIATION:
            output="COMMAND_CLASS_ASSOCIATION";
            break;
        case COMMAND_CLASS_CONFIGURATION:
            output="COMMAND_CLASS_CONFIGURATION";
            break;
        case COMMAND_CLASS_MANUFACTURER_SPECIFIC:
            output="COMMAND_CLASS_MANUFACTURER_SPECIFIC";
            break;
        case COMMAND_CLASS_APPLICATION_STATUS:
            output="COMMAND_CLASS_APPLICATION_STATUS";
            break;
        case COMMAND_CLASS_ASSOCIATION_COMMAND_CONFIGURATION:
            output="COMMAND_CLASS_ASSOCIATION_COMMAND_CONFIGURATION";
            break;
        case COMMAND_CLASS_AV_CONTENT_DIRECTORY_MD:
            output="COMMAND_CLASS_AV_CONTENT_DIRECTORY_MD";
            break;
        case COMMAND_CLASS_AV_CONTENT_SEARCH_MD:
            output="COMMAND_CLASS_AV_CONTENT_SEARCH_MD";
            break;
        case COMMAND_CLASS_AV_RENDERER_STATUS:
            output="COMMAND_CLASS_AV_RENDERER_STATUS";
            break;
        case COMMAND_CLASS_AV_TAGGING_MD:
            output="COMMAND_CLASS_AV_TAGGING_MD";
            break;
        case COMMAND_CLASS_BASIC_WINDOW_COVERING:
            output="COMMAND_CLASS_BASIC_WINDOW_COVERING";
            break;
        case COMMAND_CLASS_CHIMNEY_FAN:
            output="COMMAND_CLASS_CHIMNEY_FAN";
            break;
        case COMMAND_CLASS_COMPOSITE:
            output="COMMAND_CLASS_COMPOSITE";
            break;
        case COMMAND_CLASS_DOOR_LOCK:
            output="COMMAND_CLASS_DOOR_LOCK";
            break;
        case COMMAND_CLASS_ENERGY_PRODUCTION:
            output="COMMAND_CLASS_ENERGY_PRODUCTION";
            break;
        case COMMAND_CLASS_FIRMWARE_UPDATE_MD:
            output="COMMAND_CLASS_FIRMWARE_UPDATE_MD";
            break;
        case COMMAND_CLASS_GEOGRAPHIC_LOCATION:
            output="COMMAND_CLASS_GEOGRAPHIC_LOCATION";
            break;
        case COMMAND_CLASS_GROUPING_NAME:
            output="COMMAND_CLASS_GROUPING_NAME";
            break;
        case COMMAND_CLASS_HAIL:
            output="COMMAND_CLASS_HAIL";
            break;
        case COMMAND_CLASS_INDICATOR:
            output="COMMAND_CLASS_INDICATOR";
            break;
        case COMMAND_CLASS_IP_CONFIGURATION:
            output="COMMAND_CLASS_IP_CONFIGURATION";
            break;
        case COMMAND_CLASS_LANGUAGE:
            output="COMMAND_CLASS_LANGUAGE";
            break;
        case COMMAND_CLASS_LOCK:
            output="COMMAND_CLASS_LOCK";
            break;
        case COMMAND_CLASS_MANUFACTURER_PROPRIETARY:
            output="COMMAND_CLASS_MANUFACTURER_PROPRIETARY";
            break;
        case COMMAND_CLASS_METER_PULSE:
            output="COMMAND_CLASS_METER_PULSE";
            break;
        case COMMAND_CLASS_METER:
            output="COMMAND_CLASS_METER";
            break;
        case COMMAND_CLASS_MTP_WINDOW_COVERING:
            output="COMMAND_CLASS_MTP_WINDOW_COVERING";
            break;
        case COMMAND_CLASS_MULTI_INSTANCE_ASSOCIATION:
            output="COMMAND_CLASS_MULTI_INSTANCE_ASSOCIATION";
            break;
        case COMMAND_CLASS_MULTI_INSTANCE:
            output="COMMAND_CLASS_MULTI_INSTANCE";
            break;
        case COMMAND_CLASS_NO_OPERATION:
            output="COMMAND_CLASS_NO_OPERATION";
            break;
        case COMMAND_CLASS_NODE_NAMING:
            output="COMMAND_CLASS_NODE_NAMING";
            break;
        case COMMAND_CLASS_NON_INTEROPERABLE:
            output="COMMAND_CLASS_NON_INTEROPERABLE";
            break;
        case COMMAND_CLASS_POWERLEVEL:
            output="COMMAND_CLASS_POWERLEVEL";
            break;
        case COMMAND_CLASS_PROPRIETARY:
            output="COMMAND_CLASS_PROPRIETARY";
            break;
        case COMMAND_CLASS_PROTECTION:
            output="COMMAND_CLASS_PROTECTION";
            break;
        case COMMAND_CLASS_REMOTE_ASSOCIATION_ACTIVATE:
            output="COMMAND_CLASS_REMOTE_ASSOCIATION_ACTIVATE";
            break;
        case COMMAND_CLASS_REMOTE_ASSOCIATION:
            output="COMMAND_CLASS_REMOTE_ASSOCIATION";
            break;
        case COMMAND_CLASS_SCENE_ACTIVATION:
            output="COMMAND_CLASS_SCENE_ACTIVATION";
            break;
        case COMMAND_CLASS_SCENE_ACTUATOR_CONF:
            output="COMMAND_CLASS_SCENE_ACTUATOR_CONF";
            break;
        case COMMAND_CLASS_SCENE_CONTROLLER_CONF:
            output="COMMAND_CLASS_SCENE_CONTROLLER_CONF";
            break;
        case COMMAND_CLASS_SCREEN_ATTRIBUTES:
            output="COMMAND_CLASS_SCREEN_ATTRIBUTES";
            break;
        case COMMAND_CLASS_SCREEN_MD:
            output="COMMAND_CLASS_SCREEN_MD";
            break;
        case COMMAND_CLASS_SECURITY:
            output="COMMAND_CLASS_SECURITY";
            break;
        case COMMAND_CLASS_SENSOR_CONFIGURATION:
            output="COMMAND_CLASS_SENSOR_CONFIGURATION";
            break;
        case COMMAND_CLASS_SILENCE_ALARM:
            output="COMMAND_CLASS_SILENCE_ALARM";
            break;
        case COMMAND_CLASS_SIMPLE_AV_CONTROL:
            output="COMMAND_CLASS_SIMPLE_AV_CONTROL";
            break;
        case COMMAND_CLASS_SWITCH_BINARY:
            output="COMMAND_CLASS_SWITCH_BINARY";
            break;
        case COMMAND_CLASS_SWITCH_TOGGLE_BINARY:
            output="COMMAND_CLASS_SWITCH_TOGGLE_BINARY";
            break;
        case COMMAND_CLASS_SWITCH_TOGGLE_MULTILEVEL:
            output="COMMAND_CLASS_SWITCH_TOGGLE_MULTILEVEL";
            break;
        case COMMAND_CLASS_THERMOSTAT_FAN_MODE:
            output="COMMAND_CLASS_THERMOSTAT_FAN_MODE";
            break;
        case COMMAND_CLASS_THERMOSTAT_FAN_STATE:
            output="COMMAND_CLASS_THERMOSTAT_FAN_STATE";
            break;
        case COMMAND_CLASS_THERMOSTAT_HEATING:
            output="COMMAND_CLASS_THERMOSTAT_HEATING";
            break;
        case COMMAND_CLASS_THERMOSTAT_MODE:
            output="COMMAND_CLASS_THERMOSTAT_MODE";
            break;
        case COMMAND_CLASS_THERMOSTAT_OPERATING_STATE:
            output="COMMAND_CLASS_THERMOSTAT_OPERATING_STATE";
            break;
        case COMMAND_CLASS_THERMOSTAT_SETBACK:
            output="COMMAND_CLASS_THERMOSTAT_SETBACK";
            break;
        case COMMAND_CLASS_THERMOSTAT_SETPOINT:
            output="COMMAND_CLASS_THERMOSTAT_SETPOINT";
            break;
        case COMMAND_CLASS_TIME_PARAMETERS:
            output="COMMAND_CLASS_TIME_PARAMETERS";
            break;
        case COMMAND_CLASS_TIME:
            output="COMMAND_CLASS_TIME";
            break;
        case COMMAND_CLASS_USER_CODE:
            output="COMMAND_CLASS_USER_CODE";
            break;
        case COMMAND_CLASS_ZIP_ADV_CLIENT:
            output="COMMAND_CLASS_ZIP_ADV_CLIENT";
            break;
        case COMMAND_CLASS_ZIP_ADV_SERVER:
            output="COMMAND_CLASS_ZIP_ADV_SERVER";
            break;
        case COMMAND_CLASS_ZIP_ADV_SERVICES:
            output="COMMAND_CLASS_ZIP_ADV_SERVICES";
            break;
        case COMMAND_CLASS_ZIP_CLIENT:
            output="COMMAND_CLASS_ZIP_CLIENT";
            break;
        case COMMAND_CLASS_ZIP_SERVER:
            output="COMMAND_CLASS_ZIP_SERVER";
            break;
        case COMMAND_CLASS_ZIP_SERVICES:
            output="COMMAND_CLASS_ZIP_SERVICES";
            break;
    }
    return output;
}


//-----------------------------------------------------------------------------
// <getNodeInfo>
// Callback that is triggered when a value, group or node changes
//-----------------------------------------------------------------------------
AgoZwave::NodeInfo* AgoZwave::getNodeInfo (Notification const* _notification)
{
    uint32 const homeId = _notification->GetHomeId();
    uint8 const nodeId = _notification->GetNodeId();
    for( list<NodeInfo*>::iterator it = g_nodes.begin(); it != g_nodes.end(); ++it )
    {
        NodeInfo* nodeInfo = *it;
        if( ( nodeInfo->m_homeId == homeId ) && ( nodeInfo->m_nodeId == nodeId ) )
        {
            return nodeInfo;
        }
    }

    return NULL;
}

AgoZwave::NodeInfo* AgoZwave::getNodeInfo(uint32 homeId, uint8 nodeId)
{
    NodeInfo* result = NULL;

    for( list<NodeInfo*>::iterator it = g_nodes.begin(); it != g_nodes.end(); ++it )
    {
        NodeInfo* nodeInfo = *it;
        if( nodeInfo->m_homeId==homeId && nodeInfo->m_nodeId==nodeId )
        {
            result = nodeInfo;
            break;
        }
    }

    return result;
}


ValueID* AgoZwave::getValueID(int nodeid, int instance, string label)
{
    for( list<NodeInfo*>::iterator it = g_nodes.begin(); it != g_nodes.end(); ++it )
    {
        for (list<ValueID>::iterator it2 = (*it)->m_values.begin(); it2 != (*it)->m_values.end(); it2++ )
        {
            if ( ((*it)->m_nodeId == nodeid) && ((*it2).GetInstance() == instance) )
            {
                string valuelabel = Manager::Get()->GetValueLabel((*it2));
                if (label == valuelabel)
                {
                    // AGO_TRACE() << "Found ValueID: " << (*it2).GetId();
                    return &(*it2);
                }
            }
        }
    }
    return NULL;
}

/**
 * Get COMMAND_CLASS_CONFIGURATION parameter
 * @return map containing all parameter infos
 */
qpid::types::Variant::Map AgoZwave::getCommandClassConfigurationParameter(OpenZWave::ValueID* valueID)
{
    //init
    qpid::types::Variant::Map param;
    if( valueID==NULL )
    {
        return param;
    }

    //add invalid parameter property
    //give info to ui that parameter is invalid
    param["invalid"] = false;

    //get parameter type
    vector<string> ozwItems;
    qpid::types::Variant::List items;
    switch( valueID->GetType() )
    {
        case ValueID::ValueType_Decimal:
            //Represents a non-integer value as a string, to avoid floating point accuracy issues. 
            param["type"] = "float";
            break;
        case ValueID::ValueType_Bool:
            //Boolean, true or false 
            param["type"] = "bool";
            break;
        case ValueID::ValueType_Byte:
            //8-bit unsigned value 
            param["type"] = "byte";
            break;
        case ValueID::ValueType_Short:
            //16-bit signed value 
            param["type"] = "short";
            break;
        case ValueID::ValueType_Int:
            //32-bit signed value 
            param["type"] = "int";
            break;
        case ValueID::ValueType_Button:
            //A write-only value that is the equivalent of pressing a button to send a command to a device
            param["type"] = "button";
            param["invalid"] = true;
            break;
        case ValueID::ValueType_List:
            //List from which one item can be selected 
            param["type"] = "list";

            //get list items
            Manager::Get()->GetValueListItems(*valueID, &ozwItems);
            for( vector<string>::iterator it3=ozwItems.begin(); it3!=ozwItems.end(); it3++ )
            {
                items.push_back(*it3);
            }
            param["items"] = items;
            break;
        case ValueID::ValueType_Schedule:
            //Complex type used with the Climate Control Schedule command class
            param["type"] = "notsupported";
            param["invalid"] = true;
            break;
        case ValueID::ValueType_String:
            //Text string
            param["type"] = "string";
            break;
        case ValueID::ValueType_Raw:
            //A collection of bytes 
            param["type"] = "notsupported";
            param["invalid"] = true;
            break;
        default:
            param["invalid"] = true;
            param["type"] = "unknown";
    }

    //get parameter value
    string currentValue = "";
    if( Manager::Get()->GetValueAsString(*valueID, &currentValue) )
    {
        param["currentvalue"] = currentValue;
    }
    else
    {
        //unable to get current value
        param["invalid"] = true;
        param["currentvalue"] = "";
    }

    //get other infos
    param["label"] = Manager::Get()->GetValueLabel(*valueID);
    param["units"] = Manager::Get()->GetValueUnits(*valueID);
    param["help"] = Manager::Get()->GetValueHelp(*valueID);
    param["min"] = Manager::Get()->GetValueMin(*valueID);
    param["max"] = Manager::Get()->GetValueMax(*valueID);
    param["readonly"] = Manager::Get()->IsValueReadOnly(*valueID);
    param["commandclassid"] = COMMAND_CLASS_CONFIGURATION;
    param["instance"] = valueID->GetInstance();
    param["index"] = valueID->GetIndex();


    return param;
}

/**
 * Get COMMAND_CLASS_WAKE_UP parameter
 * @return map containing all parameter infos
 */
qpid::types::Variant::Map AgoZwave::getCommandClassWakeUpParameter(OpenZWave::ValueID* valueID)
{
    qpid::types::Variant::Map param;
    string currentValue = "";

    if( Manager::Get()->GetValueAsString(*valueID, &currentValue) )
    {
        //everything's good, fill param
        param["currentvalue"] = currentValue;
        param["type"] = "int";
        param["label"] = Manager::Get()->GetValueLabel(*valueID);
        param["units"] = Manager::Get()->GetValueUnits(*valueID);
        param["help"] = Manager::Get()->GetValueHelp(*valueID);
        param["readonly"] = Manager::Get()->IsValueReadOnly(*valueID);
        param["min"] = Manager::Get()->GetValueMin(*valueID);
        param["max"] = Manager::Get()->GetValueMax(*valueID);
        param["invalid"] = 0;
        param["commandclassid"] = COMMAND_CLASS_WAKE_UP;
        param["instance"] = valueID->GetInstance();
        param["index"] = valueID->GetIndex();
    }

    return param;
}

/**
 * Set device parameter
 * @return true if parameter found and set successfully
 */
bool AgoZwave::setCommandClassParameter(uint32 homeId, uint8 nodeId, uint8 commandClassId, uint8 index, string newValue)
{
    bool result = false;

    //get node info
    NodeInfo* nodeInfo = getNodeInfo(homeId, nodeId);

    if( nodeInfo!=NULL )
    {
        for( list<ValueID>::iterator it=nodeInfo->m_values.begin(); it!=nodeInfo->m_values.end(); it++ )
        {
            AGO_DEBUG() << "--> commandClassId=" << getHRCommandClassId(it->GetCommandClassId()) << " index=" << std::dec << (int)it->GetIndex();
            if( it->GetCommandClassId()==commandClassId && it->GetIndex()==index )
            {
                //parameter found

                //check if value has been modified
                string oldValue;
                Manager::Get()->GetValueAsString(*it, &oldValue);

                if( oldValue!=newValue )
                {                  
                    if( it->GetType()==ValueID::ValueType_List )
                    {
                        //set list value
                        result = Manager::Get()->SetValueListSelection(*it, newValue);
                        if( !result )
                        {
                            AGO_ERROR() << "Unable to set value list selection with newValue=" << newValue;
                        }
                    }
                    else
                    {
                        //set value
                        result = Manager::Get()->SetValue(*it, newValue);
                        if( !result )
                        {
                            AGO_ERROR() << "Unable to set value with newValue=" << newValue;
                        }
                    }
                }
                else
                {
                    //oldValue == newValue. Nothing done but return true anyway
                    AGO_DEBUG() << "setCommandClassParameter: try to set parameter with same value [" << oldValue << "=" << newValue << "]. Nothing done.";
                    result = true;
                }

                //stop statement
                break;
            }
        }
    }

    //handle error
    if( result==false )
    {
        if( nodeInfo==NULL )
        {
            //node not found!
            AGO_ERROR() << "Node not found! (homeId=" << homeId << " nodeId=" << nodeId << " commandClassId=" << commandClassId << " index=" << index << ")";
        }
        else
        {
            //ValueID not found!
            AGO_ERROR() << "ValueID not found! (homeId=" << homeId << " nodeId=" << nodeId << " commandClassId=" << commandClassId << " index=" << index << ")";
        }
    }

    return result;
}

/**
 * Request config parameters for all nodes
 */
void AgoZwave::requestAllNodeConfigParameters()
{
    for( list<NodeInfo*>::iterator it=g_nodes.begin(); it!=g_nodes.end(); ++it )
    {
        NodeInfo* nodeInfo = *it;
        Manager::Get()->RequestAllConfigParams(nodeInfo->m_homeId, nodeInfo->m_nodeId);
    }
}


//-----------------------------------------------------------------------------
// <OnNotification>
// Callback that is triggered when a value, group or node changes
//-----------------------------------------------------------------------------
void AgoZwave::_OnNotification (Notification const* _notification)
{
    qpid::types::Variant::Map eventmap;
    // Must do this inside a critical section to avoid conflicts with the main thread
    pthread_mutex_lock( &g_criticalSection );

    AGO_DEBUG() << "Notification " << _notification->GetType() << " received";

    switch( _notification->GetType() )
    {
        case Notification::Type_ValueAdded:
        {
            if( NodeInfo* nodeInfo = getNodeInfo( _notification ) )
            {
                // Add the new value to our list
                nodeInfo->m_values.push_back( _notification->GetValueID() );
                uint8 basic = Manager::Get()->GetNodeBasic(_notification->GetHomeId(),_notification->GetNodeId());
                uint8 generic = Manager::Get()->GetNodeGeneric(_notification->GetHomeId(),_notification->GetNodeId());
                /*uint8 specific =*/ Manager::Get()->GetNodeSpecific(_notification->GetHomeId(),_notification->GetNodeId());
                ValueID id = _notification->GetValueID();
                string label = Manager::Get()->GetValueLabel(id);
                stringstream tempstream;
                tempstream << (int) _notification->GetNodeId();
                tempstream << "/";
                tempstream << (int) id.GetInstance();
                string nodeinstance = tempstream.str();
                tempstream << "-";
                tempstream << label;
                string tempstring = tempstream.str();
                ZWaveNode *device;

                if (id.GetGenre() == ValueID::ValueGenre_Config)
                {
                    AGO_INFO() << "Configuration parameter Value Added: Home=" << std::hex <<  _notification->GetHomeId() << " Node=" << std::dec << (int) _notification->GetNodeId() << " Genre=" << std::dec << (int) id.GetGenre() << " Class=" << getHRCommandClassId(id.GetCommandClassId())  << " Instance=" << (int)id.GetInstance() << " Index=" << (int)id.GetIndex() << " Type=" << (int)id.GetType() << " Label=" << label;
                }
                else if (basic == BASIC_TYPE_CONTROLLER)
                {
                    if ((device = devices.findId(nodeinstance)) != NULL)
                    {
                        device->addValue(label, id);
                        device->setDevicetype("remote");
                    }
                    else
                    {
                        device = new ZWaveNode(nodeinstance, "remote");	
                        device->addValue(label, id);
                        devices.add(device);
                        agoConnection->addDevice(device->getId().c_str(), device->getDevicetype().c_str());
                    }
                }
                else
                {
                    switch (generic)
                    {
                        case GENERIC_TYPE_THERMOSTAT:
                            if ((device = devices.findId(nodeinstance)) == NULL)
                            {
                                device = new ZWaveNode(nodeinstance, "thermostat");
                                devices.add(device);
                                agoConnection->addDevice(device->getId().c_str(), device->getDevicetype().c_str());
                            }
                            break;
                        case GENERIC_TYPE_SWITCH_MULTILEVEL:
                            if ((device = devices.findId(nodeinstance)) == NULL)
                            {
                                device = new ZWaveNode(nodeinstance, "dimmer");
                                devices.add(device);
                                agoConnection->addDevice(device->getId().c_str(), device->getDevicetype().c_str());
                            }
                            break;
                    }

                    switch(id.GetCommandClassId())
                    {
                        case COMMAND_CLASS_CONFIGURATION:
                            AGO_DEBUG() << "COMMAND_CLASS_CONFIGURATION received";
                            break;
                        case COMMAND_CLASS_BATTERY:
                            if( label=="Battery Level" )
                            {
                                if ((device = devices.findId(tempstring)) != NULL)
                                {
                                    AGO_DEBUG() << "Battery: append batterysensor [" << label << "]";
                                    device->addValue(label, id);
                                }
                                else
                                {
                                    device = new ZWaveNode(tempstring, "batterysensor");
                                    device->addValue(label, id);
                                    devices.add(device);
                                    AGO_DEBUG() << "Battery: add new batterysensor [" << device->getId() << ", " << device->getDevicetype() << "]";
                                    agoConnection->addDevice(device->getId().c_str(), device->getDevicetype().c_str());
                                }
                            }
                            break;
                        case COMMAND_CLASS_SWITCH_MULTILEVEL:
                            if (label == "Level")
                            {
                                if ((device = devices.findId(nodeinstance)) != NULL)
                                {
                                    device->addValue(label, id);
                                    // device->setDevicetype("dimmer");
                                }
                                else
                                {
                                    device = new ZWaveNode(nodeinstance, "dimmer");	
                                    device->addValue(label, id);
                                    devices.add(device);
                                    agoConnection->addDevice(device->getId().c_str(), device->getDevicetype().c_str());
                                }
                                // Manager::Get()->EnablePoll(id);
                            }
                            break;
                        case COMMAND_CLASS_SWITCH_BINARY:
                            if (label == "Switch")
                            {
                                if ((device = devices.findId(nodeinstance)) != NULL)
                                {
                                    device->addValue(label, id);
                                }
                                else
                                {
                                    device = new ZWaveNode(nodeinstance, "switch");	
                                    device->addValue(label, id);
                                    devices.add(device);
                                    agoConnection->addDevice(device->getId().c_str(), device->getDevicetype().c_str());
                                }
                                // Manager::Get()->EnablePoll(id);
                            }
                            break;
                        case COMMAND_CLASS_SENSOR_BINARY:
                            if (label == "Sensor")
                            {
                                if ((device = devices.findId(tempstring)) != NULL)
                                {
                                    device->addValue(label, id);
                                }
                                else
                                {
                                    device = new ZWaveNode(tempstring, "binarysensor");	
                                    device->addValue(label, id);
                                    devices.add(device);
                                    agoConnection->addDevice(device->getId().c_str(), device->getDevicetype().c_str());
                                }
                                // Manager::Get()->EnablePoll(id);
                            }
                            break;
                        case COMMAND_CLASS_SENSOR_MULTILEVEL:
                            if (label == "Luminance")
                            {
                                device = new ZWaveNode(tempstring, "brightnesssensor");	
                                device->addValue(label, id);
                                devices.add(device);
                                agoConnection->addDevice(device->getId().c_str(), device->getDevicetype().c_str());
                            }
                            else if (label == "Temperature")
                            {
                                if (generic == GENERIC_TYPE_THERMOSTAT)
                                {
                                    if ((device = devices.findId(nodeinstance)) != NULL)
                                    {
                                        device->addValue(label, id);
                                    }
                                    else
                                    {
                                        device = new ZWaveNode(nodeinstance, "thermostat");	
                                        device->addValue(label, id);
                                        devices.add(device);
                                        agoConnection->addDevice(device->getId().c_str(), device->getDevicetype().c_str());
                                    }
                                }
                                else
                                {
                                    device = new ZWaveNode(tempstring, "temperaturesensor");	
                                    device->addValue(label, id);
                                    devices.add(device);
                                    agoConnection->addDevice(device->getId().c_str(), device->getDevicetype().c_str());
                                }
                            }
                            else
                            {
                                AGO_WARNING() << "unhandled label for SENSOR_MULTILEVEL. Adding generic multilevelsensor for label: " << label;
                                if ((device = devices.findId(nodeinstance)) != NULL)
                                {
                                    device->addValue(label, id);
                                }
                                else
                                {
                                    device = new ZWaveNode(nodeinstance, "multilevelsensor");	
                                    device->addValue(label, id);
                                    devices.add(device);
                                    agoConnection->addDevice(device->getId().c_str(), device->getDevicetype().c_str());
                                }
                            }
                            // Manager::Get()->EnablePoll(id);
                            break;
                        case COMMAND_CLASS_METER:
                            if (label == "Power")
                            {
                                device = new ZWaveNode(tempstring, "powermeter");	
                                device->addValue(label, id);
                                devices.add(device);
                                agoConnection->addDevice(device->getId().c_str(), device->getDevicetype().c_str());
                            }
                            else if (label == "Energy")
                            {
                                device = new ZWaveNode(tempstring, "energymeter");	
                                device->addValue(label, id);
                                devices.add(device);
                                agoConnection->addDevice(device->getId().c_str(), device->getDevicetype().c_str());
                            }
                            else
                            {
                                AGO_WARNING() << "unhandled label for CLASS_METER. Adding generic multilevelsensor for label: " << label;
                                if ((device = devices.findId(nodeinstance)) != NULL)
                                {
                                    device->addValue(label, id);
                                }
                                else
                                {
                                    device = new ZWaveNode(nodeinstance, "multilevelsensor");	
                                    device->addValue(label, id);
                                    devices.add(device);
                                    agoConnection->addDevice(device->getId().c_str(), device->getDevicetype().c_str());
                                }
                            }
                            // Manager::Get()->EnablePoll(id);
                            break;
                        case COMMAND_CLASS_BASIC_WINDOW_COVERING:
                            // if (label == "Open") {
                            if ((device = devices.findId(nodeinstance)) != NULL)
                            {
                                device->addValue(label, id);
                                device->setDevicetype("drapes");
                            }
                            else
                            {
                                device = new ZWaveNode(nodeinstance, "drapes");	
                                device->addValue(label, id);
                                devices.add(device);
                                agoConnection->addDevice(device->getId().c_str(), device->getDevicetype().c_str());
                            }
                            // Manager::Get()->EnablePoll(id);
                            //	}
                            break;
                        case COMMAND_CLASS_THERMOSTAT_SETPOINT:
                            if (polling)
                            {
                                Manager::Get()->EnablePoll(id,1);
                            }
                        case COMMAND_CLASS_THERMOSTAT_MODE:
                        case COMMAND_CLASS_THERMOSTAT_FAN_MODE:
                        case COMMAND_CLASS_THERMOSTAT_FAN_STATE:
                        case COMMAND_CLASS_THERMOSTAT_OPERATING_STATE:
                            AGO_DEBUG() << "adding thermostat label: " << label;
                            if ((device = devices.findId(nodeinstance)) != NULL)
                            {
                                device->addValue(label, id);
                                device->setDevicetype("thermostat");
                            }
                            else
                            {
                                device = new ZWaveNode(nodeinstance, "thermostat");	
                                device->addValue(label, id);
                                devices.add(device);
                                agoConnection->addDevice(device->getId().c_str(), device->getDevicetype().c_str());
                            }
                            break;
                        case COMMAND_CLASS_SENSOR_ALARM:
                            if (label == "Sensor" || label == "Alarm Level" || label == "Flood")
                            {
                                if ((device = devices.findId(tempstring)) != NULL)
                                {
                                    device->addValue(label, id);
                                }
                                else
                                {
                                    device = new ZWaveNode(tempstring, "binarysensor");	
                                    device->addValue(label, id);
                                    devices.add(device);
                                    agoConnection->addDevice(device->getId().c_str(), device->getDevicetype().c_str());
                                }
                                // Manager::Get()->EnablePoll(id);
                            }
                            break;
                        default:
                            AGO_INFO() << "Notification: Unassigned Value Added Home=" << std::hex << _notification->GetHomeId() << " Node=" << std::dec << (int)_notification->GetNodeId() << " Genre=" << std::dec << (int)id.GetGenre() << " Class=" << getHRCommandClassId(id.GetCommandClassId()) << " Instance=" << std::dec << (int)id.GetInstance() << " Index=" << std::dec << (int)id.GetIndex() << " Type=" << (int)id.GetType() << " Label: " << label;

                    }
                }
            }
            break;
        }
        case Notification::Type_ValueRemoved:
        {
            if( NodeInfo* nodeInfo = getNodeInfo( _notification ) )
            {
                // Remove the value from out list
                for( list<ValueID>::iterator it = nodeInfo->m_values.begin(); it != nodeInfo->m_values.end(); ++it )
                {
                    if( (*it) == _notification->GetValueID() )
                    {
                        nodeInfo->m_values.erase( it );
                        break;
                    }
                }
            }
            break;
        }

        case Notification::Type_ValueRefreshed:
            AGO_DEBUG() << "Notification='ValueRefreshed'";
            break;

        case Notification::Type_ValueChanged:
        {
            if( /*NodeInfo* nodeInfo =*/ getNodeInfo( _notification ) )
            {
                // One of the node values has changed
                // TBD...
                // nodeInfo = nodeInfo;
                ValueID id = _notification->GetValueID();
                string str;
                AGO_INFO() << "Notification='Value Changed' Home=" << std::hex << _notification->GetHomeId() << " Node=" << std::dec << (int)_notification->GetNodeId() << " Genre=" << std::dec << (int)id.GetGenre() << " Class=" << getHRCommandClassId(id.GetCommandClassId()) << std::dec << " Type=" << id.GetType() << " Genre=" << id.GetGenre();

                if (Manager::Get()->GetValueAsString(id, &str))
                {
                    ZWaveNode* device = NULL;
                    qpid::types::Variant cachedValue;
                    cachedValue.parse(str);
                    valueCache[id] = cachedValue;
                    string label = Manager::Get()->GetValueLabel(id);
                    string units = Manager::Get()->GetValueUnits(id);

                    // TODO: send proper types and don't convert everything to string
                    string level = str;
                    string eventtype = "";
                    if (str == "True")
                    {
                        level="255";
                    }
                    if (str == "False")
                    {
                        level="0";
                    }
                    AGO_DEBUG() << "Value=" << str << " Label=" << label << " Units=" << units;
                    if ((label == "Basic") || (label == "Switch") || (label == "Level"))
                    {
                        eventtype="event.device.statechanged";
                    }
                    else if (label == "Luminance")
                    {
                        eventtype="event.environment.brightnesschanged";
                    }
                    else if (label == "Temperature")
                    {
                        eventtype="event.environment.temperaturechanged";
                        if (units=="F" && unitsystem==0)
                        {
                            units = "degC";
                            str = float2str((atof(str.c_str())-32)*5/9);
                            level = str;
                        }
                        else if (units =="C" && unitsystem==1)
                        {
                            units = "degF";
                            str = float2str(atof(str.c_str())*9/5 + 32);
                            level = str;
                        }
                    }
                    else if (label == "Relative Humidity")
                    {
                        eventtype="event.environment.humiditychanged";
                    }
                    else if (label == "Battery Level")
                    {
                        //check if battery sensor already exists. If not create new device.
                        device = devices.findValue(id);
                        if( device==NULL || device->getDevicetype()!="batterysensor" )
                        {
                            //create new device before sending event
                            stringstream tempstream;
                            tempstream << (int)_notification->GetNodeId() << "/1-" << label;

                            device = new ZWaveNode(tempstream.str(), "batterysensor");
                            device->addValue(label, id);
                            devices.add(device);
                            AGO_DEBUG() << "Battery: add new batterysensor [" << device->getId() << ", " << device->getDevicetype() << "]";
                            agoConnection->addDevice(device->getId().c_str(), device->getDevicetype().c_str());
                        }
                        eventtype="event.device.batterylevelchanged";
                    }
                    else if (label == "Alarm Level")
                    {
                        eventtype="event.security.alarmlevelchanged";
                    }
                    else if (label == "Alarm Type")
                    {
                        eventtype="event.security.alarmtypechanged";
                    }
                    else if (label == "Sensor")
                    {
                        eventtype="event.security.sensortriggered";
                    }
                    else if (label == "Energy")
                    {
                        eventtype="event.environment.energychanged";
                    }
                    else if (label == "Power")
                    {
                        eventtype="event.environment.powerchanged";
                    }
                    else if (label == "Mode")
                    {
                        eventtype="event.environment.modechanged";
                    }
                    else if (label == "Fan Mode")
                    {
                        eventtype="event.environment.fanmodechanged";
                    }
                    else if (label == "Fan State")
                    {
                        eventtype="event.environment.fanstatechanged";
                    }
                    else if (label == "Operating State")
                    {
                        eventtype="event.environment.operatingstatechanged";
                    }
                    else if (label == "Cooling 1")
                    {
                        eventtype="event.environment.coolsetpointchanged";
                    }
                    else if (label == "Heating 1")
                    {
                        eventtype="event.environment.heatsetpointchanged";
                    }
                    else if (label == "Fan State")
                    {
                        eventtype="event.environment.fanstatechanged";
                    }
                    else if (label == "Flood")
                    {
                        eventtype="event.security.sensortriggered";
                    }

                    if (eventtype != "")
                    {
                        //search device
                        if( device==NULL )
                        {
                            device = devices.findValue(id);
                        }

                        if (device != NULL)
                        {
                            AGO_DEBUG() << "Sending " << eventtype << " from child " << device->getId();
                            agoConnection->emitEvent(device->getId().c_str(), eventtype.c_str(), level.c_str(), units.c_str());	
                        }
                    }
                }
            }
            break;
        }
        case Notification::Type_Group:
        {
            if( NodeInfo* nodeInfo = getNodeInfo( _notification ) )
            {
                // One of the node's association groups has changed
                // TBD...
                nodeInfo = nodeInfo;
                eventmap["description"]="Node association added";
                eventmap["state"]="associationchanged";
                eventmap["nodeid"] = _notification->GetNodeId();
                eventmap["homeid"] = _notification->GetHomeId();
                agoConnection->emitEvent("zwavecontroller", "event.zwave.associationchanged", eventmap);
            }
            break;
        }

        case Notification::Type_NodeAdded:
        {
            // Add the new node to our list
            NodeInfo* nodeInfo = new NodeInfo();
            nodeInfo->m_homeId = _notification->GetHomeId();
            nodeInfo->m_nodeId = _notification->GetNodeId();
            nodeInfo->m_polled = false;		
            g_nodes.push_back( nodeInfo );

            // todo: announce node
            eventmap["description"]="Node added";
            eventmap["state"]="nodeadded";
            eventmap["nodeid"] = _notification->GetNodeId();
            eventmap["homeid"] = _notification->GetHomeId();
            agoConnection->emitEvent("zwavecontroller", "event.zwave.networkchanged", eventmap);
            break;
        }

        case Notification::Type_NodeRemoved:
        {
            // Remove the node from our list
            uint32 const homeId = _notification->GetHomeId();
            uint8 const nodeId = _notification->GetNodeId();
            eventmap["description"]="Node removed";
            eventmap["state"]="noderemoved";
            eventmap["nodeid"] = _notification->GetNodeId();
            eventmap["homeid"] = _notification->GetHomeId();
            agoConnection->emitEvent("zwavecontroller", "event.zwave.networkchanged", eventmap);
            for( list<NodeInfo*>::iterator it = g_nodes.begin(); it != g_nodes.end(); ++it )
            {
                NodeInfo* nodeInfo = *it;
                if( ( nodeInfo->m_homeId == homeId ) && ( nodeInfo->m_nodeId == nodeId ) )
                {
                    g_nodes.erase( it );
                    break;
                }
            }
            break;
        }

        case Notification::Type_NodeEvent:
        {
            if( /*NodeInfo* nodeInfo =*/ getNodeInfo( _notification ) )
            {
                // We have received an event from the node, caused by a
                // basic_set or hail message.
                ValueID id = _notification->GetValueID();
                AGO_DEBUG() << "NodeEvent: HomeId=" << id.GetHomeId() << " NodeId=" << id.GetNodeId() << " Genre=" << id.GetGenre() << " CommandClassId=" << getHRCommandClassId(id.GetCommandClassId()) << " Instance=" << id.GetInstance() << " Index="<< id.GetIndex() << " Type="<< id.GetType() << " Id="<< id.GetId();
                string label = Manager::Get()->GetValueLabel(id);
                stringstream level;
                level << (int) _notification->GetByte();
                string eventtype = "event.device.statechanged";
                ZWaveNode *device = devices.findValue(id);
                if (device != NULL)
                {
                    AGO_DEBUG() << "Sending " << eventtype << " from child " << device->getId();
                    agoConnection->emitEvent(device->getId().c_str(), eventtype.c_str(), level.str().c_str(), "");	
                }
                else
                {
                    AGO_WARNING() << "no agocontrol device found for node event: Label=" << label << " Level=" << level;
                }

            }
            break;
        }
        case Notification::Type_SceneEvent:
        {
            if( /*NodeInfo* nodeInfo =*/ getNodeInfo( _notification ) )
            {
                int scene = _notification->GetSceneId();
                ValueID id = _notification->GetValueID();
                string label = Manager::Get()->GetValueLabel(id);
                stringstream tempstream;
                tempstream << (int) _notification->GetNodeId();
                tempstream << "/1";
                string nodeinstance = tempstream.str();
                string eventtype = "event.device.scenechanged";
                ZWaveNode *device;
                if ((device = devices.findId(nodeinstance)) != NULL)
                {
                    AGO_DEBUG() << "Sending " << eventtype << " for scene " << scene << " event from child " << device->getId();
                    qpid::types::Variant::Map content;
                    content["scene"]=scene;
                    agoConnection->emitEvent(device->getId().c_str(), eventtype.c_str(), content);	
                }
                else
                {
                    AGO_WARNING() << "no agocontrol device found for scene event: Node=" << nodeinstance << " Scene=" << scene;
                }

            }
            break;
        }
        case Notification::Type_PollingDisabled:
        {
            if( NodeInfo* nodeInfo = getNodeInfo( _notification ) )
            {
                nodeInfo->m_polled = false;
            }
            break;
        }

        case Notification::Type_PollingEnabled:
        {
            if( NodeInfo* nodeInfo = getNodeInfo( _notification ) )
            {
                nodeInfo->m_polled = true;
            }
            break;
        }

        case Notification::Type_DriverReady:
        {
            g_homeId = _notification->GetHomeId();
            break;
        }


        case Notification::Type_DriverFailed:
        {
            g_initFailed = true;
            pthread_cond_broadcast(&initCond);
            break;
        }

        case Notification::Type_AwakeNodesQueried:
        case Notification::Type_AllNodesQueried:
        case Notification::Type_AllNodesQueriedSomeDead:
        {
            pthread_cond_broadcast(&initCond);
            break;
        }
        case Notification::Type_DriverReset:
        case Notification::Type_Notification:
        case Notification::Type_NodeNaming:
        case Notification::Type_NodeProtocolInfo:
        case Notification::Type_NodeQueriesComplete:
        case Notification::Type_EssentialNodeQueriesComplete:
        {
            break;
        }
        default:
        {
            AGO_WARNING() << "Unhandled OpenZWave Event: " << _notification->GetType();
        }
    }

    pthread_mutex_unlock( &g_criticalSection );
}



qpid::types::Variant::Map AgoZwave::commandHandler(qpid::types::Variant::Map content)
{
    qpid::types::Variant::Map returnval;
    bool result = false;
    std::string internalid = content["internalid"].asString();

    if (internalid == "zwavecontroller")
    {
        AGO_TRACE() << "z-wave specific controller command received";
        if (content["command"] == "addnode")
        {
            Manager::Get()->BeginControllerCommand(g_homeId, Driver::ControllerCommand_AddDevice, controller_update, this, true);
            result = true;
        }
        else if (content["command"] == "removenode")
        {
            if (!(content["node"].isVoid()))
            {
                int mynode = content["node"];
                Manager::Get()->BeginControllerCommand(g_homeId, Driver::ControllerCommand_RemoveFailedNode, controller_update, this, true, mynode);
            }
            else
            {
                Manager::Get()->BeginControllerCommand(g_homeId, Driver::ControllerCommand_RemoveDevice, controller_update, this, true);
            }
            result = true;
        }
        else if (content["command"] == "healnode")
        {
            if (!(content["node"].isVoid()))
            {
                int mynode = content["node"];
                Manager::Get()->HealNetworkNode(g_homeId, mynode, true);
                result = true;
            }
            else
            {
                result = false;
            }
        }
        else if (content["command"] == "healnetwork")
        {
            Manager::Get()->HealNetwork(g_homeId, true);
            result = true;
        }
        else if (content["command"] == "refreshnode")
        {
            if (!(content["node"].isVoid()))
            {
                int mynode = content["node"];
                Manager::Get()->RefreshNodeInfo(g_homeId, mynode);
                result = true;
            }
            else
            {
                result = false;
            }
        }
        else if (content["command"] == "getstatistics")
        {
            Driver::DriverData data;
            Manager::Get()->GetDriverStatistics( g_homeId, &data );
            qpid::types::Variant::Map statistics;
            statistics["SOF"] = data.m_SOFCnt;
            statistics["ACK waiting"] = data.m_ACKWaiting;
            statistics["Read Aborts"] = data.m_readAborts;
            statistics["Bad Checksums"] = data.m_badChecksum;
            statistics["Reads"] = data.m_readCnt;
            statistics["Writes"] = data.m_writeCnt;
            statistics["CAN"] = data.m_CANCnt;
            statistics["NAK"] = data.m_NAKCnt;
            statistics["ACK"] = data.m_ACKCnt;
            statistics["OOF"] = data.m_OOFCnt;
            statistics["Dropped"] = data.m_dropped;
            statistics["Retries"] = data.m_retries;
            returnval["statistics"]=statistics;
            result = true;
        }
        else if (content["command"] == "testnode")
        {
            if (!(content["node"].isVoid()))
            {
                int mynode = content["node"];
                int count = 10;
                if (!(content["count"].isVoid()))
                {
                    count=content["count"];
                }
                Manager::Get()->TestNetworkNode(g_homeId, mynode, count);
                result = true;
            }
        }
        else if (content["command"] == "testnetwork")
        {
            int count = 10;
            if (!(content["count"].isVoid()))
            {
                count=content["count"];
            }
            Manager::Get()->TestNetwork(g_homeId, count);
            result = true;
        }
        else if (content["command"] == "getnodes")
        {
            qpid::types::Variant::Map nodelist;
            for( list<NodeInfo*>::iterator it = g_nodes.begin(); it != g_nodes.end(); ++it )
            {
                NodeInfo* nodeInfo = *it;
                string index;
                qpid::types::Variant::Map node;
                qpid::types::Variant::List neighborsList;
                qpid::types::Variant::List valuesList;
                qpid::types::Variant::Map status;
                qpid::types::Variant::List params;

                uint8* neighbors;
                uint32 numNeighbors = Manager::Get()->GetNodeNeighbors(nodeInfo->m_homeId,nodeInfo->m_nodeId,&neighbors);
                if (numNeighbors)
                {
                    for(uint32 i=0; i<numNeighbors; i++)
                    {
                        neighborsList.push_back(neighbors[i]);
                    }
                    delete [] neighbors;
                }
                node["neighbors"]=neighborsList;	

                for (list<ValueID>::iterator it2 = (*it)->m_values.begin(); it2 != (*it)->m_values.end(); it2++ )
                {
                    OpenZWave::ValueID currentValueID = *it2;
                    ZWaveNode *device = devices.findValue(currentValueID);
                    if (device != NULL)
                    {
                        valuesList.push_back(device->getId());
                    }

                    //get node specific parameters
                    uint8_t commandClassId = it2->GetCommandClassId();
                    if( commandClassId==COMMAND_CLASS_CONFIGURATION )
                    {
                        qpid::types::Variant::Map param = getCommandClassConfigurationParameter(&currentValueID);
                        if( param.size()>0 )
                        {
                            params.push_back(param);
                        }
                    }
                    else if( commandClassId==COMMAND_CLASS_WAKE_UP )
                    {
                        //need to know wake up interval
                        if( currentValueID.GetGenre()==ValueID::ValueGenre_System && currentValueID.GetInstance()==1 )
                        {
                            if( Manager::Get()->GetValueLabel(currentValueID)=="Wake-up Interval" )
                            {
                                qpid::types::Variant::Map param = getCommandClassWakeUpParameter(&currentValueID);
                                if( param.size()>0 )
                                {
                                    params.push_back(param);
                                }
                            }
                        }
                    }
                }
                node["internalids"] = valuesList;

                node["manufacturer"]=Manager::Get()->GetNodeManufacturerName(nodeInfo->m_homeId,nodeInfo->m_nodeId);
                node["version"]=Manager::Get()->GetNodeVersion(nodeInfo->m_homeId,nodeInfo->m_nodeId);
                node["basic"]=Manager::Get()->GetNodeBasic(nodeInfo->m_homeId,nodeInfo->m_nodeId);
                node["generic"]=Manager::Get()->GetNodeGeneric(nodeInfo->m_homeId,nodeInfo->m_nodeId);
                node["specific"]=Manager::Get()->GetNodeSpecific(nodeInfo->m_homeId,nodeInfo->m_nodeId);
                node["product"]=Manager::Get()->GetNodeProductName(nodeInfo->m_homeId,nodeInfo->m_nodeId);
                node["type"]=Manager::Get()->GetNodeType(nodeInfo->m_homeId,nodeInfo->m_nodeId);
                node["producttype"]=Manager::Get()->GetNodeProductType(nodeInfo->m_homeId,nodeInfo->m_nodeId);
                node["numgroups"]=Manager::Get()->GetNumGroups(nodeInfo->m_homeId,nodeInfo->m_nodeId);

                //fill node status
                status["querystage"]=Manager::Get()->GetNodeQueryStage(nodeInfo->m_homeId,nodeInfo->m_nodeId);
                status["awake"]=(Manager::Get()->IsNodeAwake(nodeInfo->m_homeId,nodeInfo->m_nodeId) ? 1 : 0);
                status["listening"]=(Manager::Get()->IsNodeListeningDevice(nodeInfo->m_homeId,nodeInfo->m_nodeId) || 
                        Manager::Get()->IsNodeFrequentListeningDevice(nodeInfo->m_homeId,nodeInfo->m_nodeId) ? 1 : 0);
                status["failed"]=(Manager::Get()->IsNodeFailed(nodeInfo->m_homeId,nodeInfo->m_nodeId) ? 1 : 0);
                node["status"]=status;

                //fill node parameters
                node["params"] = params;

                uint8 nodeid = nodeInfo->m_nodeId;
                index = static_cast<ostringstream*>( &(ostringstream() << nodeid) )->str();
                nodelist[index.c_str()] = node;
            }
            returnval["nodelist"]=nodelist;
            result = true;
        }
        else if (content["command"] == "addassociation")
        {
            int mynode = content["node"];
            int mygroup = content["group"];
            int mytarget = content["target"];
            AGO_DEBUG() << "adding association: Node=" << mynode << " Group=" << mygroup << " Target=" << mytarget;
            Manager::Get()->AddAssociation(g_homeId, mynode, mygroup, mytarget);
            result = true;
        }
        else if (content["command"] == "getassociations")
        {
            qpid::types::Variant::Map associationsmap;
            int mygroup = content["group"];
            int mynode = content["node"];
            uint8_t *associations;
            uint32_t numassoc = Manager::Get()->GetAssociations(g_homeId, mynode, mygroup, &associations);
            for (uint32_t assoc = 0; assoc < numassoc; assoc++)
            {
                associationsmap[int2str(assoc)] = associations[assoc];
            }
            if (numassoc >0) delete associations;
            returnval["associations"] = associationsmap;
            returnval["label"] = Manager::Get()->GetGroupLabel(g_homeId, mynode, mygroup);
            result = true;
        }
        else if (content["command"] == "removeassociation")
        {
            int mynode = content["node"];
            int mygroup = content["group"];
            int mytarget = content["target"];
            AGO_DEBUG() << "removing association: Node=" << mynode << " Group=" << mygroup << " Target=" << mytarget;
            Manager::Get()->RemoveAssociation(g_homeId, mynode, mygroup, mytarget);
            result = true;
        }
        else if (content["command"] == "setconfigparam")
        {
            int nodeId = content["node"];
            int commandClassId = content["commandclassid"];
            int index = content["index"];
            string value = string(content["value"]);
            AGO_DEBUG() << "setting config param: nodeId=" << nodeId << " commandClassId=" << commandClassId << " index=" << index << " value=" << value;
            result = setCommandClassParameter(g_homeId, nodeId, commandClassId, index, value);
        }
        else if( content["command"]=="requestallconfigparams" )
        {
            result = true;
            int node = content["node"];
            AGO_DEBUG() << "RequestAllConfigParams: node=" << node;
            Manager::Get()->RequestAllConfigParams(g_homeId, node);
        }
        else if (content["command"] == "downloadconfig")
        {
            result = true;
            result = Manager::Get()->BeginControllerCommand(g_homeId, Driver::ControllerCommand_ReceiveConfiguration, controller_update, NULL, true);
        }
        else if (content["command"] == "cancel")
        {
            result = true;
            Manager::Get()->CancelControllerCommand(g_homeId);
        }
        else if (content["command"] == "saveconfig")
        {
            result = true;
            Manager::Get()->WriteConfig( g_homeId );
        }
        else if (content["command"] == "allon")
        {
            result = true;
            Manager::Get()->SwitchAllOn(g_homeId );
        }
        else if (content["command"] == "alloff")
        {
            result = true;
            Manager::Get()->SwitchAllOff(g_homeId );
        }
        else if (content["command"] == "reset")
        {
            result = true;
            Manager::Get()->ResetController(g_homeId);
        }
        else
        {
            result = false;
        }
    }
    else
    {
        ZWaveNode *device = devices.findId(internalid);
        if (device != NULL)
        {
            string devicetype = device->getDevicetype();
            AGO_TRACE() << "command received for " << internalid << "(" << devicetype << ")"; 
            ValueID *tmpValueID = NULL;

            if (devicetype == "switch")
            {
                tmpValueID = device->getValueID("Switch");
                if (tmpValueID == NULL)
                {
                    returnval["result"] = -1;
                    return returnval;
                }
                if (content["command"] == "on" )
                {
                    result = Manager::Get()->SetValue(*tmpValueID , true);
                }
                else
                {
                    result = Manager::Get()->SetValue(*tmpValueID , false);
                }
            }
            else if(devicetype == "dimmer")
            {
                tmpValueID = device->getValueID("Level");
                if (tmpValueID == NULL)
                {
                    returnval["result"] = -1;
                    return returnval;
                }
                if (content["command"] == "on" )
                {
                    result = Manager::Get()->SetValue(*tmpValueID , (uint8) 255);
                }
                else if (content["command"] == "setlevel")
                {
                    uint8 level = atoi(content["level"].asString().c_str());
                    if (level > 99)
                    {
                        level=99;
                    }
                    result = Manager::Get()->SetValue(*tmpValueID, level);
                }
                else
                {
                    result = Manager::Get()->SetValue(*tmpValueID , (uint8) 0);
                }
            }
            else if (devicetype == "drapes")
            {
                if (content["command"] == "on")
                {
                    tmpValueID = device->getValueID("Level");
                    if (tmpValueID == NULL)
                    {
                        returnval["result"] = -1; 
                        return returnval;
                    }
                    result = Manager::Get()->SetValue(*tmpValueID , (uint8) 255);
                }
                else if (content["command"] == "open" )
                {
                    tmpValueID = device->getValueID("Open");
                    if (tmpValueID == NULL)
                    {
                        returnval["result"] = -1; 
                        return returnval;
                    }
                    result = Manager::Get()->SetValue(*tmpValueID , true);
                }
                else if (content["command"] == "close" )
                {
                    tmpValueID = device->getValueID("Close");
                    if (tmpValueID == NULL)
                    {
                        returnval["result"] = -1;
                        return returnval;
                    }
                    result = Manager::Get()->SetValue(*tmpValueID , true);
                }
                else if (content["command"] == "stop" )
                {
                    tmpValueID = device->getValueID("Stop");
                    if (tmpValueID == NULL)
                    {
                        returnval["result"] = -1;
                        return returnval;
                    }
                    result = Manager::Get()->SetValue(*tmpValueID , true);

                }
                else
                {
                    tmpValueID = device->getValueID("Level");
                    if (tmpValueID == NULL)
                    {
                        returnval["result"] = -1;
                        return returnval;
                    }
                    result = Manager::Get()->SetValue(*tmpValueID , (uint8) 0);
                }
            }
            else if (devicetype == "thermostat")
            {
                if (content["command"] == "settemperature")
                {
                    string mode = content["mode"].asString();
                    if  (mode == "")
                    {
                        mode = "auto";
                    }
                    if (mode == "cool")
                    {
                        tmpValueID = device->getValueID("Cooling 1");
                    }
                    else if ((mode == "auto") || (mode == "heat"))
                    {
                        tmpValueID = device->getValueID("Heating 1");
                    }
                    if (tmpValueID == NULL)
                    {
                        returnval["result"] = -1;
                        return returnval;
                    }
                    float temp = 0.0;
                    if (content["temperature"] == "-1")
                    {
                        try
                        {
                            AGO_TRACE() << "rel temp -1:" << valueCache[*tmpValueID];
                            temp = atof(valueCache[*tmpValueID].asString().c_str());
                            temp = temp - 1.0;
                        }
                        catch (...)
                        {
                            AGO_WARNING() << "can't determine current value for relative temperature change";
                            returnval["result"] = -1;
                            return returnval;
                        }
                    }
                    else if (content["temperature"] == "+1")
                    {
                        try
                        {
                            AGO_TRACE() << "rel temp +1: " << valueCache[*tmpValueID];
                            temp = atof(valueCache[*tmpValueID].asString().c_str());
                            temp = temp + 1.0;
                        }
                        catch (...)
                        {
                            AGO_WARNING() << "can't determine current value for relative temperature change";
                            returnval["result"] = -1; return returnval;
                        }
                    }
                    else
                    {
                        temp = content["temperature"];
                    }
                    AGO_TRACE() << "setting temperature: " << temp;
                    result = Manager::Get()->SetValue(*tmpValueID , temp);
                }
                else if (content["command"] == "setthermostatmode")
                {
                    string mode = content["mode"].asString();
                    tmpValueID = device->getValueID("Mode");
                    if (tmpValueID == NULL)
                    {
                        returnval["result"] = -1;
                        return returnval;
                    }
                    if (mode=="heat")
                    {
                        result = Manager::Get()->SetValueListSelection(*tmpValueID , "Heat");
                    }
                    else if (mode=="cool")
                    {
                        result = Manager::Get()->SetValueListSelection(*tmpValueID , "Cool");
                    }
                    else if (mode == "off")
                    {
                        result = Manager::Get()->SetValueListSelection(*tmpValueID , "Off");
                    }
                    else if (mode == "auxheat")
                    {
                        result = Manager::Get()->SetValueListSelection(*tmpValueID , "Aux Heat");
                    }
                    else
                    {
                        result = Manager::Get()->SetValueListSelection(*tmpValueID , "Auto");
                    }
                }
                else if (content["command"] == "setthermostatfanmode")
                {
                    string mode = content["mode"].asString();
                    tmpValueID = device->getValueID("Fan Mode");
                    if (tmpValueID == NULL)
                    {
                        returnval["result"] = -1;
                        return returnval;
                    }
                    if (mode=="circulate")
                    {
                        result = Manager::Get()->SetValueListSelection(*tmpValueID , "Circulate");
                    }
                    else if (mode=="on" || mode=="onlow")
                    {
                        result = Manager::Get()->SetValueListSelection(*tmpValueID , "On Low");
                    }
                    else if (mode=="onhigh")
                    {
                        result = Manager::Get()->SetValueListSelection(*tmpValueID , "On High");
                    }
                    else if (mode=="autohigh")
                    {
                        result = Manager::Get()->SetValueListSelection(*tmpValueID , "Auto High");
                    }
                    else
                    {
                        result = Manager::Get()->SetValueListSelection(*tmpValueID , "Auto Low");
                    }
                }
            }
        }
    }
    returnval["result"] = result ? 0 : -1;
    return returnval;
}

void AgoZwave::setupApp()
{
    std::string device;

    device=getConfigOption("device", "/dev/usbzwave");
    if (getConfigSectionOption("system", "units", "SI") != "SI")
    {
        unitsystem=1;
    }

    pthread_mutexattr_t mutexattr;

    pthread_mutexattr_init ( &mutexattr );
    pthread_mutexattr_settype( &mutexattr, PTHREAD_MUTEX_RECURSIVE );

    pthread_mutex_init( &g_criticalSection, &mutexattr );
    pthread_mutexattr_destroy( &mutexattr );

    pthread_mutex_lock( &initMutex );

    AGO_INFO() << "Starting OZW driver ver " << Manager::getVersionAsString();
    // init open zwave
    Options::Create( "/etc/openzwave/", getConfigPath("/ozw/").c_str(), "" );
    if (getConfigOption("returnroutes", "true")=="true")
    {
        Options::Get()->AddOptionBool("PerformReturnRoutes", false );
    }
    Options::Get()->AddOptionBool("ConsoleOutput", true ); 
    if (getConfigOption("sis", "true")=="true")
    {
        Options::Get()->AddOptionBool("EnableSIS", true ); 
    }
    if (getConfigOption("assumeawake", "false")=="false")
    {
        Options::Get()->AddOptionBool("AssumeAwake", false ); 
    }
    if (getConfigOption("validatevaluechanges", "false")=="true")
    {
        Options::Get()->AddOptionBool("ValidateValueChanges", true);
    }

    Options::Get()->AddOptionInt( "SaveLogLevel", LogLevel_Debug );
    Options::Get()->AddOptionInt( "QueueLogLevel", LogLevel_Debug );
    Options::Get()->AddOptionInt( "DumpTrigger", LogLevel_Error );

    int retryTimeout = atoi(getConfigOption("retrytimeout","2000").c_str());
    OpenZWave::Options::Get()->AddOptionInt("RetryTimeout", retryTimeout);

    Options::Get()->Lock();

    MyLog *myLog = new MyLog;
    // OpenZWave::Log* pLog = OpenZWave::Log::Create(myLog);
    OpenZWave::Log::SetLoggingClass(myLog);
    // OpenZWave::Log* pLog = OpenZWave::Log::Create("/var/log/zwave.log", true, false, OpenZWave::LogLevel_Info, OpenZWave::LogLevel_Debug, OpenZWave::LogLevel_Error);
    // pLog->SetLogFileName("/var/log/zwave.log"); // Make sure, in case Log::Create already was called before we got here
    // pLog->SetLoggingState(OpenZWave::LogLevel_Info, OpenZWave::LogLevel_Debug, OpenZWave::LogLevel_Error);

    Manager::Create();
    Manager::Get()->AddWatcher( on_notification, this );
    // Manager::Get()->SetPollInterval(atoi(getConfigOption("pollinterval", "300000").c_str()),true);
    if (getConfigOption("polling", "0") == "1") polling=true;
    Manager::Get()->AddDriver(device);

    // Now we just wait for the driver to become ready
    AGO_INFO() << "waiting for OZW driver to become ready";
    pthread_cond_wait( &initCond, &initMutex );

    if( !g_initFailed )
    {
        Manager::Get()->WriteConfig( g_homeId );
        Driver::DriverData data;
        Manager::Get()->GetDriverStatistics( g_homeId, &data );

        //get device parameters
        requestAllNodeConfigParameters();

        AGO_DEBUG() << "\n\n\n=================================";
        AGO_INFO() << "OZW startup complete";
        AGO_DEBUG() << devices.toString();

        agoConnection->addDevice("zwavecontroller", "zwavecontroller");
        addCommandHandler();
    }
    else
    {
        AGO_FATAL() << "unable to initialize OZW";
        Manager::Destroy();
        pthread_mutex_destroy( &g_criticalSection );
        throw StartupError();
    }	
}

void AgoZwave::cleanupApp() {
    Manager::Destroy();
    pthread_mutex_destroy( &g_criticalSection );
}

AGOAPP_ENTRY_POINT(AgoZwave);
