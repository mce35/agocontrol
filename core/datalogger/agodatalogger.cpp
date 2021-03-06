#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>

#include <string>
#include <iostream>
#include <sstream>
#include <cerrno>

#include <cppdb/frontend.h>
#include <stdarg.h>

#include <boost/date_time/posix_time/time_parsers.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <json/writer.h>
#include <json/reader.h>
#include <boost/algorithm/string/predicate.hpp>

#include <rrd.h>
#include "base64.h"

#include "agoapp.h"

#ifndef DBFILE
#define DBFILE "datalogger.db"
#endif

#ifndef DEVICEMAPFILE
#define DEVICEMAPFILE "/maps/datalogger.json"
#endif

#define JOURNAL_ALL "all"
#define JOURNAL_DEBUG "debug"
#define JOURNAL_INFO "info"
#define JOURNAL_WARNING "warning"
#define JOURNAL_ERROR "error"

using namespace std;
using namespace agocontrol;
using namespace qpid::types;
using namespace boost::posix_time;
using namespace boost::gregorian;
namespace fs = ::boost::filesystem;

using namespace agocontrol;

class AgoDataLogger: public AgoApp {
private:
    cppdb::session sql;
    std::string dbname;
    //inventory
    void updateInventory();
    bool checkInventory();

    //utils
    std::string string_format(const std::string fmt, ...);
    int string_real_length(const std::string str);
    std::string string_prepend_spaces(std::string source, size_t newSize);
    static void debugSqlite(void* foo, const char* msg);
    void computeRendering();
    void commandGetData(qpid::types::Variant::Map& content, qpid::types::Variant::Map& returnData);
    bool commandGetGraph(qpid::types::Variant::Map& content, qpid::types::Variant::Map& returnData);

    //database
    bool createTableIfNotExist(string tablename, list<string> createqueries);
    qpid::types::Variant::Map getDatabaseInfos();
    bool purgeTable(std::string table, int timestamp);
    bool isTablePurgeAllowed(std::string table);
    void getGraphData(qpid::types::Variant::List uuids, int start, int end, string environment, qpid::types::Variant::Map& result);
    bool getGraphDataFromSqlite(qpid::types::Variant::List uuids, int start, int end, string environment, qpid::types::Variant::Map& result);
    bool getGraphDataFromRrd(qpid::types::Variant::List uuids, int start, int end, qpid::types::Variant::Map& result);

    //rrd
    bool prepareGraph(std::string uuid, int multiId, qpid::types::Variant::Map& data);
    void dumpGraphParams(const char** params, const int num_params);
    bool addGraphParam(const std::string& param, char** params, int* index);
    void addDefaultParameters(int start, int end, string vertical_unit, int width, int height, char** params, int* index);
    void addDefaultThumbParameters(int duration, int width, int height, char** params, int* index);
    void addSingleGraphParameters(qpid::types::Variant::Map& data, char** params, int* index);
    void addMultiGraphParameters(qpid::types::Variant::Map& data, char** params, int* index);
    void addThumbGraphParameters(qpid::types::Variant::Map& data, char** params, int* index);
    bool generateGraph(qpid::types::Variant::List uuids, int start, int end, int thumbDuration, unsigned char** img, unsigned long* size);

    //journal
    bool eventHandlerJournal(std::string message, std::string type);
    bool getMessagesFromJournal(qpid::types::Variant::Map& content, qpid::types::Variant::Map& messages);
        
    //system
    void saveDeviceMapFile();
    qpid::types::Variant::Map commandHandler(qpid::types::Variant::Map content);
    void eventHandler(std::string subject, qpid::types::Variant::Map content);
    void eventHandlerRRDtool(std::string subject, std::string uuid, qpid::types::Variant::Map content);
    void eventHandlerSQL(std::string subject, std::string uuid, qpid::types::Variant::Map content);
    void dailyPurge();
    void setupApp();
public:
    AGOAPP_CONSTRUCTOR(AgoDataLogger);
};

qpid::types::Variant::Map inventory;
qpid::types::Variant::Map units;
bool dataLogging = 1;
bool gpsLogging = 1;
bool rrdLogging = 1;
int purgeDelay = 0; //in months
std::string desiredRendering = "plots"; // could be "image" or "plots"
std::string rendering = "plots";
//GraphDataSource graphDataSource = SQLITE;
const char* colors[] = {"#800080", "#0000FF", "#008000", "#FF00FF", "#000080", "#FF0000", "#00FF00", "#00FFFF", "#800000", "#808000", "#008080", "#C0C0C0", "#808080", "#000000", "#FFFF00"};
qpid::types::Variant::Map devicemap;
qpid::types::Variant::List allowedPurgeTables;

std::string uuidToName(std::string uuid) {
    qpid::types::Variant::Map devices = inventory["inventory"].asMap();
    if (devices[uuid].isVoid()) {
        return "";
    }
    qpid::types::Variant::Map device = devices[uuid].asMap();
    return device["name"].asString() == "" ? uuid : device["name"].asString();
}

/**
 * Update inventory
 */
void AgoDataLogger::updateInventory()
{
    bool unitsLoaded = false;

    //test unit content
    if( units.size()>0 )
        unitsLoaded = true;

    //get inventory
    inventory = agoConnection->getInventory();

    //get units
    if( !unitsLoaded && !inventory["schema"].isVoid() )
    {
        for( qpid::types::Variant::Map::iterator it=inventory["schema"].asMap()["units"].asMap().begin(); it!=inventory["schema"].asMap()["units"].asMap().end(); it++ )
        {
            units[it->first] = it->second.asMap()["label"].asString();
        }
    }
}

/**
 * Check inventory
 */
bool AgoDataLogger::checkInventory()
{
    if( inventory["devices"].isVoid() )
    {
        //inventory is empty
        return false;
    }
    return true;
}

bool AgoDataLogger::createTableIfNotExist(string tablename, list<string> createqueries) {
    try {
        cppdb::result r;
        if (sql.driver() == "sqlite3") {
            AGO_TRACE() << "checking existance of table in sqlite: " << tablename;
            r = sql<< "SELECT name FROM sqlite_master WHERE type='table' AND name = ?" << tablename << cppdb::row;
        } else {
            AGO_TRACE() << "checking existance of table in non-sqlite: " << tablename;
            r = sql << "SELECT * FROM information_schema.tables WHERE table_schema = ? AND table_name = ? LIMIT 1" << dbname << tablename << cppdb::row;
        }
        if (r.empty()) {
            AGO_INFO() << "Creating missing table '" << tablename << "'";
            for( list<string>::iterator it=createqueries.begin(); it!=createqueries.end(); it++ ) {
                sql << (*it) << cppdb::exec;
            }
            createqueries.clear();
        }
    } catch(std::exception const &e) {
        AGO_ERROR() << "Sql exception: " << e.what();
    }
    return true;
}

