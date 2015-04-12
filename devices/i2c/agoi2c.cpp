#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include <stdlib.h>
#include <sstream>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

#include "agoapp.h"

extern "C"{
#include "BMP085.c"
}

using namespace std;
using namespace agocontrol;

typedef struct {const char *file; int interval;} config_struct;

class AgoI2c: public AgoApp {
private:
    string devicefile;
    config_struct conf;
    int interval;

    void setupApp();
    qpid::types::Variant::Map commandHandler(qpid::types::Variant::Map content);

    void readBMP085();
    bool i2ccommand(const char *device, int i2caddr, int command, size_t size, __u8  *buf);
    bool i2cread(const char *device, int i2caddr, int command, size_t size, __u8 &buf);
    bool set_pcf8574_output(const char *device, int i2caddr, int output, bool state);
    bool get_pcf8574_state(const char *device, int i2caddr, uint8_t &state);
public:
    AGOAPP_CONSTRUCTOR(AgoI2c);
};

//void *readBMP085(void*){
void AgoI2c::readBMP085(){

    AGO_DEBUG() << "void *readBMP085( void* param) i2cFileName: " << devicefile << " interval: " << interval;
    while (true){
        bmp085_Calibration(devicefile.c_str());
        temperature = bmp085_GetTemperature(bmp085_ReadUT(devicefile.c_str()));
        pressure = bmp085_GetPressure(bmp085_ReadUP(devicefile.c_str()));

        stringstream value;
        value << ((double)pressure)/100;
        stringstream value2;
        value2 << ((double)temperature)/10,0x00B0;
        agoConnection->emitEvent("BMP085-baro", "event.environment.pressurechanged", value.str().c_str(), "hPa");
        agoConnection->emitEvent("BMP085-temp", "event.environment.temperaturechanged", value2.str().c_str(), "degC");

        sleep(interval);
    }
}

bool AgoI2c::i2ccommand(const char *device, int i2caddr, int command, size_t size, __u8  *buf) {
    int file = open(device, O_RDWR);
    if (file < 0) {
        AGO_FATAL() << "Cannot open device: " << device << " - error: " << file;
        return false;
    }
    else
        AGO_DEBUG() << "Open succeeded: " << device;

    if (ioctl(file, I2C_SLAVE, i2caddr) < 0) {
        AGO_ERROR() << "Cannot open i2c slave: 0x" << std::hex << i2caddr;
        return false;
    }
    else
        AGO_DEBUG() << "Open i2c slave succeeded: 0x" << std::hex << i2caddr;
    int result = i2c_smbus_write_i2c_block_data(file, command, size,buf);
    AGO_DEBUG() << "result: " << result;

    return true;
}

bool AgoI2c::i2cread(const char *device, int i2caddr, int command, size_t size, __u8 &buf) {
    int file = open(device, O_RDWR);
    if (file < 0) {
        AGO_FATAL() << "Cannot open device: " << device << " - error: " << file;
        return false;
    }
    else
        AGO_DEBUG() << "Open succeeded: " << device;

    if (ioctl(file, I2C_SLAVE, i2caddr) < 0) {
        AGO_ERROR() << "Cannot open i2c slave: 0x" << std::hex << i2caddr;
        return false;
    }
    else
        AGO_DEBUG() << "Open i2c slave succeeded: 0x" << std::hex << i2caddr;

    int result = i2c_smbus_read_i2c_block_data(file, command, size,&buf);
    AGO_DEBUG() << "result: " << result;

    return true;
}

bool AgoI2c::set_pcf8574_output(const char *device, int i2caddr, int output, bool state) {
    unsigned char buf[10];
    int file = open(device, O_RDWR);
    if (file < 0) {
        AGO_FATAL() << "Cannot open device: " << device << " - error: " << file;
        return false;
    }
    else
        AGO_DEBUG() << "Open succeeded: " << device;

    if (ioctl(file, I2C_SLAVE, i2caddr) < 0) {
        AGO_ERROR() << "Cannot open i2c slave: 0x" << std::hex << i2caddr;
        return false;
    }
    else
        AGO_DEBUG() << "Open i2c slave succeeded: 0x" << std::hex << i2caddr;

    if (read(file, buf, 1)!= 1) {
        AGO_ERROR() << "Unable to read from slave: 0x" << std::hex << i2caddr;
        return false;
    }

    if (!state) {
        buf[0] |= (1 << output);
    } else {
        buf[0] &= ~(1 << output);
    }
    if (write(file, buf, 1) != 1) {
        AGO_ERROR() << "Cannot write to i2c slave: 0x" << std::hex << i2caddr;
        return false;
    }

    return true;
}

