#include <string>
#include <string.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sstream>
#include <assert.h>

#include <boost/preprocessor/stringize.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/bind.hpp>


#include <jsoncpp/json/reader.h>
#include "agoclient.h"

using namespace std;
using namespace qpid::messaging;
using namespace qpid::types;
namespace fs = ::boost::filesystem;

// helper to determine last element
#ifndef _LIBCPP_ITERATOR
template <typename Iter>
Iter next(Iter iter)
{
	return ++iter;
}
#endif

#define MODULE_CONFDIR "/conf.d"

namespace agocontrol {
	augeas *augeas = NULL;

	bool directories_inited = false;
	fs::path config_dir;
	fs::path localstate_dir;

	fs::path prepareDirectory(const char *name, const fs::path &dir) {
		if(!fs::exists(dir)) {
			// Try to create it
			// ensureDirExists will canonical() it
			return ensureDirExists(dir);
		}
		else {
			// Exists, canonicalize it ourselfs
			return fs::canonical(dir);
		}
	}

	fs::path initDirectory(const char *name, const char *def){
		const char *tmp;
		std::string env ("AGO_");
		env+= name;

		boost::to_upper(env);

		fs::path dir(def);
		if((tmp = getenv(env.c_str())) != NULL) {
			dir = tmp;
		}

		try {
			dir = prepareDirectory(name, dir);
		} catch(const fs::filesystem_error& error) {
			// Canonical failed; does it not exist after all?
			AGO_WARNING() << "Failed to resolve " << name << " " << dir.string()
				<< ": " << error.code().message()
				<< ". Falling back to default " << def;
			dir = fs::path(def);
		}

		return dir;
	}


	void initDirectorys() {
		// DEFAULT_CONFDIR, DEFAULT_LOCALSTATEDIR must be set with compiler flag
		if(config_dir.empty()) {
			config_dir = initDirectory("confdir", BOOST_PP_STRINGIZE(DEFAULT_CONFDIR));
		}
		if(localstate_dir.empty()) {
			localstate_dir = initDirectory("localstatedir", BOOST_PP_STRINGIZE(DEFAULT_LOCALSTATEDIR));
		}
		directories_inited = true;
	}

	/* These are only callable from AgoApp and AgoClient */
	void AgoClientInternal::setConfigDir(const boost::filesystem::path &dir) {
		if(!config_dir.empty()) {
			throw runtime_error("setConfigDir after initDirectorys was called!");
		}
		config_dir = prepareDirectory("confdir", dir);
	}
	void AgoClientInternal::setLocalStateDir(const boost::filesystem::path &dir) {
		if(!localstate_dir.empty()) {
			throw runtime_error("setLocalStateDir change after initDirectorys was called!");
		}
		localstate_dir= prepareDirectory("localstatedir", dir);
	}
}

fs::path agocontrol::ensureDirExists(const boost::filesystem::path &dir) {
	if(!fs::exists(dir))  {
		// This will throw if it fails
		fs::create_directories(dir);
	}

	// Normalize the directory; it should exist and should thus not fail
	return fs::canonical(dir);
}

fs::path agocontrol::ensureParentDirExists(const boost::filesystem::path &filename) {
	if(!fs::exists(filename))  {
		// Ensure parent directory exist
		fs::path dir = filename.parent_path();
		ensureDirExists(dir);
	}
	return filename;
}

fs::path agocontrol::getConfigPath(const fs::path &subpath) {
	if(!directories_inited) {
		initDirectorys();
	}
	fs::path res(config_dir);
	if(!subpath.empty()) {
		res /= subpath;
	}
	return res;
}

fs::path agocontrol::getLocalStatePath(const fs::path &subpath) {
	if(!directories_inited) {
		initDirectorys();
	}
	fs::path res(localstate_dir);
	if(!subpath.empty()) {
		res /= subpath;
	}
	return res;
}


bool agocontrol::augeas_init()  {
	fs::path path = getConfigPath(fs::path(MODULE_CONFDIR));
	path /= "*.conf";

	fs::path extra_loadpath = getConfigPath();

	augeas = aug_init(NULL, extra_loadpath.c_str(), AUG_SAVE_BACKUP | AUG_NO_MODL_AUTOLOAD);
	if (augeas == NULL) {
		AGO_ERROR() << "Can't initalize augeas";
		return false;
	}
	aug_set(augeas, "/augeas/load/Agocontrol/lens", "agocontrol.lns");
	aug_set(augeas, "/augeas/load/Agocontrol/incl", path.c_str());
	if (aug_load(augeas) == 0) return true;
	return false;
}

