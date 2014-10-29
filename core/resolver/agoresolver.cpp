/*
	Copyright (C) 2012 Harald Klein <hari@vt100.at>

	This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License.
	This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
	of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	this is the core resolver component for ago control
*/

#include <iostream>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#include <termios.h>
#include <errno.h>
#include <stdlib.h>
#ifndef __FreeBSD__
#include <sys/sysinfo.h>
#endif

#include <sstream>
#include <map>
#include <deque>

#include <uuid/uuid.h>

#include <qpid/messaging/Connection.h>
#include <qpid/messaging/Message.h>
#include <qpid/messaging/Receiver.h>
#include <qpid/messaging/Sender.h>
#include <qpid/messaging/Session.h>
#include <qpid/messaging/Address.h>
#include <boost/asio/deadline_timer.hpp>

#include "agoapp.h"
#include "version.h"

#ifndef SCHEMADIR
#define SCHEMADIR "schema.d"
#endif

#ifndef INVENTORYDBFILE
#define INVENTORYDBFILE "db/inventory.db"
#endif

#ifndef VARIABLESMAPFILE
#define VARIABLESMAPFILE "maps/variablesmap.json"
#endif

#ifndef DEVICESMAPFILE
#define DEVICESMAPFILE "maps/devices.json"
#endif

#include "schema.h"
#include "inventory.h"

using namespace std;
using namespace qpid::messaging;
using namespace qpid::types;
using namespace agocontrol;
namespace fs = ::boost::filesystem;
namespace pt = ::boost::posix_time;

class AgoResolver: public AgoApp {
private:
	Variant::Map inventory; // used to hold device registrations
	Variant::Map schema;
	Variant::Map systeminfo; // holds system information
	Variant::Map variables; // holds global variables
	Variant::Map environment; // holds global environment like position, weather conditions, ..

	Inventory *inv;
	unsigned int discoverdelay;
	bool persistence;

	bool saveDevicemap();
	void loadDevicemap();
	void get_sysinfo() ;
	bool emitNameEvent(const char *uuid, const char *eventType, const char *name) ;
	bool emitFloorplanEvent(const char *uuid, const char *eventType, const char *floorplan, int x, int y) ;
	void handleEvent(Variant::Map *device, string subject, Variant::Map *content) ;

	void scanSchemaDir(const fs::path &schemaPrefix) ;
	qpid::types::Variant::Map commandHandler(qpid::types::Variant::Map content) ;
	void eventHandler(std::string subject, qpid::types::Variant::Map content) ;

	boost::asio::deadline_timer discoveryTimer;
	void discover(const boost::system::error_code& error) ;

	void setupApp();
	void cleanupApp();
public:
	AGOAPP_CONSTRUCTOR_HEAD(AgoResolver)
		, discoveryTimer(ioService()) {}

};

// helper to determine last element
#ifndef _LIBCPP_ITERATOR
template <typename Iter>
Iter next(Iter iter)
{
	return ++iter;
}
#endif

bool AgoResolver::saveDevicemap() {
	if (persistence) {
		AGO_TRACE() << "Saving device-map";
		return variantMapToJSONFile(inventory, getConfigPath(DEVICESMAPFILE));
	}
	return true;
}

void AgoResolver::loadDevicemap() {
	inventory = jsonFileToVariantMap(getConfigPath(DEVICESMAPFILE));
}

void AgoResolver::get_sysinfo() {
#ifndef __FreeBSD__
	/* Note on FreeBSD exclusion. Sysinfo.h does not exist, but the code below
	 * does not really use it anyway.. so just skip it.
	 */
	struct sysinfo s_info;
	int error;
	error = sysinfo(&s_info);
	if(error != 0) {
		AGO_ERROR() << "sysinfo error: " << error;
	} else { /*
		systeminfo["uptime"] = s_info.uptime;
		systeminfo["loadavg1"] = s_info.loads[0];
		systeminfo["loadavg5"] = s_info.loads[1];
		systeminfo["loadavg15"] = s_info.loads[2];
		systeminfo["totalram"] = s_info.totalram;
		systeminfo["freeram"] = s_info.freeram;
		systeminfo["procs"] = s_info.procs;
		*/
	}
#endif
}

