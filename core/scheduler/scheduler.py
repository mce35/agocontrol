#!/usr/bin/python
AGO_SCHEDULER_VERSION = '0.0.1'
############################################
"""
Scheduler: Controlled by time-of-day, weekday and simple rules
Triggers actions to device, device-group or scenario
Shift of day and triggering of events are to be driven from caller, having its own clock driven loop
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
from datetime import datetime, timedelta #, date.fromtimestamp, date.utcfromtimestamp
from variables import Variables
import json
import random
import math
from groups import Groups
import sys
if sys.version_info[0] > 2:
    # py3k
    pass
else:
    # py2
    import codecs
    import warnings

    def open(file, mode='r', buffering=-1, encoding=None,
             errors=None, newline=None, closefd=True, opener=None):
        if newline is not None:
            warnings.warn('newline is not supported in py2')
        if not closefd:
            warnings.warn('closefd is not supported in py2')
        if opener is not None:
            warnings.warn('opener is not supported in py2')
        return codecs.open(filename=file, mode=mode, encoding=encoding,
                           errors=errors, buffering=buffering)

all_days = ["mo", "tu", "we", "th", "fr", "sa", "su"]


class Scheduler(object):
    def __init__(self, lon, lat, groups=None, log=None):
        self.rules = None
        self.schedules = []


        if log is not None:
            self.log = log
        else:
            self.log = llog()
        #self.app = app
        self._nexttime = None
        self._weekday = None
        self._nexttime = None
        self._now = None
        self.sunrise = None
        self._random_minutes = None

        coords = {'longitude': float(lon), 'latitude': float(lat)}
        self.sun = Sun(coords, self.log)

        if groups is not None:
            self.groups = groups
        else:
            self.groups = None
        #try:
        #    self.log = app.log
        #except AttributeError:
        #    # We seem to be in test mode, need a local logger
        #    self.log = llog()

    def parse_conf_file(self, filename):
        if filename is not None:
            with open(filename, encoding='utf-8') as conf_file:
                conf = json.load(conf_file)
            if "rules" in conf:
                self.log.debug("Rules config: {}".format(conf["rules"]))
                self.rules = Rules(conf["rules"], self.log)

            if "items" in conf:
                self.log.debug("Schedule config: {}".format(conf["items"]))
                self.schedules = Schedules(conf["items"], self.rules, self.log, self.sun, self.groups)

    @property
    def random_minutes(self):
        return self._random_minutes

    @random_minutes.setter
    def random_minutes(self, minutes):
        self._random_minutes  = minutes

    def new_day(self, weekday):
        """ Load the schedule configuration, applying filters for the new day
            Typically called at start-up and when it's 00:00
        @return: # of activities loaded
        """
        return self.schedules.new_day(weekday, self._random_minutes)

    def get_nexttime(self):
        return self.nexttime

    def get_first(self, when="00:00"):
        """ Get the first activity at the given time or later

        @param when: hh:mm format, 00:00 - 23:59
        @return: Matching activity
        """
        item = self.schedules.get_first(when)
        if item is not None:
            self._nexttime = item["time"]

        return item

    def get_next(self, when=None):
        """ Get next activity to execute

        @param when: hh:mm format, 00:00 - 23:59
        @return: next activity
        """
        item = self.schedules.get_next(when)
        if item is not None:
            self._nexttime = item["time"]

        return item

    @property
    def nexttime(self):
        """Time-of-day for next activity, or None if there is none."""
        return self.schedules.nexttime

    def get_weekday(self):
        """
        Return current weekday in locale independent format, english short-form
        @return: E.g. "mo", "tu", etc. + weekday # (mo = 1, tu=2 etc)
        """
        d = datetime.now().strftime('%A')
        dl = d[:2].lower()
        return dl, all_days.index(dl)+1

    def now(self):
        """
        Get current time
        @return: Current time, formatted as HH:MM
        """
        h = datetime.now().strftime('%H')
        m = datetime.now().strftime('%M')
        self._now = h.zfill(2) + ":" + m.zfill(2)
        return self._now

    def execute_cmd(self):
        #TODO: Or just build the dict and let caller execute command?
        pass

    def calc_sunrise(self, date=None):
        #coords = {'longitude': float(lon), 'latitude': float(lat)}

        #sun = Sun(coords, use_local=True)

        sunrise = self.sun.get_sunrise_time(date)
        self.sunrise = sunrise['str_hr'] + ':' + sunrise['str_min']

        #print("Sunrise today: {}".format(self.sunrise))
        return self.sunrise


class Schedules(object):
    def __init__(self, jsonstr, rules, log, sun=None, groups=None):
        self.schedules = []
        self.activities = []
        self._weekday = None
        self.weekday = None
        self._nexttime = None
        self.groups = groups
        self.log = log
        self.latest_idx = -1

        self.sunrise = None
        self.sunrise_hhmm = None
        self.sunset = None
        self.sunset_hhmm = None
        self._now = None

        self.sun = sun

        for element in jsonstr:
            self.log.trace(element)
            item = Schedule(element, self.sun, rules, self.log)
            self.schedules.append(item)

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
        for idx, item in enumerate(self.activities):
            if item["time"] >= when:
                self.latest_idx = idx
                return item
        return None

    def get_next(self, when=None):
        """ Get next activity to execute

        @param when: hh:mm format, 00:00 - 23:59
        @return: next activity
        """
        if when is not None:
            return self.get_first(when)

        if (self.latest_idx + 1) >= len(self.activities):
            return None
        self.latest_idx += 1
        return self.activities[self.latest_idx]

    def list_full_day(self, now=None):
        _latest_idx = self.latest_idx
        now_printed = False

        if now is None:
            now = self.now()

        item = self.get_first("00:00")
        while item is not None:
            if (not now_printed) and item["time"] >= now:
                self.log.info("{} ----> now <----".format(now))
                now_printed = True
            item_type = "Device" if item["device"] is not None else "Group" if item["group"] is not None else "Scenario"

            dsc = item["name"]
            if dsc is None and item_type == "Device":
                dsc = item["device"]
            if dsc is None and item_type == "Group":
                grp = self.groups.find(item["group"])
                dsc = grp.name
            if dsc is None and item_type == "Scenario":
                dsc = item["scenario"]

            self.log.info("{}{} {:<8} - {:<25} - {}".format(item["time"], '*' if item["dynamic_time"] else " ", item_type, dsc, item["action"]))

            item = self.get_next()
        if not now_printed:
            self.log.info("{} ----> now <----".format(now))

        self.latest_idx = _latest_idx

    def now(self):
        """
        Get current time
        @return: Current time, formatted as HH:MM
        """
        h = datetime.now().strftime('%H')
        m = datetime.now().strftime('%M')
        self._now = h.zfill(2) + ":" + m.zfill(2)
        return self._now

    @property
    def nexttime(self):
        """Time-of-day for next activity, or None if there are no more."""
        return self._nexttime

    @property
    def weekday(self):
        """Weekday property."""
        return self._weekday

    @weekday.setter
    def weekday(self, day):
        """ Setter of weekday. Triggers loading of new day schedule if it's a new weekday
        """
        if day is not None and day not in all_days:
            raise ValueError

        if self._weekday != day:
            self.new_day(day)
            self._weekday = day

    def get_sun_times(self):
        tp = self.sun.get_sunrise_time()
        self.sunrise = tp["datetime"]
        self.sunrise_hhmm = "{}:{}".format(tp['str_hr'], tp['str_min'])

        tp = self.sun.get_sunset_time()
        self.sunset = tp["datetime"]
        self.sunset_hhmm = "{}:{}".format(tp['str_hr'], tp['str_min'])

    def new_day(self, weekday, random_minutes=None):
        """
        Build activities list, filtered to the current weekday
        @param weekday: Filter schedules for this weekday, "mo", "tu" or 1-7
        @return: # of activities loaded
        """
        _weekday = weekday

        if isinstance(weekday, (int, long)):
            if int(weekday) in range(1, 7):
                _weekday = all_days[weekday-1]
            else:
                return 0

        if weekday == "today":
            # _weekday = self. TODO:
            pass

        if _weekday not in all_days:
            return 0

        self.get_sun_times()

        activities = []
        self.activities = []
        self.latest_idx = -1
        for s in self.schedules:
            s.update_dynamic_times(random_minutes)
            for i in s.activities:
                item = s.activities[i]
                if item["enabled"] and _weekday in item["days"]:
                    activities.append(item)
        self.activities = sorted(activities, key=lambda k: k['time'])  # TODO: Check with (k["time"], k["seq"])
        if len(self.activities) > 0:
            self._nexttime = self.activities[0]["time"]  # TODO: Set to None? start time yet not known
        return len(self.activities)


class Schedule(object):
    """
    Represent a schedule item
    """
    def __init__(self, jsonstr, sun, rules=None, log=None):
        self.device = None
        self.scenario = None
        self.group = None
        self.sun = sun
        self.log = log
        if "device" in jsonstr:  # TODO: Only include the type that's at hand, not all 3?
            self.device = jsonstr["device"]
        if "scenario" in jsonstr:
            self.scenario = jsonstr["scenario"]
        if "group" in jsonstr:
            self.group = jsonstr["group"]
        self.enabled = jsonstr["enabled"]
        try:
            self.name = jsonstr["name"]
        except KeyError:
            self.name = None
        self.activities = {}

        seq = 0
        for a in jsonstr["actions"]:
            seq += 1
            x = {"action": a["action"],   # On/Off/Run etc.
                 "enabled": a["enabled"],
                 "device": self.device,
                 "scenario": self.scenario,
                 "name": self.name,
                 "group": self.group}

            if "sunrise" in a["time"] or "sunset" in a["time"]:
                x["time"] = self.get_sun_time(a["time"])
                print("{} = {}".format(a["time"], x["time"]))
                x["time_base"] = a["time"]
                x["dynamic_time"] = True
            elif "random" in a and a["random"] is True:
                x["time_base"] = a["time"]
                x["dynamic_time"] = True
                x["random_minutes"] = a["random_minutes"]
                x["time"] = self.get_random_time(a["time"], x["random_minutes"])
            else:  # TODO: Add times relative to sunset/sunrise
                x["time"] = a["time"]
                x["time_base"] = a["time"]
                x["dynamic_time"] = False

            if "days" in a:
                if a["days"][0] == u"weekdays":
                    x["days"] = ["mo", "tu", "we", "th", "fr"]
                elif a["days"][0] == u"weekends":
                    x["days"] = ["sa", "su"]
                elif a["days"][0] == u"all":
                    x["days"] = ["mo", "tu", "we", "th", "fr", "sa", "su"]
                elif a["days"][0] == u'none':
                    x["days"] = 'none'
                    self.enabled = False
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

            self.activities[seq] = x

    def get_sun_time(self, time_base):
        """
        Get times relative to sunrise or sunset
        @param time_base: Format: sunrise
                                  sunset
                                  sunrise+10m   = 10 minutes after sunrise
                                  sunrise-10m   = 10 minutes before sunrise
                                  sunset+10m   = 10 minutes after sunset
                                  sunset-10m   = 10 minutes before sunset
        @return: time as str(hh:mm) or 99:99 to indicate error
        """
        sunrise = self.sun.get_sunrise_time()
        self.log.debug("sunrise:{}".format(sunrise))
        sunrise_hhmm = "{}:{}".format(sunrise['str_hr'], sunrise['str_min'])
        sunset = self.sun.get_sunset_time()
        sunset_hhmm = "{}:{}".format(sunset['str_hr'], sunset['str_min'])

        if time_base == "sunrise":
            self.log.debug("get_sun_time Base:{} result:{}".format(time_base, sunrise_hhmm))
            return sunrise_hhmm
        elif time_base == "sunset":
            self.log.debug("get_sun_time Base:{} result:{}".format(time_base, sunset_hhmm))
            return sunset_hhmm
        elif "sunrise" in time_base or "sunset" in time_base:
            if "sunrise" in time_base:
                rt = time_base[7:]
            else:
                rt = time_base[6:]
            if rt[-1:] == "m":
                minutes = int(rt[1:-1])
                if rt[:1] == "-":
                    td = timedelta(seconds=-60 * minutes)
                else:
                    td = timedelta(seconds=60 * minutes)

                if "sunrise" in time_base:
                    new_time = sunrise["datetime"] + td
                else:
                    new_time = sunset["datetime"] + td

                new_time_hhmm = self.get_hhmm(new_time)
                self.log.debug("get_sun_time Base:{} result:{}".format(time_base, new_time_hhmm))
                return new_time_hhmm

        self.log.error("get_sun_time failed. Base:{}".format(time_base))
        return "99:99"  # TODO: Raise exception?

    def get_random_time(self, time_base, minutes):
        """
        Calculate a time based on a given time plus/minus no. of minutes
        @param time_base: Time to base new time on (hh:mm format)
        @param minutes: # minutes to deviate plus/minus the base time
        @return: new time (hh:mm format)
        """
        sign = random.randint(1, 2)
        rand_min = random.randint(1, minutes)
        time = datetime.now().replace(hour=int(time_base[:2]), minute=int(time_base[-2:]))
        if sign == 1:  # Minus
            td = timedelta(seconds=-60 * rand_min)
        else:            # Plus
            td = timedelta(seconds=60 * rand_min)

        new_time = time + td
        new_time_hhmm = self.get_hhmm(new_time)

        self.log.debug("get_random_time time_base={} {} newtime={}".format(time_base, time, new_time_hhmm))

        return new_time_hhmm

    def get_hhmm(self, dt):
        """
        Create hh:mm string from datetime object
        @param   dt: datetime object
        @return: string formatted as hh:mm
        """
        a = str(dt.hour).zfill(2) + ':' + str(dt.minute).zfill(2)
        return a

    def update_dynamic_times(self, random_minutes):
        for i in self.activities:
            item = self.activities[i]
            if item["dynamic_time"]:
                if "random_minutes" in item:
                    self.activities[i]["time"] = self.get_random_time(item["time_base"], item["random_minutes"])
                else:
                    self.activities[i]["time"] = self.get_sun_time(item["time_base"])
            elif random_minutes is not None:
                self.activities[i]["time"] = self.get_random_time(item["time_base"], random_minutes)

    def __str__(self):
        s = "Schedule: "
        if self.device is not None:
            s += "Device {}".format(self.device)
        if self.scenario is not None:
            s += "Scenario {}".format(self.scenario)
        if self.group is not None:
            s += "Group {}".format(self.group)

        s += "Enabled" if self.enabled else "Disabled"
        s += "# activities: {}".format(len(self.activities))
        return s


class Rules(object):
    """
    Collection of Rules
    """
    def __init__(self, jsonstr, log):
        self.log = log
        self.rules = []

        self.variables = Variables("variablesmap.json")  # TODO: Use system default instead
        if len(self.variables.variables) == 0:
            print("Variables map not parsed correctly, zero items retrieved")

        for element in jsonstr:
            self.log.trace(element)
            rule = Rule(element, self.variables, self.log)
            self.rules.append(rule)

    def find(self, uuid):
        rule = None
        for r in self.rules:
            if r.uuid == uuid:
                rule = r
        return rule


class Rule(object):
    """
    Represent a rule, containing rules definition and rules execution
    """
    def __init__(self, jsonstr, variables, log):
        self.log = log
        self.variables = variables
        self.name = jsonstr["name"]
        self.uuid = jsonstr["uuid"]
        self.rules = {}

        seq = 0
        for r in jsonstr["rules"]:
            seq += 1
            x = {"type":     r["type"],
                 "variable": r["variable"],
                 "operator": r["operator"],
                 "value":    r["value"]}

            self.rules[seq] = x

    def __str__(self):
        """
        Return a string representing content of the Rule object
        """
        s = "name={}, uuid={}, # rules: {} ".format(self.name, self.uuid, len(self.rules))
        return s

    def execute(self):
        """
        Execute the rule according to definition
        For now, if the rule contains several elements, a logical AND operation is applied for the elements
        @return: True or False, depending on rules outcome
        """

        results = []
        for k, r in self.rules.iteritems():
            if r["type"] == "variable check":

                vv = self.variables.get_variable(r["variable"])
                if vv is None:
                    self.log.error("Variable {} not found".format(r["variable"]))
                    return False

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

                if r["operator"] == 'le':
                    if vv <= r["value"]:
                        results.append(True)
                    else:
                        results.append(False)
                        return False

                if r["operator"] == 'gt':
                    if vv > r["value"]:
                        results.append(True)
                    else:
                        results.append(False)
                        return False

                if r["operator"] == 'ge':
                    if vv >= r["value"]:
                        results.append(True)
                    else:
                        results.append(False)
                        return False

        return True

    def addlog(self, log):
        self.log = log


class Sun(object):
    """ Calculates sunrise and sunset events
        based on http://stackoverflow.com/questions/19615350/calculate-sunrise-and-sunset-times-for-a-given-gps-coordinate-within-postgresql """
    def __init__(self, coords, log, use_local=True):
        """
        @param use_local: True: Convert to local time zone when returning sunset or sunrise.
                          False: Use UTC when returning sunset or sunrise.
        """
        self.use_local = use_local
        self.sunrise = None
        self.sunset = None
        self.now = None
        self.log = log

        self.coords = coords

    def get_sunrise_time(self, date=None):
        return self.calc_sun_time(self.coords, True, date)

    def get_sunset_time(self, date=None):
        return self.calc_sun_time(self.coords, False, date)

    def calc_sun_time(self, coords, isRiseTime, date=None, zenith=90.8):
        """
        Calculate sunrise or sunset
        @param coords:     Longitude/latitude
        @param isRiseTime: True: return time for sunrise
                           False: return time for sunset
        @param date        Date for calculation. None = Today
        @param zenith:     Zenith angle
        @return: {
            'status':   True if successful calculation
            'msg':      Error text if failure in calculating time
            'decimal':  Time (hours.minutes - minutes part is not 0..59 but 0..99!
            'hr':       hour of sunset/sunrise
            'min':      minute of sunset/sunrise
            'str_hr':   hour of sunset/sunrise, zero padded string ('00' - '23')
            'str_min':  minute of sunset/sunrise, zero padded string ('00' - '59')
            'datetime': time as a datetime object
            }
        """
        if date is None:
            day, month, year = self.get_current_utc()  # TODO: Get as parameter to make this testable.
        else:
            year = date.year
            month = date.month
            day = date.day

        self.log.debug("calc_sun_times: Coords: {} Date: {}-{}-{}".format(coords, year, month, day))

        to_rad = math.pi/180

        # 1. first calculate the day of the year
        n1 = math.floor(275 * month / 9)
        n2 = math.floor((month + 9) / 12)
        n3 = (1 + math.floor((year - 4 * math.floor(year / 4) + 2) / 3))
        n = n1 - (n2 * n3) + day - 30

        # 2. convert the longitude to hour value and calculate an approximate time
        lngHour = coords['longitude'] / 15

        if isRiseTime:
            t = n + ((6 - lngHour) / 24)
        else:  # sunset
            t = n + ((18 - lngHour) / 24)

        # 3. calculate the Sun's mean anomaly
        m = (0.9856 * t) - 3.289

        # 4. calculate the Sun's true longitude
        l = m + (1.916 * math.sin(to_rad*m)) + (0.020 * math.sin(to_rad * 2 * m)) + 282.634
        l = self.force_range(l, 360)  #NOTE: l adjusted into the range [0,360)

        # 5a. calculate the Sun's right ascension

        ra = (1/to_rad) * math.atan(0.91764 * math.tan(to_rad*l))
        ra = self.force_range(ra, 360)  #NOTE: ra adjusted into the range [0,360)

        # 5b. right ascension value needs to be in the same quadrant as L
        l_quadrant  = (math.floor(l/90)) * 90
        ra_quadrant = (math.floor(ra/90)) * 90
        ra += (l_quadrant - ra_quadrant)

        # 5c. right ascension value needs to be converted into hours
        ra /= 15

        # 6. calculate the Sun's declination
        sin_dec = 0.39782 * math.sin(to_rad*l)
        cos_dec = math.cos(math.asin(sin_dec))

        # 7a. calculate the Sun's local hour angle
        cos_h = (math.cos(to_rad*zenith) - (sin_dec * math.sin(to_rad*coords['latitude']))) \
               / (cos_dec * math.cos(to_rad*coords['latitude']))

        if cos_h > 1:
            return {'status': False, 'msg': 'the sun never rises on this location (on the specified date)'}

        if cos_h < -1:
            return {'status': False, 'msg': 'the sun never sets on this location (on the specified date)'}

        # 7b. finish calculating H and convert into hours
        if isRiseTime:
            h = 360 - (1/to_rad) * math.acos(cos_h)
        else:  # Sunset
            h = (1/to_rad) * math.acos(cos_h)

        h /= 15

        # 8. calculate local mean time of rising/setting
        local_mean_time = h + ra - (0.06571 * t) - 6.622

        # 9. adjust back to UTC
        utc_time = local_mean_time - lngHour
        utc_time = self.force_range(utc_time, 24)  # UTC time in decimal format (e.g. 23.23)

        # 10. Return
        hr = self.force_range(int(utc_time), 24)
        minute = round((utc_time - int(utc_time))*60, 0)

        if self.use_local:
            hr, minute, utc_time = self.utc_to_local(hr, minute, utc_time)

        return {
            'status': True,
            'decimal': utc_time,
            'hr': hr,
            'min': minute,
            'str_hr': self.zero_padded_string(hr, 2),
            'str_min': self.zero_padded_string(minute, 2),
            'datetime': self.get_datetime(hr, minute)
        }

    @staticmethod
    def zero_padded_string(value, length):
        a = str(value).zfill(length)
        return a

    @staticmethod
    def force_range(v, maximum):
        """ Force v to be >= 0 and < max
            Used for circular ranges (clock (0-23), circle (0-359 degrees), etc.)
        """
        if v < 0:
            return v + maximum
        elif v >= maximum:
            return v - maximum

        return v

    def get_datetime(self, hr, minute):
        if self.now is None:
            self.now = datetime.now()

        now_timestamp = time.time()
        now = datetime.fromtimestamp(now_timestamp)

        then = now.replace(hour=int(hr), minute=int(minute), second=0, microsecond=0)

        return then

    def get_current_utc(self):
        """
        @return: Current UTC time
        """
        self.now = datetime.now()
        return [self.now.day, self.now.month, self.now.year]

    @staticmethod
    def utc_to_local(utc_hr, utc_min, utc):
        now_timestamp = time.time()

        now = datetime.fromtimestamp(now_timestamp)
        now_utc = datetime.utcfromtimestamp(now_timestamp)
        offset = now - now_utc

        then = now.replace(hour=int(utc_hr), minute=int(utc_min), second=0, microsecond=0)

        then += offset

        utc += (then.hour-utc_hr)

        return [then.hour, then.minute, utc]


class Days(object):
    def __init__(self):
        pass


class llog(object):
    def __init__(self):
        pass

    def info(self, msg):
        print ("INFO: {}".format(msg))

    def trace(self, msg):
        print ("TRACE: {}".format(msg))

    def debug(self, msg):
        print ("DEBUG: {}".format(msg))

    def error(self, msg):
        print ("ERROR: {}".format(msg))