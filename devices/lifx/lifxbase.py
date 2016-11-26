#!/usr/bin/python
AGO_LIFX_VERSION = '0.0.1'
############################################
#
# LIFX base class
#
# Date of origin: 2016-11-19
#
__author__ = "Joakim Lindbom"
__copyright__ = "Copyright 2016, Joakim Lindbom"
__credits__ = ["Joakim Lindbom", "The ago control team"]
__license__ = "GPL Public License Version 3"
__maintainer__ = "Joakim Lindbom"
__email__ = 'Joakim.Lindbom@gmail.com'
__status__ = "Experimental"
__version__ = AGO_LIFX_VERSION
############################################
import syslog as syslog


class lifxbase(object):
    """Base class used for LIFX devices"""

    def __init__(self, app):
        self.LIFX_TURNON = 1  # check values
        self.LIFX_TURNOFF = 2
        self.LIFX_BELL = 4
        self.LIFX_DIM = 16
        self.LIFX_UP = 128
        self.LIFX_DOWN = 256

        self.sensors = {}
        self.switches = {}
        self.remotes = {}
        self.ignoreModels = {}
        self.ignoreDevices = {}
        self.app = None
        self.log = None

        self.app = app
        try:
            self.log = app.log
        except AttributeError:
            #We seem to be in test mode, need a local logger
            class llog:
                def __init__(self):
                    pass
                def info(self, msg):
                    print ("INFO: %s" % msg)
                def trace(self, msg):
                    print ("TRACE: %s" % msg)
                def debug(self, msg):
                    print ("DEBUG: %s" % msg)
                def error(self, msg):
                    print ("ERROR: %s" % msg)
            self.log = llog()

    def __get__(self, obj, objtype=None):
        pass

    def __set__(self, obj, val):
        pass

    def __delete__(self, obj):
        pass

    def init(self, API_KEY=None, sensor_poll_delay=None, temp_units='C'):
        pass

    def close(self):
        pass

    def turnOn(self, devId):
        pass

    def turnOff(self, devId):
        pass

    def getErrorString(self, resCode):
        pass

    def dim(self, devId, level):
        pass

    def getName(self, devId):
        pass

    def methodsReadable(self, method):
        pass

    def getNumberOfDevices(self):
        pass

    def listSensors(self):
        pass

    def listSwitches(self):
        pass

    def listRemotes(self):
        pass

    def getLightState(self, devId):
        pass

    def getDeviceId(self, i):
        pass

    def getModel(self, devId):
        pass

    def registerDeviceEvent(self, deviceEvent):
        pass

    def registerDeviceChangedEvent(self, deviceEvent):
        pass

    def registerSensorEvent(self, deviceEvent):
        pass

    def checkIgnore(self, devID):  # TODO: Flytta till, agolifx.py???
        if devID in self.ignoreDevices:
            return True
        else:
            return False
