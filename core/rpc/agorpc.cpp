/*
     Copyright (C) 2012 Harald Klein <hari@vt100.at>

     This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License.
     This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
     of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

     See the GNU General Public License for more details.

     this is a lightweight RPC/HTTP interface for ago control for platforms where the regular cherrypy based admin interface is too slow
*/

#include <iostream>
#include "mongoose.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#include <termios.h>
#include <malloc.h>
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

#include <json/value.h>
#include <json/reader.h>
#include <json/writer.h>

#include "../../devices/agozwave/CDataFile.h"


using namespace std;
using namespace qpid::messaging;
using namespace qpid::types;

// qpid session and sender/receiver
Receiver receiver;
Sender sender;
Session session;

// context for embedded web server
struct mg_context       *ctx;


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

static const char *ajax_reply_start =
  "HTTP/1.1 200 OK\r\n"
  "Cache: no-cache\r\n"
  "Access-Control-Allow-Origin: *\r\n"
  "Content-Type: application/x-javascript\r\n"
  "\r\n";

// generates a uuid as string via libuuid
string generateUuid() {
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


// json-print qpid Variant Map and List via mongoose
void mg_printmap(struct mg_connection *conn, Variant::Map map);

void mg_printlist(struct mg_connection *conn, Variant::List list) {
	mg_printf(conn, "[");
	for (Variant::List::const_iterator it = list.begin(); it != list.end(); ++it) {
		switch(it->getType()) {
			case VAR_MAP:
				mg_printmap(conn, it->asMap());
				break;
			case VAR_STRING:
				mg_printf(conn, "\"%s\"", it->asString().c_str());	
				break;
			default:
				if (it->asString().size() != 0) {
					mg_printf(conn, "%s", it->asString().c_str());	
				} else {
					mg_printf(conn, "null");
				}
		}
		if ((it != list.end()) && (next(it) != list.end())) mg_printf(conn, ",");
	}
	mg_printf(conn, "]");
}
void mg_printmap(struct mg_connection *conn, Variant::Map map) {
	mg_printf(conn, "{");
	for (Variant::Map::const_iterator it = map.begin(); it != map.end(); ++it) {
		mg_printf(conn, "\"%s\":", it->first.c_str());
		switch (it->second.getType()) {
			case VAR_MAP:
				mg_printmap(conn, it->second.asMap());
				break;
			case VAR_LIST:
				mg_printlist(conn, it->second.asList());
				break;
			case VAR_STRING:
				mg_printf(conn, "\"%s\"", it->second.asString().c_str());	
				break;
			default:
				if (it->second.asString().size() != 0) {
					mg_printf(conn, "%s", it->second.asString().c_str());	
				} else {
					mg_printf(conn, "null");
				}
		}
		if ((it != map.end()) && (next(it) != map.end())) mg_printf(conn, ",");
	}
	mg_printf(conn, "}");
}

static void update (struct mg_connection *conn, const struct mg_request_info *request_info) {
	mg_printf(conn, "%s", "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n");
	Receiver myReceiver = session.createReceiver("agocontrol; {create: always, node: {type: topic}}");

	while(true) {
		try {
			Message receiveMessage = myReceiver.fetch(Duration::SECOND * 3);
			if (receiveMessage.getContentSize() > 3) {	
				Variant::Map receiveMap;
				decode(receiveMessage,receiveMap);
				mg_printmap(conn, receiveMap);
				mg_printf(conn, "\r\n");
			} else  {
				mg_printf(conn, "%s:%s", receiveMessage.getSubject().c_str(),receiveMessage.getContent().c_str());
			}

		} catch (qpid::messaging::NoMessageAvailable) {
			printf("WARNING, no reply message to fetch\n");
		}
	}

}
static void command (struct mg_connection *conn, const struct mg_request_info *request_info) {
	char uuid[1024], command[1024], level[1024];
	Variant::Map agocommand;
	Message message;

	const char *qs = request_info->query_string;
	// int cmdlen =  mg_get_var(qs, strlen(qs == NULL ? "" : qs), "command", command, sizeof(uuid)) ;
	if (mg_get_var(qs, strlen(qs == NULL ? "" : qs), "uuid", uuid, sizeof(uuid)) > 0) agocommand["uuid"] = uuid;
	if (mg_get_var(qs, strlen(qs == NULL ? "" : qs), "command", command, sizeof(command)) > 0) agocommand["command"] = command;
	if (mg_get_var(qs, strlen(qs == NULL ? "" : qs), "level", level, sizeof(level)) > 0) agocommand["level"] = level;

	encode(agocommand, message);

	Address responseQueue("#response-queue; {create:always, delete:always}");
	Receiver responseReceiver = session.createReceiver(responseQueue);
	message.setReplyTo(responseQueue);

	sender.send(message);
	
	mg_printf(conn, "%s", "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n");
	try {
		Message response = responseReceiver.fetch(Duration::SECOND * 3);
		if (response.getContentSize() > 3) {	
			Variant::Map responseMap;
			decode(response,responseMap);
			mg_printmap(conn, responseMap);
		} else  {
			mg_printf(conn, "%s", response.getContent().c_str());
		}

	} catch (qpid::messaging::NoMessageAvailable) {
		printf("WARNING, no reply message to fetch\n");
	}

}
Variant::Map jsonToVariantMap(Json::Value value) {
	Variant::Map map;
	for (Json::ValueIterator it = value.begin(); it != value.end(); it++) {
		// printf("%s\n",it.key().asString().c_str());
		// printf("%s\n", (*it).asString().c_str());
		if ((*it).size() > 0) {
			map[it.key().asString()] = jsonToVariantMap((*it));
		} else {
			if ((*it).isString()) map[it.key().asString()] = (*it).asString();
			if ((*it).isBool()) map[it.key().asString()] = (*it).asBool();
			if ((*it).isInt()) map[it.key().asString()] = (*it).asInt();
			if ((*it).isUInt()) map[it.key().asString()] = (*it).asUInt();
			if ((*it).isDouble()) map[it.key().asString()] = (*it).asDouble();
		}
	}	
	return map;
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
			if (params.isObject()) {
				Json::Value content = params["content"];
				Json::Value subject = params["subject"];
				Variant::Map command = jsonToVariantMap(content);
				Variant::Map responseMap;
				Message message;
				encode(command, message);

				Address responseQueue("#response-queue; {create:always, delete:always}");
				Receiver responseReceiver = session.createReceiver(responseQueue);
				message.setReplyTo(responseQueue);
				if (subject.isString()) message.setSubject(subject.asString());

				sender.send(message);
				try {
					Message response = responseReceiver.fetch(Duration::SECOND * 3);
					if (!(id.isNull())) { // only send reply when id is not null
						result = true;
						if (!firstElem) mg_printf(conn, ",");
						if (response.getContentSize() > 3) {	
							decode(response,responseMap);
							mg_printf(conn, "{\"jsonrpc\": \"2.0\", \"result\": ");
							mg_printmap(conn, responseMap);
							mg_printf(conn, ", \"id\": %s}",myId.c_str());
						} else  {
							mg_printf(conn, "{\"jsonrpc\": \"2.0\", \"result\": \"");
							mg_printf(conn, "%s", response.getContent().c_str());
							mg_printf(conn, "\", \"id\": %s}",myId.c_str());
						}
					} else {
						result = false;
					}

				} catch (qpid::messaging::NoMessageAvailable) {
					mg_printf(conn, "{\"jsonrpc\": \"2.0\", \"result\": \"no-reply\", \"id\": %s}",myId.c_str());
				}
				
			} else {
				mg_printf(conn, "{\"jsonrpc\": \"2.0\", \"error\": {\"code\":-32602,\"message\":\"Invalid params\"}, \"id\": %s}",myId.c_str());
			}
		
		} else if (method == "subscribe") {
			string subscriberName = generateUuid();
			if (id.isNull()) {
				// JSON-RPC notification is invalid here as we need to return the subscription UUID somehow..
				mg_printf(conn, "{\"jsonrpc\": \"2.0\", \"error\": {\"code\":-32600,\"message\":\"Invalid Request\"}, \"id\": %s}",myId.c_str());
			} else if (subscriberName != "") {
				deque<Variant::Map> empty;
				Subscriber subscriber;
				subscriber.lastAccess=time(0);
				subscriber.queue = empty;
				pthread_mutex_lock(&mutexSubscriptions);	
				subscriptions[subscriberName] = subscriber;
				pthread_mutex_unlock(&mutexSubscriptions);	
				mg_printf(conn, "{\"jsonrpc\": \"2.0\", \"result\": \"%s\", \"id\": %s}",subscriberName.c_str(), myId.c_str());
			} else {
				// uuid is empty so malloc probably failed, we seem to be out of memory
				mg_printf(conn, "{\"jsonrpc\": \"2.0\", \"error\": {\"code\":-32000,\"message\":\"Out of memory\"}, \"id\": %s}",myId.c_str());
			}

		} else if (method == "unsubscribe") {
			if (params.isObject()) {
				Json::Value content = params["uuid"];
				if (content.isString()) {
					pthread_mutex_lock(&mutexSubscriptions);	
					subscriptions.erase(content.asString());
					pthread_mutex_unlock(&mutexSubscriptions);	
					mg_printf(conn, "{\"jsonrpc\": \"2.0\", \"result\": \"success\", \"id\": %s}",myId.c_str());
				} else {
					mg_printf(conn, "{\"jsonrpc\": \"2.0\", \"error\": {\"code\":-32602,\"message\":\"Invalid params: need uuid parameter\"}, \"id\": %s}",myId.c_str());
				}
			} else {
				mg_printf(conn, "{\"jsonrpc\": \"2.0\", \"error\": {\"code\":-32602,\"message\":\"Invalid params: need uuid parameter\"}, \"id\": %s}",myId.c_str());
			}
		} else if (method == "getevent") {
			if (params.isObject()) {
				Json::Value content = params["uuid"];
				if (content.isString()) {
					Variant::Map event;
					pthread_mutex_lock(&mutexSubscriptions);	
					map<string,Subscriber>::iterator it = subscriptions.find(content.asString());
					if (it != subscriptions.end()) {
						while (it->second.queue.size() <1) {
							pthread_mutex_unlock(&mutexSubscriptions);	
							usleep(200000);
							pthread_mutex_lock(&mutexSubscriptions);	
						}
						event = it->second.queue.front();
						it->second.queue.pop_front();
						pthread_mutex_unlock(&mutexSubscriptions);	
						mg_printf(conn, "{\"jsonrpc\": \"2.0\", \"result\": ");
						mg_printmap(conn, event);
						mg_printf(conn, ", \"id\": %s}",myId.c_str());
					} else {
						pthread_mutex_unlock(&mutexSubscriptions);	
						mg_printf(conn, "{\"jsonrpc\": \"2.0\", \"error\": {\"code\":-32602,\"message\":\"Invalid params: no current subscription for uuid\"}, \"id\": %s}",myId.c_str());
					}
				} else {
					mg_printf(conn, "{\"jsonrpc\": \"2.0\", \"error\": {\"code\":-32602,\"message\":\"Invalid params: need uuid parameter\"}, \"id\": %s}",myId.c_str());
				}
			} else {
				mg_printf(conn, "{\"jsonrpc\": \"2.0\", \"error\": {\"code\":-32602,\"message\":\"Invalid params: need uuid parameter\"}, \"id\": %s}",myId.c_str());
			}

		} else {
			mg_printf(conn, "{\"jsonrpc\": \"2.0\", \"error\": {\"code\":-32601,\"message\":\"Method not found\"}, \"id\": %s}",myId.c_str());
		}
	} else {
		mg_printf(conn, "{\"jsonrpc\": \"2.0\", \"error\": {\"code\":-32600,\"message\":\"Invalid Request\"}, \"id\": %s}",myId.c_str());
	}
	return result;
}

