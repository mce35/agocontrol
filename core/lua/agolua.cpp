#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <sstream>
#include <cerrno>

#define BOOST_FILESYSTEM_VERSION 3
#define BOOST_FILESYSTEM_NO_DEPRECATED
#include "boost/filesystem.hpp"
#include "boost/regex.hpp"

#ifndef SCRIPTSINFOSFILE
#define SCRIPTSINFOSFILE "maps/agolua.json"
#endif

#ifndef LUA_SCRIPT_DIR
#define LUA_SCRIPT_DIR "lua"
#endif

#include "base64.h"

#include "agoapp.h"

namespace fs = ::boost::filesystem;

using namespace std;
using namespace agocontrol;
using namespace qpid::types;

#ifdef __FreeBSD__
#include "lua52/lua.hpp"
#else
#include "lua5.2/lua.hpp"
#endif

const char reAll[] = "--.*--\\[\\[(.*)\\]\\](.*)";
const char reEvent[] = "(event\\.\\w+\\.\\w+)";

enum DebugMsgType {
    DBG_START = 0, //display start message
    DBG_END, //display end message
    DBG_ERROR, //display error message
    DBG_INFO //display info message
};

class AgoLua: public AgoApp {
private:
    qpid::types::Variant::Map inventory;
    bool refreshInventory; //optimize script execution (0.2s to get all inventory)
    qpid::types::Variant::Map scriptsInfos;
    boost::regex exprAll;
    boost::regex exprEvent;
    std::string agocontroller;
    int filterByEvents;
    fs::path scriptdir;
    qpid::types::Variant::Map scriptContexts;

    fs::path construct_script_name(fs::path input) ;
    void pushTableFromMap(lua_State *L, qpid::types::Variant::Map content) ;
    void pullTableToMap(lua_State *L, qpid::types::Variant::Map& table);

    void searchEvents(const fs::path& scriptPath, qpid::types::Variant::List* foundEvents) ;
    void purgeScripts() ;
    void initScript(lua_State* L, qpid::types::Variant::Map& content, const std::string& script, qpid::types::Variant::Map& context, bool debug);
    void finalizeScript(lua_State* L, qpid::types::Variant::Map& content, const std::string& script, qpid::types::Variant::Map& context);
    void debugScript(qpid::types::Variant::Map content, const std::string script);
    void executeScript(qpid::types::Variant::Map content, const fs::path &script);
    bool canExecuteScript(qpid::types::Variant::Map content, const fs::path &script);
    qpid::types::Variant::Map commandHandler(qpid::types::Variant::Map content) ;
    void eventHandler(std::string subject, qpid::types::Variant::Map content) ;

    void setupApp();

public:
    AGOAPP_CONSTRUCTOR_HEAD(AgoLua)
        , refreshInventory(true)
        , exprAll(reAll)
        , exprEvent(reEvent)
        , filterByEvents(1)
        {}

    // called from global static wrapper functions, thus public
    int luaAddDevice(lua_State *l);
    int luaSendMessage(lua_State *l);
    int luaSetVariable(lua_State *L);
    int luaGetVariable(lua_State *L);
    int luaGetDeviceInventory(lua_State *L);
    int luaGetInventory(lua_State *L);
    int luaPause(lua_State *L);
    int luaDebugPause(lua_State *L);
    int luaGetDeviceName(lua_State *L);
    int luaDebugPrint(lua_State* L);
};


#define LUA_WRAPPER(method_name) \
    static int method_name ## _wrapper(lua_State *l) { \
        AgoLua *inst = (AgoLua*) lua_touserdata(l, lua_upvalueindex(1));\
        return inst->method_name(l);\
    }

LUA_WRAPPER(luaSendMessage);
LUA_WRAPPER(luaSetVariable);
LUA_WRAPPER(luaGetVariable);
LUA_WRAPPER(luaGetDeviceInventory);
LUA_WRAPPER(luaGetInventory);
LUA_WRAPPER(luaPause);
LUA_WRAPPER(luaDebugPause);
LUA_WRAPPER(luaGetDeviceName);
LUA_WRAPPER(luaDebugPrint);

