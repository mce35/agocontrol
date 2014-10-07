#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <iostream>
#include <boost/asio.hpp> 
#include <boost/system/system_error.hpp> 
#include <execinfo.h>
#include <signal.h>
#include <time.h>
#include <cstdlib>
#include <stdexcept>

#include "MySensors.h"
#include "agoclient.h"

#ifndef DEVICEMAPFILE
#define DEVICEMAPFILE "/maps/mysensors.json"
#endif
#define RESEND_MAX_ATTEMPTS 30

using namespace std;
using namespace agocontrol;
using namespace boost::system; 
using namespace boost::asio; 
using namespace qpid::types;
namespace fs = ::boost::filesystem;

enum flush_type
{
    flush_receive = TCIFLUSH,
    flush_send = TCIOFLUSH,
    flush_both = TCIOFLUSH
};

typedef struct S_COMMAND
{
    std::string command;
    int attempts;
} T_COMMAND;

int DEBUG = 0;
int DEBUG_GW = 0;
AgoConnection *agoConnection;
pthread_mutex_t serialMutex;
pthread_mutex_t resendMutex;
pthread_mutex_t devicemapMutex;
int serialFd = 0;
pthread_t readThread;
pthread_t resendThread;
string units = "M";
qpid::types::Variant::Map devicemap;
std::map<std::string, T_COMMAND> commandsmap;
std::string gateway_protocol_version = "1.4";

io_service ioService; 
serial_port serialPort(ioService); 
string device = "";
int staleThreshold = 86400;

/**
 * Traceback stack
 */
/*void my_terminate(void);

namespace {
    // invoke set_terminate as part of global constant initialization
    static const bool SET_TERMINATE = std::set_terminate(my_terminate);
}

// This structure mirrors the one found in /usr/include/asm/ucontext.h
typedef struct _sig_ucontext {
   unsigned long     uc_flags;
   struct ucontext   *uc_link;
   stack_t           uc_stack;
   struct sigcontext uc_mcontext;
   sigset_t          uc_sigmask;
} sig_ucontext_t;

void crit_err_hdlr(int sig_num, siginfo_t * info, void * ucontext) {
    sig_ucontext_t * uc = (sig_ucontext_t *)ucontext;

    // Get the address at the time the signal was raised from the EIP (x86)
    void * caller_address = (void *) uc->uc_mcontext.eip;

    std::cerr << "signal " << sig_num 
              << " (" << strsignal(sig_num) << "), address is " 
              << info->si_addr << " from " 
              << caller_address << std::endl;

    void * array[50];
    int size = backtrace(array, 50);

    std::cerr << __FUNCTION__ << " backtrace returned " 
              << size << " frames\n\n";

    // overwrite sigaction with caller's address
    array[1] = caller_address;

    char ** messages = backtrace_symbols(array, size);

    // skip first stack frame (points here)
    for (int i = 1; i < size && messages != NULL; ++i) {
        std::cerr << "[bt]: (" << i << ") " << messages[i] << std::endl;
    }
    std::cerr << std::endl;

    free(messages);

    exit(EXIT_FAILURE);
}

void my_terminate() {
    static bool tried_throw = false;

    try {
        // try once to re-throw currently active exception
        if (!tried_throw++) throw;
    }
    catch (const std::exception &e) {
        std::cerr << __FUNCTION__ << " caught unhandled exception. what(): "
                  << e.what() << std::endl;
    }
    catch (...) {
        std::cerr << __FUNCTION__ << " caught unknown/unhandled exception." 
                  << std::endl;
    }

    void * array[50];
    int size = backtrace(array, 50);    

    std::cerr << __FUNCTION__ << " backtrace returned " 
              << size << " frames\n\n";

    char ** messages = backtrace_symbols(array, size);

    for (int i = 0; i < size && messages != NULL; ++i) {
        std::cerr << "[bt]: (" << i << ") " << messages[i] << std::endl;
    }
    std::cerr << std::endl;

    free(messages);

    abort();
}*/

/**
 * Split specified string
 */
std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems) {
	std::stringstream ss(s);
	std::string item;
	while (std::getline(ss, item, delim)) {
		elems.push_back(item);
	}
	return elems;
}
std::vector<std::string> split(const std::string &s, char delimiter) {
	std::vector<std::string> elements;
	split(s, delimiter, elements);
	return elements;
}

/**
 * Convert timestamp to Human Readable date time string (19 chars)
 */
std::string timestampToStr(const time_t* timestamp)
{
    struct tm* sTm = gmtime(timestamp);
    char hr[512] = "";

    snprintf(hr, 512, "%02d:%02d:%02d %04d/%02d/%02d", sTm->tm_hour, sTm->tm_min, sTm->tm_sec, sTm->tm_year+1900, sTm->tm_mon+1, sTm->tm_mday);

    return std::string(hr);
}

/**
 * @brief Flush a serial port's buffers.
 * @param serial_port Port to flush.
 * @param what Determines the buffers to flush.
 * @param error Set to indicate what error occurred, if any.
 * @info http://stackoverflow.com/questions/22581315/how-to-discard-data-as-it-is-sent-with-boostasio
 */
void flush_serial_port(boost::asio::serial_port& serial_port, flush_type what, boost::system::error_code& error)
{
    if (0 == ::tcflush(serial_port.lowest_layer().native_handle(), what))
    {
        error = boost::system::error_code();
    }
    else
    {
        error = boost::system::error_code(errno, boost::asio::error::get_system_category());
    }
}

/**
 * Make readable device infos
 */
std::string printDeviceInfos(std::string internalid, qpid::types::Variant::Map infos) {
    std::stringstream result;
    result << "Infos of device internalid '" << internalid << "'" << endl;
    if( !infos["protocol"].isVoid() )
        result << " - protocol=" << infos["protocol"] << endl;
    if( !infos["type"].isVoid() )
        result << " - type=" << infos["type"] << endl;
    if( !infos["value"].isVoid() )
        result << " - value=" << infos["value"] << endl;
    if( !infos["counter_sent"].isVoid() )
        result << " - counter_sent=" << infos["counter_sent"] << endl;
    if( !infos["counter_retries"].isVoid() )
        result << " - counter_retries=" << infos["counter_retries"] << endl;
    if( !infos["counter_received"].isVoid() )
        result << " - counter_received=" << infos["counter_received"] << endl;
    if( !infos["counter_failed"].isVoid() )
        result << " - counter_failed=" << infos["counter_failed"] << endl;
    if( !infos["last_timestamp"].isVoid() )
        result << " - last_timestamp=" << infos["last_timestamp"] << endl;
    return result.str();
}

/**
 * Save specified device infos
 */
void setDeviceInfos(std::string internalid, qpid::types::Variant::Map* infos)
{
    pthread_mutex_lock(&devicemapMutex);
    qpid::types::Variant::Map device = devicemap["devices"].asMap();
    device[internalid] = (*infos);
    devicemap["devices"] = device;
    variantMapToJSONFile(devicemap, getConfigPath(DEVICEMAPFILE));
    pthread_mutex_unlock(&devicemapMutex);
}

/**
 * Get infos of specified device
 */
