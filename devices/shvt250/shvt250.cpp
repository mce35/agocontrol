/*
   Copyright (C) 2015 Harald Klein <hari@vt100.at>

   This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

   See the GNU General Public License for more details.

*/
#include <iostream>
#include <sstream>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <termios.h>
#include <stdio.h>

#include "agoapp.h"

using namespace qpid::types;
using namespace std;
using namespace agocontrol;

class AgoShvt250: public AgoApp {
private:
    void setupApp();

    int serial_read (int dev,uint8_t pnt[],int len,long timeout);
    void receiveFunction();
    int fd;
public:
    AGOAPP_CONSTRUCTOR(AgoShvt250);
};

int AgoShvt250::serial_read (int dev,uint8_t pnt[],int len,long timeout) {
    int bytes = 0;
    int total = 0;
    struct timeval tv;
    fd_set fs;

    while (total < len) {
        FD_ZERO (&fs);
        FD_SET (dev,&fs);
        tv.tv_sec = 0;
        tv.tv_usec = 2000000;
        // tv.tv_usec = 100000;
        // bytes = select (FD_SETSIZE,&fs,NULL,NULL,&tv);
        bytes = select (dev+1,&fs,NULL,NULL,&tv);

        // 0 bytes or error
        if( bytes <= 0 )
        {
            return total;
        }

        bytes = read (dev,pnt+total,len-total);
        total += bytes;
    }
    return total;
}

void AgoShvt250::receiveFunction() {
    uint8_t bufr[1024];
    int old_co2;
    float old_hum;
    float old_temp;

    while (1) { 
        for (int i=0;i<1020;i++) {
            bufr[i]=0;
        }
        AGO_TRACE() << "reading serial port";
        int len = serial_read(fd, bufr, 24, 6);
        if (len==24) {
            // STX Id sp <---CO2---> sp (-) <tmp> "." <tmp> sp <hum> "." <hum>  sp SHT Mod Chk
            // 2  0   20 30 38 32 37 20 20  32 38 2e  33    20 33 32 2e  37     20 30  30  68   d a 
            // 
            // for (int i=0; i<len; i++) cout << std::hex << (int) bufr[i] << " ";

            if (bufr[0] == 0x2) {
                // SOF found
                unsigned char checksum = 0;
                for (int i=0;i<21;i++) checksum += bufr[i];
                if ((int) checksum == (int) bufr[21]) {
                    // valid checksum
                    stringstream tmp_co2;
                    tmp_co2 << bufr[3] << bufr[4] << bufr[5] << bufr[6];
                    int co2 = atoi(tmp_co2.str().c_str());
                    stringstream tmp_temp;
                    tmp_temp << bufr[8] << bufr[9] << bufr[10] << bufr[11] << bufr[12];
                    float temp = atof(tmp_temp.str().c_str());
                    stringstream tmp_hum;
                    tmp_hum << bufr[14] << bufr[15] << bufr[16] << bufr[17];
                    float hum = atof(tmp_hum.str().c_str());
                    AGO_DEBUG() << "co2: " << co2 << " hum: " << hum << " temp: " << temp ;
                    if (co2 != old_co2) {
                        agoConnection->emitEvent("shvt250-co2", "event.environment.co2changed", co2, "ppm");
                        old_co2 = co2;
                    }
                    if (hum != old_hum) {
                        agoConnection->emitEvent("shvt250-hum", "event.environment.humiditychanged", hum, "percent");
                        old_hum = hum;
                    }
                    if (temp != old_temp) {
                        agoConnection->emitEvent("shvt250-temp", "event.environment.temperaturechanged", temp, "degC");
                        old_temp = temp;
                    }
                    if ((int) bufr[19] != 0x30) AGO_ERROR() << "Sensor measurement error";
                } else {
                    AGO_DEBUG() << "invalid checksum: calculated: " << std::dec << (int) checksum << " frame: " << std::dec << (int) bufr[21];
                }
            } else {
                AGO_DEBUG() << "invalid frame, no STX";
            }
        } else AGO_DEBUG() << "invalid length: " << len;

    }
}



void AgoShvt250::setupApp() {

    fd = open(getConfigOption("device", "/dev/ttyUSB3").c_str(), O_RDWR);
    // TODO: check for error

    agoConnection->addDevice("shvt250-temp", "temperaturesensor");
    agoConnection->addDevice("shvt250-hum", "co2sensor");
    agoConnection->addDevice("shvt250-co2", "humiditysensor");

    // B9600 8n1
    struct termios tio;
    tcgetattr(fd, &tio);
    tio.c_cflag = CS8 | CLOCAL | CREAD;
    tio.c_cc[VMIN] = 1;
    tio.c_cc[VTIME] = 0;
    tio.c_lflag = 0;
    tio.c_iflag = IGNBRK;
    tio.c_oflag = 0;
    cfsetispeed(&tio, B9600);
    cfsetospeed(&tio, B9600);
    tcflush(fd, TCIFLUSH);
    tcsetattr(fd,TCSANOW,&tio);

    boost::thread t(boost::bind(&AgoShvt250::receiveFunction, this));
    t.detach();

}

AGOAPP_ENTRY_POINT(AgoShvt250);
