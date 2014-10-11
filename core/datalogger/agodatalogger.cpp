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
#include <boost/algorithm/string/predicate.hpp>
//#include <memory>

#include <rrd.h>
#include "base64.h"

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
qpid::types::Variant::Map units;
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
        cerr << "Sql error #" << rc << ": " << sqlite3_errmsg(db) << endl;;
        return false;
    }
    sqlite3_bind_text(stmt, 1, tablename.c_str(), -1, NULL);

    //execute query
    if( stmt!=NULL ) {
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        if( rc!=SQLITE_ROW ) {
            //table doesn't exit, create it
            cout << "Creating missing table '" << tablename << "'" << endl;
            for( list<string>::iterator it=createqueries.begin(); it!=createqueries.end(); it++ ) {
                rc = sqlite3_exec(db, (*it).c_str(), NULL, NULL, &sqlError);
                if(rc != SQLITE_OK) {
                    cerr << "Sql error #" << rc << ": " << sqlError << endl;
                    return false;
                }
            }
        }
    }
    createqueries.clear();

    return true;
}

std::string string_format(const std::string fmt, ...) {
    int size = ((int)fmt.size()) * 2 + 50;   // use a rubric appropriate for your code
    std::string str;
    va_list ap;
    while (1) {     // maximum 2 passes on a POSIX system...
        str.resize(size);
        va_start(ap, fmt);
        int n = vsnprintf((char *)str.data(), size, fmt.c_str(), ap);
        va_end(ap);
        if (n > -1 && n < size) {  // everything worked
            str.resize(n);
            return str;
        }
        if (n > -1)  // needed size returned
            size = n + 1;   // for null char
        else
            size *= 2;      // guess at a larger size (o/s specific)
    }
    return str;
}

/**
 * Generate RRDtool graph
 */
bool generateGraph(std::string uuid, int start, int end, unsigned char** img, unsigned long* size)
{
    bool found = false;

    //rrdfile
    stringstream filename;
    filename << uuid << ".rrd";
    fs::path rrdfile = getLocalStatePath(filename.str());

    //get device infos from inventory
    qpid::types::Variant::Map devices = inventory["devices"].asMap();
    qpid::types::Variant::Map device;
    for( qpid::types::Variant::Map::const_iterator it=devices.begin(); it!=devices.end(); it++ )
    {
        if( it->first==uuid )
        {
            //device found
            device = it->second.asMap();
        }
    }

    if( !device["devicetype"].isVoid() )
    {
        //prepare kind
        string kind = device["devicetype"].asString();
        replaceString(kind, "sensor", "");

        //prepare unit
        string unit = "U";
        string vertical_unit = "U";
        qpid::types::Variant::Map values = device["values"].asMap();
        for( qpid::types::Variant::Map::const_iterator it=values.begin(); it!=values.end(); it++ )
        {
            if( it->first==kind )
            {
                qpid::types::Variant::Map infos = it->second.asMap();
                if( !infos["unit"].isVoid() )
                {
                    vertical_unit = infos["unit"].asString();
                }
                break;
            }
        }
        if( !units[vertical_unit].isVoid() )
        {
            vertical_unit = units[vertical_unit].asString();
        }
        if( vertical_unit=="%" )
        {
            unit = "%%";
        }
        else
        {
            unit = vertical_unit;
        }

        //prepare colors
        string colorL = "#000000";
        string colorA = "#A0A0A0";
        string colorMax = "#FF0000";
        string colorMin = "#00FF00";
        string colorAvg = "#0000FF";
        if( device["devicetype"].asString()=="humiditysensor" )
        {
            //blue
            colorL = "#0000FF";
            colorA = "#7777FF";
        }
        else if( device["devicetype"].asString()=="temperaturesensor" )
        {
            //red
            colorL = "#FF0000";
            colorA = "#FF8787";
        }
        else if( device["devicetype"].asString()=="energysensor" || device["devicetype"].asString()=="batterysensor" )
        {
            //green
            colorL = "#007A00";
            colorA = "#00BB00";
        }
        else if( device["devicetype"].asString()=="brightnesssensor" )
        {
            //orange
            colorL = "#CCAA00";
            colorA = "#FFD400";
        }

        //prepare graph parameters
        string START = string_format("%d", start);
        string END = string_format("%d", end);
        string DEF = string_format("DEF:level=%s:level:AVERAGE", rrdfile.c_str());
        string GFX_AREA = string_format("AREA:level%s", colorA.c_str());
        string GFX_LINE = string_format("LINE1:level%s:%s", colorL.c_str(), kind.c_str());
        string MIN_LINE = string_format("LINE1:levelmin%s:Min\\::dashes", colorMin.c_str());
        string MIN_GPRINT = string_format("GPRINT:levelmin:%%6.2lf%s", unit.c_str());
        string MAX_LINE = string_format("LINE1:levelmax%s:Max\\::dashes", colorMax.c_str());
        string MAX_GPRINT = string_format("GPRINT:levelmax:%%6.2lf%s", unit.c_str());
        string AVG_LINE = string_format("LINE1:levelavg%s:Avg\\::dashes", colorAvg.c_str());
        string AVG_GPRINT = string_format("GPRINT:levelavg:%%6.2lf%s", unit.c_str());
        string LAST_GPRINT = string_format("GPRINT:levellast:Last\\:%%6.2lf%s", unit.c_str());

        const char* params[] = {"dummy", "", "--start", START.c_str(), "--end", END.c_str(), "--vertical-label", vertical_unit.c_str(), "--width" ,"850", "--height", "300", DEF.c_str(), "VDEF:levellast=level,LAST", "VDEF:levelavg=level,AVERAGE", "VDEF:levelmax=level,MAXIMUM", "VDEF:levelmin=level,MINIMUM", GFX_AREA.c_str(), GFX_LINE.c_str(), MIN_LINE.c_str(), MIN_GPRINT.c_str(), MAX_LINE.c_str(), MAX_GPRINT.c_str(), AVG_LINE.c_str(), AVG_GPRINT.c_str(), LAST_GPRINT.c_str()};
        int num_params = 26;

        rrd_info_t* grinfo = rrd_graph_v(num_params, (char**)params);
        rrd_info_t* walker;
        if( grinfo!=NULL )
        {
            walker = grinfo;
            while (walker)
            {
                if (strcmp(walker->key, "image") == 0)
                {
                    *img = walker->value.u_blo.ptr;
                    *size = walker->value.u_blo.size;
                    found = true;
                    break;
                }
                walker = walker->next;
            }
        }
        else
        {
            cerr << "agodatalogger-RRDtool: unable to generate graph [" << rrd_get_error() << "]" << endl;
            return false;
        }
    }
    else
    {
        //device not found
        cerr << "agodatalogger-RRDtool: device not found" << endl;
        return false;
    }

    if( !found) 
    {
        cerr << "agodatalogger-RRDtool: no image generated by rrd_graph_v command" << endl;
        return false;
    }
    
    return true;
}

