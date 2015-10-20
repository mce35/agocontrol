#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <pthread.h>

#include <string>
#include <iostream>
#include <sstream>
#include <cerrno>

#include "agoapp.h"
#include "bool.h"

#ifndef EVENTMAPFILE
#define EVENTMAPFILE "maps/eventmap.json"
#endif

using namespace std;
using namespace agocontrol;
using namespace qpid::types;
namespace fs = ::boost::filesystem;

class AgoEvent: public AgoApp {
private:
    qpid::types::Variant::Map eventmap;
    qpid::types::Variant::Map commandHandler(qpid::types::Variant::Map content) ;
    void eventHandler(std::string subject, qpid::types::Variant::Map content) ;
    void setupApp();
public:
    AGOAPP_CONSTRUCTOR(AgoEvent);
};


double variantToDouble(qpid::types::Variant v) {
    double result;
    switch(v.getType()) {
        case VAR_DOUBLE:
            result = v.asDouble();	
            break;
        case VAR_FLOAT:
            result = v.asFloat();	
            break;
        case VAR_BOOL:
            result = v.asBool();
            break;
        case VAR_STRING:
            result = atof(v.asString().c_str());
            break;
        case VAR_INT8:
            result = v.asInt8();
            break;
        case VAR_INT16:
            result = v.asInt16();
            break;
        case VAR_INT32:
            result = v.asInt32();
            break;
        case VAR_INT64:
            result = v.asInt64();
            break;
        case VAR_UINT8:
            result = v.asUint8();
            break;
        case VAR_UINT16:
            result = v.asUint16();
            break;
        case VAR_UINT32:
            result = v.asUint32();
            break;
        case VAR_UINT64:
            result = v.asUint64();
            break;
        default:
            AGO_ERROR() << "No conversion for type: " << v;
            result = 0;
    }
    return result;
}

bool operator<(qpid::types::Variant a, qpid::types::Variant b) {
    return variantToDouble(a) < variantToDouble(b);
}
bool operator>(qpid::types::Variant a, qpid::types::Variant b) {
    return b < a;
}
bool operator<=(qpid::types::Variant a, qpid::types::Variant b) {
    return !(a>b);
}
bool operator>=(qpid::types::Variant a, qpid::types::Variant b) {
    return !(a<b);
}

// example event:eb68c4a5-364c-4fb8-9b13-7ea3a784081f:{action:{command:on, uuid:25090479-566d-4cef-877a-3e1927ed4af0}, criteria:{0:{comp:eq, lval:hour, rval:7}, 1:{comp:eq, lval:minute, rval:1}}, event:event.environment.timechanged, nesting:(criteria["0"] and criteria["1"])}