bool AgoResolver::emitNameEvent(const char *uuid, const char *eventType, const char *name) {
	Variant::Map content;
	content["name"] = name;
	content["uuid"] = uuid;
	return agoConnection->sendMessage(eventType, content);
}

bool AgoResolver::emitFloorplanEvent(const char *uuid, const char *eventType, const char *floorplan, int x, int y) {
	Variant::Map content;
	content["uuid"] = uuid;
	content["floorplan"] = floorplan;
	content["x"] = x;
	content["y"] = y;
	return agoConnection->sendMessage(eventType, content);
}

string valuesToString(Variant::Map *values) {
	string result;
	for (Variant::Map::const_iterator it = values->begin(); it != values->end(); ++it) {
		result += it->second.asString();
		if ((it != values->end()) && (next(it) != values->end())) result += "/";
	}
	return result;
}

// handles events that update the state or values of a device
void AgoResolver::handleEvent(Variant::Map *device, string subject, Variant::Map *content) {
	Variant::Map *values;
	if ((*device)["values"].isVoid()) {
		AGO_ERROR() << "device[values] is empty in handleEvent()";
		return;
	}
	values = &(*device)["values"].asMap();
	if ((subject == "event.device.statechanged") || (subject == "event.security.sensortriggered")) {
		(*values)["state"] = (*content)["level"];
		(*device)["state"] = (*content)["level"];
		(*device)["state"].setEncoding("utf8");
		saveDevicemap();
		// (*device)["state"] = valuesToString(values);
	} else if (subject == "event.environment.positionchanged") {
		Variant::Map value;
		stringstream timestamp;

		value["unit"] = (*content)["unit"];
		value["latitude"] = (*content)["latitude"];
		value["longitude"] = (*content)["longitude"];

		timestamp << time(NULL);
		value["timestamp"] = timestamp.str();

		(*values)["position"] = value;
		saveDevicemap();

	} else if ( ((subject.find("event.environment.")!=std::string::npos) && (subject.find("changed")!=std::string::npos))
			|| (subject=="event.device.batterylevelchanged") ) {
		Variant::Map value;
		stringstream timestamp;
		string quantity = subject;

		//remove useless part of event (changed, event, environment ...)
		if( subject.find("event.environment.")!=std::string::npos )
		{
			//event.environment.XXXchanged
			replaceString(quantity, "event.environment.", "");
			replaceString(quantity, "changed", "");
		}
		else if( subject=="event.device.batterylevelchanged" )
		{
			replaceString(quantity, "event.device.", "");
			replaceString(quantity, "changed", "");
		}

		value["unit"] = (*content)["unit"];
		value["level"] = (*content)["level"];

		timestamp << time(NULL);
		value["timestamp"] = timestamp.str();

		(*values)[quantity] = value;
		saveDevicemap();
	}
}

