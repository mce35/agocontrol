#include <exception>
#include <boost/algorithm/string/predicate.hpp>
#include "agosystem.h"
using namespace std;
using namespace qpid::types;
namespace fs = ::boost::filesystem;

const char* PROCESS_BLACKLIST[] = {"agoclient.py", "agosystem", "agodrain", "agologger.py"};
const int PROCESS_BLACKLIST_SIZE = sizeof(PROCESS_BLACKLIST)/sizeof(*PROCESS_BLACKLIST);
const char* BIN_DIR = "/opt/agocontrol/bin";
const int MONITOR_INTERVAL = 5; //time between 2 monitoring (in seconds)
#ifndef SYSTEMMAPFILE
#define SYSTEMMAPFILE "maps/systemmap.json"
#endif


/**
 * Print process infos (debug purpose only, no map checking)
 */
void AgoSystem::printProcess(qpid::types::Variant::Map process)
{
    AGO_DEBUG() << "running=" << process["running"];
    AGO_DEBUG() << "current stats:";
    qpid::types::Variant::Map cs = process["currentStats"].asMap();
#ifndef FREEBSD
    AGO_DEBUG() << " utime=" << cs["utime"] << " cutime=" << cs["cutime"] << " stime=" << cs["stime"] << " cstime=" << cs["cstime"] << " cpuTotalTime=" << cs["cpuTotalTime"] << " ucpu=" << cs["ucpu"] << " scpu=" << cs["scpu"];
    AGO_DEBUG() << "last stats:";
    qpid::types::Variant::Map ls = process["lastStats"].asMap();
    AGO_DEBUG() << " utime=" << ls["utime"] << " cutime=" << ls["cutime"] << " stime=" << ls["stime"] << " cstime=" << ls["cstime"] << " cpuTotalTime=" << ls["cpuTotalTime"] << " ucpu=" << ls["ucpu"] << " scpu=" << cs["scpu"];
#else
    AGO_DEBUG() << " ucpu=" << cs["ucpu"];
    AGO_DEBUG() << "last stats:";
    qpid::types::Variant::Map ls = process["lastStats"].asMap();
    AGO_DEBUG() << " ucpu=" << ls["ucpu"];
#endif
}

/**
 * Fill monitored processes stats
 */
void AgoSystem::fillProcessesStats(qpid::types::Variant::Map& processes)
{
    //lock processes
    processesMutex.lock();

    //init
    qpid::types::Variant::List monitored = config["monitored"].asList();

    //update running flag (set to disabled)
    for( qpid::types::Variant::Map::iterator it=processes.begin(); it!=processes.end(); it++ )
    {
        //get stats
        qpid::types::Variant::Map stats = it->second.asMap();

        //update running state
        stats["running"] = false;

        //update monitored state
        int flag = false;
        for( qpid::types::Variant::List::iterator it1=monitored.begin(); it1!=monitored.end(); it1++ )
        {
            if( (*it1).asString()==it->first )
            {
                flag = true;
                break;
            }
        }
        stats["monitored"] = flag;

        //save stats
        processes[it->first] = stats;
    }

    // get processes statistics, platform dependent
    getProcessInfo();

    //unlock processes
    processesMutex.unlock();
}

/**
 * Check processes states and launch alarms if needed
 */
