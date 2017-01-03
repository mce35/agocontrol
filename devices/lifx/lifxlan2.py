#!/usr/bin/python
AGO_LIFX_VERSION = '0.0.1'
############################################
#
# LIFX class supporting device handling via LIFX LAN APIs
#
# Date of origin: 2016-12-10
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
import time
from agoclient.agoapp import ConfigurationError
import json
from lifxlan import *
import colorsys

class LIFX_Offline(Exception):
    def __init__(self, msg):
        self.msg = msg

    def __str__(self):
        return repr(self.msg)

class LifxLAN2(lifxbase):
    """Class used for Lifx devices via LIFX LAN API"""

    def __init__(self, app):
        super(LifxLAN2, self).__init__(app)

        self.devices = {}
        self.models = {}
        self.names = {}
        self.devicesRetrieved = False
        with open('prodinfo.json') as json_file:
            self.prodinfo =json.load(json_file)

        # TODO: self.colour: remove or move to device
        self.colour = {"hue": None,
                       "saturation": None,
                       "brightness": None,
                       "kelvin": None}
        self.dim_level = None  # 0-100%

    def __get__(self, obj, objtype=None):
        pass

    def __set__(self, obj, val):
        pass

    def __delete__(self, obj):
        pass

    def init(self, num_lights, sensor_poll_delay=None, temp_units='C', RetryLimit=3, RetryTime=2):
        self.SUPPORTED_METHODS = self.LIFX_TURNON | self.LIFX_TURNOFF | self.LIFX_DIM

        self.RetryLimit = RetryLimit
        self.RetryTime = RetryTime

        self.log.info("Discovering lights...")
        self.lifx = LifxLAN(num_lights)

        return True

    def close(self):
        pass

    def turnOn(self, devid):
        """ Turn on light"""
        self.log.trace('turnOn {}'.format(self.devices[devid]["name"]))
        self.devices[devid]["bulb"].set_power("on", True)
        self.devices[devid]["status"] = "on"
        return True

    def turnOff(self, devid):
        """ Turn off light"""
        self.log.trace('turnOff {}'.format(self.devices[devid]["name"]))
        self.devices[devid]["bulb"].set_power("off", True)
        self.devices[devid]["status"] = "off"
        return True

    def set_state(self, devid, payload, limit=0):
        """Set state of light to on, off, color, dim """
        pass

    def set_colour(self, devid, red, green, blue):
        """Set light to colour (RGB)
           rgb: 0-255
        """

        self.log.info("color rgb:{},{},{}".format(red, green, blue))  # TODO: Change to trace

        self.devices[devid]["bulb"].set_RGB(red, green, blue)

        return

        #h, s, b = colorsys.rgb_to_hsv(red/255.0, green/255.0, blue/255.0)

        #print ("hue={}".format(h))
        #print ('brightness={}'.format(b))
        #print ('sat={}'.format(s))

        #hue = int(round(h * 65535.0, 0))
        #sat = int(round(s * 65535.0, 0))
        #light = int(round(b * 65535.0, 0))

        #self.devices[devid]["bulb"].set_HSB(hue, sat, light)

        #return

    def list_lights(self):
        """Get a list of devices on local LAN"""
        self.devices = {}
        devices = self.lifx.get_lights()
        if devices is None:
            self.log.debug('list_lights: No bulbs found!')
            return {}  # None

        for i in devices:
            dev_id = i.source_id
            name = i.get_label()
            dev = {
                "id": name,
                "internal_id": dev_id,
                "name": name,
                'bulb': i}
            self.log.debug('list_lights: Found {}'.format(dev_id))
            self.log.debug('list_lights: Bulb info {}'.format(i))
            for m in self.prodinfo[0]['products']:
                self.log.debug('Matching {} with {}'.format(i.product, m))
                if i.product == m['pid']:
                    #print "found"
                    dev['model'] = m['name']
                    if m['features']['color'] == True:
                        dev["isRGB"] = True
                    else:
                        dev["isRGB"] = False
                    # TODO: Also check ['features']['infrared'] and ['features']['multizone']
                    break
            if 'model' not in dev:
                self.log.error("Couldn't locate a suitable bulb in prodinfo")

            dev["isDimmer"] = True
            try:
                power = i.get_power()
                lvl = int(100.0 * round(power / 65535.0, 2))
                dev["dimlevel"] = lvl  # From 16 bit integer to percent
                dev["status"] = "on" if lvl > 0 else "off"
            except:
                self.log.error("Oops. Error getting power state. Dev={} Power={}".format(i, i.power_level))

            self.switches[name] = dev
            self.devices[name] = dev

            self.log.info('Found {}'.format(dev['name']))

        self.devicesRetrieved = True

        return self.devices

    def getLightState(self, devid):
        """Get state of one light"""
        self.log.trace('getLighState {}'.format(self.devices[devid]["name"]))

        try:
            power = self.devices[devid]["bulb"].get_power()
            self.log.info("power {}".format(power))
        except IOError:
            self.log.error('IOError from LIFXLAN. Ignoring request')
            return None  # TODO: On IOError - log + return last known value?

        color = self.devices[devid]["bulb"].get_color()
        self.colour = {"hue": color[0],
                       "saturation": color[1],
                       "brightness": color[2],
                       "kelvin": color[3]}

        lvl = int(100.0 * round(self.colour["brightness"] / 65535.0, 2))

        #print('Brightness={}, level={}'.format(self.colour["brightness"], lvl))
        self.devices[devid]["dimlevel"] = lvl  # From 16 bit integer to percent

        state = {"power":    u'on' if power > 0 else u'off',  # 'on'/'off'
                 "dimlevel": int(lvl),                  # 0-100
                 }
        return state

        #    self.log.error('getLightState failed. Received status {}'.format(response.status_code))
        #    return None


    def listSwitches(self):
        """Create a dictionary with all lights"""

        if len(self.devices) == 0:
            devs = self.list_lights()

        return self.switches

    def getErrorString(self, res_code):
        return res_code  # TOT: Remove

    def dim(self, devId, level, duration=1, limit=0):
        """ Dim light, level=0-100 """
        self.log.debug('Dim {} Level {}%'.format(self.devices[devId]["name"], str(float(level))))
        power = 65535*level/100
        self.devices[devId]["bulb"].set_brightness(power, duration=duration, rapid=False)
        self.colour["brightness"] = power
        self.dim_level = level  # TODO Remove?
        # TODO: duration needs to be handled by a thread that gradually fades

        return True

    def get_dim(self, devId):
        colour = self.get_colour(devId)
        lvl = int(100*colour[2]/65535)
        self.dim_level = lvl
        return lvl

    def get_colour(self, devId):
        color = self.devices[devId]["bulb"].get_color()
        #print color
        self.colour = {"hue": color[0],
                       "saturation": color[1],
                       "brightness": color[2],
                       "kelvin": color[3]}
        return self.colour

    def set_colourtemp(self, devId, kelvin):
        """ Set colour temperature of light. Other colour parameters not affected"""
        self.devices[devId]["bulb"].set_colortemp(kelvin, duration=1)
        return True  # TODO Check if colour temp was changed

    def set_brightness(self, devId, lvl):
        """ test """
        self.devices[devId]["bulb"].set_brightness(lvl, duration=1)
        return True  # TODO Check if brightness was changed

    def set_hue(self, devId, hue):
        """ test """
        self.devices[devId]["bulb"].set_hue(hue, duration=1)
        return True  # TODO Check if brightness was changed

    def set_saturation(self, devId, saturation):
        """ test """
        self.devices[devId]["bulb"].set_saturation(saturation, duration=1)
        return True  # TODO Check if brightness was changed

    def checkResponse(self, response):
        pass


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
        if len(self.switches) == 0:
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
        pass