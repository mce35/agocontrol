#!/usr/bin/python
AGO_LIFX_VERSION = '0.0.1'
############################################
#
# LIFX class supporting device handling via LIFX Cloud APIs
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

from lifxbase import lifxbase
# import sys, getopt, httplib, urllib, json, os, thread, time
# import oauth.oauth as oauth
# import datetime
from agoclient.agoapp import ConfigurationError
import json
import requests


class LifxNet(lifxbase):
    """Class used for Lifx devices via LIFX Cloud API"""

    def __init__(self, app):
        super(LifxNet, self).__init__(app)

        self.devices = {}
        self.models = {}
        self.names = {}
        self.devicesRetrieved = False

    def __get__(self, obj, objtype=None):
        pass

    def __set__(self, obj, val):
        pass

    def __delete__(self, obj):
        pass

    def init(self, API_KEY=None, sensor_poll_delay=None, temp_units='C'):
        self.SUPPORTED_METHODS = self.LIFX_TURNON | self.LIFX_TURNOFF | self.LIFX_DIM

        self.API_KEY = API_KEY
        if not self.API_KEY:
            raise ConfigurationError("API_KEY missing in LIFX.conf. Cannot continue")

        self.headers = {"Authorization": "Bearer %s" % self.API_KEY, }
        return True

    def close(self):
        pass

    def set_state(self, devId, state):
        """Set light to "on" or "off" """
        payload = {"power": state,}
        #self.log.trace(payload)
        response = requests.put('https://api.lifx.com/v1/lights/' + devId + '/state', data=payload, headers=self.headers)
        return self.checkResponse(response)

    def list_lights(self):
        """Get a list of devices for current account"""
        response = requests.get('https://api.lifx.com/v1/lights/all', headers=self.headers)
        #print response.content
        return response

    def getLightState(self, devId):
        """Get state of one light"""
        response = requests.get('https://api.lifx.com/v1/lights/id:{}'.format(devId), headers=self.headers)

        if response.status_code == 200:
            # print response.content
            rsp = response.content
            if "id" in rsp:  # .content:
                rj = json.loads(rsp)
                state= {"power" : rj[0]["power"],
                        "dimlevel" : rj[0]["brightness"],
                       }

                return state

        return None


    def listSwitches(self):
        """Create a dictionary with all lights"""
        ra = self.list_lights()
        rsp = ra.content
        if "id" in rsp:
            rj = json.loads(rsp)
            lights = {}
            for i in rj:
                # print i
                light = {}
                if u'id' in i:
                    dev_id = i["id"]

                    model = i["product"]["name"]  # u'White 800'

                    dev = {
                        "id": dev_id,
                        "name": i["label"],
                        "model": model}
                    if 'White' in model:  # TODO: Replace with Dimmer property
                        dev["isDimmer"] = True
                    else:
                        dev["isDimmer"] = False
                    # light["status"] = "on" if i["connected"] else "Off"
                    self.switches[dev_id] = dev
        return self.switches

    def turnOn(self, devId):
        """ Turn on light"""
        self.log.trace('turnOn {}'.format(devId))
        self.set_state(devId, "on")
        return True

    def turnOff(self, dev_Id):
        """ Turn off light"""
        self.log.trace('turnOff {}'.format(dev_Id))
        self.set_state(dev_Id, "off")
        # return self.doMethod(devId, self.LIFX_TURNOFF)
        return True

    def getErrorString(self, res_code):
        return res_code  # TOT: Remove

    def dim(self, devId, level):
        """ Dim light, level=0-100 """
        self.log.trace('Dim {} level {}'.format(devId, str(float(level/100.0))))
        #TODO: Add support for Duration
        payload = {"power": "on",
                   "brightness": float(level/100.0),
                   "duration": float(1.0), }
        # print payload
        response = requests.put('https://api.lifx.com/v1/lights/' + devId + '/state', data=payload, headers=self.headers)
        # return self.doMethod(devId, self.LIFX_DIM, level)
        return self.checkResponse(response)

    def checkResponse(self, response):
        # Response: < Response[207] >
        # Status code: 207
        # Content: {"results": [{"id": "e111d111f111", "label": "LIFX Bulb 12f116", "status": "offline"}]}

        # Response: < Response[207] >
        # Status code: 207
        # Content: {"results": [{"id": "e111d112f111", "label": "LIFX Bulb 12f116", "status": "ok"}]}

        # print ("Response: %s" % rsp)
        # print ("Status code: %d" % rsp.status_code)
        # print ("Content: %s" % rsp.content)

        if response.status_code == 200:
            return True

        if response.status_code == 207:
            rspj = json.loads(response.content)

            try:
                for r in rspj[u'results']:
                    if r[u'status'] == 'ok':  # TODO: check if this is sufficient
                        return True
                    if r[u'status'] == 'offline':
                        self.log.error("Cloud API return Offline status. Check your connection and if http://api.lifx.com is alive")

            except KeyError:
                self.log.error("Malformed response Code=%d, rsp=%s" % (response.status_code, response.content))

            return False

        def ErrorMessages(error_code):
            sw = {
                400: 'Bad request',
                401: 'Unauthorised, bad access token. Check the API_KEY setting.',
                403: 'Permission denied. Bad OAuth scope. Check the API_KEY setting.',
                404: 'Not found. This is either a bug or LIFX has changed their APIs.',
                422: 'Missing or malformed parameters. This is either a bug or LIFX has changed their APIs.',
                426: 'HTTP was used instead of HTTPS. Likely a bug',
                429: 'Too many requests. Slow down, max 60 requests/minute',
                500: 'Something went wrong on LIFXs end.',
                502: 'Something went wrong on LIFXs end.',
                503: 'Something went wrong on LIFXs end.',
                523: 'Something went wrong on LIFXs end.',
            }
            return sw.get(error_code, "Something went wrong and no message text assigned. Bummer.")

        self.log.error(ErrorMessages(response.status_code) + ' Code={}, rsp={}'.format(response.status_code, response.content))
        return False


    def getName(self, dev_id):
        try:
            return self.names[dev_id]
        except:
            response = self.doRequest('device/info', {'id': dev_id, 'supportedMethods': self.SUPPORTED_METHODS})

            if ('error' in response):
                name = ''
                retString = response['error']
                print ("retString=" + retString)
            else:
                name = response['name']
                self.names[dev_id] = response['name']

            return name

    def getNumberOfDevices(self):
        if len(self.switches) > 0:
            return len(self.switches)
        else:
            self.listSwitches()
            return len(self.switches)

    def getDeviceId(self, i):
        return (self.devices[i])

    def getModel(self, dev_id):
        if dev_id in self.switches:
            s = self.switches[dev_id]
            return s["model"]
        elif dev_id in self.remotes:
            s = self.remotes[dev_id]
            return s["model"]
        elif dev_id in self.sensors:
            s = self.sensors[dev_id]
            return s["model"]

        response = self.doRequest('device/info', {'id': dev_id, 'supportedMethods': self.SUPPORTED_METHODS})

        if ('error' in response):
            model = ''
            retString = response['error']
            print ("retString=" + retString)
        else:
            if response['type'] == 'device':
                self.models[dev_id] = response['model']
            elif response['type'] == "group":
                self.models[dev_id] = 'group'
                # Devices in the group stored in  response['devices']

        return self.models[dev_id]

    # dead code

    def listRemotes(self):
        if not self.devicesRetrieved:
            self.listSwitches()
        return self.remotes

    def listSensors(self):
        response = self.doRequest('sensors/list', {'includeIgnored': 1, 'includeValues': 1})
        self.log.info("Number of sensors: %i" % len(response['sensor']))
        for sensor in response['sensor']:
            if sensor["id"] not in self.sensors:
                s = {}
                devId = str(sensor["id"])
                s["id"] = devId
                s["name"] = sensor["name"]
                s["new"] = True
                if "temp" in sensor:
                    s["isTempSensor"] = True
                    s["temp"] = float(sensor["temp"])  # C/ F
                    s["lastTemp"] = -274.0
                else:
                    s["isTempSensor"] = False

                if "humidity" in sensor:
                    s["id"] = devId
                    s["isHumiditySensor"] = True
                    s["humidity"] = float(sensor["humidity"])
                    s["lastHumidity"] = -999.0
                else:
                    s["isHumiditySensor"] = False

                if "temp" in sensor and "humidity" in sensor:
                    s["isMultiLevel"] = True
                else:
                    s["isMultiLevel"] = False

                self.sensors[devId] = s
                self.names[sensor["id"]] = devId

        return self.sensors

    # def sensorThread(self, sensorCallback, dummy):
    #    pass