qpid::types::Variant::Map getDeviceInfos(std::string internalid) {
    qpid::types::Variant::Map out;
    qpid::types::Variant::Map devices = devicemap["devices"].asMap();
    bool save = false;

    if( devices.count(internalid)==1 )
    {
        out = devices[internalid].asMap();

        //check field existence
        if( out["protocol"].isVoid() ) 
        {
            out["protocol"] = "0.0";
            save = true;
        }
        
        if( save )
        {
            setDeviceInfos(internalid, &out);
        }
    }
    return out;
}

/**
 * Make readable received message from MySensor gateway
 */
std::string prettyPrint(std::string message)
{
    std::string payload = "";
    std::string ack = "";
    std::stringstream result;
    std::string internalid = "";
    qpid::types::Variant::Map infos;

    std::vector<std::string> items = split(message, ';');
    if ( items.size()<4 || items.size()>6 )
    {
        result.str("");
        result << "ERROR, malformed string: " << message << endl;
    }
    else
    {
        //internalid
        internalid = items[0] + "/" + items[1];
        result << internalid << ";";

        if( gateway_protocol_version=="1.4" )
        {
            //protocol v1.4

            //message type
            result << getMsgTypeNameV14((msgTypeV14)atoi(items[2].c_str())) << ";";
            //ack
            result << items[3] << ";";
            //message subtype
            switch (atoi(items[2].c_str())) {
                case PRESENTATION_V14:
                    //device type
                    result << getDeviceTypeNameV14((deviceTypesV14)atoi(items[4].c_str())) << ";";
                    //protocol version (payload)
                    result << items[5] << endl;;
                    break;
                case SET_V14:
                case REQ_V14:
                    //variable type
                    result << getVariableTypeNameV14((varTypesV14)atoi(items[4].c_str())) << ";";
                    //value (payload)
                    result << items[5] << endl;
                    break;
                case INTERNAL_V14:
                    //internal message type
                    if( atoi(items[4].c_str())==I_LOG_MESSAGE_V14 && !DEBUG_GW )
                    {
                        //filter gateway log message
                        result.str("");
                    }
                    else
                    {
                        result << getInternalTypeNameV14((internalTypesV14)atoi(items[4].c_str())) << ";";
                        //value (payload)
                        if( items.size()==6 )
                            result << items[5] << endl;
                        else
                            result << endl;
                    }
                    break;
                case STREAM_V14:
                    //stream message
                    //TODO when fully implemented in MySensors
                    result << "STREAM (not implemented!)" << endl;
                    break;
                default:
                    result << items[3] << endl;;
            }
        }
        else if( gateway_protocol_version=="1.3" )
        {
            //protocol v1.3

            if (items.size() == 5)
                payload=items[4];
            result << items[0] << "/" << items[1] << ";" << getMsgTypeNameV13((msgTypeV13)atoi(items[2].c_str())) << ";";
            switch (atoi(items[2].c_str())) {
                case PRESENTATION_V13:
                    result << getDeviceTypeNameV13((deviceTypesV13)atoi(items[3].c_str()));
                    break;
                case SET_VARIABLE_V13:
                    result << getVariableTypeNameV13((varTypesV13)atoi(items[3].c_str()));
                    break;
                case REQUEST_VARIABLE_V13:
                    result << getVariableTypeNameV13((varTypesV13)atoi(items[3].c_str()));
                    break;
                case VARIABLE_ACK_V13:
                    result << getVariableTypeNameV13((varTypesV13)atoi(items[3].c_str()));
                    break;
                case INTERNAL_V13:
                    result << getInternalTypeNameV13((internalTypesV13)atoi(items[3].c_str()));
                    break;
                default:
                    result << items[3];
            }
            result <<  ";" << payload << endl;
        }
        else
        {
            result.str("");
            result << "ERROR, unsupported protocol version '" << gateway_protocol_version << "'" << endl;
        }
    }
    return result.str();
}

/**
 * Delete device and all associated infos
 */
bool deleteDevice(std::string internalid)
{
    //init
    qpid::types::Variant::Map devices = devicemap["devices"].asMap();
    bool result = true;

    //check if device exists
    if( !devices[internalid].isVoid() )
    {
        //remove device from uuidmap
        if( agoConnection->removeDevice(internalid.c_str()) )
        {
            //clear all infos
            pthread_mutex_lock(&devicemapMutex);
            devices.erase(internalid);
            devicemap["devices"] = devices;
            variantMapToJSONFile(devicemap, getConfigPath(DEVICEMAPFILE));
            pthread_mutex_unlock(&devicemapMutex);

            result = true;
            cout << "Device \"" << internalid << "\" removed successfully" << endl;
        }
        else
        {
            //unable to remove device
            result = false;
            cout << "Unable to remove device \"" << internalid << "\"" << endl;
        }
    }
    else
    {
        //device not found
        result = false;
        cout << "Unable to remove unknown device \"" << internalid << "\"" << endl;
    }
    
    return result;
}

/**
 * Save all necessary infos for new device and register it to agocontrol
 */
void addDevice(std::string internalid, std::string devicetype, qpid::types::Variant::Map devices, qpid::types::Variant::Map infos)
{
    pthread_mutex_lock(&devicemapMutex);
    infos["type"] = devicetype;
    infos["value"] = "0";
    infos["counter_sent"] = 0;
    infos["counter_received"] = 0;
    infos["counter_retries"] = 0;
    infos["counter_failed"] = 0;
    infos["last_timestamp"] = 0;
    infos["protocol"] = gateway_protocol_version;
    devices[internalid] = infos;
    devicemap["devices"] = devices;
    variantMapToJSONFile(devicemap, getConfigPath(DEVICEMAPFILE));
    agoConnection->addDevice(internalid.c_str(), devicetype.c_str());
    pthread_mutex_unlock(&devicemapMutex);
}

/**
 * Open serial port
 */
bool openSerialPort(string device)
{
    bool result = true;
    try
    {
        serialPort.open(device); 
        serialPort.set_option(serial_port::baud_rate(115200)); 
        serialPort.set_option(serial_port::parity(serial_port::parity::none)); 
        serialPort.set_option(serial_port::character_size(serial_port::character_size(8))); 
        serialPort.set_option(serial_port::stop_bits(serial_port::stop_bits::one)); 
        serialPort.set_option(serial_port::flow_control(serial_port::flow_control::none)); 
        //flush serial content
        boost::system::error_code error;
        flush_serial_port(serialPort, flush_both, error);
        if( DEBUG )
            cout << "Flushing serial port: " << error.message() << endl;
    }
    catch(std::exception const&  ex)
    {
        cerr  << "Can't open serial port: " << ex.what() << endl;
        result = false;
    }
    return result;
}

/**
 * Close serial port
 */
void closeSerialPort() {
    try {
        serialPort.close();
    }
    catch( std::exception const&  ex) {
        //cerr  << "Can't close serial port: " << ex.what() << endl;
    }
}

/**
 * Check internalid
 */
