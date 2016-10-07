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
                self.players[internalid].mopidy.play()
                #    "event.device.statechanged", "255", "")
            elif content["command"] == "pause":
                self.log.debug("Pause player %s", internalid)
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

        # use the self.get_config_option method.
        # This will look in the configuration file conf.d/<app name>.conf
        # under the [<app name>] section.
        # In this example, this means example.conf and [example] section
        players = self.get_config_option("players", "[none]")

        for p in players:
            host = self.get_config_option("host", "127.0.0.1", section=p, app='mopidy')
            port = self.get_config_option("host", "6680", section=p, app='mopidy')
            self.connection.add_device("Mopidy", "mopidy", p)
            self.log.info("Mopidy player %s added. host=%s, port=%s", host, port)
            self.players[p] = self.mopidy(host, port)  # TODO: Add user/pw etc.?
            self.players[p] = self.emit_media_infos()

    def app_cleanup(self):
        # When our app is about to shutdown, we should clean up any resources we've
        # allocated in app_setup. This is done here.
        # In this example, we do not have any resources..
        pass

    def emit_media_info(self, player):
        self.log.debug('emit MEDIAINFO')
        TrackInfo = self.players[player].GetCurrentTrackInfo()
        #client.emit_event_raw(player, "event.device.mediainfos", TrackInfo)
        self.connection.emit_event(player, "event.device.mediainfos", TrackInfo, "")

if __name__ == "__main__":
    AgoMopidy().main()

