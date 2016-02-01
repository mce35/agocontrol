#!/usr/bin/python
AGO_WEATHERREPORTER_VERSION = '0.0.3'
############################################
"""
Weather reporing support for ago control
Uploading temperature and humidity to crowd sourced weather data sites

"""
__author__     = "Joakim Lindbom"
__copyright__  = "Copyright 2014, Joakim Lindbom"
__date__       = "2016-01-31"
__credits__    = ["Joakim Lindbom", "The ago control team"]
__license__    = "GPL Public License Version 3"
__maintainer__ = "Joakim Lindbom"
__email__      = 'Joakim.Lindbom@gmail.com'
__status__     = "Experimental"
__version__    = AGO_WEATHERREPORTER_VERSION
############################################

import time
from qpid.messaging import Message
from datetime import date, datetime
import threading
import requests
import agoclient

import json

class AgoWeatherReporter(agoclient.AgoApp):

    def event_handler(self, subject, content):
        """event handler - Processes incomming events and looks if they are of a relevant kind
                         - Look up if there are reporting services to call for the sensor
        """
        if "event.environment.temperaturechanged" in subject:
            self.log.trace("subject=%s content=%s", subject, content)

            js = json.loads(str(content).replace("u\'", '\"').replace("\'", '\"')) #TODO: check if replacement is necessary
            print ("level=", js["level"])

            for x in self.sensors:
                if js["uuid"] == self.sensors[x]["UUID"]:
                    self.sensors[x]["temp"] = float(js["level"])
                    self.log.info("New temp for device=" + self.sensors[x]["device"] + " temp=" + str(self.sensors[x]["temp"]))

                    if "temperatur.nu" in self.sensors[x]:
                        #if self.sensors[x]["unit"] == 'degC': #TODO: Handle degF also
                        TN=self.sensors[x]["temperatur.nu"]
                        print ("TN-Hash: %s", TN["Hash"])
                        self.sendTemperaturNu(self.sensors[x]["temp"], TN["Hash"]) #TODO: check if return ok
                        self.sensors[x]["temperatur.nu"]["LastUpdate"]=datetime.now()
                    else:
                        self.log.trace ("No temperatur.nu upload")

                    if "WeatherUnderground" in self.sensors[x]:
                        WU=self.sensors[x]["WeatherUnderground"]
                        print ("WS-Station %s", WU["Station"])
                        self.sendWeatherUnderground(WU["Station"], WU["Password"],self.sensors[x]["temp"], self.sensors[x]["unit"]) #TODO: check if return ok
                        self.sensors[x]["WeatherUnderground"]["LastUpdate"]=datetime.now()
                    else:
                        self.log.trace("No WU upload")

        if "event.environment.humiditychanged" in subject:
            self.log.trace("subject=%s content=%s", subject, content)
            #TODO: add humidity reporting


    def sendTemperaturNu(self, temp, reporthash):
        """ Send temperature data to http://temperatur.nu
        """
        self.log.info ("temperatur.nu reporting temp=" +str(temp)+ " for hash=" + reporthash)

        # Critical section, don't want two of these running at the same time
        with self.temperaturenu_lock:
            r = requests.get('http://www.temperatur.nu/rapportera.php?hash=' + reporthash + '&t=' + str(temp))

            if r.status_code == 200 and "ok! (" in r.text and len(r.text) > 6:
                return True
            else:
                self.log.error("something went wrong when reporting. response=" + data)
                # Some logging needed here
                return False

    def sendWeatherUnderground(self, stationId, password, temp, tempUnit):
        """ Send weather data to http://wunderground.com
        """

        #TODO: Add humidity reportiong

        self.log.info ("Weather Underground reporting temp=" +str(temp)+ " for Station=" + stationId)

        # Critical section, don't want two of these running at the same time
        with self.weatherunderground_lock:
            if tempUnit == "degC":
                tempstr = 'tempc'
                #tempF = float(temp)*9/5+32
            else:
                tempstr = 'tempf'
                #tempF = float(temp)

            data = {
                'ID': stationId,
                'PASSWORD': password,
                'dateutc': str(datetime.utcnow()),
                tempstr: str(temp),
                'action': 'updateraw',
                'softwaretype': 'ago control',
                'realtime': '1',
                'rtfreq': '2.5'
            }
            self.log.trace ("WeatherUnderground station=%s pw=%s temp=%s", data["ID"], data["PASSWORD"], str(temp))
            url = 'http://rtupdate.wunderground.com/weatherstation/updateweatherstation.php'

            r = requests.post(url=url, data=data)

            #Raise exception if status code is not 200
            r.raise_for_status()
            if r.text == 'success\n':
                return True
            else:
                self.log.error ('Uploading to WeatherUnderground failed. response=%s', r.text)
                return False

    def setup_app(self):
        self.log.trace("setup_app start")
        self.sensors = {}
        self.temperaturenu_lock = threading.Lock()
        self.weatherunderground_lock = threading.Lock()

        self.connection.add_event_handler(self.event_handler)

        app = "weatherreporter"

        #self.connection.add_handler(self.message_handler)

        # TODO: Get inventory, iterate all sensors to see if there is any configuration for it

        try:
            self.Delay = self.get_config_option('Delay', 301, section="General", app=app)
        except ValueError:
            self.Delay=300 # 5 minutes

        try:
            WU_Password = self.get_config_option('Password', '', section="WeatherUnderground", app=app)
        except ValueError:
            WU_Password = None

        print ("Delay=" + str(self.Delay))
        #print ("Password=" + WU_Password)

        try:
            devIds  = (self.get_config_option('sensors', '', section="General", app=app)).replace(' ', '').split(',')
        except ValueError:
            self.Delay=300 # 5 minutes

        for devId in devIds.__iter__():
            print ("devId=%s", devId)
            sensor = {}
            sensor[devId] = devId
            sensor["delay"] = self.Delay

            try:
                sensor["UUID"]= self.get_config_option("UUID", "", section=devId, app=app)
            except ValueError:
                self.log.error ("UUID mandatory, skipping sensor %s", devId)
                continue
            if sensor["UUID"] =="":
                self.log.error ("UUID mandatory, skipping sensor %s", devId)
                continue
            sensor["device"] = devId

            try: # Check if Weather Underground is defined for this sensor
                WU_Station = self.get_config_option('WU-Station', '', section=devId, app=app)
            except ValueError:
                self.log.info ("No Weather Underground config for devId=%s", devId)

            try:
                WU_Delay = int(self.get_config_option('Delay', self.Delay, 'Weather Underground', app=app))
            except ValueError:
                self.log.info ("No Weather Underground delay for devId=%s", devId)

            if WU_Station != '':
                WeatherUnderground={}
                WeatherUnderground["Station"] = WU_Station
                WeatherUnderground["Delay"] = WU_Delay
                WeatherUnderground["Password"]=WU_Password
                sensor["WeatherUnderground"]=WeatherUnderground

            try: # Check if temperatur.nu is defined for this sensor
                TN_Hash= self.get_config_option('TN-Hash', '', section=devId, app=app)
            except ValueError:
                self.log.info ("No temperatur.nu config for devId=%s", devId)

            try:
                TN_Delay = int(self.get_config_option('Delay', self.Delay, 'temperatur.ny', app=app))
            except ValueError:
                self.log.info ("No temperatur.nu delay for devId=%s", devId)

            if TN_Hash != '':
                temperaturnu={}
                temperaturnu["Hash"] = TN_Hash
                temperaturnu["Delay"] = TN_Delay
                temperaturnu["LastUpdate"] = -1
                sensor["temperatur.nu"] = temperaturnu

            sensor["unit"]="degC" #TODO: Look it up
            sensor["temp"]=-274

            print ("sensor=")
            print (sensor)
            self.sensors[devId]= sensor

        # Set-up background thread to handle periodic updates
        BACKGROUND = reportThread(self)
        BACKGROUND.setDaemon(True)
        BACKGROUND.start()