void AgoSystem::checkProcessesStates(qpid::types::Variant::Map& processes)
{
    //lock processes
    processesMutex.lock();

    for( qpid::types::Variant::Map::iterator it=processes.begin(); it!=processes.end(); it++ )
    {
        //get process stats
        qpid::types::Variant::Map stats = it->second.asMap();

        //check if process is running
        if( !stats["running"].isVoid() && !stats["monitored"].isVoid() )
        {
            if( stats["running"].asBool()==false )
            {
                //process is not running
                //reset current stats
                qpid::types::Variant::Map current = stats["currentStats"].asMap();
#ifndef FREEBSD
                current["utime"] = 0;
                current["cutime"] = 0;
                current["stime"] = 0;
                current["cstime"] = 0;
                current["cpuTotalTime"] = 0;
#endif
                current["vsize"] = 0;
                current["rss"] = 0;
                current["ucpu"] = 0;
                current["scpu"] = 0;
                stats["currentStats"] = current;

                //and reset last stats
                qpid::types::Variant::Map last = stats["lastStats"].asMap();
#ifndef FREEBSD
                last["utime"] = 0;
                last["cutime"] = 0;
                last["stime"] = 0;
                last["cstime"] = 0;
                last["cpuTotalTime"] = 0;
#endif
                last["vsize"] = 0;
                last["rss"] = 0;
                last["ucpu"] = 0;
                last["scpu"] = 0;
                stats["lastStats"] = last;

                if( stats["monitored"].asBool()==true && stats["alarmDead"].asBool()==false )
                {
                    //process is monitored, send alarm
                    qpid::types::Variant::Map content;
                    content["process"] = it->first;
                    agoConnection->emitEvent("systemcontroller", "event.monitoring.processdead", content);
                    AGO_INFO() << "Process '" << it->first << "' is not running";
                    stats["alarmDead"] = true;
                }
            }
            else
            {
                stats["alarmDead"] = false;
            }
        }

        //check memory
        int64_t memoryThreshold = config["memoryThreshold"].asInt64() * 1000000;
        if( memoryThreshold>0 )
        {
            qpid::types::Variant::Map cs = stats["currentStats"].asMap();
            if( cs["rss"].asInt64()>=memoryThreshold )
            {
                if( stats["alarmMemory"].asBool()==false )
                {
                    //process has reached memory threshold, send alarm
                    qpid::types::Variant::Map content;
                    content["process"] = it->first;
                    agoConnection->emitEvent("systemcontroller", "event.monitoring.memoryoverhead", content);
                    AGO_INFO() << "Memory overhead detected for '" << it->first << "'";
                    stats["alarmMemory"] = true;
                }
            }
            else
            {
                stats["alarmMemory"] = false;
            }
        }

        //save process stats
        processes[it->first] = stats;
    }

    //unlock processes
    processesMutex.unlock();
}

/**
 * Create list with specified process name. Used to debug.
 */
qpid::types::Variant::Map AgoSystem::getAgoProcessListDebug(string procName)
{
    qpid::types::Variant::Map output;
    qpid::types::Variant::Map stats;
    qpid::types::Variant::Map currentStats;
    currentStats["utime"] = (uint64_t)0;
    currentStats["cutime"] = (uint64_t)0;
    currentStats["stime"] = (uint64_t)0;
    currentStats["cstime"] = (uint64_t)0;
    currentStats["vsize"] = (uint64_t)0;
    currentStats["rss"] = (uint64_t)0;
    currentStats["cpuTotalTime"] = (uint32_t)0;
    currentStats["ucpu"] = (uint32_t)0;
    currentStats["scpu"] = (uint32_t)0;
    stats["currentStats"] = currentStats;
    qpid::types::Variant::Map lastStats;
    lastStats["utime"] = (uint64_t)0;
    lastStats["cutime"] = (uint64_t)0;
    lastStats["stime"] = (uint64_t)0;
    lastStats["cstime"] = (uint64_t)0;
    lastStats["vsize"] = (uint64_t)0;
    lastStats["rss"] = (uint64_t)0;
    lastStats["cpuTotalTime"] = (uint32_t)0;
    lastStats["ucpu"] = (uint32_t)0;
    lastStats["scpu"] = (uint32_t)0;
    stats["lastStats"] = lastStats;
    stats["running"] = false;
    output[procName] = stats;
    return output;
}

/**
 * Get process structure
 */
