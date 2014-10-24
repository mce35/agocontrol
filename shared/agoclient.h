#ifndef AGOCLIENT_H
#define AGOCLIENT_H

#include <string>
#include <sstream>
#include <fstream>

#ifndef __FreeBSD__
#include <malloc.h>
#endif
#include <stdio.h>
#include <unistd.h>

#include <qpid/messaging/Connection.h>
#include <qpid/messaging/Message.h>
#include <qpid/messaging/Receiver.h>
#include <qpid/messaging/Sender.h>
#include <qpid/messaging/Session.h>
#include <qpid/messaging/Address.h>

#include <jsoncpp/json/value.h>

#define BOOST_FILESYSTEM_VERSION 3
#define BOOST_FILESYSTEM_NO_DEPRECATED
#include <boost/filesystem.hpp>
#include <boost/function.hpp>

#include "agolog.h"

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
	qpid::types::Variant::Map jsonFileToVariantMap(const boost::filesystem::path &filename);
	// write a Variant::Map to a JSON file.
	bool variantMapToJSONFile(qpid::types::Variant::Map map, const boost::filesystem::path &filename);

	/// helper to generate a string containing a uuid.
	std::string generateUuid();

	/// Return the full path to the configuration directory, with subpath appended if not NULL
	boost::filesystem::path getConfigPath(const boost::filesystem::path &subpath = boost::filesystem::path());

	/// Return the full path to the local-state directory, with subpath appended if not NULL
	boost::filesystem::path getLocalStatePath(const boost::filesystem::path &subpath = boost::filesystem::path());

	class AgoApp;
	class AgoClientInternal {
		// Do not expose to other than AgoApp
		friend class AgoApp;
	protected:
		static void setConfigDir(const boost::filesystem::path &dir);
		static void setLocalStateDir(const boost::filesystem::path &dir);
	};

	/// Ensures that the directory exists
	/// Note: throws exception if directory creation fails
	boost::filesystem::path ensureDirExists(const boost::filesystem::path &dir);

	/// Ensures that the parent directory for the specified filename exists
	/// Note: throws exception if directory creation fails
	boost::filesystem::path ensureParentDirExists(const boost::filesystem::path &filename);

	bool augeas_init();
	std::string augeasPathFromSectionOption(const char *section, const char *option);

	/// fetch a value from the config file.
	std::string getConfigOption(const char *section, const char *option, std::string &defaultvalue);
	std::string getConfigOption(const char *section, const char *option, const char *defaultvalue);
	boost::filesystem::path getConfigOption(const char *section, const char *option, const boost::filesystem::path &defaultvalue);

	/// fetch a value from the config file. if it does not exist in designated section, try
	/// fallback_section before finally falling back on defaultvalue
	std::string getConfigOption(const char *section, const char *fallback_section, const char *option, const char *defaultvalue);
	qpid::types::Variant::Map getConfigTree();

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
			bool shutdownSignaled;
			bool storeUuidMap(); // stores the map on disk
			bool loadUuidMap(); // loads it
			boost::filesystem::path uuidMapFile;
			std::string instance;
			void reportDevices();
			bool filterCommands;
			boost::function< qpid::types::Variant::Map (qpid::types::Variant::Map) > commandHandler;
			boost::function< void (std::string, qpid::types::Variant::Map) > eventHandler;
			bool emitDeviceAnnounce(const char *internalId, const char *deviceType);
			bool emitDeviceRemove(const char *internalId);
			bool emitDeviceStale(const char* internalId, const int stale);
		public:
			AgoConnection(const char *interfacename);
			~AgoConnection();
			void run();
			void shutdown();
			bool addDevice(const char *internalId, const char *deviceType);
			bool addDevice(const char *internalId, const char *deviceType, bool passuuid);
			bool removeDevice(const char *internalId);
			bool suspendDevice(const char* internalId);
			bool resumeDevice(const char* internalId);
			std::string getDeviceType(const char *internalId);
			int isDeviceStale(const char *internalId);

			// C-style function pointers
			bool addHandler(qpid::types::Variant::Map (*handler)(qpid::types::Variant::Map));
			bool addEventHandler(void (*eventHandler)(std::string, qpid::types::Variant::Map));

			// C++-style function pointers, permits class references
			bool addHandler(boost::function<qpid::types::Variant::Map (qpid::types::Variant::Map)> handler);
			bool addEventHandler(boost::function<void (std::string, qpid::types::Variant::Map)> eventHandler);

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
			std::string uuidToInternalId(std::string uuid); // lookup in map
			std::string internalIdToUuid(std::string internalId); // lookup in map
	};

}


#endif
