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

#include <boost/preprocessor/stringize.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/locks.hpp>

#include <signal.h>
#include <sys/wait.h>

#include "agoapp.h"

//mongoose close idle connection after 30 seconds (timeout)
#define MONGOOSE_POLLING 1000 //in ms, mongoose polling time
#define GETEVENT_DEFAULT_TIMEOUT 28 // mongoose poll every seconds (see MONGOOSE_POLLING)

//upload file path
#define UPLOAD_PATH "/tmp/"

//default auth file
#define HTPASSWD ".htpasswd"

/* JSON-RPC 2.0 standard error codes
 * http://www.jsonrpc.org/specification#error_object
 */
#define JSONRPC_PARSE_ERROR -32700
#define JSONRPC_INVALID_REQUEST -32600
#define JSONRPC_METHOD_NOT_FOUND -32601
#define JSONRPC_INVALID_PARAMS -32602
#define JSONRPC_INTERNAL_ERROR -32603

// -32000 to -32099 impl-defined server errors
#define AGO_JSONRPC_NO_EVENT -32000

namespace fs = ::boost::filesystem;
using namespace std;
using namespace qpid::messaging;
using namespace qpid::types;
using namespace agocontrol;


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
    int timeout;

    GetEventState(std::string subscriptionId_, const Json::Value rpcRequestId_)
        : subscriptionId(subscriptionId_)
          , rpcRequestId(rpcRequestId_)
          , lastPoll(0)
          , timeout(GETEVENT_DEFAULT_TIMEOUT)	{
              inited = time(NULL);
          }
};

// helper to determine last element
#ifndef _LIBCPP_ITERATOR
template <typename Iter>
Iter next(Iter iter)
{
    return ++iter;
}
#endif


class AgoRpc: public AgoApp {
private:
    //auth
    FILE* authFile;
    map<string,Subscriber> subscriptions;
    boost::mutex mutexSubscriptions;

    list<mg_server*> all_servers;
    list<boost::thread*> server_threads;
    time_t last_event;

    bool getEventRequest(struct mg_connection *conn, GetEventState* state);
    bool jsonrpcRequestHandler(struct mg_connection *conn, Json::Value request) ;
    bool jsonrpc(struct mg_connection *conn);
    void uploadFiles(struct mg_connection *conn);
    bool downloadFile(struct mg_connection *conn);

    void eventHandler(std::string subject, qpid::types::Variant::Map content) ;

    void serve_webserver(void *server);

    void setupApp();
    void cleanupApp();
public:
    AGOAPP_CONSTRUCTOR_HEAD(AgoRpc)
        , authFile(NULL)
        {}

    // Called from public mg_event_handler_wrapper
    int mg_event_handler(struct mg_connection *conn, enum mg_event event);
};

static int mg_event_handler_wrapper(struct mg_connection *conn, enum mg_event event) {
    AgoRpc* inst = (AgoRpc*)conn->server_param;
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

/**
 * Add JSON-RPC headers (jsonrpc, id) onto the response_root object,
 * serialize it to JSON, and write it to the mg_connnection
 *
 * Note: response_root is modified!
 *
 * Always returns false, to indicate that no more output is to be written.
 */
static bool mg_rpc_reply(struct mg_connection *conn, const Json::Value &request_or_id, Json::Value &response_root)
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
    return false;
}

/**
 * Write a RPC response with one element in the root with the name set in result_key (default "result").
 *
 * Always returns false, to indicate that no more output is to be written.
 */
static bool mg_rpc_reply_result(struct mg_connection *conn, const Json::Value &request_or_id, const Json::Value &result, std::string result_key="result") {
    Json::Value root(Json::objectValue);
    root[result_key] = result;
    return mg_rpc_reply(conn, request_or_id, root);
}

/**
 * Write a RPC response with a "result" element whose value is a simple string.
 *
 * Always returns false, to indicate that no more output is to be written.
 */
static bool mg_rpc_reply_result(struct mg_connection *conn, const Json::Value &request_or_id, const std::string result) {
    Json::Value r(result);
    return mg_rpc_reply_result(conn, request_or_id, r);
}

/**
 * Write a RCP response with an "error" element having the specified code/message.
 *
 * Always returns false, to indicate that no more output is to be written.
 */
static bool mg_rpc_reply_error(struct mg_connection *conn, const Json::Value &request_or_id, int code, const std::string message) {
    Json::Value error(Json::objectValue);
    error["code"] = code;
    error["message"] = message;

    return mg_rpc_reply_result(conn, request_or_id, error, "error");
}

