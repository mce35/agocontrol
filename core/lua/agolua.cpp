#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <pthread.h>

#include <string>
#include <iostream>
#include <sstream>
#include <cerrno>

#define BOOST_FILESYSTEM_VERSION 3
#define BOOST_FILESYSTEM_NO_DEPRECATED 
#include "boost/filesystem.hpp"
#include "boost/regex.hpp"
//#include "boost/filesystem/operations.hpp"

#ifndef SCRIPTSINFOSFILE
#define SCRIPTSINFOSFILE "maps/agolua.json"
#endif

#ifndef LUA_SCRIPT_DIR
#define LUA_SCRIPT_DIR "lua"
#endif

#include "base64.h"

#include "agoclient.h"

namespace fs = ::boost::filesystem;

using namespace std;
using namespace agocontrol;

#ifdef __FreeBSD__
#include "lua52/lua.hpp"
#else
#include "lua5.2/lua.hpp"
#endif

qpid::types::Variant::Map inventory;
bool refreshInventory = true; //optimize script execution (0.2s to get all inventory)
qpid::types::Variant::Map scriptsInfos;
const char reAll[] = "--.*--\\[\\[(.*)\\]\\](.*)"; 
const char reEvent[] = "(event\\.\\w+\\.\\w+)"; 
boost::regex exprAll(reAll);
boost::regex exprEvent(reEvent);
AgoConnection *agoConnection;
std::string agocontroller;
int filterByEvents = 1;

fs::path scriptdir;

static const luaL_Reg loadedlibs[] = {
	{"_G", luaopen_base},
	{LUA_TABLIBNAME, luaopen_table},
	{LUA_STRLIBNAME, luaopen_string},
	{LUA_MATHLIBNAME, luaopen_math},
	{NULL, NULL}
};

static fs::path construct_script_name(fs::path input) {
	fs::path out = (scriptdir / input);
	// replace == add/append
	out.replace_extension(".lua");
	return out;
}

// read file into string. credits go to "insane coder" - http://stackoverflow.com/questions/2602013/read-whole-ascii-file-into-c-stdstring
static std::string get_file_contents(const fs::path &filename) {
	std::ifstream in(filename.c_str(), std::ios::in | std::ios::binary);
	if (in)
	{
		std::string contents;
		in.seekg(0, std::ios::end);
		contents.resize(in.tellg());
		in.seekg(0, std::ios::beg);
		in.read(&contents[0], contents.size());
		in.close();
		return(contents);
	}
	throw(errno);
}