qpid::types::Variant::Map AgoSystem::getProcessStructure()
{
    qpid::types::Variant::Map stats;

    qpid::types::Variant::Map currentStats;
#ifndef FREEBSD
    currentStats["utime"] = (uint64_t)0;
    currentStats["cutime"] = (uint64_t)0;
    currentStats["stime"] = (uint64_t)0;
    currentStats["cstime"] = (uint64_t)0;
    currentStats["cpuTotalTime"] = (uint32_t)0;
#endif
    currentStats["vsize"] = (uint64_t)0;
    currentStats["rss"] = (uint64_t)0;
    currentStats["ucpu"] = (uint32_t)0;
    currentStats["scpu"] = (uint32_t)0;
    stats["currentStats"] = currentStats;

    qpid::types::Variant::Map lastStats;
#ifndef FREEBSD
    lastStats["utime"] = (uint64_t)0;
    lastStats["cutime"] = (uint64_t)0;
    lastStats["stime"] = (uint64_t)0;
    lastStats["cstime"] = (uint64_t)0;
    lastStats["cpuTotalTime"] = (uint32_t)0;
#endif
    lastStats["vsize"] = (uint64_t)0;
    lastStats["rss"] = (uint64_t)0;
    lastStats["ucpu"] = (uint32_t)0;
    lastStats["scpu"] = (uint32_t)0;
    stats["lastStats"] = lastStats;

    stats["running"] = false;
    stats["monitored"] = false;

    stats["alarmMemory"] = false;
    stats["alarmDead"] = false;

    return stats;
}

/**
 * Return true if specified process is black listed
 */
bool AgoSystem::isProcessBlackListed(const std::string processName)
{
    for( int i=0; i<PROCESS_BLACKLIST_SIZE; i++ )
    {
        if( PROCESS_BLACKLIST[i]==processName )
        {
            return true;
        }
    }
    return false;
}

/**
 * Get ago process list
 */
qpid::types::Variant::Map AgoSystem::getAgoProcessList()
{
    //init
    qpid::types::Variant::Map output;
    AGO_INFO() << "Monitored processes:";
    if( fs::exists(BIN_DIR) )
    {
        fs::recursive_directory_iterator it(BIN_DIR);
        fs::recursive_directory_iterator endit;
        while( it!=endit )
        {
            if( fs::is_regular_file(*it) && (it->path().extension().string()==".py" || it->path().extension().string()=="") &&
                boost::algorithm::starts_with(it->path().filename().string(), "ago") )
            {
                //check if process black listed
                if( !isProcessBlackListed(it->path().filename().string()) )
                {
                    qpid::types::Variant::Map stats = getProcessStructure();
                    output[it->path().filename().string()] = stats;
                    AGO_TRACE() << " - " << it->path().filename().string();
                }
                else
                {
                    AGO_TRACE() << " - " << it->path().filename().string() << " [BLACKLISTED]";
                }
            }
            ++it;
        }
    }
    else
    {
        // Temp dummy
        qpid::types::Variant::Map stats = getProcessStructure();
        output["agoresolver"] = stats;

        stats = getProcessStructure();
        output["agorpc"] = stats;

        stats = getProcessStructure();
        output["agoscenario"] = stats;

        stats = getProcessStructure();
        output["agoevent"] = stats;
        stats = getProcessStructure();
        output["agotimer"] = stats;
    }

    //append qpid
    qpid::types::Variant::Map stats = getProcessStructure();
    output["qpidd"] = stats;
    AGO_TRACE() << " - qpidd";

    return output;
}

/**
 * Refresh agocontrol process list
 */
void AgoSystem::refreshAgoProcessList(qpid::types::Variant::Map& processes)
{
    //first of all get process list
    qpid::types::Variant::Map newProcesses = getAgoProcessList();

    //lock processes
    processesMutex.lock();

    //then synchronise maps
    bool found = false;
    for( qpid::types::Variant::Map::iterator it=newProcesses.begin(); it!=newProcesses.end(); it++ )
    {
        found = false;

        //iterate over current process list
        for( qpid::types::Variant::Map::iterator it2=processes.begin(); it2!=processes.end(); it2++ )
        {
            if( it->first==it2->first )
            {
                //process found
                found = true;
                break;
            }
        }

        if( !found && !isProcessBlackListed(it->first) )
        {
            //add new entry
            qpid::types::Variant::Map stats = getProcessStructure();
            processes[it->first] = stats;
            AGO_DEBUG() << "New process detected and added [" << it->first << "]";
        }
    }

    //unlock processes
    processesMutex.unlock();
}