bool agocontrol::nameval(const std::string& in, std::string& name, std::string& value) {
	std::string::size_type i = in.find("=");
	if (i == std::string::npos) {
		name = in;
		return false;
	} else {
		name = in.substr(0, i);
		if (i+1 < in.size()) {
			value = in.substr(i+1);
			return true;
		} else {
			return false;
		}
	}
}

void agocontrol::replaceString(std::string& subject, const std::string& search, const std::string& replace) {
	size_t pos = 0;
	while ((pos = subject.find(search, pos)) != std::string::npos) {
		subject.replace(pos, search.length(), replace);
		pos += replace.length();
	}
}

std::string agocontrol::int2str(int i) {
	stringstream sstream;
	sstream << i;
	return sstream.str();
}

std::string agocontrol::float2str(float f) {
	stringstream sstream;
	sstream << f;
	return sstream.str();
}

bool agocontrol::variantMapToJSONFile(qpid::types::Variant::Map map, const fs::path &filename) {
	ofstream mapfile;
	try { 
		mapfile.open(filename.c_str());
		mapfile << variantMapToJSONString(map);
		mapfile.close();
		return true;
	} catch (...) {
		printf("ERROR: Can't write %s\n",filename.c_str());
		return false;
	}
}

qpid::types::Variant::Map agocontrol::jsonFileToVariantMap(const fs::path &filename) {
	string content;
	qpid::types::Variant::Map result;
	ifstream mapfile (filename.c_str());
	if (mapfile.is_open()) {
		while (mapfile.good()) {
			string line;
			getline(mapfile, line);
			content += line;
		}
		mapfile.close();
	}
	result =  jsonStringToVariantMap(content);
	return result;
}


std::string agocontrol::variantMapToJSONString(qpid::types::Variant::Map map) {
	string result;
	result += "{";
	for (Variant::Map::const_iterator it = map.begin(); it != map.end(); ++it) {
		result += "\""+ it->first + "\":";
		std::string tmpstring;
		switch (it->second.getType()) {
			case VAR_MAP:
				result += variantMapToJSONString(it->second.asMap());
				break;
			case VAR_LIST:
				result += variantListToJSONString(it->second.asList());
				break;
			case VAR_STRING:
				tmpstring = it->second.asString();
				replaceString(tmpstring, "\"", "\\\"");	
				result += "\"" +  tmpstring + "\"";
				break;
			default:
				if (it->second.asString().size() != 0) {
					result += it->second.asString();	
				} else {
					result += "null";
				}
		}
		if ((it != map.end()) && (next(it) != map.end())) result += ",";
	}
	result += "}";
	
	return result;
}

std::string agocontrol::variantListToJSONString(qpid::types::Variant::List list) {
	string result;
	result += "[";
	for (Variant::List::const_iterator it = list.begin(); it != list.end(); ++it) {
		std::string tmpstring;
		switch(it->getType()) {
			case VAR_MAP:
				result += variantMapToJSONString(it->asMap());
				break;
			case VAR_LIST:
				result += variantListToJSONString(it->asList());
				break;
			case VAR_STRING:
				tmpstring = it->asString();
				replaceString(tmpstring, "\"", "\\\"");	
				result += "\"" +  tmpstring + "\"";
				break;
			default:
				if (it->asString().size() != 0) {
					result += it->asString();
				} else {
					result += "null";
				}
		}
		if ((it != list.end()) && (next(it) != list.end())) result += ",";
	}
	result += "]";
	return result;
}

