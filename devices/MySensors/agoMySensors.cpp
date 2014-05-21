#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <iostream>
#include <boost/asio.hpp> 
#include <boost/system/system_error.hpp> 

#include "MySensors.h"
#include "agoclient.h"

#ifndef DEVICEMAPFILE
#define DEVICEMAPFILE CONFDIR "/maps/mysensors.json"
#endif
#define RESEND_MAX_ATTEMPTS 30

using namespace std;
using namespace agocontrol;
using namespace boost::system; 
using namespace boost::asio; 
using namespace qpid::types;

typedef struct S_COMMAND {
    std::string command;
    int attempts;
} T_COMMAND;

int DEBUG = 0;
AgoConnection *agoConnection;
pthread_mutex_t serialMutex;
pthread_mutex_t resendMutex;
int serialFd = 0;
pthread_t readThread;
pthread_t resendThread;
string units = "M";
qpid::types::Variant::Map devicemap;
std::map<std::string, T_COMMAND> commandsmap;

io_service ioService; 
serial_port serialPort(ioService); 

/**
 * Split specified string
 */
std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems) {
	std::stringstream ss(s);
	std::string item;
	while (std::getline(ss, item, delim)) {
		elems.push_back(item);
	}
	return elems;
}
std::vector<std::string> split(const std::string &s, char delimiter) {
	std::vector<std::string> elements;
	split(s, delimiter, elements);
	return elements;
}

/**
 * Make readable device infos
 */
std::string printDeviceInfos(std::string internalid, qpid::types::Variant::Map infos) {
    std::stringstream result;
    result << "Infos of device internalid '" << internalid << "'" << endl;
    if( !infos["type"].isVoid() )
        result << " - type=" << infos["type"] << endl;
    if( !infos["value"].isVoid() )
        result << " - value=" << infos["value"] << endl;
    if( !infos["counter_sent"].isVoid() )
        result << " - counter_sent=" << infos["counter_sent"] << endl;
    if( !infos["counter_resent"].isVoid() )
        result << " - counter_resent=" << infos["counter_resent"] << endl;
    if( !infos["counter_received"].isVoid() )
        result << " - counter_received=" << infos["counter_received"] << endl;
    if( !infos["counter_failed"].isVoid() )
        result << " - counter_failed=" << infos["counter_failed"] << endl;
    return result.str();
}

/**
 * Make readable received message from MySensor gateway
 */
std::string prettyPrint(std::string message) {
	int radioId, childId, messageType, subType;
	std::stringstream result;

	std::vector<std::string> items = split(message, ';');
	if (items.size() < 4 || items.size() > 5) {
		result << "ERROR, malformed string: " << message << endl;
	} else {
		std::string payload;
		if (items.size() == 5) payload=items[4];
		result << items[0] << "/" << items[1] << ";" << getMsgTypeName((msgType)atoi(items[2].c_str())) << ";";
		switch (atoi(items[2].c_str())) {
			 case PRESENTATION:
				result << getDeviceTypeName((deviceTypes)atoi(items[3].c_str()));
				break;
			case SET_VARIABLE:
				result << getVariableTypeName((varTypes)atoi(items[3].c_str()));
				break;
			case REQUEST_VARIABLE:
				result << getVariableTypeName((varTypes)atoi(items[3].c_str()));
				break;
			case VARIABLE_ACK:
				result << getVariableTypeName((varTypes)atoi(items[3].c_str()));
				break;
			case INTERNAL:
				result << getInternalTypeName((internalTypes)atoi(items[3].c_str()));
				break;
			default:
				result << items[3];
		}
		result <<  ";" << payload << std::endl;

	}
	return result.str();
}

/**
 * Get infos of specified device
 */
qpid::types::Variant::Map getDeviceInfos(std::string internalid) {
    qpid::types::Variant::Map out;
	qpid::types::Variant::Map devices = devicemap["devices"].asMap();
    if( devices.count(internalid)==1 ) {
        return devices[internalid].asMap();
    }
    return out;
}

/**
 * Save specified device infos
 */
void setDeviceInfos(std::string internalid, qpid::types::Variant::Map* infos) {
    qpid::types::Variant::Map device = devicemap["devices"].asMap();
    device[internalid] = (*infos);
    devicemap["devices"] = device;
    variantMapToJSONFile(devicemap,DEVICEMAPFILE);
}

/**
 * Send command to MySensor gateway
 */
