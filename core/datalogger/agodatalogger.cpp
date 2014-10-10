#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <pthread.h>

#include <string>
#include <iostream>
#include <sstream>
#include <cerrno>

#include <sqlite3.h>

#include <boost/date_time/posix_time/time_parsers.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <jsoncpp/json/writer.h>
#include <jsoncpp/json/reader.h>

#include "agoclient.h"

#ifndef DBFILE
#define DBFILE "datalogger.db"
#endif

using namespace std;
using namespace agocontrol;
namespace fs = ::boost::filesystem;

AgoConnection *agoConnection;
sqlite3 *db;
qpid::types::Variant::Map inventory;
bool logAllEvents = 1;

std::string uuidToName(std::string uuid) {
	qpid::types::Variant::Map devices = inventory["inventory"].asMap();
	if (devices[uuid].isVoid()) {
		return "";
	}
	qpid::types::Variant::Map device = devices[uuid].asMap();
	return device["name"].asString() == "" ? uuid : device["name"].asString();
}

bool createTableIfNotExist(string tablename, list<string> createqueries) {
	//init
	sqlite3_stmt *stmt = NULL;
	int rc;
	char* sqlError = 0;
	string query = "SELECT name FROM sqlite_master WHERE type='table' AND name = ?";

	//prepare query
	rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, NULL);
	if(rc != SQLITE_OK) {
		AGO_ERROR() << "sql error #" << rc << ": " << sqlite3_errmsg(db);
		return false;
	}
	sqlite3_bind_text(stmt, 1, tablename.c_str(), -1, NULL);

	//execute query
	if( stmt!=NULL ) {
		rc = sqlite3_step(stmt);
		sqlite3_finalize(stmt);
		if( rc!=SQLITE_ROW ) {
			//table doesn't exit, create it
			AGO_INFO() << "Creating missing table '" << tablename << "'" << endl;
			for( list<string>::iterator it=createqueries.begin(); it!=createqueries.end(); it++ ) {
				rc = sqlite3_exec(db, (*it).c_str(), NULL, NULL, &sqlError);
				if(rc != SQLITE_OK) {
					AGO_ERROR() << "sql error #" << rc << ": " << sqlError;
					return false;
				}
			}
		}
	}
	createqueries.clear();

	return true;
}

void eventHandler(std::string subject, qpid::types::Variant::Map content) {
	sqlite3_stmt *stmt = NULL;
	int rc;
	string result;

	//log all events?
	if( !logAllEvents )
	{
		//events log deactivated
		return;
	}

	// string devicename = uuidToName(content["uuid"].asString());
	string uuid = content["uuid"].asString();
	if (subject != "" ) {
		if( subject=="event.environment.positionchanged" && content["latitude"].asString()!="" && content["longitude"].asString()!="" ) {
			AGO_INFO() << "specific environment case: position" << endl;
			string lat = content["latitude"].asString();
			string lon = content["longitude"].asString();

			string query = "INSERT INTO position VALUES(null, ?, ?, ?, ?)";
			rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, NULL);
			if(rc != SQLITE_OK) {
				AGO_ERROR() << "sql error #" << rc << ": " << sqlite3_errmsg(db);
				return;
			}

			sqlite3_bind_text(stmt, 1, uuid.c_str(), -1, NULL);
			sqlite3_bind_text(stmt, 2, lat.c_str(), -1, NULL);
			sqlite3_bind_text(stmt, 3, lon.c_str(), -1, NULL);
			sqlite3_bind_int(stmt, 4, time(NULL));
		}
		else if( content["level"].asString() != "") {
			replaceString(subject, "event.environment.", "");
			replaceString(subject, "event.device.", "");
			replaceString(subject, "changed", "");
			replaceString(subject, "event.", "");
			AGO_DEBUG() << "environment: " << subject << endl;

			string level = content["level"].asString();

			string query = "INSERT INTO data VALUES(null, ?, ?, ?, ?)";
			rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, NULL);
			if(rc != SQLITE_OK) {
				AGO_ERROR() << "sql error #" << rc << ": " << sqlite3_errmsg(db);
				return;
			}

			sqlite3_bind_text(stmt, 1, uuid.c_str(), -1, NULL);
			sqlite3_bind_text(stmt, 2, subject.c_str(), -1, NULL);
			sqlite3_bind_text(stmt, 3, level.c_str(), -1, NULL);
			sqlite3_bind_int(stmt, 4, time(NULL));
		}

		if( stmt!=NULL ) {
			rc = sqlite3_step(stmt);
			switch(rc) {
				case SQLITE_ERROR:
					AGO_ERROR() << "step error: " << sqlite3_errmsg(db);
					break;
				case SQLITE_ROW:
					if (sqlite3_column_type(stmt, 0) == SQLITE_TEXT) result =string( (const char*)sqlite3_column_text(stmt, 0));
					break;
			}

			sqlite3_finalize(stmt);
		}
	}
}

