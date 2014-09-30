#include <string>
#include <sstream>
#include <fstream>

#ifndef __FreeBSD__
#include <malloc.h>
#endif
#include <stdio.h>
#include <unistd.h>
#include <syslog.h>
#include <iostream>

#include <qpid/messaging/Connection.h>
#include <qpid/messaging/Message.h>
#include <qpid/messaging/Receiver.h>
#include <qpid/messaging/Sender.h>
#include <qpid/messaging/Session.h>
#include <qpid/messaging/Address.h>

#include <jsoncpp/json/value.h>

#include <uuid/uuid.h>
#include <augeas.h>

namespace agocontrol {

	bool nameval(const std::string& in, std::string& name, std::string& value);

	/// string replace helper.
	void replaceString(std::string& subject, const std::string& search, const std::string& replace);
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
	qpid::types::Variant::Map jsonFileToVariantMap(std::string filename);
	// write a Variant::Map to a JSON file.
	bool variantMapToJSONFile(qpid::types::Variant::Map map, std::string filename);

	/// helper to generate a string containing a uuid.
	std::string generateUuid();

	/// Return the full path to the configuration directory, with subpath appended if not NULL
	std::string getConfigPath(const char *subpath = NULL);

	/// Return the full path to the local-state directory, with subpath appended if not NULL
	std::string getLocalStatePath(const char *subpath = NULL);

	bool augeas_init();
	augeas *augeas = NULL;
	/// fetch a value from the config file.
	std::string getConfigOption(const char *section, const char *option, std::string defaultvalue);
	std::string getConfigOption(const char *section, const char *option, const char *defaultvalue);

	/// save value to the config file
	bool setConfigOption(const char *section, const char *option, const char* value);
	bool setConfigOption(const char *section, const char *option, const float value);
	bool setConfigOption(const char *section, const char *option, const int value);
	bool setConfigOption(const char *section, const char *option, const bool value);

	/// convert int to std::string.
	std::string int2str(int i);

	/// convert float to std::string.
	std::string float2str(float f);

	/// ago control client connection class.
	class AgoConnection {
		protected:
			qpid::messaging::Connection connection;
			qpid::messaging::Sender sender;
			qpid::messaging::Receiver receiver;
			qpid::messaging::Session session;
			qpid::types::Variant::Map deviceMap; // this holds the internal device list
			qpid::types::Variant::Map uuidMap; // this holds the permanent uuid to internal id mapping
			bool storeUuidMap(); // stores the map on disk
			bool loadUuidMap(); // loads it
			std::string uuidMapFile;
			std::string instance;
			std::string uuidToInternalId(std::string uuid); // lookup in map
			std::string internalIdToUuid(std::string internalId); // lookup in map
			void reportDevices();
			qpid::types::Variant::Map (*commandHandler)(qpid::types::Variant::Map);
			bool filterCommands;
			void (*eventHandler)(std::string, qpid::types::Variant::Map);
			bool emitDeviceAnnounce(const char *internalId, const char *deviceType);
			bool emitDeviceRemove(const char *internalId);
			bool emitDeviceStale(const char* internalId, const int stale);
		public:
			AgoConnection(const char *interfacename);
			~AgoConnection();
			void run();
			bool addDevice(const char *internalId, const char *deviceType);
			bool addDevice(const char *internalId, const char *deviceType, bool passuuid);
			bool removeDevice(const char *internalId);
			bool suspendDevice(const char* internalId);
			bool resumeDevice(const char* internalId);
			std::string getDeviceType(const char *internalId);
			int isDeviceStale(const char *internalId);
			bool addHandler(qpid::types::Variant::Map (*handler)(qpid::types::Variant::Map));
			bool addEventHandler(void (*eventHandler)(std::string, qpid::types::Variant::Map));
			bool setFilter(bool filter);
			bool sendMessage(const char *subject, qpid::types::Variant::Map content);
			bool sendMessage(qpid::types::Variant::Map content);
			qpid::types::Variant::Map sendMessageReply(const char *subject, qpid::types::Variant::Map content);
			bool emitEvent(const char *internalId, const char *eventType, const char *level, const char *units);
			bool emitEvent(const char *internalId, const char *eventType, float level, const char *units);
			bool emitEvent(const char *internalId, const char *eventType, int level, const char *units);
			bool emitEvent(const char *internalId, const char *eventType, qpid::types::Variant::Map content);
			qpid::types::Variant::Map getInventory();
			std::string getAgocontroller();
			bool setGlobalVariable(std::string variable, qpid::types::Variant value);
	};


	enum LogPriority {
		kLogEmerg	= LOG_EMERG,   // system is unusable
		kLogAlert	= LOG_ALERT,   // action must be taken immediately
		kLogCrit		= LOG_CRIT,    // critical conditions
		kLogErr		= LOG_ERR,     // error conditions
		kLogWarning = LOG_WARNING, // warning conditions
		kLogNotice  = LOG_NOTICE,  // normal, but significant, condition
		kLogInfo		= LOG_INFO,    // informational message
		kLogDebug	= LOG_DEBUG    // debug-level message
	};

	std::ostream& operator<< (std::ostream& os, const LogPriority& log_priority);

	class Log : public std::basic_streambuf<char, std::char_traits<char> > {
		public:
			explicit Log(std::string ident, int facility);

		protected:
			int sync();
			int overflow(int c);

		private:
			friend std::ostream& operator<< (std::ostream& os, const LogPriority& log_priority);
			std::string buffer_;
			int facility_;
			int priority_;
			char ident_[50];
	};

}

