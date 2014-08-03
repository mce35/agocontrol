/*
     Copyright (C) 2012 Harald Klein <hari@vt100.at>

     This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License.
     This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
     of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

     See the GNU General Public License for more details.

     this is a lightweight RPC/HTTP interface for ago control for platforms where the regular cherrypy based admin interface is too slow
*/

#include <iostream>
#include <ctime>
#include "mongoose.h"

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

#define BOOST_FILESYSTEM_VERSION 3
#define BOOST_FILESYSTEM_NO_DEPRECATED
#include "boost/filesystem.hpp"

#include "agoclient.h"

//mongoose close idle connection after 30 seconds (timeout)
#define GETEVENT_MAX_RETRIES 140 //0.2s*140=28 seconds

//upload file path
#define UPLOAD_PATH "/tmp/"

//default auth file
#define HTPASSWD ".htpasswd"

namespace fs = ::boost::filesystem;
using namespace std;
using namespace qpid::messaging;
using namespace qpid::types;
using namespace agocontrol; 

//agoclient
AgoConnection *agoConnection;

//auth
FILE* authFile = NULL;

// struct and map for json-rpc event subscriptions
struct Subscriber
{
	deque<Variant::Map> queue;
	time_t lastAccess;
};

map<string,Subscriber> subscriptions;
pthread_mutex_t mutexSubscriptions;

// helper to determine last element
template <typename Iter>
Iter next(Iter iter)
{
    return ++iter;
}

// json-print qpid Variant Map and List via mongoose
void mg_printmap(struct mg_connection *conn, Variant::Map map);

