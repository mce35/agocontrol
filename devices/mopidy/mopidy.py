import requests
import json
import threading, Queue
import time
try:
    import websocket
    WebSocketsAvailabe = True
except ImportError:
    WebSocketsAvailabe = False

#from enum import Enum

#class RunningStates(Enum):

class RunningStates(object):
    """Valid running states of a player"""
    On = 1
    Off = 2


class PlayingStates(object):
    """Valid playing states of a player"""
    Unknown = 0
    Playing = 1
    Paused = 2
    Stopped = 3
    #Streaming = 4


class Mopidy():
    """Class to access the Mopidy JSON RPC 2.0 API """

    def __init__(self, host, port, app, q=None):
        self.host = host
        self.port = port

        self.app = app
        try:
            self.log = app.log
        except AttributeError:
            #We seem to be in test mode, need a local logger
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
            self.log = llog()

        self.url = "http://" + self.host + ":" +  self.port + "/mopidy/rpc"
        self.wsurl = "ws://" + self.host + ":" + self.port + "/mopidy/ws"
        self.headers = {'content-type': 'application/json'}
        self.version = self.GetVersion()
        self.trackinfo = {}
        self.trackinfo = {'title': 'None', 'album': 'None', 'artist': 'None', 'cover': None} #TODO: Get cover as b64
        self.runningstate = RunningStates.Off
        self.playingstate = PlayingStates.Unknown
        self.connected = False
        self.q = q

        if self.version is None:
            self.connected = False
            self.log.error('Error communicating with the Mopidy player')
        else:
            self.connected = True
            self.setRunningState(RunningStates.On)
            self.trackinfo = self.GetCurrentTrackInfo()

        if self.connected and WebSocketsAvailabe:
            websocket.enableTrace(True) # TODO: Remove
            ws = websocket.WebSocketApp(self.wsurl, on_message=self.OnMessage, on_error=self.OnError, on_close=self.OnClose)
            if ws:
                self.log.info('WS ok')
            else:
                self.log.info('WS not ok')
            wst = threading.Thread(target=ws.run_forever)
            wst.daemon = True
            wst.start()

            conn_timeout = 5

            while not ws.sock.connected and conn_timeout:
                self.log.info('not yet connected')
                time.sleep(1)
                conn_timeout -= 1

    def OnMessage(self, ws, message):
        result = json.loads(message)
        #Response from Mopidy when playing from Spotify
        #{"tl_track":
        #  {"track":
        #    {"album":
        #       {"date": "2011",
        #        "__model__": "Album",
        #        "artists":
        #            [{"__model__": "Artist",
        #              "name": "Various Artists",
        #              "uri": "spotify:artist:0LyfQWJT6nXafLPZqxe9Of"}],
        #        "name": "Sucker Punch",
        #        "uri": "spotify:album:0pdTBKhW8Du8Iher1xi8Gb"},
        #     "__model__": "Track",
        #     "name": "Sweet Dreams (Are Made Of This) - Sucker Punch: Original Motion Picture Soundtrack",
        #     "disc_no": 0,
        #     "uri": "spotify:track:0cBTbpSTj66GXHBE4eJWdv",
        #     "length": 319000,
        #     "track_no": 1,
        #     "artists": [
        #        {"__model__": "Artist",
        #         "name": "Dave Stewart",
        #         "uri": "spotify:artist:7gcCQIlkkfbul5Mt0jBQkg"},
        #        {"__model__": "Artist",
        #         "name": "Emily Browning",
        #         "uri": "spotify:artist:0ncLUhzYnidjOPYR2DPd7d"},
        #        {"__model__": "Artist",
        #         "name": "Annie Lennox",
        #         "uri": "spotify:artist:5MspMQqdVbdwP6ax3GXqum"}],
        #      "date": "2011",
        #      "bitrate": 320},
        #      "__model__": "TlTrack",
        #      "tlid": 68},
        #"event": "track_playback_started"}

        if u'tl_track' in result and u'track' in result[u'tl_track']:
            tr = result[u'tl_track'][u'track']
            if u'album' in tr:
                album = tr[u'album'][u'name']
            if u'artists' in tr[u'album']:
                artist = tr[u'album'][u'artists'][0][u'name']
            track = tr[u'name']

            if self.q != None:
                trackinfo = {'title': track, 'album': album, 'artist': artist, 'cover': None}  # TODO: Get cover as b64
                self.q.put(trackinfo)

    def OnClose(self, ws):
        self.log.info('WS close received')

    def OnError(self, ws, error):
        self.log.info('WS error received - ' + str(error))

    def send(self, ws, method):
        payload = '{\
        "method": "' + method + '",\
        "jsonrpc": "2.0",\
        "params": {},\
        "id": 1\
        }'

        ws.send(payload)


    def setRunningState(self, runningstate):
        self.runningstate = runningstate

    def setPlayingState(self, playingstate):
        self.playingtate = playingstate

    def call_mopidy(self, method, parm1=None, val1=None):
        """Call Mopidy JSON RPC API"""
        if parm1 == None:
            payload = {
                "method": method,
                "jsonrpc": "2.0",
                "params": {},
                "id": 1
            }
        else:
            payload = {
                "method": method,
                "jsonrpc": "2.0",
                "params": {parm1 : val1},
                "id": 1
            }

        try:
            response = requests.post(self.url, data=json.dumps(payload), headers=self.headers).json()
            if "result" in response:
                self.connected = True
                return response["result"]
        except requests.exceptions.ConnectionError:
            self.connected = False
            #TODO: Add logging
            #TODO: figure out how to set self.connected = False
            return None

    def GetVersion(self):
        """Get Mopidy version"""
        result = self.call_mopidy("core.get_version")
        return result

    def GetCurrentTITrack(self):
        """Get current track"""
        result = self.call_mopidy("core.playback.get_current_tl_track")
        return result # TODO: Extract artist and track info. Place in class members?

    def Pause(self):
        """Pause the player"""
        result = self.call_mopidy("core.playback.pause")
        self.setPlayingState(PlayingStates.Paused)
        return True #TODO: Check playing state and base return on actuals

    def Play(self):
        """Send a Play command to Mopidy. If a track is playing, it will be played from the begining"""
        result = self.call_mopidy("core.playback.play")
        self.setPlayingState(PlayingStates.Playing)
        return True #TODO: Check playing state and base return on actuals

    def Stop(self):
        """Send a Stop command to Mopidy."""
        result = self.call_mopidy("core.playback.stop")
        self.setPlayingState(PlayingStates.Stopped)
        return True #TODO: Check playing state and base return on actuals

    def NextTrack(self):
        """Send a NextTrack command to Mopidy."""
        result = self.call_mopidy("core.playback.next")
        return True #TODO: Check playing state and base return on actuals

    def PreviousTrack(self):
        """Send a PreviousTrack command to Mopidy."""
        result = self.call_mopidy("core.playback.previous")
        return True #TODO: Check playing state and base return on actuals

    def SetVolume(self, volume):
        """Set volume"""
        #print "volume=" + str(volume)
        result = self.call_mopidy("core.playback.set_volume", "volume", int(volume))
        return True #TODO: Check playing state and base return on actuals


    def GetState(self):
        """Get playing state from Mopidy"""
        result = self.call_mopidy("core.playback.get_state")
        if 'playing' in result:
            self.setPlayingState(PlayingStates.Playing)
        elif 'paused' in result:
            self.setPlayingState(PlayingStates.Paused)
        return result

    def GetCurrentTrackInfo(self):
        """Get info on currently loaded track in Mopidy."""
        # TODO: Look into handling trackinfo for streamed content

        artist = "<none>"
        album = "<none>"
        track = "<none>"

        try:
            result = self.call_mopidy("core.playback.get_current_track")
            if result and u'album' in result:
                #print 'date=' + result[u'album'][u'date']
                album = result[u'album'][u'name']
                if u'artists' in result[u'album']:
                    artist = result[u'album'][u'artists'][0][u'name']
                track = result[u'name']
        finally:
            pass

        self.TrackInfo = {'title': track, 'album': album, 'artist': artist, 'cover': None} #TODO: Get cover as b64

        return self.TrackInfo


if __name__ == "__main__":
    # TODO: Move to testsuite
    a = Mopidy("192.168.1.237", "6680")
    #print a.GetVersion()
    #print a.GetCurrentTITrack()
    print a.GetState()
    a.Pause()
    print a.GetState()
    a.Play()
    print a.GetState()
    a.GetCurrentTrackInfo()
    print "Next track"
    a.NextTrack()
    a.GetCurrentTrackInfo()
    print "Next track"
    a.NextTrack()
    a.GetCurrentTrackInfo()
    a.SetVolume(75)

    while True:
        time.sleep(5)



