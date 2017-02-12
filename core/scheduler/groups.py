#!/usr/bin/python

import json

basedir = "/etc/opt/agocontrol/"  #TODO: Get from config
vmap = "maps/groupsmap.json"

class Groups:
    """
    Collection of Groups
    """
    def __init__(self, filename, log=None):
        if log is not None:
            self.log = log
        else:
            self.log = llog()
        self.Groups = []

        if filename is not None:
            with open(filename) as conf_file:
                jsonstr = json.load(conf_file)

        for element in jsonstr["groups"]:
            # self.log.trace(element)
            grp = Group(element, self.log)
            self.Groups.append(grp)
            #print Group

    def find(self, uuid):
        grp = None
        for r in self.Groups:
            if r.uuid == uuid:
                grp = r
        return grp

class Group(object):
    """
    Represent a Group, containing Groups definition
    """
    def __init__(self, jsonstr, log):
        self.log = log
        self._name = jsonstr["name"]
        self.uuid = jsonstr["uuid"]
        self.devices = []
        for d in jsonstr["devices"]:
            self.devices.append(d)

    def __str__(self):
        """
        Return a string representing content of the Group object
        """
        s = "name={}, uuid={}, # devices: {} ".format(self._name, self.uuid, len(self.devices))
        return s

    @property
    def name(self):
        return self._name


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