void GetGraphData(qpid::types::Variant::Map content, qpid::types::Variant::Map &result) {
	sqlite3_stmt *stmt;
	int rc;
	qpid::types::Variant::List values;

	// Parse the timestrings
	string startDate = content["start"].asString();
	string endDate = content["end"].asString();
	replaceString(startDate, "-", "");
	replaceString(startDate, ":", "");
	replaceString(startDate, "Z", "");
	replaceString(endDate, "-", "");
	replaceString(endDate, ":", "");
	replaceString(endDate, "Z", "");

	//get environment
	string environment = content["env"].asString();
	AGO_TRACE() << "GetGraphData:" << environment << endl;

	boost::posix_time::ptime base(boost::gregorian::date(1970, 1, 1));
	boost::posix_time::time_duration start = boost::posix_time::from_iso_string(startDate) - base;
	boost::posix_time::time_duration end = boost::posix_time::from_iso_string(endDate) - base;

	//prepare specific query string
	if( environment=="position" ) {
		rc = sqlite3_prepare_v2(db, "SELECT timestamp, latitude, longitude FROM position WHERE timestamp BETWEEN ? AND ? AND uuid = ? ORDER BY timestamp", -1, &stmt, NULL);
	}
	else {
		rc = sqlite3_prepare_v2(db, "SELECT timestamp, level FROM data WHERE timestamp BETWEEN ? AND ? AND environment = ? AND uuid = ? ORDER BY timestamp", -1, &stmt, NULL);
	}

	//check query
	if(rc != SQLITE_OK) {
		AGO_ERROR() << "sql error #" << rc << ": " << sqlite3_errmsg(db);
		return;
	}

	//add specific fields
	if( environment=="position" ) {
		//fill query
		sqlite3_bind_int(stmt, 1, start.total_seconds());
		sqlite3_bind_int(stmt, 2, end.total_seconds());
		sqlite3_bind_text(stmt, 3, content["deviceid"].asString().c_str(), -1, NULL);

		do {
			rc = sqlite3_step(stmt);
			switch(rc) {
				case SQLITE_ERROR:
					AGO_ERROR() << "step error: " << sqlite3_errmsg(db);
					break;
				case SQLITE_ROW:
					qpid::types::Variant::Map value;
					value["time"] = sqlite3_column_int(stmt, 0);
					value["latitude"] = sqlite3_column_double(stmt, 1);
					value["longitude"] = sqlite3_column_double(stmt, 2);
					values.push_back(value);
					break;
			}
		}
		while (rc == SQLITE_ROW);
	}
	else {
		//fill query
		sqlite3_bind_int(stmt, 1, start.total_seconds());
		sqlite3_bind_int(stmt, 2, end.total_seconds());
		sqlite3_bind_text(stmt, 3, environment.c_str(), -1, NULL);
		sqlite3_bind_text(stmt, 4, content["deviceid"].asString().c_str(), -1, NULL);

		do {
			rc = sqlite3_step(stmt);
			switch(rc) {
				case SQLITE_ERROR:
					AGO_ERROR() << "step error: " << sqlite3_errmsg(db);
					break;
				case SQLITE_ROW:
					qpid::types::Variant::Map value;
					value["time"] = sqlite3_column_int(stmt, 0);
					value["level"] = sqlite3_column_double(stmt, 1);
					values.push_back(value);
					break;
			}
		}
		while (rc == SQLITE_ROW);
	}

	sqlite3_finalize(stmt);

	qpid::types::Variant::Map data;
	data["values"] = values;
	result["result"] = data;
}

qpid::types::Variant::Map commandHandler(qpid::types::Variant::Map content) {
	qpid::types::Variant::Map returnval;
	std::string internalid = content["internalid"].asString();
	if (internalid == "dataloggercontroller") {
		if (content["command"] == "getdata") {
			GetGraphData(content, returnval);
		} else if (content["command"] == "getdeviceenvironments") {
			sqlite3_stmt *stmt;
			int rc;
			rc = sqlite3_prepare_v2(db, "SELECT distinct uuid, environment FROM data", -1, &stmt, NULL);
			if(rc != SQLITE_OK) {
				AGO_ERROR() << "sql error #" << rc << ": " << sqlite3_errmsg(db);
				return returnval;
			}

			do {
				rc = sqlite3_step(stmt);
				switch(rc) {
					case SQLITE_ERROR:
						AGO_ERROR() << "step error: " << sqlite3_errmsg(db);
						break;
					case SQLITE_ROW:
						returnval[string((const char*)sqlite3_column_text(stmt, 0))] = string((const char*)sqlite3_column_text(stmt, 1));
						break;
				}
			}
			while (rc == SQLITE_ROW);

			sqlite3_finalize(stmt);
		}
	}
	return returnval;
}

int main(int argc, char **argv) {
	fs::path dbpath = ensureParentDirExists(getLocalStatePath(DBFILE));
	int rc = sqlite3_open(dbpath.c_str(), &db);
	if( rc != SQLITE_OK){
		AGO_FATAL() <<  "Can't open database: " << sqlite3_errmsg(db);
		sqlite3_close(db);
		return 1;
	}

	list<string> queries;
	queries.push_back("CREATE TABLE position(id INTEGER PRIMARY KEY AUTOINCREMENT, uuid TEXT, latitude REAL, longitude REAL, timestamp LONG)");
	queries.push_back("CREATE INDEX timestamp_position_idx ON position(timestamp)");
	queries.push_back("CREATE INDEX uuid_position_idx ON position(uuid)");
	createTableIfNotExist("position", queries);

	agoConnection = new AgoConnection("datalogger");	
	agoConnection->addDevice("dataloggercontroller", "dataloggercontroller");
	agoConnection->addHandler(commandHandler);
	agoConnection->addEventHandler(eventHandler);
	inventory = agoConnection->getInventory();

	//read config
	std::string optString = getConfigOption("datalogger", "logAllEvents", "1");
	int r;
	if(sscanf(optString.c_str(), "%d", &r) == 1) {
		logAllEvents = (r == 1);
	}

	agoConnection->run();

	return 0;
}