qpid::types::Variant::Map AgoResolver::commandHandler(qpid::types::Variant::Map content) {
	std::string internalid = content["internalid"].asString();
	qpid::types::Variant::Map reply;
	if (internalid == "agocontroller") {
		if (content["command"] == "setroomname") {
			string uuid = content["room"];
			// if no uuid is provided, we need to generate one for a new room
			if (uuid == "") uuid = generateUuid();
			if (inv->setroomname(uuid, content["name"]) == 0) {
				reply["uuid"] = uuid;
				reply["returncode"] = 0;
				emitNameEvent(uuid.c_str(), "event.system.roomnamechanged", content["name"].asString().c_str());
			} else {
				reply["returncode"] = -1;
			}
		} else if (content["command"] == "setdeviceroom") {
			if ((content["device"].asString() != "") && (inv->setdeviceroom(content["device"], content["room"]) == 0)) {
				reply["returncode"] = 0;
				// update room in local device map
				Variant::Map *device;
				string room = inv->getdeviceroom(content["device"]);
				string uuid = content["device"];
				if (!inventory[uuid].isVoid()) {
					device = &inventory[uuid].asMap();
					(*device)["room"]= room;
				}
			} else {
				reply["returncode"] = -1;
			}
		} else if (content["command"] == "setdevicename") {
			if ((content["device"].asString() != "") && (inv->setdevicename(content["device"], content["name"]) == 0)) {
				reply["returncode"] = 0;
				// update name in local device map
				Variant::Map *device;
				string name = inv->getdevicename(content["device"]);
				string uuid = content["device"];
				if (!inventory[uuid].isVoid()) {
					device = &inventory[uuid].asMap();
					(*device)["name"]= name;
				}
				saveDevicemap();
				emitNameEvent(content["device"].asString().c_str(), "event.system.devicenamechanged", content["name"].asString().c_str());
			} else {
				reply["returncode"] = -1;
			}
		} else if (content["command"] == "deleteroom") {
			if (inv->deleteroom(content["room"]) == 0) {
				string uuid = content["room"].asString();
				emitNameEvent(uuid.c_str(), "event.system.roomdeleted", "");
				reply["returncode"] = 0;
			} else {
				reply["returncode"] = -1;
			}
		} else if (content["command"] == "setfloorplanname") {
			string uuid = content["floorplan"];
			// if no uuid is provided, we need to generate one for a new floorplan
			if (uuid == "") uuid = generateUuid();
			if (inv->setfloorplanname(uuid, content["name"]) == 0) {
				reply["uuid"] = uuid;
				reply["returncode"] = 0;
				emitNameEvent(content["floorplan"].asString().c_str(), "event.system.floorplannamechanged", content["name"].asString().c_str());
			} else {
				reply["returncode"] = -1;
			}
		} else if (content["command"] == "setdevicefloorplan") {
			if ((content["device"].asString() != "") && (inv->setdevicefloorplan(content["device"], content["floorplan"], content["x"], content["y"]) == 0)) {
				reply["returncode"] = 0;
				emitFloorplanEvent(content["device"].asString().c_str(), "event.system.floorplandevicechanged", content["floorplan"].asString().c_str(), content["x"], content["y"]);
			} else {
				reply["returncode"] = -1;
			}

		} else if (content["command"] == "deletefloorplan") {
			if (inv->deletefloorplan(content["floorplan"]) == 0) {
				reply["returncode"] = 0;
				emitNameEvent(content["floorplan"].asString().c_str(), "event.system.floorplandeleted", "");
			} else {
				reply["returncode"] = -1;
			}
		} else if (content["command"] == "setvariable") {
			if (content["variable"].asString() != "" && content["value"].asString() != "") {
				variables[content["variable"].asString()] = content["value"].asString();
				if (variantMapToJSONFile(variables, getConfigPath(VARIABLESMAPFILE))) {
					reply["returncode"] = 0;
				} else {
					reply["returncode"] = -1;
				}
			} else {
				reply["returncode"] = -1;
			}
		} else if (content["command"] == "delvariable") {
			if (content["variable"].asString() != "") {
				Variant::Map::iterator it = variables.find(content["variable"].asString());
				if (it != variables.end()) {
					variables.erase(it);
					if (variantMapToJSONFile(variables, getConfigPath(VARIABLESMAPFILE))) {
						reply["returncode"] = 0;
					} else {
						reply["returncode"] = -1;
					}
				} else {
					reply["returncode"] = -1;
				}
			}
		} else if (content["command"] == "getdevice") {
			if (content["device"].asString() != "") {
				if (!(inventory[content["device"].asString()].isVoid())) {
					reply["device"] = inventory[content["device"].asString()].asMap();
					reply["returncode"] = 0;
				} else {
					reply["returncode"] = -1;
				}
			} else {
				reply["returncode"] = -1;
			}
		} else if (content["command"] == "getconfigtree") {
			reply["config"] = getConfigTree();
			reply["returncode"] = 0;
		} else if (content["command"] == "setconfig") {
			if ((content["section"].asString() != "") && (content["option"].asString() != "") && (content["value"].asString() != "")) {
				setConfigOption(content["section"].asString().c_str(), content["option"].asString().c_str(), content["value"].asString().c_str());
				reply["returncode"]=0;
			} else {
				reply["returncode"]=-1;
			}
		}
	} else {
		if (content["command"] == "inventory") {
			// AGO_TRACE() << "responding to inventory request";
			for (qpid::types::Variant::Map::iterator it = inventory.begin(); it != inventory.end(); it++) {
				if (!it->second.isVoid()) {
					qpid::types::Variant::Map *device = &it->second.asMap();
					if (time(NULL) - (*device)["lastseen"].asUint64() > 2*discoverdelay) {
						// AGO_TRACE() << "Stale device: " << it->first;
						(*device)["stale"] = 1;
						saveDevicemap();
					}
				}
			}
			reply["devices"] = inventory;
			reply["schema"] = schema;
			reply["rooms"] = inv->getrooms();
			reply["floorplans"] = inv->getfloorplans();
			get_sysinfo();
			reply["system"] = systeminfo;
			reply["variables"] = variables;
			reply["environment"] = environment;
			reply["returncode"] = 0;
		}
	}
	return reply;
}