void pushTableFromMap(lua_State *L, qpid::types::Variant::Map content) {
	lua_createtable(L, 0, 0);
	for (qpid::types::Variant::Map::const_iterator it=content.begin(); it!=content.end(); it++) {
		switch (it->second.getType()) {
			case qpid::types::VAR_INT8:
				lua_pushstring(L,it->first.c_str());
				lua_pushnumber(L,it->second.asInt8());
				lua_settable(L, -3);
				break;
			case qpid::types::VAR_INT16:
				lua_pushstring(L,it->first.c_str());
				lua_pushnumber(L,it->second.asInt16());
				lua_settable(L, -3);
				break;
			case qpid::types::VAR_INT32:
				lua_pushstring(L,it->first.c_str());
				lua_pushnumber(L,it->second.asInt32());
				lua_settable(L, -3);
				break;
			case qpid::types::VAR_INT64:
				lua_pushstring(L,it->first.c_str());
				lua_pushnumber(L,it->second.asInt64());
				lua_settable(L, -3);
				break;
			case qpid::types::VAR_UINT8:
				lua_pushstring(L,it->first.c_str());
				lua_pushnumber(L,it->second.asUint8());
				lua_settable(L, -3);
				break;
			case qpid::types::VAR_UINT16:
				lua_pushstring(L,it->first.c_str());
				lua_pushnumber(L,it->second.asUint16());
				lua_settable(L, -3);
				break;
			case qpid::types::VAR_UINT32:
				lua_pushstring(L,it->first.c_str());
				lua_pushnumber(L,it->second.asUint32());
				lua_settable(L, -3);
				break;
			case qpid::types::VAR_UINT64:
				lua_pushstring(L,it->first.c_str());
				lua_pushnumber(L,it->second.asUint64());
				lua_settable(L, -3);
				break;
			case qpid::types::VAR_FLOAT:
				lua_pushstring(L,it->first.c_str());
				lua_pushnumber(L,it->second.asFloat());
				lua_settable(L, -3);
				break;
			case qpid::types::VAR_DOUBLE:
				lua_pushstring(L,it->first.c_str());
				lua_pushnumber(L,it->second.asDouble());
				lua_settable(L, -3);
				break;
			case qpid::types::VAR_STRING:
				lua_pushstring(L,it->first.c_str());
				lua_pushstring(L,it->second.asString().c_str());
				lua_settable(L, -3);
				break;
			case qpid::types::VAR_UUID:
				lua_pushstring(L,it->first.c_str());
				lua_pushstring(L,it->second.asString().c_str());
				lua_settable(L, -3);
				break;
			case qpid::types::VAR_MAP:
				lua_pushstring(L,it->first.c_str());
				pushTableFromMap(L,it->second.asMap());
				lua_settable(L, -3);
				break;
			case qpid::types::VAR_LIST:
				// TODO: push list
				break;
			case qpid::types::VAR_BOOL:
				lua_pushstring(L,it->first.c_str());
				lua_pushboolean(L,it->second.asBool());
				lua_settable(L, -3);
				break;
			case qpid::types::VAR_VOID:
				lua_pushstring(L,it->first.c_str());
				lua_pushnil(L);
				lua_settable(L, -3);
				break;
			//default:
				//lua_pushstring(L,it->first.c_str());
				//lua_pushstring(L,"unhandled");
				//AGO_TRACE() << "undhandled type: " << it->second.getType();
				//lua_settable(L, -3);
		}
	}	
}

int luaAddDevice(lua_State *l) { // DRAFT
	int argc = lua_gettop(l);
	if (argc != 2) return 0;

	std::string internalid = lua_tostring(l, lua_gettop(l));
	lua_pop(l, 1);
	std::string devicetype = lua_tostring(l, lua_gettop(l));
	lua_pop(l, 1);

	agoConnection->addDevice(internalid.c_str(), devicetype.c_str());
	return 0;
}

int luaSendMessage(lua_State *l) {
	qpid::types::Variant::Map content;
	std::string subject;
	// number of input arguments
	int argc = lua_gettop(l);

	// print input arguments
	for(int i=0; i<argc; i++) {
		string name, value;
		if (nameval(string(lua_tostring(l, lua_gettop(l))),name, value)) {
			if (name == "subject") {
				subject = value;
			}
			content[name]=value;
		}
		lua_pop(l, 1);
	}
	AGO_TRACE() << "Sending message: " << subject << " " << content;
	qpid::types::Variant::Map replyMap = agoConnection->sendMessageReply(subject.c_str(), content);	 
	lua_pushnumber(l, 0);
	return 1;
}

int luaSetVariable(lua_State *L) {
	qpid::types::Variant::Map content;
	std::string subject;

	//get input arguments
	content["variable"] = std::string(lua_tostring(L,1));
	content["value"] = std::string(lua_tostring(L,2));
	content["command"]="setvariable";
	content["uuid"]=agocontroller;
	AGO_TRACE() << "Sending message: " << content;
	qpid::types::Variant::Map replyMap = agoConnection->sendMessageReply(subject.c_str(), content);
	//manage result
	if( !replyMap["returncode"].isVoid() )
	{
		lua_pushnumber(L, replyMap["returncode"].asInt32());
	}
	else
	{
		//sendcommand problem
		lua_pushnumber(L,0);
	}

	//refresh inventory only once (performance optimization)
	if( refreshInventory )
	{
		inventory = agoConnection->getInventory();
		refreshInventory = false;
	}

	//update current inventory to reflect changes without reloading it (too long!!)
	qpid::types::Variant::Map variables = inventory["variables"].asMap();
	if( !variables[content["variable"]].isVoid() )
	{
		variables[content["variable"]] = content["value"];
		inventory["variables"] = variables;
	}

	return 1;
}

