#!/usr/bin/python
"""Agocontrol MQTT device."""

# agocontrol MQTT device
#
# copyright (c) 2015 Harald Klein <hari+ago@vt100.at>
#

# to use the client library we have to import it

import agoclient
import paho.mqtt.client as mqtt

import threading
import time

class AgoMQTT(agoclient.AgoApp):
    """AgoControl MQTT device"""

    def message_handler(self, internalid, content):
        """The messagehandler."""

    def setup_app(self):
        # specify our message handler method
        self.connection.add_handler(self.message_handler)

        self.mqtt_broker = self.get_config_option("broker", "127.0.0.1")
        self.mqtt_port = self.get_config_option("port", "1883")


        BACKGROUND = MQTTThread(self)
        BACKGROUND.setDaemon(True)
        BACKGROUND.start()

    def app_cleanup(self):
        pass

class MQTTThread(threading.Thread):
    """MQTTThread"""
    def __init__(self, app):
        threading.Thread.__init__(self)
        self.app = app

    def on_connect(self, client, userdata, rc, rest):
        self.app.log.info("Connected to MQTT broker: %s", str(rc))
        self.client.subscribe("sensors/#")

    def on_message(self, client, userdata, msg):
        self.app.log.info("Received MQTT message on topic %s: %s", str(msg.topic),str(msg.payload))
        topic = str(msg.topic)
        if topic.find("temperature") != -1:
            self.app.connection.add_device(str(msg.topic), "temperaturesensor")
            self.app.connection.emit_event(str(msg.topic), "event.environment.temperaturechanged", float(msg.payload), "degC")
        if topic.find("humidity") != -1:
            self.app.connection.add_device(str(msg.topic), "humiditysensor")
            self.app.connection.emit_event(str(msg.topic), "event.environment.humiditychanged", float(msg.payload), "%")
        if topic.find("pressure") != -1:
            self.app.connection.add_device(str(msg.topic), "pressuresensor")
            self.app.connection.emit_event(str(msg.topic), "event.environment.pressurechanged", float(msg.payload), "mbar")

    def run(self):
        self.client = mqtt.Client()
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message
        self.client.connect(self.app.mqtt_broker, 1883, 60)
        rc=0
        while rc == 0:
            rc = self.client.loop()

if __name__ == "__main__":
    AgoMQTT().main()