void sendcommand(std::string command) {
    if( DEBUG )
        cout << " => RE-SENDING: " << command;
    serialPort.write_some(buffer(command));
}
void sendcommand(std::string internalid, int messageType, int subType, std::string payload) {
    std::vector<std::string> items = split(internalid, '/');
    stringstream command;
    qpid::types::Variant::Map infos = getDeviceInfos(internalid);

    //prepare command
    int radioId = atoi(items[0].c_str());
    int childId = atoi(items[1].c_str());
    command << radioId << ";" << childId << ";" << messageType << ";" << subType << ";" << payload << "\n";

    //save command if device is an actuator and message type is SET_VARIABLE
    if( infos.size()>0 && infos["type"]=="switch" && messageType==SET_VARIABLE ) {
        T_COMMAND cmd;
        cmd.command = command.str();
        cmd.attempts = 0;
		pthread_mutex_lock(&resendMutex);
        commandsmap[internalid] = cmd;
		pthread_mutex_unlock(&resendMutex);
    }

    //send command
    if( DEBUG )
        cout << " => SENDING: " << command.str();
    serialPort.write_some(buffer(command.str()));
}

/**
 * Agocontrol command handler
 */
qpid::types::Variant::Map commandHandler(qpid::types::Variant::Map command) {
	qpid::types::Variant::Map returnval;
    qpid::types::Variant::Map infos;
    std::string deviceType = "";
    std::string cmd = "";
    std::string internalid = "";
    if( DEBUG )
        cout << "CommandHandler" << command << endl;
    if( command.count("internalid")==1 && command.count("command")==1 ) {
        //get values
        cmd = command["command"].asString();
        internalid = command["internalid"].asString();

        //switch to specified command
        if( cmd=="getcounters" ) {
            //return devices counters
            qpid::types::Variant::Map devices = devicemap["devices"].asMap();
	        for (qpid::types::Variant::Map::const_iterator it = devices.begin(); it != devices.end(); it++) {
                returnval[it->first.c_str()] = it->second.asMap();
        	}
        }
        else if( cmd=="resetcounters" ) {
            //reset all counters
            qpid::types::Variant::Map devices = devicemap["devices"].asMap();
	        for (qpid::types::Variant::Map::iterator it = devices.begin(); it != devices.end(); it++) {
                infos = getDeviceInfos(it->first);
                if( infos.size()>0 ) {
                    if( !infos["counter_received"].isVoid() ) {
                        infos["counter_received"] = 0;
                    }
                    if( !infos["counter_sent"].isVoid() ) {
                        infos["counter_sent"] = 0;
                    }
                    if( !infos["counter_resent"].isVoid() ) {
                        infos["counter_resent"] = 0;
                    }
                    if( !infos["counter_failed"].isVoid() ) {
                        infos["counter_failed"] = 0;
                    }
                    setDeviceInfos(it->first, &infos);
                }
        	}
        }
        else {
            //get device infos
            infos = getDeviceInfos(command["internalid"].asString());
            //check if device found
            if( infos.size()>0 ) {
                deviceType = infos["type"].asString();
                //switch according to specific device type
                if( deviceType=="switch" ) {
                    if( cmd=="off" ) {
                        infos["value"] = "0";
                        setDeviceInfos(internalid, &infos);
                        sendcommand(internalid, SET_VARIABLE, V_LIGHT, "0");
                    }
                    else if( cmd=="on" ) {
                        infos["value"] = "1";
                        setDeviceInfos(internalid, &infos);
                        sendcommand(internalid, SET_VARIABLE, V_LIGHT, "1");
                    }
                }
                //TODO add more device type here
                returnval["error"] = 0;
                returnval["msg"] = "";
            }
            else {
                //internalid doesn't belong to this controller
                returnval["error"] = 1;
                returnval["msg"] = "Unmanaged internalid";
            }
        }
    }
	return returnval;
}

/**
 * Read line from Serial
 */
std::string readLine() {
        char c;
        std::string result;
        for(;;)
        {
		boost::asio::read(serialPort,boost::asio::buffer(&c,1));
		switch(c)
		{
			case '\r':
			    break;
			case '\n':
			    return result;
			default:
			    result+=c;
		}
        }
	return result;
}

/**
 * Save all necessary infos for new device and register it to agocontrol
 */
void addDevice(std::string internalid, std::string devicetype, qpid::types::Variant::Map devices, qpid::types::Variant::Map infos) {
    infos["type"] = devicetype;
    infos["value"] = "0";
    infos["counter_sent"] = 0;
    infos["counter_received"] = 0;
    infos["counter_resent"] = 0;
    infos["counter_failed"] = 0;
    devices[internalid] = infos;
	devicemap["devices"] = devices;
	variantMapToJSONFile(devicemap,DEVICEMAPFILE);
    agoConnection->addDevice(internalid.c_str(), devicetype.c_str());
}