bool checkInternalid(std::string internalid)
{
    bool result = false;
    if( internalid.length()>0 )
    {
        //format <int>/<int>
        int nodeId = 666;
        int childId = 666;
        int read = sscanf(internalid.c_str(), "%d/%d", &nodeId, &childId);
        if( read==2 && nodeId>=0 && nodeId<=255 && childId>=0 && childId<=255 )
        {
            //specified internalid is valid
            result = true;
        }
        else
        {
            //error parsing internalid, seems to be invalid
            result = false;
        }
    }
    else
    {  
        //invalid input internalid
        result = false;
    }
    return result;
}

/**
 * Send command to MySensor gateway
 */
void sendcommand(std::string command)
{
    if( DEBUG )
    {
        time_t t = time(NULL);
        cout << " => " << timestampToStr(&t) << " RE-SENDING: " << command;
    }
    serialPort.write_some(buffer(command));
}
void sendcommandV14(std::string internalid, int messageType, int ack, int subType, std::string payload)
{
    std::vector<std::string> items = split(internalid, '/');
    stringstream command;
    qpid::types::Variant::Map infos = getDeviceInfos(internalid);

    //prepare command
    int nodeId = atoi(items[0].c_str());
    int childId = atoi(items[1].c_str());
    command << nodeId << ";" << childId << ";" << messageType << ";" << ack << ";" << subType << ";" << payload << "\n";

    //save command if device is an actuator and message type is SET
    if( infos.size()>0 && infos["type"]=="switch" && messageType==SET_V14 )
    {
        //check if internalid has no command pending
        pthread_mutex_lock(&resendMutex);
        if( commandsmap.count(internalid)==0 )
        {
            T_COMMAND cmd;
            cmd.command = command.str();
            cmd.attempts = 0;
            commandsmap[internalid] = cmd;
        }
        pthread_mutex_unlock(&resendMutex);
    }

    //send command
    if( DEBUG )
    {
        time_t t = time(NULL);
        cout << " => " << timestampToStr(&t) << " SENDING: " << command.str();
    }
    serialPort.write_some(buffer(command.str()));
}
void sendcommandV13(std::string internalid, int messageType, int subType, std::string payload)
{
    std::vector<std::string> items = split(internalid, '/');
    stringstream command;
    qpid::types::Variant::Map infos = getDeviceInfos(internalid);

    //prepare command
    int nodeId = atoi(items[0].c_str());
    int childId = atoi(items[1].c_str());
    command << nodeId << ";" << childId << ";" << messageType << ";" << subType << ";" << payload << "\n";

    //save command if device is an actuator and message type is SET_VARIABLE
    if( infos.size()>0 && infos["type"]=="switch" && messageType==SET_VARIABLE_V13 )
    {
        //check if internalid has no command pending
        pthread_mutex_lock(&resendMutex);
        if( commandsmap.count(internalid)==0 )
        {
            T_COMMAND cmd;
            cmd.command = command.str();
            cmd.attempts = 0;
            commandsmap[internalid] = cmd;
        }
        pthread_mutex_unlock(&resendMutex);
    }

    //send command
    if( DEBUG )
    {
        time_t t = time(NULL);
        cout << " => " << timestampToStr(&t) << " SENDING: " << command.str();
    }
    serialPort.write_some(buffer(command.str()));
}

/**
 * Agocontrol command handler
 */
qpid::types::Variant::Map commandHandler(qpid::types::Variant::Map command) {
    qpid::types::Variant::Map returnval;
    qpid::types::Variant::Map infos;
    std::string deviceType = "";
    std::string cmd = "";
    std::string internalid = "";
    if( DEBUG )
        cout << "CommandHandler" << command << endl;
    if( command.count("internalid")==1 && command.count("command")==1 ) {
        //get values
        cmd = command["command"].asString();
        internalid = command["internalid"].asString();

        //switch to specified command
        if( cmd=="getcounters" )
        {
            //return devices counters
            returnval["error"] = 0;
            returnval["msg"] = "";
            qpid::types::Variant::Map counters;
            qpid::types::Variant::Map devices = devicemap["devices"].asMap();
            for (qpid::types::Variant::Map::const_iterator it = devices.begin(); it != devices.end(); it++)
            {
                qpid::types::Variant::Map content = it->second.asMap();
                //add device name + device type
                std::stringstream deviceNameType;
                deviceNameType << it->first.c_str() << " (";
                infos = getDeviceInfos(it->first);
                if( infos.size()>0 && !infos["type"].isVoid() ) {
                    deviceNameType << infos["type"];
                }
                else {
                    deviceNameType << "unknown";
                }
                deviceNameType << ")";
                content["device"] = deviceNameType.str().c_str();
                if( !content["last_timestamp"].isVoid() ) {
                    int32_t timestamp = content["last_timestamp"].asInt32();
                    content["datetime"] = timestampToStr((time_t*)&timestamp);
                }
                else {
                    content["datetime"] = "?";
                }
                counters[it->first.c_str()] = content;
            }
            returnval["counters"] = counters;
        }
        else if( cmd=="resetallcounters" )
        {
            //reset all counters
            qpid::types::Variant::Map devices = devicemap["devices"].asMap();
            for (qpid::types::Variant::Map::iterator it = devices.begin(); it != devices.end(); it++) {
                infos = getDeviceInfos(it->first);
                if( infos.size()>0 ) {
                    infos["counter_received"] = 0;
                    infos["counter_sent"] = 0;
                    infos["counter_retries"] = 0;
                    infos["counter_failed"] = 0;
                    infos["last_timestamp"] = 0;
                    setDeviceInfos(it->first, &infos);
                }
        	}
            returnval["error"] = 0;
            returnval["msg"] = "";
        }
        else if( cmd=="resetcounters" )
        {
            //reset counters of specified sensor
            //command["device"] format: {internalid:<>, type:<>}
            qpid::types::Variant::Map device = command["device"].asMap();
            if( !device["internalid"].isVoid() )
            {
                infos = getDeviceInfos(device["internalid"].asString());
                if( infos.size()>0 )
                {
                    infos["counter_received"] = 0;
                    infos["counter_sent"] = 0;
                    infos["counter_retries"] = 0;
                    infos["counter_failed"] = 0;
                    infos["last_timestamp"] = 0;
                    setDeviceInfos(device["internalid"].asString(), &infos);
                }
                returnval["error"] = 0;
                returnval["msg"] = "";
            }
            else
            {
                //invalid command format
                returnval["error"] = 6;
                returnval["msg"] = "Invalid command received";
            }
        }
        else if( cmd=="getport" )
        {
            //get serial port
            returnval["error"] = 0;
            returnval["msg"] = "";
            returnval["port"] = device;
        }
        else if( cmd=="setport" )
        {
            //set serial port
            if( !command["port"].isVoid() ) {
                //restart communication
                closeSerialPort();
                if( !openSerialPort(command["port"].asString()) ) {
                    returnval["error"] = 1;
                    returnval["msg"] = "Unable to open specified port";
                }
                else {
                    //everything looks good, save port
                    device = command["port"].asString();
                    if( !setConfigOption("mysensors", "device", device.c_str()) ) {
                        returnval["error"] = 2;
                        returnval["msg"] = "Unable to save serial port to config file";
                    }
                    else {
                        returnval["error"] = 0;
                        returnval["msg"] = "";
                    }
                }
            }
            else {
                //port is missing
                returnval["error"] = 3;
                returnval["msg"] = "No port specified";
            }
        }
        else if( cmd=="getdevices" )
        {
            //return list of devices (with device type too!!)
            returnval["error"] = 0;
            returnval["msg"] = "";
            qpid::types::Variant::List devicesList;
            qpid::types::Variant::Map devices = devicemap["devices"].asMap();
            for (qpid::types::Variant::Map::const_iterator it = devices.begin(); it != devices.end(); it++) {
                infos = getDeviceInfos(it->first);
                qpid::types::Variant::Map item;
                item["internalid"] = it->first.c_str();
                if( infos.size()>0 && !infos["type"].isVoid() ) {
                    item["type"] = infos["type"];
                }
                else {
                    item["type"] = "unknown";
                }
                devicesList.push_back(item);
            }
            returnval["devices"] = devicesList;
        }
        else if( cmd=="remove" )
        {
            //remove specified device
            if( !command["device"].isVoid() )
            {
                //command["device"] format: {internalid:<>, type:<>}
                qpid::types::Variant::Map device = command["device"].asMap();
                if( !device["internalid"].isVoid() )
                {
                    if( deleteDevice(device["internalid"].asString()) )
                    {
                        returnval["error"] = 0;
                        returnval["msg"] = "";
                    }
                    else {
                        returnval["error"] = 7;
                        returnval["msg"] = "Unable to delete sensor";
                    }
                }
                else
                {
                    //invalid command format
                    returnval["error"] = 6;
                    returnval["msg"] = "Invalid command received";
                }
            }
            else
            {
                //device id is missing
                returnval["error"] = 4;
                returnval["msg"] = "Device is missing";
            }
        }
        else
        {
            //get device infos
            infos = getDeviceInfos(command["internalid"].asString());
            //check if device found
            if( infos.size()>0 )
            {
                deviceType = infos["type"].asString();
                //switch according to specific device type
                if( deviceType=="switch" )
                {
                    if( cmd=="off" )
                    {
                        if( infos["value"].asString()=="1" )
                        {
                            if( infos["protocol"].asString()=="1.4" )
                                sendcommandV14(internalid, SET_V14, 1, V_LIGHT_V14, "0");
                            else
                                sendcommandV13(internalid, SET_VARIABLE_V13, V_LIGHT_V13, "0");
                        }
                        else
                        {
                            if( DEBUG ) cout << " -> Command dropped (value is the same)" << endl;
                        }
                    }
                    else if( cmd=="on" )
                    {
                        if( infos["value"].asString()=="0" )
                        {
                            if( infos["protocol"].asString()=="1.4" )
                                sendcommandV14(internalid, SET_V14, 1, V_LIGHT_V14, "1");
                            else
                                sendcommandV13(internalid, SET_VARIABLE_V13, V_LIGHT_V13, "1");
                        }
                        else 
                        {
                            if( DEBUG ) cout << " -> Command dropped (value is the same)" << endl;
                        }
                    }
                }
                //TODO add more device type here
                returnval["error"] = 0;
                returnval["msg"] = "";
            }
            else
            {
                //internalid doesn't belong to this controller
                returnval["error"] = 5;
                returnval["msg"] = "Unmanaged internalid";
            }
        }
    }
    return returnval;
}