/**
 * Return variable value
 */
int luaGetVariable(lua_State *L)
{
	//init
	std::string variableName = "";

	//refresh inventory only once (performance optimization)
	if( refreshInventory )
	{
		inventory = agoConnection->getInventory();
		refreshInventory = false;
	}
	qpid::types::Variant::Map deviceInventory = inventory["devices"].asMap();

	//get variable name
	variableName = std::string(lua_tostring(L,1));

	if( variableName.length()>0 )
	{
		qpid::types::Variant::Map variables = inventory["variables"].asMap();
		if( !variables[variableName].isVoid() )
		{
			lua_pushstring(L, variables[variableName].asString().c_str());
		}
		else
		{
			//unknown variable
			lua_pushnil(L);
		}
	}
	else
	{
		//bad parameter
		lua_pushnil(L);
	}

	return 1;
}

/**
 * Return value from inventory["device"]
 * Callback format: getDeviceInventory(uuid, param)
 */
int luaGetDeviceInventory(lua_State *L)
{
	//init
	std::string uuid = "";
	std::string attribute = "";
	std::string subAttribute = "";

	//refresh inventory only once (performance optimization)
	if( refreshInventory )
	{
		inventory = agoConnection->getInventory();
		refreshInventory = false;
	}
	qpid::types::Variant::Map deviceInventory = inventory["devices"].asMap();

	// number of input arguments
	int argc = lua_gettop(L);

	// print input arguments
	for(int i=1; i<=argc; ++i)
	{
		switch(i)
		{
			case 1:
				uuid = std::string(lua_tostring(L,i));
				break;
			case 2:
				attribute = std::string(lua_tostring(L,i));
				break;
			case 3:
				subAttribute = std::string(lua_tostring(L,i));
				break;
			default:
				//unmanaged parameter
				break;
		}
	}
	AGO_TRACE() << "Get device inventory: inventory['devices'][" << uuid << "][" << attribute << "]";
	if( subAttribute.length()>0 )
	{
		AGO_TRACE() << "[" << subAttribute << "]";
	}
	if( !deviceInventory[uuid].isVoid() )
	{
		qpid::types::Variant::Map attributes = deviceInventory[uuid].asMap();
		if( !attributes[attribute].isVoid() )
		{
			//return main device attribute
			lua_pushstring(L, attributes[attribute].asString().c_str());
		}
		else
		{
			//search attribute in device values
			qpid::types::Variant::Map values = attributes["values"].asMap();
			for( qpid::types::Variant::Map::iterator it=values.begin(); it!=values.end(); it++ )
			{
				//TODO return device value property (quantity, unit, latitude, longitude...)
				qpid::types::Variant::Map value = it->second.asMap();
				lua_pushstring(L, value[subAttribute].asString().c_str());
			}
			//lua_pushnil(L);
		}
	}
	else
	{
		//device not found
		lua_pushnil(L);
	}
	return 1;
}

/**
 * Force getting inventory manually
 */
int luaGetInventory(lua_State *L) {
	inventory = agoConnection->getInventory();
	refreshInventory = false;
	pushTableFromMap(L, inventory);
	//lua_setglobal(L, "inventory");
	return 1;
}

/**
 * Search triggered events in specified script
 * @param scriptPath: script path to parse
 * @return foundEvents: fill map with found script events
 * @info based on http://www.boost.org/doc/libs/1_31_0/libs/regex/example/snippets/regex_search_example.cpp
 */
