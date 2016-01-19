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
                camera.resolution = (1024, 768)
                camera.quality = (90)
                camera.start_preview()
                time.sleep(0.1)
                camera.capture(stream, format='jpeg')
                frame = stream.getvalue()

    return client.response_success({'image':buffer(base64.b64encode(frame))})

client.add_handler(messageHandler)

client.add_device("raspiCamera", "camera")

client.run()