qpid::types::Variant::List agocontrol::jsonToVariantList(Json::Value value) {
	Variant::List list;
	try {
		for (Json::ValueIterator it = value.begin(); it != value.end(); it++) {
			switch((*it).type()) {
				case Json::nullValue:
					break;
				case Json::intValue:
					list.push_back( (*it).asInt());
					break;
				case Json::uintValue:
					list.push_back( (*it).asUInt());
					break;
				case Json::realValue:
					list.push_back( (*it).asDouble());
					break;
				case Json::stringValue:
					list.push_back( (*it).asString());
					break;
				case Json::booleanValue:
					list.push_back( (*it).asBool());
					break;
				case Json::arrayValue:
					list.push_back(jsonToVariantList((*it)));
					break;
				case Json::objectValue:
					list.push_back(jsonToVariantMap((*it)));
					break;
				default:
					AGO_WARNING() << "Unhandled Json::ValueType in jsonToVariantList()";


			}
		}
	} catch (const std::exception& error) {
		AGO_ERROR() << "Exception during JSON->Variant::List conversion: " << error.what();
	}


	return list;
}
qpid::types::Variant::Map agocontrol::jsonToVariantMap(Json::Value value) {
	Variant::Map map;
	try {
		for (Json::ValueIterator it = value.begin(); it != value.end(); it++) {
			switch((*it).type()) {
				case Json::nullValue:
					break;
				case Json::intValue:
					map[it.key().asString()] = (*it).asInt();
					break;
				case Json::uintValue:
					map[it.key().asString()] = (*it).asUInt();
					break;
				case Json::realValue:
					map[it.key().asString()] = (*it).asDouble();
					break;
				case Json::stringValue:
					map[it.key().asString()] = (*it).asString();
					break;
				case Json::booleanValue:
					map[it.key().asString()] = (*it).asBool();
					break;
				case Json::arrayValue:
					map[it.key().asString()] = jsonToVariantList((*it));
					break;
				case Json::objectValue:
					map[it.key().asString()] = jsonToVariantMap((*it));
					break;
				default:
					AGO_WARNING() << "Unhandled Json::ValueType in jsonToVariantMap()";
			}
		}	
	} catch (const std::exception& error) {
		AGO_ERROR() << "Exception during JSON->Variant::Map conversion: " << error.what();
	}
	return map;
}

qpid::types::Variant::Map agocontrol::jsonStringToVariantMap(std::string jsonstring) {
	Json::Value root;
	Json::Reader reader;
	Variant::Map result;

	try {
		if ( reader.parse(jsonstring, root)) {
			result = jsonToVariantMap(root);
		}/* else { 
			printf("warning, could not parse json to Variant::Map: %s\n",jsonstring.c_str());
		}*/
	} catch (const std::exception& error) {
		AGO_ERROR() << "Exception during JSON String->Variant::Map conversion: " << error.what();
	}
	return result;
}

// generates a uuid as string via libuuid
std::string agocontrol::generateUuid() {
	string strUuid;
	char *name;
	if ((name=(char*)malloc(38)) != NULL) {
		uuid_t tmpuuid;
		name[0]=0;
		uuid_generate(tmpuuid);
		uuid_unparse(tmpuuid,name);
		strUuid = string(name);
		free(name);
	}
	return strUuid;
}

std::string agocontrol::getConfigOption(const char *section, const char *option, std::string &defaultvalue) {
	return getConfigOption(section, option, defaultvalue.c_str());
}

fs::path agocontrol::getConfigOption(const char *section, const char *option, const fs::path &defaultvalue) {
	std::string value = getConfigOption(section, option, defaultvalue.c_str());
	return fs::path(value);
}

std::string agocontrol::getConfigOption(const char *section, const char *option,
		const char *defaultvalue) {
	return getConfigOption(section, NULL, option, defaultvalue);
}

std::string agocontrol::augeasPathFromSectionOption(const char *section, const char *option) {
	assert(section != NULL);
	assert(option != NULL);

	std::stringstream valuepath;
	valuepath << "/files";
	valuepath << getConfigPath(MODULE_CONFDIR).string();
	valuepath << "/";
	valuepath << section;
	valuepath << ".conf";
	valuepath << "/";
	valuepath << section;
	valuepath << "/";
	valuepath << option;
	return valuepath.str();
}

