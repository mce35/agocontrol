#! /usr/bin/env python

import random
import threading
import time
import logging

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

    def app_cmd_line_options(self, parser):
        """App-specific command line options"""
        parser.add_argument('-i', '--interval', type=float,
                default = 5,
                help="How many seconds (int/float) to wait between sent messages")


    def setup_app(self):
        self.connection.add_handler(self.messageHandler)

        self.connection.add_device("123", "dimmer")
        self.connection.add_device("124", "switch")
        self.connection.add_device("125", "binarysensor")
        self.connection.add_device("126", "multilevelsensor")
        self.connection.add_device("127", "pushbutton")

        self.log.info("Starting test thread")
        self.background = TestEvent(self, self.args.interval)
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
    def __init__(self, app, interval):
        threading.Thread.__init__(self)
        self.app = app
        self.interval = interval

    def run(self):
        level = 0
        counter = 0
        log = logging.getLogger('SimulatorTestThread')
        while not self.app.is_exit_signaled():
            counter = counter + 1
            if counter > 3:
                counter = 0
                temp = random.randint(50,300) / 10 + random.randint(0,90)/100.0
                hum = random.randint(20, 75) + random.randint(0,90)/100.0
                log.debug("Sending enviromnet changes on sensor 126 (%.2f dgr C, %.2f %% humidity)",
                        temp, hum)
                self.connection.emit_event("126", "event.environment.temperaturechanged", temp, "degC");
                self.connection.emit_event("126", "event.environment.humiditychanged", hum, "percent");

            log.debug("Sending sensortriggered for internal-ID 125, level %d", level)
            self.connection.emit_event("125", "event.security.sensortriggered", level, "")

            if (level == 0):
                level = 255
            else:
                level = 0

            log.trace("Next command in %f seconds...", self.interval)
            time.sleep(self.interval)

if __name__ == "__main__":
    AgoSimulator().main()