void searchEvents(const fs::path& scriptPath, qpid::types::Variant::List* foundEvents)
{
	//get script content
	std::string content = get_file_contents(scriptPath);

	//parse content
	std::string::const_iterator start, end;
	start = content.begin();
	end = content.end();
	boost::match_results<std::string::const_iterator> what;
	boost::match_flag_type flags = boost::match_default;
	std::string lua = "";
	lua = content; //make non blockly script parseable
	while(boost::regex_search(start, end, what, exprAll, flags))
	{
		// what[0] contains the whole string
		// what[1] contains xml
		// what[2] contains lua
		lua = std::string(what[2]);
		// update search position:
		start = what[0].second;
		// update flags:
		flags |= boost::match_prev_avail;
		flags |= boost::match_not_bob;
	}

	start = lua.begin();
	end = lua.end();
	while(boost::regex_search(start, end, what, exprEvent, flags))
	{
		foundEvents->push_back(std::string(what[1]));
		// update search position:
		start = what[0].second;
		// update flags:
		flags |= boost::match_prev_avail;
		flags |= boost::match_not_bob;
	}
}

/**
 * Purge old scripts from scriptsInfos
 */
void purgeScripts()
{
	//TODO walk through all scripts in scriptsInfos and remove non existing entries
}

bool runScript(qpid::types::Variant::Map content, const fs::path &script) {
	//check if file modified
	qpid::types::Variant::Map scripts = scriptsInfos["scripts"].asMap();
	qpid::types::Variant::Map infos;
	std::time_t updated = boost::filesystem::last_write_time(script);
	bool parseScript = false;
	if( scripts[script.string()].isVoid() )
	{
		//force script parsing
		parseScript = true;
	}
	else
	{
		//script already referenced, check last modified date
		infos = scripts[script.string()].asMap();
		if( infos["updated"].asInt32()!=updated )
		{
			//script modified, parse again content
			parseScript = true;
		}
	}
	if( parseScript )
	{
		//AGO_TRACE() << "Update script infos (" << script << ")";
		infos["updated"] = (int32_t)updated;
		qpid::types::Variant::List events;
		searchEvents(script, &events);
		infos["events"] = events;
		scripts[script.string()] = infos;
		scriptsInfos["scripts"] = scripts;
		variantMapToJSONFile(scriptsInfos, getConfigPath(SCRIPTSINFOSFILE));
	}

	//check if current triggered event is caught in script
	bool executeScript = false;
	if( filterByEvents==1 )
	{
		qpid::types::Variant::List events = infos["events"].asList();
		if( events.size()>0 )
		{
			for( qpid::types::Variant::List::iterator it=events.begin(); it!=events.end(); it++ )
			{
				if( (*it)==content["subject"] )
				{
					executeScript = true;
					break;
				}
			}
		}
		else
		{
			//no events detected in script, trigger it everytime :S
			executeScript = true;
		}
	}
	else
	{
		//config option disable events filtering
		executeScript = true;
	}

	if( executeScript )
	{
		AGO_TRACE() << "Running " << script;
		refreshInventory = true;
		lua_State *L;
		const luaL_Reg *lib;

		L = luaL_newstate();
		for (lib = loadedlibs; lib->func; lib++) {
			luaL_requiref(L, lib->name, lib->func, 1);
			lua_pop(L, 1);
		}
		luaL_openlibs(L);

		lua_register(L, "sendMessage", luaSendMessage);
		lua_register(L, "setVariable", luaSetVariable);
		lua_register(L, "getVariable", luaGetVariable);
		lua_register(L, "getDeviceInventory", luaGetDeviceInventory);
		lua_register(L, "getInventory", luaGetInventory);
		// lua_register(L, "addDevice", luaAddDevice);

		pushTableFromMap(L, content);
		lua_setglobal(L, "content");
		//pushTableFromMap(L, inventory);
		//lua_setglobal(L, "inventory");

		int status = luaL_loadfile(L, script.c_str());
		int result = 0;
		if(status == LUA_OK) {
			result = lua_pcall(L, 0, LUA_MULTRET, 0);
		} else {
			AGO_ERROR()<< "Could not load script: " << script;
		}
		if ( result!=0 ) {
			AGO_ERROR() << "Script " << script << " failed: " << lua_tostring(L, -1);
			lua_pop(L, 1); // remove error message
		}

		lua_close(L);
		return status == 0 ? true : false;
	}
	else
	{
		//AGO_TRACE() << "-- NOT running script " << script;
	}
	return true;
}

