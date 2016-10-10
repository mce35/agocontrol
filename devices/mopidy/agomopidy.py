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
import threading, Queue
import time
import websocket
from mopidy import Mopidy
from mopidy import RunningStates
from mopidy import PlayingStates

STATE_OFF = "0"
STATE_ON = "255"
STATE_STREAM = "50"
STATE_PLAY = "100"
STATE_STOP = "150"
STATE_PAUSE = "200"

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
            self.internalid = internalid
            # TODO: Move thread start here
            player = self.players[internalid]["mopidy"]
            if content["command"] == "on":
                self.log.debug("switching on: %s", internalid)
                # TODO Add ON command
                self.connection.emit_event(internalid, "event.device.statechanged", STATE_ON, "")
                self.players[internalid]["mopidy"].playingstate = RunningStates.On
                self.emit_media_info(internalid)

            elif content["command"] == "off":
                self.log.debug("switching off: %s", internalid)
                # TODO Add OFF command
                self.connection.emit_event(internalid, "event.device.statechanged", STATE_OFF, "")
                self.players[internalid]["mopidy"].playingstate = RunningStates.Off

            elif content["command"] == "play":
                self.log.debug("Start playing: player %s", internalid)
                self.connection.emit_event(internalid, "event.device.statechanged", STATE_PLAY, "")
                player.Play()
                self.emit_media_info(internalid)
                self.state = "playing"
            elif content["command"] == "pause":
                self.log.debug("Pause player %s", internalid)
                self.connection.emit_event(internalid, "event.device.statechanged", STATE_PAUSE, "")
                player.Pause()
                self.emit_media_info(internalid)
                self.state = "paused"
            elif content["command"] == "stop":
                self.log.debug("Stoping playing: player %s", internalid)
                self.connection.emit_event(internalid, "event.device.statechanged", STATE_STOP, "")
                player.Stop()
                self.emit_media_info(internalid)
                self.state = "stopped"

            elif content["command"] == "next":
                self.log.debug("Next track. Player %s", internalid)
                player.NextTrack()
                self.emit_media_info(internalid)
            elif content["command"] == "previous":
                self.log.debug("Previous track. Player %s", internalid)
                player.PreviousTrack()
                self.emit_media_info(internalid)

            elif content["command"] == "setvolume":
                self.log.debug("Set volume for player %s", internalid)
                if 'volume' in content:
                    # print "volume=" + str(content['volume'])
                    player.SetVolume(content['volume'])
                    return True
                else:
                    self.log.info('Missing parameter "volume" to command SETVOLUME. Player=%s", internalid')
                    return False

            elif content["command"] == "mediainfos":
                infos = player.GetCurrentTrackInfo()
                self.log.info("Requested to get MediaInfo. Player %s", internalid) # TODO: revert to debug
                return infos

    def setup_app(self):
        self.connection.add_handler(self.message_handler)
        self.internalid = None

        self.players = {}
        playerlist = self.get_config_option("Players", "[none]")


        if ',' not in playerlist:
            playerlist += ','

        print "playerlist=" + playerlist

        for p in playerlist.split(","):
            # TODO: remove spaces from p?
            if len(p) > 0:
                print "p=" + p
                self.log.debug('Player ' + p + ' found')
                host = self.get_config_option("host", "127.0.0.1", section=p, app='mopidy')
                port = self.get_config_option("port", "6680", section=p, app='mopidy')
                self.connection.add_device(p, "mopidy")
                self.log.info("Mopidy player %s added. host=%s, port=%s", p, host, port)

                q = Queue.Queue()

                mop = Mopidy(host, port, q)
                if mop.connected:
                    self.log.info("Mopidy player %s connected.", p)
                else:
                    self.log.info("Mopidy player %s NOT connected.", p)

                player = {
                    "name"   : p,
                    "host"   : host,
                    "port"   : port,
                    "mopidy" : mop,
                    "queue"  : q
                }
                self.players[p] = player
                self.emit_media_info(p)
                self.log.info("Now playing: %s - %s - %s ", self.players[p]["mopidy"].TrackInfo["artist"],
                              self.players[p]["mopidy"].TrackInfo["album"],
                              self.players[p]["mopidy"].TrackInfo["title"])  #TODO: change to debug

                if mop.runningstate == RunningStates.On:
                    self.connection.emit_event(p, "event.device.statechanged", STATE_ON, "")
                else:
                    self.connection.emit_event(p, "event.device.statechanged", STATE_OFF, "")

                BACKGROUND = MediaInfoCollector(self, p, q)
                BACKGROUND.setDaemon(True)
                BACKGROUND.start()

    def app_cleanup(self):
        # When our app is about to shutdown, we should clean up any resources we've
        # allocated in app_setup. This is done here.
        pass

    def emit_media_info(self, player):
        self.log.info('emitting MEDIAINFOS')
        TrackInfo = self.players[player]["mopidy"].GetCurrentTrackInfo()
        self.connection.emit_event_raw(player, "event.device.mediainfos", TrackInfo)

class MediaInfoCollector(threading.Thread):
    """Class driving a thread to collect media info."""
    # TODO: Replace with event monitoring via Mopidy WebSocket API
    def __init__(self, app, player, q):
        super(MediaInfoCollector, self).__init__()
        self.app = app
        self.player = player
        self.q = q
        self.stoprequest = threading.Event()

    def join(self, timeout=None):
        """Shutdown thread"""
        self.stoprequest.set()
        super(MediaInfoCollector, self).join(timeout)

    def run(self):
        while not self.stoprequest.isSet():
            try:
                mediainfo = self.q.get(True, 0.5)
                #print "MediaCollector: mediainfo=" + str(mediainfo)
                self.app.connection.emit_event_raw(self.player, "event.device.mediainfos", mediainfo)
            except Queue.Empty:
                continue


if __name__ == "__main__":
    AgoMopidy().main()

