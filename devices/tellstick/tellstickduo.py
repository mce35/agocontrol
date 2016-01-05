#!/usr/bin/python
AGO_TELLSTICK_VERSION = '0.0.9'
"""
############################################
#
# Tellstick Duo class
#
# Date of origin: 2014-01-25
#
__author__     = "Joakim Lindbom"
__copyright__  = "Copyright 2014, Joakim Lindbom"
__credits__    = ["Joakim Lindbom", "The ago control team"]
__license__    = "GPL Public License Version 3"
__maintainer__ = "Joakim Lindbom"
__email__      = 'Joakim.Lindbom@gmail.com'
__status__     = "Experimental"
__version__    = AGO_TELLSTICK_VERSION
############################################
"""
from tellstickbase import tellstickbase
import td

class tellstickduo(tellstickbase):
    """Class used for Tellstick & Tellstick Duo devices"""
    def __get__(self, obj, objtype=None):
        pass

    def __set__(self, obj, val):
        pass

    def __delete__(self, obj):
        pass

    def init(self, SensorPollDelay, TempUnits):
        #TELLSTICK_BELL | TELLSTICK_TOGGLE | TELLSTICK_LEARN | TELLSTICK_EXECUTE | TELLSTICK_UP | TELLSTICK_DOWN | TELLSTICK_STOP
        td.init( defaultMethods = td.TELLSTICK_TURNON | td.TELLSTICK_TURNOFF | td.TELLSTICK_DIM)
        self.log.info("Init executed")

    def close(self):
        return td.close()

    def turnOn(self, devId):
        resCode = td.turnOn(devId)
        return self.getErrorString(resCode).lower()

    def turnOff(self, devId):
        resCode =  td.turnOff(devId)
        return self.getErrorString(resCode).lower()

    def getErrorString(self, resCode):
        return td.getErrorString(resCode)

    def dim(self, devId, level):
        resCode = td.dim(devId, level)
        return self.getErrorString(resCode).lower()

    def getName(self,devId):
        return td.getName(devId)

    def methodsReadable(self, method, default):
        return td.methodsReadable(method, default)

    def getNumberOfDevices(self):
        return td.getNumberOfDevices()

    def getNumberOfSensors(self):
        return td.getNumberOfDevices() #wrong


    def getDeviceId(self,i):
        return td.getDeviceId(i)

    def getModel(self, devId):
        return td.getModel(devId)

    def registerDeviceEvent(self, deviceEvent):
        return td.registerDeviceEvent(deviceEvent)

    def registerDeviceChangedEvent(self, deviceEvent):
        return td.registerDeviceChangedEvent(deviceEvent)

    def SensorEventInterceptor (self, protocol, model, id, dataType, value, timestamp, callbackId):
        devId = 'S' + str(id)       # Prefix 'S' to make sure name doesn't clash with self-defined devices
        devIdT = devId + "-temp"
        devIdH = devId + "-hum"
        #self.checkIgnore(self, devId) #TODO: Add once moved
        self.log.trace ("SensorEventInterceptor called for " + devId)

        if devId not in self.ignoreDevices:
            # New temperature sensor?
            if devIdT not in self.sensors and dataType & td.TELLSTICK_TEMPERATURE == td.TELLSTICK_TEMPERATURE:
                s = {}
                s["id"] =devIdT
                s["model"]= model
                self.log.debug("New sensor intercepted: devId=" + devIdT + " model=" + model)
                s["new"] = True
                s["temp"] = float(value) # C/F
                s["lastTemp"] = float(-274.0)
                s["isTempSensor"] = True
                s["isHumiditySensor"] = False
                self.sensors[devIdT] = s

            # New humidity sensor?
            if devIdH not in self.sensors and dataType & td.TELLSTICK_HUMIDITY == td.TELLSTICK_HUMIDITY:
                s = {}
                s["id"] =devIdH
                s["model"]= model
                self.log.debug("New sensor intercepted: devId=" + devIdH + " model=" + model)
                s["new"] = True
                s["humidity"] = float(value)
                s["lastHumidity"] = float(-999.0)
                s["isHumiditySensor"] = True
                s["isTempSensor"] = False
                self.sensors[devIdH] = s

            # Call registered callback
            self.SensorEvent(protocol, model, devId, dataType, value, timestamp, callbackId)

    def registerSensorEvent(self, deviceEvent):
        self.SensorEvent = deviceEvent
        return td.registerSensorEvent(self.SensorEventInterceptor)

    def listSensors(self):
        if len(self.sensors) == 0:
            self.sensors = td.listSensors()

        return self.sensors


    def listSwitches(self):
        if len(self.switches) == 0:

            for i in range(self.getNumberOfDevices()):
                devId = self.getDeviceId(i)
                model = self.getModel(devId)

                if ('switch' in model or 'dimmer' in model):
                    dev = {}
                    dev["id"] = devId
                    dev["name"] = self.getName(devId)
                    dev["model"] = model
                    if 'dimmer' in model:
                        dev["isDimmer"] = True
                    else:
                        dev["isDimmer"] = False

                    self.switches[devId] = dev
        return self.switches

    def listRemotes(self):
        if len(self.remotes) == 0:
            for i in range(self.getNumberOfDevices()):
                devId = self.getDeviceId(i)
                model = self.getModel(devId)
                if 'switch' not in model and 'dimmer' not in model:
                    dev = {}
                    dev["id"] = devId
                    dev["name"] = self.getName(devId)
                    dev["model"] = model

                    self.remotes[devId] = dev
        return self.remotes
