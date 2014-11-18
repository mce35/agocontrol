#! /usr/bin/env python

import random
import syslog
import threading
import time

import agoclient

class AgoSimulator(agoclient.AgoApp):
    def messageHandler(internalid, content):
        if "command" in content:
            if content["command"] == "on":
                print "switching on: " + internalid
                self.connection.emit_event(internalid, "event.device.statechanged", "255", "")
            if content["command"] == "off":
                print "switching off: " + internalid
                self.connection.emit_event(internalid, "event.device.statechanged", "0", "")
            if content["command"] == "push":
                print "push button: " + internalid
            if content['command'] == 'setlevel':
                if 'level' in content:
                    print "device level changed", content["level"]
                    self.connection.emit_event(internalid, "event.device.statechanged", content["level"], "")


    def setup_app(self):
        self.connection.add_handler(self.messageHandler)

        self.connection.add_device("123", "dimmer")
        self.connection.add_device("124", "switch")
        self.connection.add_device("125", "binarysensor")
        self.connection.add_device("126", "multilevelsensor")
        self.connection.add_device("127", "pushbutton")

        self.log.info("Starting test thread")
        self.background = TestEvent()
        self.background.connection = self.connection
        self.background.setDaemon(True)
        self.background.start()

    def cleanup_app(self):
        # Unfortunately, there is no good way to wakeup the python sleep().
        # In this particular case, we can just let it die. Since it's a daemon thread,
        # it will.

        #self.background.join()
        pass


class TestEvent(threading.Thread):
    def __init__(self,):
        threading.Thread.__init__(self)    
        self.connection = None

    def run(self):
        level = 0
        counter = 0
        while True:
            counter = counter + 1
            if counter > 3:
                counter = 0
                temp = random.randint(50,300) / 10
                self.connection.emit_event("126", "event.environment.temperaturechanged", temp, "degC");
                self.connection.emit_event("126", "event.environment.humiditychanged", random.randint(20, 75), "percent");

            self.connection.emit_event("125", "event.security.sensortriggered", level, "")

            if (level == 0):
                level = 255
            else:
                level = 0

            time.sleep(5)

if __name__ == "__main__":
    AgoSimulator().main()
