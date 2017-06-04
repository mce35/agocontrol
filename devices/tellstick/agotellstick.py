#!/usr/bin/python
AGO_TELLSTICK_VERSION = '0.0.91'
############################################
#
# Tellstick support for ago control
#
__author__ = 'Joakim Lindbom'
__copyright__ = 'Copyright 2013-2016, Joakim Lindbom'
__date__ = '2013-12-24'
__credits__ = ['Joakim Lindbom', 'The ago control team']
__license__ = 'GPL Public License Version 3'
__maintainer__ = 'Joakim Lindbom'
__email__ = 'Joakim.Lindbom@gmail.com'
__status__ = 'Experimental'
__version__ = AGO_TELLSTICK_VERSION
############################################

# TODO: Add filter to sensor reading. Example of bad data:


import time
from qpid.messaging import Message
import agoclient


class AgoTellstick(agoclient.AgoApp):
    def message_handler(self, internalid, content):
        if "command" in content:
            # On
            if content["command"] == "on":
                resCode = self.tellstick.turnOn(internalid)
                if resCode != 'success':  # != 0:
                    self.log.trace("rescode = %s", resCode)
                    # res = self.tellstick.getErrorString(resCode)
                    self.log.error("Failed to turn on device, res=%s", resCode)
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
                    # res = self.tellstick.getErrorString(resCode)
                    self.log.error("Failed to turn off device, res=%s", resCode)
                else:
                    # res = 'Success'
                    self.connection.emit_event(internalid, "event.device.statechanged", 0, "")

                self.log.debug("Turning off device: %s res=%s", internalid, resCode)

            # Setlevel for dimmer
            if content["command"] == "setlevel":
                resCode = self.tellstick.dim(internalid, int(
                    255 * int(content["level"])) / 100)  # Different scales: aGo use 0-100, Tellstick use 0-255
                if resCode != 'success':  # 0:
                    self.log.error("Failed dimming device, res=%s", resCode)
                else:
                    # res = 'Success'
                    self.connection.emit_event(internalid, "event.device.statechanged", content["level"], "")

                self.log.debug("Dimming device=%s res=%s level=%s", internalid, resCode, str(content["level"]))

    # Event handlers for device and sensor events
    # This method is a call-back, triggered when there is a device event
    def agoDeviceEvent(self, deviceId, method, data, callbackId):
        self.log.trace("agoDeviceEvent devId=%s method=%s data=%s", str(deviceId), method, data)

        received = self.event_received.get(deviceId)
        if received is None:
            received = self.event_received[deviceId] = 0
            self.lasttime[deviceId] = time.time()

        self.log.trace("time - lasttime = %d", time.time() - self.lasttime[deviceId])

        if received == 1:
            delay = self.dev_delay.get(deviceId)
            if delay is None:
                delay = self.general_delay

            if (time.time() - self.lasttime[deviceId]) > delay:
                # No echo, stop cancelling events
                received = self.event_received[deviceId] = 0
            else:
                self.log.trace("Echo cancelled")

        if received == 0:
            # if debug:
            # info('%d: DeviceEvent Device: %d - %s  method: %s, data: %s' %(time.time(), deviceId,
            # self.tellstick.getName(deviceId), self.tellstick.methodsReadable.get(method, 'Unknown'), data))

            received = self.event_received[deviceId] = 1
            self.lasttime[deviceId] = time.time()

            # print "method=" + str(method)
            if method == self.tellstick.TELLSTICK_TURNON:
                self.connection.emit_event(deviceId, "event.device.statechanged", 255, "")
                self.log.trace("emit_event statechanged %s ON 255", deviceId)

            if method == self.tellstick.TELLSTICK_TURNOFF:
                self.connection.emit_event(deviceId, "event.device.statechanged", 0, "")
                self.log.trace("emit_event statechanged %s OFF 0", deviceId)

    # def agoDeviceChangeEvent(self, deviceId, changeEvent, changeType, callbackId):
    #    if debug:
    #        print ('%d: DeviceChangeEvent Device: %d - %s' %(time.time(), deviceId, self.tellstick.getName(deviceId)))
    #        print ('  changeEvent: %d' %( changeEvent ))
    #        print ('  changeType: %d' %( changeType ))


    # def agoRawDeviceEvent(self, data, controllerId, callbackId):
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
        if not self.sensors[devId]["ignore"]:
            self.log.debug("emitTempChanged called for devID=%s temp=%f", str(devId), temp)
            tempC = temp
            self.log.trace("TempUnits=%s", self.TempUnits)
            if self.TempUnits == 'F':
                tempF = 9.0 / 5.0 * tempC + 32.0
                if tempF != float(self.sensors[devId]["lastTemp"]):
                    self.sensors[devId]["lastTemp"] = tempF
                    self.connection.emit_event(devId, "event.environment.temperaturechanged", str(tempF), "degF")
                    self.sensors[devId]["lastEmit"] = time.time()
                    self.log.debug("emitTempChanged emitted temp=%s F for device %s", str(tempF), str(devId))
            else:
                # print "devId=" + str(devId)
                self.log.trace("TempC=%s lastTemp=%s", str(tempC), str(self.sensors[devId]["lastTemp"]))
                if tempC != float(self.sensors[devId]["lastTemp"]):
                    self.sensors[devId]["lastTemp"] = tempC
                    # + "-temp"
                    self.connection.emit_event(devId, "event.environment.temperaturechanged", str(tempC), "degC")
                    self.sensors[devId]["lastEmit"] = time.time()
                    self.log.debug("emitTempChanged emitted temp=%s C for device %s", str(tempC), str(devId))

    def emitHumidityChanged(self, devId, humidity):
        if not self.sensors[devId]["ignore"]:
            self.log.debug("emitHumidityChanged called for devID=%s humidity=", str(devId))  # , humidity) TODO: Fix
            if humidity != float(self.sensors[devId]["lastHumidity"]):
                self.sensors[devId]["lastHumidity"] = humidity
                self.sensors[devId]["lastEmit"] = time.time()
                # + "-hum"
                self.connection.emit_event(devId, "event.environment.humiditychanged", str(humidity), "%")
                # self.log.debug("emitHumdityChanged emitted hum=%s% for devId=%s", str(humidity), str(devId))
                self.log.debug("emitHumdityChanged emitted devId=%s", str(devId))

    def loadSensorConfig(self, devId):
        try:
            offset = float(self.get_config_option('Offset', 0.0, section=str(devId), app='tellstick'))
            self.log.info("Offset for dev=%s set to %s", devId, str(offset))
        except ValueError:
            offset = 0.0
            self.log.info("No offset for dev=%s", devId)
        self.sensors[devId]["offset"] = offset

        try:
            desc = float(self.get_config_option('Desc', "", section=str(devId), app='tellstick'))
        except ValueError:
            desc = ""
        self.sensors[devId]["description"] = desc

    def listNewSensors(self):
        sensors = self.tellstick.listSensors()
        self.log.debug("listSensors returned %d items", len(sensors))
        for id, value in sensors.iteritems():
            self.log.trace("listNewSensors: devId: %s ", str(id))
            if not value["new"]:
                continue

            value["new"] = False

            devId = str(id)
            if devId not in self.sensors:
                self.sensors[devId] = sensors[id]
                self.sensors[devId]["ignore"] = False

                if devId in self.ignoreDevices:
                    self.sensors[devId]["ignore"] = True
                else:
                    if value["model"] in self.ignoreModels:
                        self.sensors[devId]["ignore"] = True
                    else:
                        self.sensors[devId]["ignore"] = False
                        try:
                            ignoreDev = self.get_config_option('Ignore', "No", section=devId, app='tellstick')
                            if ignoreDev.strip().lower() == "yes":
                                self.sensors[devId]["ignore"] = True
                        except ValueError:
                            self.sensors[devId]["ignore"] = False

                        if not self.sensors[devId]["ignore"]:
                            self.loadSensorConfig(devId)
                            if value["isTempSensor"]:
                                self.connection.add_device(devId, "temperaturesensor")
                                self.emitTempChanged(devId, float(value["temp"]) + value["offset"])
                                self.log.info("devId=%s added as tempsensor at %s deg", devId,
                                              str(value["temp"] + value["offset"]))
                            if value["isHumiditySensor"]:
                                self.connection.add_device(devId, "humiditysensor")
                                self.emitHumidityChanged(devId, float(value["humidity"]) + value["offset"])
                                self.log.info("devId=%s added as humidity sensor at %s%%", devId,
                                              str(value["humidity"] + value["offset"]))

    def agoSensorEvent(self, protocol, model, id, dataType, value, timestamp, callbackId):
        self.log.trace("SensorEvent protocol: %s model: %s id: %s value: %s%s timestamp: %d",
                       protocol, model, id, value, (" deg" if dataType == 1 else "%"), timestamp)

        devId = str(id) + ("-temp" if dataType == 1 else "-hum")
        if devId not in self.sensors:
            self.log.trace("New temp sensor found id= %s", devId)
            self.listNewSensors()

        if not self.sensors[devId]["ignore"]:
            if dataType & self.tellstick.TELLSTICK_TEMPERATURE == self.tellstick.TELLSTICK_TEMPERATURE:
                self.log.trace("Emit temp change for %s temp=%f offset=%f", str(devId), float(value),
                               self.sensors[devId]["offset"])
                self.log.trace("Emit temp change for %s offset temp=%f ", str(devId),
                               float(value) + self.sensors[devId]["offset"])
                self.emitTempChanged(devId, float(value) + self.sensors[devId]["offset"])
                # tempC = value
                # if TempUnits == 'F':
                #     tempF = 9.0/5.0 * tempC + 32.0
                #     self.connection.emit_event(str(devId), "event.environment.temperaturechanged", tempF, "degF")
                # else:
                #     self.connection.emit_event(str(devId), "event.environment.temperaturechanged", tempC, "degC")
            if dataType & self.tellstick.TELLSTICK_HUMIDITY == self.tellstick.TELLSTICK_HUMIDITY:
                self.log.trace("Emit humidity change for %s", str(devId))
                self.emitHumidityChanged(devId, float(float(value) + self.sensors[devId]["offset"]))
                # self.connection.emit_event(str(devId), "event.environment.humiditychanged", float(value), "%")

    def __init__(self):
        self.timers = {}  # List of timers
        self.event_received = {}
        self.lasttime = {}
        self.dev_delay = {}
        self.sensors = {}
        self.ignoreModels = {}
        self.ignoreDevices = {}
        self.SensorMaxWait = 300
        self.inventory = {}

        self.tellstick = None
        self.SensorPollInterval = 300.0  # 5 minutes
        self.general_delay = 0.5
        self.setnames = False
        self.TempUnits = "C"
        #super(AgoTellstick, self).__init__()
        agoclient.AgoApp.__init__(self)


    def setup_app(self):
        try:
            self.general_delay = float(
                self.get_config_option('Delay', 500, section='EventDevices', app='tellstick')) / 1000
        except ValueError:
            self.general_delay = 0.5

        try:
            self.SensorPollInterval = float(
                self.get_config_option('SensorPollInterval', 300, section='tellstick', app='tellstick'))
            self.log.debug("SensorPollIntervall set to %d", self.SensorPollInterval)
        except ValueError:
            self.SensorPollInterval = 300.0  # 5 minutes
            self.log.debug("SensorPollIntervall defaulted to 300s")

        units = self.get_config_option("units", "SI", section="units")

        if units.lower() == "us":
            self.TempUnits = "F"
        self.log.debug("TempUnits: %s", self.TempUnits)

        stick_version = self.get_config_option('StickVersion', 'Duo', section='tellstick', app='tellstick')
        try:
            if "Net" in stick_version:
                # Postpone tellsticknet loading, has extra dependencies which
                # we can do without if we've only got a Duo
                from tellsticknet import tellsticknet
                self.tellstick = tellsticknet(self)
                self.log.debug("Stick: Tellstick Net found in config")
            else:
                from tellstickduo import tellstickduo
                self.tellstick = tellstickduo(self)
                self.log.debug("Stick: Defaulting to Tellstick Duo")
        except OSError, e:
            self.log.error("Failed to load Tellstick stick version code: %s", e)
            raise agoclient.agoapp.StartupError()

        try:
            setnames = self.get_config_option('SetNames', 'No', section='tellstick', app='tellstick')
        except ValueError:
            setnames = 'No'

        self.setnames = (True if setnames.lower() == 'yes' else False)
        self.log.info("setnames: Device names will " + (
            "" if self.setnames else "not ") + "be fetched from Tellstick config file")

        self.tellstick.init(self.SensorPollInterval, self.TempUnits)

        self.connection.add_handler(self.message_handler)

        # Get agocontroller, required to set names on new devices
        self.inventory = self.connection.get_inventory()
        self.agoController = self.connection.get_agocontroller(self.inventory)
        if self.agoController is None:
            self.log.error("No agocontroller found, cannot set device names")
            raise agoclient.agoapp.StartupError()

        ####################################################
        self.log.info("Getting list of models and devices to ignore")
        self.readIgnores()

        self.log.info("Getting switches and dimmers + remotes & motion sensors")
        self.listNewDevices()

        self.log.info("Getting temp and humidity sensors")
        try:
            self.SensorMaxWait = float(
                self.get_config_option('MaxWait', 300, section='Sensors', app='tellstick'))
            self.log.debug("SensorMaxWait set to %s" + str(self.SensorMaxWait))
        except ValueError:
            self.SensorMaxWait = 300.0  # 5 minutes
            self.log.debug("SensorMaxWait defaulted to 300s")

        self.listNewSensors()

        #
        #  Register event handlers
        #
        cbId = []

        cbId.append(self.tellstick.registerDeviceEvent(self.agoDeviceEvent))
        self.log.debug('Register device event returned: %s', str(cbId[-1]))

        # cbId.append(self.tellstick.registerDeviceChangedEvent(self.agoDeviceChangeEvent))
        # info ('Register device changed event returned:' + str(cbId[-1]))

        # cbId.append(self.tellstick.registerRawDeviceEvent(self.agoRawDeviceEvent))
        # info ('Register raw device event returned:' + str(cbId[-1]))

        cbId.append(self.tellstick.registerSensorEvent(self.agoSensorEvent))
        self.log.debug('Register sensor event returned: %s', str(cbId[-1]))

    def readIgnores(self):
        try:
            ignoreModels = self.get_config_option('IgnoreModels', "", section='EventDevices', app='tellstick')
            self.log.debug("Got from config file: IgnoreModel=%s", ignoreModels)
        except ValueError:
            ignoreModels = ""
            self.log.debug("ValueError, IgnoreModels is empty")
        self.ignoreModels = ignoreModels.replace(' ', '').split(',')
        self.tellstick.ignoreModels = self.ignoreModels
        try:
            ignoreDevices = self.get_config_option('IgnoreDevices', "", section='EventDevices', app='tellstick')
            self.log.debug("Got from config file: IgnoreDevices=%s", ignoreDevices)
        except ValueError:
            ignoreDevices = ""
            self.log.debug("ValueError, IgnoreDevices is empty")
        self.ignoreDevices = ignoreDevices.replace(' ', '').split(',')
        self.tellstick.ignoreDevices = self.ignoreDevices

    def listNewDevices(self):
        def setNameIfNecessary(deviceUUID, name):
            dev = self.inventory['devices'].get(deviceUUID)
            if (dev is None or dev['name'] == '') and name != '':
                content = {"command": "setdevicename",
                           "uuid"   : self.agoController,
                           "device" : deviceUUID,
                           "name"   : name}

                message = Message(content=content)
                self.connection.send_message(None, content)
                self.log.debug("'setdevicename' message sent for %s, name=%s", deviceUUID, name)

        switches = self.tellstick.listSwitches()
        for devId, dev in switches.iteritems():
            model = dev["model"]
            name = dev["name"]

            self.log.trace("devId=%s name=%s model=%s", devId, name, model)

            if devId in self.ignoreDevices:
                self.log.info("Device ignored")

            else:
                try:
                    type = self.get_config_option('Type', "unset", section=str(devId), app='tellstick')
                except ValueError:
                    type = "unset"

                # if model not in self.ignoreModels:
                if type.lower() == "wall switch" or type.lower() == "motion sensor":
                    try:
                        self.dev_delay[devId] = float(
                            self.get_config_option('Delay', self.general_delay, section=str(devId),
                                                   app='tellstick')) / 1000
                    except ValueError:
                        self.dev_delay[devId] = self.general_delay

                    self.connection.add_device(devId, "binarysensor")
                    self.log.info("devId=%s added as %s", str(devId), type)

                elif dev["isDimmer"]:
                    self.connection.add_device(devId, "dimmer")
                    self.log.info("devId=%s added as dimmer", str(devId))

                else:
                    self.connection.add_device(devId, "switch")
                    self.log.info("devId=%s added as switch", str(devId))

                deviceUUID = self.connection.internal_id_to_uuid(devId)
                # info ("deviceUUID=" + deviceUUID + " name=" + self.tellstick.getName(devId))
                # info("Switch Name=" + dev.name + " protocol=" + dev.protocol + " model=" + dev.model)

                # If a new device, set name from the tellstick config file
                if self.setnames:
                    setNameIfNecessary(deviceUUID, name)

    def app_cleanup(self):
        if self.tellstick:
            self.tellstick.close()


if __name__ == "__main__":
    AgoTellstick().main()
