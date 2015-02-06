/*
   Copyright (C) 2012 Harald Klein <hari@vt100.at>

   This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

   See the GNU General Public License for more details.

   */

#include <iostream>
#include <ctime>
#include "../../core/rpc/mongoose.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <math.h>

#include <termios.h>
#ifndef __FreeBSD__
#include <malloc.h>
#endif
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

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

#include <jsoncpp/json/value.h>
#include <jsoncpp/json/reader.h>
#include <jsoncpp/json/writer.h>

#include <boost/preprocessor/stringize.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/locks.hpp>

#include <signal.h>
#include <sys/wait.h>

#include "agoapp.h"

//mongoose close idle connection after 30 seconds (timeout)
#define MONGOOSE_POLLING 1000 //in ms, mongoose polling time
#define GETEVENT_DEFAULT_TIMEOUT 28 // mongoose poll every seconds (see MONGOOSE_POLLING)

namespace fs = ::boost::filesystem;

using namespace std;
using namespace qpid::messaging;
using namespace qpid::types;
using namespace agocontrol;

// helper to determine last element
#ifndef _LIBCPP_ITERATOR
template <typename Iter>
Iter next(Iter iter)
{
    return ++iter;
}
#endif


class AgoImperiHome: public AgoApp {
private:
    list<mg_server*> all_servers;
    list<boost::thread*> server_threads;
    time_t last_event;

    bool jsonrpc(struct mg_connection *conn);

    void eventHandler(std::string subject, qpid::types::Variant::Map content) ;

    void serve_webserver(void *server);

    void setupApp();
    void cleanupApp();
    std::string convertDeviceType(std::string devicetype);

public:
    AGOAPP_CONSTRUCTOR(AgoImperiHome)

    // Called from public mg_event_handler_wrapper
    int mg_event_handler(struct mg_connection *conn, enum mg_event event);
};

static int mg_event_handler_wrapper(struct mg_connection *conn, enum mg_event event) {
    AgoImperiHome* inst = (AgoImperiHome*)conn->server_param;
    return inst->mg_event_handler(conn, event);
}


// json-print qpid Variant Map and List via mongoose
static void mg_printmap(struct mg_connection *conn, Variant::Map map);

static void mg_printlist(struct mg_connection *conn, Variant::List list) {
    mg_printf_data(conn, "[");
    for (Variant::List::const_iterator it = list.begin(); it != list.end(); ++it) {
        switch(it->getType()) {
            case VAR_MAP:
                mg_printmap(conn, it->asMap());
                break;
            case VAR_STRING:
                mg_printf_data(conn, "\"%s\"", it->asString().c_str());
                break;
            case VAR_BOOL:
                if( it->asBool() )
                    mg_printf_data(conn, "1");
                else
                    mg_printf_data(conn, "0");
                break;
            default:
                if (it->asString().size() != 0) {
                    mg_printf_data(conn, "%s", it->asString().c_str());
                } else {
                    mg_printf_data(conn, "null");
                }
        }
        if ((it != list.end()) && (next(it) != list.end())) mg_printf_data(conn, ",");
    }
    mg_printf_data(conn, "]");
}

static void mg_printmap(struct mg_connection *conn, Variant::Map map) {
    mg_printf_data(conn, "{");
    for (Variant::Map::const_iterator it = map.begin(); it != map.end(); ++it) {
        mg_printf_data(conn, "\"%s\":", it->first.c_str());
        switch (it->second.getType()) {
            case VAR_MAP:
                mg_printmap(conn, it->second.asMap());
                break;
            case VAR_LIST:
                mg_printlist(conn, it->second.asList());
                break;
            case VAR_STRING:
                mg_printf_data(conn, "\"%s\"", it->second.asString().c_str());
                break;
            case VAR_BOOL:
                if( it->second.asBool() )
                    mg_printf_data(conn, "1");
                else
                    mg_printf_data(conn, "0");
                break;
            default:
                if (it->second.asString().size() != 0) {
                    mg_printf_data(conn, "%s", it->second.asString().c_str());
                } else {
                    mg_printf_data(conn, "null");
                }
        }
        if ((it != map.end()) && (next(it) != map.end())) mg_printf_data(conn, ",");
    }
    mg_printf_data(conn, "}");
}

static bool mg_rpc_reply_map(struct mg_connection *conn, const Json::Value &request_or_id, const Variant::Map &responseMap) {
    //	Json::Value r(result);
    //	mg_rpc_reply_result(conn, request_or_id, r);
    Json::Value request_id;
    if(request_or_id.type() == Json::objectValue) {
        request_id = request_or_id.get("id", Json::Value());
    }else
        request_id = request_or_id;

    Json::FastWriter writer;
    std::string request_idstr = writer.write(request_id);

    // XXX: Write without building full JSON
    mg_printf_data(conn, "{\"jsonrpc\": \"2.0\", \"result\": ");
    mg_printmap(conn, responseMap);
    mg_printf_data(conn, ", \"id\": %s}", request_idstr.c_str());
    return false;
}

