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

import agoclient
from scheduler import Scheduler, all_days

import json


class AgoScheduler(agoclient.AgoApp):

    def event_handler(self, subject, content):
        """event handler - Processes incoming events and looks if they are of a relevant kind
                         - For now, it's only time event that are of interest
        """
        self.log.trace("event_handler start")

        if "event.environment.timechanged" in subject:
            self.log.trace("subject=%s content=%s", subject, content)

            js = json.loads(str(content).replace("u\'", '\"').replace("\'", '\"'))  # TODO: check if replacement is necessary
            #self.log.trace("js={}".format(js))
            #self.log.trace("uuid={} level={}".format(js["uuid"], js["level"]))
            t_now = self.s.now()
            self.log.trace("now={} - waiting for {}".format(t_now, self.nexttime))

            if self.nexttime is not None and t_now == self.nexttime:
                while True:  # Loop through all items with same time
                    self.log.info("Action to be triggered: {}".format(self.next_item))
                    self.next_item = self.s.get_next()
                    if self.next_item is None:
                        self.nexttime = None
                        break
                    if t_now != self.next_item["time"]:
                        self.nexttime = self.next_item["time"]
                        break
                if self.nexttime is not None:
                    self.log.debug("Next item scheduled for {}".format(self.nexttime))
                else:
                    self.log.debug("No more scheduled items for today")

            if self.weekdayno != int(js["weekday"]):
                # TODO: Check config if a new schedule file is to be loaded
                #self.log.info("New day! found: {}".format(int(js["weekday"])))
                #self.log.info("weekday: old {} new {}".format(self.weekday, js["weekday"]))
                self.new_day(all_days[int(js["weekday"])-1])
                self.weekdayno = int(js["weekday"])

        # TODO: Event handler for "reload" event

    def new_day(self, weekday):
        """ Load schedules for a new day

        @param weekday: "mo", "tu", etc.
        @return: nothing
        """
        no_activities = self.s.new_day(weekday)
        self.log.info("New day: {}. Loaded {} schedule items for today".format(weekday, no_activities))

    def setup_app(self):
        self.log.trace("setup_app start")
        self.connection.add_event_handler(self.event_handler)
        app = "scheduler"

        mapfile = self.get_config_option('schedule', None, section=app, app=app)

        if mapfile is None:
            self.log.error("No Schedule file found in config file. Idling.")

        self.s = Scheduler(self)

        self.s.parse_conf_file("schedule.json")  # TODO: Replace with real file
        dl, day_no = self.s.get_weekday()
        #self.log.info("Weekday={}".format(dl))
        self.s.weekday = dl
        self.new_day(all_days[day_no-1])
        self.weekday =dl
        self.weekdayno = day_no

        # item = self.s.get_first("00:00")
        # item = self.s.get_first(self.s.now())
        # self.log.info item

        self.next_item = self.s.get_first(self.s.now())
        if self.next_item is not None:
            self.nexttime = self.next_item["time"]
        else:
            self.nexttime = None
        self.log.debug("First item scheduled for {}".format(self.nexttime))

if __name__ == "__main__":
    AgoScheduler().main()
4