/**
 * Read line from Serial
 */
std::string readLine(bool* error) {
    char c;
    std::string result;
    (*error) = false;
    try {
        for(;;) {
            boost::asio::read(serialPort,boost::asio::buffer(&c,1));
            switch(c) {
                case '\r':
                    break;
                case '\n':
                    return result;
                default:
                    result+=c;
            }
        }
    }
    catch (std::exception& e) {
        cerr << "Unable to read line: " << e.what() << endl;
        (*error) = true;
    }
    return result;
}

/**
 * Create new device (called during init or when PRESENTATION message is received from MySensors gateway)
 */
void newDevice(std::string internalid, std::string devicetype)
{
    //init
    qpid::types::Variant::Map devices = devicemap["devices"].asMap();
    qpid::types::Variant::Map infos = getDeviceInfos(internalid);

    if( infos.size()>0 )
    {
        //internalid already referenced
        if( infos["type"].asString()!=devicetype )
        {
            //sensors is probably reconditioned, remove it before adding it
            cout << "Reconditioned sensor detected (" << infos["type"] << "=>" << devicetype << ")" << endl;
            deleteDevice(internalid);
            //refresh infos
            infos = getDeviceInfos(internalid);
            //and add brand new device
            devices = devicemap["devices"].asMap();
            addDevice(internalid, devicetype, devices, infos);
        }
        else if( infos["protocol"].asString()!=gateway_protocol_version )
        {
            //sensors code was updated to different protocol
            cout << "Sensor protocol changed (" << infos["protocol"] << "=>" << gateway_protocol_version << ")" << endl;
            //refresh infos
            infos = getDeviceInfos(internalid);
            if( infos.size()>0 )
            {
                infos["protocol"] = gateway_protocol_version;
                setDeviceInfos(internalid, &infos);
            }
        }
        else
        {
            //sensor has just rebooted
            addDevice(internalid, devicetype, devices, infos);
        }
    }
    else
    {
        //add new device
        addDevice(internalid, devicetype, devices, infos);
    }
}

/**
 * Resend function (threaded)
 * Allow to send again a command until ack is received (only for certain device type)
 */
void* resendFunction(void* param)
{
    qpid::types::Variant::Map infos;
    while(1)
    {
        pthread_mutex_lock(&resendMutex);
        for( std::map<std::string,T_COMMAND>::iterator it=commandsmap.begin(); it!=commandsmap.end(); it++ )
        {
            if( it->second.attempts<RESEND_MAX_ATTEMPTS )
            {
                //resend command
                sendcommand(it->second.command);
                it->second.attempts++;
                //update counter
                infos = getDeviceInfos(it->first);
                if( infos.size()>0 )
                {
                    if( infos["counter_retries"].isVoid() )
                    {
                        infos["counter_retries"] = 1;
                    }
                    else
                    {
                        infos["counter_retries"] = infos["counter_retries"].asUint64()+1;
                    }
                    setDeviceInfos(it->first, &infos);
                }
            }
            else
            {
                //max attempts reached
                cout << "Too many attemps. Command failed: " << it->second.command;

                //increase counter
                qpid::types::Variant::Map infos = getDeviceInfos(it->first);
                if( infos.size()>0 )
                {
                    if( infos["counter_failed"].isVoid() )
                    {
                        infos["counter_failed"] = 1;
                    }
                    else
                    {
                        infos["counter_failed"] = infos["counter_failed"].asUint64()+1;
                    }
                    setDeviceInfos(it->first, &infos);
                }

                //delete command from list
                std::map<std::string, T_COMMAND>::iterator cmd = commandsmap.find(it->first);
                if( cmd!=commandsmap.end() )
                {
                    commandsmap.erase(cmd);
                }
                else
                {
                    cout << "ERROR: command not found in map!. Unable to delete" << endl;
                }

            }
        }
        pthread_mutex_unlock(&resendMutex);
        usleep(500000);
    }
    return NULL;
}