/**
 * Store event data into RRDtool database
 */
void eventHandlerRRDtool(std::string subject, std::string uuid, qpid::types::Variant::Map content)
{
    if( (subject=="event.device.batterylevelchanged" || boost::algorithm::starts_with(subject, "event.environment.")) && !content["level"].isVoid() && !content["uuid"].isVoid() )
    {
        //generate rrd filename and path
        stringstream filename;
        filename << content["uuid"].asString() << ".rrd";
        fs::path rrdfile = getLocalStatePath(filename.str());

        //create rrd file if necessary
        if( !fs::exists(rrdfile) )
        {
            cout << "New device detected, create rrdfile " << rrdfile.string() << endl;
            const char *params[] = {"DS:level:GAUGE:21600:U:U", "RRA:AVERAGE:0.5:1:1440", "RRA:AVERAGE:0.5:5:2016", "RRA:AVERAGE:0.5:30:1488", "RRA:AVERAGE:0.5:60:8760", "RRA:AVERAGE:0.5:360:2920", "RRA:MIN:0.5:1:1440", "RRA:MIN:0.5:5:2016", "RRA:MIN:0.5:30:1488", "RRA:MIN:0.5:60:8760", "RRA:MIN:0.5:360:2920", "RRA:MAX:0.5:1:1440", "RRA:MAX:0.5:5:2016", "RRA:MAX:0.5:30:1488", "RRA:MAX:0.5:60:8760", "RRA:MAX:0.5:360:2920"};

            int res = rrd_create_r(rrdfile.string().c_str(), 60, 0, 16, params);
            if( res<0 )
            {
                cerr << "agodatalogger-RRDtool: unable to create rrdfile [" << rrd_get_error() << "]" << endl;
            }
        }  

        //update rrd
        if( fs::exists(rrdfile) )
        {
            cout << "update rrdfile " << rrdfile.string() << endl;
            char param[50];
            snprintf(param, 50, "N:%s", content["level"].asString().c_str());
            const char* params[] = {param};
    
            int res = rrd_update_r(rrdfile.string().c_str(), "level", 1, params);
            if( res<0 )
            {
                cerr << "agodatalogger-RRDtool: unable to update data [" << rrd_get_error() << "]" << endl;
            }
        }
    }
}

/**
 * Store event data into SQLite database
 */
