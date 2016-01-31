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

                    if "TN-Hash" in self.sensors[x]:
                        if self.sensors[x]["unit"] == 'degC': #TODO: Handle degF also
                            print ("TN-Hash: %s", self.sensors[x]["TN-Hash"])
                            self.sendTemperaturNu(self.sensors[x]["temp"], self.sensors[x]["TN-Hash"]) #TODO: check if return ok
                    else:
                        self.log.trace ("No temperatur.nu upload")

                    if "WU-Station" in self.sensors[x]:
                        print ("WS-Station %s", self.sensors[x]["WU-Station"])
                        self.sendWeatherUnderground(self.sensors[x]["WU-Station"], self.WU_password,self.sensors[x]["temp"], self.sensors[x]["unit"]) #TODO: check if return ok
                    else:
                        self.log.trace("No WU upload")

        if "event.environment.humiditychanged" in subject:
            self.log.trace("subject=%s content=%s", subject, content)
            #TODO: add humidity reporting


    def sendTemperaturNu(self, temp, reporthash):
        """ Send temperature data to http://temperatur.nu
        """
        self.log.info ("temperatur.nu reporting temp=" +str(temp)+ " for UUID=" + reporthash)

        # Critical section, don't want two of these running at the same time
        with self.temperaturenu_lock:
            r = requests.get('http://www.temperatur.nu/rapportera.php?hash=' + reporthash + '&t=' + str(temp))

            if r.status_code == 200 and "ok! (" in r.text and len(r.text) > 6:
                return True
            else:
                self.log.error("something went wrong when reporting. response=" + data)
                # Some logging needed here
                return False

    def sendWeatherUnderground(self, stationid, password, temp, tempUnit):
        #TODO: Add humidity reportiong
        if tempUnit == "degC":
            tempstr = 'tempc'
            #tempF = float(temp)*9/5+32
        else:
            tempstr = 'tempf'
            #tempF = float(temp)

        data = {
            'ID': stationid,
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
        self.sensors = {}
        self.temperaturenu_lock = threading.Lock()
        self.weatherunderground_lock = threading.Lock()

        self.connection.add_event_handler(self.event_handler)

        app = "weatherreporter"

        self.log.info("setup_app")
        #self.connection.add_handler(self.message_handler)

        # TODO: Get inventory, iterate all sensors to see if there is any configuration for it

        try:
            self.Delay = self.get_config_option('Delay', 301, section="General", app=app)
        except ValueError:
            self.Delay=300 # 5 minutes

        try:
            self.WU_password = self.get_config_option('Password', 'None', section="WeatherUnderground", app=app)
        except ValueError:
            self.WU_password = None

        print ("Delay=" + str(self.Delay))
        print ("Password=" + self.WU_password)

        try:
            devIds  = (self.get_config_option('sensors', '', section="General", app=app)).replace(' ', '').split(',')
        except ValueError:
            self.Delay=300 # 5 minutes

        for devId in devIds.__iter__():
            print ("devId=%s", devId)
            sensor = {}
            sensor[devId] = devId
            try:
                sensor["UUID"]= self.get_config_option("UUID", "", section=devId, app=app)
            except ValueError:
                self.log.error ("UUID mandatory, skipping sensor %s", devId)
                continue
            if sensor["UUID"] =="":
                self.log.error ("UUID mandatory, skipping sensor %s", devId)
                continue
            sensor["device"] = devId

            try:
                WU_Station = self.get_config_option('WU-Station', '', section=devId, app=app)
            except ValueError:
                self.log.info("No WU-Station")
            if WU_Station != '':
                sensor["WU-Station"] = WU_Station

            try:
                TN_Hash= self.get_config_option('TN-Hash', '', section=devId, app=app)
            except ValueError:
                self.log.info ("No temperatur.nu hash value")
            if TN_Hash != '':
                sensor["TN-Hash"] = TN_Hash

            sensor["unit"]="degC" #TODO: Look it up

            print ("sensor=")
            print (sensor)
            self.sensors[devId]= sensor


if __name__ == "__main__":
    AgoWeatherReporter().main()
