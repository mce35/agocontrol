#!/usr/bin/python
AGO_TELLSTICK_VERSION = '0.0.9'
############################################
#
# Tellstick Duo support for ago control
#
__author__     = "Joakim Lindbom"
__copyright__  = "Copyright 2013, 2014, Joakim Lindbom"
__date__       = "2013-12-24"
__credits__    = ["Joakim Lindbom", "The ago control team"]
__license__    = "GPL Public License Version 3"
__maintainer__ = "Joakim Lindbom"
__email__      = 'Joakim.Lindbom@gmail.com'
__status__     = "Experimental"
__version__    = AGO_TELLSTICK_VERSION
############################################

import time
from qpid.messaging import Message
import agoclient

class AgoTellstick(agoclient.AgoApp):
    def message_handler(self, internalid, content):
        if "command" in content:
            # On
            if content["command"] == "on":
                resCode = self.tellstick.turnOn(internalid)
                if resCode != 'success':   # != 0:
                    self.log.trace("rescode = %s", resCode)
                    #res = self.tellstick.getErrorString(resCode)
                    self.log.error("Failed to turn on device, res=" + resCode)
                else:
                    self.connection.emit_event(internalid, "event.device.statechanged", 255, "")

                self.log.debug("Turning on device: %s res=%s", internalid, resCode)

            # Allon - TODO: will require changes in schema.yaml + somewhere else too
            if content["command"] == "allon":
                self.log.debug("Command 'allon' received (ignored)")

            # Off
            if content["command"] == "off":
                resCode = self.tellstick.turnOff(internalid)
                if resCode != 'success':  # 0:
                    #res = self.tellstick.getErrorString(resCode)
                    self.log.error("Failed to turn off device, res=" + resCode)
                else:
                    #res = 'Success'
                    self.connection.emit_event(internalid, "event.device.statechanged", 0, "")

                self.log.debug("Turning off device: %s res=%s", internalid, resCode)

            # Setlevel for dimmer
            if content["command"] == "setlevel":
                resCode = self.tellstick.dim(internalid, int(255 * int(content["level"]))/100)  # Different scales: aGo use 0-100, Tellstick use 0-255
                if resCode != 'success':  # 0:
                    self.log.error( "Failed dimming device, res=%s", resCode)
                else:
                    #res = 'Success'
                    self.connection.emit_event(internalid, "event.device.statechanged", content["level"], "")

                self.log.debug("Dimming device=%s res=%s level=%s", internalid, resCode, str(content["level"]))

    #Event handlers for device and sensor events
    def agoDeviceEvent(self, deviceId, method, data, callbackId):
        self.log.trace("agoDeviceEvent devId=%s method=%s data=%s", str(deviceId), method, data)

        received = self.event_received.get(deviceId)
        if received == None:
            received = self.event_received[deviceId] = 0
            self.lasttime[deviceId] = time.time()

        self.log.trace("time - lasttime = %d", time.time() - self.lasttime[deviceId])

        if received == 1:
            delay = self.dev_delay.get(deviceId)
            if delay == None:
                delay = self.general_delay

            if (time.time() - self.lasttime[deviceId]) > delay:
                #No echo, stop cancelling events
                received = self.event_received[deviceId] = 0
            else:
                self.log.debug("Echo cancelled")

        if received == 0:
            #if debug:
                #info('%d: DeviceEvent Device: %d - %s  method: %s, data: %s' %(time.time(), deviceId, self.tellstick.getName(deviceId), self.tellstick.methodsReadable.get(method, 'Unknown'), data))

            received = self.event_received[deviceId] = 1
            self.lasttime[deviceId] = time.time()

            #print "method=" + str(method)
            if (method == self.tellstick.TELLSTICK_TURNON):
                self.connection.emit_event(deviceId, "event.device.statechanged", 255, "")
                self.log.debug("emit_event statechanged %s ON 255", deviceId)

            if (method == self.tellstick.TELLSTICK_TURNOFF):
                self.connection.emit_event(deviceId, "event.device.statechanged", 0, "")
                self.log.debug("emit_event statechanged %s OFF 0", deviceId)
            # if (method == self.tellstick.TELLSTICK_DIM): #Hmm, not sure if this can happen?!?
            #     level = int(100 * int(data))/255
            #     if int(data) > 0 and int(data) < 255:
            #         level = level +1
            #     self.connection.emit_event(deviceId, "event.device.statechanged", str(level), "")
            #     if debug:
            #         self.log.info("emit_event statechanged DIM " + str(level))


    #def agoDeviceChangeEvent(self, deviceId, changeEvent, changeType, callbackId):
    #    if debug:
    #        print ('%d: DeviceChangeEvent Device: %d - %s' %(time.time(), deviceId, self.tellstick.getName(deviceId)))
    #        print ('  changeEvent: %d' %( changeEvent ))
    #        print ('  changeType: %d' %( changeType ))


    #def agoRawDeviceEvent(self, data, controllerId, callbackId):
    #    if debug:
    #        print ('%d: RawDeviceEvent: %s - controllerId: %s' %(time.time(), data, controllerId))
    #    if 'class:command' in str(data):
    #        protocol = data.split(";")[1].split(":")[1]
    #        print ("protocol=" + protocol)
    #
    #        if "arctech" or "waveman" in protocol:
    #            house = data.split(";")[3].split(":")[1]
    #            unit = data.split(";")[4].split(":")[1]
    #            devID = "SW" + house+unit
    #            print ("devid=", devID)
    #            method = data.split(";")[5].split(":")[1]
    #            print ("method=" + method)
    #
    #        if "sartano" in protocol:
    #            code = data.split(";")[3].split(":")[1]
    #            devID = "SW" + code
    #            print ("devid=", devID)
    #            method = data.split(";")[4].split(":")[1]
    #            print ("method=" + method)

    def emitTempChanged(self, devId, temp):
        tempC = temp
        if self.TempUnits == 'F':
            tempF = 9.0/5.0 * tempC + 32.0
            if tempF != float(self.sensors[devId]["lastTemp"]):
                self.sensors[devId]["lastTemp"] = tempF
                self.connection.emit_event(devId, "event.environment.temperaturechanged", str(tempF), "degF")
        else:
            #print "devId=" + str(devId)
            if tempC != float(self.sensors[devId]["lastTemp"]):
                self.sensors[devId]["lastTemp"] = tempC
                self.connection.emit_event(devId, "event.environment.temperaturechanged", str(tempC), "degC")

    def emitHumidityChanged(self, devId, humidity):
        #print "hum=" + str(humidity)
        #print "last=" + str(sensors[devId]["lastHumidity"])

        if humidity != float(self.sensors[devId]["lastHumidity"]):
            self.sensors[devId]["lastHumidity"] = humidity
            self.connection.emit_event(devId, "event.environment.humiditychanged", str(humidity), "%")

    def listNewSensors(self):
        self.sensors = self.tellstick.listSensors()
        for id, value in self.sensors.iteritems():
            if value["new"] != True:
                continue

            value["new"] = False
            devId = str(id)
            if value["isMultiLevel"]:
                self.connection.add_device (devId, "multilevelsensor")
                if value["isTempSensor"]:
                    self.emitTempChanged(devId, float(value["temp"]))
                if value["isHumiditySensor"]:
                    self.emitHumidityChanged(devId, float(value["humidity"]))

            else:
                if value["isTempSensor"]:
                    self.connection.add_device(devId, "temperaturesensor")
                    self.emitTempChanged(devId, float(value["temp"]))

                if value["isHumiditySensor"]:
                    self.connection.add_device (devId, "multilevelsensor")
                    self.emitHumidityChanged(devId, float(value["humidity"]))

    def agoSensorEvent(self, protocol, model, id, dataType, value, timestamp, callbackId):
        self.log.trace("SensorEvent called for %s", str(id))
        #print '%d: SensorEvent' %(time.time())
        #print '  protocol: %s' %(protocol)
        #print '  model: %s' %(model)
        #print '  id: %s' %(id)
        #print '  dataType: %d' %(dataType)
        #print '  value: %s' %(value)
        #print '  timestamp: %d' %(timestamp)

        self.listNewSensors()
        devId = str(id)
        if "temp" in model and dataType & self.tellstick.TELLSTICK_TEMPERATURE == self.tellstick.TELLSTICK_TEMPERATURE:
            self.emitTempChanged(devId, float(value))
            # tempC = value
            # if TempUnits == 'F':
            #     tempF = 9.0/5.0 * tempC + 32.0
            #     self.connection.emit_event(str(devId), "event.environment.temperaturechanged", tempF, "degF")
            # else:
            #     self.connection.emit_event(str(devId), "event.environment.temperaturechanged", tempC, "degC")
        if "humidity" in model and dataType & self.tellstick.TELLSTICK_HUMIDITY == self.tellstick.TELLSTICK_HUMIDITY:
            self.emitHumidityChanged(devId, float(value))
            #self.connection.emit_event(str(devId), "event.environment.humiditychanged", float(value), "%")

    def setup_app(self):
        self.timers = {} # List of timers
        self.event_received = {}
        self.lasttime = {}
        self.dev_delay = {}
        self.sensors = {}

        #device = (agoclient.get_config_option("tellstick", "device", "/dev/usbxxxx")

        try:
            self.general_delay = float(self.get_config_option('Delay', 500, 'EventDevices'))/1000
        except:
            self.general_delay = 0.5

        try:
            self.SensorPollDelay = float(self.get_config_option('SensorPollDelay', 300))/1000
        except:
            self.SensorPollDelay = 300.0  # 5 minutes

        units = self.get_config_option("units", "SI", section="units")
        self.TempUnits = "C"
        if units.lower() == "us":
            self.TempUnits = "F"

        stickVersion = self.get_config_option('StickVersion', 'Tellstick Duo')

        if "Net" in stickVersion:
            # Postpone tellsticknet loading, has extra dependencies which
            # we can do without if we've only got a Duo
            from tellsticknet import tellsticknet
            self.tellstick = tellsticknet()
        else:
            from  tellstickduo import  tellstickduo
            self.tellstick = tellstickduo()

        self.tellstick.init(self.SensorPollDelay, self.TempUnits)

        self.connection.add_handler(self.message_handler)

        # Get inventory, required to set names on new devices
        inventory = self.connection.get_inventory().content
        agoController = None
        for uuid in inventory['devices'].keys():
            d = inventory['devices'][uuid]
            if d['devicetype'] == 'agocontroller':
                agoController = uuid
                break

        if agoController == None:
            self.log.warning("No agocontroller found, cannot set device names")
        else:
            self.log.debug("agoController found: %s", agoController)

        def setNameIfNecessary(deviceUUID, name):
            dev = inventory['devices'].get(deviceUUID)
            if (dev == None or dev['name'] == '') and name != '':
                content = {}
                content["command"] = "setdevicename"
                content["uuid"] = agoController
                content["device"] = deviceUUID
                content["name"] = name

                message = Message(content=content)
                self.connection.send_message (None, content)
                self.log.debug("'setdevicename' message sent for %s, name=", deviceUUID, name)

        # Get devices from Telldus, announce to Ago Control
        self.log.info("Getting switches and dimmers")
        switches = self.tellstick.listSwitches()
        for devId, dev in switches.iteritems():
        #    devId = self.tellstick.getDeviceId(i)
        #    model = self.tellstick.getModel(devId)
        #    name = self.tellstick.getName(devId)
            model = dev["model"]
            name = dev["name"]

            self.log.info("devId=%s name=%s model=%s", devId, name, model)
            #+ " method=" + self.tellstick.methods(devId))

            #found = False
            deviceUUID = ""

            #if ("selflearning-switch" in model or model == "switch"):
            if dev["isDimmer"]:
                self.connection.add_device(devId, "dimmer")
            else:
                self.connection.add_device(devId, "switch")

            deviceUUID = self.connection.internal_id_to_uuid (devId)
            #info ("deviceUUID=" + deviceUUID + " name=" + self.tellstick.getName(devId))
            #info("Switch Name=" + dev.name + " protocol=" + dev.protocol + " model=" + dev.model)
            #if ("selflearning-dimmer" in model or model == "dimmer"):

            # Check if device already exist, if not - send its name from the tellstick config file
            setNameIfNecessary(deviceUUID, name)

        self.log.info("Getting remotes & motion sensors")
        remotes = self.tellstick.listRemotes()
        for devId, dev in remotes.iteritems():
            model = dev["model"]
            name = dev["name"]
            self.log.info("devId=%s name=%s model=%s", devId, name, model)

            if not "codeswitch" in model:
                self.connection.add_device(devId, "binarysensor")
                deviceUUID = self.connection.internal_id_to_uuid (devId)

                #found = True
                #"devId=" + str(devId) + " model " + model
                try:
                    self.dev_delay[devId] = float(self.get_config_option(str(devId) + '_Delay', 5000, 'EventDevices')) / 1000
                except:
                    self.dev_delay[devId] = self.general_delay

                # Check if device already exist, if not - send its name from the tellstick config file
                setNameIfNecessary(deviceUUID, name)

        self.log.info("Getting temp and humidity sensors")
        self.listNewSensors()

        #
        #  Register event handlers
        #
        cbId = []

        cbId.append(self.tellstick.registerDeviceEvent(self.agoDeviceEvent))
        self.log.debug('Register device event returned: %s', str(cbId[-1]))

        #cbId.append(self.tellstick.registerDeviceChangedEvent(self.agoDeviceChangeEvent))
        #info ('Register device changed event returned:' + str(cbId[-1]))

        #cbId.append(self.tellstick.registerRawDeviceEvent(self.agoRawDeviceEvent))
        #info ('Register raw device event returned:' + str(cbId[-1]))

        cbId.append(self.tellstick.registerSensorEvent(self.agoSensorEvent))
        self.log.debug('Register sensor event returned: %s', str(cbId[-1]))

    def app_cleanup(self):
        if self.tellstick:
            self.tellstick.close()

if __name__ == "__main__":
    AgoTellstick().main()

