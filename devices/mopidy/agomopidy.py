#!/usr/bin/python
import agoclient
import threading
import Queue
from mopidy import Mopidy
from mopidy import RunningStates
# from mopidy import PlayingStates

AGO_MOPIDY_VERSION = '0.0.1'
#
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
#


STATE_OFF = "0"
STATE_ON = "255"
STATE_STREAM = "50"
STATE_PLAY = "100"
STATE_STOP = "150"
STATE_PAUSE = "200"


class AgoMopidy(agoclient.AgoApp):
    """AgoControl device for the Mopidy music player"""

    def __init__(self):
        self.internalid = None
        self.state = None
        self.players = {}

        #super(AgoMopidy, self).__init__()
        agoclient.AgoApp.__init__(self)

    @staticmethod
    def app_cmd_line_options(parser):
        """App-specific command line options"""
        parser.add_argument('-T', '--test', action="store_true",
                            help="This can be set or not set")

        parser.add_argument('--set-parameter',
                            help="Set this to do set_config_option with the value upon startup")

    def message_handler(self, internalid, content):
        """The messagehandler."""
        if "command" in content:
            self.internalid = internalid
            player = self.players[internalid]["mopidy"]

            if content["command"] == "on":
                self.log.debug("switching on: %s", internalid)
                self.connection.emit_event(internalid, "event.device.statechanged", STATE_ON, "")
                self.players[internalid][
                    "mopidy"].playingstate = RunningStates.On
                self.emit_media_info(internalid)
            elif content["command"] == "off":
                self.log.debug("switching off: %s", internalid)
                self.connection.emit_event(internalid, "event.device.statechanged", STATE_OFF, "")
                self.players[internalid][
                    "mopidy"].playingstate = RunningStates.Off

            elif content["command"] == "play":
                self.log.debug("Start playing: player %s", internalid)
                player.Play()
                self.connection.emit_event(internalid, "event.device.statechanged", STATE_PLAY, "")
                self.emit_media_info(internalid)
                self.state = "playing"
            elif content["command"] == "pause":
                self.log.debug("Pause player %s", internalid)
                player.Pause()
                self.connection.emit_event(internalid, "event.device.statechanged", STATE_PAUSE, "")
                self.emit_media_info(internalid)
                self.state = "paused"
            elif content["command"] == "stop":
                self.log.debug("Stoping playing: player %s", internalid)
                player.Stop()
                self.connection.emit_event(internalid, "event.device.statechanged", STATE_STOP, "")
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
                    player.SetVolume(content['volume'])
                    return True
                else:
                    self.log.info(
                        'Missing parameter "volume" to command SETVOLUME. Player=%s", internalid')
                    return False

            elif content["command"] == "mediainfos":
                infos = player.GetCurrentTrackInfo()
                self.log.info(
                    "Requested to get MediaInfo. Player %s",
                    internalid)  # TODO: revert to debug
                return infos

    def setup_app(self):
        """ Initiate the mopidy device"""
        self.connection.add_handler(self.message_handler)
        self.internalid = None

        self.players = {}
        playerlist = self.get_config_option("Players", "[none]")

        if ',' not in playerlist:
            playerlist += ','

        for pl in playerlist.split(","):
            # TODO: remove spaces from p?
            if len(pl) > 0:
                self.log.debug('Player ' + pl + ' found')
                host = self.get_config_option("host", "127.0.0.1", section=pl, app='mopidy')
                port = self.get_config_option("port", "6680", section=pl, app='mopidy')
                self.connection.add_device(pl, "mopidy")
                self.log.info("Mopidy player %s added. host=%s, port=%s", pl, host, port)

                q1 = Queue.Queue()

                mop = Mopidy(host, port, self, q1)
                if mop.connected:
                    self.log.info("Mopidy player %s connected.", pl)
                else:
                    self.log.info("Mopidy player %s NOT connected.", pl)

                player = {
                    "name": pl,
                    "host": host,
                    "port": port,
                    "mopidy": mop,
                    "queue": q1
                }
                self.players[pl] = player
                self.emit_media_info(pl)
                self.log.info("Now playing: %s - %s - %s ",
                              self.players[pl]["mopidy"].TrackInfo["artist"],
                              self.players[pl]["mopidy"].TrackInfo["album"],
                              self.players[pl]["mopidy"].TrackInfo["title"])  # TODO: change to debug

                if mop.runningstate == RunningStates.On:
                    self.connection.emit_event(pl, "event.device.statechanged", STATE_ON, "")
                else:
                    self.connection.emit_event(pl, "event.device.statechanged", STATE_OFF, "")

                background_thread = MediaInfoCollector(self, pl, q1)
                background_thread.setDaemon(True)
                background_thread.start()

    def app_cleanup(self):
        """Any cleanup of resources goes here"""
        pass

    def emit_media_info(self, player):
        """Emit media info to agocontroll message bus"""
        self.log.info('emitting MEDIAINFOS')
        trackinfo = self.players[player]["mopidy"].GetCurrentTrackInfo()
        self.connection.emit_event_raw(player, "event.device.mediainfos", trackinfo)


class MediaInfoCollector(threading.Thread):

    """Class driving a thread to collect media info."""

    def __init__(self, app, player, q1):
        """Constructor - connect to a Queue receiving media change info received via Mopidy WebSocket API"""
        super(MediaInfoCollector, self).__init__()
        self.app = app
        self.player = player
        self.q1 = q1
        self.stoprequest = threading.Event()

    def join(self, timeout=None):
        """Shutdown thread"""
        self.stoprequest.set()
        super(MediaInfoCollector, self).join(timeout)

    def run(self):
        """Main thread logic. Poll queue and relay to agocontrol message bus"""
        while not self.stoprequest.isSet():
            try:
                mediainfo = self.q1.get(True, 0.5)
                self.app.connection.emit_event_raw(self.player, "event.device.mediainfos", mediainfo)
            except Queue.Empty:
                continue


if __name__ == "__main__":
    AgoMopidy().main()
