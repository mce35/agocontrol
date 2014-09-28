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
#include <boost/filesystem.hpp>
#include <boost/preprocessor/stringize.hpp>

#include <signal.h>
#include <sys/wait.h>

#include "agoclient.h"

//mongoose close idle connection after 30 seconds (timeout)
#define MONGOOSE_POLLING 1000 //in ms, mongoose polling time
#define GETEVENT_MAX_RETRIES 28 //mongoose poll every seconds (see MONGOOSE_POLLING)

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

class GetEventState
{
public:
	std::string subscriptionId;
	Json::Value rpcRequestId;
	time_t inited;
	time_t lastPoll;

	GetEventState(std::string subscriptionId_, const Json::Value rpcRequestId_)
		: subscriptionId(subscriptionId_)
		, rpcRequestId(rpcRequestId_)
		, lastPoll(0) {
			inited = time(NULL);
	}
};

map<string,Subscriber> subscriptions;
pthread_mutex_t mutexSubscriptions;

list<mg_server*> all_servers;
time_t last_event;


// helper to determine last element
#ifndef _LIBCPP_ITERATOR
template <typename Iter>
Iter next(Iter iter)
{
	return ++iter;
}
#endif

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

/**
 * Add JSON-RPC headers (jsonrpc, id) onto the response_root object,
 * serialize it to JSON, and write it to the mg_connnection
 *
 * Note: response_root is modified!
 */
static void mg_rpc_reply(struct mg_connection *conn, const Json::Value &request_or_id, Json::Value &response_root)
{
	response_root["jsonrpc"] = "2.0";

	Json::Value request_id;
	if(request_or_id.type() == Json::objectValue) {
		request_id = request_or_id.get("id", Json::Value());
	}else
		request_id = request_or_id;

	if(!request_id.empty())
		response_root["id"] = request_id;

	Json::FastWriter writer;
	std::string data(writer.write(response_root));

	mg_send_data(conn, data.c_str(), data.size());
}

/**
 * Write a RPC response with one element in the root with the name set in result_key (default "result").
 */
static void mg_rpc_reply_result(struct mg_connection *conn, const Json::Value &request_or_id, const Json::Value &result, std::string result_key="result") {
	Json::Value root(Json::objectValue);
	root[result_key] = result;
	mg_rpc_reply(conn, request_or_id, root);
}

/**
 * Write a RPC response with a "result" element whose value is a simple string.
 */
static void mg_rpc_reply_result(struct mg_connection *conn, const Json::Value &request_or_id, const std::string result) {
	Json::Value r(result);
	mg_rpc_reply_result(conn, request_or_id, r);
}

/**
 * Write a RCP response with an "error" element having the specified code/message.
 */
static void mg_rpc_reply_error(struct mg_connection *conn, const Json::Value &request_or_id, int code, const std::string message) {
	Json::Value error(Json::objectValue);
	error["code"] = code;
	error["message"] = message;

	mg_rpc_reply_result(conn, request_or_id, error, "error");
}

/**
 * Write a RPC response with a "result" element being a response map
 */
static void mg_rpc_reply_map(struct mg_connection *conn, const Json::Value &request_or_id, const Variant::Map &responseMap) {
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
}

static void command (struct mg_connection *conn) {
	char uuid[1024], command[1024], level[1024];
	Variant::Map agocommand;
	Variant::Map responseMap;

	if (mg_get_var(conn, "uuid", uuid, sizeof(uuid)) > 0) agocommand["uuid"] = uuid;
	if (mg_get_var(conn, "command", command, sizeof(command)) > 0) agocommand["command"] = command;
	if (mg_get_var(conn, "level", level, sizeof(level)) > 0) agocommand["level"] = level;

	//send command and write ajax response
	mg_send_header(conn, "Content-Type", "application/json");
	responseMap = agoConnection->sendMessageReply("", agocommand);
	if( responseMap.size()>0 )
	{
		mg_printmap(conn, responseMap);
	}
}

/**
 * Handle getEvent polling request
 * Return true if an event wasn't found to say to mongoose request is not over (return MG_MORE).
 * Return false if an event is found to say to mongoose request is over (return MG_TRUE) then app.js
 * will request again getEvent().
 *
 * Connection parameters are stored directly into mg_connection object inside connection_param variable.
 * Thanks to this variable its possible to call many times getEventRequest.
 */
