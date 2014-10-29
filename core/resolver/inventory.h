#include <qpid/messaging/Connection.h>
#include <qpid/messaging/Message.h>
#include <qpid/messaging/Receiver.h>
#include <qpid/messaging/Sender.h>
#include <qpid/messaging/Session.h>
#include <qpid/messaging/Address.h>

#include <string>

#include <sqlite3.h>

#define BOOST_FILESYSTEM_VERSION 3
#define BOOST_FILESYSTEM_NO_DEPRECATED
#include <boost/filesystem.hpp>

using namespace std;
using namespace qpid::messaging;
using namespace qpid::types;
namespace fs = ::boost::filesystem;

class Inventory {
	public:
		Inventory(const fs::path &dbfile);
		~Inventory();
		void close();

		string getdevicename (string uuid);
		string getdeviceroom (string uuid);
		int setdevicename (string uuid, string name);
		string getroomname (string uuid);
		int setroomname (string uuid, string name);
		int setdeviceroom (string deviceuuid, string roomuuid);
		string getdeviceroomname (string uuid);
		Variant::Map getrooms();
		int deleteroom (string uuid);

		string getfloorplanname (string uuid);
		int setfloorplanname(string uuid, string name);
		int setdevicefloorplan(string deviceuuid, string floorplanuuid, int x, int y);
		int deletefloorplan(string uuid);
		Variant::Map getfloorplans();

		string getlocationname (string uuid);
		string getroomlocation (string uuid);
		int setlocationname(string uuid, string name);
		int setroomlocation(string roomuuid, string locationuuid);
		int deletelocation(string uuid);
		Variant::Map getlocations();

		int createuser(string uuid, string username, string password, string pin, string description);
		int deleteuser(string uuid);
		int authuser(string uuid);
		int setpassword(string uuid);
		int setpin(string uuid);
		int setpermission(string uuid, string permission);
		int deletepermission(string uuid, string permission);
		Variant::Map getpermissions(string uuid);

		
	private:
		sqlite3 *db;
		string getfirst(const char *query);
		string getfirst(const char *query, int n, ...);
		bool createTableIfNotExist(std::string tablename, std::string createquery);
};