std::string AgoImperiHome::convertDeviceType(std::string devicetype) {
	std::string result;
	qpid::types::Variant::Map convertMap;
	convertMap["camera"] = "DevCamera";
	convertMap["co2sensor"] = "DevCO2";
	convertMap["dimmer"] = "DevDimmer";
	convertMap["multilevelsensor"] = "DevGenericSensor";
	convertMap["luminance"] = "DevLuminosity";
	convertMap["drapes"] = "DevShutter";
	convertMap["smokedetector"] = "DevSmoke";
	convertMap["switch"] = "DevSwitch";
	convertMap["temperaturesensor"] = "DevTemperature";
	convertMap["thermostat"] = "DevThermostat";
	result = convertMap[devicetype].asString();
	return result;
}

/**
 * Mongoose event handler
 */
int AgoImperiHome::mg_event_handler(struct mg_connection *conn, enum mg_event event)
{
    int result = MG_FALSE;

    if (event == MG_REQUEST)
    {
	std::string uri = conn->uri;
	AGO_TRACE() << "Trying to handle MG_REQUEST with URI: " << uri;
	if (strcmp(conn->uri, "/") == 0) {

		mg_send_header(conn, "Content-Type", "text/html");
		mg_printf(conn, "<HTML><BODY>ImperiHome Gateway</BODY></HTML>");
		result = MG_TRUE;
	} else if (uri=="/devices")
        {
		qpid::types::Variant::List devicelist;
		qpid::types::Variant::Map inventory = agoConnection->getInventory();
		if( inventory.size()>0 && !inventory["devices"].isVoid() ) {
			qpid::types::Variant::Map devices = inventory["devices"].asMap();
			for (qpid::types::Variant::Map::iterator it = devices.begin(); it != devices.end(); it++) {
				qpid::types::Variant::Map device = it->second.asMap();
				if (device["name"].asString() == "") continue;
				if (device["room"].asString() == "") continue;
				AGO_TRACE() << device;
				qpid::types::Variant::Map deviceinfo;
				deviceinfo["name"]=device["name"];
				deviceinfo["room"]=device["room"];
				deviceinfo["type"]=convertDeviceType(device["devicetype"]);
				deviceinfo["id"]=it->first;
				devicelist.push_back(deviceinfo);
			}
		}	
		mg_send_header(conn, "Content-Type", "application/json");
		qpid::types::Variant::Map finaldevices;
		finaldevices["devices"] = devicelist;
		mg_printmap(conn,finaldevices);	
		result = MG_TRUE;
        }
        else if( strcmp(conn->uri, "/system") == 0)
        {
		qpid::types::Variant::Map systeminfo;
		systeminfo["apiversion"] = 1;
		systeminfo["id"] = getConfigSectionOption("system", "uuid", "00000000-0000-0000-000000000000");
		mg_send_header(conn, "Content-Type", "application/json");
		mg_printmap(conn, systeminfo);	
		result = MG_TRUE;
        }
        else if( strcmp(conn->uri, "/rooms") == 0)
        {
		qpid::types::Variant::List roomlist;
		qpid::types::Variant::Map inventory = agoConnection->getInventory();
		if( inventory.size()>0 && !inventory["rooms"].isVoid() ) {
			qpid::types::Variant::Map rooms = inventory["rooms"].asMap();
			for (qpid::types::Variant::Map::iterator it = rooms.begin(); it != rooms.end(); it++) {
				qpid::types::Variant::Map room = it->second.asMap();
				AGO_TRACE() << room;
				qpid::types::Variant::Map roominfo;
				roominfo["name"]=room["name"];
				roominfo["id"]=it->first;
				roomlist.push_back(roominfo);
			}
		}	
		mg_send_header(conn, "Content-Type", "application/json");
		qpid::types::Variant::Map finalrooms;
		finalrooms["rooms"] = roomlist;
		mg_printmap(conn,finalrooms);	
		result = MG_TRUE;
	
	} else if ( uri.find ("/devices/",0) != std::string::npos && uri.find("/action/",0) != std::string::npos)  {
		std::vector<std::string> items = split(uri, '/');
		for (int i=0;i<items.size();i++) {
			AGO_TRACE() << "Item " << i << ": " << items[i];
		}
		if ( items.size()>=5 && items.size()<=6 ) {
			if (items[1] == "devices" && items[3]=="action") {
				qpid::types::Variant::Map command;
				/*
				DevDimmer,DevSwitch,: setstatus Param 0/1
				*/	
				command["command"]=items[4];
				command["uuid"]=items[2];
				agoConnection->sendMessage("", command);
				mg_send_header(conn, "Content-Type", "application/json");
				result = MG_TRUE;
			}
		}
	} else {
            // No suitable handler found, mark as not processed. Mongoose will
            // try to serve the request.
		AGO_ERROR() << "No handler for URI: " << conn->uri;
            result = MG_FALSE;
        }
    }
    else if( event==MG_POLL )
    {
        if( conn->connection_param )
        {
        }
    }
    else if( event==MG_CLOSE )
    {
        if( conn->connection_param )
        {
        }
    }
    else if (event == MG_AUTH)
    {
            result = MG_TRUE;
    }

    return result;
}