/**
 * Create new device (called during init or when PRESENTATION message is received from MySensors gateway)
 */
void newDevice(std::string internalid, std::string devicetype) {
    //init
	qpid::types::Variant::Map devices = devicemap["devices"].asMap();
    qpid::types::Variant::Map infos = getDeviceInfos(internalid);

    if( infos.size()>0 ) {
        //internalid already referenced
        if( infos["type"].asString()!=devicetype ) {
            //sensors is probably reconditioned, remove it before adding it
            cout << "Reconditioned sensor detected (oldType=" << infos["type"] << " newType=" << devicetype << ")" << endl;
            agoConnection->removeDevice(internalid.c_str());
            addDevice(internalid, devicetype, devices, infos);
        }
        else {
            //sensor has just rebooted
            addDevice(internalid, devicetype, devices, infos);
        }
    }
    else {
        //add new device
        addDevice(internalid, devicetype, devices, infos);
    }
}

/**
 * Resend function (threaded)
 * Allow to send again a command until ack is received (only for certain device type)
 */
void* resendFunction(void* param) {
    qpid::types::Variant::Map infos;
    while(1) {
		pthread_mutex_lock(&resendMutex);
        for( std::map<std::string,T_COMMAND>::iterator it=commandsmap.begin(); it!=commandsmap.end(); it++ ) {
            if( it->second.attempts<RESEND_MAX_ATTEMPTS ) {
                //resend command
                sendcommand(it->second.command);
                it->second.attempts++;
                //update counter
                infos = getDeviceInfos(it->first);
                if( infos.size()>0 ) {
                    if( infos["counter_resent"].isVoid() ) {
                        infos["counter_resent"] = 1;
                    }
                    else {
                        infos["counter_resent"] = infos["counter_resent"].asUint64()+1;
                    }
                    setDeviceInfos(it->first, &infos);
                }
            }
            else {
                //max attempts reached, log and delete command from list
                commandsmap.erase(it->first);

                //increase counter
                qpid::types::Variant::Map infos = getDeviceInfos(it->first);
                if( infos.size()>0 ) {
                    if( infos["counter_failed"].isVoid() ) {
                        infos["counter_failed"] = 1;
                    }
                    else {
                        infos["counter_failed"] = infos["counter_failed"].asUint64()+1;
                    }
                    setDeviceInfos(it->first, &infos);
                }
            }
        }
		pthread_mutex_unlock(&resendMutex);
        usleep(500000);
    }
    return NULL;
}

/**
 * Serial read function (threaded)
 */
