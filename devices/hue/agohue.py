#!/usr/bin/env python

# ago hue device
#
# copyright (c) 2016 Harald Klein <hari+ago@vt100.at>
#

import agoclient
from os import path
from qhue import Bridge, QhueException, create_new_username
from rgb_cie import ColorHelper, Converter

import threading
import time

CRED_FILE_PATH = "qhue_username.txt"

class AgoHue(agoclient.AgoApp):
    def message_handler(self, internalid, content):
        """The messagehandler."""
        if "command" in content:
            if content["command"] == "on":
                self.log.debug("switching on: %s", internalid)
                try:
                    self.bridge.lights[internalid].state(bri=255)
                    self.connection.emit_event(internalid, "event.device.statechanged", "255", "")
                except:
                    self.log.info("Cannot change brightness for lamp %s", internalid)
            elif content["command"] == "off":
                self.log.debug("switching off: %s", internalid)
                try:
                    self.bridge.lights[internalid].state(bri=0)
                    self.connection.emit_event(internalid, "event.device.statechanged", "0", "")
                except:
                    self.log.info("Cannot change brightness for lamp %s", internalid)
            elif content["command"] == "setlevel":
                self.log.debug("setting level for: %s", internalid)
                try:
                    self.bridge.lights[internalid].state(bri=int(content["level"]) * 255 / 100)
                    self.connection.emit_event(internalid, "event.device.statechanged", "0", "")
                except:
                    self.log.info("Cannot change brightness for lamp %s", internalid)
            elif content["command"] == "setcolor":
                self.log.debug("setting color for: %s", internalid)
                red = int(content["red"]);
                green = int(content["green"]);
                blue = int(content["blue"]);
                #red = int(content["red"]) / 255;
                #green = int(content["green"]) / 255;
                #blue = int(content["blue"]) / 255;
                # X = red * 0.649926 + green * 0.103455 + blue * 0.197109;
                # Y = red * 0.234327 + green * 0.743075 + blue * 0.022598;
                # Z = red * 0.0000000 + green * 0.053077 + blue * 1.035763;
                # finalX = X / (X + Y + Z);
                #finalY = Y / (X + Y + Z);
                #self.log.debug("R:{} G:{} B:{} X:{} Y:{}".format(red,green,blue,finalX,finalY))
                #self.bridge.lights[internalid].state(xy=[finalX,finalY])
                self.bridge.lights[internalid].state(xy=self.converter.rgbToCIE1931(red, green, blue))

    def setup_app(self):
        self.connection.add_handler(self.message_handler)
        bridgeip = self.get_config_option("ip", "192.168.80.109")
        self.log.info("Configuration parameter 'ip' was set to %s", bridgeip)



        # check for a credential file
        if not path.exists(CRED_FILE_PATH):

            while True:
                try:
                    username = create_new_username(bridgeip)
                    break
                except QhueException as err:
                    print "Error occurred while creating a new username: {}".format(err)

            # store the username in a credential file
            with open(CRED_FILE_PATH, "w") as cred_file:
                cred_file.write(username)

        else:
            with open(CRED_FILE_PATH, "r") as cred_file:
                username = cred_file.read()

        # create the bridge resource, passing the captured username
        self.bridge = Bridge(bridgeip, username)
        self.converter = Converter()

        # create a lights resource
        lights = self.bridge.lights

        # query the API and print the results
        for light in lights():
            print self.bridge.lights[light]()
            self.connection.add_device(light, "dimmerrgb")

        # print lights()


if __name__ == "__main__":
    AgoHue().main()