/**
 * Agoclient event handler
 */
void AgoImperiHome::eventHandler(std::string subject, qpid::types::Variant::Map content)
{
}

/**
 * Webserver process (threaded)
 */
void AgoImperiHome::serve_webserver(void *server)
{
    AGO_TRACE() << "Webserver thread started";
    while(!isExitSignaled())
    {
        mg_poll_server((struct mg_server *) server, MONGOOSE_POLLING);
    }
    AGO_TRACE() << "Webserver thread terminated";
}

/**
 * Mongoose signal handler
 * @info code from https://github.com/cesanta/mongoose/blob/master/examples/server.c#L101
 */
static void signal_handler(int sig_num) {
    // Reinstantiate signal handler
    signal(sig_num, signal_handler);

    // Do not do the trick with ignoring SIGCHLD, cause not all OSes (e.g. QNX)
    // reap zombies if SIGCHLD is ignored. On QNX, for example, waitpid()
    // fails if SIGCHLD is ignored, making system() non-functional.
    if (sig_num == SIGCHLD)
    {
        do {} while (waitpid(-1, &sig_num, WNOHANG) > 0);
    }
    else
    {
        //nothing to do
    }
}

void AgoImperiHome::setupApp() {
    Variant::List ports;
    string port;
    string split;
    fs::path htdocs;
    fs::path certificate;
    string numthreads;
    string domainname;
    bool useSSL;
    struct mg_server *firstServer = NULL;
    int maxthreads;
    int threadId = 0;

    //get parameters
    port = getConfigOption("ports", "8088");
    htdocs = getConfigOption("htdocs", fs::path(BOOST_PP_STRINGIZE(DEFAULT_HTMLDIR)));
    numthreads = getConfigOption("numthreads", "3");
    domainname = getConfigOption("domainname", "agocontrol");

    //ports
    while( port.find(',')!=std::string::npos )
    {
        std::size_t pos = port.find(',');
        split = port.substr(0, pos);
        port = port.substr(pos+1);
        ports.push_back(split);
    }
    ports.push_back(port);

    // start webservers
    sscanf(numthreads.c_str(), "%d", &maxthreads);
    while( !ports.empty() )
    {
        //get port
        port = ports.front().asString();
        ports.pop_front();

        //SSL
        useSSL = port.find('s') != std::string::npos;

        //start webserver threads
        if( useSSL )
            AGO_INFO() << "Starting webserver on port " << port << " using SSL";
        else
            AGO_INFO() << "Starting webserver on port " << port;

        for( int i=0; i<maxthreads; i++ )
        {
            struct mg_server *server;
            server = mg_create_server(this, mg_event_handler_wrapper);
            mg_set_option(server, "document_root", htdocs.string().c_str());
            mg_set_option(server, "auth_domain", domainname.c_str());
            if( useSSL )
            {
                mg_set_option(server, "ssl_certificate", certificate.string().c_str());
            }
            if( firstServer==NULL )
            {
                mg_set_option(server, "listening_port", port.c_str());
            }
            else
            {
                mg_set_listening_socket(server, mg_get_listening_socket(firstServer));
            }
            if( firstServer==NULL )
            {
                firstServer = server;
            }
            boost::thread *t = new boost::thread(boost::bind(&AgoImperiHome::serve_webserver, this, server));
            server_threads.push_back(t);
            all_servers.push_back(server);
        }

        //reset flags
        firstServer = NULL;
        useSSL = false;
        port = "";
        threadId++;
    }

    //avoid cgi zombies
    signal(SIGCHLD, signal_handler);

    addEventHandler();
}

void AgoImperiHome::cleanupApp() {
    // Signal wakes up one of the mg_poll_server calls,
    // the rest will exit. Calling mg_wakup would however block fully, since the first
    // thread does not respond anymore.. So just wait <1s for them
    AGO_TRACE() << "Waiting for webserver threads";
    for(list<boost::thread*>::iterator i = server_threads.begin(); i != server_threads.end(); i++) {
        (*i)->join();
    }
    AGO_TRACE() << "All webserver threads returned";
    for(list<boost::thread*>::iterator i = server_threads.begin(); i != server_threads.end(); i++) {
        delete *i;
    }
}

AGOAPP_ENTRY_POINT(AgoImperiHome);