void *receiveFunction(void *param) {
	while (1) {
		pthread_mutex_lock (&serialMutex);

		std::string line = readLine();
        if( DEBUG )
    		cout << " => RECEIVING: " << prettyPrint(line);
		std::vector<std::string> items = split(line, ';');
		if (items.size() > 3 && items.size() < 6) {
			int radioId = atoi(items[0].c_str());
			int childId = atoi(items[1].c_str());
			string internalid = items[0] + "/" + items[1];
			int messageType = atoi(items[2].c_str());
			int subType = atoi(items[3].c_str());
            int valid = 0;
			string payload;
            qpid::types::Variant::Map infos;
            
            //get device infos
            infos = getDeviceInfos(internalid);

			if (items.size() ==5) payload = items[4];
			switch (messageType) {
				case INTERNAL:
					switch (subType) {
						case I_BATTERY_LEVEL:
							break; // TODO: emit battery level event
						case I_TIME:
							{
								stringstream timestamp;
								timestamp << time(NULL);
								sendcommand(internalid, INTERNAL, I_TIME, timestamp.str());
							}
							break;
						case I_REQUEST_ID:
							{
                                //return radio id to sensor
								stringstream id;
								int freeid = devicemap["nextid"];
                                //@info radioId - The unique id (1-254) for this sensor. Default 255 (auto mode).
                                if( freeid>=254 ) {
                                    cerr << "FATAL: no radioId available!" << endl;
                                }
                                else {
      								devicemap["nextid"]=freeid+1;
	    							variantMapToJSONFile(devicemap,DEVICEMAPFILE);
								    id << freeid;
    								sendcommand(internalid, INTERNAL, I_REQUEST_ID, id.str());
                                }
							}
							break;
						case I_PING:
							sendcommand(internalid, INTERNAL, I_PING_ACK, "");
							break;
						case I_UNIT:
							sendcommand(internalid, INTERNAL, I_UNIT, units);
							break;
					}
					break;
				case PRESENTATION:
                    cout << "PRESENTATION: " << subType << endl;
					switch (subType) {
						case S_DOOR:
						case S_MOTION:
							newDevice(internalid, "binarysensor");
							break;
						case S_SMOKE:
							newDevice(internalid, "smokedetector");
							break;
						case S_LIGHT:
						case S_HEATER:
							newDevice(internalid, "switch");
							break;
						case S_DIMMER:
							newDevice(internalid, "dimmer");
							break;
						case S_COVER:
							newDevice(internalid, "drapes");
							break;
						case S_TEMP:
							newDevice(internalid, "temperaturesensor");
							break;
						case S_HUM:
							newDevice(internalid, "humiditysensor");
							break;
						case S_BARO:
							newDevice(internalid, "barometricsensor");
							break;
						case S_WIND:
							newDevice(internalid, "windsensor");
							break;
						case S_RAIN:
							newDevice(internalid, "rainsensor");
							break;
						case S_UV:
							newDevice(internalid, "uvsensor");
							break;
						case S_WEIGHT:
							newDevice(internalid, "weightsensor");
							break;
						case S_POWER:
							newDevice(internalid, "powermeter");
							break;
						case S_DISTANCE:
							newDevice(internalid, "distancesensor");
							break;
						case S_LIGHT_LEVEL:
							newDevice(internalid, "brightnesssensor");
							break;
						case S_LOCK:
							newDevice(internalid, "lock");
							break;
						case S_IR:
							newDevice(internalid, "infraredblaster");
							break;
						case S_WATER:
							newDevice(internalid, "watermeter");
							break;
					}
					break;
				case REQUEST_VARIABLE:
                    if( infos.size()>0 ) {
                        //increase counter
                        if( infos["counter_sent"].isVoid() ) {
                            infos["counter_sent"] = 1;
                        }
                        else {
                            infos["counter_sent"] = infos["counter_sent"].asUint64()+1;
                        }
                        setDeviceInfos(internalid, &infos);
                        //send value
                        sendcommand(internalid, SET_VARIABLE, subType, infos["value"]);
                    }
                    else {
                        //device not found
                        //TODO log flood!
                        cerr  << "Device not found: unable to get its value" << endl;
                    }
					break;
				case SET_VARIABLE:
                    //remove command from map to avoid sending command again
		            pthread_mutex_lock(&resendMutex);
                    if( commandsmap.count(internalid)!=0 ) {
                        commandsmap.erase(internalid);
                    }
            		pthread_mutex_unlock(&resendMutex);

                    //increase counter
                    if( infos.size()>0 ) {
                        if( infos["counter_received"].isVoid() ) {
                            infos["counter_received"] = 1;
                        }
                        else {
                            infos["counter_received"] = infos["counter_received"].asUint64()+1;
                        }
                        setDeviceInfos(internalid, &infos);
                    }

                    //do something on received event
					switch (subType) {
						case V_TEMP:
                            valid = 1;
							if (units == "M") {
								agoConnection->emitEvent(internalid.c_str(), "event.environment.temperaturechanged", payload.c_str(), "degC");
							} else {
								agoConnection->emitEvent(internalid.c_str(), "event.environment.temperaturechanged", payload.c_str(), "degF");
							}
							break;
						case V_TRIPPED:
                            valid = 1;
							agoConnection->emitEvent(internalid.c_str(), "event.security.sensortriggered", payload == "1" ? 255 : 0, "");
							break;
						case V_HUM:
                            valid = 1;
							agoConnection->emitEvent(internalid.c_str(), "event.environment.humiditychanged", payload.c_str(), "percent");
							break;
						case V_LIGHT:
                            valid = 1;
							agoConnection->emitEvent(internalid.c_str(), "event.device.statechanged", payload=="1" ? 255 : 0, "");
							break;
						case V_DIMMER:
                            valid = 1;
							agoConnection->emitEvent(internalid.c_str(), "event.device.statechanged", payload.c_str(), "");
							break;
						case V_PRESSURE:
                            valid = 1;
							agoConnection->emitEvent(internalid.c_str(), "event.environment.pressurechanged", payload.c_str(), "mBar");
							break;
						case V_FORECAST: break;
						case V_RAIN: break;
						case V_RAINRATE: break;
						case V_WIND: break;
						case V_GUST: break;
						case V_DIRECTION: break;
						case V_UV: break;
						case V_WEIGHT: break;
						case V_DISTANCE: break;
						case V_IMPEDANCE: break;
						case V_ARMED: break;
						case V_WATT: break;
						case V_KWH: break;
						case V_SCENE_ON: break;
						case V_SCENE_OFF: break;
						case V_HEATER: break;
						case V_HEATER_SW: break;
						case V_LIGHT_LEVEL:
                            valid = 1;
							agoConnection->emitEvent(internalid.c_str(), "event.environment.brightnesschanged", payload.c_str(), "lux");
                            break;
						case V_VAR1: break;
						case V_VAR2: break;
						case V_VAR3: break;
						case V_VAR4: break;
						case V_VAR5: break;
						case V_UP: break;
						case V_DOWN: break;
						case V_STOP: break;
						case V_IR_SEND: break;
						case V_IR_RECEIVE: break;
						case V_FLOW: break;
						case V_VOLUME: break;
						case V_LOCK_STATUS: break;
						default:
							break;
					}

                    if( valid==1 ) {
                        //save current device value
                        infos = getDeviceInfos(internalid);
                        if( infos.size()>0 ) {
                            infos["value"] = payload;
                            setDeviceInfos(internalid, &infos);
                        }
                    }
                    else {
                        //unsupported sensor
                        cerr << "WARN: sensor with subType=" << subType << " not supported yet" << endl;
                    }

                    //send ack
                    sendcommand(internalid, VARIABLE_ACK, subType, payload);
					break;
				case VARIABLE_ACK:
                    //TODO useful on controller?
                    cout << "VARIABLE_ACK" << endl;
					break;
				default:
					break;
			}

		}
		pthread_mutex_unlock (&serialMutex);
		
	}
	return NULL;
}

