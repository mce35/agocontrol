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

class AgoSecurity: public AgoApp {
private:
    std::string agocontroller;
    boost::thread securityThread;
    bool isSecurityThreadRunning;
    bool wantSecurityThreadRunning;
    qpid::types::Variant::Map securitymap;

    /* example map:

       {
       "housemode": "armed",
       "zones": {
       "away": [
       {
       "delay": 15,
       "zone": "foyer"
       }
       ],
       "armed": [
       {
       "delay": 12,
       "zone": "hull"
       },
       {
       "delay": 0,
       "zone": "garden"
       }
       ]
       }

*/

    bool checkPin(std::string _pin) ;
    bool findList(qpid::types::Variant::List list, std::string elem) ;
    int getZoneDelay(qpid::types::Variant::Map smap, std::string elem) ;
    void alarmthread(std::string zone) ;
    qpid::types::Variant::Map commandHandler(qpid::types::Variant::Map content) ;

    void setupApp() ;

public:
    AGOAPP_CONSTRUCTOR_HEAD(AgoSecurity)
        , isSecurityThreadRunning(false)
        , wantSecurityThreadRunning(false) {}
};

bool AgoSecurity::checkPin(std::string _pin) {
    stringstream pins(getConfigOption("pin", "0815"));
    string pin;
    while (getline(pins, pin, ',')) {
        if (_pin == pin) return true;
    }
    return false;
}


bool AgoSecurity::findList(qpid::types::Variant::List list, std::string elem) {
    //qpid::types::Variant::List::const_iterator it = std::find(list.begin(), list.end(), elem);
    for (qpid::types::Variant::List::const_iterator it = list.begin(); it != list.end(); it++) {
        if (it->getType() == qpid::types::VAR_MAP) {
            qpid::types::Variant::Map map = it->asMap();
            if (map["zone"].asString() == elem) {
                // AGO_TRACE() << "found zone: " << map["zone"];
                return true;
            }
        }

    }
    return false;
}

int AgoSecurity::getZoneDelay(qpid::types::Variant::Map smap, std::string elem) {
    qpid::types::Variant::Map zonemap;
    qpid::types::Variant::List list;
    std::string housemode = securitymap["housemode"];
    if (!(smap["zones"].isVoid())) zonemap = smap["zones"].asMap();
    if (!(zonemap[housemode].isVoid())) {
        if (zonemap[housemode].getType() == qpid::types::VAR_LIST) {
            list = zonemap[housemode].asList();
            for (qpid::types::Variant::List::const_iterator it = list.begin(); it != list.end(); it++) {
                if (it->getType() == qpid::types::VAR_MAP) {
                    qpid::types::Variant::Map map = it->asMap();
                    if (map["zone"].asString() == elem) {
                        // AGO_TRACE() << "found zone: " << map["zone"];
                        if (map["delay"].isVoid()) {
                            return 0;
                        } else {
                            int delay = map["delay"].asUint8();
                            return delay;
                        }
                    }
                }

            }
        }
    }
    return 0;
}

void AgoSecurity::alarmthread(std::string zone) {
    int delay=getZoneDelay(securitymap, zone);	
    try {
        Variant::Map content;
        content["zone"]=zone;
        AGO_INFO() << "Alarm triggered, zone: " << zone << " delay: " << delay;
        while (wantSecurityThreadRunning && delay-- > 0) {
            Variant::Map countdowneventcontent;
            countdowneventcontent["delay"]=delay;
            countdowneventcontent["zone"]=zone;
            AGO_TRACE() << "count down: " << delay;
            agoConnection->emitEvent("securitycontroller", "event.security.countdown", countdowneventcontent);
            boost::this_thread::sleep(pt::seconds(1));
        }

        AGO_INFO() << "Countdown expired for " << zone << ", sending alarm event";
        agoConnection->emitEvent("securitycontroller", "event.security.intruderalert", content);
    }catch(boost::thread_interrupted &e) {
        AGO_DEBUG() << "Alarmthread cancelled";
    }

    isSecurityThreadRunning = false;
    wantSecurityThreadRunning = false;
    AGO_DEBUG() << "Alarm thread exited";
}

