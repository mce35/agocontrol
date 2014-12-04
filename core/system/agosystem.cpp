#include <proc/readproc.h>
#include <boost/algorithm/string/predicate.hpp>
#include "agoapp.h"

using namespace std;
using namespace agocontrol;
namespace fs = ::boost::filesystem;

using namespace agocontrol;

class AgoSystem: public AgoApp {
private:
    qpid::types::Variant::Map commandHandler(qpid::types::Variant::Map content) ;
    void eventHandler(std::string subject, qpid::types::Variant::Map content) ;

    qpid::types::Variant::Map getAgoProcessList();
    qpid::types::Variant::Map getAgoProcessListDebug(string procName);
    void fillProcessesStats(qpid::types::Variant::Map& processes);
    uint32_t getCpuTotalTime();
    void getCpuPercentage(qpid::types::Variant::Map& current, qpid::types::Variant::Map& last, double* ucpu, double* scpu);
    void printProcess(qpid::types::Variant::Map process);
    void monitorProcesses();
    qpid::types::Variant::Map getProcessStructure();

    void setupApp();

    boost::mutex processesMutex;
    qpid::types::Variant::Map processes;
public:
    AGOAPP_CONSTRUCTOR(AgoSystem);
};

const char* PROCESS_BLACKLIST[] = {"agoclient.py", "agosystem"};
const int PROCESS_BLACKLIST_SIZE = 2;
const char* BIN_DIR = "/opt/agocontrol/bin";
const int MONITOR_INTERVAL = 5; //time between 2 monitoring (in seconds)


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
    *ucpu = 100 * (double)(((current["utime"].asUint64() + current["cutime"].asUint64()) - (last["utime"].asUint64() + last["cutime"].asUint64())) / (double)totalTimeDiff);
    *scpu = 100 * (double)(((current["stime"].asUint64() + current["cstime"].asUint64()) - (last["stime"].asUint64() + last["cstime"].asUint64())) / (double)totalTimeDiff);
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

    //update running flag (set to disabled)
    for( qpid::types::Variant::Map::iterator it=processes.begin(); it!=processes.end(); it++ )
    {
        qpid::types::Variant::Map stats = it->second.asMap();
        stats["running"] = false;
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
            cs["vsize"] = (uint64_t)proc_info.vm_size;
            cs["rss"] = (uint64_t)proc_info.vm_rss;
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
    output["qpid"] = stats;
    AGO_INFO() << " - qpid";

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
    std::string internalid = content["internalid"].asString();
    AGO_DEBUG() << "Command received:" << content;
    if (internalid == "systemcontroller")
    {
        if (content["command"] == "status")
        {
            processesMutex.lock();
            returnval = processes;
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
        AGO_DEBUG() << "Updating stats...";
        fillProcessesStats(processes);
        //printProcess(processes["xcalc"].asMap());

        //pause
        sleep(MONITOR_INTERVAL);
    }
}

void AgoSystem::setupApp()
{
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