void AgoResolver::eventHandler(std::string subject, qpid::types::Variant::Map content) {
	if (subject == "event.device.announce") {
		string uuid = content["uuid"];
		if (uuid != "") {
			// AGO_TRACE() << "preparing device: uuid=" << uuid;
			Variant::Map device;
			Variant::Map values;
			device["devicetype"]=content["devicetype"].asString();
			device["internalid"]=content["internalid"].asString();
			device["handled-by"]=content["handled-by"].asString();
			// AGO_TRACE() << "getting name from inventory";
			device["name"]=inv->getdevicename(content["uuid"].asString());
			if (device["name"].asString() == "" && device["devicetype"] == "agocontroller") device["name"]="agocontroller";
			device["name"].setEncoding("utf8");
			// AGO_TRACE() << "getting room from inventory";
			device["room"]=inv->getdeviceroom(content["uuid"].asString());
			device["room"].setEncoding("utf8");
			uint64_t timestamp;
			timestamp = time(NULL);
			device["lastseen"] = timestamp;
			device["stale"] = 0;
			qpid::types::Variant::Map::const_iterator it = inventory.find(uuid);
			if (it == inventory.end()) {
				// device is newly announced, set default state and values
				device["state"]="0";
				device["state"].setEncoding("utf8");
				device["values"]=values;
				AGO_INFO() << "adding device: uuid=" << uuid << " type: " << device["devicetype"].asString();
			} else {
				// device exists, get current values
				// TODO: use a non-const interator and modify the timestamp in place to avoid the following copying of data
				qpid::types::Variant::Map olddevice;
				if (!it->second.isVoid()) {
					olddevice= it->second.asMap();
					device["state"] = olddevice["state"];
					device["values"] = olddevice["values"];
				}
			}
			inventory[uuid] = device;
			saveDevicemap();
		}
	} else if (subject == "event.device.remove") {
		string uuid = content["uuid"];
		if (uuid != "") {
			AGO_INFO() << "removing device: uuid=" << uuid;
			Variant::Map::iterator it = inventory.find(uuid);
			if (it != inventory.end()) {
				inventory.erase(it);
				saveDevicemap();
			}
		}
	} else if (subject == "event.environment.timechanged") {
		variables["hour"] = content["hour"].asString();
		variables["day"] = content["day"].asString();
		variables["weekday"] = content["weekday"].asString();
		variables["minute"] = content["minute"].asString();
		variables["month"] = content["month"].asString();
	}
	else if( subject=="event.device.stale" )
	{
		Variant::Map *device;
		string uuid = content["uuid"];
		if (!inventory[uuid].isVoid())
		{
			device = &inventory[uuid].asMap();
			(*device)["stale"] = content["stale"].asInt8();
			saveDevicemap();
		}
	}
	else {
		if (subject == "event.environment.positionchanged") {
			environment["latitude"] = content["latitude"];
			environment["longitude"] = content["longitude"];
		}
		if (content["uuid"].asString() != "") {
			string uuid = content["uuid"];
			// see if we have that device in the inventory already, if yes handle the event
			if (inventory.find(uuid) != inventory.end()) {
				if (!inventory[uuid].isVoid()) handleEvent(&inventory[uuid].asMap(), subject, &content);
			}
		}

	}
}

