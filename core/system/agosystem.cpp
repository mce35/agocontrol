#include <proc/readproc.h>
#include <boost/algorithm/string/predicate.hpp>
#include "agoapp.h"

using namespace std;
using namespace agocontrol;
namespace fs = ::boost::filesystem;

using namespace agocontrol;
using namespace qpid::types;

class AgoSystem: public AgoApp {
private:
    qpid::types::Variant::Map commandHandler(qpid::types::Variant::Map content) ;
    void eventHandler(std::string subject, qpid::types::Variant::Map content) ;

    qpid::types::Variant::Map getAgoProcessList();
    qpid::types::Variant::Map getAgoProcessListDebug(string procName);
    void fillProcessesStats(qpid::types::Variant::Map& processes);
    void checkProcessesStates(qpid::types::Variant::Map& processes);
    uint32_t getCpuTotalTime();
    void getCpuPercentage(qpid::types::Variant::Map& current, qpid::types::Variant::Map& last, double* ucpu, double* scpu);
    void printProcess(qpid::types::Variant::Map process);
    void monitorProcesses();
    qpid::types::Variant::Map getProcessStructure();

    void setupApp();

    boost::mutex processesMutex;
    qpid::types::Variant::Map processes;
    qpid::types::Variant::Map config;
public:
    AGOAPP_CONSTRUCTOR(AgoSystem);
};

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
    AGO_DEBUG() << " utime=" << cs["utime"] << " cutime=" << cs["cutime"] << " stime=" << cs["stime"] << " cstime=" << cs["cstime"] << " cpuTotalTime=" << cs["cpuTotalTime"] << " ucpu=" << cs["ucpu"] << " scpu=" << cs["scpu"];
    AGO_DEBUG() << "last stats:";
    qpid::types::Variant::Map ls = process["lastStats"].asMap();
    AGO_DEBUG() << " utime=" << ls["utime"] << " cutime=" << ls["cutime"] << " stime=" << ls["stime"] << " cstime=" << ls["cstime"] << " cpuTotalTime=" << ls["cpuTotalTime"] << " ucpu=" << ls["ucpu"] << " scpu=" << cs["scpu"];
}

/**
 * Calculates the elapsed CPU percentage between 2 measuring points.
 * CPU usage calculation extracted from https://github.com/fho/code_snippets/blob/master/c/getusage.c
 */
void AgoSystem::getCpuPercentage(qpid::types::Variant::Map& current, qpid::types::Variant::Map& last, double* ucpu, double* scpu)
{
    uint32_t totalTimeDiff = current["cpuTotalTime"].asUint32() - last["cpuTotalTime"].asUint32();

    uint64_t totalCurrentU = current["utime"].asUint64() + current["cutime"].asUint64();
    uint64_t totalLastU = last["utime"].asUint64() + last["cutime"].asUint64();
    if( (totalCurrentU-totalLastU)>0 )
    {
        *ucpu = 100 * (double)((totalCurrentU - totalLastU) / (double)totalTimeDiff);
    }
    else
    {
        *ucpu = 0;
    }

    uint64_t totalCurrentS = current["stime"].asUint64() + current["cstime"].asUint64();
    uint64_t totalLastS = last["stime"].asUint64() + last["cstime"].asUint64();
    if( (totalCurrentS-totalLastS)>0 )
    {
        *scpu = 100 * (double)((totalCurrentS - totalLastS) / (double)totalTimeDiff);
    }
    else
    {
        *scpu = 0;
    }
}

/**
 * return CPU total time from /proc/stat file
 * @return 0 if error occured
 */
