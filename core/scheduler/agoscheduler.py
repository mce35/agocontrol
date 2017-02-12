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
from groups import Groups

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
            t_now = self.s.now()
            self.log.trace("now={} - waiting for {}".format(t_now, self.nexttime))

            if self.nexttime is not None and t_now == self.nexttime:
                while True:  # Loop through all items with same time
                    self.log.debug("Action to be triggered: {}".format(self.next_item))
                    try:
                        self.sendmessage(self.next_item)
                    except NameError as e:
                        self.log.error("Oops, could not send message. Msg={}".format(e))
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
                self.weekdayno = int(js["weekday"])
                self.new_day(self.weekdayno)
                self.next_item = self.s.get_first("00:00")
                if self.next_item is not None:
                    self.nexttime = self.next_item["time"]
                else:
                    self.nexttime = None
                self.log.debug("First item scheduled for {}".format(self.nexttime))


        # TODO: Event handler for "reload" event

    def device_msg(self, uuid, action, level=None):
        content = {"uuid": uuid,  # TODO: Check unicode encoded strings
                   "action": action}
        if action == "setlevel":
            content["level"] = level
        elif action == "fade":
            pass

        if action in {"on", "off", "setlevel"}:
            msg = "event.device.statechanged"

            self.log.debug("About to set device content={}".format(content))
            self.connection.send_message(msg, content)

    def sendmessage(self, item):
        self.log.debug(("In sendmessage. item={}".format(item)))
        if item["device"] is not None:
            if item["action"] in {"on", "off"}:
                self.device_msg(item["device"], item["action"])
            elif item["action"] in {"setlevel"}:
                self.device_msg(item["device"], item["action"], item["level"])
            # content = {"uuid": item["device"]}
            # if item["action"] == "setlevel":
            #     content["action"] = "setlevel"
            #     content["level"] = int(item["level"])
            # elif item["action"] == "on":
            #     content["command"] = "on"
            # elif item["action"] == "off":
            #     content["command"] = "off"
            # elif item["action"] == "fade":
            #     pass

            # if item["action"] in {"on", "off", "setlevel"}:
            #     msg = "event.device.statechanged"
            #
            #     # todo: aDD ACTION SPECIFIC FIELDS
            #     self.log.debug("About to set device content={}".format(content))
            #     self.connection.send_message(msg, content)

        if item["scenario"] is not None:
            content = {"uuid": item["scenario"],
                       "command": "run"}
            self.log.debug("About to execute scenario content={}".format(content))
            self.connection.send_message(None, content)

        if item["group"] is not None:
            self.log.info(("About to send message to device group {}".format(item["group"])))
            grp = self.groups.find(item["group"])
            if grp is not None:
                for dev in grp.devices:
                    if dev is not None:
                        if item["action"] in {"on", "off"}:
                            self.device_msg(dev, item["action"])
                        elif item["action"] in {"setlevel"}:
                            self.device_msg(dev, item["action"], item["level"])


    def getScenarioControllerUuid(self):
        """Get UUID for the scenario controller"""
        inventory = self.connection.get_inventory()

        for uuid in inventory['devices']:
            if inventory['devices'][uuid]['devicetype'] == 'scenariocontroller':
                self.log.debug("Found Scenario Controller {}".format(uuid))
                return uuid

        return None

    def new_day(self, weekdayno):
        """ Load schedules for a new day

        @param weekdayno: 1-7
        @return: nothing
        """
        self.weekday = weekday = all_days[weekdayno - 1]
        self.log.trace("new_day() weekday={}".format(weekday))
        no_activities = self.s.new_day(weekday)
        self.next_item = self.s.get_first("00:00")
        self.weekdayno = weekdayno

        self.s.schedules.list_full_day()
        self.log.info("New day: {}. Loaded {} schedule items for today.".format(weekday, no_activities))

    def setup_app(self):
        self.log.trace("setup_app start")
        self.connection.add_event_handler(self.event_handler)
        app = "scheduler"

        self.scenario_controllerUUID = self.getScenarioControllerUuid()
        if self.scenario_controllerUUID is None:
            self.log.error(("Scenario Controler not found."))

        mapfile = self.get_config_option('schedule', None, section=app, app=app)

        if mapfile is None:
            self.mapfile = None
            self.log.error("No Schedule file found in config file. Idling.")
        else:
            self.mapfile = agoclient.config.CONFDIR + '/conf.d/' + mapfile
            self.log.info("Reading schedules from {}".format(self.mapfile))

        self.groups = Groups(agoclient.config.CONFDIR + '/conf.d/' + 'groups.json')  # TODO: Change to proper file

        self.s = Scheduler(self, self.groups)

        self.s.parse_conf_file(self.mapfile)
        dl, day_no = self.s.get_weekday()
        self.s.weekday = dl
        self.log.trace("day_no={}".format(day_no))
        self.new_day(day_no)
        #self.weekday =dl
        #self.weekdayno = day_no
        #self.s.schedules.list_full_day()

        # item = self.s.get_first("00:00")
        # item = self.s.get_first(self.s.now())
        # self.log.info item

        self.next_item = self.s.get_first(self.s.now())
        if self.next_item is not None:
            self.nexttime = self.next_item["time"]
        else:
            self.nexttime = None
        self.log.debug("First item scheduled for {}".format(self.nexttime))
        #TODO: Check if this is now - then execute it before going to the event handler


if __name__ == "__main__":
    AgoScheduler().main()
4