/**
 * Process message of protocol v1.3
 */
void processMessageV13(int radioId, int childId, int messageType, int subType, std::string payload, std::string internalid, qpid::types::Variant::Map infos)
{
    int valid = 0;
    stringstream id;
    stringstream timestamp;
    int freeid;

    switch (messageType)
    {
        case INTERNAL_V13:
            switch (subType)
            {
                case I_BATTERY_LEVEL_V13:
                    if( infos.size()==0 )
                    {
                        //create device
                        newDevice(internalid, "batterysensor");
                    }

                    //increase counter
                    if( infos.size()>0 )
                    {
                        if( infos["counter_received"].isVoid() )
                        {
                            infos["counter_received"] = 1;
                        }
                        else
                        {
                            infos["counter_received"] = infos["counter_received"].asUint64()+1;
                        }
                        infos["last_timestamp"] = (int)(time(NULL));
                        setDeviceInfos(internalid, &infos);
                    }

                    //emit battery level
                    agoConnection->emitEvent(internalid.c_str(), "event.device.batterylevelchanged", payload.c_str(), "percent");
                    break;
                case I_TIME_V13:
                    timestamp << time(NULL);
                    sendcommandV13(internalid, INTERNAL_V13, I_TIME_V13, timestamp.str());
                    break;
                case I_REQUEST_ID_V13:
                    //return radio id to sensor
                    freeid = devicemap["nextid"];
                    //@info nodeId - The unique id (1-254) for this sensor. Default 255 (auto mode).
                    if( freeid>=254 ) {
                        cerr << "FATAL: no nodeId available!" << endl;
                    }
                    else
                    {
                        pthread_mutex_lock(&devicemapMutex);
                        devicemap["nextid"]=freeid+1;
                        variantMapToJSONFile(devicemap, getConfigPath(DEVICEMAPFILE));
                        pthread_mutex_unlock(&devicemapMutex);
                        id << freeid;
                        sendcommandV13(internalid, INTERNAL_V13, I_REQUEST_ID_V13, id.str());
                    }
                    break;
                case I_PING_V13:
                    sendcommandV13(internalid, INTERNAL_V13, I_PING_ACK_V13, "");
                    break;
                case I_UNIT_V13:
                    sendcommandV13(internalid, INTERNAL_V13, I_UNIT_V13, units);
                    break;
                default:
                    cout << "INTERNAL subtype '" << subType << "' not supported (protocol v1.3)" << endl;
            }
            break;

        case PRESENTATION_V13:
            cout << "PRESENTATION: " << subType << endl;
            switch (subType)
            {
                case S_DOOR_V13:
                case S_MOTION_V13:
                    newDevice(internalid, "binarysensor");
                    break;
                case S_SMOKE_V13:
                    newDevice(internalid, "smokedetector");
                    break;
                case S_LIGHT_V13:
                case S_HEATER_V13:
                    newDevice(internalid, "switch");
                    break;
                case S_DIMMER_V13:
                    newDevice(internalid, "dimmer");
                    break;
                case S_COVER_V13:
                    newDevice(internalid, "drapes");
                    break;
                case S_TEMP_V13:
                    newDevice(internalid, "temperaturesensor");
                    break;
                case S_HUM_V13:
                    newDevice(internalid, "humiditysensor");
                    break;
                case S_BARO_V13:
                    newDevice(internalid, "barometricsensor");
                    break;
                case S_WIND_V13:
                    newDevice(internalid, "windsensor");
                    break;
                case S_RAIN_V13:
                    newDevice(internalid, "rainsensor");
                    break;
                case S_UV_V13:
                    newDevice(internalid, "uvsensor");
                    break;
                case S_WEIGHT_V13:
                    newDevice(internalid, "weightsensor");
                    break;
                case S_POWER_V13:
                    newDevice(internalid, "powermeter");
                    break;
                case S_DISTANCE_V13:
                    newDevice(internalid, "distancesensor");
                    break;
                case S_LIGHT_LEVEL_V13:
                    newDevice(internalid, "brightnesssensor");
                    break;
                case S_LOCK_V13:
                    newDevice(internalid, "lock");
                    break;
                case S_IR_V13:
                    newDevice(internalid, "infraredblaster");
                    break;
                case S_WATER_V13:
                    newDevice(internalid, "watermeter");
                    break;
                default:
                    cout << "PRESENTATION subtype '" << subType << "' not supported (protocol v1.3)" << endl;
            }
            break;

        case REQUEST_VARIABLE_V13:
            if( infos.size()>0 )
            {
                //increase counter
                if( infos["counter_sent"].isVoid() ) {
                    infos["counter_sent"] = 1;
                }
                else
                {
                    infos["counter_sent"] = infos["counter_sent"].asUint64()+1;
                }
                setDeviceInfos(internalid, &infos);
                //send value
                sendcommandV13(internalid, SET_VARIABLE_V13, subType, infos["value"]);
            }
            else
            {
                //device not found
                //TODO log flood!
                cerr  << "Device not found: unable to get its value" << endl;
            }
            break;

        case SET_VARIABLE_V13:
            //remove command from map to avoid sending command again
            pthread_mutex_lock(&resendMutex);
            if( commandsmap.count(internalid)!=0 )
            {
                commandsmap.erase(internalid);
            }
            pthread_mutex_unlock(&resendMutex);

            //increase counter
            if( infos.size()>0 )
            {
                if( infos["counter_received"].isVoid() )
                {
                    infos["counter_received"] = 1;
                }
                else
                {
                    infos["counter_received"] = infos["counter_received"].asUint64()+1;
                }
                infos["last_timestamp"] = (int)(time(NULL));
                setDeviceInfos(internalid, &infos);
            }

            //do something on received event
            switch (subType)
            {
                case V_TEMP_V13:
                    valid = 1;
                    if (units == "M") {
                        agoConnection->emitEvent(internalid.c_str(), "event.environment.temperaturechanged", payload.c_str(), "degC");
                    } else {
                        agoConnection->emitEvent(internalid.c_str(), "event.environment.temperaturechanged", payload.c_str(), "degF");
                    }
                    break;
                case V_TRIPPED_V13:
                    valid = 1;
                    agoConnection->emitEvent(internalid.c_str(), "event.security.sensortriggered", payload == "1" ? 255 : 0, "");
                    break;
                case V_HUM_V13:
                    valid = 1;
                    agoConnection->emitEvent(internalid.c_str(), "event.environment.humiditychanged", payload.c_str(), "percent");
                    break;
                case V_LIGHT_V13:
                    valid = 1;
                    agoConnection->emitEvent(internalid.c_str(), "event.device.statechanged", payload=="1" ? 255 : 0, "");
                    break;
                case V_DIMMER_V13:
                    valid = 1;
                    agoConnection->emitEvent(internalid.c_str(), "event.device.statechanged", payload.c_str(), "");
                    break;
                case V_PRESSURE_V13:
                    valid = 1;
                    agoConnection->emitEvent(internalid.c_str(), "event.environment.pressurechanged", payload.c_str(), "mBar");
                    break;
                case V_FORECAST_V13:
                    valid = 1;
                    agoConnection->emitEvent(internalid.c_str(), "event.environment.forecastchanged", payload.c_str(), "");
                    break;
                case V_RAIN_V13:
                    break;
                case V_RAINRATE_V13:
                    break;
                case V_WIND_V13:
                    break;
                case V_GUST_V13:
                    break;
                case V_DIRECTION_V13:
                    break;
                case V_UV_V13:
                    break;
                case V_WEIGHT_V13:
                    break;
                case V_DISTANCE_V13: 
                    valid = 1;
                    if (units == "M")
                    {
                        agoConnection->emitEvent(internalid.c_str(), "event.environment.distancechanged", payload.c_str(), "cm");
                    }
                    else
                    {
                        agoConnection->emitEvent(internalid.c_str(), "event.environment.distancechanged", payload.c_str(), "inch");
                    }
                    break;
                case V_IMPEDANCE_V13:
                    break;
                case V_ARMED_V13:
                    break;
                case V_WATT_V13:
                    break;
                case V_KWH_V13:
                    break;
                case V_SCENE_ON_V13:
                    break;
                case V_SCENE_OFF_V13:
                    break;
                case V_HEATER_V13:
                    break;
                case V_HEATER_SW_V13:
                    break;
                case V_LIGHT_LEVEL_V13:
                    valid = 1;
                    agoConnection->emitEvent(internalid.c_str(), "event.environment.brightnesschanged", payload.c_str(), "lux");
                    break;
                case V_VAR1_V13:
                    break;
                case V_VAR2_V13:
                    break;
                case V_VAR3_V13:
                    break;
                case V_VAR4_V13:
                    break;
                case V_VAR5_V13:
                    break;
                case V_UP_V13:
                    break;
                case V_DOWN_V13:
                    break;
                case V_STOP_V13:
                    break;
                case V_IR_SEND_V13:
                    break;
                case V_IR_RECEIVE_V13:
                    break;
                case V_FLOW_V13:
                    break;
                case V_VOLUME_V13:
                    break;
                case V_LOCK_STATUS_V13:
                    break;
                default:
                    break;
            }

            if( valid==1 )
            {
                //save current device value
                infos = getDeviceInfos(internalid);
                if( infos.size()>0 )
                {
                    infos["value"] = payload;
                    setDeviceInfos(internalid, &infos);
                }
            }
            else
            {
                //unsupported sensor
                cerr << "WARN: sensor with subType=" << subType << " not supported yet (protocol v1.3)" << endl;
            }

            //send ack
            sendcommandV13(internalid, VARIABLE_ACK_V13, subType, payload);
            break;

        case VARIABLE_ACK_V13:
            //TODO useful on controller?
            cout << "VARIABLE_ACK" << endl;
            break;

        default:
            break;
    }
}

