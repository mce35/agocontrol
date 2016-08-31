#ifndef INVENTORY_H
#define INVENTORY_H

#include <qpid/messaging/Connection.h>
#include <qpid/messaging/Message.h>
#include <qpid/messaging/Receiver.h>
#include <qpid/messaging/Sender.h>
#include <qpid/messaging/Session.h>
#include <qpid/messaging/Address.h>

#include <string>

#include <sqlite3.h>

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

    bool isDeviceRegistered(string uuid);
    void deleteDevice(string uuid);

    string getDeviceName(string uuid);
    string getDeviceRoom(string uuid);
    bool setDeviceName(string uuid, string name);
    string getRoomName(string uuid);
    bool setRoomName(string uuid, string name);
    bool setDeviceRoom(string deviceuuid, string roomuuid);
    string getDeviceRoomName(string uuid);
    Variant::Map getRooms();
    bool deleteRoom(string uuid);

    string getFloorplanName(string uuid);
    bool setFloorplanName(string uuid, string name);
    bool setDeviceFloorplan(string deviceuuid, string floorplanuuid, int x, int y);
    void delDeviceFloorplan(string deviceuuid, string floorplanuuid);
    bool deleteFloorplan(string uuid);
    Variant::Map getFloorplans();

    string getLocationName(string uuid);
    string getRoomLocation(string uuid);
    bool setLocationName(string uuid, string name);
    bool setRoomLocation(string roomuuid, string locationuuid);
    bool deleteLocation(string uuid);
    Variant::Map getLocations();

    bool createUser(string uuid, string username, string password, string pin, string description);
    bool deleteUser(string uuid);
    bool authUser(string uuid);
    bool setPassword(string uuid);
    bool setPin(string uuid);
    bool setPermission(string uuid, string permission);
    bool deletePermission(string uuid, string permission);
    Variant::Map getPermissions(string uuid);


private:
    sqlite3 *db;
    string getFirst(const char *query);
    string getFirst(const char *query, int n, ...);
    string getFirstFound(const char *query, bool &found, int n, ...);
    string getFirstArgs(const char *query, bool &found, int n, va_list args);
    bool createTableIfNotExist(std::string tablename, std::string createquery);
};

#endif