qpid::types::Variant::Map commandHandler(qpid::types::Variant::Map content) {
	qpid::types::Variant::Map returnval;
	if (content["command"] == "inventory") return returnval;
	std::string internalid = content["internalid"].asString();
	if (internalid == "luacontroller") {
		if (content["command"]=="getscriptlist") {
			qpid::types::Variant::List scriptlist;
			if (fs::exists(scriptdir)) {
				fs::recursive_directory_iterator it(scriptdir);
				fs::recursive_directory_iterator endit;
				while (it != endit) {
					if (fs::is_regular_file(*it) && (it->path().extension().string() == ".lua") && (it->path().filename().string() != "helper.lua")) {
						scriptlist.push_back(qpid::types::Variant(it->path().stem().string()));
					}
					++it;
				}
			}
			returnval["scriptlist"]=scriptlist;
			returnval["result"]=0;
		} else if (content["command"] == "getscript") {
			if (content["name"].asString() != "") {
				try {
					// if a path is passed, strip it for security reasons
					fs::path input(content["name"]);
					fs::path script = construct_script_name(input.stem());
					string scriptcontent = get_file_contents(script);
					AGO_TRACE() << "reading script " << script;
					returnval["script"]=base64_encode(reinterpret_cast<const unsigned char*>(scriptcontent.c_str()), scriptcontent.length());
					returnval["result"]=0;
					returnval["name"]=content["name"].asString();
				} catch(...) {
					returnval["error"]="can't read script";
					returnval["result"]=-1;
				}
			}
		} else if (content["command"] == "setscript") {
			if (content["name"].asString() != "") {
				try {
					// if a path is passed, strip it for security reasons
					fs::path input(content["name"]);
					fs::path script = construct_script_name(input.stem());
					std::ofstream file;
					// XXX: this did not seem to throw even if the directory did not exist...
					file.open(script.c_str());
					file << content["script"].asString();
					file.close();
					returnval["error"]="";
					returnval["result"]=0;
				} catch(...) {
					returnval["error"]="can't write script";
					returnval["result"]=-1;
				}
			}
		} else if (content["command"] == "delscript") {
			if (content["name"].asString() != "") {
				try {
					// if a path is passed, strip it for security reasons
					fs::path input(content["name"]);
					fs::path target = construct_script_name(input.stem());
					if (fs::remove (target)) {
						returnval["result"]=0;
					} else {
						returnval["error"]="no such script";
						returnval["result"]=-1;
					}
				} catch(...) {
					returnval["error"]="can't delete script";
					returnval["result"]=-1;
				}
			}
		} else if (content["command"] == "renscript") {
			if ( !content["oldname"].isVoid() && content["oldname"].asString()!="" && !content["newname"].isVoid() && content["newname"].asString()!="" )
			{
				try {
					// if a path is passed, strip it for security reasons
					fs::path input(content["oldname"]);
					fs::path source = construct_script_name(input.stem());

					fs::path output(content["newname"]);
					fs::path target = construct_script_name(output.stem());

					//check if destination file already exists
					if( !fs::exists(target) )
					{
						//rename script
						fs::rename(source, target);
						returnval["result"]=0;
					}
					else
					{
						returnval["error"]="Script with new name already exists. Script not renamed";
						returnval["result"]=-1;
					}
				} catch( const exception& e ) {
					AGO_ERROR() << "Exception during file renaming" << e.what();
					returnval["error"]="Unable to rename script";
					returnval["result"]=-1;
				}
			}
		} else if (content["command"] == "uploadfile") {
			//import script
			if( !content["filepath"].isVoid() && content["filepath"].asString()!="" &&
				 !content["filename"].isVoid() && content["filename"].asString()!="" )
			{
				//check file
				fs::path source(content["filepath"]);
				if( fs::is_regular_file(status(source)) && source.extension().string()==".lua")
				{
					try {
						std::string filename;
						if( content["filename"].asString().find("blockly_")!=0 )
						{
							// prepend "blockly_" string
							filename = "blockly_";
						}
						filename += content["filename"].asString();

						fs::path target = scriptdir / filename;

						//check if desination file already exists
						if( !fs::exists(target) )
						{
							//move file
							AGO_DEBUG() << "import " << source << " to " << target;
							fs::copy_file(source, target);
							returnval["error"] = "";
							returnval["result"] = 0;
						}
						else
						{
							AGO_DEBUG() << "Script already exists, nothing overwritten";
							returnval["error"] = "Script already exists. Script not imported";
							returnval["result"] = -1;
						}
					} catch( const exception& e ) {
						AGO_ERROR() << "Exception during script import" << e.what();
						returnval["error"] = "Unable to import script";
						returnval["result"] = -1;
					}
				}
				else
				{
					//invalid file, reject it
					AGO_ERROR() << "Unsupported file uploaded";
					returnval["error"] = "Unsupported file";
					returnval["result"] = -1;
				}
			}
			else
			{
				//invalid request
				AGO_ERROR() << "Invalid file upload request";
				returnval["error"] = "Invalid request";
				returnval["result"] = -1;
			}
		} else if (content["command"] == "downloadfile") {
			AGO_TRACE() << "download file command received: " << content;
			//export script
			if( !content["filename"].isVoid() && content["filename"].asString()!="" )
			{
				std::string file = "blockly_" + content["filename"].asString();
				fs::path target = construct_script_name(file);
				AGO_DEBUG() << "file to download " << target;

				//check if file exists
				if( fs::exists(target) )
				{
					//file exists, return full path
					AGO_DEBUG() << "Send fullpath of file to download " << target;
					returnval["error"] = "";
					returnval["filepath"] = target.string();
					returnval["result"] = 0;
				}
				else
				{
					//requested file doesn't exists
					AGO_ERROR() << "File to download doesn't exist";
					returnval["error"] = "File doesn't exist";
					returnval["result"] = -1;
				}
			}
			else
			{
				//invalid request
				AGO_ERROR() << "Invalid file upload request";
				returnval["error"]="Invalid request";
				returnval["result"]=-1;
			}
		} else {
			returnval["error"]="invalid command";
			returnval["result"]=-1;
		}
	} else {
		if (fs::exists(scriptdir)) {
			fs::recursive_directory_iterator it(scriptdir);
			fs::recursive_directory_iterator endit;
			while (it != endit) {
				if (fs::is_regular_file(*it) && (it->path().extension().string() == ".lua") &&
						(it->path().filename().string() != "helper.lua")) {
					runScript(content, it->path());
				}
				++it;
			}
		}
	}
	return returnval;
}