/**
 * Process message v1.4
 */
void processMessageV14(int nodeId, int childId, int messageType, int ack, int subType, std::string payload, std::string internalid, qpid::types::Variant::Map infos)
{
    int valid = 0;
    stringstream timestamp;
    stringstream id;
    int freeid;
    std::map<std::string, T_COMMAND>::iterator cmd;

    switch (messageType)
    {
        case INTERNAL_V14:
            switch (subType)
            {
                case I_BATTERY_LEVEL_V14:
                    if( infos.size()==0 )
                    {
                        //create device
                        newDevice(internalid, "batterysensor");
                    }

                    //increase counter
                    if( infos.size()>0 )
                    {
                        if( infos["counter_received"].isVoid() )
                        {
                            infos["counter_received"] = 1;
                        }
                        else
                        {
                            infos["counter_received"] = infos["counter_received"].asUint64()+1;
                        }
                        infos["last_timestamp"] = (int)(time(NULL));
                        setDeviceInfos(internalid, &infos);
                    }

                    //emit battery level
                    agoConnection->emitEvent(internalid.c_str(), "event.device.batterylevelchanged", payload.c_str(), "percent");
                    break;
                case I_TIME_V14:
                    timestamp << time(NULL);
                    sendcommandV14(internalid, INTERNAL_V14, 0, I_TIME_V14, timestamp.str());
                    break;
                case I_ID_REQUEST_V14:
                    //return radio id to sensor
                    freeid = devicemap["nextid"];
                    //@info nodeId - The unique id (1-254) for this sensor. Default 255 (auto mode).
                    if( freeid>=254 )
                    {
                        cerr << "FATAL: no nodeId available!" << endl;
                    }
                    else
                    {
                        pthread_mutex_lock(&devicemapMutex);
                        devicemap["nextid"]=freeid+1;
                        variantMapToJSONFile(devicemap, getConfigPath(DEVICEMAPFILE));
                        pthread_mutex_unlock(&devicemapMutex);
                        id << freeid;
                        sendcommandV14(internalid, INTERNAL_V14, 0, I_ID_RESPONSE_V14, id.str());
                    }
                    break;
                case I_LOG_MESSAGE_V14:
                    //debug message from gateway. Already displayed with prettyPrint when debug activated
                    break;
                default:
                    cout << "INTERNAL subtype '" << subType << "' not supported (protocol v1.4)" << endl;
            }
            break;

        case PRESENTATION_V14:
            switch (subType)
            {
                case S_DOOR_V14:
                case S_MOTION_V14:
                    newDevice(internalid, "binarysensor");
                    break;
                case S_SMOKE_V14:
                    newDevice(internalid, "smokedetector");
                    break;
                case S_LIGHT_V14:
                case S_HEATER_V14:
                    newDevice(internalid, "switch");
                    break;
                case S_DIMMER_V14:
                    newDevice(internalid, "dimmer");
                    break;
                case S_COVER_V14:
                    newDevice(internalid, "drapes");
                    break;
                case S_TEMP_V14:
                    newDevice(internalid, "temperaturesensor");
                    break;
                case S_HUM_V14:
                    newDevice(internalid, "humiditysensor");
                    break;
                case S_BARO_V14:
                    newDevice(internalid, "barometricsensor");
                    break;
                case S_WIND_V14:
                    newDevice(internalid, "windsensor");
                    break;
                case S_RAIN_V14:
                    newDevice(internalid, "rainsensor");
                    break;
                case S_UV_V14:
                    newDevice(internalid, "uvsensor");
                    break;
                case S_WEIGHT_V14:
                    newDevice(internalid, "weightsensor");
                    break;
                case S_POWER_V14:
                    newDevice(internalid, "powermeter");
                    break;
                case S_DISTANCE_V14:
                    newDevice(internalid, "distancesensor");
                    break;
                case S_LIGHT_LEVEL_V14:
                    newDevice(internalid, "brightnesssensor");
                    break;
                case S_LOCK_V14:
                    newDevice(internalid, "lock");
                    break;
                case S_IR_V14:
                    newDevice(internalid, "infraredblaster");
                    break;
                case S_AIR_QUALITY_V14:
                    newDevice(internalid, "airsensor");
                    break;
                case S_CUSTOM_V14:
                    cout << "Device type 'CUSTOM' cannot be implemented in agocontrol" << endl;
                    break;
                case S_DUST_V14:
                    newDevice(internalid, "dustsensor");
                    break;
                case S_SCENE_CONTROLLER_V14:
                    newDevice(internalid, "scenecontroller");
                    break;
                default:
                    cout << "PRESENTATION subtype '" << subType << "' not supported (protocol v1.4)" << endl;
            }
            break;

        case REQ_V14:
            if( infos.size()>0 )
            {
                //increase counter
                if( infos["counter_sent"].isVoid() )
                {
                    infos["counter_sent"] = 1;
                }
                else
                {
                    infos["counter_sent"] = infos["counter_sent"].asUint64()+1;
                }
                setDeviceInfos(internalid, &infos);
                //send value
                sendcommandV14(internalid, SET_V14, 0, subType, infos["value"]);
            }
            else
            {
                //device not found
                //TODO log flood!
                cerr  << "Device not found: unable to get its value" << endl;
            }
            break;

        case SET_V14:
            //remove command from map to avoid sending command again
            pthread_mutex_lock(&resendMutex);
            cmd = commandsmap.find(internalid);
            if( cmd!=commandsmap.end() && ack==1 )
            {
                //command exists in command list for this device and its an ack
                //remove command from list
                cout << "Ack received for command " << cmd->second.command;
                commandsmap.erase(cmd);
                ack = 0;
            }
            pthread_mutex_unlock(&resendMutex);

            //increase counter
            if( infos.size()>0 )
            {
                if( infos["counter_received"].isVoid() )
                {
                    infos["counter_received"] = 1;
                }
                else
                {
                    infos["counter_received"] = infos["counter_received"].asUint64()+1;
                }
                infos["last_timestamp"] = (int)(time(NULL));
                setDeviceInfos(internalid, &infos);
            }

            //do something on received event
            switch (subType)
            {
                case V_TEMP_V14:
                    valid = 1;
                    if (units == "M")
                    {
                        agoConnection->emitEvent(internalid.c_str(), "event.environment.temperaturechanged", payload.c_str(), "degC");
                    }
                    else
                    {
                        agoConnection->emitEvent(internalid.c_str(), "event.environment.temperaturechanged", payload.c_str(), "degF");
                    }
                    break;
                case V_TRIPPED_V14:
                    valid = 1;
                    agoConnection->emitEvent(internalid.c_str(), "event.security.sensortriggered", payload == "1" ? 255 : 0, "");
                    break;
                case V_HUM_V14:
                    valid = 1;
                    agoConnection->emitEvent(internalid.c_str(), "event.environment.humiditychanged", payload.c_str(), "percent");
                    break;
                case V_LIGHT_V14:
                    valid = 1;
                    agoConnection->emitEvent(internalid.c_str(), "event.device.statechanged", payload=="1" ? 255 : 0, "");
                    break;
                case V_DIMMER_V14:
                    valid = 1;
                    agoConnection->emitEvent(internalid.c_str(), "event.device.statechanged", payload.c_str(), "");
                    break;
                case V_PRESSURE_V14:
                    valid = 1;
                    agoConnection->emitEvent(internalid.c_str(), "event.environment.pressurechanged", payload.c_str(), "mBar");
                    break;
                case V_FORECAST_V14:
                    valid = 1;
                    agoConnection->emitEvent(internalid.c_str(), "event.environment.forecastchanged", payload.c_str(), "");
                    break;
                case V_RAIN_V14:
                    break;
                case V_RAINRATE_V14:
                    break;
                case V_WIND_V14:
                    break;
                case V_GUST_V14:
                    break;
                case V_DIRECTION_V14:
                    break;
                case V_UV_V14:
                    break;
                case V_WEIGHT_V14:
                    break;
                case V_DISTANCE_V14: 
                    valid = 1;
                    if (units == "M")
                    {
                        agoConnection->emitEvent(internalid.c_str(), "event.environment.distancechanged", payload.c_str(), "cm");
                    }
                    else
                    {
                        agoConnection->emitEvent(internalid.c_str(), "event.environment.distancechanged", payload.c_str(), "inch");
                    }
                    break;
                case V_IMPEDANCE_V14:
                    break;
                case V_ARMED_V14:
                    break;
                case V_WATT_V14:
                    break;
                case V_KWH_V14:
                    break;
                case V_SCENE_ON_V14:
                    break;
                case V_SCENE_OFF_V14:
                    break;
                case V_HEATER_V14:
                    break;
                case V_HEATER_SW_V14:
                    break;
                case V_LIGHT_LEVEL_V14:
                    valid = 1;
                    agoConnection->emitEvent(internalid.c_str(), "event.environment.brightnesschanged", payload.c_str(), "lux");
                    break;
                case V_VAR1_V14:
                    break;
                case V_VAR2_V14:
                    break;
                case V_VAR3_V14:
                    break;
                case V_VAR4_V14:
                    break;
                case V_VAR5_V14:
                    break;
                case V_UP_V14:
                    break;
                case V_DOWN_V14:
                    break;
                case V_STOP_V14:
                    break;
                case V_IR_SEND_V14:
                    break;
                case V_IR_RECEIVE_V14:
                    break;
                case V_FLOW_V14:
                    break;
                case V_VOLUME_V14:
                    break;
                case V_LOCK_STATUS_V14:
                    break;
                default:
                    break;
            }

            if( valid==1 )
            {
                //save current device value
                infos = getDeviceInfos(internalid);
                if( infos.size()>0 )
                {
                    infos["value"] = payload;
                    setDeviceInfos(internalid, &infos);
                }
            }
            else
            {
                //unsupported sensor
                cerr << "WARN: sensor with subType=" << subType << " not supported yet" << endl;
            }

            //send ack if necessary
            if( ack )
            {
                sendcommandV14(internalid, SET_V14, 0, subType, payload);
            }
            break;

        case STREAM_V14:
            //TODO nothing implemented in MySensor yet
            cout << "STREAM" << endl;
            break;
        default:
            break;
    }
}

