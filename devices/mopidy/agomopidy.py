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
from mopidy import Mopidy

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
            player = self.players[internalid]["mopidy"]
            if content["command"] == "on":
                self.log.debug("switching on: %s", internalid)
                # TODO Add ON command
                self.connection.emit_event(internalid, "event.device.statechanged", "255", "")

            elif content["command"] == "off":
                self.log.debug("switching off: %s", internalid)
                # TODO Add OFF command
                self.connection.emit_event(internalid, "event.device.statechanged", "0", "")

            elif content["command"] == "play":
                self.log.debug("Start playing: player %s", internalid)
                # self.players[internalid]["mopidy"].Play()
                player.Play()
                #    "event.device.statechanged", "255", "")
            elif content["command"] == "pause":
                self.log.debug("Pause player %s", internalid)
                # self.players[internalid]["mopidy"].Pause()
                player.Pause()
                #    "event.device.statechanged", "255", "")

            elif content["command"] == "next":
                self.log.debug("Next track. Player %s", internalid)
                # self.players[internalid]["mopidy"].NextTrack()
                player.NextTrack()
                #    "event.device.statechanged", "255", "")
            elif content["command"] == "previous":
                self.log.debug("Previous track. Player %s", internalid)
                # self.players[internalid]["mopidy"].PreviousTrack()
                player.PreviousTrack()
                #    "event.device.statechanged", "255", "")

            elif content["command"] == "setvolume":
                self.log.debug("Set volume for player %s", internalid)
                if 'volume' in content:
                    # print "volume=" + str(content['volume'])
                    player.SetVolume(content['volume'])
                    return True
                else:
                    self.log.info('Missing parameter "volume" to command SETVOLUME. Player=%s", internalid')
                    return False

                # "event.device.statechanged", "255", "")

            elif content["command"] == "mediainfos":
                infos = player.GetCurrentTrackInfo()
                self.log.info("Requested to get MediaInfo. Player %s", internalid) # TODO: revert to debug
                return infos

    def setup_app(self):
        self.connection.add_handler(self.message_handler)

        # If we want to use the custom command line argument, we look at
        # self.args:
        if self.args.test:
            self.log.info("Test argument was set")
        else:
            self.log.warning("Test argument was NOT set")

        self.players = {}
        players = self.get_config_option("Players", "[none]")

        for p in players.split(","):
            # TODO: remove spaces from p?
            self.log.debug('Player ' + p + ' found')
            host = self.get_config_option("host", "127.0.0.1", section=p, app='mopidy')
            port = self.get_config_option("port", "6680", section=p, app='mopidy')
            self.connection.add_device(p, "mopidy")
            self.log.info("Mopidy player %s added. host=%s, port=%s", p, host, port)

            mop = Mopidy(host, port)
            if mop.connected:
                self.log.info("Mopidy player %s connected.", p)
            else:
                self.log.info("Mopidy player %s NOT connected.", p)

            player = {
                "name"   : p,
                "host"   : host,
                "port"   : port,
                "mopidy" : mop
            }
            self.players[p] = player
            self.emit_media_info(p)
            self.log.info("Now playing: %s - %s - %s ", self.players[p]["mopidy"].TrackInfo["artist"],
                          self.players[p]["mopidy"].TrackInfo["album"],
                          self.players[p]["mopidy"].TrackInfo["title"])  #TODO: change to debug

            BACKGROUND = MediaInfoCollector(self, mop)
            BACKGROUND.setDaemon(True)
            BACKGROUND.start()

    def app_cleanup(self):
        # When our app is about to shutdown, we should clean up any resources we've
        # allocated in app_setup. This is done here.
        # In this example, we do not have any resources..
        pass

    def emit_media_info(self, player):
        self.log.info('emit MEDIAINFO')
        TrackInfo = self.players[player]["mopidy"].GetCurrentTrackInfo()

        TrackInfo = {'title': self.players[player]["mopidy"].TrackInfo["title"],
                     'album': self.players[player]["mopidy"].TrackInfo["album"],
                     'artist': self.players[player]["mopidy"].TrackInfo["artist"],
                     'cover': None}

        # TrackInfo = {'title': player.TrackInfo["title"],
        #             'album': player.TrackInfo["album"],
        #             'artist': player.TrackInfo["artist"],
        #             'cover': None}

        self.connection.emit_event_raw(player, "event.device.mediainfos", TrackInfo)


class MediaInfoCollector(threading.Thread):
    """Class driving a thread to collect media info."""
    def __init__(self, app, player):
        threading.Thread.__init__(self)
        self.app = app
        self.player = player

    def run(self):
        # TODO: Add a proper shutdown..
        # self.player.GetCurrentTrackInfo()
        # self.app.emit_media_info(self.player)
        time.sleep(30)


if __name__ == "__main__":
    AgoMopidy().main()