void eventHandlerSQLite(std::string subject, std::string uuid, qpid::types::Variant::Map content)
{
    sqlite3_stmt *stmt = NULL;
    int rc;
    string result;

    if( subject=="event.environment.positionchanged" && content["latitude"].asString()!="" && content["longitude"].asString()!="" )
    {
        cout << "specific environment case: position" << endl;
        string lat = content["latitude"].asString();
        string lon = content["longitude"].asString();

        string query = "INSERT INTO position VALUES(null, ?, ?, ?, ?)";
        rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, NULL);
        if(rc != SQLITE_OK)
        {
            fprintf(stderr, "sql error #%d: %s\n", rc,sqlite3_errmsg(db));
            return;
        }

        sqlite3_bind_text(stmt, 1, uuid.c_str(), -1, NULL);
        sqlite3_bind_text(stmt, 2, lat.c_str(), -1, NULL);
        sqlite3_bind_text(stmt, 3, lon.c_str(), -1, NULL);
        sqlite3_bind_int(stmt, 4, time(NULL));
    }
    else if( content["level"].asString() != "")
    {
        replaceString(subject, "event.environment.", "");
        replaceString(subject, "event.device.", "");
        replaceString(subject, "changed", "");
        replaceString(subject, "event.", "");

        string level = content["level"].asString();

        string query = "INSERT INTO data VALUES(null, ?, ?, ?, ?)";
        rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, NULL);
        if(rc != SQLITE_OK) {
            fprintf(stderr, "sql error #%d: %s\n", rc,sqlite3_errmsg(db));
            return;
        }

        sqlite3_bind_text(stmt, 1, uuid.c_str(), -1, NULL);
        sqlite3_bind_text(stmt, 2, subject.c_str(), -1, NULL);
        sqlite3_bind_text(stmt, 3, level.c_str(), -1, NULL);
        sqlite3_bind_int(stmt, 4, time(NULL));
    }

    if( stmt!=NULL )
    {
        rc = sqlite3_step(stmt);
        switch(rc)
        {
            case SQLITE_ERROR:
                fprintf(stderr, "step error: %s\n",sqlite3_errmsg(db));
                break;
            case SQLITE_ROW:
                if (sqlite3_column_type(stmt, 0) == SQLITE_TEXT) result =string( (const char*)sqlite3_column_text(stmt, 0));
                break;
        }

        sqlite3_finalize(stmt);
    }
}

/**
 * Main event handler
 */
void eventHandler(std::string subject, qpid::types::Variant::Map content) {
    cout << subject << endl;
    if( subject!="" && !content["uuid"].isVoid() )
    {
        if( logAllEvents )
        {
            eventHandlerSQLite(subject, content["uuid"].asString(), content);
        }

        eventHandlerRRDtool(subject, content["uuid"].asString(), content);
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
    cout << "GetGraphData:" << environment << endl;

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
        fprintf(stderr, "sql error #%d: %s\n", rc,sqlite3_errmsg(db));
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
                    fprintf(stderr, "step error: %s\n",sqlite3_errmsg(db));
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
                    fprintf(stderr, "step error: %s\n",sqlite3_errmsg(db));
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
    if (internalid == "dataloggercontroller")
    {
        if (content["command"] == "getdata")
        {
            GetGraphData(content, returnval);
        }
        else if (content["command"] == "getdeviceenvironments")
        {
            sqlite3_stmt *stmt;
            int rc;
            rc = sqlite3_prepare_v2(db, "SELECT distinct uuid, environment FROM data", -1, &stmt, NULL);
            if(rc != SQLITE_OK)
            {
                fprintf(stderr, "sql error #%d: %s\n", rc,sqlite3_errmsg(db));
                return returnval;
            }

            do
            {
                rc = sqlite3_step(stmt);
                switch(rc)
                {
                    case SQLITE_ERROR:
                        fprintf(stderr, "step error: %s\n",sqlite3_errmsg(db));
                        break;
                    case SQLITE_ROW:
                        returnval[string((const char*)sqlite3_column_text(stmt, 0))] = string((const char*)sqlite3_column_text(stmt, 1));
                        break;
                }
            }
            while (rc == SQLITE_ROW);

            sqlite3_finalize(stmt);
        }
        else if( content["command"] == "getgraph" )
        {
            unsigned char* img = NULL;
            unsigned long size = 0;
            if( generateGraph(content["device"].asString(), content["start"].asInt32(), content["end"].asInt32(), &img, &size) )
            {
                returnval["error"] = 0;
                returnval["msg"] = "";
                returnval["graph"] = base64_encode(img, size);
            }
            else
            {
                //error generating graph
                returnval["error"] = 1;
                returnval["msg"] = "Internal error";
            }
        }
    }
    return returnval;
}

int main(int argc, char **argv) {
    fs::path dbpath = ensureParentDirExists(getLocalStatePath(DBFILE));
    int rc = sqlite3_open(dbpath.c_str(), &db);
    if( rc != SQLITE_OK){
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
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

    //get units
    for( qpid::types::Variant::Map::iterator it=inventory["schema"].asMap()["units"].asMap().begin(); it!=inventory["schema"].asMap()["units"].asMap().end(); it++ )
    {
        units[it->first] = it->second.asMap()["label"].asString();
    }

    //read config
    std::string optString = getConfigOption("datalogger", "logAllEvents", "1");
    int r;
    if(sscanf(optString.c_str(), "%d", &r) == 1) {
        logAllEvents = (r == 1);
    }

    agoConnection->run();

    return 0;
}
