#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

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
bool enableSql = 1;
bool enableRrd = 1;
//const char *colors[] = {"#800080", "#FF00FF", "#000080", "#0000FF", "#008080", "#00FFFF", "#008000", "#00FF00", "#808000", "#FFFF00", "#800000", "#FF0000", "#000000", "#808080", "#C0C0C0"};
const char* colors[] = {"#800080", "#0000FF", "#008000", "#FF00FF", "#000080", "#FF0000", "#00FF00", "#00FFFF", "#800000", "#808000", "#008080", "#C0C0C0", "#808080", "#000000", "#FFFF00"};

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

/**
 * Format string to specified format
 */
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
 * Return real string length
 */
int string_real_length(const std::string str)
{
    int len = 0;
    const char* s = str.c_str();
    while(*s)
    {
        len += (*s++ & 0xc0) != 0x80;
    }
    return len;
}

/**
 * Prepend spaces to source string until source size reached
 */
std::string string_prepend_spaces(std::string source, size_t newSize)
{
    std::string output = source;
    int max = (int)newSize - string_real_length(output);
    for( int i=0; i<max; i++ )
    {
        output.insert(0, 1, ' ');
    }
    return output;
}

/**
 * Prepare graph for specified uuid
 * @param uuid: device uuid
 * @param multiId: if multigraph specify its index (-1 if not multigraph)
 * @param data: output map
 */
bool prepareGraph(std::string uuid, int multiId, qpid::types::Variant::Map& data)
{
    //check params
    if( multiId>=15 )
    {
        //no more color available
        cerr << "agodatalogger-RRDtool: no more color available" << endl;
        return false;
    }

    //filename
    stringstream filename;
    filename << uuid << ".rrd";

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
        string pretty_kind = kind;
        if( multiId>=0 )
        {
            //keep first 4 chars
            pretty_kind.resize(4);
            //and append device name (or uuid if name not set)
            if( !device["name"].isVoid() && device["name"].asString().length()>0 )
            {
                pretty_kind = device["name"].asString();
            }
            else
            {
                pretty_kind = uuid + " (" + pretty_kind + ")";
            }
        }
        pretty_kind.resize(20, ' ');

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
        if( multiId<0 )
        {
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
        }
        else
        {
            //multi graph (only line color necessary)
            colorL = colors[multiId];
        }

        //fill output map
        data["filename"] = filename.str();
        data["kind" ] = pretty_kind;
        data["unit"] = unit;
        data["vertical_unit"] = vertical_unit;
        data["colorL"] = colorL;
        data["colorA"] = colorA;
        data["colorMax"] = colorMax;
        data["colorMin"] = colorMin;
        data["colorAvg"] = colorAvg;
    }
    else
    {
        //device not found
        cerr << "agodatalogger-RRDtool: device not found" << endl;
        return false;
    }

    return true;
}

/**
 * Display params (debug purpose)
 */
void dumpGraphParams(const char** params, const int num_params)
{
    for( int i=0; i<num_params; i++ )
    {
        cout << " - " << string(params[i]) << endl;
    }
}

/**
 * Add graph param
 */
bool addGraphParam(const std::string& param, char** params, int* index)
{
    params[(*index)] = (char*)malloc(sizeof(char)*param.length()+1);
    if( params[(*index)]!=NULL )
    {
        strcpy(params[(*index)], param.c_str());
        (*index)++;
        return true;
    }
    return false;
}

/**
 * Add default and mandatory graph params
 */
void addDefaultParameters(int start, int end, string vertical_unit, int width, int height, char** params, int* index)
{
    string param = "";
    
    //first params
    addGraphParam("dummy", params, index);
    addGraphParam("", params, index);

    //start
    addGraphParam("--start", params, index);
    addGraphParam(string_format("%d", start), params, index);

    //end
    addGraphParam("--end", params, index);
    addGraphParam(string_format("%d", end), params, index);

    //vertical label
    if( vertical_unit.length()>0 )
    {
        addGraphParam("--vertical-label", params, index);
        addGraphParam(vertical_unit, params, index);
    }

    //size
    addGraphParam("--width", params, index);
    addGraphParam(string_format("%d", width), params, index);
    addGraphParam("--height", params, index);
    addGraphParam(string_format("%d", height), params, index);
}

/**
 * Add single graph parameters
 */