/**
 * Write a RPC response with a "result" element being a response map
 *
 * Always returns false, to indicate that no more output is to be written.
 */
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

/**
 * Handle getEvent polling request
 * Return true if an event wasn't found to say to mongoose request is not over (return MG_MORE).
 * Return false if an event is found to say to mongoose request is over (return MG_TRUE) then app.js
 * will request again getEvent().
 *
 * Connection parameters are stored directly into mg_connection object inside connection_param variable.
 * Thanks to this variable its possible to call many times getEventRequest.
 */
bool AgoRpc::getEventRequest(struct mg_connection *conn, GetEventState* state)
{
    int result = true;
    Variant::Map event;

    //look for available event
    boost::unique_lock<boost::mutex> lock(mutexSubscriptions);
    map<string,Subscriber>::iterator it = subscriptions.find(state->subscriptionId);

    if(it == subscriptions.end()) {
        lock.unlock();
        mg_rpc_reply_error(conn, state->rpcRequestId, JSONRPC_INVALID_PARAMS, "Invalid params: no current subscription for uuid");
        return false;
    }

    if(it->second.queue.size() >= 1 )
    {
        //event found
        event = it->second.queue.front();
        it->second.queue.pop_front();
        lock.unlock();

        std::string myId(state->rpcRequestId.toStyledString());
        mg_printf_data(conn, "{\"jsonrpc\": \"2.0\", \"result\": ");
        mg_printmap(conn, event);
        mg_printf_data(conn, ", \"id\": %s}", myId.c_str());

        result = false;
    }

    return result;
}

/**
 * Process a single JSONRPC requests.
 *
 * Returns:
 * 	false if a response has been written,
 * 	true if further processing is required
 */
bool AgoRpc::jsonrpcRequestHandler(struct mg_connection *conn, Json::Value request) {
    Json::StyledWriter writer;
    string myId;
    const Json::Value id = request.get("id", Json::Value());
    const string method = request.get("method", "message").asString();
    const string version = request.get("jsonrpc", "unspec").asString();

    myId = writer.write(id);
    if (version != "2.0") {
        return mg_rpc_reply_error(conn, request, JSONRPC_INVALID_REQUEST, "Invalid Request");
    }

    const Json::Value params = request.get("params", Json::Value());
    if (method == "message" ) {
        if (!params.isObject() || params.empty()) {
            return mg_rpc_reply_error(conn, request, JSONRPC_INVALID_PARAMS, "Invalid params");
        }

        //prepare message
        Json::Value content = params["content"];
        Json::Value subject = params["subject"];
        Variant::Map command = jsonToVariantMap(content);

        //send message and handle response
        //AGO_TRACE() << "Request: " << command;
        Variant::Map responseMap = agoConnection->sendMessageReply(subject.asString().c_str(), command);
        if(responseMap.size() == 0 || id.isNull() ) // only send reply when id is not null
        {
            // no response
            if(responseMap.size() == 0) {
                AGO_ERROR() << "No reply message to fetch. Failed message: " << "subject=" <<subject<<": " << command;
            }

            return mg_rpc_reply_result(conn, request, std::string("no-reply"));
        }

        //AGO_TRACE() << "Response: " << responseMap;
        return mg_rpc_reply_map(conn, request, responseMap);

    } else if (method == "subscribe") {
        string subscriberName = generateUuid();
        if (id.isNull()) {
            // JSON-RPC notification is invalid here as we need to return the subscription UUID somehow..
            return mg_rpc_reply_error(conn, request, JSONRPC_INVALID_REQUEST, "Invalid Request");
        }

        if (subscriberName == "") {
            // uuid is empty so malloc probably failed, we seem to be out of memory
            return mg_rpc_reply_error(conn, request, JSONRPC_INTERNAL_ERROR, "Out of memory");
        }

        deque<Variant::Map> empty;
        Subscriber subscriber;
        subscriber.lastAccess=time(0);
        subscriber.queue = empty;
        {
            boost::lock_guard<boost::mutex> lock(mutexSubscriptions);
            subscriptions[subscriberName] = subscriber;
        }
        return mg_rpc_reply_result(conn, request, subscriberName);

    } else if (method == "unsubscribe") {
        if (!params.isObject()) {
            return mg_rpc_reply_error(conn, request, JSONRPC_INVALID_PARAMS, "Invalid params: need uuid parameter");
        }

        Json::Value content = params["uuid"];
        if (!content.isString()) {
            return mg_rpc_reply_error(conn, request, JSONRPC_INVALID_PARAMS, "Invalid params: need uuid parameter");
        }

        AGO_DEBUG() << "removing subscription: " << content.asString();
        {
            boost::lock_guard<boost::mutex> lock(mutexSubscriptions);
            map<string,Subscriber>::iterator it = subscriptions.find(content.asString());
            if (it != subscriptions.end()) {
                subscriptions.erase(content.asString());
            }
        }

        return mg_rpc_reply_result(conn, request, std::string("success"));
    } else if (method == "getevent") {
        if (!params.isObject()) {
            return mg_rpc_reply_error(conn, request, JSONRPC_INVALID_PARAMS, "Invalid params: need uuid parameter");
        }

        Json::Value content = params["uuid"];
        if (!content.isString()){
            return mg_rpc_reply_error(conn, request, JSONRPC_INVALID_PARAMS, "Invalid params: need uuid parameter");
        }

        //add connection param (used to get connection param when polling see MG_POLL)
        GetEventState *state = new GetEventState(content.asString(), id);

        Json::Value timeout = params["timeout"];
        if(timeout.isInt() && timeout <= GETEVENT_DEFAULT_TIMEOUT) {
            state->timeout = timeout.asInt();
        }

        conn->connection_param = state;
        return getEventRequest(conn, state);
    }

    return mg_rpc_reply_error(conn, request, JSONRPC_METHOD_NOT_FOUND, "Method not found");
}