uint32_t AgoSystem::getCpuTotalTime()
{
    uint32_t cpuTotalTime = 0;
    int i = 0;

    FILE *fstat = fopen("/proc/stat", "r");
    if (fstat == NULL)
    {
        perror("FOPEN ERROR ");
        fclose(fstat);
        return 0;
    }
    long unsigned int cpu_time[10];
    bzero(cpu_time, sizeof(cpu_time));
    if (fscanf(fstat, "%*s %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu",
                &cpu_time[0], &cpu_time[1], &cpu_time[2], &cpu_time[3],
                &cpu_time[4], &cpu_time[5], &cpu_time[6], &cpu_time[7],
                &cpu_time[8], &cpu_time[9]) == EOF)
    {
        fclose(fstat);
        return 0;
    }
    fclose(fstat);

    for(i=0; i < 10;i++)
    {
        cpuTotalTime += cpu_time[i];
    }

    return cpuTotalTime;
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

    //get processes statistics
    PROCTAB* proc = openproc(PROC_FILLMEM | PROC_FILLSTAT | PROC_FILLSTATUS);
    proc_t proc_info;
    memset(&proc_info, 0, sizeof(proc_info));
    while (readproc(proc, &proc_info) != NULL)
    {
        string procName = string(proc_info.cmd);
        if( processes.find(procName)!=processes.end()  )
        {
            qpid::types::Variant::Map stats = processes[procName].asMap();
            qpid::types::Variant::Map cs = stats["currentStats"].asMap();
            qpid::types::Variant::Map ls = stats["lastStats"].asMap();

            //update current stats
            cs["utime"] = (uint64_t)proc_info.utime;
            cs["stime"] = (uint64_t)proc_info.stime;
            cs["cutime"] = (uint64_t)proc_info.cutime;
            cs["cstime"] = (uint64_t)proc_info.cstime;
            cs["vsize"] = (uint64_t)proc_info.vm_size * 1024;
            cs["rss"] = (uint64_t)proc_info.vm_rss * 1024;
            cs["cpuTotalTime"] = (uint32_t)getCpuTotalTime();
            double ucpu=0, scpu=0;
            if( ls["utime"].asUint64()!=0 )
            {
                getCpuPercentage(cs, ls, &ucpu, &scpu);
            }
            cs["ucpu"] = (double)ucpu;
            cs["scpu"] = (double)scpu;

            //update last stats
            stats["lastStats"] = cs;

            //finalize
            stats["currentStats"] = cs;
            stats["running"] = true;
            processes[procName] = stats;
        }
    }
    closeproc(proc);

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
            if( stats["running"].asBool()==false && stats["monitored"].asBool()==true )
            {
                if( stats["alarmDead"].asBool()==false )
                {
                    //process is not running and it is monitored, send alarm
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
    stats["monitored"] = false;

    stats["alarmMemory"] = false;
    stats["alarmDead"] = false;

    return stats;
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
                //file seems valid
                bool blackListed = false;
                for( int i=0; i<PROCESS_BLACKLIST_SIZE; i++ )
                {
                    if( PROCESS_BLACKLIST[i]==it->path().filename().string() )
                    {
                        //drop file
                        blackListed = true;
                        break;
                    }
                }

                if( !blackListed )
                {
                    qpid::types::Variant::Map stats = getProcessStructure();
                    output[it->path().filename().string()] = stats;
                    AGO_INFO() << " - " << it->path().filename().string();
                }
                else
                {
                    AGO_DEBUG() << " - " << it->path().filename().string() << " [BLACKLISTED]";
                }
            }
            ++it;
        }
    }

    //append qpid
    qpid::types::Variant::Map stats = getProcessStructure();
    output["qpidd"] = stats;
    AGO_INFO() << " - qpidd";

    return output;
}

/**
 * Event handler
 */
void AgoSystem::eventHandler(std::string subject, qpid::types::Variant::Map content)
{
}

/**
 * Command handler
 */
qpid::types::Variant::Map AgoSystem::commandHandler(qpid::types::Variant::Map content)
{
    qpid::types::Variant::Map returnval;
    returnval["error"] = 0;
    returnval["msg"] = "";
    std::string internalid = content["internalid"].asString();
    AGO_DEBUG() << "Command received:" << content;
    if (internalid == "systemcontroller")
    {
        if( content["command"]=="getprocesslist" )
        {
            processesMutex.lock();
            returnval = processes;
            processesMutex.unlock();
        }
        else if( content["command"]=="getstatus" )
        {
            processesMutex.lock();
            returnval["processes"] = processes;
            returnval["memoryThreshold"] = config["memoryThreshold"].asInt64();
            processesMutex.unlock();
        }
        else if( content["command"]=="setmonitoredprocesses" )
        {
            processesMutex.lock();
            if( !content["processes"].isVoid() )
            {
                qpid::types::Variant::List monitored = content["processes"].asList();
                //and save list to config file
                config["monitored"] = monitored;
                variantMapToJSONFile(config, getConfigPath(SYSTEMMAPFILE));
            }
            processesMutex.unlock();
        }
        else if( content["command"]=="setmemorythreshold" )
        {
            processesMutex.lock();
            if( !content["threshold"].isVoid() )
            {
                char* p;
                int64_t threshold = (int64_t)strtoll(content["threshold"].asString().c_str(), &p, 10);
                if( *p==0 )
                {
                    if( threshold<0 )
                    {
                        threshold = 0;
                    }
                    config["memoryThreshold"] = threshold;
                    variantMapToJSONFile(config, getConfigPath(SYSTEMMAPFILE));
                }
                else
                {
                    //unsupported threshold type
                    AGO_ERROR() << "Invalid threshold type. Unable to convert";
                    returnval["error"] = 1;
                    returnval["msg"] = "Invalid value";
                    returnval["old"] = config["memoryThreshold"].asInt64();
                }
            }
            else
            {
                //missing parameter
                returnval["error"] = 1;
                returnval["msg"] = "Missing parameter";
                returnval["old"] = config["memoryThreshold"].asInt64();
            }
            processesMutex.unlock();
        }
        else
        {
            AGO_WARNING() << "Command not found";
            returnval["error"] = 1;
            returnval["msg"] = "Internal error";
        }
    }
    return returnval;
}

/**
 * Monitor processes (threaded)
 */
void AgoSystem::monitorProcesses()
{
    //get ago processes to monitor
    //processes = getAgoProcessListDebug("xcalc");
    processes = getAgoProcessList();
    AGO_DEBUG() << processes;

    //launch monitoring...
    while( !isExitSignaled() )
    {
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
    //addEventHandler();
    addCommandHandler();
}

AGOAPP_ENTRY_POINT(AgoSystem);