void eventHandler(std::string subject, qpid::types::Variant::Map content) {
	if (subject == "event.device.announce") return;
	content["subject"]=subject;
	commandHandler(content);
}

int main(int argc, char **argv) {

	agoConnection = new AgoConnection("lua");

	agocontroller = agoConnection->getAgocontroller();

	//get config
	std::string optString = getConfigOption("lua", "filterByEvents", "1");
	sscanf(optString.c_str(), "%d", &filterByEvents);

	scriptdir = ensureDirExists(getConfigPath(LUA_SCRIPT_DIR));

	//load script infos file
	scriptsInfos = jsonFileToVariantMap(getConfigPath(SCRIPTSINFOSFILE));
	if (scriptsInfos["scripts"].isVoid())
	{
		qpid::types::Variant::Map scripts;
		scriptsInfos["scripts"] = scripts;
		variantMapToJSONFile(scriptsInfos, getConfigPath(SCRIPTSINFOSFILE));
	}

	agoConnection->addDevice("luacontroller", "luacontroller");
	agoConnection->addHandler(commandHandler);
	agoConnection->addEventHandler(eventHandler);
	// sleep(5);
	// for (qpid::types::Variant::Map::const_iterator it = eventmap.begin(); it!=eventmap.end(); it++) { 


	agoConnection->run();

}
