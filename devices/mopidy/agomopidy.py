#!/usr/bin/python
AGO_MOPIDY_VERSION = '0.0.1'
############################################
#
# Mopidy support for ago control
#
__author__ = 'Joakim Lindbom'
__copyright__ = 'Copyright 2013-2016, Joakim Lindbom'
__date__ = '2016-10-01'
__credits__ = ['Joakim Lindbom', 'The ago control team']
__license__ = 'GPL Public License Version 3'
__maintainer__ = 'Joakim Lindbom'
__email__ = 'Joakim.Lindbom@gmail.com'
__status__ = 'Experimental'
__version__ = AGO_MOPIDY_VERSION
############################################

import agoclient
import threading
import time
#import pykka
import mopidy.py

# When developing, it is recommended to run with -d or -t argument,
# to enable debug/trace logging and log to console.
# Please refer to -h for other options.

class AgoMopidy(agoclient.AgoApp):
    """AgoControl device for the Mopidy music player"""
    def app_cmd_line_options(self, parser):
        """App-specific command line options"""
        parser.add_argument('-T', '--test', action="store_true",
                help="This can be set or not set")

        parser.add_argument('--set-parameter',
                help="Set this to do set_config_option with the value upon startup")

    def message_handler(self, internalid, content):
        """The messagehandler."""
        if "command" in content:
            if content["command"] == "on":
                self.log.debug("switching on: %s", internalid)

                                # Emit an event which notifies all other components of the state change
                #self.connection.emit_event(internalid,
                #    "event.device.statechanged", "255", "")
            if content["command"] == "play":
                self.log.debug("Start playing: player %s", internalid)
                self.mopidy.play()
                #    "event.device.statechanged", "255", "")



            elif content["command"] == "off":
                self.log.debug("switching off: %s", internalid)
                self.connection.emit_event(internalid, "event.device.statechanged", "0", "")

    def setup_app(self):
        self.connection.add_handler(self.message_handler)

        # If we want to use the custom command line argument, we look at
        # self.args:
        if self.args.test:
            self.log.info("Test argument was set")
        else:
            self.log.warning("Test argument was NOT set")

        # if you need to fetch any settings from config.ini,
        # use the self.get_config_option method.
        # This will look in the configuration file conf.d/<app name>.conf
        # under the [<app name>] section.
        # In this example, this means example.conf and [example] section
        host = self.get_config_option("host", "127.0.0.1")
        self.log.info("Configuration parameter 'host' was set to %s", host)
        port = self.get_config_option("host", "6680")
        self.log.info("Configuration parameter 'port' was set to %s", port)

        if self.args.set_parameter:
            self.log.info("Setting configuration parameter 'host' to %s", self.args.set_parameter)
            self.set_config_option("host", self.args.set_parameter)

            param = self.get_config_option("host", "0")
            self.log.info("Configuration parameter 'host' is now set to %s", param)

        self.mopidy(host, port) #TODO: Add user/pw etc.?

        self.connection.add_device("Mopidy", "switch", 'example-switch')

        # for our threading example in the next section we also add a binary sensor:
        self.connection.add_device("125", "binarysensor")

        # then we add a BACKGROUND thread. This is not required and
        # just shows how to send events from a separate thread. This might
        # be handy when you have to poll something in the BACKGROUND or need
        # to handle some other communication. If you don't need one or if
        # you want to keep things simple at the moment just skip this section.
        BACKGROUND = TestEvent(self)
        BACKGROUND.setDaemon(True)
        BACKGROUND.start()

        # now you should have added all devices, set up all your internal
        # and device specific stuff, started everything like listener threads
        # or whatever.
        # setup_app will now return, and AgoApp will do the rest.
        # This normally means calling app_main, which calls the run() method
        # on the AgoConnection object, which will block and start message handling.

    def app_cleanup(self):
        # When our app is about to shutdown, we should clean up any resources we've
        # allocated in app_setup. This is done here.
        # In this example, we do not have any resources..
        pass


if __name__ == "__main__":
    AgoMopidy().main()