/**
 * Process one or more JSONRPC requests.
 *
 * Returns:
 * 	false if a response has been written, and no more output is to be written
 * 	true if further processing is required, and MG_POLL shall continue
 */
bool AgoRpc::jsonrpc(struct mg_connection *conn)
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
                // XXX: If any previous jsonrpcRequestHandler returned TRUE,
                // that request will be silently dropped/unprocessed/not processed properly.
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
        mg_rpc_reply_error(conn, Json::Value(), JSONRPC_PARSE_ERROR, "Parse error");
    }

    return result;
}

/**
 * Upload files
 * @info source from https://github.com/cesanta/mongoose/blob/master/examples/upload.c
 */
void AgoRpc::uploadFiles(struct mg_connection *conn)
{
    //init
    const char *data;
    int data_len, ofs = 0;
    char var_name[100], file_name[100];
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
            // Sanitize filename, it should only be a filename.
            fs::path orig_fn(file_name);
            fs::path safe_fn = orig_fn.filename();
            if(std::string(file_name) != safe_fn.string()){
                AGO_ERROR() << "Rejecting file upload, unsafe path \"" << file_name << "\" ";
                uploadError = "Invalid filename";
                break;
            }

            //check if uuid found
            if(uuid.size() == 0)
            {
                //no uuid found yet, drop file
                continue;
            }

            // Save file to a temporary path
            fs::path tempfile = fs::path(UPLOAD_PATH) / fs::unique_path().replace_extension(safe_fn.extension());
            FILE* fp = fopen(tempfile.c_str(), "wb");
            if( fp )
            {
                //write file first
                AGO_DEBUG() << "Uploading file \"" << safe_fn.string() << "\" file to " << tempfile;
                int written = fwrite(data, sizeof(char), data_len, fp);
                fclose(fp);
                if( written!=data_len )
                {
                    //error writting file, drop it
                    fs::remove(tempfile);
                    AGO_ERROR() << "Uploaded file \"" << tempfile.string() << "\" not fully written (no space left?)";
                    uploadError = "Unable to write file (no space left?)";
                    continue;
                }

                //then send command
                content.clear();
                content["uuid"] = std::string(uuid);
                content["command"] = "uploadfile";
                content["filepath"] = tempfile.string();
                content["filename"] = safe_fn.string();
                content["filesize"] = data_len;

                Variant::Map responseMap = agoConnection->sendMessageReply("", content);
                if( responseMap.size()==0 )
                {
                    //command failed, drop file
                    fs::remove(tempfile);
                    AGO_ERROR() << "Uploaded file \"" << tempfile.string() << "\" dropped because command failed";
                    uploadError = "Internal error";
                    continue;
                }

                if( !responseMap["result"].isVoid() && responseMap["result"].asInt16()!=0 )
                {
                    //file rejected, drop it
                    fs::remove(tempfile);
                    if( !responseMap["error"].isVoid() )
                    {
                        uploadError = responseMap["error"].asString();
                    }
                    else
                    {
                        uploadError = "File rejected";
                    }
                    AGO_ERROR() << "Uploaded file \"" << safe_fn.string() << "\" rejected by recipient: " << uploadError;
                    //uploadError = "File rejected: no handler available";
                    continue;
                }

                //add file to output
                Variant::Map file;
                file["name"] = safe_fn.string();
                file["size"] = data_len;
                files.push_back(file);

                //delete file (it should be processed by sendcommand)
                //TODO: maybe a purge process could be interesting to implement
                fs::remove(tempfile);
            }else{
                AGO_ERROR() << "Failed to open file " << tempfile.string() << " for writing: " << strerror(errno);
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
bool AgoRpc::downloadFile(struct mg_connection *conn)
{
    //init
    char param[1024];
    Variant::Map content;
    Variant::Map responseMap;
    string downloadError = "";
    fs::path filepath;

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
                filepath = fs::path(responseMap["filepath"].asString());
                AGO_DEBUG() << "Downloading file \"" << filepath << "\"";
            }
            else
            {
                //invalid command response
                AGO_ERROR() << "Download file, sendCommand returned invalid response (need filepath)";
                downloadError = "Internal error";
            }
        }
        else
        {
            //command failed
            AGO_ERROR() << "Download file, sendCommand failed, unable to send file";
            downloadError = "Internal error";
        }
    }
    else
    {
        //missing parameters!
        AGO_ERROR() << "Download file, missing parameters. Nothing done";
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
int AgoRpc::mg_event_handler(struct mg_connection *conn, enum mg_event event)
{
    int result = MG_FALSE;

    if (event == MG_REQUEST)
    {
        if (strcmp(conn->uri, "/jsonrpc") == 0)
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
                if((state->lastPoll - state->inited) > state->timeout) {
                    // close connection now (before mongoose close connection)
                    mg_rpc_reply_error(conn, state->rpcRequestId, AGO_JSONRPC_NO_EVENT, "no messages for subscription");
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
            rewind(authFile);
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
void AgoRpc::eventHandler(std::string subject, qpid::types::Variant::Map content)
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

    {
        boost::lock_guard<boost::mutex> lock(mutexSubscriptions);
        //AGO_TRACE() << "Incoming notify: " << content;
        for (map<string,Subscriber>::iterator it = subscriptions.begin(); it != subscriptions.end(); ) {
            if (it->second.queue.size() > 100) {
                // this subscription seems to be abandoned, let's remove it to save resources
                AGO_INFO() << "removing subscription as the queue size exceeds limits: " << it->first.c_str();
                subscriptions.erase(it++);
            } else {
                it->second.queue.push_back(content);
                ++it;
            }
        }
    }

    // Wake servers to let pending getEvent trigger
    last_event = time(NULL);
    for(list<mg_server*>::iterator i = all_servers.begin(); i != all_servers.end(); i++) {
        mg_wakeup_server(*i);
    }
}

/**
 * Webserver process (threaded)
 */
void AgoRpc::serve_webserver(void *server)
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

void AgoRpc::setupApp() {
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
    port = getConfigOption("ports", "8008,8009s");
    htdocs = getConfigOption("htdocs", fs::path(BOOST_PP_STRINGIZE(DEFAULT_HTMLDIR)));
    certificate = getConfigOption("certificate", getConfigPath("/rpc/rpc_cert.pem"));
    numthreads = getConfigOption("numthreads", "30");
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

    //auth
    fs::path authPath = htdocs / HTPASSWD;
    if( fs::exists(authPath) )
    {
        //activate auth
        authFile = fopen(authPath.c_str(), "r");
        if( authFile==NULL )
        {
            //unable to parse auth file
            AGO_ERROR() << "Auth support: error parsing \"" << authPath.string() << "\" file. Authentication deactivated";
        }
        else
        {
            AGO_INFO() << "Enabling authentication";
        }
    }
    else
    {
        AGO_INFO() << "Disabling authentication: file does not exist";
    }

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
            boost::thread *t = new boost::thread(boost::bind(&AgoRpc::serve_webserver, this, server));
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

void AgoRpc::cleanupApp() {
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
    fclose(authFile);
}

AGOAPP_ENTRY_POINT(AgoRpc);