qpid::types::Variant::Map agocontrol::getConfigTree() {
	qpid::types::Variant::Map tree;
	if (augeas==NULL) augeas_init();
	if (augeas == NULL) {
		AGO_ERROR() << "cannot initialize augeas";
		return tree;
	}
	char **matches;
	std::stringstream path;
	path << "/files";
	path << getConfigPath(MODULE_CONFDIR).string();
	path << "/";
	std::string prefix = path.str();
	path << "/*";
	int num = aug_match(augeas, path.str().c_str(), &matches);
	for (int i=0; i < num; i++) {
		const char *val;
		aug_get(augeas, matches[i], &val);
		if (val != NULL) {
			// TODO: split augeas path into section and option
			std::string match = matches[i];
			replaceString(match, prefix, "");
			size_t pos = match.find(".conf");
			std::string section = match.substr(0,pos);
			replaceString(match, section, "");
			replaceString(match, ".conf/", "");
			std::string option = match.substr(1); // skip trailing slash
			if (!(tree[section].isVoid())) {
				qpid::types::Variant::Map sectionMap = tree[section].asMap();
				sectionMap[option] = val;
				tree[section] = sectionMap;
			} else {
				qpid::types::Variant::Map sectionMap;
				sectionMap[option] = val;
				tree[section] = sectionMap;
			}
		}
		free((void *) matches[i]);
	}
	free(matches);
	return tree;

}

std::string agocontrol::getConfigOption(const char *section, const char *fallback_section,
		const char *option, const char *defaultvalue) {
	if (augeas==NULL) augeas_init();
	if (augeas == NULL) {
		AGO_ERROR() << "cannot initialize augeas";
		return defaultvalue;
	}

	const char *value;
	int ret =  aug_get(augeas, augeasPathFromSectionOption(section, option).c_str(), &value);
	if (ret != 1) {
		if(fallback_section) {
			ret =  aug_get(augeas, augeasPathFromSectionOption(fallback_section, option).c_str(), &value);
		}
	}

	if(ret != 1) {
		// cout << "AUGEAS: no " <<  valuepath.str() << " - using default value: " << defaultvalue << endl;
		if(defaultvalue) {
			return std::string(defaultvalue);
		}
	} else {
		if(value != NULL) {
			std::stringstream result;
			// cout << "AUGEAS: using config value: " << value << endl;
			result << value;
			return result.str();
		}
	}

	// empty
	return std::string();
}

bool agocontrol::setConfigOption(const char* section, const char* option, const char* value) {
	bool result = true;
	if (augeas==NULL) augeas_init();
	if (augeas == NULL) {
		AGO_ERROR() << "cannot initialize augeas";
		return false;
	}

	if (aug_set(augeas, augeasPathFromSectionOption(section, option).c_str(), value) == -1) {
		AGO_WARNING() << "Could not set value!";
		return false;
	}

	if (aug_save(augeas)==-1) {
		AGO_ERROR() << "Could not write config file!";
		return false;
	}


	return result;
}

bool agocontrol::setConfigOption(const char* section, const char* option, const float value) {
	std::stringstream stringvalue;
	stringvalue << value;
	return setConfigOption(section, option, stringvalue.str().c_str());
}

bool agocontrol::setConfigOption(const char* section, const char* option, const int value) {
	std::stringstream stringvalue;
	stringvalue << value;
	return setConfigOption(section, option, stringvalue.str().c_str());
}

bool agocontrol::setConfigOption(const char* section, const char* option, const bool value) {
	std::stringstream stringvalue;
	stringvalue << value;
	return setConfigOption(section, option, stringvalue.str().c_str());
}

agocontrol::AgoConnection::AgoConnection(const char *interfacename)
	: shutdownSignaled(false)
{
	// TODO: Move to AgoApp
	::agocontrol::log::log_container::initDefault();

	Variant::Map connectionOptions;
	std::string broker = getConfigOption("system", "broker", "localhost:5672");
	connectionOptions["username"] = getConfigOption("system", "username", "agocontrol");
	connectionOptions["password"] = getConfigOption("system", "password", "letmein");
	connectionOptions["reconnect"] = "true";

	filterCommands = true; // only pass commands for child devices to handler by default
	instance = interfacename;

	uuidMapFile = getConfigPath("uuidmap");
	uuidMapFile /= (std::string(interfacename) + ".json");
	AGO_DEBUG() << "Using uuidMapFile " << uuidMapFile;
	try {
		ensureParentDirExists(uuidMapFile);
	} catch(const std::exception& error) {
		// exception msg has already been logged
		AGO_FATAL()
			<< "Couldn't use configuration dir path: "
			<< error.what();
		_exit(1);
	}

	loadUuidMap();

	connection = Connection(broker, connectionOptions);
	try {
		AGO_DEBUG() << "Opening broker connection: " << broker;
		connection.open(); 
		session = connection.createSession(); 
		sender = session.createSender("agocontrol; {create: always, node: {type: topic}}"); 
	} catch(const std::exception& error) {
		AGO_FATAL() << "Failed to connect to broker: " << error.what();
		connection.close();
		_exit(1);
	}
}

