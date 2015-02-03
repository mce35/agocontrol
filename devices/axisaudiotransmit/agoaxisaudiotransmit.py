#!/usr/bin/env python

import requests
import sys
import base64
import time
from socket import socket

import agoclient

# workaround with socket as the axis cgi returns an empty line, causing a httplib exception
def send_audio(host, port, path, username, password, data):
    base64string = base64.encodestring('%s:%s' % (username, password)).replace('\n', '')

    s = socket()
    s.connect((host, port))
    # print s.recv(4096)
    s.send("POST %s HTTP/1.1\r\n" % path)
    s.send("Authorization: Basic %s\r\n" % base64string)
    s.send("User-Agent: curl/7.26.0\r\n")
    s.send("Host: %s\r\n" % host)
    s.send("Accept: */*\r\n")
    s.send("Content-Type: audio/basic\r\n")
    s.send("Content-Length:  999999\r\n")
    s.send("Connection: Keep-Alive\r\n")
    s.send("Cache-Control: no-cache\r\n")
    # s.send("Expect: 100-continue\r\n")
    s.send("\r\n")
    time.sleep(0.1)
    s.send(data)
    #print s.recv(4096)
    s.close()

class AgoAxisAudioTransmit(agoclient.AgoApp):
    def message_handler(self, internalid, content):
        if "command" in content:
            if content["command"] == "playfile":
                if 'file' in content:
                    data = open(content['file'], 'rb').read()
                    send_audio(internalid,self.port,self.path,self.username,self.password,data)
    def setup_app(self):
        self.connection.add_handler(self.message_handler)
        try:
            devices = map(str, self.get_config_option("devices", "192.168.80.15").split(','))
            for device in devices:
                self.connection.add_device(device, "audioannounce")
        except:
            self.log.error('cannot parse device config')
        self.username = self.get_config_option("username","root")
        self.password = self.get_config_option("password","")
        self.path = '/axis-cgi/audio/transmit.cgi'
        self.port = 80
                
    def cleanup_app(self):
        pass

if __name__ == "__main__":
    AgoAxisAudioTransmit().main()