class reportThread(threading.Thread):
    """ Some weather sites need to get updated frequently in order to accept the data.
        This thread sends data on a defined interval
    """
    def __init__(self,app):
        threading.Thread.__init__(self)
        self.Delay = 60
        self.app = app

    def run(self):
        while not self.app.is_exit_signaled():
            #TODO: Check if delay needs to be divided into sub-periods to avoid haning when close-down
            time.sleep (self.Delay) #general_delay or service specific
            now = datetime.now()
            for s, val in self.app.sensors.iteritems():
                if "temperatur.nu" in val and val['temp'] != -274:
                    diff = (now-val["temperatur.nu"]["LastUpdate"]).seconds
                    if diff>=val["temperatur.nu"]["Delay"]:
                        self.app.log.trace("temperatur.nu background report")
                        self.app.sendTemperaturNu(val["temp"], val["temperatur.nu"]["Hash"])
                        self.app.sensors[s]["temperatur.nu"]["LastUpdate"]=datetime.now()

                if "WeatherUnderground" in val and val['temp'] != -274:
                    diff = (now-val["WeatherUnderground"]["LastUpdate"]).seconds
                    if diff>=val["WeatherUnderground"]["Delay"]:
                        self.app.log.trace("Weather Underground background report")
                        WU=val["WeatherUnderground"]
                        self.app.sendWeatherUnderground(WU["Station"], WU["Password"],self.app.sensors[s]["temp"], self.app.sensors[s]["unit"]) #TODO: check if return ok
                        self.app.sensors[s]["WeatherUnderground"]["LastUpdate"]=datetime.now()


if __name__ == "__main__":
    AgoWeatherReporter().main()