/**
 * Save device map file
 */
void AgoDataLogger::saveDeviceMapFile()
{
    fs::path dmf = getConfigPath(DEVICEMAPFILE);
    variantMapToJSONFile(devicemap, dmf);
}

/**
 * Format string to specified format
 */
std::string AgoDataLogger::string_format(const std::string fmt, ...) {
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
int AgoDataLogger::string_real_length(const std::string str)
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
 * Prepend to source string until source size reached
 */
std::string AgoDataLogger::string_prepend_spaces(std::string source, size_t newSize)
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
bool AgoDataLogger::prepareGraph(std::string uuid, int multiId, qpid::types::Variant::Map& data)
{
    //check params
    if( multiId>=15 )
    {
        //no more color available
        AGO_ERROR() << "agodatalogger-RRDtool: no more color available";
        return false;
    }

    //filename
    stringstream filename;
    filename << uuid << ".rrd";

    //get device infos from inventory
    if( !checkInventory() )
    {
        //unable to continue
        AGO_ERROR() << "agodatalogger-RRDtool: no inventory available";
        return false;
    }
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
        replaceString(kind, "meter", "");
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
        if( !device["values"].isVoid() )
        {
            qpid::types::Variant::Map values = device["values"].asMap();
            for( qpid::types::Variant::Map::const_iterator it=values.begin(); it!=values.end(); it++ )
            {
                if( it->first==kind || it->first=="batterylevel" )
                {
                    qpid::types::Variant::Map infos = it->second.asMap();
                    if( !infos["unit"].isVoid() )
                    {
                        vertical_unit = infos["unit"].asString();
                    }
                    break;
                }
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
            else if( device["devicetype"].asString()=="energysensor" || device["devicetype"].asString()=="powersensor" || 
                    device["devicetype"].asString()=="powermeter" || device["devicetype"].asString()=="batterysensor" )
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
        AGO_ERROR() << "agodatalogger-RRDtool: device not found";
        return false;
    }

    return true;
}

/**
 * Display params (debug purpose)
 */
void AgoDataLogger::dumpGraphParams(const char** params, const int num_params)
{
    AGO_TRACE() << "Dump graph parameters (" << num_params << " params) :";
    for( int i=0; i<num_params; i++ )
    {
        AGO_TRACE() << " - " << string(params[i]);
    }
}

/**
 * Add graph param
 */
bool AgoDataLogger::addGraphParam(const std::string& param, char** params, int* index)
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
void AgoDataLogger::addDefaultParameters(int start, int end, string vertical_unit, int width, int height, char** params, int* index)
{
    //first params
    addGraphParam("dummy", params, index);
    addGraphParam("-", params, index);

    addGraphParam("--slope-mode", params, index);

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
 * Add default thumb graph parameters
 */
void AgoDataLogger::addDefaultThumbParameters(int duration, int width, int height, char** params, int* index)
{
    //first params
    addGraphParam("dummy", params, index);
    addGraphParam("-", params, index);

    //start
    addGraphParam("--start", params, index);
    addGraphParam(string_format("end-%dh", duration), params, index);

    //end
    addGraphParam("--end", params, index);
    addGraphParam("now", params, index);

    //size
    addGraphParam("--width", params, index);
    addGraphParam(string_format("%d", width), params, index);
    addGraphParam("--height", params, index);
    addGraphParam(string_format("%d", height), params, index);

    //legend
    addGraphParam("--no-legend", params, index);
    //addGraphParam("--y-grid", params, index);
    //addGraphParam("none", params, index);
    addGraphParam("--x-grid", params, index);
    addGraphParam("none", params, index);
}

/**
 * Add single graph parameters
 */
void AgoDataLogger::addSingleGraphParameters(qpid::types::Variant::Map& data, char** params, int* index)
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
void AgoDataLogger::addMultiGraphParameters(qpid::types::Variant::Map& data, char** params, int* index)
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

    //AVG GPRINT
    addGraphParam(string_format("GPRINT:levelavg%d:     Avg %%6.2lf%s", id, data["unit"].asString().c_str()), params, index);

    //LAST GPRINT
    addGraphParam(string_format("GPRINT:levellast%d:     Last %%6.2lf%s", id, data["unit"].asString().c_str()), params, index);

    //new line
    addGraphParam("COMMENT:\\n", params, index);
}

/**
 * Add thumb graph parameters
 */
void AgoDataLogger::addThumbGraphParameters(qpid::types::Variant::Map& data, char** params, int* index)
{
    //keep index value
    int id = (*index);

    //DEF
    fs::path rrdfile = getLocalStatePath(data["filename"].asString());
    addGraphParam(string_format("DEF:level%d=%s:level:AVERAGE", id, rrdfile.c_str()), params, index);

    //GFX LINE
    addGraphParam(string_format("LINE1:level%d%s:%s", id, data["colorL"].asString().c_str(), data["kind"].asString().c_str()), params, index);
}

/**
 * Generate RRDtool graph
 */
bool AgoDataLogger::generateGraph(qpid::types::Variant::List uuids, int start, int end, int thumbDuration, unsigned char** img, unsigned long* size)
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
        int defaultNumParam = 13;
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
                    defaultNumParam = 11;
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

        if( thumbDuration<=0 )
        {
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
        else
        {
            //alloc memory
            num_params = 2 * datas.size() + 13;
            params = (char**)malloc(sizeof(char*) * num_params);

            //add graph parameters
            addDefaultThumbParameters(thumbDuration, 250, 40, params, &index);
            for( qpid::types::Variant::List::iterator it=datas.begin(); it!=datas.end(); it++ )
            {
                data = (*it).asMap();
                addThumbGraphParameters(data, params, &index);
            }
        }
    }
    else if( datas.size()==1 )
    {
        //single graph
        data = datas.front().asMap();
        if( thumbDuration<=0 )
        {
            //alloc memory
            num_params = 14 + 13; //14 specific graph parameters + 13 default parameters
            params = (char**)malloc(sizeof(char*) * num_params);

            //add graph parameters
            addDefaultParameters(start, end, data["vertical_unit"], 850, 300, params, &index);
            addSingleGraphParameters(data, params, &index);
        }
        else
        {
            //alloc memory
            num_params = 2 * datas.size() + 13;
            params = (char**)malloc(sizeof(char*) * num_params);

            //add graph parameters
            addDefaultThumbParameters(thumbDuration, 250, 40, params, &index);
            addThumbGraphParameters(data, params, &index);
        }
    }
    else
    {
        //no data
        AGO_ERROR() << "agodatalogger-RRDtool: no data";
        return false;
    }
    num_params = index;
    dumpGraphParams((const char**)params, num_params);

    //build graph
    bool found = false;
    rrd_clear_error();
    rrd_info_t* grinfo = rrd_graph_v(num_params, (char**)params);
    rrd_info_t* walker;
    if( grinfo!=NULL )
    {
        walker = grinfo;
        while (walker)
        {
            AGO_TRACE() << "RRD walker key = " << walker->key;
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
        AGO_ERROR() << "agodatalogger-RRDtool: unable to generate graph [" << rrd_get_error() << "]";
        return false;
    }

    if( !found ) 
    {
        AGO_ERROR() << "agodatalogger-RRDtool: no image generated by rrd_graph_v command";
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
void AgoDataLogger::eventHandlerRRDtool(std::string subject, std::string uuid, qpid::types::Variant::Map content)
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
            AGO_INFO() << "New device detected, create rrdfile " << rrdfile.string();
            const char *params[] = {"DS:level:GAUGE:21600:U:U", "RRA:AVERAGE:0.5:1:1440", "RRA:AVERAGE:0.5:5:2016", "RRA:AVERAGE:0.5:30:1488", "RRA:AVERAGE:0.5:60:8760", "RRA:AVERAGE:0.5:360:2920", "RRA:MIN:0.5:1:1440", "RRA:MIN:0.5:5:2016", "RRA:MIN:0.5:30:1488", "RRA:MIN:0.5:60:8760", "RRA:MIN:0.5:360:2920", "RRA:MAX:0.5:1:1440", "RRA:MAX:0.5:5:2016", "RRA:MAX:0.5:30:1488", "RRA:MAX:0.5:60:8760", "RRA:MAX:0.5:360:2920"};

            rrd_clear_error();
            int res = rrd_create_r(rrdfile.string().c_str(), 60, 0, 16, params);
            if( res<0 )
            {
                AGO_ERROR() << "agodatalogger-RRDtool: unable to create rrdfile [" << rrd_get_error() << "]";
            }
        }  

        //update rrd
        if( fs::exists(rrdfile) )
        {
            char param[50];
            snprintf(param, 50, "N:%s", content["level"].asString().c_str());
            const char* params[] = {param};

            rrd_clear_error();
            int res = rrd_update_r(rrdfile.string().c_str(), "level", 1, params);
            if( res<0 )
            {
                AGO_ERROR() << "agodatalogger-RRDtool: unable to update data [" << rrd_get_error() << "] with param [" << param << "]";
            }
        }
    }
}

/**
 * Store event data into SQL database
 */
void AgoDataLogger::eventHandlerSQL(std::string subject, std::string uuid, qpid::types::Variant::Map content)
{
    string result;

    if( gpsLogging && subject=="event.environment.positionchanged" && content["latitude"].asString()!="" && content["longitude"].asString()!="" )
    {
        AGO_DEBUG() << "specific environment case: position";
        string lat = content["latitude"].asString();
        string lon = content["longitude"].asString();
        try {
            sql <<  "INSERT INTO position VALUES(null, ?, ?, ?, ?)" << uuid << lat << lon << (int)time(NULL) << cppdb::exec;
        } catch(std::exception const &e) {
            AGO_ERROR() << "Sql error: "  << e.what();
            return;
        }
    }
    else if( dataLogging && content["level"].asString() != "")
    {
        replaceString(subject, "event.environment.", "");
        replaceString(subject, "event.device.", "");
        replaceString(subject, "changed", "");
        replaceString(subject, "event.", "");
        replaceString(subject, "security.sensortriggered", "state"); // convert security sensortriggered event to state

        try {
            cppdb::statement stat = sql << "INSERT INTO data VALUES(null, ?, ?, ?, ?)";

            string level = content["level"].asString();
            stat.bind(uuid);
            stat.bind(subject);

            double value;
            switch(content["level"].getType()) {
                case qpid::types::VAR_DOUBLE:
                    value = content["level"].asDouble();
                    stat.bind(value);
                    break;
                case qpid::types::VAR_FLOAT:
                    value = content["level"].asFloat();
                    stat.bind(value);
                    break;
                default:
                    stat.bind(level);
            }

            stat.bind(time(NULL));

            stat.exec();
        } catch(std::exception const &e) {
            AGO_ERROR() << "Sql error: "  << e.what();
            return;
        }
    }

}

/**
 * Store journal message into SQLite database
 */
bool AgoDataLogger::eventHandlerJournal(std::string message, std::string type)
{
    try {
        sql <<  "INSERT INTO journal VALUES(null, ?, ?, ?)" << time(NULL) << message << type << cppdb::exec;
    } catch(std::exception const &e) {
        AGO_ERROR() << "Sql error: "  << e.what();
        return false;
    }
    return true;
}

/**
 * Main event handler
 */
void AgoDataLogger::eventHandler(std::string subject, qpid::types::Variant::Map content)
{
    if( subject!="" && !content["uuid"].isVoid() )
    {
        //data logging
        eventHandlerSQL(subject, content["uuid"].asString(), content);

        //rrd logging
        if( rrdLogging )
        {
            eventHandlerRRDtool(subject, content["uuid"].asString(), content);
        }
    }
    else if( subject=="event.environment.timechanged" )
    {
        updateInventory();

        if( !content["hour"].isVoid() && !content["minute"].isVoid() && content["hour"].asInt8()==0 && content["minute"].asInt8()==0 )
        {
            //midnight launch daily purge
            dailyPurge();
        }
    }
}

void AgoDataLogger::debugSqlite(void* foo, const char* msg)
{
    AGO_TRACE() << "SQLITE: " << msg;
}

/**
 * Return graph data from rrd file
 */
//bool AgoDataLogger::GetGraphDataFromRrd(qpid::types::Variant::Map content, qpid::types::Variant::Map &result)
bool AgoDataLogger::getGraphDataFromRrd(qpid::types::Variant::List uuids, int start, int end, qpid::types::Variant::Map& result)
{
    AGO_TRACE() << "getGraphDataFromRrd";

    qpid::types::Variant::List values;
    bool error = false;
    string uuid = uuids.front().asString();
    stringstream filename;
    filename << uuid << ".rrd";
    fs::path rrdfile = getLocalStatePath(filename.str());
    string filenamestr = rrdfile.string();
    time_t startTimet = (time_t)start;;
    time_t endTimet = (time_t)end;
    AGO_TRACE() << "file=" << filenamestr << " start=" << start << " end=" << end;

    unsigned long step = 0;
    unsigned long ds_cnt;
    char** ds_namv;
    rrd_value_t *data = NULL;
    rrd_clear_error();
    int count = 0;
    unsigned long ds = 0;
    //rrd_fetch_r example found here https://github.com/pldimitrov/Rrd/blob/master/src/Rrd.c
    int res = rrd_fetch_r(filenamestr.c_str(), "AVERAGE", &startTimet, &endTimet, &step, &ds_cnt, &ds_namv, &data);
    if( res==0 && data!=NULL )
    {
        int size = (endTimet - startTimet) / step - 1;
        double level = 0;
        for( ds=0; ds<ds_cnt; ds++ )
        {
            for( int i=0; i<size; i++ )
            {
                level = (double)data[ds+i*ds_cnt];
                if( !isnan(level) )
                {
                    count++;
                    qpid::types::Variant::Map value;
                    value["time"] = (uint64_t)startTimet;
                    value["level"] = (double)data[ds+i*ds_cnt];
                    values.push_back(value);
                }
                startTimet += step;
            }   
        }
        AGO_TRACE() << "rrd_fetch returns: step=" << step << " datasource_count=" << ds_cnt << " data_count=" << count;
    }
    else
    {
        AGO_DEBUG() << "Fetch failed: " << rrd_get_error();
        error = true;
    }

    //free memory
    if( data )
        free(data);
    for( unsigned int i=0; i<sizeof(ds_namv)/sizeof(char*); i++ )
        free(ds_namv[i]);
    free(ds_namv);

    AGO_TRACE() << "RRD query returns " << values.size() << " values";

    result["values"] = values;
    return !error;
}

/**
 * Return graph data from sqlite
 */
bool AgoDataLogger::getGraphDataFromSqlite(qpid::types::Variant::List uuids, int start, int end, string environment, qpid::types::Variant::Map& result)
{
    AGO_TRACE() << "getGraphDataFromSqlite: " << environment;
    qpid::types::Variant::List values;
    string uuid = uuids.front().asString();
    try {
        if( environment=="position" )
        {
            AGO_TRACE() << "Execute query on postition table";
            cppdb::result r = sql << "SELECT timestamp, latitude, longitude FROM position WHERE timestamp BETWEEN ? AND ? AND uuid = ? ORDER BY timestamp" << start << end << uuid;
            while(r.next()) {
                qpid::types::Variant::Map value;
                value["time"] = r.get<int>("timestamp");
                value["latitude"] = r.get<double>("latitude");
                value["longitude"] = r.get<double>("longitude");
                values.push_back(value);
            }
        }
        else
        {
            AGO_TRACE() << "Execute query on data table";
            cppdb::result r = sql << "SELECT timestamp, level FROM data WHERE timestamp BETWEEN ? AND ? AND environment = ? AND uuid = ? ORDER BY timestamp" << start << end << environment << uuid;
            while(r.next()) {
                qpid::types::Variant::Map value;
                value["time"] = r.get<int>("timestamp");
                value["level"] = r.get<double>("level");
                values.push_back(value);
            }
        }
    } catch (std::exception const &e) {
        AGO_ERROR() << "SQL Error: " << e.what();
        return false;
    }
    AGO_TRACE() << "SQL query returns " << values.size() << " values";

    result["values"] = values;
    return true;
}

/**
 * Return data for graph generation
 */
void AgoDataLogger::getGraphData(qpid::types::Variant::List uuids, int start, int end, string environment, qpid::types::Variant::Map& result)
{
    if( dataLogging )
    {
        getGraphDataFromSqlite(uuids, start, end, environment, result);
    }
    else if( environment=="position" )
    {
        //force values retrieving from database
        getGraphDataFromSqlite(uuids, start, end, environment, result);
    }
    else if( rrdLogging )
    {
        getGraphDataFromRrd(uuids, start, end, result);
    }
    else
    {
        qpid::types::Variant::List empty;
        result["values"] = empty;
    }
}

/**
 * Return messages from journal
 * datetime format: 2015-07-12T22:00:00.000Z
 */
bool AgoDataLogger::getMessagesFromJournal(qpid::types::Variant::Map& content, qpid::types::Variant::Map& result)
{
    qpid::types::Variant::List messages;
    std::string filter = "";
    std::string type = "";

    //handle filter
    if( !content["filter"].isVoid() )
    {
        filter = content["filter"].asString();
        //append jokers
        filter = "%" + filter + "%";
    }

    //handle type
    if( !content["type"].isVoid() )
    {
        if( content["type"]==JOURNAL_ALL )
        {
            type = "%%";
        }
        else
        {
            type = content["type"].asString();
        }
    }


    //parse the timestrings
    string startDate = content["start"].asString();
    string endDate = content["end"].asString();
    replaceString(startDate, "-", "");
    replaceString(startDate, ":", "");
    replaceString(startDate, "Z", "");
    replaceString(endDate, "-", "");
    replaceString(endDate, ":", "");
    replaceString(endDate, "Z", "");
    boost::posix_time::ptime base(boost::gregorian::date(1970, 1, 1));
    boost::posix_time::time_duration start = boost::posix_time::from_iso_string(startDate) - base;
    boost::posix_time::time_duration end = boost::posix_time::from_iso_string(endDate) - base;

    AGO_TRACE() << "getMessagesFromJournal: start=" << start.total_seconds() << " end=" << end.total_seconds() << " filter=" << filter << " type=" << type;
    try {
        cppdb::result r = sql <<  "SELECT timestamp, message, type FROM journal WHERE timestamp BETWEEN ? AND ? AND message LIKE ? AND type LIKE ? ORDER BY timestamp DESC" << start.total_seconds() << end.total_seconds() << filter << type;
        while (r.next()) {
            qpid::types::Variant::Map value;
            value["time"] = r.get<int>("timestamp");
            value["message"] = r.get<std::string>("message");
            value["type"] = r.get<std::string>("type");
            messages.push_back(value);
        }
    } catch (std::exception const &e) {
        AGO_ERROR() << "SQL Error: " << e.what();
        return false;
    }

    AGO_TRACE() << "Query returns " << messages.size() << " messages";

    //prepare result
    result["messages"] = messages;

    return true;
}

/**
 * Return some information about database (size, date of first entry...)
 */
qpid::types::Variant::Map AgoDataLogger::getDatabaseInfos()
{
    qpid::types::Variant::Map returnval;
    returnval["data_start"] = 0;
    returnval["data_end"] = 0;
    returnval["data_count"] = 0;
    returnval["position_start"] = 0;
    returnval["position_end"] = 0;
    returnval["position_count"] = 0;
    returnval["journal_start"] = 0;
    returnval["journal_end"] = 0;
    returnval["journal_count"] = 0;

    //get data time range
    try {
        cppdb::result r = sql <<  "SELECT MIN(timestamp) AS min, MAX(timestamp) AS max FROM data" << cppdb::row;
        if (!r.empty()) {
            returnval["data_start"] = r.get<int>("min");
            returnval["data_end"] = r.get<int>("max");
        }
    } catch (std::exception const &e) {
        AGO_ERROR() << "SQL Error: " << e.what();
    }

    //get data count
    try {
        cppdb::result r = sql <<  "SELECT COUNT(id) AS count FROM data" << cppdb::row;
        if (!r.empty()) {
            returnval["data_count"] = r.get<int64_t>("count");
        }
    } catch (std::exception const &e) {
        AGO_ERROR() << "SQL Error: " << e.what();
    }

    //get position time range
    try {
        cppdb::result r = sql <<  "SELECT MIN(timestamp) AS min, MAX(timestamp) AS max FROM position" << cppdb::row;
        if (!r.empty()) {
            returnval["position_start"] = r.get<int>("min");
            returnval["position_end"] = r.get<int>("max");
        }
    } catch (std::exception const &e) {
        AGO_ERROR() << "SQL Error: " << e.what();
    }

    //get position count
    try {
        cppdb::result r = sql <<  "SELECT COUNT(id) AS count FROM position" << cppdb::row;
        if (!r.empty()) {
            returnval["position_count"] = r.get<int64_t>("count");
        }
    } catch (std::exception const &e) {
        AGO_ERROR() << "SQL Error: " << e.what();
    }

    //get journal time range
    try {
        cppdb::result r = sql <<  "SELECT MIN(timestamp) AS min, MAX(timestamp) AS max FROM journal" << cppdb::row;
        if (!r.empty()) {
            returnval["journal_start"] = r.get<int>("min");
            returnval["journal_end"] = r.get<int>("max");
        }
    } catch (std::exception const &e) {
        AGO_ERROR() << "SQL Error: " << e.what();
    }

    //get journal count
    try {
        cppdb::result r = sql <<  "SELECT COUNT(id) AS count FROM journal" << cppdb::row;
        if (!r.empty()) {
            returnval["journal_count"] = r.get<int64_t>("count");
        }
    } catch (std::exception const &e) {
        AGO_ERROR() << "SQL Error: " << e.what();
    }

    return returnval;
}

/**
 * Purge specified table before specified timestamp.
 * If timestamp=0 all table content is purged
 */
bool AgoDataLogger::purgeTable(std::string table, int timestamp=0)
{
    stringstream query;
    query << "DELETE FROM " << table;

    if( timestamp!=0 )
    {
        query << " WHERE timestamp<" << timestamp;
    }

    AGO_TRACE() << "purgeTable query: " << query.str();
    try {
        sql << query.str() << cppdb::exec;
        //vacuum database
        if (sql.driver() == "sqlite3") sql << "VACUUM" << cppdb::exec;
    } catch(std::exception const &e) {
        AGO_ERROR() << "SQL Error: " << e.what();
        return false;
    }
    return true;
}

/**
 * Return specified datetime as database format
 * Datetime format: 2015-07-12T22:00:00.000Z
 */
std::string dateToDatabaseFormat(boost::posix_time::ptime pt)
{
  std::string s;
  std::ostringstream datetime_ss;
  time_facet * p_time_output = new time_facet;
  std::locale special_locale (std::locale(""), p_time_output);
  datetime_ss.imbue (special_locale);
  (*p_time_output).format("%Y-%m-%dT%H:%M:%SZ");
  datetime_ss << pt;
  s = datetime_ss.str().c_str();
  return s;

}

/**
 * Table can be purged?
 */
bool AgoDataLogger::isTablePurgeAllowed(std::string table)
{
    for( qpid::types::Variant::List::iterator it=allowedPurgeTables.begin(); it!=allowedPurgeTables.end(); it++ )
    {
        if( (*it)==table )
        {
            return true;
        }
    }
    return false;
}

/**
 * Execute daily purge on tables
 */
void AgoDataLogger::dailyPurge()
{
    if( purgeDelay>0 )
    {
        //get current timestamp
        int timestamp = time(NULL);

        //decrease current timestamp with number of configured months
        timestamp -= purgeDelay * 2628000;

        //get infos before purge
        qpid::types::Variant::Map before = getDatabaseInfos();

        //purge tables
        for( qpid::types::Variant::List::iterator it=allowedPurgeTables.begin(); it!=allowedPurgeTables.end(); it++ )
        {
            purgeTable((*it), timestamp);
        }

        //get infos after purge
        qpid::types::Variant::Map after = getDatabaseInfos();

        //log infos
        int dataCount = before["data_count"].asInt32() - after["data_count"].asInt32();
        int journalCount = before["journal_count"].asInt32() - after["journal_count"].asInt32();
        int positionCount = before["position_count"].asInt32() - after["position_count"].asInt32();
        AGO_INFO() << "Daily purge removed " << dataCount << " from data table, " << positionCount << " from position table, " << journalCount << " from journal table";
    }
    else
    {
        //purge disabled
        AGO_DEBUG() << "Daily database purge disabled";
    }
}

/**
 * Compute real rendering value according to user preferences
 */
void AgoDataLogger::computeRendering()
{
    if( !dataLogging && !rrdLogging )
    {
        //no choice: no rendering
        rendering = "none";
    }
    else if( dataLogging && !rrdLogging )
    {
        //only plots rendering available
        rendering = "plots";
    }
    else if( !dataLogging && rrdLogging )
    {
        //rrd enabled, both plots and image available, use user preference
        rendering = desiredRendering;
    }
    else
    {
        //both rrd and data logging are enabled, use user preference
        rendering = desiredRendering;
    }
    AGO_DEBUG() << "Computed rendering: " << rendering;
}

/**
 * "getdata" and "getrawdata" commands handler
 */
void AgoDataLogger::commandGetData(qpid::types::Variant::Map& content, qpid::types::Variant::Map& returnData)
{
    AGO_TRACE() << "commandGetData";

    //check parameters
    checkMsgParameter(content, "start", VAR_INT32);
    checkMsgParameter(content, "end", VAR_INT32);
    checkMsgParameter(content, "devices", VAR_LIST);

    //variables
    qpid::types::Variant::List uuids;
    uuids = content["devices"].asList();
    string environment = "";
    if( !content["env"].isVoid() )
    {
        environment = content["env"].asString();
    }

    //get data
    getGraphData(uuids, content["start"].asInt32(), content["end"].asInt32(), environment, returnData);
}

/**
 * "getgraph" command handler
 */
bool AgoDataLogger::commandGetGraph(qpid::types::Variant::Map& content, qpid::types::Variant::Map& returnData)
{
    AGO_TRACE() << "commandGetGraph";

    //check parameters
    checkMsgParameter(content, "start", VAR_INT32);
    checkMsgParameter(content, "end", VAR_INT32);
    checkMsgParameter(content, "devices", VAR_LIST);

    //variables
    unsigned char* img = NULL;
    unsigned long size = 0;
    qpid::types::Variant::List uuids;
    uuids = content["devices"].asList();

    //is a multigraph?
    if( uuids.size()==1 )
    {
        string internalid = agoConnection->uuidToInternalId((*uuids.begin()).asString());
        if( internalid.length()>0 && !devicemap["multigraphs"].asMap()[internalid].isVoid() )
        {
            uuids = devicemap["multigraphs"].asMap()[internalid].asMap()["uuids"].asList();
        }
    }

    //get image
    if( generateGraph(uuids, content["start"].asInt32(), content["end"].asInt32(), 0, &img, &size) )
    {
        returnData["graph"] = base64_encode(img, size);
        return true;
    }
    else
    {
        //error generating graph
        return false;
    }
}

/**
 * Command handler
 */
qpid::types::Variant::Map AgoDataLogger::commandHandler(qpid::types::Variant::Map content)
{
    qpid::types::Variant::Map returnData;
    std::string internalid = content["internalid"].asString();
    if (internalid == "dataloggercontroller")
    {
        if( (content["command"]=="getdata" || content["command"]=="getgraph") && rendering=="none" )
        {
            //no data storage, nothing to return
            returnData["rendering"] = rendering;
            return responseSuccess(returnData);
        }
        else if( content["command"]=="getdata" )
        {
            //get data according to user preferences
            
            //is multigraph?
            bool isMultigraph = false;
            checkMsgParameter(content, "devices", VAR_LIST);
            qpid::types::Variant::List uuids = content["devices"].asList();
            if( uuids.size()==1 )
            {
                string internalid = agoConnection->uuidToInternalId((*uuids.begin()).asString());
                if( internalid.length()>0 && !devicemap["multigraphs"].asMap()[internalid].isVoid() )
                {
                    isMultigraph = true;
                }
            }
            
            if( isMultigraph || rendering=="image" )
            {
                //render image
                //multigraph only implemented on rrdtool for now
                if( commandGetGraph(content, returnData) )
                {
                    returnData["rendering"] = "image";
                    return responseSuccess(returnData);
                }
                else
                {
                    return responseFailed("Failed to generate graph");
                }
                
            }
            else
            {
                //render plots
                commandGetData(content, returnData);
                returnData["rendering"] = "plots";
                return responseSuccess(returnData);
            }
        }
        else if( content["command"]=="getrawdata" )
        {
            //explicitely request raw data (device values)
            commandGetData(content, returnData);
            returnData["rendering"] = "raw";
            return responseSuccess(returnData);
        }
        else if( content["command"]=="getgraph" )
        {
            //explicitely request an image
            if( commandGetGraph(content, returnData) )
            {
                returnData["rendering"] = "image";
                return responseSuccess(returnData);
            }
            else
            {
                return responseFailed("Failed to generate graph");
            }
        }
        else if (content["command"] == "getdeviceenvironments")
        {
            try {
                cppdb::result r = sql << "SELECT distinct uuid, environment FROM data";
                while (r.next()) {
                    returnData[r.get<std::string>("uuid")]=r.get<std::string>("environment");
                }
            } catch(std::exception const &e) {
                AGO_ERROR() << "SQL Error: " << e.what();
                return responseFailed("SQL Error");
            }
            return responseSuccess(returnData);
        }
        else if( content["command"]=="getconfig" )
        {
            //add multigrahs
            qpid::types::Variant::List multis;
            if( !devicemap["multigraphs"].isVoid() )
            {
                for( qpid::types::Variant::Map::iterator it=devicemap["multigraphs"].asMap().begin(); it!=devicemap["multigraphs"].asMap().end(); it++ )
                {
                    qpid::types::Variant::Map multi;
                    if( !it->second.isVoid() && !it->second.asMap()["uuids"].isVoid() )
                    {
                        multi["name"] = it->first;
                        multi["uuids"] = it->second.asMap()["uuids"].asList();
                        multis.push_back(multi);
                    }
                }
            }

            //database infos
            qpid::types::Variant::Map db;
            fs::path dbpath = ensureParentDirExists(getLocalStatePath(DBFILE));
            struct stat stat_buf;
            int rc = stat(dbpath.c_str(), &stat_buf);
            db["size"] = (rc==0 ? (int)stat_buf.st_size : 0);
            db["infos"] = getDatabaseInfos();

            returnData["error"] = 0;
            returnData["msg"] = "";
            returnData["multigraphs"] = multis;
            returnData["dataLogging"] = dataLogging ? 1 : 0;
            returnData["gpsLogging"] = gpsLogging ? 1 : 0;
            returnData["rrdLogging"] = rrdLogging ? 1 : 0;
            returnData["purgeDelay"] = purgeDelay;
            returnData["rendering"] = desiredRendering;
            returnData["database"] = db;
            return responseSuccess(returnData);
        }
        else if( content["command"]=="addmultigraph" )
        {
            checkMsgParameter(content, "uuids", VAR_LIST);
            checkMsgParameter(content, "period", VAR_INT32);

            string internalid = "multigraph" + string(devicemap["nextid"]);
            if( agoConnection->addDevice(internalid.c_str(), "multigraph") )
            {
                devicemap["nextid"] = devicemap["nextid"].asInt32() + 1;
                qpid::types::Variant::Map device;
                device["uuids"] = content["uuids"].asList();
                device["period"] = content["period"].asInt32();
                devicemap["multigraphs"].asMap()[internalid] = device;
                saveDeviceMapFile();
                return responseSuccess("Multigraph " + internalid + " created successfully");
            }
            else
            {
                AGO_ERROR() << "Unable to add new multigraph";
                return responseFailed("Failed to add device");
            }
        }
        else if( content["command"]=="deletemultigraph" )
        {
            checkMsgParameter(content, "multigraph", VAR_STRING);

            string internalid = content["multigraph"].asString();
            if( agoConnection->removeDevice(internalid.c_str()) )
            {
                devicemap["multigraphs"].asMap().erase(internalid);
                saveDeviceMapFile();
                return responseSuccess("Multigraph " + internalid + " deleted successfully");
            }
            else
            {
                AGO_ERROR() << "Unable to delete multigraph " << internalid;
                return responseFailed("Failed to delete graph");
            }
        }
        else if( content["command"]=="getthumb" )
        {
            checkMsgParameter(content, "multigraph", VAR_STRING);

            //check if inventory available
            if( !checkInventory() )
            {
                //force inventory update during getthumb command because thumb requests are performed during first ui loading
                updateInventory();
            }

            string internalid = content["multigraph"].asString();
            if( !devicemap["multigraphs"].isVoid() && !devicemap["multigraphs"].asMap()[internalid].isVoid() && !devicemap["multigraphs"].asMap()[internalid].asMap()["uuids"].isVoid() )
            {
                unsigned char* img = NULL;
                unsigned long size = 0;
                int period = 12;

                //get thumb period
                if( !devicemap["multigraphs"].asMap()[internalid].asMap()["period"].isVoid() )
                {
                    period = devicemap["multigraphs"].asMap()[internalid].asMap()["period"].asInt32();
                }

                if( generateGraph(devicemap["multigraphs"].asMap()[internalid].asMap()["uuids"].asList(), 0, 0, period, &img, &size) )
                {
                    returnData["graph"] = base64_encode(img, size);
                    return responseSuccess(returnData);
                }
                else
                {
                    //error generating graph
                    return responseFailed("Internal error");
                }
            }
            else
            {
                AGO_ERROR() << "Unable to get thumb: it seems multigraph '" << internalid << "' doesn't exist";
                return responseFailed("Unknown multigraph");
            }
        }
        else if( content["command"]=="setconfig" )
        {
            checkMsgParameter(content, "dataLogging", VAR_BOOL);
            checkMsgParameter(content, "rrdLogging", VAR_BOOL);
            checkMsgParameter(content, "gpsLogging", VAR_BOOL);
            checkMsgParameter(content, "purgeDelay", VAR_INT32);
            checkMsgParameter(content, "rendering", VAR_STRING);

            bool error = false;
            if( content["dataLogging"].asBool() )
            {
                dataLogging = true;
                AGO_INFO() << "Data logging enabled";
            }
            else
            {
                dataLogging = false;
                AGO_INFO() << "Data logging disabled";
            }
            if( !setConfigOption("dataLogging", dataLogging) )
            {
                AGO_ERROR() << "Unable to save dataLogging status to config file";
                error = true;
            }

            if( !error )
            {
                if( content["gpsLogging"].asBool() )
                {
                    gpsLogging = true;
                    AGO_INFO() << "GPS logging enabled";
                }
                else
                {
                    gpsLogging = false;
                    AGO_INFO() << "GPS logging disabled";
                }
                if( !setConfigOption("gpsLogging", gpsLogging) )
                {
                    AGO_ERROR() << "Unable to save gpsLogging status to config file";
                    error = true;
                }
            }
    
            if( !error )
            {
                if( content["rrdLogging"].asBool() )
                {
                    rrdLogging = true;
                    AGO_INFO() << "RRD logging enabled";
                }
                else
                {
                    rrdLogging = false;
                    AGO_INFO() << "RRD logging disabled";
                }
                if( !setConfigOption("rrdLogging", rrdLogging) )
                {
                    AGO_ERROR() << "Unable to save rrdLogging status to config file";
                    error = true;
                }
            }

            if( !error )
            {
                purgeDelay = content["purgeDelay"].asInt32();
                if( !setConfigOption("purgeDelay", purgeDelay) )
                {
                    AGO_ERROR() << "Unable to save purge delay to config file";
                    error = true;
                }
            }

            if( !error )
            {
                desiredRendering = content["rendering"].asString();
                if( !setConfigOption("rendering", desiredRendering.c_str()) )
                {
                    AGO_ERROR() << "Unable to save rendering value to config file";
                    error = true;
                }
                else
                {
                    //get real rendering according user preferences
                    computeRendering();
                }   
            }
    
            if( error )
            {
                return responseFailed("Failed to save one or more options");
            }

            if( dataLogging )
                AGO_INFO() << "Data logging enabled";
            else
                AGO_INFO() << "Data logging disabled";
            if( gpsLogging )
                AGO_INFO() << "GPS logging enabled";
            else
                AGO_INFO() << "GPS logging disabled";
            if( rrdLogging )
                AGO_INFO() << "RRD logging enabled";
            else
                AGO_INFO() << "RRD logging disabled";

            return responseSuccess();
        }
        else if( content["command"]=="purgetable" )
        {
            checkMsgParameter(content, "table", VAR_STRING);

            //security check
            std::string table = content["table"].asString();
            if( isTablePurgeAllowed(table) )
            {
                if( purgeTable(table) )
                {
                    return responseSuccess();
                }
                else
                {
                    AGO_ERROR() << "Unable to purge table '" << table << "'";
                    return responseFailed("Internal error");
                }
            }
            else
            {
                AGO_ERROR() << "Purge table '" << table << "' not allowed";
                return responseFailed("Table purge not allowed");
            }
        }
        else
        {
            return responseUnknownCommand();
        }
    }
    else if( internalid=="journal" )
    {
        if( content["command"]=="addmessage" )
        {
            //store journal message
            checkMsgParameter(content, "message", VAR_STRING);
            if( content["type"].isVoid() )
            {
                content["type"] = JOURNAL_INFO;
            }

            if( eventHandlerJournal(content["message"].asString(), content["type"].asString()) )
            {
                return responseSuccess();
            }
            else
            {
                return responseFailed("Internal error");
            }
        }
        else if( content["command"]=="getmessages" )
        {
            //return messages in specified time range
            checkMsgParameter(content, "filter", VAR_STRING, true);
            checkMsgParameter(content, "type", VAR_STRING, false);
            if( content["start"].isVoid() && content["end"].isVoid() )
            {
                //no timerange specified, return message of today
                ptime s(date(day_clock::local_day()), hours(0));
                ptime e(date(day_clock::local_day()), hours(23)+minutes(59)+seconds(59));
                content["start"] = dateToDatabaseFormat(s);
                content["end"] = dateToDatabaseFormat(e);
            }
            else
            {
                checkMsgParameter(content, "start", VAR_STRING, false);
                checkMsgParameter(content, "end", VAR_STRING, false);
            }

            if( getMessagesFromJournal(content, returnData) )
            {
                return responseSuccess(returnData);
            }
            else
            {
                return responseFailed("Internal error");
            }
        }
        else
        {
            return responseUnknownCommand();
        }
    }

    //We do not support sending commands to our 'devices'
    return responseNoDeviceCommands();
}

void AgoDataLogger::setupApp()
{
    //init
    allowedPurgeTables.push_back("data");
    allowedPurgeTables.push_back("position");
    allowedPurgeTables.push_back("journal");

    //init database
    fs::path dbpath = ensureParentDirExists(getLocalStatePath(DBFILE));
    try {
        std::string dbconnection = getConfigOption("dbconnection", string("sqlite3:db=" + dbpath.string()).c_str());
        AGO_TRACE() << "CppDB connection string: " << dbconnection;
        sql = cppdb::session(dbconnection);
        AGO_INFO() << "Using " << sql.driver() << " database via CppDB";
        if (sql.driver() == "mysql") {
       	    size_t start = dbconnection.find("database=");
            if (start != std::string::npos) {
                std::string remainder = dbconnection.substr(start+9);
                size_t end = remainder.find(";");
                if (end != std::string::npos) {
                    dbname = remainder.substr(0,end);
		} else {
                    dbname = remainder;
		}
		AGO_INFO() << "Database name: " << dbname;
            }

        }
    } catch (std::exception const &e) {
        AGO_ERROR() << "Can't open database: " << e.what();
        throw StartupError();
    }

    //create missing tables
    list<string> queries;
    //db
    if (sql.driver() == "sqlite3") {
        queries.push_back("CREATE TABLE data(id INTEGER PRIMARY KEY AUTOINCREMENT, uuid TEXT, environment TEXT, level REAL, timestamp LONG);");
    } else {
        queries.push_back("CREATE TABLE data (id INTEGER PRIMARY KEY AUTO_INCREMENT, uuid VARCHAR(36), environment VARCHAR(64), level REAL, timestamp INT(11));");
    }
    queries.push_back("CREATE INDEX timestamp_idx ON data (timestamp);");
    queries.push_back("CREATE INDEX environment_idx ON data (environment);");
    queries.push_back("CREATE INDEX uuid_idx ON data (uuid);");
    createTableIfNotExist("data", queries);
    queries.clear();
    //position
    if (sql.driver() == "sqlite3") {
        queries.push_back("CREATE TABLE position(id INTEGER PRIMARY KEY AUTOINCREMENT, uuid TEXT, latitude REAL, longitude REAL, timestamp LONG)");
    } else {
        queries.push_back("CREATE TABLE position (id INTEGER PRIMARY KEY AUTO_INCREMENT, uuid VARCHAR(36), latitude REAL, longitude REAL, timestamp INT(11))");
    }
    queries.push_back("CREATE INDEX timestamp_position_idx ON position (timestamp)");
    queries.push_back("CREATE INDEX uuid_position_idx ON position (uuid)");
    createTableIfNotExist("position", queries);
    queries.clear();
    //journal table
    if (sql.driver() == "sqlite3") {
        queries.push_back("CREATE TABLE journal(id INTEGER PRIMARY KEY AUTOINCREMENT, timestamp LONG, message TEXT, type TEXT)");
    } else {
        queries.push_back("CREATE TABLE journal (id INTEGER PRIMARY KEY AUTO_INCREMENT, timestamp INT(11), message TEXT, type VARCHAR(64))");
    }
    queries.push_back("CREATE INDEX timestamp_journal_idx ON journal (timestamp)");
    queries.push_back("CREATE INDEX type_journal_idx ON journal (type)");
    createTableIfNotExist("journal", queries);
    queries.clear();

    //add controller
    agoConnection->addDevice("dataloggercontroller", "dataloggercontroller");

    //add journal
    agoConnection->addDevice("journal", "journal");

    //read config
    std::string optString = getConfigOption("dataLogging", "1");
    int r;
    if(sscanf(optString.c_str(), "%d", &r) == 1) {
        dataLogging = (r == 1);
    }
    optString = getConfigOption("gpsLogging", "1");
    if(sscanf(optString.c_str(), "%d", &r) == 1) {
        gpsLogging = (r == 1);
    }
    optString = getConfigOption("rrdLogging", "1");
    if(sscanf(optString.c_str(), "%d", &r) == 1) {
        rrdLogging = (r == 1);
    }
    if( dataLogging )
        AGO_INFO() << "Data logging enabled";
    else
        AGO_INFO() << "Data logging disabled";
    if( gpsLogging )
        AGO_INFO() << "GPS logging enabled";
    else
        AGO_INFO() << "GPS logging disabled";
    if( rrdLogging )
        AGO_INFO() << "RRD logging enabled";
    else
        AGO_INFO() << "RRD logging disabled";
    optString = getConfigOption("purgeDelay", "0");
    if(sscanf(optString.c_str(), "%d", &r) == 1)
    {
        purgeDelay = r;
    }
    AGO_INFO() << "Purge delay = " << purgeDelay << " months";
    desiredRendering = getConfigOption("rendering", "plots");
    AGO_INFO() << "Rendering = " << desiredRendering;
    //get real rendering according user preferences
    computeRendering();

    // load map, create sections if empty
    fs::path dmf = getConfigPath(DEVICEMAPFILE);
    devicemap = jsonFileToVariantMap(dmf);
    if (devicemap["nextid"].isVoid())
    {
        devicemap["nextid"] = 1;
        variantMapToJSONFile(devicemap, dmf);
    }
    if (devicemap["multigraphs"].isVoid())
    {
        qpid::types::Variant::Map devices;
        devicemap["multigraphs"] = devices;
        variantMapToJSONFile(devicemap, dmf);
    }

    //register existing devices
    AGO_INFO() << "Register existing multigraphs:";
    qpid::types::Variant::Map multigraphs = devicemap["multigraphs"].asMap();
    for( qpid::types::Variant::Map::const_iterator it=multigraphs.begin(); it!=multigraphs.end(); it++ )
    {
        std::string internalid = (std::string)it->first;
        AGO_INFO() << " - " << internalid;
        agoConnection->addDevice(internalid.c_str(), "multigraph");
    }

    addEventHandler();
    addCommandHandler();
}

AGOAPP_ENTRY_POINT(AgoDataLogger);