void AgoEvent::eventHandler(std::string subject, qpid::types::Variant::Map content)
{
    // ignore device announce events
    if( subject=="event.device.announce" || subject=="event.device.discover" )
    {
        return;
    }

    // iterate event map and match for event name
    if( eventmap.size()>0 )
    {
        for (qpid::types::Variant::Map::const_iterator it = eventmap.begin(); it!=eventmap.end(); it++)
        { 
            qpid::types::Variant::Map event;
            if (!(it->second.isVoid()))
            {
                event = it->second.asMap();
            }
            else
            {
                AGO_ERROR() << "Eventmap entry is void";
            }
            if (event["event"] == subject)
            {
                AGO_TRACE() << "Found matching event: " << event;
                // check if the event is disabled
                if ((!(event["disabled"].isVoid())) && (event["disabled"].asBool() == true))
                {
                    return;
                }

                qpid::types::Variant::Map inventory; // this will hold the inventory from the resolver if we need it during evaluation
                qpid::types::Variant::Map criteria; // this holds the criteria evaluation results for each criteria
                std::string nesting = event["nesting"].asString();
                if (!event["criteria"].isVoid()) for (qpid::types::Variant::Map::const_iterator crit = event["criteria"].asMap().begin(); crit!= event["criteria"].asMap().end(); crit++)
                {
                    AGO_TRACE() << "criteria[" << crit->first << "] - " << crit->second;
                    qpid::types::Variant::Map element;
                    if (!(crit->second.isVoid()))
                    {
                        element = crit->second.asMap();
                    }
                    else
                    {
                        AGO_ERROR() << "Criteria element is void";
                    }
                    try
                    {
                        // AGO_TRACE() << "LVAL: " << element["lval"];
                        qpid::types::Variant::Map lvalmap;
                        qpid::types::Variant lval;
                        if (!element["lval"].isVoid())
                        {
                            if (element["lval"].getType()==qpid::types::VAR_STRING)
                            {
                                // legacy eventmap entry
                                lvalmap["type"] = "event";
                                lvalmap["parameter"] = element["lval"];
                            }
                            else
                            {
                                lvalmap = element["lval"].asMap();
                            }
                        }
                        // determine lval depending on type
                        if (lvalmap["type"] == "variable")
                        {
                            qpid::types::Variant::Map variables;
                            std::string name = lvalmap["name"];
                            if (inventory["system"].isVoid()) inventory = agoConnection->getInventory(); // fetch inventory as it is needed for eval but has not yet been pulled
                            if (!inventory["variables"].isVoid())
                            {
                                variables = inventory["variables"].asMap();
                            }
                            lval = variables[name];	

                        }
                        else if (lvalmap["type"] == "device")
                        {
                            std::string uuid = lvalmap["uuid"].asString();
                            if (inventory["system"].isVoid()) inventory = agoConnection->getInventory(); // fetch inventory as it is needed for eval but has not yet been pulled
                            qpid::types::Variant::Map devices = inventory["devices"].asMap();
                            qpid::types::Variant::Map device = devices[uuid].asMap();
                            if (lvalmap["parameter"] == "state")
                            {
                                lval = device["state"];
                            }
                            else
                            {
                                qpid::types::Variant::Map values = device["values"].asMap();
                                std::string parameter = lvalmap["parameter"].asString();
                                qpid::types::Variant::Map value = values[parameter].asMap();
                                lval = value["level"];
                            }
                        }
                        else //event
                        {
                            lval = content[lvalmap["parameter"].asString()];
                        }
                        qpid::types::Variant rval = element["rval"];
                        AGO_TRACE() << "lval: " << lval << " (" << getTypeName(lval.getType()) << ")";
                        AGO_TRACE() << "rval: " << rval << " (" << getTypeName(rval.getType()) << ")";

                        if (element["comp"] == "eq")
                        {
                            if (lval.getType()==qpid::types::VAR_STRING || rval.getType()==qpid::types::VAR_STRING) // compare as string
                            {
                                criteria[crit->first] = lval.asString() == rval.asString(); 
                            }
                            else
                            {
                                criteria[crit->first] = lval.isEqualTo(rval);
                            }
                        } else if (element["comp"] == "neq")
                        {
                            if (lval.getType()==qpid::types::VAR_STRING || rval.getType()==qpid::types::VAR_STRING) // compare as string
                            {
                                criteria[crit->first] = lval.asString() != rval.asString(); 
                            }
                            else
                            {
                                criteria[crit->first] = !(lval.isEqualTo(rval));
                            }
                        } else if (element["comp"] == "lt")
                        {
                            criteria[crit->first] = lval < rval;
                        }
                        else if (element["comp"] == "gt")
                        {
                            criteria[crit->first] = lval > rval;
                        }
                        else if (element["comp"] == "gte")
                        {
                            criteria[crit->first] = lval >= rval;
                        }
                        else if (element["comp"] == "lte")
                        {
                            criteria[crit->first] = lval <= rval;
                        }
                        else
                        {
                            criteria[crit->first] = false;
                        }
                        AGO_TRACE() << lval << " " << element["comp"] << " " << rval << " : " << criteria[crit->first];
                    }
                    catch ( const std::exception& error)
                    {
                        stringstream errorstring;
                        errorstring << error.what();
                        AGO_ERROR() << "Exception occured: " << errorstring.str();
                        criteria[crit->first] = false;
                    }

                    // this is for converted legacy scenario maps
                    stringstream token; token << "criteria[\"" << crit->first << "\"]";
                    stringstream boolval; boolval << criteria[crit->first];
                    replaceString(nesting, token.str(), boolval.str()); 

                    // new javascript editor sends criteria[x] not criteria["x"]
                    stringstream token2; token2 << "criteria[" << crit->first << "]";
                    stringstream boolval2; boolval2 << criteria[crit->first];
                    replaceString(nesting, token2.str(), boolval2.str()); 
                }
                replaceString(nesting, "and", "&");
                replaceString(nesting, "or", "|");
                nesting += ";";
                AGO_TRACE() << "nesting prepared: " << nesting;
                if (evaluateNesting(nesting))
                {
                    AGO_DEBUG() << "sending event action as command";
                    agoConnection->sendMessage(event["action"].asMap());
                }
            }	
        }
    }
}