agocontrol::AgoConnection::~AgoConnection() {
	try {
		if(connection.isOpen()) {
			AGO_DEBUG() << "Closing broker connection";
			connection.close();
		}
	} catch(const std::exception& error) {
		AGO_ERROR() << "Failed to close broker connection: " << error.what();
	}
}


void agocontrol::AgoConnection::run() {
	try {
		receiver = session.createReceiver("agocontrol; {create: always, node: {type: topic}}");
	} catch(const std::exception& error) {
		AGO_FATAL() << "Failed to create broker receiver: " << error.what();
		_exit(1);
	}

	while( !shutdownSignaled ) {
		try{
			Variant::Map content;
			Message message = receiver.fetch(Duration::SECOND * 3);
			session.acknowledge();

			// workaround for bug qpid-3445
			if (message.getContent().size() < 4) {
				throw qpid::messaging::EncodingException("message too small");
			}

			decode(message, content);
			AGO_TRACE() << "Processing message [src=" << message.getReplyTo() <<
				", sub="<< message.getSubject()<<"]: " << content;

			if (content["command"] == "discover") {
				reportDevices(); // make resolver happy and announce devices on discover request
			} else {
				if (message.getSubject().size() == 0) {
					// no subject, this is a command
					string internalid = uuidToInternalId(content["uuid"].asString());
					// lets see if this is for one of our devices
					bool isOurDevice = (internalid.size() > 0) && (deviceMap.find(internalIdToUuid(internalid)) != deviceMap.end());
					//  only handle if a command handler is set. In addition it needs to be one of our device when the filter is enabled
					if ( ( isOurDevice || (!(filterCommands))) && !commandHandler.empty()) {

						// printf("command for id %s found, calling handler\n", internalid.c_str());
						if (internalid.size() > 0) content["internalid"] = internalid;
						qpid::types::Variant::Map responsemap = commandHandler(content);
						// found a match, reply to sender and pass the command to the assigned handler method
						const Address& replyaddress = message.getReplyTo();
						// only send a reply if this was for one of our childs
						// or if it was the special command inventory when the filterCommands was false, that's used by the resolver
						// to reply to "anonymous" requests not destined to any specific uuid
						if ((replyaddress && isOurDevice) || (content["command"]=="inventory" && filterCommands==false)) {
							AGO_TRACE() << "Sending reply " << responsemap;
							Session replysession = connection.createSession();
							try {
								Sender replysender = replysession.createSender(replyaddress);
								Message response;
								encode(responsemap, response);
								response.setSubject(instance);
								replysender.send(response);
								replysession.close();
							} catch(const std::exception& error) {
								AGO_ERROR() << "Failed to send reply: " << error.what();;
								replysession.close();
							}
						} 
					}
				} else if (!eventHandler.empty()) {
					eventHandler(message.getSubject(), content);
				}
			}
		} catch(const NoMessageAvailable& error) {
			
		} catch(const std::exception& error) {
			if(shutdownSignaled)
				break;

			AGO_ERROR() << "Exception in message loop: " << error.what();
			if (session.hasError()) {
				AGO_ERROR() << "Session has error, recreating";
				session.close();
				session = connection.createSession(); 
				receiver = session.createReceiver("agocontrol; {create: always, node: {type: topic}}"); 
				sender = session.createSender("agocontrol; {create: always, node: {type: topic}}"); 
			}

			usleep(50);
		}
	}
}

void agocontrol::AgoConnection::shutdown() {
	shutdownSignaled = true;
	if(connection.isOpen()) {
		AGO_DEBUG() << "Closing broker connection";
		connection.close();
	}
}

