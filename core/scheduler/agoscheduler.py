#!/usr/bin/python
AGO_SCHEDULER_VERSION = '0.0.1'
############################################
"""
Device and device group scheduler

"""
__author__     = "Joakim Lindbom"
__copyright__  = "Copyright 2017, Joakim Lindbom"
__date__       = "2017-01-27"
__credits__    = ["Joakim Lindbom", "The ago control team"]
__license__    = "GPL Public License Version 3"
__maintainer__ = "Joakim Lindbom"
__email__      = 'Joakim.Lindbom@gmail.com'
__status__     = "Experimental"
__version__    = AGO_SCHEDULER_VERSION
############################################

import time
from datetime import date, datetime
import threading
import agoclient
import sys

import json

class AgoScheduler(agoclient.AgoApp):

    def event_handler(self, subject, content):
        """event handler - Processes incoming events and looks if they are of a relevant kind
                         - For now, it's only time event that are of interrest
        """
        self.log.trace("event_handler start")
        if "event.environment.timechanged" in subject:
            self.log.trace("subject=%s content=%s", subject, content)

            js = json.loads(str(content).replace("u\'", '\"').replace("\'", '\"'))  # TODO: check if replacement is necessary
            self.log.debug("uuid={} level={}".format(js["uuid"], js["level"]))  # TODO: Change to trace

            for x in self.sensors:
                if js["uuid"] == self.sensors[x]["UUID"]:
                    self.sensors[x]["temp"] = float(js["level"])
                    self.log.debug("New temp for device=" + self.sensors[x]["device"] + " temp=" + str(self.sensors[x]["temp"]))



    def setup_app(self):
        self.log.trace("setup_app start")
        self.sensors = {}

        self.connection.add_event_handler(self.event_handler)

        app = "scheduler"



        for devId in devIds.__iter__():
            self.log.debug("devId={}".format(devId))
            sensor = {}
            sensor[devId] = devId
            sensor["delay"] = self.Delay

            try:
                sensor["UUID"] = self.get_config_option("UUID", "", section=devId, app=app)
            except ValueError:
                self.log.error("UUID mandatory, skipping sensor {}".format(devId))
                continue
            if sensor["UUID"] == "":
                self.log.error("UUID mandatory, skipping sensor {}".format(devId))
                continue
            sensor["device"] = devId

            try:  # Check if Weather Underground is defined for this sensor
                WU_Station = self.get_config_option('WU-Station', '', section=devId, app=app)
            except ValueError:
                self.log.info("No Weather Underground config for devId={}".format(devId))

            try:
                WU_Delay = int(self.get_config_option('Delay', self.Delay, 'Weather Underground', app=app))
            except ValueError:
                self.log.info("No Weather Underground delay for devId=%s", devId)

            if WU_Station != '':
                WeatherUnderground = {}
                WeatherUnderground["Station"] = WU_Station
                WeatherUnderground["Delay"] = WU_Delay
                WeatherUnderground["Password"] = WU_Password
                sensor["WeatherUnderground"] = WeatherUnderground

            try: # Check if temperatur.nu is defined for this sensor
                TN_Hash= self.get_config_option('TN-Hash', '', section=devId, app=app)
            except ValueError:
                self.log.info("No temperatur.nu config for devId={}".format(devId))

            try:
                TN_Delay = int(self.get_config_option('Delay', self.Delay, 'temperatur.ny', app=app))
            except ValueError:
                self.log.info("No temperatur.nu delay for devId={}".format(devId))

            if TN_Hash != '':
                temperaturnu = {}
                temperaturnu["Hash"] = TN_Hash
                temperaturnu["Delay"] = TN_Delay
                temperaturnu["LastUpdate"] = -1
                sensor["temperatur.nu"] = temperaturnu

            sensor["unit"] = "degC"  # TODO: Look it up
            sensor["temp"] = -274

            print ("sensor=")
            print (sensor)
            self.sensors[devId] = sensor


if __name__ == "__main__":
    AgoScheduler().main()
