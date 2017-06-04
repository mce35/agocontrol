#ifndef AGOCLIENT_H
#define AGOCLIENT_H

#include <string>
#include <sstream>
#include <fstream>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <qpid/messaging/Connection.h>
#include <qpid/messaging/Message.h>
#include <qpid/messaging/Receiver.h>
#include <qpid/messaging/Sender.h>
#include <qpid/messaging/Session.h>
#include <qpid/messaging/Address.h>

#include <json/value.h>

#include <boost/filesystem.hpp>
#include <boost/function.hpp>

#include "agolog.h"
#include "agoconfig.h"

#include "agoproto.h"

#include <uuid/uuid.h>
namespace agocontrol {

    bool nameval(const std::string& in, std::string& name, std::string& value);

    /// string replace helper.
    void replaceString(std::string& subject, const std::string& search, const std::string& replace);

    // string split helper
    std::vector<std::string> split(const std::string &s, char delimiter);
    std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems);

    /// convert a Variant::Map to JSON string representation.
    std::string variantMapToJSONString(qpid::types::Variant::Map map);
    /// convert a Variant::List to JSON string.
    std::string variantListToJSONString(qpid::types::Variant::List list);
    /// convert a JSON value to a Variant::Map.
    qpid::types::Variant::Map jsonToVariantMap(Json::Value value);
    /// convert a JSON string to a Variant::List.
    qpid::types::Variant::List jsonToVariantList(Json::Value value);
    /// convert a JSON string to a Variant::Map.
    qpid::types::Variant::Map jsonStringToVariantMap(std::string jsonstring);

    /// convert content of a JSON file containing JSON data.
    qpid::types::Variant::Map jsonFileToVariantMap(const boost::filesystem::path &filename);
    // write a Variant::Map to a JSON file.
    bool variantMapToJSONFile(qpid::types::Variant::Map map, const boost::filesystem::path &filename);

    /// helper to generate a string containing a uuid.
    std::string generateUuid();

    /// helper for conversions
    std::string uint64ToString(uint64_t i);
    unsigned int stringToUint(std::string v);

    /// convert int to std::string.
    std::string int2str(int i);

    /// convert float to std::string.
    std::string float2str(float f);
    std::string double2str(double f);

    /// ago control client connection class.
    class AgoConnection {
    protected:
        qpid::messaging::Connection connection;
        qpid::messaging::Sender sender;
        qpid::messaging::Receiver receiver;
        qpid::messaging::Session session;
        qpid::types::Variant::Map deviceMap; // this holds the internal device list
        qpid::types::Variant::Map uuidMap; // this holds the permanent uuid to internal id mapping
        bool shutdownSignaled;
        bool storeUuidMap(); // stores the map on disk
        bool loadUuidMap(); // loads it
        boost::filesystem::path uuidMapFile;
        std::string instance;
        void reportDevices();
        bool filterCommands;
        boost::function< qpid::types::Variant::Map (qpid::types::Variant::Map) > commandHandler;
        boost::function< void (std::string, qpid::types::Variant::Map) > eventHandler;
        bool emitDeviceAnnounce(const char *internalId, const char *deviceType, const char*initialName);
        bool emitDeviceDiscover(const char *internalId, const char *deviceType);
        bool emitDeviceRemove(const char *internalId);
    public:
        AgoConnection(const char *interfacename);
        ~AgoConnection();
        void run();
        void shutdown();
        bool addDevice(const char *internalId, const char *deviceType, const char*initialName = NULL);
        bool addDevice(const char *internalId, const char *deviceType, bool passuuid);
        bool removeDevice(const char *internalId);
        bool suspendDevice(const char* internalId);
        bool resumeDevice(const char* internalId);
        std::string getDeviceType(const char *internalId);
        int isDeviceStale(const char* internalId);
        bool emitDeviceStale(const char* uuid, const int stale);

        // C-style function pointers
        bool addHandler(qpid::types::Variant::Map (*handler)(qpid::types::Variant::Map));
        bool addEventHandler(void (*eventHandler)(std::string, qpid::types::Variant::Map));

        // C++-style function pointers, permits class references
        bool addHandler(boost::function<qpid::types::Variant::Map (qpid::types::Variant::Map)> handler);
        bool addEventHandler(boost::function<void (std::string, qpid::types::Variant::Map)> eventHandler);

        bool setFilter(bool filter);
        bool sendMessage(const char *subject, qpid::types::Variant::Map content);
        bool sendMessage(qpid::types::Variant::Map content);

        // New-style API.
        // TEMP NOTE: Only use towards apps using new style command handlers!
        AgoResponse sendRequest(const qpid::types::Variant::Map& content);
        AgoResponse sendRequest(const std::string& subject, const qpid::types::Variant::Map& content);

        // Deprecated
        qpid::types::Variant::Map sendMessageReply(const char *subject, const qpid::types::Variant::Map& content, qpid::messaging::Duration timeout);
        qpid::types::Variant::Map sendMessageReply(const char *subject, const qpid::types::Variant::Map& content);

        bool emitEvent(const char *internalId, const char *eventType, const char *level, const char *units);
        bool emitEvent(const char *internalId, const char *eventType, double level, const char *units);
        bool emitEvent(const char *internalId, const char *eventType, int level, const char *units);
        bool emitEvent(const char *internalId, const char *eventType, qpid::types::Variant::Map content);
        qpid::types::Variant::Map getInventory();
        std::string getAgocontroller();
        bool setGlobalVariable(std::string variable, qpid::types::Variant value);
        std::string uuidToInternalId(std::string uuid); // lookup in map
        std::string internalIdToUuid(std::string internalId); // lookup in map
    };

}/* namespace agocontrol */

#endif