bool agocontrol::AgoConnection::emitDeviceAnnounce(const char *internalId, const char *deviceType) {
	Variant::Map content;
	Message event;

	content["devicetype"] = deviceType;
	content["internalid"] = internalId;
	content["handled-by"] = instance;
	content["uuid"] = internalIdToUuid(internalId);
	encode(content, event);
	event.setSubject("event.device.announce");
	try {
		sender.send(event);
	} catch(const std::exception& error) {
		AGO_ERROR() << "Exception in emitDeviceAnnounce: " << error.what();
		return false;
	} 
	return true;
} 

/**
 * Emit stale state
 */
bool agocontrol::AgoConnection::emitDeviceStale(const char* internalId, const int stale)
{
	Variant::Map content;
	Message event;

	content["internalid"] = internalId;
	content["stale"] = stale;
	content["uuid"] = internalIdToUuid(internalId);
	encode(content, event);
	event.setSubject("event.device.stale");
	try
	{
		sender.send(event);
	}
	catch(const std::exception& error)
	{
		AGO_ERROR() << "Exception in emitDeviceStale: " << error.what();
		return false;
	} 
	return true;
}

bool agocontrol::AgoConnection::emitDeviceRemove(const char *internalId) {
	Variant::Map content;
	Message event;

	content["uuid"] = internalIdToUuid(internalId);
	encode(content, event);
	event.setSubject("event.device.remove");
	try {
		sender.send(event);
	} catch(const std::exception& error) {
		AGO_ERROR() << "Exception in emitDeviceRemove: " << error.what();
		return false;
	} 
	return true;
} 

bool agocontrol::AgoConnection::addDevice(const char *internalId, const char *deviceType, bool passuuid) {
	if (!passuuid) return addDevice(internalId, deviceType);
	uuidMap[internalId] = internalId;
	storeUuidMap();
	return addDevice(internalId, deviceType);

}

bool agocontrol::AgoConnection::addDevice(const char *internalId, const char *deviceType) {
	if (internalIdToUuid(internalId).size()==0) {
		// need to generate new uuid
		uuidMap[generateUuid()] = internalId;
		storeUuidMap();
	}
	Variant::Map device;
	device["devicetype"] = deviceType;
	device["internalid"] = internalId;
	device["stale"] = 0;
	deviceMap[internalIdToUuid(internalId)] = device;
	emitDeviceAnnounce(internalId, deviceType);
	return true;
}

bool agocontrol::AgoConnection::removeDevice(const char *internalId) {
	if (internalIdToUuid(internalId).size()!=0) {
		emitDeviceRemove(internalId);
		Variant::Map::const_iterator it = deviceMap.find(internalIdToUuid(internalId));
		if (it != deviceMap.end()) deviceMap.erase(it->first);
		// deviceMap[internalIdToUuid(internalId)] = device;
		return true;
	} else return false;
}

/**
 * Suspend device (set stale flag)
 * Stale device are not announced during a discover command
 */
bool agocontrol::AgoConnection::suspendDevice(const char* internalId)
{
	string uuid = internalIdToUuid(internalId);
	if( uuid.size()>0 && !deviceMap[uuid].isVoid() )
	{
		deviceMap[internalIdToUuid(internalId)].asMap()["stale"] = 1;
		emitDeviceStale(internalId, 1);
		return true;
	}
	return false;
}

/**
 * Resume device (reset stale flag)
 * Stale device are not announced during a discover command
 */
bool agocontrol::AgoConnection::resumeDevice(const char* internalId)
{
	string uuid = internalIdToUuid(internalId);
	if( uuid.size()>0 && !deviceMap[uuid].isVoid() )
	{
		deviceMap[internalIdToUuid(internalId)].asMap()["stale"] = 0;
		emitDeviceStale(internalId, 0);
		return true;
	}
	return false;
}
std::string agocontrol::AgoConnection::uuidToInternalId(std::string uuid) {
	return uuidMap[uuid].asString();
} 

std::string agocontrol::AgoConnection::internalIdToUuid(std::string internalId) {
	string result;
	for (Variant::Map::const_iterator it = uuidMap.begin(); it != uuidMap.end(); ++it) {
		if (it->second.asString() == internalId) return it->first;
	}
	return result;
}

