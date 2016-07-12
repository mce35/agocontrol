#!/usr/bin/python
"""ago control MQTT device"""

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
    """ago control MQTT device"""

    def start_paho_thread(self):
        BACKGROUND = MQTTThread(self)
        BACKGROUND.setDaemon(True)
        BACKGROUND.start()

    def setup_app(self):
        self.mqtt_broker = self.get_config_option("broker", "127.0.0.1")
        self.mqtt_port = self.get_config_option("port", "1883")
        self.topic = self.get_config_option("topic", "sensors/#")

        self.devicelist = []
        self.start_paho_thread()

    def announce_device(self, internalid, devicetype):
        if not internalid in self.devicelist:
            self.devicelist.append(internalid)
            self.connection.add_device(internalid, devicetype)

class MQTTThread(threading.Thread):
    """MQTTThread"""
    def __init__(self, app):
        threading.Thread.__init__(self)
        self.app = app
        self.connected = False

    def on_connect(self, client, obj, flags, rc):
        self.app.log.info("Connected to MQTT broker %s:%s (rc=%d)", self.app.mqtt_broker, self.app.mqtt_port, rc)
        self.app.log.info("Subscribing to: %s", self.app.topic)
        self.client.subscribe(self.app.topic)

    def on_subscribe(self, client, obj, mid, granted_qos):
        self.app.log.debug("Subscribed to topic with MID: %d", mid)

    def on_message(self, client, obj, msg):
        self.app.log.info("Received MQTT message on topic %s: %s", msg.topic, msg.payload)
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
        self.app.log.error("Disconnected from MQTT broker (rc=%d)", rc)
        # self.app.signal_exit()
        self.connected = False

    def run(self):
        self.client = mqtt.Client()
        
        self.client.on_connect = self.on_connect
        self.client.on_disconnect = self.on_disconnect
        self.client.on_subscribe = self.on_subscribe
        self.client.on_message = self.on_message
        self.client.on_log = self.on_log
        
        while not self.app.is_exit_signaled():
            while not self.connected:
                try:
                    rc = self.client.connect(self.app.mqtt_broker, self.app.mqtt_port, 60)
                    self.connected = True
                except:
                    self.app.log.error("Cannot connect to MQTT broker: %s:%s (rc=%d)", self.app.mqtt_broker, self.app.mqtt_port, rc)
                    time.sleep(3)

            rc = 0
            self.app.log.debug("Entering MQTT client loop")
            while rc == 0:
                rc = self.client.loop()
        self.app.log.error("MQTT Thread stopped")

if __name__ == "__main__":
    AgoMQTT().main()
