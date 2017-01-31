#!/usr/bin/python
AGO_SCHEDULER_VERSION = '0.0.1'
############################################
"""
Device, device group and scenario scheduler

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
from scheduler import Scheduler

import json

class AgoScheduler(agoclient.AgoApp):

    def event_handler(self, subject, content):
        """event handler - Processes incoming events and looks if they are of a relevant kind
                         - For now, it's only time event that are of interest
        """
        self.log.trace("event_handler start")

        if "event.environment.timechanged" in subject:
            #self.log.trace("subject=%s content=%s", subject, content)

            js = json.loads(str(content).replace("u\'", '\"').replace("\'", '\"'))  # TODO: check if replacement is necessary
            #self.log.info("js={}".format(js))  # TODO: Change to trace
            #self.log.debug("uuid={} level={}".format(js["uuid"], js["level"]))  # TODO: Change to trace
            #t_now = str(js["hour"]) + ":" + str(js["minute"])
            t_now = self.s.now()
            self.log.info("now={} - waiting for {}".format(t_now, self.nexttime))  # TODO: Change to trace

            if t_now == self.nexttime:
                self.log.info("Action to be triggered: {}".format(self.next_item))

                # Trigger actions - how about several at same time?
                self.next_item = self.s.get_next()
                self.nexttime = self.next_item["time"]

            if self.weekdayno != int(js["weekday"]):
                self.log.info("New day! found: {}".format(int(js["weekday"])))
                self.log.info("weekday: old {} new {}".format(self.weekday), js["weekday"])
                no_activities = self.s.new_day(int(js["weekday"]))
                self.log.info("Loaded {} activities for ")
                self.weekdayno = int(js["weekday"])


    def setup_app(self):
        self.log.trace("setup_app start")
        self.connection.add_event_handler(self.event_handler)
        app = "scheduler"


        self.s = Scheduler(self)

        self.s.parse_conf_file("schedule.json")  # TODO: Replace with real file
        dl, day_no = self.s.get_weekday()
        self.log.info("Weekday={}".format(dl))
        self.s.weekday = dl
        self.s.new_day(dl)
        self.weekday =dl
        self.weekdayno = day_no

        #item = self.s.get_first("00:00")
        #item = self.s.get_first(self.s.now())
        #self.log.info item

        self.next_item = self.s.get_first(self.s.now())
        self.nexttime = self.next_item["time"]
        self.log.info("setup_app nexttime={}".format(self.nexttime))


if __name__ == "__main__":
    AgoScheduler().main()
4