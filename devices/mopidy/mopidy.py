import requests
import json
import websocket
import threading, Queue
import time

#from enum import Enum

#class RunningStates(Enum):
class RunningStates():
    On = 1
    Off = 2

class PlayingStates():
    Unknown = 0
    Playing = 1
    Paused = 2
    Stopped = 3
    #Streaming = 4


class Mopidy():
    """Class to access the Mopidy JSON RPC 2.0 API """

    def __init__(self, host, port, q=None):
        self.host = host
        self.port = port
        self.url = "http://" + self.host + ":" +  self.port + "/mopidy/rpc"
        self.wsurl  = "ws://" + self.host + ":" + self.port + "/mopidy/ws"
        self.headers = {'content-type': 'application/json'}
        self.version = self.GetVersion()
        self.TrackInfo = {}
        self.TrackInfo = {'title': 'None', 'album': 'None', 'artist': 'None', 'cover': None} #TODO: Get cover as b64
        self.runningstate = RunningStates.Off
        self.playingstate = PlayingStates.Unknown
        self.connected = False
        self.q = q

        if self.version == None:
            self.connected = False
            #print "Error communicating with the Mopidy player"
        else:
            self.connected = True
            self.setRunningState(RunningStates.On)
            self.TrackInfo = self.GetCurrentTrackInfo()

        if self.connected:
            websocket.enableTrace(True) # TODO: Remove
            ws = websocket.WebSocketApp(self.wsurl, on_message=self.OnMessage, on_error=self.OnError, on_close = self.OnClose)
            if ws:
                print "WS ok"
            else:
                print "WS no OK"
            wst = threading.Thread(target=ws.run_forever)
            wst.daemon = True
            wst.start()
            print "1"


            conn_timeout = 5

            while not ws.sock.connected and conn_timeout:
            #if not ws.sock.connected:
                print "not yet connected"
                time.sleep(1)
                conn_timeout -= 1

            # self.send(ws, "core.get_version")

    def OnMessage(self, ws, message):
        #print message
        result = json.loads(message)
        #print result
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
            print "tr - " + str(tr)
            # print 'date=' + result[u'album'][u'date']
            if u'album' in tr:
                album = tr[u'album'][u'name']
            if u'artists' in tr[u'album']:
                artist = tr[u'album'][u'artists'][0][u'name']
            track = tr[u'name']

            print "artist=" + artist
            print 'album=' + album
            print "track=" + track

            if self.q != None:
                TrackInfo = {'title': track, 'album': album, 'artist': artist, 'cover': None}  # TODO: Get cover as b64
                self.q.put (TrackInfo)

    def OnClose(self, ws):
        print "WS close received"

    def OnError(self, ws, error):
        print "WS error received - " + str(error)

    def send(self, ws, method):
        payload = '{\
        "method": "' + method + '",\
        "jsonrpc": "2.0",\
        "params": {},\
        "id": 1\
        }'

        print  payload

        ws.send(payload)
        print "Sent"


    def setRunningState (self, runningstate):
        self.runningstate = runningstate

    def setPlayingState(self, playingstate):
        self.playingtate = playingstate

    def CallMopidy(self, method, parm1=None, val1=None):
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
            # assert response["jsonrpc"]
            if "result" in response:
                self.connected = True
                #print response
                return response["result"]
        except requests.exceptions.ConnectionError:
            self.connected = False
            #print response #requests.exceptions.ConnectionError
            #TODO: Add logging
            #TODO: figure out how to set self.connected = False
            return None

    def GetVersion(self):
        """Get Mopidy version"""
        result = self.CallMopidy("core.get_version")
        #print result
        return result

    def GetCurrentTITrack(self):
        """Get current track"""
        result = self.CallMopidy("core.playback.get_current_tl_track")
        print result
        return result # TODO: Extract artist and track info. Place in class members?

    def Pause(self):
        """Pause the player"""
        result = self.CallMopidy("core.playback.pause")
        self.setPlayingState(PlayingStates.Paused)
        return True #TODO: Check playing state and base return on actuals

    def Play(self):
        """Send a Play command to Mopidy. If a track is playing, it will be played from the begining"""
        result = self.CallMopidy("core.playback.play")
        self.setPlayingState(PlayingStates.Playing)
        return True #TODO: Check playing state and base return on actuals

    def Stop(self):
        """Send a Stop command to Mopidy."""
        result = self.CallMopidy("core.playback.stop")
        self.setPlayingState(PlayingStates.Stopped)
        return True #TODO: Check playing state and base return on actuals

    def NextTrack(self):
        """Send a NextTrack command to Mopidy."""
        result = self.CallMopidy("core.playback.next")
        return True #TODO: Check playing state and base return on actuals

    def PreviousTrack(self):
        """Send a PreviousTrack command to Mopidy."""
        result = self.CallMopidy("core.playback.previous")
        return True #TODO: Check playing state and base return on actuals

    def SetVolume(self, volume):
        """Set volume"""
        #print "volume=" + str(volume)
        result = self.CallMopidy("core.playback.set_volume", "volume", int(volume))
        return True #TODO: Check playing state and base return on actuals


    def GetState(self):
        """Get playing state from Mopidy"""
        result = self.CallMopidy("core.playback.get_state")
        if 'playing' in result:
            self.setPlayingState(PlayingStates.Playing)
        elif 'paused' in result:
            self.setPlayingState(PlayingStates.Paused)
        return result

    def GetCurrentTrackInfo(self):
        """Get info on currently loaded track in Mopidy."""
        # TODO: Look into handling trackinfo for streamed content

        artist = "<none>"
        album  = "<none>"
        track  = "<none>"

        try:
            result = self.CallMopidy("core.playback.get_current_track")
            if result and u'album' in result:
                #print 'date=' + result[u'album'][u'date']
                album= result[u'album'][u'name']
                if u'artists' in result[u'album']:
                    artist = result[u'album'][u'artists'][0][u'name']
                track = result[u'name']
        finally:
            pass

        #print "artist=" + artist
        #print 'album=' + album
        #print "track=" + track

        self.TrackInfo = {'title': track, 'album': album, 'artist': artist, 'cover': None} #TODO: Get cover as b64

        return self.TrackInfo


if __name__ == "__main__":
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

    while 1==1:
        time.sleep(5)