bool AgoI2c::get_pcf8574_state(const char *device, int i2caddr, uint8_t &state) {
    unsigned char buf[10];
    int file = open(device, O_RDWR);
    if (file < 0) {
        AGO_FATAL() << "Cannot open device: " << device << " - error: " << file;
        return false;
    }
    else
        AGO_DEBUG() << "Open succeeded: " << device;

    if (ioctl(file, I2C_SLAVE, i2caddr) < 0) {
        AGO_ERROR() << "Cannot open i2c slave: 0x" << std::hex << i2caddr;
        return false;
    }
    else
        AGO_DEBUG() << "Open i2c slave succeeded: 0x" << std::hex << i2caddr;

    if (read(file, buf, 1)!= 1) {
        AGO_ERROR() << "Unable to read from slave: 0x" << std::hex << i2caddr;
        return false;
    }
    state = buf[0];
    return true;
}

qpid::types::Variant::Map AgoI2c::commandHandler(qpid::types::Variant::Map content) {
    string internalid = content["internalid"].asString();
    if (internalid.find("pcf8574:") != std::string::npos) {
        unsigned found = internalid.find(":");
        string tmpid = internalid.substr(found+1);
        bool state;
        if (content["command"] == "on") state = true; else state=false;
        unsigned sep = tmpid.find("/");
        if (sep != std::string::npos) {
            int output = atoi(tmpid.substr(sep+1).c_str());
            int i2caddr = strtol(tmpid.substr(0, sep).c_str(), NULL, 16);
            AGO_DEBUG() << "setting output " << output << " to state " << state << " for i2caddr: 0x" << std::hex << i2caddr;
            if (set_pcf8574_output(devicefile.c_str(),i2caddr, output, state)) {
                if (state) { 
                    agoConnection->emitEvent(internalid.c_str(), "event.device.statechanged", "255", "");
                } else {
                    agoConnection->emitEvent(internalid.c_str(), "event.device.statechanged", "0", "");
                }
                return responseSuccess();
            } else return responseError(RESPONSE_ERR_INTERNAL, "Cannot set PCF8574 output");
        }
    }
    return responseUnknownCommand();
}


void AgoI2c::setupApp() {
    devicefile=getConfigOption("bus", "/dev/i2c-0");
    stringstream devices(getConfigOption("devices", "pcf8574:32")); 

    string device;
    while (getline(devices, device, ',')) {
        stringstream tmpdevice(device);
        string type;
        getline(tmpdevice, type, ':');
        if (type == "BMP085") {
            string addrBMP085;
            getline(tmpdevice, addrBMP085, ':');

            agoConnection->addDevice("BMP085-baro", "barometersensor");
            agoConnection->addDevice("BMP085-temp", "temperaturesensor");

            interval = 300; // need to make this configurable

            boost::thread t(boost::bind(&AgoI2c::readBMP085, this));
            t.detach();
        }
        if (type == "pcf8574") {
            string addr;
            getline(tmpdevice, addr, ':');
            uint8_t state;
            if (get_pcf8574_state(devicefile.c_str(), atoi(addr.c_str()), state)!= true) {
                AGO_ERROR() << "can't read pcf8574 state on startup";
            }
            for (int i=0;i<8;i++) {
                stringstream id;
                id << device << "/" << i;
                agoConnection->addDevice(id.str().c_str(), "switch");
                if (state & (1 << i)) {	
                    agoConnection->emitEvent(id.str().c_str(), "event.device.statechanged", "255", "");
                } else {
                    agoConnection->emitEvent(id.str().c_str(), "event.device.statechanged", "0", "");
                }
            }
        }
    } 
    addCommandHandler();

}

AGOAPP_ENTRY_POINT(AgoI2c);