void AgoResolver::discover(const boost::system::error_code& error) {
	if(error) {
	  	return;
	}

	Variant::Map discovercmd;
	discovercmd["command"] = "discover";
	AGO_TRACE() << "Sending discover message";
	agoConnection->sendMessage("",discovercmd);

	discoveryTimer.expires_from_now(pt::seconds(discoverdelay));
	discoveryTimer.async_wait(boost::bind(&AgoResolver::discover, this, _1));
}

void AgoResolver::scanSchemaDir(const fs::path &schemaPrefix) {
	// generate vector of all schema files
	std::vector<fs::path> schemaArray;
	fs::path schemadir(schemaPrefix);
	if (fs::exists(schemadir)) {
		fs::recursive_directory_iterator it(schemadir);
		fs::recursive_directory_iterator endit;
		while (it != endit) {
			if (fs::is_regular_file(*it) && (it->path().extension().string() == ".yaml")) {
				schemaArray.push_back(it->path().filename());
			}
			++it;
		}
	}
	if (schemaArray.size() < 1) {
		throw new ConfigurationError("Can't find any schemas in " + schemaPrefix.string());
	}

	// load schema files in proper order
	std::sort(schemaArray.begin(), schemaArray.end());

	fs::path schemaFile = schemaPrefix / schemaArray.front();
	schema = parseSchema(schemaFile);
	AGO_DEBUG() << "parsing schema file:" << schemaFile;
	for (size_t i = 1; i < schemaArray.size(); i++) {
		schemaFile = schemaPrefix / schemaArray[i];
		AGO_DEBUG() << "parsing additional schema file:" << schemaFile;
		schema = mergeMap(schema, parseSchema(schemaFile));
	}
}

void AgoResolver::setupApp() {
	addCommandHandler();
	addEventHandler();
	agoConnection->setFilter(false);

	fs::path schemaPrefix;

	schemaPrefix = getConfigOption("system", "schemapath", getConfigPath(SCHEMADIR));
	discoverdelay = atoi(getConfigOption("system", "discoverdelay", "300").c_str());
	persistence = (atoi(getConfigOption("system","devicepersistence", "1").c_str()) == 1);

	systeminfo["uuid"] = getConfigOption("system", "uuid", "00000000-0000-0000-000000000000");
	systeminfo["version"] = AGOCONTROL_VERSION;

	scanSchemaDir(schemaPrefix);

	AGO_TRACE() << "reading inventory";
	try {
		inv = new Inventory(ensureParentDirExists(getConfigPath(INVENTORYDBFILE)));
	}catch(std::exception& e){
		throw ConfigurationError(std::string("Failed to load inventory: ") + e.what());
	}

	variables = jsonFileToVariantMap(getConfigPath(VARIABLESMAPFILE));
	if (persistence) {
		AGO_TRACE() << "reading devicemap";
		loadDevicemap();
	}

	agoConnection->addDevice("agocontroller","agocontroller");

	// Wait 2s before first discovery
	discoveryTimer.expires_from_now(pt::seconds(2));
	discoveryTimer.async_wait(boost::bind(&AgoResolver::discover, this, _1));
}

void AgoResolver::cleanupApp() {
	if(inv) {
		inv->close();
		delete inv;
		inv = NULL;
	}

	discoveryTimer.cancel();
}

AGOAPP_ENTRY_POINT(AgoResolver);