bool getEventRequest(struct mg_connection *conn, GetEventState* state)
{
	int result = true;
	Variant::Map event;
	bool eventFound = false;

	//look for available event
	pthread_mutex_lock(&mutexSubscriptions);
	map<string,Subscriber>::iterator it = subscriptions.find(state->subscriptionId);

	if(it == subscriptions.end()) {
		pthread_mutex_unlock(&mutexSubscriptions);
		mg_rpc_reply_error(conn, state->rpcRequestId, -32603, "Invalid params: no current subscription for uuid");
		return false;
	}

	if(it->second.queue.size() >= 1 )
	{
		//event found
		event = it->second.queue.front();
		it->second.queue.pop_front();
		pthread_mutex_unlock(&mutexSubscriptions);

		std::string myId(state->rpcRequestId.toStyledString());
		mg_printf_data(conn, "{\"jsonrpc\": \"2.0\", \"result\": ");
		mg_printmap(conn, event);
		mg_printf_data(conn, ", \"id\": %s}", myId.c_str());

		eventFound = true;
		result = false;
	}
	else {
		pthread_mutex_unlock(&mutexSubscriptions);
	}

	return result;
}

bool jsonrpcRequestHandler(struct mg_connection *conn, Json::Value request) {
	Json::StyledWriter writer;
	string myId;
	const Json::Value id = request.get("id", Json::Value());
	const string method = request.get("method", "message").asString();
	const string version = request.get("jsonrpc", "unspec").asString();

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
					//cout << "Response: " << responseMap << endl;
					mg_rpc_reply_map(conn, request, responseMap);
				}
				else
				{
					//no response
					printf("WARNING, no reply message to fetch\n");
					mg_rpc_reply_result(conn, request, std::string("no-reply"));
				}

			} else {
				mg_rpc_reply_error(conn, request, -32602, "Invalid params");
			}

		} else if (method == "subscribe") {
			string subscriberName = generateUuid();
			if (id.isNull()) {
				// JSON-RPC notification is invalid here as we need to return the subscription UUID somehow..
				mg_rpc_reply_error(conn, request, -32600, "Invalid Request");
			} else if (subscriberName != "") {
				deque<Variant::Map> empty;
				Subscriber subscriber;
				subscriber.lastAccess=time(0);
				subscriber.queue = empty;
				pthread_mutex_lock(&mutexSubscriptions);	
				subscriptions[subscriberName] = subscriber;
				pthread_mutex_unlock(&mutexSubscriptions);	
				mg_rpc_reply_result(conn, request, subscriberName);
			} else {
				// uuid is empty so malloc probably failed, we seem to be out of memory
				mg_rpc_reply_error(conn, request, -32000, "Out of memory");
			}

		} else if (method == "unsubscribe") {
			if (params.isObject()) {
				Json::Value content = params["uuid"];
				if (content.isString()) {
					cout << "removing subscription: " << content.asString() << endl;
					pthread_mutex_lock(&mutexSubscriptions);	
					map<string,Subscriber>::iterator it = subscriptions.find(content.asString());
					if (it != subscriptions.end()) {
						subscriptions.erase(content.asString());
					}
					pthread_mutex_unlock(&mutexSubscriptions);
					mg_rpc_reply_result(conn, request, std::string("success"));
				} else {
					mg_rpc_reply_error(conn, request, -32602, "Invalid params: need uuid parameter");
				}
			} else {
				mg_rpc_reply_error(conn, request, -32602, "Invalid params: need uuid parameter");
			}
		} else if (method == "getevent") {
			if (params.isObject()) {
				Json::Value content = params["uuid"];
				if (content.isString()) {
					//add connection param (used to get connection param when polling see MG_POLL)
					GetEventState *state = new GetEventState(content.asString(), id);

					conn->connection_param = state;
					return getEventRequest(conn, state);
				}
				else
				{
					mg_rpc_reply_error(conn, request, -32602, "Invalid params: need uuid parameter");
				}
			} else {
				mg_rpc_reply_error(conn, request, -32602, "Invalid params: need uuid parameter");
			}
		} else {
			mg_rpc_reply_error(conn, request, -32601, "Method not found");
		}
	} else {
		mg_rpc_reply_error(conn, request, -32600, "Invalid Request");
	}

	return false;
}

static bool jsonrpc(struct mg_connection *conn)
{
	Json::Value root;
	Json::Reader reader;
	bool result = false;

	//add header
	mg_send_header(conn, "Content-Type", "application/json");

	if ( reader.parse(conn->content, conn->content + conn->content_len, root, false) )
	{
		if (root.isArray())
		{
			mg_printf_data(conn, "[");
			for (unsigned int i = 0; i< root.size(); i++)
			{
				result = jsonrpcRequestHandler(conn, root[i]);
			}
			mg_printf_data(conn, "]");
		}
		else
		{
			result = jsonrpcRequestHandler(conn, root);
		}
	}
	else
	{
		mg_rpc_reply_error(conn, Json::Value(), -32700, "Parse error");
	}

	return result;
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
				//write file first
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

				//then send command
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
					//uploadError = "File rejected: no handler available";
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
	mg_send_header(conn, "Content-Type", "application/json");
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
		mg_send_header(conn, "Content-Type", "application/json");
		mg_printf_data(conn, "{\"jsonrpc\": \"2.0\"");
		mg_printf_data(conn, ", \"error\": \"%s\"}", downloadError.c_str());
		return false;
	}
}