void addSingleGraphParameters(qpid::types::Variant::Map& data, char** params, int* index)
{
    string param = "";

    //DEF
    fs::path rrdfile = getLocalStatePath(data["filename"].asString());
    addGraphParam(string_format("DEF:level=%s:level:AVERAGE", rrdfile.c_str()), params, index);

    //VDEF last
    addGraphParam("VDEF:levellast=level,LAST", params, index);

    //VDEF average
    addGraphParam("VDEF:levelavg=level,AVERAGE", params, index);

    //VDEF max
    addGraphParam("VDEF:levelmax=level,MAXIMUM", params, index);

    //VDEF min
    addGraphParam("VDEF:levelmin=level,MINIMUM", params, index);

    //GFX AREA
    addGraphParam(string_format("AREA:level%s", data["colorA"].asString().c_str()), params, index);

    //GFX LINE
    addGraphParam(string_format("LINE1:level%s:%s", data["colorL"].asString().c_str(), data["kind"].asString().c_str()), params, index);

    //MIN LINE
    addGraphParam(string_format("LINE1:levelmin%s::dashes", data["colorMin"].asString().c_str()), params, index);

    //MIN GPRINT
    addGraphParam(string_format("GPRINT:levelmin:   Min %%6.2lf%s", data["unit"].asString().c_str()), params, index);

    //MAX LINE
    addGraphParam(string_format("LINE1:levelmax%s::dashes", data["colorMax"].asString().c_str()), params, index);

    //MAX GPRINT
    addGraphParam(string_format("GPRINT:levelmax:   Max %%6.2lf%s", data["unit"].asString().c_str()), params, index);

    //AVG LINE
    addGraphParam(string_format("LINE1:levelavg%s::dashes", data["colorAvg"].asString().c_str()), params, index);

    //AVG GPRINT
    addGraphParam(string_format("GPRINT:levelavg:   Avg %%6.2lf%s", data["unit"].asString().c_str()), params, index);

    //LAST GPRINT
    addGraphParam(string_format("GPRINT:levellast:   Last %%6.2lf%s", data["unit"].asString().c_str()), params, index);
}

/**
 * Add multi graph parameters
 */
void addMultiGraphParameters(qpid::types::Variant::Map& data, char** params, int* index)
{
    //keep index value
    int id = (*index);

    //DEF
    fs::path rrdfile = getLocalStatePath(data["filename"].asString());
    addGraphParam(string_format("DEF:level%d=%s:level:AVERAGE", id, rrdfile.c_str()), params, index);

    //VDEF last
    addGraphParam(string_format("VDEF:levellast%d=level%d,LAST", id, id), params, index);

    //VDEF average
    addGraphParam(string_format("VDEF:levelavg%d=level%d,AVERAGE", id, id), params, index);

    //VDEF max
    addGraphParam(string_format("VDEF:levelmax%d=level%d,MAXIMUM", id, id), params, index);

    //VDEF min
    addGraphParam(string_format("VDEF:levelmin%d=level%d,MINIMUM", id, id), params, index);

    //GFX LINE
    addGraphParam(string_format("LINE1:level%d%s:%s", id, data["colorL"].asString().c_str(), data["kind"].asString().c_str()), params, index);

    //MIN GPRINT
    addGraphParam(string_format("GPRINT:levelmin%d:     Min %%6.2lf%s", id, data["unit"].asString().c_str()), params, index);

    //MAX GPRINT
    addGraphParam(string_format("GPRINT:levelmax%d:     Max %%6.2lf%s", id, data["unit"].asString().c_str()), params, index);

    //AVG GRPINT
    addGraphParam(string_format("GPRINT:levelavg%d:     Avg %%6.2lf%s", id, data["unit"].asString().c_str()), params, index);

    //LAST GPRINT
    addGraphParam(string_format("GPRINT:levellast%d:     Last %%6.2lf%s", id, data["unit"].asString().c_str()), params, index);

    //new line
    addGraphParam("COMMENT:\\n", params, index);
}

/**
 * Generate RRDtool graph
 */
