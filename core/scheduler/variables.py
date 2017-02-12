#!/usr/bin/python

import json

basedir = "/etc/opt/agocontrol/"  #TODO: Get from config
vmap = "maps/variablesmap.json"


class variables(object):
    def __init__(self, filename=None):
        self.variables = {}
        self.refresh(filename)
        if filename is not None:
            self.filename = filename
        else:
            self.filename = basedir+vmap

        self.refresh(self.filename)

    def refresh(self, filename):
        #print "Using file {}".format(filename)
        self.variables = {}
        # TODO: Get via AgoConnection instead if direct from map file
        with open(filename) as conf_file:
            conf = json.load(conf_file)

        for k, v in conf.items():
            self.variables[k] = v

    def get_variable(self, variable):
        """ Gets the value for a global variable

        @param   variable: The variable to set
        @return: The value retrieved, or None if non-existent
        """
        self.refresh(self.filename)
        try:
            val = self.variables[variable]
        except KeyError:
            val = None

        return val


    def set_variable(self, variable, value):
        """ Sets the global variable, store it to the global map

        @param variable: The variable to set
        @param value:    The value to assign. Needs to be a string for now
        @return:
        """
        pass

    def add_variable(self, variable, value):
        """ Creates a new global variable, store it to the global map

        @param variable: The variable to set
        @param value:    The value to assign. Needs to be a string for now
        @return:
        """
        pass

    def del_variable(self, variable):
        """ Deletes a global variable from the global map

        @param variable: The variable to set
        @return:
        """
        pass