/**
 * Serial read function (threaded)
 */
void *receiveFunction(void *param) {
    bool error = false;
    std::string log = "";

    while (1)
    {
        pthread_mutex_lock (&serialMutex);

        //read line
        std::string line = readLine(&error);

        //check errors on serial port
        if( error )
        {
            //error occured! close port
            cout << "Reconnecting to serial port" << endl;
            closeSerialPort();
            //pause (100ms)
            usleep(100000);
            //and reopen it
            openSerialPort(device);

            pthread_mutex_unlock (&serialMutex);
            continue;
        }

        if( DEBUG )
        {
            log = prettyPrint(line);
            if( log.size()>0 )
            {
                time_t t = time(NULL);
                cout << " => " << timestampToStr(&t) << " RECEIVING: " << log;
            }
        }

        std::vector<std::string> items = split(line, ';');
        if ( items.size()>=4 && items.size()<=6 )
        {
            int nodeId = atoi(items[0].c_str());
            int childId = atoi(items[1].c_str());
            string internalid = items[0] + "/" + items[1];
            int messageType = atoi(items[2].c_str());
            int subType;
            qpid::types::Variant::Map infos;
            std::string payload = "";

            //get device infos
            infos = getDeviceInfos(internalid);

            //process message
            if( gateway_protocol_version=="1.4" )
            {
                int ack = atoi(items[3].c_str());
                subType = atoi(items[4].c_str());
                if( items.size()==6 )
                    payload = items[5];
                processMessageV14(nodeId, childId, messageType, ack, subType, payload, internalid, infos);
            }
            else if( gateway_protocol_version=="1.3" )
            {
                subType = atoi(items[3].c_str());
                if( items.size()==5 )
                    payload = items[4];
                processMessageV13(nodeId, childId, messageType, subType, payload, internalid, infos);
            }
            else
            {
                //protocol not supported
                cout << "Error: protocol version '" << gateway_protocol_version << "' not supported" << endl;
            }
        }

        pthread_mutex_unlock (&serialMutex);
    }

    return NULL;
}