bool generateGraph(qpid::types::Variant::List uuids, int start, int end, unsigned char** img, unsigned long* size)
{
    qpid::types::Variant::Map data;
    char** params;
    int num_params = 0;
    int index = 0;
    int multiId = -1;
    qpid::types::Variant::List datas;

    //get graph datas
    for( qpid::types::Variant::List::iterator it=uuids.begin(); it!=uuids.end(); it++ )
    {
        //multigraph?
        if( uuids.size()>1 )
            multiId++;

        //get data
        qpid::types::Variant::Map data;
        if( prepareGraph((*it).asString(), multiId, data) )
        {
            datas.push_back(data);
        }
    }

    //prepare graph parameters
    if( datas.size()>1 )
    {
        //multigraph
        //adjust some stuff
        int defaultNumParam = 12;
        string vertical_unit = "";
        string lastUnit = "";
        size_t maxUnitLength = 0;
        for( qpid::types::Variant::List::iterator it=datas.begin(); it!=datas.end(); it++ )
        {
            vertical_unit = (*it).asMap()["vertical_unit"].asString();

            if( (*it).asMap()["unit"].asString().length()>maxUnitLength )
            {
                maxUnitLength = (*it).asMap()["unit"].asString().length();
            }

            if( (*it).asMap()["unit"].asString()!=lastUnit )
            {
                if( lastUnit.length()>0 )
                {
                    //not the same unit
                    defaultNumParam = 10;
                    vertical_unit = "";
                }
                else
                {
                    lastUnit = (*it).asMap()["unit"].asString();
                }
            }
        }
        //format unit (add spaces for better display)
        for( qpid::types::Variant::List::iterator it=datas.begin(); it!=datas.end(); it++ )
        {
            string unit = (*it).asMap()["unit"].asString();
            //special case for %%
            if( unit=="%%" )
            {
                (*it).asMap()["unit"] = string_prepend_spaces(unit, maxUnitLength+1);
            }
            else
            {
                (*it).asMap()["unit"] = string_prepend_spaces(unit, maxUnitLength);
            }
        }

        //alloc memory
        num_params = 11 * datas.size() + defaultNumParam; //11 parameters per datas + 10 default parameters (wo vertical_unit) or 12 (with vertical_unit)
        params = (char**)malloc(sizeof(char*) * num_params);

        //add graph parameters
        addDefaultParameters(start, end, vertical_unit, 850, 300, params, &index);
        for( qpid::types::Variant::List::iterator it=datas.begin(); it!=datas.end(); it++ )
        {
            data = (*it).asMap();
            addMultiGraphParameters(data, params, &index);
        }
    }
    else if( datas.size()==1 )
    {
        //single graph
        //alloc memory
        num_params = 14 + 12; //14 specific graph parameters + 12 default parameters
        params = (char**)malloc(sizeof(char*) * num_params);
        data = datas.front().asMap();

        //add graph parameters
        addDefaultParameters(start, end, data["vertical_unit"], 850, 300, params, &index);
        addSingleGraphParameters(data, params, &index);
    }
    else
    {
        //no data
        cerr << "agodatalogger-RRDtool: no data" << endl;
        return false;
    }
    dumpGraphParams((const char**)params, num_params);

    //build graph
    bool found = false;
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

    if( !found ) 
    {
        cerr << "agodatalogger-RRDtool: no image generated by rrd_graph_v command" << endl;
        return false;
    }

    //free memory
    for( int i=0; i<num_params; i++ )
    {
        free(params[i]);
        params[i] = NULL;
    }
    free(params);

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
        if( enableSql )
        {
            eventHandlerSQLite(subject, content["uuid"].asString(), content);
        }

        if( enableRrd )
        {
            eventHandlerRRDtool(subject, content["uuid"].asString(), content);
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
            if( generateGraph(content["devices"].asList(), content["start"].asInt32(), content["end"].asInt32(), &img, &size) )
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
    std::string optString = getConfigOption("datalogger", "enableSql", "1");
    int r;
    if(sscanf(optString.c_str(), "%d", &r) == 1) {
        enableSql = (r == 1);
    }
    optString = getConfigOption("datalogger", "enableRrd", "1");
    if(sscanf(optString.c_str(), "%d", &r) == 1) {
        enableRrd = (r == 1);
    }
    if( enableSql )
        cout << "SQL logging enabled" << endl;
    else
        cout << "SQL logging disabled" << endl;
    if( enableRrd )
        cout << "RRDtool logging enabled" << endl;
    else
        cout << "RRDtool logging disabled" << endl;

    agoConnection->run();
    return 0;
}
