import requests
import json

class Mopidy():
    """Class to access the Mopidy JSON RPC 2.0 API """

    def __init__(self ,host, port):
        self.host = host
        self.port = port
        self.url = "http://" + self.host + ":" +  self.port + "/mopidy/rpc"
        self.headers = {'content-type': 'application/json'}
        self.version = self.GetVersion()
        if self.version == "":
            print "Error communicating with the Mopidy player"

    def CallMopidy(self, method):
        """Call Mopidy JSON RPC API"""
        payload = {
            "method": method,
            "jsonrpc": "2.0",
            "params": {},
            "id": 1
        }

        try:
            response = requests.post(self.url, data=json.dumps(payload), headers=self.headers).json()
            if "result" in response:
                return response["result"]
        except requests.exceptions.ConnectionError:
            #print response #requests.exceptions.ConnectionError
            #TODO: Add logging
            return None

    def GetVersion(self):
        """Get Mopidy version"""
        result = self.CallMopidy("core.get_version")

        # print requests.exceptions.ConnectionError
        # assert response["jsonrpc"]

        if result:
            return result
        else:
            #if "error" in response:
            print "Not OK"
            return "" #TODO: return None?

    def GetCurrentTITrack(self):
        """Get current track"""
        result = self.CallMopidy("core.playback.get_current_tl_track")
        print result

        # print requests.exceptions.ConnectionError
        return result # TODO: Extract artist and track info. Place in class members?

    def Pause(self):
        """Pause the player"""
        result = self.CallMopidy("core.playback.pause")

        return True #TODO: Check playing state and base return on actuals

    def Play(self):
        """Send a Play command to Mopidy. If a track is playinjg, it will be played from the begining"""
        result = self.CallMopidy("core.playback.play")

        # print requests.exceptions.ConnectionError
        return True #TODO: Check playing state and base return on actuals

    def GetState(self):
        """Get playing state from Mopidy"""
        result = self.CallMopidy("core.playback.get_state")

        return result

    def GetCurrentTrackInfo(self):
        """Send a Play command to Mopidy. If a track is playinjg, it will be played from the begining"""
        result = self.CallMopidy("core.playback.get_current_track")

        artist = "<none>"
        album  = "<none>"
        track  = "<none>"

        if u'album' in result:
            #print 'date=' + result[u'album'][u'date']
            album= result[u'album'][u'name']
            if u'artists' in result[u'album']:
                artist = result[u'album'][u'artists'][0][u'name']
            track = result[u'name']

        print "artist=" + artist
        print 'album=' + album
        print "track=" + track

        TrackInfo = {'title': track, 'album': album, 'artist': artist, 'cover': None} #TODO: Get cover as b64

        return TrackInfo


if __name__ == "__main__":
    a = Mopidy("192.168.1.237", "6680")
    #print a.GetVersion()
    #print a.GetCurrentTITrack()
    print a.GetState()
    #a.Pause()
    #a.GetState()
    #a.Play()
    #a.GetState()
    a.GetCurrentTrackInfo()




