#ifndef AGOSYSTEM_H
#define AGOSYSTEM_H
#include "agoapp.h"

using namespace agocontrol;

class AgoSystem: public AgoApp {
private:
    qpid::types::Variant::Map commandHandler(qpid::types::Variant::Map content) ;

    qpid::types::Variant::Map getAgoProcessList();
    qpid::types::Variant::Map getAgoProcessListDebug(std::string procName);

    // Implemented in processinfo_<platform>.cpp
    void getProcessInfo();

    void fillProcessesStats(qpid::types::Variant::Map& processes);
    void checkProcessesStates(qpid::types::Variant::Map& processes);
    void printProcess(qpid::types::Variant::Map process);
    void monitorProcesses();
    qpid::types::Variant::Map getProcessStructure();
    bool isProcessBlackListed(const std::string processName);
    void refreshAgoProcessList(qpid::types::Variant::Map& processes);

    void setupApp();

    boost::mutex processesMutex;
    qpid::types::Variant::Map processes;
    qpid::types::Variant::Map config;
public:
    AGOAPP_CONSTRUCTOR(AgoSystem);
};

#endif
