#!/usr/bin/python
AGO_SCHEDULER_VERSION = '0.0.1'
############################################
"""
Basic class for device and device group conf

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
import sys

import json

all_days = {"mo", "tu", "we", "th", "fr", "sa", "su"}

class Scheduler:
    def __init__(self, app):
        self.rules = None
        self.schedules = []

        self.log = None
        self.app = app
        try:
            self.log = app.log
        except AttributeError:
            # We seem to be in test mode, need a local logger
            self.log = llog()

    def parse_conf_file(self, filename):
        with open(filename) as conf_file:
            conf = json.load(conf_file)
        if "rules" in conf:
            self.log.debug("Rules config: {}".format(conf["rules"]))
            self.rules = Rules(conf["rules"])

        if "items" in conf:
            self.log.debug("Schedule config: {}".format(conf["items"]))
            self.schedules = Schedules(conf["items"], self.rules)

    def new_day(self, weekday):
        """ Load the confs for the new day
            E.g. called when it's 00:00
        """
        self.schedules.new_day(weekday)

    def get_first(self, when="00:00"):
        """ Get the first activity at the given time or later

        @param when: hh:mm format, 00:00 - 23:59
        @return: Matching activity
        """
        return self.schedules.get_first(when)

    def get_next(self, when=None):
        """ Get next activity to execute

        @param when: hh:mm format, 00:00 - 23:59
        @return: next activity
        """
        return self.schedules.get_next(when)

    def get_weekday(self):
        d = datetime.now().strftime('%A')
        dl= d[:2].lower()  # TODO: Check if this is local dependant
        #d1 = datetime.weekday()
        #d2 = all_days[d1]
        return dl

    def now(self):
        """
        Get current time
        @return: Current time, formatted as HH:MM
        """
        h = datetime.now().strftime('%H')
        m = datetime.now().strftime('%M')
        self._now = h.zfill(2) + ":" + m.zfill(2)
        return self._now


class Schedules(object):
    def __init__(self, jsonstr, rules):
        self.schedules = []
        self.activities = []
        self._weekday = None
        self.weekday = None

        for element in jsonstr:
            # self.log.trace(element)
            item = Schedule(element, rules)
            self.schedules.append(item)
            # print item

    def find(self, uuid):
        rule = None
        for r in self.rules:
            if r.uuid == uuid:
                rule = r
        return rule

    def get_first(self, when="00:00"):
        """ Get the first activity at the given time or later

        @param when: hh:mm format, 00:00 - 23:59
        @return: Matching activity or None if nothing found
        """
        #for item in self.activities:
        for idx, item in enumerate(self.activities):
            if item["time"] >= when:
                #print "found first "
                self.latest_idx = idx
                return item
        return None

    def get_next(self, when=None):
        """ Get next activity to execute

        @param when: hh:mm format, 00:00 - 23:59
        @return: next activity
        """
        if self.latest_idx > len(self.activities):
            return None
        self.latest_idx += 1
        return (self.activities[self.latest_idx])

    @property
    def weekday(self):
        """Weekday property."""
        print "getter of weekday called"
        return self._weekday

    @weekday.setter
    def weekday(self, day):
        print "setter of weekday called"
        if day is not None and day not in all_days:
            raise ValueError

        if self._weekday != day:
            self.new_day(day)
            self._weekday = day

    def new_day(self, weekday):
        activities = []
        self.activities = []
        for s in self.schedules:
            for i in s.schedules:
                item = s.schedules[i]
                #print item
                if item["enabled"] and weekday in item["days"]:
                    #found  a day to include
                    activities.append(item)
        #print self.activities
        #print "_______________________________________"
        #for item in activities:
        #    print item
        print "_______________________________________"
        print "Sorted activities"
        self.activities= sorted(activities, key=lambda k: k['time'])
        for item in self.activities:
            print item
        print "_______________________________________"

class Schedule:
    def __init__(self, jsonstr, rules=None):
        self.device = None
        self.scenario = None
        self.group = None
        if "device" in jsonstr:
            self.device = jsonstr["device"]
        if "scenario" in jsonstr:
            self.scenario = jsonstr["scenario"]
        if "group-uuid" in jsonstr:
            self.group = jsonstr["group"]
        self.enabled = jsonstr["enabled"]
        self.schedules = {}

        seq = 0
        for a in jsonstr["actions"]:
            seq += 1
            x = {"action": a["action"],   # On/Off/Run etc
                 "time":   a["time"],
                 "enabled": a["enabled"],
                 "device": self.device,
                 "scenario": self.scenario,
                 "group": self.group}
            if "days" in a:
                if a["days"][0] == u"weekdays":
                    x["days"] = ["mo", "tu", "we", "th", "fr"]
                elif a["days"][0] == u"weekends":
                    x["days"] = ["sa", "su"]
                elif a["days"][0] == u"all":
                    x["days"] = ["mo", "tu", "we", "th", "fr", "sa", "su"]
                else:
                    x["days"] = a["days"]

            if "level" in a:
                x["level"] = a["level"]
            if "tolevel" in a:
                x["tolevel"] = a["tolevel"]
            if "endtime" in a:
                x["endtime"] = a["endtime"]

            if "seq" in a:
                x["seq"] = a["seq"]
            if "rule" in a:
                x["rule-uuid"] = a["rule"]
                x["rule"] = rules.find(a["rule"])
                #print x["rule"]

            self.schedules[seq] = x
            #print (seq, self.schedules[seq])

    def __str__(self):
        s = "Schedule: "
        if self.device is not None:
            s += "Device {}".format(self.device)
        if self.scenario is not None:
            s += "Scenario {}".format(self.scenario)
        if self.group is not None:
            s += "Group {}".format(self.group)
        s += "Enabled" if self.enabled else "Disabled"
        s += "# schedules: {}".format(len(self.schedules))

        return s

class Rules:
    def __init__(self, jsonstr):
        self.rules = []
        for element in jsonstr:
            # self.log.trace(element)
            rule = Rule(element)
            self.rules.append(rule)
            #print rule

    def find(self, uuid):
        rule = None
        for r in self.rules:
            if r.uuid == uuid:
                rule = r
        return rule

class Rule:
    def __init__(self, jsonstr):
        self.name = jsonstr["name"]
        self.uuid = jsonstr["uuid"]
        self.rules = {}
        #print self.name
        seq = 0
        for r in jsonstr["rules"]:
            seq += 1
            x = {"type":     r["type"],
                 "variable": r["variable"],
                 "operator": r["operator"],
                 "value":    r["value"]}
            #print x

            self.rules[seq] = x
            #print (seq, self.rules[seq])

    def __str__(self):
        """Return a string representing content f the Rule object"""
        s = "name={}, uuid={}, type={}, # rules: {} ".format(self.name, self.uuid, self.type, len(self.rules))
        return s

    def execute(self):
        results = []
        for k, r in self.rules.iteritems():
            if r["type"] == "variable check":
                if r["variable"] == "HouseMode":
                    vv = "At home"  # TODO: Get variable from inventory using r["variable"]
                if r["variable"] == "test":
                    vv = "True"

                if r["operator"] == 'eq':
                    if vv == r["value"]:
                        results.append(True)
                    else:
                        results.append(False)
                        return False
                if r["operator"] == 'lt':
                    if vv < r["value"]:
                        results.append(True)
                    else:
                        results.append(False)
                        return False


        return True


        return True

    def addlog(self, log):
        self.log = log

class Days:
    def __init__(self):
        pass


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