void mg_printlist(struct mg_connection *conn, Variant::List list) {
	mg_printf_data(conn, "[");
	for (Variant::List::const_iterator it = list.begin(); it != list.end(); ++it) {
		switch(it->getType()) {
			case VAR_MAP:
				mg_printmap(conn, it->asMap());
				break;
			case VAR_STRING:
				mg_printf_data(conn, "\"%s\"", it->asString().c_str());	
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
void mg_printmap(struct mg_connection *conn, Variant::Map map) {
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

static void command (struct mg_connection *conn) {
    char uuid[1024], command[1024], level[1024];
    Variant::Map agocommand;
    Variant::Map responseMap;

    if (mg_get_var(conn, "uuid", uuid, sizeof(uuid)) > 0) agocommand["uuid"] = uuid;
    if (mg_get_var(conn, "command", command, sizeof(command)) > 0) agocommand["command"] = command;
    if (mg_get_var(conn, "level", level, sizeof(level)) > 0) agocommand["level"] = level;

    //send command and write ajax response
    mg_printf_data(conn, "%s", "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n");
    responseMap = agoConnection->sendMessageReply("", agocommand);   
    if( responseMap.size()>0 )
    {
        mg_printmap(conn, responseMap);
    }
}

bool jsonrpcRequestHandler(struct mg_connection *conn, Json::Value request, bool firstElem) {
	Json::StyledWriter writer;
	string myId;
	const Json::Value id = request.get("id", Json::Value());
	const string method = request.get("method", "message").asString();
	const string version = request.get("jsonrpc", "unspec").asString();
	bool result;

	myId = writer.write(id);
	if (version == "2.0") {
		const Json::Value params = request.get("params", Json::Value());
		if (method == "message" ) {
			if (params.isObject())
            {
                //prepare message
                Json::Value content = params["content"];
                Json::Value subject = params["subject"];
                Variant::Map command = jsonToVariantMap(content);

                //send message and handle response
                Variant::Map responseMap = agoConnection->sendMessageReply(subject.asString().c_str(), command);
                if( responseMap.size()>0 && !id.isNull() ) // only send reply when id is not null
                {
                    cout << "Response: " << responseMap << endl;
                    mg_printf_data(conn, "{\"jsonrpc\": \"2.0\", \"result\": ");
                    mg_printmap(conn, responseMap);
                    mg_printf_data(conn, ", \"id\": %s}",myId.c_str());
                }
                else
                {
                    //no response
                    printf("WARNING, no reply message to fetch\n");
                    mg_printf_data(conn, "{\"jsonrpc\": \"2.0\", \"result\": \"no-reply\", \"id\": %s}",myId.c_str());
                }

            } else {
                mg_printf_data(conn, "{\"jsonrpc\": \"2.0\", \"error\": {\"code\":-32602,\"message\":\"Invalid params\"}, \"id\": %s}",myId.c_str());
            }
		
		} else if (method == "subscribe") {
			string subscriberName = generateUuid();
			if (id.isNull()) {
				// JSON-RPC notification is invalid here as we need to return the subscription UUID somehow..
				mg_printf_data(conn, "{\"jsonrpc\": \"2.0\", \"error\": {\"code\":-32600,\"message\":\"Invalid Request\"}, \"id\": %s}",myId.c_str());
			} else if (subscriberName != "") {
				deque<Variant::Map> empty;
				Subscriber subscriber;
				subscriber.lastAccess=time(0);
				subscriber.queue = empty;
				pthread_mutex_lock(&mutexSubscriptions);	
				subscriptions[subscriberName] = subscriber;
				pthread_mutex_unlock(&mutexSubscriptions);	
				mg_printf_data(conn, "{\"jsonrpc\": \"2.0\", \"result\": \"%s\", \"id\": %s}",subscriberName.c_str(), myId.c_str());
			} else {
				// uuid is empty so malloc probably failed, we seem to be out of memory
				mg_printf_data(conn, "{\"jsonrpc\": \"2.0\", \"error\": {\"code\":-32000,\"message\":\"Out of memory\"}, \"id\": %s}",myId.c_str());
			}

		} else if (method == "unsubscribe") {
			if (params.isObject()) {
				Json::Value content = params["uuid"];
				if (content.isString()) {
					cout << "removing subscription: " << content.asString() << endl;
					pthread_mutex_lock(&mutexSubscriptions);	
					map<string,Subscriber>::iterator it = subscriptions.find(content.asString());
					if (it != subscriptions.end()) {
						Subscriber *sub = &(it->second);
						subscriptions.erase(content.asString());
					}
					pthread_mutex_unlock(&mutexSubscriptions);	
					mg_printf_data(conn, "{\"jsonrpc\": \"2.0\", \"result\": \"success\", \"id\": %s}",myId.c_str());
				} else {
					mg_printf_data(conn, "{\"jsonrpc\": \"2.0\", \"error\": {\"code\":-32602,\"message\":\"Invalid params: need uuid parameter\"}, \"id\": %s}",myId.c_str());
				}
			} else {
				mg_printf_data(conn, "{\"jsonrpc\": \"2.0\", \"error\": {\"code\":-32602,\"message\":\"Invalid params: need uuid parameter\"}, \"id\": %s}",myId.c_str());
			}
		} else if (method == "getevent") {
			if (params.isObject()) {
				Json::Value content = params["uuid"];
				if (content.isString()) {
					Variant::Map event;
					pthread_mutex_lock(&mutexSubscriptions);	
					map<string,Subscriber>::iterator it = subscriptions.find(content.asString());
                    int retries = 0;
					while( (it!=subscriptions.end()) && (it->second.queue.size()<1) && (retries<GETEVENT_MAX_RETRIES) ) {
						pthread_mutex_unlock(&mutexSubscriptions);	
						usleep(200000);
						pthread_mutex_lock(&mutexSubscriptions);	
						// we need to search again, subscription might have been deleted during lock release
						it = subscriptions.find(content.asString());
                        retries++;
                    }
                    if( it!=subscriptions.end() && it->second.queue.size()>=1 ) {
    					event = it->second.queue.front();
	    				it->second.queue.pop_front();
		    			pthread_mutex_unlock(&mutexSubscriptions);	
			    		mg_printf_data(conn, "{\"jsonrpc\": \"2.0\", \"result\": ");
				    	mg_printmap(conn, event);
           			    mg_printf_data(conn, ", \"id\": %s}",myId.c_str());
		    		} else {
			    		pthread_mutex_unlock(&mutexSubscriptions);	
				    	mg_printf_data(conn, "{\"jsonrpc\": \"2.0\", \"retries\": %d, \"error\": {\"code\":-32602,\"message\":\"Invalid params: no current subscription for uuid\"}, \"id\": %s}", retries,  myId.c_str());
				    }  
				} else {
					mg_printf_data(conn, "{\"jsonrpc\": \"2.0\", \"error\": {\"code\":-32602,\"message\":\"Invalid params: need uuid parameter\"}, \"id\": %s}",myId.c_str());
				}
			} else {
				mg_printf_data(conn, "{\"jsonrpc\": \"2.0\", \"error\": {\"code\":-32602,\"message\":\"Invalid params: need uuid parameter\"}, \"id\": %s}",myId.c_str());
			}

		} else {
			mg_printf_data(conn, "{\"jsonrpc\": \"2.0\", \"error\": {\"code\":-32601,\"message\":\"Method not found\"}, \"id\": %s}",myId.c_str());
		}
	} else {
		mg_printf_data(conn, "{\"jsonrpc\": \"2.0\", \"error\": {\"code\":-32600,\"message\":\"Invalid Request\"}, \"id\": %s}",myId.c_str());
	}
	return result;
}

static void jsonrpc (struct mg_connection *conn) {
	Json::Value root;
	Json::Reader reader;

	if ( reader.parse(conn->content, conn->content + conn->content_len, root, false) ) {
		if (root.isArray()) {
			bool firstElem = true;
			mg_printf_data(conn, "[");
			for (unsigned int i = 0; i< root.size(); i++) {
				bool result = jsonrpcRequestHandler(conn, root[i], firstElem);
				if (result) firstElem = false; 
			}
			mg_printf_data(conn, "]");
		} else {
			jsonrpcRequestHandler(conn, root, true);
		}
	} else {
		mg_printf_data(conn, "%s", "{\"jsonrpc\": \"2.0\", \"error\": {\"code\":-32700,\"message\":\"Parse error\"}, \"id\": null}");
	}
}

/**
 * Upload files
 * @info source from https://github.com/cesanta/mongoose/blob/master/examples/upload.c
 */
static void uploadFiles(struct mg_connection *conn)
{
    //init
    const char *data;
    int data_len, ofs = 0;
    char var_name[100], file_name[100];
    string path = "";
    Variant::Map content;
    Variant::List files;
    char posted_var[1024] = "";
    std::string uuid = "";
    string uploadError = "";

    //upload files
    while ((ofs = mg_parse_multipart(conn->content + ofs, conn->content_len - ofs, var_name, sizeof(var_name),
            file_name, sizeof(file_name), &data, &data_len)) > 0) {

        if( strlen(file_name)>0 )
        {
            //it's a file
            
            //check if uuid found
            if( uuid.size()==0 )
            {
                //no uuid found yet, drop file
                continue;
            }
                
            //save file to upload path
            path = std::string(UPLOAD_PATH) + std::string(file_name);
            fs::path filepath(path);
            FILE* fp = fopen(path.c_str(), "wb");
            if( fp )
            {
                //send command
                content.clear();
                content["uuid"] = std::string(uuid);
                content["command"] = "uploadfile";
                content["filepath"] = path;
                content["filename"] = std::string(file_name);
                content["filesize"] = data_len;

                Variant::Map responseMap = agoConnection->sendMessageReply("", content);
                if( responseMap.size()==0 )
                {
                    //command failed, drop file
                    fs::remove(filepath);
                    cout << "Uploaded file \"" << file_name << "\" dropped because command failed" << endl;
                    uploadError = "Internal error";
                    continue;
                }

                if( !responseMap["result"].isVoid() && responseMap["result"].asInt16()!=0 )
                {
                    //file rejected, drop it
                    fs::remove(filepath);
                    if( !responseMap["error"].isVoid() )
                    {
                        uploadError = responseMap["error"].asString();
                    }
                    else
                    {
                        uploadError = "File rejected";
                    }
                    cerr << "Uploaded file \"" << file_name << "\" rejected by system: " << uploadError << endl;
                    uploadError = "File rejected: no handler available";
                    continue;
                }

                //write file
                cout << "Upload file to \"" << path << "\"" << endl;
                int written = fwrite(data, sizeof(char), data_len, fp);
                fclose(fp);
                if( written!=data_len )
                {
                    //error writting file, drop it
                    fs::remove(filepath);
                    cerr << "Uploaded file \"" << path << "\" not fully written (no space left?)" << endl;
                    uploadError = "Unable to write file (no space left?)";
                    continue;
                }

                //add file to output
                Variant::Map file;
                file["name"] = string(file_name);
                file["size"] = data_len;
                files.push_back(file);
    
                //delete file (it should be processed by sendcommand)
                //TODO: maybe a purge process could be interesting to implement
                fs::remove(filepath);
            }
        }
        else
        {
            //it's a posted value
            if( strcmp(var_name, "uuid")==0 )
            {
                strncpy(posted_var, data, data_len);
                uuid = string(posted_var);
            }
        }
    }

    //prepare request answer
    mg_printf_data(conn, "{\"jsonrpc\": \"2.0\", \"result\": {\"files\": ");
    mg_printlist(conn, files);
    mg_printf_data(conn, ", \"count\": %d", files.size());
    mg_printf_data(conn, ", \"error\": \"%s\" } }", uploadError.c_str());
}

/**
 * Download file
 * @info https://github.com/cesanta/mongoose/blob/master/examples/file.c
 */
static bool downloadFile(struct mg_connection *conn)
{
    //init
    char param[1024];
    Variant::Map content;
    Variant::Map responseMap;
    string downloadError = "";
    string filepath = "";

    //get params
	if( mg_get_var(conn, "filename", param, sizeof(param))>0 )
        content["filename"] = string(param);
	if( mg_get_var(conn, "uuid", param, sizeof(param))>0 )
        content["uuid"] = string(param);
    content["command"] = "downloadfile";

    if( !content["filename"].isVoid() && !content["uuid"].isVoid() )
    {
        //send command
        responseMap = agoConnection->sendMessageReply("", content);
        if( responseMap.size()>0 )
        {
            //command sent successfully
            if( !responseMap["filepath"].isVoid() && responseMap["filepath"].asString().length()>0 )
            {
                //all seems valid
                filepath = responseMap["filepath"].asString();
                cout << "Downloading file \"" << filepath << "\"" << endl;
            }
            else
            {
                //invalid command response
                cerr << "Download file, sendCommand returned invalid response (need filepath)" << endl;
                downloadError = "Internal error";
            }
        }
        else
        {
            //command failed
            cerr << "Download file, sendCommand failed, unable to send file" << endl;
            downloadError = "Internal error";
        }
    }
    else
    {
        //missing parameters!
        cerr << "Download file, missing parameters. Nothing done" << endl;
        downloadError = "Invalid request parameters";
    }

    if( downloadError.length()==0 )
    {
        mg_send_file(conn, filepath.c_str());
        return true;
    }
    else
    {
        mg_printf_data(conn, "{\"jsonrpc\": \"2.0\"");
        mg_printf_data(conn, ", \"error\": \"%s\"}", downloadError.c_str());
        return false;
    }
}

/**
 * Mongoose event handler
 */
static int event_handler(struct mg_connection *conn, enum mg_event event) {
    if (event == MG_REQUEST) {
        if (strcmp(conn->uri, "/command") == 0) {
            command(conn);
            return MG_TRUE;
        } else if (strcmp(conn->uri, "/jsonrpc") == 0) {
            jsonrpc(conn);
            return MG_TRUE;
        } else if( strcmp(conn->uri, "/upload") == 0) {
            uploadFiles(conn);
            return MG_TRUE;
        } else if( strcmp(conn->uri, "/download") == 0) {
            if( downloadFile(conn) )
                return MG_MORE;
            else
                return MG_TRUE;
        } else {
            // No suitable handler found, mark as not processed. Mongoose will
            // try to serve the request.
            return MG_FALSE;
        }
    } else if (event == MG_AUTH) {
        if( authFile!=NULL )
        {
            //check auth
            return mg_authorize_digest(conn, authFile);
        }
        else
        {
            //no auth
            return MG_TRUE;
        }
    } else {
        return MG_FALSE;
    }

    return MG_FALSE;
}

/**
 * Agoclient event handler
 */
void ago_event_handler(std::string subject, qpid::types::Variant::Map content)
{
    // don't flood clients with unneeded events
    if( subject=="event.environment.timechanged")
        return;

    //remove empty command from content
    Variant::Map::iterator it = content.find("command");
    if( it!=content.end() && content["command"].isVoid() )
    {
        content.erase("command");
    }

    //prepare event content
    content["event"] = subject;
    if( subject.find("event.environment.")!=std::string::npos && subject.find("changed")!= std::string::npos )
    {
        string quantity = subject;
        quantity.erase(quantity.begin(),quantity.begin()+18);
        quantity.erase(quantity.end()-7,quantity.end());
        content["quantity"] = quantity;
    }

    pthread_mutex_lock(&mutexSubscriptions);
    for (map<string,Subscriber>::iterator it = subscriptions.begin(); it != subscriptions.end(); ) {
        if (it->second.queue.size() > 100) {
            // this subscription seems to be abandoned, let's remove it to save resources
            printf("removing subscription %s as the queue size exceeds limits\n", it->first.c_str());
            Subscriber *sub = &(it->second);
            subscriptions.erase(it++);
        } else {
            it->second.queue.push_back(content);
            ++it;
        }
    }
    pthread_mutex_unlock(&mutexSubscriptions);	
}

/**
 * Webserver process (threaded)
 */
static void *serve_webserver(void *server)
{
    for (;;)
    {
        mg_poll_server((struct mg_server *) server, 1000);
    }
    return NULL;
}

int main(int argc, char **argv) {
    string broker;
    Variant::List ports;
    string port;
    string split;
    string htdocs;
    string certificate;
    string numthreads;
    string domainname;
    bool useSSL;
    struct mg_server *firstServer = NULL;
    char serverId[3] = "";
    int maxthreads;
    int threadId = 0;

    //create agoclient
    agoConnection = new AgoConnection("rpc");

    //get parameters
    Variant::Map connectionOptions;
    broker=getConfigOption("system", "broker", "localhost:5672");
    connectionOptions["username"]=getConfigOption("system", "username", "agocontrol");
    connectionOptions["password"]=getConfigOption("system", "password", "letmein");
    port=getConfigOption("rpc", "ports", "8008,8009s");
    htdocs=getConfigOption("rpc", "htdocs", HTMLDIR);
    certificate=getConfigOption("rpc", "certificate", CONFDIR "/rpc/rpc_cert.pem");
    numthreads=getConfigOption("rpc", "numthreads", "30");
    domainname=getConfigOption("rpc", "domainname", "agocontrol");

    //ports
    while( port.find(',')!=std::string::npos )
    {
        std::size_t pos = port.find(',');
        split = port.substr(0, pos);
        port = port.substr(pos+1);
        ports.push_back(split);
    }
    ports.push_back(port);

    //auth
    stringstream auth;
    auth << htdocs << "/" << HTPASSWD;
    fs::path authPath(auth.str());
    if( fs::exists(authPath) )
    {
        //activate auth
        authFile = fopen(auth.str().c_str(), "r");
        if( authFile==NULL )
        {
            //unable to parse auth file
            cout << "Auth support: error parsing \"" << auth << "\" file. Auth deactivated" << endl;
        }
        else
        {
            cout << "Auth support: yes" << endl;
        }
    }
    else
    {
        cout << "Auth support: no" << endl;
    }

    pthread_mutex_init(&mutexSubscriptions, NULL);

    // start webservers
    sscanf(numthreads.c_str(), "%d", &maxthreads);
    int numThreadsPerPort = ceil((int)maxthreads/(int)ports.size());
    while( !ports.empty() )
    {
        //get port
        port = ports.front().asString();
        ports.pop_front();

        //SSL
        useSSL = port.find('s') != std::string::npos;
        
        //start webserver threads
        cout << "Starting webserver (" << numThreadsPerPort << " threads) on port " << port;
        if( useSSL )
            cout << " using SSL";
        cout << endl;
        for( int i=0; i<numThreadsPerPort; i++ )
        {
            struct mg_server *server;
            sprintf(serverId, "%d", threadId);
            server = mg_create_server((void*)serverId, event_handler);
            mg_set_option(server, "listening_port", port.c_str()); 
            mg_set_option(server, "document_root", htdocs.c_str()); 
            mg_set_option(server, "auth_domain", domainname.c_str()); 
            if( useSSL )
            {
                mg_set_option(server, "ssl_certificate", certificate.c_str()); 
            }
            if( firstServer==NULL )
            {
                firstServer = server;
            }
            else
            {
                mg_set_listening_socket(server, mg_get_listening_socket(firstServer));
            }
            mg_start_thread(serve_webserver, server);
        }

        //reset flags
        firstServer = NULL;
        useSSL = false;
        port = "";
        threadId++;
    }

    //configure and run agoclient
    agoConnection->addEventHandler(ago_event_handler);
    agoConnection->run();

}
