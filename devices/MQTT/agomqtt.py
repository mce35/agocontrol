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

import json
import threading
import time

class AgoMQTT(agoclient.AgoApp):
    """ago control MQTT device"""

    def setup_app(self):
        self.mqtt_broker = self.get_config_option("broker", "127.0.0.1")
        self.mqtt_port = self.get_config_option("port", "1883")
        self.topic = self.get_config_option("topic", "sensors/#")
        self.mapping = self.get_config_option("mapping")

        # load existing device list, add and mark devices as stale
        self.devicelist = {}
        try:
            with open(agoclient.config.get_localstate_path() + "/mqtt_devices.json") as store:
                self.devicelist = json.load(store)
            for internalid in self.devicelist.keys():
                self.connection.add_device(internalid, self.devicelist[internalid])
                self.connection.suspend_device(internalid)
        except ValueError:
            self.log.error("Failed to decode MQTT state file - will not use it")
            self.devicelist = {}
        except IOError:
            pass

        self.worker = MQTTThread(self)
        self.worker.start()

    def cleanup_app(self):
        self.log.info("Saving MQTT device states")
        try:
            with open(agoclient.config.get_localstate_path() + "/mqtt_devices.json", "w") as store:
                json.dump(self.devicelist, store)
        except IOError:
            self.log.error("Failed to write MQTT device list")
        self.worker.join()

    def announce_device(self, internalid, devicetype):
        if not internalid in self.devicelist.keys():
            self.devicelist[internalid] = devicetype
            self.connection.add_device(internalid, devicetype)

class MQTTThread(threading.Thread):
    """MQTTThread"""
    def __init__(self, app):
        threading.Thread.__init__(self)
        self.app = app
        self.connected = False
        self.mapping = {
            "temperature":["temperaturesensor", "event.environment.temperaturechanged", "degC"],
            "humidity":["humiditysensor", "event.environment.humiditychanged", "%"],
            "pressure":["barometersensor", "event.environment.pressurechanged", "mBar"],
            "power":["powermeter", "event.environment.powerchanged", "W"],
            "flow":["flowmeter", "event.environment.flowchanged", "m^3"],
            "energy":["energymeter", "event.environment.energychanged", "kWh"]
        }
        
        if self.app.mapping:
            for mapping in self.app.mapping.split(";"):
                parts = mapping.split(":")
                if len(parts) != 4:
                    self.app.log.error("Wrong number of parameters for mapping - skipping entry '%s'", mapping)
                    continue
                
                # do not permit to overwrite existing mappings (for backwards compatibility)
                if not parts[0] in self.mapping:
                    self.app.log.info("Adding new mapping for: %s (%s)", parts[0], ",".join(parts[1:]))
                    self.mapping[parts[0]] = parts[1:]
                else:
                    self.app.log.warning("Skipping existing mapping for: %s", parts[0])

    def on_connect(self, client, obj, flags, rc):
        self.app.log.info("Connected to MQTT broker %s:%s (rc=%d, %s)", self.app.mqtt_broker, self.app.mqtt_port, rc, mqtt.error_string(rc))
        self.app.log.info("Subscribing to: %s", self.app.topic)
        self.client.subscribe(self.app.topic)

    def on_subscribe(self, client, obj, mid, granted_qos):
        self.app.log.debug("Subscribed to topic with MID: %d", mid)

    def on_message(self, client, obj, msg):
        self.app.log.info("Received MQTT message on topic %s: %s", msg.topic, msg.payload)
        for key in self.mapping.keys():
            if msg.topic.find(key) != -1:
                self.app.log.debug("Matched key '%s' for topic '%s' - emitting '%s'", key, msg.topic, self.mapping[key][1])
                self.app.announce_device(msg.topic, self.mapping[key][0])
                self.app.connection.resume_device(msg.topic)
                self.app.connection.emit_event(msg.topic, self.mapping[key][1], float(msg.payload), self.mapping[key][2])
                break

    def on_log(self, client, obj, level, string):
        self.app.log.debug("Paho log: %s %s", str(level), string)

    def on_disconnect(self, client, obj, rc):
        self.app.log.info("Disconnected from MQTT broker (rc=%d, %s)", rc, mqtt.error_string(rc))
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
                    self.client.connect(self.app.mqtt_broker, self.app.mqtt_port, 60)
                    self.connected = True
                except:
                    self.app.log.error("Cannot connect to MQTT broker: %s:%s (rc=%d, %s)", self.app.mqtt_broker, self.app.mqtt_port, rc, mqtt.error_string(rc))
                    time.sleep(3)

            rc = self.client.loop()
            if rc != mqtt.MQTT_ERR_SUCCESS:
                self.app.log.warning("MQTT loop returned code %d (%s)", rc, mqtt.error_string(rc))

        if self.connected:
            self.client.disconnect()

        self.app.log.debug("MQTT Thread stopped")

if __name__ == "__main__":
    AgoMQTT().main()