qpid::types::Variant::Map AgoSecurity::commandHandler(qpid::types::Variant::Map content) {
    AGO_TRACE() << "handling command: " << content;
    qpid::types::Variant::Map returnval;
    qpid::types::Variant::Map returncode;

    std::string internalid = content["internalid"].asString();

    if (internalid == "securitycontroller") {
        if (content["command"] == "sethousemode") {
            // TODO: handle delay
            if (content["mode"].asString() != "") {
                if (checkPin(content["pin"].asString())) {
                    securitymap["housemode"] = content["mode"].asString();
                    AGO_INFO() << "setting mode: " << content["mode"];
                    agoConnection->setGlobalVariable("housemode", content["mode"]);
                    Variant::Map eventcontent;
                    eventcontent["housemode"]= content["mode"].asString();
                    agoConnection->emitEvent("securitycontroller", "event.security.housemodechanged", eventcontent);

                    if (variantMapToJSONFile(securitymap, getConfigPath(SECURITYMAPFILE))) {
                        returncode["code"]="success";
                        returncode["message"]="housemode changed";
                        returnval["result"] = returncode;
                    } else {
                        returncode["code"]="error.security.housemodechange";
                        returncode["message"]="cannot write securitymap on housemode change";
                        returnval["error"] = returncode;
                    }
                } else {
                    AGO_ERROR() << "invalid pin";
                    returncode["code"]="error.security.invalidpin";
                    returncode["message"]="invalid pin specified";
                    returnval["error"] = returncode;
                }
            } else {
                returncode["code"]="error.parameter.missing";
                returncode["message"]="parameter 'housemode' is missing";
                returnval["error"] = returncode;
            }

        } else if (content["command"] == "gethousemode") {
            if (!(securitymap["housemode"].isVoid())) {
                qpid::types::Variant::Map returndata;
                returndata["housemode"]= securitymap["housemode"];
                returncode["data"] = returndata;
                returncode["housemode"] = securitymap["housemode"]; // FIXME: added for backward compatibility
                
                returnval["result"]=returncode;
            } else {
                returncode["code"] = "error.security.housemodenotset";
                returncode["message"] = "No housemode set";
                returnval["error"] = returncode;
            }
        } else if (content["command"] == "triggerzone") {
            std::string zone = content["zone"];
            qpid::types::Variant::Map zonemap;
            std::string housemode = securitymap["housemode"];
            AGO_DEBUG() << "Housemode: " << housemode << " Zone: " << zone;
            if (!(securitymap["zones"].isVoid())) zonemap = securitymap["zones"].asMap();
            if (!(zonemap[housemode].isVoid())) {
                if (zonemap[housemode].getType() == qpid::types::VAR_LIST) {
                    // let's see if the zone is active in the current house mode
                    if (findList(zonemap[housemode].asList(), zone)) {
                        // check if there is already an alarmthread running
                        if (isSecurityThreadRunning == false) {
                            try {
                                // XXX: Does not handle multiple zone alarms.
                                wantSecurityThreadRunning = true;
                                securityThread = boost::thread(boost::bind(&AgoSecurity::alarmthread, this, zone));
                                isSecurityThreadRunning = true;
                                returnval["result"] = 0;
                            }catch(std::exception& e){
                                AGO_FATAL() << "failed to start alarmthread! " << e.what();
                                returncode["code"]="error.security.alarmthreadfailed";
                                returncode["message"]="failed to start alarmthread";
                                returnval["error"] = returncode;
                            }
                        } else {
                            AGO_DEBUG() << "alarmthread already running";
                            returncode["code"]="success";
                            returncode["message"]="alarmthread already running";
                            returnval["result"] = returncode;
                        }
                    } else {
                        returncode["message"] = "no such zone in this housemode";
                        returncode["code"] = "success";
                        returnval["result"] = returncode;
                    }
                } else {
                    returncode["code"]="error.security.invalidmap";
                    returncode["message"]="invalid map, zonemap[housemode] is not a list!";
                    returnval["error"] = returncode;
                }	
            } else {
                returncode["code"]="error.security.invalidmap";
                returncode["message"]="invalid map, zonemap[housemode] is void!";
                returnval["error"] = returncode;
            }
        } else if (content["command"] == "setzones") {
            try {
                AGO_TRACE() << "setzones request";

                if (checkPin(content["pin"].asString())) {
                    qpid::types::Variant::Map newzones = content["zonemap"].asMap();
                    AGO_TRACE() << "zone content:" << newzones;
                    securitymap["zones"] = newzones;
                    if (variantMapToJSONFile(securitymap, getConfigPath(SECURITYMAPFILE))) {
                        returncode["code"]="success";
                        returnval["result"] = returncode;
                    } else {
                        returncode["message"]="cannot save securitymap";
                        returncode["code"]="error.security.setzones";
                        returnval["error"] = returncode;
                    }
                } else {
                    AGO_ERROR() << "invalid pin";
                    returncode["code"]="error.security.invalidpin";
                    returncode["message"]="invalid pin specified";
                    returnval["error"] = returncode;
                }
            } catch (qpid::types::InvalidConversion) {
                AGO_ERROR() << "invalid conversion";
                returncode["message"]="invalid conversion when parsing securitymap";
                returncode["code"]="error.security.setzones";
                returnval["error"] = returncode;
            } catch (const exception& e) {
                AGO_ERROR() << "exception in setzones: " << e.what();
                returncode["message"]="exception when parsing securitymap";
                returncode["code"]="error.security.setzones";
                returnval["error"] = returncode;
            }
        } else if (content["command"] == "getzones") {
            if (!(securitymap["zones"].isVoid())) {
                qpid::types::Variant::Map returndata;
                returndata["zonemap"] = securitymap["zones"].asMap();
                returncode["data"] = returndata;
                returncode["zonemap"] = securitymap["zones"].asMap(); // FIXME: added for backward compatibility
                returnval["result"]=returncode;
            } else {
                AGO_ERROR() << "securitymap is empty";
                returncode["message"]="securitymap is empty";
                returncode["code"]="error.security.invalidmap";
                returnval["error"] = returncode;
            }

        } else if (content["command"] == "cancel") {
            if (checkPin(content["pin"].asString())) {
                if (isSecurityThreadRunning) {
                    wantSecurityThreadRunning = false;
                    try {
                        securityThread.interrupt();
                        isSecurityThreadRunning = false;
                        AGO_INFO() << "alarm cancelled";
                        returncode["code"]="success";
                        returncode["message"]="alarm cancelled";
                        returnval["result"] = returncode;
                    }catch(std::exception& e){
                        AGO_ERROR() << "cannot cancel alarm thread!";
                        returncode["message"]="cannot cancel alarm thread";
                        returncode["code"]="error.security.alarmthreadcancelfailed";
                        returnval["error"] = returncode;
                    }

                } else {
                    AGO_ERROR() << "no alarm thread running";
                    returncode["message"]="no alarm thread running";
                    returncode["code"]="error.security.alarmthreadcancelfailed";
                    returnval["error"] = returncode;
                }
            } else {
                AGO_ERROR() << "invalid pin in cancel command";
                returncode["code"]="error.security.invalidpin";
                returncode["message"]="invalid pin specified";
                returnval["error"] = returncode;
            }
        } else {
            AGO_ERROR() << "no such command";
            returncode["code"]="error.command.invalid";
            returnval["error"] = returncode;
        }

    } else {
        returncode["code"]="error.device.invalid";
        returnval["error"] = returncode;
    }
    return returnval;
}

void AgoSecurity::setupApp() {
    securitymap = jsonFileToVariantMap(getConfigPath(SECURITYMAPFILE));

    AGO_TRACE() << "Loaded securitymap: " << securitymap;
    std::string housemode = securitymap["housemode"];
    AGO_DEBUG() << "Current house mode: " << housemode;
    agoConnection->setGlobalVariable("housemode", housemode);

    agoConnection->addDevice("securitycontroller", "securitycontroller");
    addCommandHandler();
}

AGOAPP_ENTRY_POINT(AgoSecurity);