static void jsonrpc (struct mg_connection *conn, const struct mg_request_info *request_info) {
	Json::Value root;
	Json::Reader reader;
	char post_data[65535];
	int post_data_len;

	post_data_len = mg_read(conn, post_data, sizeof(post_data));
	mg_printf(conn, "%s", ajax_reply_start);		
	if ( reader.parse(post_data, post_data + post_data_len, root, false) ) {
		if (root.isArray()) {
			bool firstElem = true;
			mg_printf(conn, "[");
			for (unsigned int i = 0; i< root.size(); i++) {
				bool result = jsonrpcRequestHandler(conn, root[i], firstElem);
				if (result) firstElem = false; 
			}
			mg_printf(conn, "]");
		} else {
			jsonrpcRequestHandler(conn, root, true);
		}
	} else {
		mg_printf(conn, "%s", "{\"jsonrpc\": \"2.0\", \"error\": {\"code\":-32700,\"message\":\"Parse error\"}, \"id\": null}");
	}
}


static void *event_handler(enum mg_event event,
                           struct mg_connection *conn) {
  const struct mg_request_info *request_info = mg_get_request_info(conn);
  void *processed =  (void *) "yes";

  if (event == MG_NEW_REQUEST) {
    if (strcmp(request_info->uri, "/command") == 0) {
      command(conn, request_info);
    } else if (strcmp(request_info->uri, "/jsonrpc") == 0) {
      jsonrpc(conn, request_info);
    } else if (strcmp(request_info->uri, "/update") == 0) {
      update(conn, request_info);
    } else {
      // No suitable handler found, mark as not processed. Mongoose will
      // try to serve the request.
      processed = NULL;
    }
  } else if (event == MG_EVENT_LOG) {
    printf("%s\n", (const char *) mg_get_request_info(conn)->ev_data);
    processed = NULL;
  } else {
    processed = NULL;
  }

  return processed;
}



