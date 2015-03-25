#!/usr/bin/python
"""Agocontrol MQTT device."""

# agocontrol MQTT device
#
# copyright (c) 2015 Harald Klein <hari+ago@vt100.at>
#

# to use the client library we have to import it

import agoclient
try:
    import paho.mqtt.client as mqtt
except ImportError:
    import agomqttpahoclient as mqtt

import threading
import time

class AgoMQTT(agoclient.AgoApp):
    """AgoControl MQTT device"""

    def message_handler(self, internalid, content):
        """The messagehandler."""

    def start_paho_thread(self):
        BACKGROUND = MQTTThread(self)
        BACKGROUND.setDaemon(True)
        BACKGROUND.start()

    def setup_app(self):
        # specify our message handler method
        self.connection.add_handler(self.message_handler)

        self.mqtt_broker = self.get_config_option("broker", "127.0.0.1")
        self.mqtt_port = self.get_config_option("port", "1883")

        self.devicelist = []
        self.start_paho_thread()

    def announce_device(self, internalid, devicetype):
        if not internalid in self.devicelist:
            self.devicelist.append(internalid)
            self.connection.add_device(internalid, devicetype)

    def app_cleanup(self):
        pass

class MQTTThread(threading.Thread):
    """MQTTThread"""
    def __init__(self, app):
        threading.Thread.__init__(self)
        self.app = app
        self.connected = False

    def on_connect(self, client, obj, flags, rc):
        self.app.log.info("Connected to MQTT broker: %s", str(rc))
        self.app.log.info("Subscribing to: %s",self.app.get_config_option("topic", "sensors/#"))
        self.client.subscribe(self.app.get_config_option("topic", "sensors/#"))

    def on_subscribe(self, client, obj, mid, granted_qos):
        self.app.log.info("Subscribed to topic: %s", str(mid))

    def on_message(self, client, obj, msg):
        self.app.log.info("Received MQTT message on topic %s: %s", str(msg.topic),str(msg.payload))
        topic = str(msg.topic)
        if topic.find("temperature") != -1:
            self.app.announce_device(str(msg.topic), "temperaturesensor")
            self.app.connection.emit_event(str(msg.topic), "event.environment.temperaturechanged", float(msg.payload), "degC")
        if topic.find("humidity") != -1:
            self.app.announce_device(str(msg.topic), "humiditysensor")
            self.app.connection.emit_event(str(msg.topic), "event.environment.humiditychanged", float(msg.payload), "%")
        if topic.find("pressure") != -1:
            self.app.announce_device(str(msg.topic), "barometersensor")
            self.app.connection.emit_event(str(msg.topic), "event.environment.pressurechanged", float(msg.payload), "mBar")

    def on_log(self, client, obj, level, string):
        self.app.log.debug("Paho log: %s %s", str(level), string)

    def on_disconnect(self, client, obj, rc):
        self.app.log.error("Disconected from MQTT broker: %s", str(rc))
        #self.app.signal_exit()
        self.connected = False

    def run(self):
        self.client = mqtt.Client()
        self.client.on_connect = self.on_connect
        self.client.on_disconnect = self.on_disconnect
        self.client.on_message = self.on_message
        self.client.on_log = self.on_log
        while not self.app.is_exit_signaled():
            while not self.connected:
                try:
                    self.client.connect(self.app.mqtt_broker, 1883, 60)
                    self.connected=True
                except:
                    self.app.log.error("Cannot connect to MQTT broker: %s", self.app.mqtt_broker)
                    time.sleep(3)
            rc=0
            self.app.log.debug("entering MQTT client loop")
            while rc == 0:
                rc = self.client.loop()
        self.app.log.error("MQTT Thread stopped")

if __name__ == "__main__":
    AgoMQTT().main()

