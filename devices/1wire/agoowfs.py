#! /usr/bin/env python

import sys
import syslog
import ow
import time
import threading

import agoclient

CLIENT = agoclient.AgoConnection("owfs")

DEVICE = agoclient.get_config_option("owfs", "device", "/dev/usbowfs")


# route stderr to syslog
class LogErr:
    def write(self, data):
        syslog.syslog(syslog.LOG_ERR, data)

syslog.openlog(sys.argv[0], syslog.LOG_PID, syslog.LOG_DAEMON)
# sys.stderr = LogErr()

SENSORS = {}

syslog.syslog(syslog.LOG_NOTICE, "agoowfs.py startup")
try:
    ow.init(str(DEVICE))
except ow.exNoController:
    syslog.syslog(syslog.LOG_ERROR, "can't open one wire device, aborting")
    time.sleep(5)
    exit(-1)

syslog.syslog(syslog.LOG_NOTICE, "reading devices")
ROOT = ow.Sensor('/')


for _sensor in ROOT.sensors():
    if _sensor._type == 'DS18S20' or _sensor._type == 'DS18B20':
        CLIENT.add_device(_sensor._path, "multilevelsensor")
    if _sensor._type == 'DS2438':
        try:
            if ow.owfs_get('%s/MultiSensor/type' % _sensor._path) == 'MS-TL':
                CLIENT.add_device(_sensor._path, "multilevelsensor")
            if ow.owfs_get('%s/MultiSensor/type' % _sensor._path) == 'MS-TH':
                CLIENT.add_device(_sensor._path, "multilevelsensor")
            if ow.owfs_get('%s/MultiSensor/type' % _sensor._path) == 'MS-T':
                CLIENT.add_device(_sensor._path, "multilevelsensor")
        except ow.exUnknownSensor, exception:
            print exception
    if _sensor._type == 'DS2406':
        _sensor.PIO_B = '0'
        _sensor.latch_B = '0'
        _sensor.set_alarm = '111'
        CLIENT.add_device(_sensor._path, "switch")
        CLIENT.add_device(_sensor._path, "binarysensor")


def message_handler(internalid, content):
    for sensor in ROOT.sensors():
        if (sensor._path == internalid):
            if "command" in content:
                if content["command"] == "on":
                    print "switching on: ", internalid
                    sensor.PIO_A = '1'
                    CLIENT.emit_event(internalid,
                        "event.device.state", "255", "")
                if content["command"] == "off":
                    print "switching off: ", internalid
                    sensor.PIO_A = '0'
                    CLIENT.emit_event(internalid,
                        "event.device.state", "0", "")


CLIENT.add_handler(message_handler)


class read_bus(threading.Thread):
    def __init__(self,):
        threading.Thread.__init__(self)

    def run(self):
        # wait till devices should be announced
        time.sleep(5)
        while (True):
            try:
                for sensor in ROOT.sensors():
                    if (sensor._type == 'DS18S20' or sensor._type == 'DS18B20'
                        or sensor._type == 'DS2438'):
                        temp = round(float(sensor.temperature), 1)
                        if sensor._path in SENSORS:
                            if 'temp' in SENSORS[sensor._path]:
                                if (abs(SENSORS[sensor._path]['temp'] -
                                        temp) > 0.5):
                                    CLIENT.emit_event(sensor._path,
                                        "event.environment.temperaturechanged",
                                        temp, "degC")
                                    SENSORS[sensor._path]['temp'] = temp
                        else:
                            CLIENT.emit_event(sensor._path,
                                "event.environment.temperaturechanged",
                                temp, "degC")
                            SENSORS[sensor._path] = {}
                            SENSORS[sensor._path]['temp'] = temp
                    if sensor._type == 'DS2438':
                        try:
                            if (ow.owfs_get('%s/MultiSensor/type' %
                                sensor._path) == 'MS-TL'):
                                rawvalue = float(ow.owfs_get('/uncached%s/VAD'
                                    % sensor._path))
                                if rawvalue > 10:
                                    rawvalue = 0
                                lightlevel = int(round(20 * rawvalue))
                                if 'light' in SENSORS[sensor._path]:
                                    if (abs(SENSORS[sensor._path]['light'] -
                                        lightlevel) > 5):
                                        CLIENT.emit_event(sensor._path,
                                            "event.environment.brightnesschanged",
                                            lightlevel, "percent")
                                        SENSORS[sensor._path]['light'] = lightlevel
                                else:
                                    CLIENT.emit_event(sensor._path,
                                        "event.environment.brightnesschanged",
                                        lightlevel, "percent")
                                    SENSORS[sensor._path]['light'] = lightlevel
                            if ow.owfs_get('%s/MultiSensor/type' % sensor._path) == 'MS-TH':
                                humraw = ow.owfs_get('/uncached%s/humidity' % sensor._path)
                                humidity = round(float(humraw))
                                if 'hum' in SENSORS[sensor._path]:
                                    if abs(SENSORS[sensor._path]['hum'] - humidity) > 2:
                                        CLIENT.emit_event(sensor._path,
                                            "event.environment.humiditychanged",
                                            humidity, "percent")
                                        SENSORS[sensor._path]['hum'] = humidity
                                else:
                                    CLIENT.emit_event(sensor._path,
                                        "event.environment.humiditychanged",
                                        humidity, "percent")
                                    SENSORS[sensor._path]['hum'] = humidity
                        except ow.exUnknownSensor, exception:
                            print exception
                    if sensor._type == 'DS2406':
                        if sensor.latch_B == '1':
                            sensor.latch_B = '0'
                            if sensor.sensed_B == '1':
                                CLIENT.emit_event(sensor._path,
                                    "event.security.sensortriggered", 255, "")
                            else:
                                CLIENT.emit_event(sensor._path,
                                    "event.security.sensortriggered", 0, "")
            except ow.exUnknownSensor:
                pass
            time.sleep(2)

BACKGROUND = read_bus()
BACKGROUND.setDaemon(True)
syslog.syslog(syslog.LOG_NOTICE, "starting read_bus() thread")
BACKGROUND.start()

syslog.syslog(syslog.LOG_NOTICE, "passing control to agoclient")
CLIENT.run()