qpid::types::Variant::Map AgoEvent::commandHandler(qpid::types::Variant::Map content)
{
    qpid::types::Variant::Map responseData;
    std::string internalid = content["internalid"].asString();
    if (internalid == "eventcontroller")
    {
        if (content["command"] == "setevent")
        {
            checkMsgParameter(content, "eventmap", VAR_MAP);

            AGO_DEBUG() << "setevent request";
            qpid::types::Variant::Map newevent = content["eventmap"].asMap();
            std::string eventuuid = content["event"].asString();
            if (eventuuid == "")
                eventuuid = generateUuid();

            AGO_TRACE() << "event content:" << newevent;
            AGO_TRACE() << "event uuid:" << eventuuid;
            eventmap[eventuuid] = newevent;

            agoConnection->addDevice(eventuuid.c_str(), "event", true);
            if (variantMapToJSONFile(eventmap, getConfigPath(EVENTMAPFILE)))
            {
                responseData["event"] = eventuuid;
                return responseSuccess(responseData);
            }
            else
            {
                return responseFailed("Failed to write map file");
            }
        }
        else if (content["command"] == "getevent")
        {
            checkMsgParameter(content, "event", VAR_STRING);

            std::string event = content["event"].asString();

            AGO_DEBUG() << "getevent request:" << event;
            responseData["eventmap"] = eventmap[event].asMap();
            responseData["event"] = event;

            return responseSuccess(responseData);
        }
        else if (content["command"] == "delevent")
        {
            checkMsgParameter(content, "event", VAR_STRING);

            std::string event = content["event"].asString();
            AGO_DEBUG() << "delevent request:" << event;

            qpid::types::Variant::Map::iterator it = eventmap.find(event);
            if (it != eventmap.end())
            {
                AGO_TRACE() << "removing ago device" << event;
                agoConnection->removeDevice(it->first.c_str());
                eventmap.erase(it);
                if (!variantMapToJSONFile(eventmap, getConfigPath(EVENTMAPFILE)))
                {
                    return responseFailed("Failed to write map file");
                }
            }

            // If it was not found, it was already deleted; delete succeded
            return responseSuccess();
        }else{
            return responseUnknownCommand();
        }
    }

    // We do not support sending commands to our 'devices'
    return responseNoDeviceCommands();
}

void AgoEvent::setupApp()
{
    AGO_DEBUG() << "parsing eventmap file" << endl;
    fs::path file = ensureParentDirExists(getConfigPath(EVENTMAPFILE));
    eventmap = jsonFileToVariantMap(file);

    addCommandHandler();
    addEventHandler();

    agoConnection->addDevice("eventcontroller", "eventcontroller");

    for (qpid::types::Variant::Map::const_iterator it = eventmap.begin(); it!=eventmap.end(); it++)
    {
        AGO_DEBUG() << "adding event:" << it->first << ":" << it->second << endl;	
        agoConnection->addDevice(it->first.c_str(), "event", true);
    }
}

AGOAPP_ENTRY_POINT(AgoEvent);