/**
 * main
 */
int main(int argc, char **argv) {
    std::string device;
    device=getConfigOption("mysensors", "device", "/dev/ttyACM0");

    //get command line parameters
    bool continu = true;
    do
    {
        switch(getopt(argc,argv,"d"))
        {
            case 'd': 
                //activate debug
                DEBUG = 1;
                cout << "DEBUG activated" << endl;
                break;
            default:
                continu = false;
                break;
        }
    } while(continu);

    // determine reply for INTERNAL;I_UNIT message - defaults to "M"etric
    if (getConfigOption("system","units","SI")!="SI") units="I";

    try {
        serialPort.open(device); 
        serialPort.set_option(serial_port::baud_rate(115200)); 
        serialPort.set_option(serial_port::parity(serial_port::parity::none)); 
        serialPort.set_option(serial_port::character_size(serial_port::character_size(8))); 
        serialPort.set_option(serial_port::stop_bits(serial_port::stop_bits::one)); 
        serialPort.set_option(serial_port::flow_control(serial_port::flow_control::none)); 
    } catch(std::exception const&  ex) {
        cerr  << "Can't open serial port:" << ex.what() << endl;
        exit(1);
    }

    // load map, create sections if empty
    devicemap = jsonFileToVariantMap(DEVICEMAPFILE);
    if (devicemap["nextid"].isVoid()) {
        devicemap["nextid"]=1;
        variantMapToJSONFile(devicemap,DEVICEMAPFILE);
    }
    if (devicemap["devices"].isVoid()) {
        qpid::types::Variant::Map devices;
        devicemap["devices"]=devices;
        variantMapToJSONFile(devicemap,DEVICEMAPFILE);
    }

    //cout << readLine() << endl;
    cout << "Requesting gateway version: ";
    std::string getVersion = "0;0;4;4;\n";
    serialPort.write_some(buffer(getVersion));
    cout << readLine() << endl;

    //init agocontrol client
    cout << "Initializing MySensor controller" << endl;
    agoConnection = new AgoConnection("mysensors");
    agoConnection->addDevice("mysensorscontroller", "mysensorscontroller");
    agoConnection->addHandler(commandHandler);

    //init threads
    pthread_mutex_init(&serialMutex, NULL);
    pthread_mutex_init(&resendMutex, NULL);
    if( pthread_create(&resendThread, NULL, resendFunction, NULL) < 0 ) {
        cerr << "Unable to create resend thread (errno=" << errno << ")" << endl;
        exit(1);
    }
    if( pthread_create(&readThread, NULL, receiveFunction, NULL) < 0 ) {
        cerr << "Unable to create read thread (errno=" << errno << ")" << endl;
        exit(1);
    }

    //register existing devices
    cout << "Register existing devices:" << endl;
    qpid::types::Variant::Map devices = devicemap["devices"].asMap();
    for (qpid::types::Variant::Map::const_iterator it = devices.begin(); it != devices.end(); it++) {
        qpid::types::Variant::Map infos = it->second.asMap();
        cout << " - " << it->first.c_str() << ":" << infos["type"].asString().c_str() << endl;
        agoConnection->addDevice(it->first.c_str(), (infos["type"].asString()).c_str());	
    }

    //run client
    cout << "Running MySensor controller..." << endl;
    agoConnection->run();
}
