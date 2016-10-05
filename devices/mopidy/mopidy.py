import requests
import json

#TODO: Add error handling

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

    def GetVersion(self):
        """Get Mopidy version"""
        payload = {
            "method": "core.get_version",
            "jsonrpc": "2.0",
            "params": {},
            "id": 1
        }

        response = requests.post(self.url, data=json.dumps(payload), headers=self.headers).json()
        # print requests.exceptions.ConnectionError
        # assert response["jsonrpc"]
        print response
        if "result" in response:
            print "OK"
            return response["result"]
        else:
            if "error" in response:
                print "Not OK"
                return "" #TODO: return None?

    def GetCurrentTITrack(self):
        """Get current track"""
        payload = {
            "method": "core.playback.get_current_tl_track",
            "jsonrpc": "2.0",
            "params": {},
            "id": 1
        }

        response = requests.post(self.url, data=json.dumps(payload), headers=self.headers).json()
        # print requests.exceptions.ConnectionError
        return response["result"] # TODO: Extract artist and track info. Place in class members?


    def Pause(self):
        """Get current track"""
        payload = {
            "method": "core.playback.pause",
            "jsonrpc": "2.0",
            "params": {},
            "id": 1
        }

        response = requests.post(self.url, data=json.dumps(payload), headers=self.headers).json()
        # print requests.exceptions.ConnectionError
        return True #TODO: Check playing state and base return on actuals


    def Play(self):
        """Get current track"""
        payload = {
            "method": "core.playback.play",
            "jsonrpc": "2.0",
            "params": {},
            "id": 1
        }

        response = requests.post(self.url, data=json.dumps(payload), headers=self.headers).json()
        # print requests.exceptions.ConnectionError
        return True #TODO: Check playing state and base return on actuals


if __name__ == "__main__":
    a = Mopidy("192.168.1.237", "6680")
    print a.GetVersion()
    #print a.GetCurrentTITrack()
    a.Play()
    #a.Pause()