void agocontrol::AgoConnection::reportDevices() {
	for (Variant::Map::const_iterator it = deviceMap.begin(); it != deviceMap.end(); ++it) {
		Variant::Map device;
		Variant::Map content;
		Message event;

		// printf("uuid: %s\n", it->first.c_str());
		device = it->second.asMap();
		// printf("devicetype: %s\n", device["devicetype"].asString().c_str());
		// do not announce stale devices
		if( device["stale"].asInt8()==0 )
		{
			emitDeviceAnnounce(device["internalid"].asString().c_str(), device["devicetype"].asString().c_str());
		}
	}
}

bool agocontrol::AgoConnection::storeUuidMap() {
	ofstream mapfile;
	mapfile.open(uuidMapFile.c_str());
	mapfile << variantMapToJSONString(uuidMap);
	mapfile.close();
	return true;
}

bool agocontrol::AgoConnection::loadUuidMap() {
	string content;
	ifstream mapfile (uuidMapFile.c_str());
	if (mapfile.is_open()) {
		while (mapfile.good()) {
			string line;
			getline(mapfile, line);
			content += line;
		}
		mapfile.close();
	}
	uuidMap = jsonStringToVariantMap(content);
	return true;
}

bool agocontrol::AgoConnection::addHandler(qpid::types::Variant::Map (*handler)(qpid::types::Variant::Map)) {
	addHandler(boost::bind(handler, _1));
	return true;
}

bool agocontrol::AgoConnection::addHandler(boost::function<qpid::types::Variant::Map (qpid::types::Variant::Map)> handler)
{
	commandHandler = handler;
	return true;
}

bool agocontrol::AgoConnection::addEventHandler(void (*handler)(std::string, qpid::types::Variant::Map)) {
	addEventHandler(boost::bind(handler, _1, _2));
	return true;
}

bool agocontrol::AgoConnection::addEventHandler(boost::function<void (std::string, qpid::types::Variant::Map)> handler)
{
	eventHandler = handler;
	return true;
}


bool agocontrol::AgoConnection::sendMessage(const char *subject, qpid::types::Variant::Map content) {
	Message message;

	try {
		encode(content, message);
		message.setSubject(subject);

		AGO_TRACE() << "Sending message [src=" << message.getReplyTo() <<
			", sub="<< message.getSubject()<<"]: " << content;
		sender.send(message);
	} catch(const std::exception& error) {
		AGO_ERROR() << "Exception in sendMessage: " << error.what();
		return false;
	}
	
	return true;
}

qpid::types::Variant::Map agocontrol::AgoConnection::sendMessageReply(const char *subject, qpid::types::Variant::Map content) {
	Message message;
	qpid::types::Variant::Map responseMap;
	Receiver responseReceiver;
	try {
		encode(content, message);
		message.setSubject(subject);
		Address responseQueue("#response-queue; {create:always, delete:always}");
		responseReceiver = session.createReceiver(responseQueue);
		message.setReplyTo(responseQueue);

		AGO_TRACE() << "Sending message [sub=" << subject << ", reply=" << responseQueue <<"]" << content;
		sender.send(message);
		Message response = responseReceiver.fetch(Duration::SECOND * 3);
		session.acknowledge();
		if (response.getContentSize() > 3) {
			decode(response,responseMap);
		} else {
			responseMap["response"] = response.getContent();
		}
		AGO_TRACE() << "Reply received: " << responseMap;
	} catch (qpid::messaging::NoMessageAvailable) {
		AGO_WARNING() << "No reply for message sent to subject " << subject;
		printf("WARNING, no reply message to fetch\n");
	} catch(const std::exception& error) {
		AGO_ERROR() << "Exception in sendMessageReply: " << error.what();
	}
	try {
		responseReceiver.close();
	} catch(const std::exception& error) {
		AGO_ERROR() << "Cleanup error in in sendMessageReply: " << error.what();
	}
	return responseMap;
}


bool agocontrol::AgoConnection::sendMessage(qpid::types::Variant::Map content) {
	return sendMessage("",content);
}