const luaL_Reg loadedlibs[] = {
    {"_G", luaopen_base},
    {LUA_TABLIBNAME, luaopen_table},
    {LUA_STRLIBNAME, luaopen_string},
    {LUA_MATHLIBNAME, luaopen_math},
    {NULL, NULL}
};

fs::path AgoLua::construct_script_name(fs::path input)
{
    fs::path out = (scriptdir / input);
    // replace == add/append
    out.replace_extension(".lua");
    return out;
}

// read file into string. credits go to "insane coder" - http://stackoverflow.com/questions/2602013/read-whole-ascii-file-into-c-stdstring
static std::string get_file_contents(const fs::path &filename)
{
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

void AgoLua::pushTableFromMap(lua_State *L, qpid::types::Variant::Map content)
{
    lua_createtable(L, 0, 0);
    for (qpid::types::Variant::Map::const_iterator it=content.begin(); it!=content.end(); it++)
    {
        switch (it->second.getType())
        {
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

/**
 * Fill specified cpp variant map with lua table content
 * Before calling this function you need to call lua_getglobal(<lua obj>, <lua table name>)
 */
void AgoLua::pullTableToMap(lua_State *L, qpid::types::Variant::Map& table)
{
    lua_pushnil(L);
    while( lua_next(L, -2)!=0 )
    {
        if( lua_isstring(L, -1) )
        {
            table[lua_tostring(L, -2)] = lua_tostring(L, -1);
        }
        else if( lua_isnumber(L, -1) )
        {
            table[lua_tostring(L, -2)] = lua_tonumber(L, -1);
        }
        else if( lua_isboolean(L, -1) )
        {
            table[lua_tostring(L, -2)] = lua_toboolean(L, -1);
        }
        else if( lua_isnoneornil(L, -1) )
        {
            //drop null field
        }
        else if( lua_istable(L, -1) )
        {
            qpid::types::Variant::Map newTable;
            pullTableToMap(L, newTable);
            table[lua_tostring(L, -2)] = newTable;
        }

        lua_pop(L, 1);
    }
}

/**
 * Send message LUA function
 */
int AgoLua::luaSendMessage(lua_State *L)
{
    qpid::types::Variant::Map content;
    std::string subject;
    // number of input arguments
    int argc = lua_gettop(L);

    // print input arguments
    for(int i=0; i<argc; i++)
    {
        string name, value;
        if (nameval(string(lua_tostring(L, lua_gettop(L))),name, value))
        {
            if (name == "subject")
            {
                subject = value;
            }
            content[name]=value;
        }
        lua_pop(L, 1);
    }

    //execute sendMessage
    AGO_DEBUG() << "Sending message: " << subject << " " << content;
    qpid::types::Variant::Map replyMap = agoConnection->sendMessageReply(subject.c_str(), content);
    pushTableFromMap(L, replyMap);

    return 1;
}

/**
 * Set variable LUA function
 */
int AgoLua::luaSetVariable(lua_State *L)
{
    qpid::types::Variant::Map content;
    std::string subject;

    //get input arguments
    content["variable"] = std::string(lua_tostring(L,1));
    content["value"] = std::string(lua_tostring(L,2));
    content["command"]="setvariable";
    content["uuid"]=agocontroller;

    AGO_DEBUG() << "Setting variable: " << content;
    AgoResponse resp = agoConnection->sendRequest(subject.c_str(), content);

    //manage result
    if( resp.isOk() )
    {
        // XXX: Not sure what we are supposed to set here; returncode was set in older code
        // but there are no docs on how this can be used.
        lua_pushnumber(L, 1);
    }
    else
    {
        //sendcommand problem
        lua_pushnumber(L, 0);
    }

    //refresh inventory only once (performance optimization)
    if( refreshInventory )
    {
        inventory = agoConnection->getInventory();
        refreshInventory = false;
    }

    if( inventory.size()>0 && !inventory["devices"].isVoid() && !inventory["variables"].isVoid() )
    {
        //update current inventory to reflect changes without reloading it (too long!!)
        qpid::types::Variant::Map variables = inventory["variables"].asMap();
        if( !variables[content["variable"]].isVoid() )
        {
            variables[content["variable"]] = content["value"];
            inventory["variables"] = variables;
            lua_pushnumber(L, 1);
        }
        else
        {
            //unknown variable
            lua_pushnumber(L, 0);
        }
    }
    else
    {
        //no inventory available
        refreshInventory = false;
        lua_pushnumber(L, 0);
    }

    return 1;
}

/**
 * Return variable value in LUA
 */
int AgoLua::luaGetVariable(lua_State *L)
{
    //init
    std::string variableName = "";

    //refresh inventory only once (performance optimization)
    if( refreshInventory )
    {
        inventory = agoConnection->getInventory();
        refreshInventory = false;
    }

    if( inventory.size()>0 && !inventory["devices"].isVoid() )
    {
        qpid::types::Variant::Map deviceInventory = inventory["devices"].asMap();

        //get variable name
        variableName = std::string(lua_tostring(L,1));

        if( variableName.length()>0 && !inventory["variables"].isVoid() )
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
    }
    else
    {
        //no inventory available
        refreshInventory = true;
        lua_pushnil(L);
    }

    return 1;
}

/**
 * Return value from inventory["device"]
 * Callback format: getDeviceInventory(uuid, param)
 */
int AgoLua::luaGetDeviceInventory(lua_State *L)
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

    if( inventory.size()>0 && !inventory["devices"].isVoid() )
    {
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

        AGO_DEBUG() << "Get device inventory: inventory['devices'][" << uuid << "][" << attribute << "]";
        if( subAttribute.length()>0 )
        {
            AGO_DEBUG() << "[" << subAttribute << "]";
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
                bool found = false;
                if( !attributes["values"].isVoid() )
                {
                    qpid::types::Variant::Map values = attributes["values"].asMap();
                    for( qpid::types::Variant::Map::iterator it=values.begin(); it!=values.end(); it++ )
                    {
                        //TODO return device value property (quantity, unit, latitude, longitude...)
                        qpid::types::Variant::Map value;
                        if (!(it->second.isVoid())) value = it->second.asMap();
                        lua_pushstring(L, value[subAttribute].asString().c_str());
                        found = true;
                        break;
                    }
                }

                //handle item not found
                if( !found )
                {
                    lua_pushnil(L);
                }
            }
        }
        else
        {
            //device not found
            lua_pushnil(L);
        }
    }
    else
    {
        //no inventory available
        refreshInventory = true;
        lua_pushnil(L);
    }

    return 1;
}

/**
 * Force getting inventory manually
 */
int AgoLua::luaGetInventory(lua_State *L)
{
    inventory = agoConnection->getInventory();
    refreshInventory = false;
    pushTableFromMap(L, inventory);
    return 1;
}

/**
 * Pause script execution
 */
int AgoLua::luaPause(lua_State *L)
{
    //init
    int duration;

    //get duration
    duration = (int)lua_tointeger(L,1);

    //pause script
    sleep(duration);

    lua_pushnil(L);
    return 1;
}

/**
 * Pause script execution (debug mode)
 */
int AgoLua::luaDebugPause(lua_State* L)
{
    //disable pause in debug mode
    lua_pushnil(L);
    return 1;
}

/**
 * Print function (debug mode)
 */
int AgoLua::luaDebugPrint(lua_State* L)
{
    //init
    qpid::types::Variant::Map printResult;
    std::string msg = std::string(lua_tostring(L,1));

    AGO_DEBUG() << "Debug script: print " << msg;
    printResult["type"] = DBG_INFO;
    printResult["msg"] = msg;
    agoConnection->emitEvent("luacontroller", "event.system.debugscript", printResult);
    lua_pushnil(L);
    return 1;
}

/**
 * Get device name according to specified uuid
 */
int AgoLua::luaGetDeviceName(lua_State* L)
{
    //init
    std::string uuid;

    //refresh inventory only once (performance optimization)
    if( refreshInventory )
    {
        inventory = agoConnection->getInventory();
        refreshInventory = false;
    }

    if( inventory.size()>0 && !inventory["devices"].isVoid() )
    {
        qpid::types::Variant::Map deviceInventory = inventory["devices"].asMap();

        //get uuid
        uuid = std::string(lua_tostring(L,1));
        if( uuid.length()>0 && !inventory["devices"].isVoid() )
        {
            qpid::types::Variant::Map devices = inventory["devices"].asMap();
            if( !devices[uuid].isVoid() )
            {
                qpid::types::Variant::Map device = devices[uuid].asMap();
                if( !device["name"].isVoid() )
                {
                    lua_pushstring(L, device["name"].asString().c_str());
                }
                else
                {
                    //no field name for found device
                    lua_pushnil(L);
                }
            }
            else
            {
                //unknown device
                lua_pushnil(L);
            }
        }
        else
        {
            //no uuid, function failed
            lua_pushnil(L);
        }
    }
    else
    {
        //no inventory available
        refreshInventory = true;
        lua_pushnil(L);
    }

    return 1;
}

/**
 * Search triggered events in specified script
 * @param scriptPath: script path to parse
 * @return foundEvents: fill map with found script events
 * @info based on http://www.boost.org/doc/libs/1_31_0/libs/regex/example/snippets/regex_search_example.cpp
 */
void AgoLua::searchEvents(const fs::path& scriptPath, qpid::types::Variant::List* foundEvents)
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
void AgoLua::purgeScripts()
{
    //TODO walk through all scripts in scriptsInfos and remove non existing entries
}

/**
 * Init script execution
 */
void AgoLua::initScript(lua_State* L, qpid::types::Variant::Map& content, const std::string& script, qpid::types::Variant::Map& context, bool debug)
{
    //load libs
    const luaL_Reg *lib;
    for (lib = loadedlibs; lib->func; lib++)
    {
        luaL_requiref(L, lib->name, lib->func, 1);
        lua_pop(L, 1);
    }
    luaL_openlibs(L);

    //load script context (from local variables)
    if( scriptContexts[script].isVoid() )
    {
        //no context yet, create empty one
        scriptContexts[script] = context;
    }
    else
    {
        if (!scriptContexts[script].isVoid()) context = scriptContexts[script].asMap();
    }
    AGO_TRACE() << "LUA context before:" << context;

    //add mapped functions
#define LUA_REGISTER_WRAPPER(L, lua_name, method_name) \
    lua_pushlightuserdata(L, this); \
    lua_pushcclosure(L, &method_name ## _wrapper, 1); \
    lua_setglobal(L, lua_name)

    LUA_REGISTER_WRAPPER(L, "sendMessage", luaSendMessage);
    LUA_REGISTER_WRAPPER(L, "setVariable", luaSetVariable);
    LUA_REGISTER_WRAPPER(L, "getVariable", luaGetVariable);
    LUA_REGISTER_WRAPPER(L, "getDeviceInventory", luaGetDeviceInventory);
    LUA_REGISTER_WRAPPER(L, "getInventory", luaGetInventory);
    if( !debug )
    {
        LUA_REGISTER_WRAPPER(L, "pause", luaPause);
    }
    else
    {
        LUA_REGISTER_WRAPPER(L, "pause", luaDebugPause);
    }
    LUA_REGISTER_WRAPPER(L, "getDeviceName", luaGetDeviceName);
    if( debug )
    {
        LUA_REGISTER_WRAPPER(L, "print", luaDebugPrint);
    }

    pushTableFromMap(L, content);
    lua_setglobal(L, "content");
    pushTableFromMap(L, context);
    lua_setglobal(L, "context");
}

/**
 * Finalize script execution
 */
void AgoLua::finalizeScript(lua_State* L, qpid::types::Variant::Map& content, const std::string& script, qpid::types::Variant::Map& context)
{
    //handle context value
    lua_getglobal(L, "context");
    pullTableToMap(L, context);
    scriptContexts[script] = context;
    AGO_TRACE() << "LUA context after:" << context;
}

/**
 * Debug script (threaded)
 */
void AgoLua::debugScript(qpid::types::Variant::Map content, const std::string script)
{
    //init
    qpid::types::Variant::Map debugResult;
    refreshInventory = true;
    qpid::types::Variant::Map context;
    lua_State *L;

    try
    {
        //create new lua
        L = luaL_newstate();

        //init script
        initScript(L, content, "debug", context, true);

        //execute script
        AGO_TRACE() << "Debugging script";
        debugResult["type"] = DBG_START;
        debugResult["msg"] = "Script debugging started";
        agoConnection->emitEvent("luacontroller", "event.system.debugscript", debugResult);
        int status = luaL_loadstring(L, script.c_str());
        int result = 0;
        if(status == LUA_OK)
        {
            result = lua_pcall(L, 0, LUA_MULTRET, 0);
        }
        else
        {
            AGO_ERROR()<< "Could not load debug script";
            debugResult["type"] = DBG_ERROR;
            debugResult["msg"] = "Could not load debug script";
            agoConnection->emitEvent("luacontroller", "event.system.debugscript", debugResult);
        }
        if ( result!=0 )
        {
            std::string err = lua_tostring(L, -1);
            AGO_ERROR() << "Debug failed: " << err;
            debugResult["type"] = DBG_ERROR;
            debugResult["msg"] = err;
            agoConnection->emitEvent("luacontroller", "event.system.debugscript", debugResult);
            lua_pop(L, 1); // remove error message
        }
 
        //finalize script
        finalizeScript(L, content, "debug", context);
        lua_close(L);
        AGO_TRACE() << "Debug execution finished.";
        debugResult["type"] = DBG_END;
        debugResult["msg"] = "Script debugging terminated";
        agoConnection->emitEvent("luacontroller", "event.system.debugscript", debugResult);
    }
    catch(...)
    {
        debugResult["type"] = DBG_ERROR;
        debugResult["msg"] = "Script debugging crashed";
        agoConnection->emitEvent("luacontroller", "event.system.debugscript", debugResult);
    }
}

/**
 * Execute LUA script (blocking).
 * Called in separate thread.
 */
void AgoLua::executeScript(qpid::types::Variant::Map content, const fs::path &script)
{
    //init
    // XXX: This is global for all scripts!
    refreshInventory = true;
    qpid::types::Variant::Map context;
    lua_State *L;
    L = luaL_newstate();

    //init script
    initScript(L, content, script.string(), context, false);

    //execute script
    AGO_TRACE() << "Loading " << script;
    int status = luaL_loadfile(L, script.c_str());
    int result = 0;
    if(status == LUA_OK)
    {
        result = lua_pcall(L, 0, LUA_MULTRET, 0);
    }
    else
    {
        AGO_ERROR()<< "Could not load script: " << script;
    }
    if ( result!=0 )
    {
        AGO_ERROR() << "Script " << script << " failed: " << lua_tostring(L, -1);
        lua_pop(L, 1); // remove error message
    }

    //finalize script
    finalizeScript(L, content, script.string(), context);
    lua_close(L);
    AGO_TRACE() << "Execution of " << script << " finished.";
}

/**
 * Return true if script can be executed
 */
bool AgoLua::canExecuteScript(qpid::types::Variant::Map content, const fs::path &script)
{
    //check integrity
    if( scriptsInfos["scripts"].isVoid() )
    {
        return false;
    }

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
        if( !scripts[script.string()].isVoid() )
        {
            infos = scripts[script.string()].asMap();
            if( infos["updated"].asInt32()!=updated )
            {
                //script modified, parse again content
                parseScript = true;
            }
        }
    }
    if( parseScript )
    {
        AGO_DEBUG() << "Update script infos (" << script << ")";
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

    return executeScript;
}

/**
 * Agocontrol command handler
 */
qpid::types::Variant::Map AgoLua::commandHandler(qpid::types::Variant::Map content)
{
    qpid::types::Variant::Map returnData;
    // XXX: Why does lua answer on inventory??
#if 0
    if (content["command"] == "inventory")
    {
        return returnData;
    }
#endif

    std::string internalid = content["internalid"].asString();
    if (internalid == "luacontroller")
    {
        if (content["command"]=="getscriptlist")
        {
            qpid::types::Variant::List scriptlist;
            if (fs::exists(scriptdir))
            {
                fs::recursive_directory_iterator it(scriptdir);
                fs::recursive_directory_iterator endit;
                while (it != endit)
                {
                    if (fs::is_regular_file(*it) && (it->path().extension().string() == ".lua") && (it->path().filename().string() != "helper.lua"))
                    {
                        scriptlist.push_back(qpid::types::Variant(it->path().stem().string()));
                    }
                    ++it;
                }
            }
            returnData["scriptlist"]=scriptlist;
            return responseSuccess(returnData);
        }
        else if (content["command"] == "getscript")
        {
            checkMsgParameter(content, "name", VAR_STRING);

            try
            {
                // if a path is passed, strip it for security reasons
                fs::path input(content["name"]);
                fs::path script = construct_script_name(input.stem());
                string scriptcontent = get_file_contents(script);
                AGO_DEBUG() << "Reading script " << script;
                returnData["script"]=base64_encode(reinterpret_cast<const unsigned char*>(scriptcontent.c_str()), scriptcontent.length());
                returnData["name"]=content["name"].asString();
                return responseSuccess(returnData);
            }
            catch( const fs::filesystem_error& e )
            {
                AGO_ERROR() << "Exception during file reading " << e.what();
                return responseFailed("Unable to read script");
            }
        }
        else if (content["command"] == "setscript" )
        {
            checkMsgParameter(content, "name", VAR_STRING);

            try
            {
                // if a path is passed, strip it for security reasons
                fs::path input(content["name"]);
                fs::path script = construct_script_name(input.stem());
                std::ofstream file;

                // XXX: this did not seem to throw even if the directory did not exist...
                file.open(script.c_str());
                file << content["script"].asString();
                file.close();
                return responseSuccess();
            }
            catch( const fs::filesystem_error& e )
            {
                AGO_ERROR() << "Exception during file writing " << e.what();
                return responseFailed("Unable to write script");
            }
        }
        else if (content["command"] == "delscript")
        {
            checkMsgParameter(content, "name", VAR_STRING);

            try
            {
                // if a path is passed, strip it for security reasons
                fs::path input(content["name"]);
                fs::path target = construct_script_name(input.stem());
                if (fs::remove (target))
                {
                    return responseSuccess();
                }
                else
                {
                    return responseFailed("no such script");
                }
            }
            catch( const fs::filesystem_error& e )
            {
                AGO_ERROR() << "Exception during file deleting " << e.what();
                return responseFailed("Unable to delete script");
            }
        }
        else if (content["command"] == "renscript")
        {
            checkMsgParameter(content, "oldname", VAR_STRING);
            checkMsgParameter(content, "newname", VAR_STRING);
        
            try
            {
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
                    return responseSuccess();
                }
                else
                {
                    return responseFailed("Script with new name already exists. Script not renamed");
                }
            }
            catch( const fs::filesystem_error& e )
            {
                AGO_ERROR() << "Exception during file renaming" << e.what();
                return responseFailed("Unable to rename script");
            }
        }
        else if( content["command"]=="debugscript" )
        {
            AGO_DEBUG() << "debug received: " << content;
            checkMsgParameter(content, "script", VAR_STRING);
            checkMsgParameter(content, "data", VAR_MAP);

            std::string script = content["script"].asString();
            qpid::types::Variant::Map data = content["data"].asMap();
            AGO_DEBUG() << "Debug script: script=" << script << " content=" << data;

            boost::thread t( boost::bind(&AgoLua::debugScript, this, data, script) );
            t.detach();

            AGO_INFO() << "Command 'debugscript': debug started";
            return responseSuccess();
        }
        else if (content["command"] == "uploadfile")
        {
            //import script
            checkMsgParameter(content, "filepath", VAR_STRING);
            checkMsgParameter(content, "filename", VAR_STRING);

            //check file
            fs::path source(content["filepath"]);
            if( fs::is_regular_file(status(source)) && source.extension().string()==".lua")
            {
                try
                {
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
                        return responseSuccess();
                    }
                    else
                    {
                        AGO_DEBUG() << "Script already exists, nothing overwritten";
                        return responseFailed("Script already exists. Script not imported");
                    }
                }
                catch( const exception& e )
                {
                    AGO_ERROR() << "Exception during script import" << e.what();
                    return responseFailed("Unable to import script");
                }
            }
            else
            {
                //invalid file, reject it
                AGO_ERROR() << "Unsupported file uploaded";
                return responseFailed("Unsupported file");
            }
        }
        else if (content["command"] == "downloadfile")
        {
            //export script
            AGO_DEBUG() << "download file command received: " << content;
            checkMsgParameter(content, "filename", VAR_STRING);

            std::string file = "blockly_" + content["filename"].asString();
            fs::path target = construct_script_name(file);
            AGO_DEBUG() << "file to download " << target;

            //check if file exists
            if( fs::exists(target) )
            {
                //file exists, return full path
                AGO_DEBUG() << "Send fullpath of file to download " << target;
                returnData["filepath"] = target.string();
                return responseSuccess(returnData);
            }
            else
            {
                //requested file doesn't exists
                AGO_ERROR() << "File to download doesn't exist";
                return responseFailed("File doesn't exist");
            }
        }
        else
        {
            return responseUnknownCommand();
        }
    }
    else
    {
        //execute scripts
        bool found=false;
        if (fs::exists(scriptdir))
        {
            fs::recursive_directory_iterator it(scriptdir);
            fs::recursive_directory_iterator endit;
            while (it != endit)
            {
                if (fs::is_regular_file(*it) && (it->path().extension().string() == ".lua") &&
                        (it->path().filename().string() != "helper.lua"))
                {
                    if( canExecuteScript(content, it->path()) )
                    {
                        found = true;
                        boost::thread t( boost::bind(&AgoLua::executeScript, this, content, it->path()) );
                        t.detach();
                    }
                }
                ++it;
            }
        }

        if(!found) {
            return responseError(RESPONSE_ERR_NOT_FOUND, "script not found");
        }

        return responseSuccess();
    }
}

/**
 * Agocontrol event handler
 */
void AgoLua::eventHandler(std::string subject, qpid::types::Variant::Map content)
{
    if ( subject == "event.device.announce" )
    {
        return;
    }
    else
    {
        //execute scripts
        content["subject"] = subject;
        commandHandler(content);
    }
}

void AgoLua::setupApp()
{
    agocontroller = agoConnection->getAgocontroller();

    //get config
    std::string optString = getConfigOption("filterByEvents", "1");
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
    addCommandHandler();
    addEventHandler();
}

AGOAPP_ENTRY_POINT(AgoLua);