/**
 * Mongoose event handler
 */
static int event_handler(struct mg_connection *conn, enum mg_event event)
{
	int result = MG_FALSE;

	if (event == MG_REQUEST)
	{
		if (strcmp(conn->uri, "/command") == 0)
		{
			command(conn);
			result = MG_TRUE;
		}
		else if (strcmp(conn->uri, "/jsonrpc") == 0)
		{
			if( jsonrpc(conn) )
				result = MG_MORE;
			else
				result = MG_TRUE;
		}
		else if( strcmp(conn->uri, "/upload") == 0)
		{
			uploadFiles(conn);
			result = MG_TRUE;
		}
		else if( strcmp(conn->uri, "/download") == 0)
		{
			if( downloadFile(conn) )
				result = MG_MORE;
			else
				result = MG_TRUE;
		}
		else
		{
			// No suitable handler found, mark as not processed. Mongoose will
			// try to serve the request.
			result = MG_FALSE;
		}
	}
	else if( event==MG_POLL )
	{
		if( conn->connection_param )
		{
			GetEventState* state = (GetEventState*)conn->connection_param;
			if (last_event >= state->lastPoll)
			{
				if( !getEventRequest(conn, state) )
				{
					//event found. Stop polling request
					result = MG_TRUE;
				}
			}
			state->lastPoll = time(NULL);

			if(result == MG_FALSE) {
				if((state->lastPoll - state->inited) > GETEVENT_MAX_RETRIES) {
					// close connection now (before mongoose close connection)
					mg_rpc_reply_error(conn, state->rpcRequestId, -32602, "no messages for subscription");
					result = MG_TRUE;
				}
			}
		}
	}
	else if( event==MG_CLOSE )
	{
		if( conn->connection_param )
		{
			GetEventState* state = (GetEventState*)conn->connection_param;
			delete state;
			conn->connection_param = NULL;
		}
	}
	else if (event == MG_AUTH)
	{
		if( authFile!=NULL )
		{
			//check auth
			result = mg_authorize_digest(conn, authFile);
		}
		else
		{
			//no auth
			result = MG_TRUE;
		}
	}

	return result;
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
		replaceString(quantity, "event.environment.", "");
		replaceString(quantity, "changed", "");
		content["quantity"] = quantity;
	}
	else if( subject=="event.device.batterylevelchanged" )
	{
		string quantity = subject;
		replaceString(quantity, "event.device.", "");
		replaceString(quantity, "changed", "");
		content["quantity"] = quantity;
	}

	pthread_mutex_lock(&mutexSubscriptions);
	for (map<string,Subscriber>::iterator it = subscriptions.begin(); it != subscriptions.end(); ) {
		if (it->second.queue.size() > 100) {
			// this subscription seems to be abandoned, let's remove it to save resources
			printf("removing subscription %s as the queue size exceeds limits\n", it->first.c_str());
			subscriptions.erase(it++);
		} else {
			it->second.queue.push_back(content);
			++it;
		}
	}
	pthread_mutex_unlock(&mutexSubscriptions);

	// Wake servers to let pending getEvent trigger
	last_event = time(NULL);
	for(list<mg_server*>::iterator i = all_servers.begin(); i != all_servers.end(); i++) {
		mg_wakeup_server(*i);
	}
}

/**
 * Webserver process (threaded)
 */
static void *serve_webserver(void *server)
{
	for (;;)
	{
		mg_poll_server((struct mg_server *) server, MONGOOSE_POLLING);
	}
	return NULL;
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

int main(int argc, char **argv) {
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
	port=getConfigOption("rpc", "ports", "8008,8009s");
	htdocs=getConfigOption("rpc", "htdocs", BOOST_PP_STRINGIZE(DEFAULT_HTMLDIR));
	certificate=getConfigOption("rpc", "certificate", getConfigPath("/rpc/rpc_cert.pem"));
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
	while( !ports.empty() )
	{
		//get port
		port = ports.front().asString();
		ports.pop_front();

		//SSL
		useSSL = port.find('s') != std::string::npos;

		//start webserver threads
		cout << "Starting webserver on port " << port;
		if( useSSL )
			cout << " using SSL";
		cout << endl;
		for( int i=0; i<maxthreads; i++ )
		{
			struct mg_server *server;
			sprintf(serverId, "%d", threadId);
			server = mg_create_server((void*)serverId, event_handler);
			mg_set_option(server, "document_root", htdocs.c_str());
			mg_set_option(server, "auth_domain", domainname.c_str());
			if( useSSL )
			{
				mg_set_option(server, "ssl_certificate", certificate.c_str());
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
			mg_start_thread(serve_webserver, server);
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

	//configure and run agoclient
	agoConnection->addEventHandler(ago_event_handler);
	agoConnection->run();

}