/**
 * Device stale checking thread
 * Check stale state every minutes
 */
void *checkStale(void *param)
{
    while(true)
    {
        //get current time
        time_t now = time(NULL);

        qpid::types::Variant::Map devices = devicemap["devices"].asMap();
        for (qpid::types::Variant::Map::const_iterator it = devices.begin(); it != devices.end(); it++)
        {
            qpid::types::Variant::Map infos = it->second.asMap();
            std::string internalid = (std::string)it->first;
            if( !infos["last_timestamp"].isVoid() )
            {
                if( !agoConnection->isDeviceStale(internalid.c_str()) )
                {
                    if( (int)now>(infos["last_timestamp"].asInt32()+staleThreshold) )
                    {
                        //device is stalled
                        //cout << "suspend device " << internalid << " last_ts=" << infos["last_timestamp"].asInt32() << " threshold=" << staleThreshold << " now=" << (int)now << endl;
                        //cout << "isstale? " << agoConnection->isDeviceStale(internalid.c_str()) << endl;
                        agoConnection->suspendDevice(internalid.c_str());
                    }
                }
                else
                {
                    if( infos["last_timestamp"].asInt32()>=((int)now-staleThreshold) )
                    {
                        //device woke up
                        //cout << "resume device " << internalid << " last_ts=" << infos["last_timestamp"].asInt32() << " threshold=" << staleThreshold << " now=" << (int)now << endl;
                        agoConnection->resumeDevice(internalid.c_str());
                    }
                }
            }
        }

        //pause (1min)
        sleep(60);
    }

    return NULL;
}

/**
 * main
 */
int main(int argc, char **argv)
{
    //exception handling (trace back)
    /*struct sigaction sigact;
    sigact.sa_sigaction = crit_err_hdlr;
    sigact.sa_flags = SA_RESTART | SA_SIGINFO;
    if (sigaction(SIGABRT, &sigact, (struct sigaction *)NULL) != 0)
    {
        std::cerr << "error setting handler for signal " << SIGABRT << " (" << strsignal(SIGABRT) << ")\n";
        exit(EXIT_FAILURE);
    }*/

    //get config
    device = getConfigOption("mysensors", "device", "/dev/ttyACM0");
    staleThreshold = atoi(getConfigOption("mysensors", "staleThreshold", "86400").c_str());

    //get command line parameters
    bool continu = true;
    do
    {
        switch(getopt(argc,argv,"dgh"))
        {
            case 'd': 
                //activate debug
                DEBUG = 1;
                cout << "DEBUG activated" << endl;
                break;
            case 'g':
                //activate gateway debug message
                DEBUG_GW = 1;
                cout << "DEBUG Gateway activated" << endl;
                break;
            case 'h':
                //usage
                cout << "Usage: agoMySensors [-dgh]" << endl;
                cout << "Options:" << endl;
                cout << " -d: display debug message" << endl;
                cout << " -g: display debug message from MySensors Gateway" << endl;
                cout << " -h: this help" << endl;
                exit(0);
                break;
            default:
                continu = false;
                break;
        }
    } while(continu);

    // determine reply for INTERNAL;I_UNIT message - defaults to "M"etric
    if (getConfigOption("system","units","SI")!="SI") units="I";

    //open serial port
    if( DEBUG )
        cout << "Opening serial port '" << device << "'..." << endl;
    if( !openSerialPort(device) )
    {
        exit(1);
    }

    // load map, create sections if empty
    fs::path dmf = getConfigPath(DEVICEMAPFILE);
    devicemap = jsonFileToVariantMap(dmf);
    if (devicemap["nextid"].isVoid())
    {
        devicemap["nextid"]=1;
        variantMapToJSONFile(devicemap, dmf);
    }
    if (devicemap["devices"].isVoid())
    {
        qpid::types::Variant::Map devices;
        devicemap["devices"]=devices;
        variantMapToJSONFile(devicemap, dmf);
    }

    bool error;
    cout << "Requesting gateway version...";
    std::string getVersion = "0;0;3;0;2\n";
    serialPort.write_some(buffer(getVersion));
    std::string line = readLine(&error);
    if( !error )
    {
        std::vector<std::string> items = split(line, ';');
        if( items.size()>0 )
        {
            //payload (last field, contains protocol version)
            gateway_protocol_version = items[items.size()-1].c_str();
            //check protocol version
            if( gateway_protocol_version!="1.4" && gateway_protocol_version!="1.3" )
            {
                //unknown protocol version, exit now
                cout << "Unknown gateway protocol version. Exit" << endl;
                exit(1);
            }
            else
            {
                cout << " found v" << gateway_protocol_version << endl;
            }
        }
    }

    //init agocontrol client
    cout << "Initializing MySensors controller" << endl;
    agoConnection = new AgoConnection("mysensors");
    agoConnection->addDevice("mysensorscontroller", "mysensorscontroller");
    agoConnection->addHandler(commandHandler);

    //init threads
    pthread_mutex_init(&serialMutex, NULL);
    pthread_mutex_init(&resendMutex, NULL);
    pthread_mutex_init(&devicemapMutex, NULL);
    if( pthread_create(&resendThread, NULL, resendFunction, NULL) < 0 ) {
        cerr << "Unable to create resend thread (errno=" << errno << ")" << endl;
        exit(1);
    }
    if( pthread_create(&readThread, NULL, receiveFunction, NULL) < 0 ) {
        cerr << "Unable to create read thread (errno=" << errno << ")" << endl;
        exit(1);
    }

    //register existing devices
    cout << "Register existing devices:" << endl;
    qpid::types::Variant::Map devices = devicemap["devices"].asMap();
    for (qpid::types::Variant::Map::const_iterator it = devices.begin(); it != devices.end(); it++) {
        qpid::types::Variant::Map infos = it->second.asMap();
        std::string internalid = (std::string)it->first;
        cout << " - " << internalid << ":" << infos["type"].asString().c_str();
        if( internalid.length()>0 && checkInternalid(internalid) )
        {
            agoConnection->addDevice(it->first.c_str(), (infos["type"].asString()).c_str());
        }
        else
        {
            cout << " [INVALID]";
        }
        cout << endl;
    }

    //run check stale thread
    static pthread_t checkStaleThread;
    pthread_create(&checkStaleThread, NULL, checkStale, NULL);

    //run client
    cout << "Running MySensors controller..." << endl;
    agoConnection->run();
}