/**
 * Command handler
 */
qpid::types::Variant::Map AgoSystem::commandHandler(qpid::types::Variant::Map content)
{
    qpid::types::Variant::Map responseData;
    std::string internalid = content["internalid"].asString();
    AGO_DEBUG() << "Command received:" << content;
    if (internalid == "systemcontroller")
    {
        boost::lock_guard<boost::mutex> lock(processesMutex);
        if( content["command"]=="getprocesslist" )
        {
            return responseSuccess(processes);
        }
        else if( content["command"]=="getstatus" )
        {
            responseData["processes"] = processes;
            responseData["memoryThreshold"] = config["memoryThreshold"].asInt64();
            return responseSuccess(responseData);
        }
        else if( content["command"]=="setmonitoredprocesses" )
        {
            checkMsgParameter(content, "processes", VAR_LIST);

            qpid::types::Variant::List monitored = content["processes"].asList();
            //and save list to config file
            config["monitored"] = monitored;
            if(!variantMapToJSONFile(config, getConfigPath(SYSTEMMAPFILE)))
                return responseFailed("Failed to write map file");

            return responseSuccess();
        }
        else if( content["command"]=="setmemorythreshold" )
        {
            checkMsgParameter(content, "threshold", VAR_INT64);

            int64_t threshold = content["threshold"].asInt64();
            if( threshold < 0 )
            {
                threshold = 0;
            }

            config["memoryThreshold"] = threshold;
            if(!variantMapToJSONFile(config, getConfigPath(SYSTEMMAPFILE)))
                return responseFailed("Failed to write map file");

            return responseSuccess();
        }
        else
        {
            return responseUnknownCommand();
        }
    }
    
    // We have no devices registered but our own
    throw std::logic_error("Should not go here");
}

/**
 * Monitor processes (threaded)
 */
void AgoSystem::monitorProcesses()
{
    //get ago processes to monitor
    processes = getAgoProcessList();
    AGO_TRACE() << processes;

    //refresh interval
    int count = 0;
    int interval = 60 / MONITOR_INTERVAL;
    if( interval==0 )
    {
        //monitor interval is greater than 1 minute. Refresh process list at each monitor interval
        interval = 1;
    }
    AGO_TRACE() << "Refresh process list interval: " << interval;

    //launch monitoring...
    while( !isExitSignaled() )
    {
        //periodically refresh process list
        if( count==interval )
        {
            AGO_DEBUG() << "Refresh process list";
            refreshAgoProcessList(processes);
            count = 0;
        }
        count++;

        //update processes stats
        fillProcessesStats(processes);

        //check process states
        checkProcessesStates(processes);

        //pause
        sleep(MONITOR_INTERVAL);
    }
}

void AgoSystem::setupApp()
{
    //open conf file
    config = jsonFileToVariantMap(getConfigPath(SYSTEMMAPFILE));
    //add missing config parameters
    if( config["monitored"].isVoid() )
    {
        qpid::types::Variant::List monitored;
        config["monitored"] = monitored;
        variantMapToJSONFile(config, getConfigPath(SYSTEMMAPFILE));
    }

    if( config["memoryThreshold"].isVoid() )
    {
        config["memoryThreshold"] = 0;
        variantMapToJSONFile(config, getConfigPath(SYSTEMMAPFILE));
    }

    //launch monitoring thread
    boost::thread t(boost::bind(&AgoSystem::monitorProcesses, this));
    t.detach();

    //add controller
    agoConnection->addDevice("systemcontroller", "systemcontroller");

    //add handlers
    addCommandHandler();
}

AGOAPP_ENTRY_POINT(AgoSystem);
