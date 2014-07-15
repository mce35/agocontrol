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
#define SCRIPTSINFOSFILE CONFDIR "/maps/agolua.json"
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

#ifndef LUA_SCRIPT_DIR
#define LUA_SCRIPT_DIR CONFDIR "/lua/"
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

static const luaL_Reg loadedlibs[] = {
  {"_G", luaopen_base},
  {LUA_TABLIBNAME, luaopen_table},
  {LUA_STRLIBNAME, luaopen_string},
  {LUA_MATHLIBNAME, luaopen_math},
  {NULL, NULL}
};

// read file into string. credits go to "insane coder" - http://stackoverflow.com/questions/2602013/read-whole-ascii-file-into-c-stdstring
std::string get_file_contents(const char *filename) {
	std::ifstream in(filename, std::ios::in | std::ios::binary);
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
	cout << "Sending message: " << subject << " " << content << endl;
	qpid::types::Variant::Map replyMap = agoConnection->sendMessageReply(subject.c_str(), content);	 
	lua_pushnumber(l, 0);
	return 1;
}

int luaSetVariable(lua_State *L) {
    qpid::types::Variant::Map content;
    std::string subject;
    // number of input arguments
    int argc = lua_gettop(L);

    //get input arguments
    content["variable"] = std::string(lua_tostring(L,1));
    content["value"] = std::string(lua_tostring(L,2));
    content["command"]="setvariable";
    content["uuid"]=agocontroller;
    cout << "Sending message: " << content << endl;
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
	cout << "Get device inventory: inventory['devices'][" << uuid << "][" << attribute << "]";
    if( subAttribute.length()>0 )
    {
        cout << "[" << subAttribute << "]";
    }
    cout << endl;
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
				//cout << "undhandled type: " << it->second.getType() << endl;
				//lua_settable(L, -3);
		}
	}	
}

/**
 * Search triggered events in specified script
 * @param scriptPath: script path to parse
 * @return foundEvents: fill map with found script events
 * @info based on http://www.boost.org/doc/libs/1_31_0/libs/regex/example/snippets/regex_search_example.cpp
 */
void searchEvents(const char* scriptPath, qpid::types::Variant::List* foundEvents)
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

bool runScript(qpid::types::Variant::Map content, const char *script) {
    //check if file modified
    qpid::types::Variant::Map scripts = scriptsInfos["scripts"].asMap();
    qpid::types::Variant::Map infos;
    std::time_t updated = boost::filesystem::last_write_time(script);
    bool parseScript = false;
    if( scripts[script].isVoid() )
    {
        //force script parsing
        parseScript = true;
    }
    else
    {
        //script already referenced, check last modified date
        infos = scripts[script].asMap();
        if( infos["updated"].asInt32()!=updated )
        {
            //script modified, parse again content
            parseScript = true;
        }
    }
    if( parseScript )
    {
        //cout << "Update script infos (" << script << ")" << endl;
        infos["updated"] = (int32_t)updated;
        qpid::types::Variant::List events;
        searchEvents(script, &events);
        infos["events"] = events;
        scripts[script] = infos;
        scriptsInfos["scripts"] = scripts;
        variantMapToJSONFile(scriptsInfos, SCRIPTSINFOSFILE);
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
        cout << "========================================" << endl;
        cout << "Running " << script << "..." << endl;
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
        // lua_register(L, "addDevice", luaAddDevice);

        pushTableFromMap(L, content);
        lua_setglobal(L, "content");
        //pushTableFromMap(L, inventory);
        //lua_setglobal(L, "inventory");

        int status = luaL_loadfile(L, script);
        int result = 0;
        if(status == LUA_OK) {
            result = lua_pcall(L, 0, LUA_MULTRET, 0);
        } else {
            std::cout << "-- Could not load the script " << script << std::endl;
        }
        if ( result!=0 ) {
            std::cerr << "-- SCRIPT FAILED: " << lua_tostring(L, -1) << std::endl;
            lua_pop(L, 1); // remove error message
        }
        
        lua_close(L);
        cout << "========================================" << endl;
        return status == 0 ? true : false;
    }
    else
    {
        //cout << "-- NOT running script " << script << endl;
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
			fs::path scriptdir(LUA_SCRIPT_DIR);
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
					string script = LUA_SCRIPT_DIR + input.stem().string() + ".lua";
					string scriptcontent = get_file_contents(script.c_str());
					cout << "reading script " << script << endl;
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
					string script = LUA_SCRIPT_DIR + input.stem().string() + ".lua";
					std::ofstream file;
					file.open(script.c_str());
					file << content["script"].asString();
					file.close();
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
					string script = LUA_SCRIPT_DIR + input.stem().string() + ".lua";
					fs::path target(script);
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
		} else {
			returnval["error"]="invalid command";
			returnval["result"]=-1;
		}
		return returnval;
	} else {
		fs::path scriptdir(LUA_SCRIPT_DIR);
		if (fs::exists(scriptdir)) {
			fs::recursive_directory_iterator it(scriptdir);
			fs::recursive_directory_iterator endit;
			while (it != endit) {
				if (fs::is_regular_file(*it) && (it->path().extension().string() == ".lua") && (it->path().filename().string() != "helper.lua")) {
					runScript(content, it->path().c_str());
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

	agocontroller="";
	while(agocontroller=="") {
		qpid::types::Variant::Map inventory = agoConnection->getInventory();
		if (!(inventory["devices"].isVoid())) {
			qpid::types::Variant::Map devices = inventory["devices"].asMap();
			qpid::types::Variant::Map::const_iterator it;
			for (it = devices.begin(); it != devices.end(); it++) {
				if (!(it->second.isVoid())) {
					qpid::types::Variant::Map device = it->second.asMap();
					if (device["devicetype"] == "agocontroller") {
						cout << "Agocontroller: " << it->first << endl;
						agocontroller = it->first;
					}
				}
			}
		}
	}

    //get config
    std::string optString = getConfigOption("lua", "filterByEvents", "1");
    sscanf(optString.c_str(), "%d", &filterByEvents);

    //load script infos file
    scriptsInfos = jsonFileToVariantMap(SCRIPTSINFOSFILE);
    if (scriptsInfos["scripts"].isVoid())
    {
        qpid::types::Variant::Map scripts;
        scriptsInfos["scripts"] = scripts;
        variantMapToJSONFile(scriptsInfos, SCRIPTSINFOSFILE);
    }

	agoConnection->addDevice("luacontroller", "luacontroller");
	agoConnection->addHandler(commandHandler);
	agoConnection->addEventHandler(eventHandler);
	// sleep(5);
	// for (qpid::types::Variant::Map::const_iterator it = eventmap.begin(); it!=eventmap.end(); it++) { 


	agoConnection->run();

}
