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
from scheduler import Scheduler

import json

class AgoScheduler(agoclient.AgoApp):

    def event_handler(self, subject, content):
        """event handler - Processes incoming events and looks if they are of a relevant kind
                         - For now, it's only time event that are of interrest
        """
        self.log.trace("event_handler start")
        if "event.environment.timechanged" in subject:
            #self.log.trace("subject=%s content=%s", subject, content)

            js = json.loads(str(content).replace("u\'", '\"').replace("\'", '\"'))  # TODO: check if replacement is necessary
            #self.log.info("js={}".format(js))  # TODO: Change to trace
            #self.log.debug("uuid={} level={}".format(js["uuid"], js["level"]))  # TODO: Change to trace
            #t_now = str(js["hour"]) + ":" + str(js["minute"])
            t_now = self.s.now()
            print("now={} - waiting for {}".format(t_now, self.nexttime))  # TODO: Change to trace

            if t_now == self.nexttime:
                print("Action to be triggered: {}".format(self.next_item))
                self.s.get_next()

            if self.weekdayno != int(js["weekday"]):
                print ("New day! found: {}".format(int(js["weekday"])))
                print ("weekday: old {} new".format(self.weekday), js["weekday"])
                self.s.new_day(int(js["weekday"]))
                self.weekdayno = int(js["weekday"])


    def setup_app(self):
        self.log.trace("setup_app start")
        self.connection.add_event_handler(self.event_handler)
        app = "scheduler"

        self.s = Scheduler(self)

        self.s.parse_conf_file("schedule.json")  # TODO: Replace with real file
        d = self.s.get_weekday()
        print ("weekday: {}".format(d))
        self.s.weekday = d
        self.s.new_day(d)
        #item = self.s.get_first("00:00")
        #item = self.s.get_first(self.s.now())
        #print item

        self.next_item = self.s.get_first(self.s.now())
        self.nexttime = self.next_item["time"]
        print ("setup_app nexttime={}".format(self.nexttime))

        self.weekday =None
        self.weekdayno = None

if __name__ == "__main__":
    AgoScheduler().main()
4