bool agocontrol::AgoConnection::emitEvent(const char *internalId, const char *eventType, const char *level, const char *unit) {
	Variant::Map content;
	string _level = level;
	Variant value;
	value.parse(level);
	content["level"] = value;
	content["unit"] = unit;
	content["uuid"] = internalIdToUuid(internalId);
	return sendMessage(eventType, content);
}
bool agocontrol::AgoConnection::emitEvent(const char *internalId, const char *eventType, float level, const char *unit) {
	Variant::Map content;
	content["level"] = level;
	content["unit"] = unit;
	content["uuid"] = internalIdToUuid(internalId);
	return sendMessage(eventType, content);
}
bool agocontrol::AgoConnection::emitEvent(const char *internalId, const char *eventType, int level, const char *unit) {
	Variant::Map content;
	content["level"] = level;
	content["unit"] = unit;
	content["uuid"] = internalIdToUuid(internalId);
	return sendMessage(eventType, content);
}

bool agocontrol::AgoConnection::emitEvent(const char *internalId, const char *eventType, qpid::types::Variant::Map _content) {
	Variant::Map content;
	content = _content;
	content["uuid"] = internalIdToUuid(internalId);
	return sendMessage(eventType, content);
}

string agocontrol::AgoConnection::getDeviceType(const char *internalId) {
	string uuid = internalIdToUuid(internalId);
	if (uuid.size() > 0) {
		Variant::Map device = deviceMap[internalIdToUuid(internalId)].asMap();
		return device["devicetype"];
	} else return "";

}

/**
 * Return device stale state
 */
int agocontrol::AgoConnection::isDeviceStale(const char* internalId)
{
	string uuid = internalIdToUuid(internalId);
	if (uuid.size() > 0)
	{
		Variant::Map device = deviceMap[internalIdToUuid(internalId)].asMap();
		return device["stale"].asInt8();
	}
	else
	{
		return 0;
	}
}

bool agocontrol::AgoConnection::setFilter(bool filter) {
	filterCommands = filter;
	return filterCommands;
}

qpid::types::Variant::Map agocontrol::AgoConnection::getInventory() {
	Variant::Map content;
	Variant::Map responseMap;
	content["command"] = "inventory";
	Message message;
	encode(content, message);
	Address responseQueue("#response-queue; {create:always, delete:always}");
	Receiver responseReceiver = session.createReceiver(responseQueue);
	message.setReplyTo(responseQueue);
	sender.send(message);
	try {
		Message response = responseReceiver.fetch(Duration::SECOND * 3);
		session.acknowledge();
		if (response.getContentSize() > 3) {	
			decode(response,responseMap);
		}
	} catch (qpid::messaging::NoMessageAvailable) {
		AGO_WARNING() << "No getInventory response";
	}
	try {
		responseReceiver.close();
	} catch(const std::exception& error) {
		AGO_ERROR() << "Exception in getInventory cleanup: " << error.what();
	}
	return responseMap;
}

std::string agocontrol::AgoConnection::getAgocontroller() {
	std::string agocontroller;
	int retry = 10;
	while(agocontroller=="" && retry-- > 0) {
		qpid::types::Variant::Map inventory = getInventory();
		if (!(inventory["devices"].isVoid())) {
			qpid::types::Variant::Map devices = inventory["devices"].asMap();
			qpid::types::Variant::Map::const_iterator it;
			for (it = devices.begin(); it != devices.end(); it++) {
				if (!(it->second.isVoid())) {
					qpid::types::Variant::Map device = it->second.asMap();
					if (device["devicetype"] == "agocontroller") {
						AGO_DEBUG() << "Found Agocontroller: " << it->first;
						agocontroller = it->first;
					}
				}
			}
		} else {
			AGO_WARNING() << "Unable to resolve agocontroller, retrying";
			sleep(1);
		}
	}
	if (agocontroller == "")
		AGO_WARNING() << "Failed to resolve agocontroller, giving up";

	return agocontroller;
}

bool agocontrol::AgoConnection::setGlobalVariable(std::string variable, qpid::types::Variant value) {
	Variant::Map setvariable;
	std::string agocontroller = getAgocontroller();
	if (agocontroller != "") {
		setvariable["uuid"] = agocontroller;
		setvariable["command"] = "setvariable";
		setvariable["variable"] = variable;
		setvariable["value"] = value;
		return sendMessage("", setvariable);
	} 
	return false;
}

