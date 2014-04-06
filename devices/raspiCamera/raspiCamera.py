#! /usr/bin/python

# raspiCamera device
#
#  enabling camera suport on raspi http://www.raspberrypi.org/camera 
#
#  apt-get install python-picamera
#

import agoclient
import urllib2
import base64
import picamera
import io
import time


client = agoclient.AgoConnection("raspiCamera")

def messageHandler(internalid, content):
    result = {}
    result["result"] = -1;
    if "command" in content:
        if content['command'] == 'getvideoframe':
            print "raspiCamera getting video frame"
            stream = io.BytesIO()
            with picamera.PiCamera() as camera:
                camera.resolution = (200, 140)
                camera.quality = (20)
                camera.start_preview()
                time.sleep(0.1)
                camera.capture(stream, format='jpeg')
            result["image"] = base64.b64encode(stream.getvalue())
            result["result"] = 0;
            print "result[""image""] sending"

    return result

client.add_handler(messageHandler)
#devicelist=agoclient.get_config_option("raspiCamera", "devices", "")

client.add_device("raspiCamera", "camera")

client.run()