int main(int argc, char **argv) {
	string broker;
	string port; 

	Variant::Map connectionOptions;
	CDataFile ExistingDF("/etc/opt/agocontrol/config.ini");

	t_Str szBroker  = t_Str("");
	szBroker = ExistingDF.GetString("broker", "system");
	if ( szBroker.size() == 0 )
		broker="localhost:5672";
	else		
		broker= szBroker;
	t_Str szUsername  = t_Str("");
	szUsername = ExistingDF.GetString("username", "system");
	if ( szUsername.size() == 0 )
		connectionOptions["username"]="agocontrol";
	else		
		connectionOptions["username"] = szUsername;

	t_Str szPassword  = t_Str("");
	szPassword = ExistingDF.GetString("password", "system");
	if ( szPassword.size() == 0 )
		connectionOptions["password"]="letmein";
	else		
		connectionOptions["password"]=szPassword;

	t_Str szRPCPort  = t_Str("");
	szRPCPort = ExistingDF.GetString("rpcport", "system");
	if ( szRPCPort.size() == 0 )
		port = "8008";
	else		
		port = szRPCPort;

	static const char *options[] = {
	  "document_root", "html",
	  "listening_ports", port.c_str(),
	  "num_threads", "5",
	  NULL
	};
	connectionOptions["reconnect"] = "true";


	Connection connection(broker, connectionOptions);
	try {
		connection.open(); 
		session = connection.createSession(); 
		receiver = session.createReceiver("agocontrol; {create: always, node: {type: topic}}"); 
		sender = session.createSender("agocontrol; {create: always, node: {type: topic}}"); 
	} catch(const std::exception& error) {
		std::cerr << error.what() << std::endl;
		connection.close();
		printf("could not startup\n");
		return 1;
	}

	pthread_mutex_init(&mutexSubscriptions, NULL);

	// start web server
	if((ctx = mg_start(&event_handler, NULL, options)) == NULL) {
		printf("Cannot start http server\n");
	}

	while (true) {
		try{
			Variant::Map content;
			string subject;
			Message message = receiver.fetch(Duration::SECOND * 3);
			session.acknowledge();

			subject = message.getSubject();

			// test if it is an event
			if (subject.size()>0) {
				// don't flood clients with unneeded events
				if (subject == "event.environment.timechanged") continue;

				//printf("received event: %s\n", subject.c_str());	
				// workaround for bug qpid-3445
				if (message.getContent().size() < 4) {
					throw qpid::messaging::EncodingException("message too small");
				}

				decode(message, content);
				content["event"] = subject;
				pthread_mutex_lock(&mutexSubscriptions);	
				for (map<string,Subscriber>::iterator it = subscriptions.begin(); it != subscriptions.end(); it++) {
					if (it->second.queue.size() > 100) {
						// this subscription seems to be abandoned, let's remove it to save resources
						printf("removing subscription %s as the queue size exceeds limits\n", it->first.c_str());
						subscriptions.erase(it);
					} else {
						it->second.queue.push_back(content);
					}
				}
				pthread_mutex_unlock(&mutexSubscriptions);	
			}	

		} catch(const NoMessageAvailable& error) {
			
		} catch(const std::exception& error) {
			std::cerr << error.what() << std::endl;
		}
	}

}
