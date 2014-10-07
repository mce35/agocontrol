#! /usr/bin/python
# -*- coding: utf-8 -*-

# IPX800 relay board client
# http://gce-electronics.com
# copyright (c) 2013 tang
 
import sys
import os
import agoclient
import threading
import time
import logging
from pyipx800v3 import Ipx800v3
from qpid.datatypes import uuid4
import json

DEVICEMAPFILE = os.path.join(agoclient.CONFDIR, 'maps/ipx800.json')
IPX_WEBSERVER_PORT = 8010
STATE_ON = 255
STATE_OFF = 0
STATE_OPENED = 1
STATE_CLOSED = 0
STATE_OPENING = 50
STATE_CLOSING = 100
STATE_STOPPED = 150
DEVICE_BOARD = ''
DEVICE_OUTPUT_SWITCH = 'oswitch'
DEVICE_OUTPUT_DRAPES = 'odrapes'
DEVICE_ANALOG_TEMPERATURE = 'atemperature'
DEVICE_ANALOG_HUMIDITY = 'ahumidity'
DEVICE_ANALOG_LIGHT = 'alight'
DEVICE_ANALOG_VOLT = 'avolt'
DEVICE_ANALOG_BINARY = 'abinary'
DEVICE_COUNTER = 'counter'
DEVICE_DIGITAL_BINARY = 'dbinary'
GET_STATUS_FREQUENCY = 60 #in seconds

client = None
devices = {}
durations = {}
ipx800v3 = None
units = 'SI'
configLock = threading.Lock()
devicesLock = threading.Lock()
getBoardsStatusTask = None

#logging.basicConfig(filename='agoipx800.log', level=logging.INFO, format="%(asctime)s %(levelname)s : %(message)s")
logging.basicConfig(level=logging.INFO, format="%(asctime)s %(name)s %(levelname)s : %(message)s")
logger = logging.getLogger('agoipx800')


#=================================
#classes
#=================================
class Task:
    def __init__(self, interval, task, args= [], kwargs={}):
        self._args = args
        self._kwargs = kwargs
        self._interval = interval
        self._task = task
        self.__timer = None

    def __run(self):
        self.__timer = threading.Timer(self._interval, self.__run)
        self.__timer.start()
        self._task(*self._args, **self._kwargs)

    def start(self):
        if self.__timer:
            self.stop()
        self.__timer = threading.Timer(self._interval, self.__run)
        self.__timer.start()

    def stop(self):
        if self.__timer:
            self.__timer.cancel()
            self.__timer = None



#=================================
#utils
#=================================
def quit(msg):
    """Exit application"""
    global ipx800v3, client
    if getBoardsStatusTask:
        getBoardsStatusTask.stop()
    if client:
        del client
        client = None
    if ipx800v3:
        ipx800v3.stop()
        del ipx800v3
        ipx800v3 = None
    logger.fatal(msg)
    sys.exit(0)
    
def saveDevices():
    """Save devices infos"""
    global DEVICEMAPFILE, devices
    try:
        configLock.acquire()
        f = open(DEVICEMAPFILE, "w")
        f.write(json.dumps(devices))
        f.close()
        configLock.release()
    except:
        logger.exception('Unable to save devices infos')
        return False
    return True

def loadDevices():
    """Load devices infos"""
    global DEVICEMAPFILE, devices
    try:
        if os.path.exists(DEVICEMAPFILE):
            configLock.acquire()
            f = open(DEVICEMAPFILE, "r")
            j = f.readline()
            f.close()
            devices = json.loads(j)
            configLock.release()
        else:
            configLock.acquire()
            logger.info('Create default empty config file "%s"' % DEVICEMAPFILE)
            f = open(DEVICEMAPFILE, "w")
            f.write(json.dumps({}))
            f.close()
            devices = {}
            configLock.release()
    except:
        logger.exception('Unable to load devices infos')
        return False
    return True
    
def getDevice(ipxIp, internalid):
    """Return device identified by internalid on ipxIp board
       @param ipxIp: ipx ip address
       @param internalid: device internalid"""
    global devices
    if not devices.has_key(ipxIp):
        logger.warning('Try to get a device of unknown IPX800 [%s]' % ipxIp)
        return None
    if devices[ipxIp].has_key(internalid):
        return devices[ipxIp][internalid]
    return None
    
def getIpx800Ip(internalid):
    """Return ipx ip of which the device (internalid) belongs"""
    global devices
    for ipxIp in devices:
        if ipxIp==internalid:
            #the internalid is a board
            return ipxIp
        else:
            #maybe the internalid is a device
            for device in devices[ipxIp]:
                if device==internalid:
                    #device found
                    return ipxIp
    return None

def getDeviceUsingOutput(ipxIp, outputId):
    """return device using specified outputId
       @param ipxIp: ip address of ipx 
       @param outputId : output id to search for (must be int)
       @return None if no device are using specified output id, device otherwise"""
    global devices
    if not devices.has_key(ipxIp):
        logger.warning('Try to get a device of unknown IPX800 [%s]' % ipxIp)
        return None, None
    for internalid in devices[ipxIp]:
        if devices[ipxIp][internalid]['type']==DEVICE_OUTPUT_SWITCH:
            if outputId in devices[ipxIp][internalid]['outputs']:
                return internalid, devices[ipxIp][internalid]
        elif devices[ipxIp][internalid]['type']==DEVICE_OUTPUT_DRAPES:
            if outputId==devices[ipxIp][internalid]['open'] or outputId==devices[ipxIp][internalid]['close']:
                return internalid, devices[ipxIp][internalid]
    return None, None

def getDeviceUsingDigital(ipxIp, digitalId):
    """return device using specified digitalId
       @param ipxIp: ip address of ipx 
       @param digitalId : digital id to search for
       @return None if no device are using specified digital id, device otherwise"""
    global devices
    if not devices.has_key(ipxIp):
        logger.warning('Try to get a device of unknown IPX800 [%s]' % ipxIp)
        return None, None
    for internalid in devices[ipxIp]:
        if digitalId in devices[ipxIp][internalid]['digitals']:
            return internalid, devices[ipxIp][internalid]
    return None, None

def getDeviceUsingAnalog(ipxIp, analogId):
    """return device using specified analogId
       @param ipxIp: ip address of ipx 
       @param analogId : analog id to search for
       @return None if no device are using specified analog id, device otherwise"""
    global devices
    if not devices.has_key(ipxIp):
        logger.warning('Try to get a device of unknown IPX800 [%s]' % ipxIp)
        return None, None
    for internalid in devices[ipxIp]:
        if analogId in devices[ipxIp][internalid]['analogs']:
            return internalid, devices[ipxIp][internalid]
    return None, None

def getDeviceUsingCounter(ipxIp, counterId):
    """return device using specified counterId
       @param ipxIp: ip address of ipx 
       @param counterId : counter id to search for
       @return None if no device are using specified counter id, device otherwise"""
    global devices
    if not devices.has_key(ipxIp):
        logger.warning('Try to get a device of unknown IPX800 [%s]' % ipxIp)
        return None, None
    for internalid in devices[ipxIp]:
        if counterId in devices[ipxIp][internalid]['counters']:
            return internalid, devices[ipxIp][internalid]
    return None, None

def getOutputLinkedTo(ipxIp, binary):
    """Return output linked to specified binary
       @param binary: binary device internalid """
    global devices
    if not devices.has_key(ipxIp):
        logger.warning('Try to get a device of unknown IPX800 [%s]' % ipxIp)
        return None, None
    for internalid in devices[ipxIp]:
        if binary in devices[ipxIp][internalid]['links']:
            return internalid, devices[ipxIp][internalid]
    return None, None

def getEmptyDevice():
    """Return empty structure of a device"""
    return {'type':None, 'outputs':[], 'digitals':[], 'analogs':[], 'counters':[], 'state':None, 'duration':None, 'open':None, 'close':None, 'links':[]}

def updateDeviceDuration(ipxIp, internalid, duration):
    """Update device durarion"""
    global devices
    if devices.has_key(ipxIp) and devices[ipxIp].has_key(internalid) and isinstance(duration, dict) and duration.has_key('start') and duration.has_key('stop'):
        devicesLock.acquire()
        devices[ipxIp][internalid]['duration'] = duration['stop'] - duration['start']
        saveDevices()
        devicesLock.release()
    else:
        logger.error('updateDeviceDuration: device "%s" not found' % internalid)
    
def emitDeviceValueChanged(ipxIp, internalid, value):
    """Emit event for specified device"""
    global client, units, devices
    #get device
    device = getDevice(ipxIp, internalid)
    if device:
        event = 'event.device.statechanged'
        unit = '-'
        if device['type']==DEVICE_ANALOG_TEMPERATURE:
            event = 'event.environment.temperaturechanged'
            if units=='SI':
                unit = 'degC'
            else:
                try:
                    value = ((value*9)+(5*32))/5
                    unit = 'degF'
                except:
                    unit = 'degC'
        elif device['type']==DEVICE_ANALOG_HUMIDITY:
            event = 'event.environment.humiditychanged'
            unit = '%'
        elif device['type']==DEVICE_ANALOG_VOLT:
            event = 'event.environment.energychanged'
            unit = 'Volts'
        elif device['type']==DEVICE_ANALOG_LIGHT:
            event = 'event.environment.brightnesschanged'
            unit = 'Lux'
        elif device['type']==DEVICE_COUNTER:
            event = 'event.environment.counterchanged'
            unit = '-'
        elif device['type']==DEVICE_ANALOG_BINARY:
            #fix value: 0<value<1023
            if value<512:
                value = STATE_ON
            else:
                value = STATE_OFF
            event = 'event.device.statechanged'
            unit = '-'
        elif device['type']==DEVICE_DIGITAL_BINARY:
            #TODO fix value?
            event = 'event.device.statechanged'
            unit = '-'
        else:
            event = 'event.device.statechanged'
            unit = None
        devicesLock.acquire()
        devices[ipxIp][internalid]['state'] = value
        devicesLock.release()
        saveDevices()
        client.emit_event(internalid, event, str(value), unit)
        return True
    else:
        logger.error('emitDeviceValueChanged: no device found for internalid "%s"' % internalid)
        return False
        

        
#=================================
#functions
#=================================
def ipxCallback(ipxIp, content):
    """ipxCallback is called after IPX800 M2M call
       ipxDict: dict of digitals/outputs/counters/timer/... depending on IPX800 M2M config"""
    logger.debug('Callback received from ipx "%s": %s' % (ipxIp, str(content)))
    global client
    global durations
    #update device state
    for item in content:
        if item.startswith('out'):
            #ipx output
            try:
                outputid = int(item.replace('out', ''))
                (internalid, device) = getDeviceUsingOutput(ipxIp, outputid)
                if device:
                    if device['type']==DEVICE_OUTPUT_SWITCH:
                        if content[item]==0:
                            if device['state']==STATE_ON:
                                #off action
                                logger.info('Switch "%s@%s" turned off' % (ipxIp, internalid))
                                emitDeviceValueChanged(ipxIp, internalid, STATE_OFF)
                        elif content[item]==1:
                            if device['state']==STATE_OFF:
                                #on action
                                logger.info('Switch "%s@%s" turned on' % (ipxIp, internalid))
                                emitDeviceValueChanged(ipxIp, internalid, STATE_ON)
                        else:
                            logger.warning('Unknown value received for switch action [%s]' % str(content[item]))
                    elif device['type']==DEVICE_OUTPUT_DRAPES:
                        if not durations.has_key(internalid):
                            durations[internalid] = {'start':0, 'stop':0}
                        if outputid==device['open']:
                            #open action
                            if content[item]==0:
                                if device['state']==STATE_OPENING:
                                    #drapes opened
                                    logger.info('Drapes "%s@%s" opened' % (ipxIp, internalid))
                                    emitDeviceValueChanged(ipxIp, internalid, STATE_OPENED)
                                    durations[internalid]['stop'] = int(time.time())
                                    logger.debug('duration[%s]: start=%d stop=%d' % (internalid, durations[internalid]['start'], durations[internalid]['stop']))
                                    updateDeviceDuration(ipxIp, internalid, durations[internalid])
                            elif content[item]==1:
                                if device['state']==STATE_CLOSED:
                                    #drapes is opening
                                    logger.info('Drapes "%s@%s" is opening' % (ipxIp, internalid))
                                    emitDeviceValueChanged(ipxIp, internalid, STATE_OPENING)
                                    durations[internalid]['start'] = int(time.time())
                            else:
                                #unknown value
                                logger.warning('Unknown value received for drapes open action [%s]' % str(content[item]))
                        elif outputid==device['close']:
                            #close action
                            if content[item]==0:
                                if device['state']==STATE_CLOSING:
                                    #drapes closed
                                    logger.info('Drapes "%s@%s" closed' % (ipxIp, internalid))
                                    emitDeviceValueChanged(ipxIp, internalid, STATE_CLOSED)
                                    durations[internalid]['stop'] = int(time.time())
                                    logger.debug('duration[%s]: start=%d stop=%d' % (internalid, durations[internalid]['start'], durations[internalid]['stop']))
                                    updateDeviceDuration(ipxIp, internalid, durations[internalid])
                            elif content[item]==1:
                                if device['state']==STATE_OPENED:
                                    #Drapes is closing
                                    logger.info('Drapes "%s@%s" is closing' % (ipxIp, internalid))
                                    emitDeviceValueChanged(ipxIp, internalid, STATE_CLOSING)
                                    durations[internalid]['start'] = int(time.time())
                            else:
                                #unknown value
                                logger.warning('Unknown value received for drapes close action [%s]' % str(content[item]))
                    else:
                        #TODO manage new device using outputs here
                        pass
            except:
                logger.exception('Exception in ipxCallback (output):')

        elif item.startswith('in'):
            #ipx digital
            try:
                digitalid = int(item.replace('in', ''))
                (internalid, device) = getDeviceUsingDigital(ipxIp, digitalid)
                if device:
                    if device['type']==DEVICE_DIGITAL_BINARY:
                        logger.info('Update value of digital "%s@%s[%s]" with "%s"' % (ipxIp, internalid, device['type'], str(content[item])))
                        emitDeviceValueChanged(ipxIp, internalid, content[item])
                    else:
                        #handle new device using digitals here
                        pass
            except:
                logger.exception('Exception in ipxCallback (digital):')

        elif item.startswith('an'):
            #analog
            try:
                analogid = int(item.replace('an', ''))
                (internalid, device) = getDeviceUsingAnalog(ipxIp, analogid)
                if device:
                    if device['type'] in (DEVICE_ANALOG_TEMPERATURE, DEVICE_ANALOG_VOLT, DEVICE_ANALOG_HUMIDITY, DEVICE_ANALOG_LIGHT, DEVICE_ANALOG_BINARY):
                        logger.info('Update value of analog "%s@%s[%s]" with "%s"' % (ipxIp, internalid, device['type'], str(content[item])))
                        emitDeviceValueChanged(ipxIp, internalid, content[item])
                    else:
                        #handle new device using analogs here
                        pass
            except:
                logger.exception('Exception in ipxCallback (analog):')

        elif item.startswith('cnt'):
            try:
                #counter
                counterid = int(item.replace('cnt', ''))
                (internalid, device) = getDeviceUsingCounter(ipxIp, counterid)
                if device:
                    if device['type']==DEVICE_COUNTER:
                        logger.info('Update value of counter device "%s@%s" with "%s"' % (ipxIp, internalid, str(content[item])))
                        emitDeviceValueChanged(ipxIp, internalid, content[item])
                    else:
                        #TODO manage new device using counters here
                        pass
            except:
                logger.exception('Exception in ipxCallback (counter):')

        else:
            #unmanaged info
            pass

def getBoardsStatus():
    """get status of boards devices"""
    global devices
    if len(devices)==0:
        return
    logger.info('Update values of analog and counter devices:')
    for ipxIp in devices:
        status = ipx800v3.getStatus(ipxIp)
        if status and len(status)>0:
            for internalid in devices[ipxIp]:
                if devices[ipxIp][internalid]['type'] in (DEVICE_ANALOG_TEMPERATURE, DEVICE_ANALOG_HUMIDITY, DEVICE_ANALOG_VOLT, DEVICE_ANALOG_LIGHT, DEVICE_ANALOG_BINARY):
                    if len(devices[ipxIp][internalid]['analogs'])==1 and status.has_key('an%d' % devices[ipxIp][internalid]['analogs'][0]):
                        value = status['an%d' % devices[ipxIp][internalid]['analogs'][0]]
                        logger.info('  - value "%s" set to analog "%s@%s"' % (str(value), ipxIp, internalid))
                        emitDeviceValueChanged(ipxIp, internalid, value)
                    else:
                        logger.warning('Unable to set current analog value to "%s@%s"' % (ipxIp, internalid))
                        #logger.info('key=%s status:%s' % ('an%d' % devices[ipxIp][internalid]['analogs'][0], status))
                        emitDeviceValueChanged(ipxIp, internalid, 0.0)

                elif devices[ipxIp][internalid]['type']==DEVICE_COUNTER:
                    if len(devices[ipxIp][internalid]['counters'])==1 and status.has_key('cnt%d' % devices[ipxIp][internalid]['counters'][0]):
                        value = status['cnt%d' % devices[ipxIp][internalid]['counters'][0]]
                        logger.info('  - value "%s" set to counter "%s@%s"' % (str(value), ipxIp, internalid))
                        emitDeviceValueChanged(ipxIp, internalid, value)
                    else:
                        logger.warning('Unable to set current counter value to "%s@%s"' % (ipxIp, internalid))
                        #logger.info('key=%s status:%s' % ('cnt%d' % devices[ipxIp][internalid]['counters'][0], status))
                        emitDeviceValueChanged(ipxIp, internalid, 0)

                elif devices[ipxIp][internalid]['type']==DEVICE_DIGITAL_BINARY:
                    if len(devices[ipxIp][internalid]['digitals'])==1 and status.has_key('in%d' % devices[ipxIp][internalid]['digitals'][0]):
                        value = status['in%d' % devices[ipxIp][internalid]['digitals'][0]]
                        logger.info('  - value "%s" set to digital "%s@%s"' % (str(value), ipxIp, internalid))
                        emitDeviceValueChanged(ipxIp, internalid, value)
                    else:
                        logger.warning('Unable to set current digital value to "%s@%s"' % (ipxIp, internalid))
                        #logger.info('key=%s status:%s' % ('in%d' % devices[ipxIp][internalid]['digitals'][0], status))
                        emitDeviceValueChanged(ipxIp, internalid, 0)
    

def addIpx800v3Board(ipxIp):
    """add ipx800v3 board"""
    global client, devices
    if ipxIp and len(ipxIp)>0 and not devices.has_key(ipxIp):
        devices[ipxIp] = {}
        if saveDevices():
            client.add_device(ipxIp, 'ipx800v3board')
            return True
        else:
            logger.error('addIpx800v3Board: unable to save devices')
            return False
    else:
        logger.error('addIpx800v3Board: ipx already registered!')
        return False

def removeIpx800v3Board(ipxIp):
    """remove ipx800v3 board"""
    global client, devices
    if ipxIp and len(ipxIp)>0 and devices.has_key(ipxIp):
        del devices[ipxIp]
        if saveDevices():
            client.remove_device(ipxIp)
            return True
        else:
            logger.error('addIpx800v3Board: unable to save devices')
            return False
    else:
        logger.error('removeIpx800v3Board: invalid ip address [%s]' % ipxIp)
        return False
        
def deleteDevice(ipxIp, internalid):
    """Delete device"""
    #TODO when removing output, remove links too !
    global client, devices
    if ipxIp and len(ipxIp)>0 and internalid and len(internalid)>0:
        if devices[ipxIp].has_key(internalid):
            client.remove_device(internalid)
            del devices[ipxIp][internalid]
            if saveDevices():
                return True, ''
            else:
                logger.error('deleteDevice: unable to save devices')
                return False, 'Internal error'
        else:
            logger.error('deleteDevice: device "%s" doesnt exists' % internalid)
            return False, 'Unable to delete device'
    else:
        logger.error('deleteDevice: wrong parameter')
        return False, 'Internal error'

def addOutputSwitch(ipxIp, outputId):
    """add switch as virtual device
       @param name: name of switch
       @param output: used relay"""
    global client
    if ipxIp and len(ipxIp)>0:
        #make sure outputId is integer
        try:
            outputId = int(outputId)
        except:
            logger.error('addOutputSwitch: unable to add switch because invalid id [%s]' % str(outputId))
            return False, 'Unable to add switch because invalid id [%s]' % str(outputId)

        #search device already using specified output
        (internalid, device) = getDeviceUsingOutput(ipxIp, outputId)
        if device:
            logger.error('addOutputSwitch: output id %d is already used by device "%s"' % (outputId, internalid))
            return False, 'Output id %d is already used by device %s' % (outputId, internalid)
            
        #add device
        internalid = str(uuid4())
        devices[ipxIp][internalid] = getEmptyDevice()
        devices[ipxIp][internalid]['type'] = DEVICE_OUTPUT_SWITCH
        devices[ipxIp][internalid]['state'] = STATE_OFF
        devices[ipxIp][internalid]['outputs'].append(outputId)
        if saveDevices():
            client.add_device(internalid, 'switch')
            emitDeviceValueChanged(ipxIp, internalid, devices[ipxIp][internalid]['state'])
            logger.info('Switch "%s@%s" added successfully' % (ipxIp, internalid))
            return True, ''
        else:
            logger.error('addOutputSwitch: unable to save devices')
            return False, 'Internal error'
    else:
        logger.error('addOutputSwitch: invalid parameters')
        return False, 'Internal error'

def addOutputDrapes(ipxIp, relayOpen, relayClose):
    """add drapes as virtual device
       @param ipxIp: ipx ip address
       @param relayOpen: relay used to open the drapes (saved in inputs list)
       @param relayClose: relay used to close the drapes (saved in outputs list)"""
    global client
    if ipxIp and len(ipxIp)>0:
        #make sure relayOpen and relayClose are integers
        try:
            relayOpen = int(relayOpen)
            relayClose = int(relayClose)
        except:
            logger.error('addOutputDrapes: unable to add drape because invalid id [open=%s close=%s]' % (str(relayOpen), str(relayClose)))
            return False, 'Unable to add drape because invalid id [open=%s close=%s]' % (str(relayOpen), str(relayClose))

        #check specified outputs
        if relayOpen==relayClose:
            logger.error('addOutputDrapes: open and close relay are the same')
            return False, 'Open and close relays must be different'
        (internalid, device) = getDeviceUsingOutput(ipxIp, relayOpen)
        if device:
            logger.error('addOutputDrapes: output id %d used to open drapes is already used by device "%s"' % (relayOpen, internalid))
            return False, 'Output id %d used to open drapes is already used by device %s' % (relayOpen, internalid)
        (internalid, device) = getDeviceUsingOutput(ipxIp, relayClose)
        if device:
            logger.error('addOutputDrapes: output id %d used to close drapes is already used by device "%s"' % (relayClose, internalid))
            return False, 'Output id %d used to close drapes is already used by device %s' % (relayClose, internalid)
            
        #add device
        internalid = str(uuid4())
        devices[ipxIp][internalid] = getEmptyDevice()
        devices[ipxIp][internalid]['type'] = DEVICE_OUTPUT_DRAPES
        devices[ipxIp][internalid]['state'] = STATE_OPENED
        devices[ipxIp][internalid]['open'] = relayOpen
        devices[ipxIp][internalid]['close'] = relayClose
        if saveDevices():
            client.add_device(internalid, 'drapes')
            emitDeviceValueChanged(ipxIp, internalid, devices[ipxIp][internalid]['state'])
            logger.info('Drapes "%s@%s" added successfully' % (ipxIp, internalid))
            return True, ''
        else:
            logger.error('addOutputDrapes: unable to save devices')
            return False, 'Internal error'
    else:
        logger.error('addOutputDrapes: invalid parameters')
        return False, 'Internal error'

def addDigitalBinary(ipxIp, digitalId):
    """add binary sensor as virtual device
       @param name: name of switch
       @param digitalId: used digital input"""
    global client
    if ipxIp and len(ipxIp)>0:
        #make sure digitalId is integer
        try:
            digitalId = int(digitalId)
        except:
            logger.error('addDigitalBinary: unable to add binary because invalid id [%s]' % str(digitalId))
            return False, 'Unable to add binary because invalid digital id [%s]' % str(digitalId)

        #search device already using specified digital
        (internalid, device) = getDeviceUsingDigital(ipxIp, digitalId)
        if device:
            logger.error('addDigitalBinary: digital id %d is already used by device "%s"' % (digitalId, internalid))
            return False, 'Digital id %d is already used by device %s' % (digitalId, internalid)
            
        #add device
        internalid = str(uuid4())
        devices[ipxIp][internalid] = getEmptyDevice()
        devices[ipxIp][internalid]['type'] = DEVICE_DIGITAL_BINARY
        devices[ipxIp][internalid]['state'] = STATE_OFF
        devices[ipxIp][internalid]['digitals'].append(digitalId)
        if saveDevices():
            client.add_device(internalid, 'binarysensor')
            emitDeviceValueChanged(ipxIp, internalid, devices[ipxIp][internalid]['state'])
            logger.info('Digital binary "%s@%s" added successfully' % (ipxIp, internalid))
            return True, ''
        else:
            logger.error('addDigitalBinary: unable to save devices')
            return False, 'Internal error'
    else:
        logger.error('addDigitalBinary: invalid parameters')
        return False, 'Internal error'

def addAnalog(ipxIp, deviceType, analogId):
    """add analog device
       @param ipxIp: ipx ip address
       @param analogId: analog id used
       @param deviceType: device type """
    global client, ipx800v3
    if ipxIp and len(ipxIp)>0 and deviceType in (DEVICE_ANALOG_TEMPERATURE, DEVICE_ANALOG_HUMIDITY, DEVICE_ANALOG_VOLT, DEVICE_ANALOG_LIGHT, DEVICE_ANALOG_BINARY):
        #make sure analogId is integer
        try:
            analogId = int(analogId)
        except:
            logger.error('addAnalog: unable to add analog because invalid id [%s]' % str(analogId))
            return False, 'Unable to add analog because invalid id [%s]' % str(analogId)

        #search device already using specified analog
        (internalid, device) = getDeviceUsingAnalog(ipxIp, analogId)
        if device:
            logger.error('addAnalog: analog id %s is already used by device "%s"' % (str(analogId), internalid))
            return False, 'Analog id %s is already used by device %s' % (str(analogId), internalid)

        #get current analog value
        status = ipx800v3.getStatus(ipxIp)
        value = 0.0
        if status and len(status)>0:
            key = 'an%d' % analogId
            if status.has_key(key):
                value = status[key]

        #add device
        internalid = str(uuid4())
        devices[ipxIp][internalid] = getEmptyDevice()
        devices[ipxIp][internalid]['type'] = deviceType
        devices[ipxIp][internalid]['state'] = value
        devices[ipxIp][internalid]['analogs'].append(analogId)
        if saveDevices():
            if deviceType==DEVICE_ANALOG_TEMPERATURE:
                client.add_device(internalid, 'temperaturesensor')
            elif deviceType==DEVICE_ANALOG_HUMIDITY:
                client.add_device(internalid, 'humiditysensor')
            elif deviceType==DEVICE_ANALOG_VOLT:
                client.add_device(internalid, 'energymeter')
            elif deviceType==DEVICE_ANALOG_LIGHT:
                client.add_device(internalid, 'brightnesssensor')
            elif deviceType==DEVICE_ANALOG_BINARY:
                client.add_device(internalid, 'binarysensor')
            else:
                logger.error('addAnalog: no analog type found')
            emitDeviceValueChanged(ipxIp, internalid, value)
            logger.info('Analog "%s@%s" of type "%s" added successfully with value "%s"' % (ipxIp, internalid, deviceType, str(value)))
            return True, ''
        else:
            logger.error('addAnalog: unable to save devices')
            return False, 'Internal error'
    else:
        logger.error('addAnalog: invalid parameters')
        return False, 'Internal error'

def addCounter(ipxIp, counterId):
    """add counter device
       @param ipxIp: ipx ip address
       @param counterId: counter id used"""
    global client
    if ipxIp and len(ipxIp)>0:
        #make sure counterId is integer
        try:
            counterId = int(counterId)
        except:
            logger.error('addCounter: unable to add counter because invalid id [%s]' % str(counterId))
            return False, 'Unable to add counter because invalid id [%s]' % str(counterId)

        #search device already using specified output
        (internalid, device) = getDeviceUsingCounter(ipxIp, counterId)
        if device:
            logger.error('addCounter: counter id %s is already used by device "%s"' % (str(counterId), internalid))
            return False, 'AddCounter: counter id %s is already used by device %s' % (str(counterId), internalid)

        #get current counter value
        status = ipx800v3.getStatus(ipxIp)
        value = 0
        if status and len(status)>0:
            key = 'cnt%d' % counterId
            if status.has_key(key):
                value = status[key]

        #add device
        internalid = str(uuid4())
        devices[ipxIp][internalid] = getEmptyDevice()
        devices[ipxIp][internalid]['type'] = DEVICE_COUNTER
        devices[ipxIp][internalid]['state'] = value
        devices[ipxIp][internalid]['counters'].append(counterId)
        if saveDevices():
            client.add_device(internalid, 'multilevelsensor')
            emitDeviceValueChanged(ipxIp, internalid, value)
            logger.info('Counter "%s@%s" added successfully with value "%s"' % (ipxIp, internalid, str(value)))
            return True, ''
        else:
            logger.error('addCounter: unable to save devices')
            return False, 'Internal error'
    else:
        logger.error('addCounter: invalid parameters')
        return False, 'Internal error'

def addLink(ipxIp, output, binary):
    """add link between specified output and digital
       @param output: output internalid
       @param binary: binary device internalid """
    global devices
    if ipxIp and len(ipxIp)>0 and devices.has_key(ipxIp) and devices[ipxIp].has_key(output) and devices[ipxIp].has_key(binary):
        #get existing link
        (internalid, device) = getOutputLinkedTo(ipxIp, binary)
        if device:
            logger.error('addLink: output %s is already linked to binary %s' % (str(internalid), str(binary)))
            return False, 'addLink: output %s is already linked to binary %s' % (str(internalid), str(binary))

        #create link (stored in output object type)
        devices[ipxIp][output]['links'].append(binary)
        if saveDevices():
            logger.info('Link added successfully for output %s' % (output))
            return True, ''
        else:
            logger.error('addLink: unable to save devices')
            return False, 'Internal error'
    else:
        logger.error('addLink: invalid parameters')
        return False, 'Internal error'

def deleteLink(ipxIp, output, digital):
    """delete link between output and digital
       @param output: output internalid
       @param digital: digital internalid"""
    global devices
    if ipxIp and len(ipxIp)>0 and devices.has_key(ipxIp) and devices[ipxIp].has_key(output) and devices[ipxIp].has_key(digital):
        #get digital input
        if len(devices[ipxIp][digital]['digitals'])==1:
            devices[ipxIp][output]['links'].remove(digital)
            saveDevices()
            return True, ''
        else:
            logger.error('deleteLink: invalid digital inputs count (expected only one)')
            return False, 'Internal error'
    else:
        logger.error('deleteLink: invalid parameters')
        return False, 'Internal error'

def checkLink(ipxIp, output):
    """Check link status
       @return True if link is valid (binary device is OFF) or no link associated
       @return False if link is not valid (binary device is ON) 
       @param output: output internalid """
    global devices
    dev = getDevice(ipxIp, output)
    if not dev:
        logger.error('checkLink: unknown output device "%s"' % str(output))
        return False

    if len(dev['links'])>0:
        #first of all get boards status
        getBoardsStatus()
        #then check links status
        for binary in dev['links']:
            if devices[ipxIp].has_key(binary):
                if devices[ipxIp][binary]['state']==STATE_ON:
                    return False
                else:
                    return True
            else:
                logger.error('Unknown device "%s" referenced in links for output "%s"' % (str(binary), str(output)))
                return False
    else:
        #no associated links
        return True

def openDrapes(ipxIp, internalid):
    """open drapes
       @param internalid: internal id 
       @param ipxIp: ipx ip"""
    global ipx800v3, devices
    dev = getDevice(ipxIp, internalid)
    if dev:
        if dev['state']!=STATE_OPENED:
            #check link
            if checkLink(ipxIp, internalid):
                outputId = dev['open']
                logger.info('openDrapes: ipxIp=%s outputId=%d' % (ipxIp, outputId))
                ipx800v3.setOutput(ipxIp, outputId, 1)
            else:
                logger.info('openDrapes: associated binary prevents from drapes to be opened')
                return False
        else:
            logger.info('openDrapes: drape %s is already opened' % str(internalid))
            return False
    else:
        logger.error('closeDrapes: no device found for "%s@%s"' % (str(ipxIp), str(internalid)))
        return False
    return True

def closeDrapes(ipxIp, internalid):
    """open drapes
       @param internalid: internal id 
       @param ipxIp: ipx ip"""
    global ipx800v3, devices
    dev = getDevice(ipxIp, internalid)
    if dev:
        if dev['state']!=STATE_CLOSED:
            #check link
            if checkLink(ipxIp, internalid):
                outputId = dev['close']
                logger.debug('closeDrapes: ipxIp=%s outputId=%d' % (ipxIp, outputId))
                ipx800v3.setOutput(ipxIp, outputId, 1)
            else:
                logger.info('closeDrapes: associated binary prevents from drapes to be closed')
                return False
        else:
            logger.info('closeDrapes: drape %s is already closed' % str(internalid))
            return False
    else:
        logger.error('closeDrapes: no device found for "%s@%s"' % (str(ipxIp), str(internalid)))
        return False
    return True

def stopDrapes(ipxIp, internalid):
    """stop drapes
       @param internalid: internal id 
       @param ipxIp: ipx ip"""
    global ipx800v3
    logger.debug('stopDrapes: ipxIp=%s' % (ipxIp))
    dev = getDevice(ipxIp, internalid)
    if dev:
        if dev['state']==STATE_OPENING:
            ipx800v3.setOutput(ipxIp, dev['open'], 0)
            client.update_device_state(internalid, STATE_STOPPED)
        elif dev['state']==STATE_CLOSING:
            ipx800v3.setOutput(ipxIp, dev['close'], 0)
            client.update_device_state(internalid, STATE_STOPPED)
    else:
        logger.error('stopDrapes: no device found for "%s@%s"' % (str(ipxIp), str(internalid)))
        return False
    return True

def setlevelDrapes(ipxIp, internalid):
    """setlevelDrapes is a callback of setlevel command Timer
       @param internalid: internal id 
       @param ipxIp: ipx ip"""
    logger.info('setlevelDrapes: ipxIp=%s' % (ipxIp))
    stopDrapes(ipxIp, internalid)
    return False

def turnOnSwitch(ipxIp, internalid):
    """turn on switch
       @param internalid: internal id 
       @param ipxIp: ipx ip"""
    global ipx800v3, devices
    dev = getDevice(ipxIp, internalid)
    if dev:
        if checkLink(ipxIp, internalid):
            outputId = dev['outputs'][0]
            ipx800v3.setOutput(ipxIp, outputId, 1)
        else:
            logger.info('turnOnSwitch: associated binary prevents from switch to be turned on')
            return False
    else:
        logger.error('turnOnSwitch: no device found for "%s@%s"' % (str(ipxIp), str(internalid)))
        return False
    return True

def turnOffSwitch(ipxIp, internalid):
    """turn off switch
       @param internalid: internal id 
       @param ipxIp: ipx ip"""
    global ipx800v3, devices
    dev = getDevice(ipxIp, internalid)
    if dev:
        if checkLink(ipxIp, internalid):
            outputId = dev['outputs'][0]
            ipx800v3.setOutput(ipxIp, outputId, 0)
        else:
            logger.info('turnOffSwitch: associated binary prevents from switch to be turned off')
            return False
    else:
        logger.error('turnOffSwitch: no device found for "%s@%s"' % (str(ipxIp), str(internalid)))
        return False
    return True

def resetCounter(ipxIp, internalid):
    """Reset specified counter
       @param internalid: internalid
       @param ipxIp: ipx ip
       @param counterId: counter id"""
    global ipx800v3, devices
    dev = getDevice(ipxIp, internalid)
    if dev:
        counterId = dev['counters'][0]
        ipx800v3.setCounter(ipxIp, counterId, 0)
    else:
        logger.error('resetCounter: no device found for "%s@%s"' % (str(ipxIp), str(internalid)))
        return False
    return True

def commandHandler(internalid, content):
    """ago command handler"""
    logger.info('commandHandler: %s, %s' % (internalid,content))
    global client
    command = None

    if content.has_key('command'):
        command = content['command']
    else:
        logger.error('No command specified')
        return None

    if internalid=='ipx800controller':
        #controller command
        if command=='addboard':
            if content.has_key('ip'):
                logger.info('Add IPX800v3 board with IP "%s"' % content['ip'])
                if addIpx800v3Board(content['ip']):
                    return {'error':0, 'msg':'Board added successfully'}
                else:
                    return {'error':1, 'msg':'Board already registered'}
            else:
                logger.error('Missing "ip" parameter')
                return {'error':1, 'msg':'Internal error'}
                
        elif command=='getboards':
            boards = []
            for ipxIp in devices:
                boards.append(ipxIp)
            return {'error':0, 'msg':'', 'boards':boards}

        else:
            return {'error':1, 'msg':'Unknown command'}
    else:
        #device command
        
        #get ipx ip
        ipxIp = getIpx800Ip(internalid)
        if not ipxIp:
            logger.error('No Ipx ip found!')
            return {'error':1, 'msg':'Internal error'}
            
        if command=='adddevice':
            #check type presence
            if not content.has_key('type'):
                logger.error('Missing "type" parameter')
                return {'error':1, 'msg':'Internal error'}

            logger.debug('Add device of type "%s"' % content['type'])
            if content['type']==DEVICE_OUTPUT_SWITCH:
                if content.has_key('pin1'):
                    logger.info('Add switch on "%s" using Out%s' % (ipxIp, str(content['pin1'])))
                    (res, msg) = addOutputSwitch(ipxIp, content['pin1'])
                    if res:
                        return {'error':0, 'msg':'Device added successfully'}
                    else:
                        return {'error':1, 'msg':msg}
                else:
                    logger.error('Missing "pin1" parameter')
                    return {'error':1, 'msg':'Internal error'}
                    
            elif content['type']==DEVICE_OUTPUT_DRAPES:
                if content.has_key('pin1') and content.has_key('pin2'):
                    logger.info('Add drapes on "%s" using Out%s and Out%s' % (ipxIp, str(content['pin1']), str(content['pin2'])))
                    (res, msg) = addOutputDrapes(ipxIp, content['pin1'], content['pin2'])
                    if res:
                        return {'error':0, 'msg':'Device added successfully'}
                    else:
                        return {'error':1, 'msg':msg}
                else:
                    logger.error('Missing "pin1" and/or "pin2" parameters')
                    return {'error':1, 'msg':'Internal error'}
                    
            elif content['type'] in (DEVICE_ANALOG_TEMPERATURE, DEVICE_ANALOG_HUMIDITY, DEVICE_ANALOG_VOLT, DEVICE_ANALOG_LIGHT, DEVICE_ANALOG_BINARY):
                if content.has_key('pin1'):
                    logger.info('Add analog on "%s" using An%s' % (ipxIp, str(content['pin1'])))
                    (res, msg) = addAnalog(ipxIp, content['type'], content['pin1'])
                    if res:
                        return {'error':0, 'msg':'Device added successfully'}
                    else:
                        return {'error':1, 'msg':msg}
                else:
                    logger.error('Missing "pin1" parameter')
                    return {'error':1, 'msg':'Internal error'}
                    
            elif content['type']==DEVICE_COUNTER:
                if content.has_key('pin1'):
                    logger.info('Add counter on "%s" using C%s' % (ipxIp, str(content['pin1'])))
                    (res, msg) = addCounter(ipxIp, content['pin1'])
                    if res:
                        return {'error':0, 'msg':'Device added successfully'}
                    else:
                        return {'error':1, 'msg':msg}
                else:
                    logger.error('Missing "pin1" parameter')
                    return {'error':1, 'msg':'Internal error'}
                    
            elif content['type']==DEVICE_DIGITAL_BINARY:
                if content.has_key('pin1'):
                    logger.info('Add digital binary on "%s" using In%s' % (ipxIp, str(content['pin1'])))
                    (res, msg) = addDigitalBinary(ipxIp, content['pin1'])
                    if res:
                        return {'error':0, 'msg':'Device added successfully'}
                    else:
                        return {'error':1, 'msg':msg}
                else:
                    logger.error('Missing "pin1" parameter')
                    return {'error':1, 'msg':'Internal error'}

            else:
                logger.error('adddevice: unknown device type "%s"' % (content['type']))
                return {'error':1, 'msg':'Internal error'}
                
        elif command=='deletedevice':
            if content.has_key('device'):
                logger.info('Delete device "%s"' % content['device'])
                (res, msg) = deleteDevice(ipxIp, content['device'])
                if res:
                    return {'error':0, 'msg':'Device deleted successfully'}
                else:
                    return {'error':1, 'msg':msg}
            else:
                logger.error('Missing "device" parameter')
                return {'error':1, 'msg':'Internal error'}

        elif command=='addlink':
            if content.has_key('output') and content.has_key('binary'):
                logger.info('Create link between output "%s" and binary "%s"' % (str(content['output']), str(content['binary'])))
                (res, msg) = addLink(ipxIp, content['output'], content['binary'])
                if res:
                    return {'error':0, 'msg':'Link created successfully'}
                else:
                    return {'error':1, 'msg':msg}
            else:
                logger.error('Missing "output" and/or "binary" parameter')
                return {'error':1, 'msg':'Internal error'}
                    
        elif command=='deletelink':
            if content.has_key('output') and content.has_key('digital'):
                logger.info('Delete link between output "%s" and digital "%s"' % (str(content['output']), str(content['digital'])))
                (res, msg) = deleteLink(ipxIp, content['output'], content['digital'])
                if res:
                    return {'error':0, 'msg':'Link deleted successfully'}
                else:
                    return {'error':1, 'msg':msg}
            else:
                logger.error('Missing "output" and/or "digital" parameter')
                return {'error':1, 'msg':'Internal error'}

        elif command=='on':
            device = getDevice(ipxIp, internalid)
            if device:
                if device['type']==DEVICE_OUTPUT_SWITCH:
                    if len(device['outputs'])==1:
                        logger.info('Turn on switch "%s@%s"' % (ipxIp, internalid))
                        turnOnSwitch(ipxIp, internalid)
                    else:
                        logger.error('command turnOn: outputs is not valid (1 awaited, %d received) [%s]' % (len(device['outputs']), device['outputs']))
                        return {'error':1, 'msg':'Internal error'}
                elif device['type']==DEVICE_OUTPUT_DRAPES:
                    logger.info('Open drapes "%s@%s"' % (ipxIp, internalid))
                    openDrapes(ipxIp, internalid)
                return {'error':0, 'msg':''}
            else:
                logger.error('commandHandler: command stop: no device found "%s"' % internalid)
                return {'error':1, 'msg':'No device found'}
                    
        elif command=='off':
            device = getDevice(ipxIp, internalid)
            if device:
                if device['type']==DEVICE_OUTPUT_SWITCH:
                    if len(device['outputs'])==1:
                        logger.info('Turn off switch "%s@%s"' % (ipxIp, internalid))
                        turnOffSwitch(internalid, ipxIp, device['outputs'][0])
                    else:
                        logger.error('command turnOff: outputs is not valid (1 awaited, %d received) [%s]' % (len(device['outputs']), device['outputs']))
                        return {'error':1, 'msg':'Internal error'}
                elif device['type']==DEVICE_OUTPUT_DRAPES:
                    logger.info('Close drapes "%s@%s"' % (ipxIp, internalid))
                    closeDrapes(ipxIp, internalid)
                return {'error':0, 'msg':''}
            else:
                logger.error('commandHandler: command stop: no device found "%s"' % internalid)
                return {'error':1, 'msg':'No device found'}
                    
        elif command=='allon':
            for internalid in devices[ipxIp]:
                if devices[ipxIp][internalid]['type']==DEVICE_OUTPUT_DRAPES:
                    openDrapes(ipxIp, internalid)
                elif devices[ipxIp][internalid]['type']==DEVICE_OUTPUT_SWITCH:
                    if len(devices[ipxIp][internalid]['outputs'])==1:
                        turnOnSwitch(ipxIp, internalid)
                    else:
                        logger.error('command allOn: outputs is not valid (1 awaited, %d received) [%s]' % (len(devices[ipxIp][internalid]['outputs']), devices[ipxIp][internalid]['outputs']))
                        return {'error':1, 'msg':'Internal error'}
            return {'error':0, 'msg':''}
            
        elif command=='alloff':
            for internalid in devices[ipxIp]:
                if devices[ipxIp][internalid]['type']==DEVICE_OUTPUT_DRAPES:
                    closeDrapes(ipxIp, internalid)
                elif devices[ipxIp][internalid]['type']==DEVICE_OUTPUT_SWITCH:
                    if len(devices[ipxIp][internalid]['outputs'])==1:
                        turnOffSwitch(internalid, ipxIp, devices[ipxIp][internalid]['outputs'][0])
                    else:
                        logger.error('command allOff: outputs is not valid (1 awaited, %d received) [%s]' % (len(devices[ipxIp][internalid]['outputs']), devices[ipxIp][internalid]['outputs']))
                        return {'error':1, 'msg':'Internal error'}
            return {'error':0, 'msg':''}
            
        elif command=='reset':
            #command only available for counters (multilevelsensors)
            if not content.has_key('device'):
                logger.error('command reset: missing parameter "device"')
                return {'error':1, 'msg':'Internal error'}

            internalid = content['device']
            device = getDevice(ipxIp, internalid)
            if device:
                if len(device['counters'])==1:
                    logger.info('Reset counter "%s@%s"' % (ipxIp, internalid))
                    resetCounter(ipxIp, internalid)
                else:
                    logger.error('command reset: counters is not valid (1 awaited, %d received) [%s]' % (len(device['counters']), device['counters']))
                    return {'error':1, 'msg':'Internal error'}
                return {'error':0, 'msg':''}
            else:
                logger.error('commandHandler: command reset: no device found "%s"' % internalid)
                return {'error':1, 'msg':'No device found'}
                
        elif command=='status':
            #return all digitals/outputs/analogs/counter usage (used or not)
            outputs = []
            digitals = []
            analogs = []
            counters = []
            for internalid in devices[ipxIp]:
                #digitals
                for digital in devices[ipxIp][internalid]['digitals']:
                    digitals.append('D%d' % (int(digital)+1))
                #outputs
                if devices[ipxIp][internalid]['type']==DEVICE_OUTPUT_DRAPES:
                    outputs.append('Ch%d' % (int(devices[ipxIp][internalid]['open'])+1))
                    outputs.append('Ch%d' % (int(devices[ipxIp][internalid]['close'])+1))
                else:
                    for output in devices[ipxIp][internalid]['outputs']:
                        outputs.append('Ch%d' % (int(output)+1))
                #analogs
                for analog in devices[ipxIp][internalid]['analogs']:
                    analogs.append('A%d' % (int(analog)+1))
                #counters
                for counter in devices[ipxIp][internalid]['counters']:
                    counters.append('C%d' % (int(counter)+1))
            outputs.sort()
            digitals.sort()
            analogs.sort()
            counters.sort()
            
            #and return all board devices and links
            links = []
            devs = {'outputs':[], 'digitals':[], 'analogs':[], 'counters':[]}
            for internalid in devices[ipxIp]:
                if devices[ipxIp][internalid]['type'] in (DEVICE_OUTPUT_DRAPES, DEVICE_OUTPUT_SWITCH):
                    if devices[ipxIp][internalid]['type']==DEVICE_OUTPUT_SWITCH:
                        devs['outputs'].append({'internalid':internalid, 'type':devices[ipxIp][internalid]['type'], 'inputs':'-'.join(str(x) for x in devices[ipxIp][internalid]['outputs'])})
                    if devices[ipxIp][internalid]['type']==DEVICE_OUTPUT_DRAPES:
                        devs['outputs'].append({'internalid':internalid, 'type':devices[ipxIp][internalid]['type'], 'inputs':'-'.join([str(devices[ipxIp][internalid]['open']), str(devices[ipxIp][internalid]['close'])])})
                    #links
                    for binary in devices[ipxIp][internalid]['links']:
                        if devices[ipxIp].has_key(binary):
                            links.append({'output':{'internalid':internalid, 'type':devices[ipxIp][internalid]['type']}, 'binary':{'internalid':binary, 'type':devices[ipxIp][binary]['type']}})
                        else:
                            logger.error('Device not found for digitalId #%s' % (str(digitalId)))
                if devices[ipxIp][internalid]['type'] in (DEVICE_COUNTER):
                    devs['counters'].append({'internalid':internalid, 'type':devices[ipxIp][internalid]['type'], 'inputs':'-'.join(str(x) for x in devices[ipxIp][internalid]['counters'])})
                if devices[ipxIp][internalid]['type'] in (DEVICE_ANALOG_TEMPERATURE, DEVICE_ANALOG_HUMIDITY, DEVICE_ANALOG_LIGHT, DEVICE_ANALOG_VOLT, DEVICE_ANALOG_BINARY):
                    devs['analogs'].append({'internalid':internalid, 'type':devices[ipxIp][internalid]['type'], 'inputs':'-'.join(str(x) for x in devices[ipxIp][internalid]['analogs'])})
                if devices[ipxIp][internalid]['type'] in (DEVICE_DIGITAL_BINARY):
                    devs['digitals'].append({'internalid':internalid, 'type':devices[ipxIp][internalid]['type'], 'inputs':'-'.join(str(x) for x in devices[ipxIp][internalid]['digitals'])})
            
            status = {'outputs':" ".join(outputs), 'digitals':" ".join(digitals), 'analogs':" ".join(analogs), 'counters':" ".join(counters)}
            logger.info(status)
            return {'error':0, 'msg':'', 'status':status, 'devices':devs, 'links':links}
            
        elif command=='setlevel':
            if not content.has_key('level'):
                logger.error('setlevel command: missing parameters')
                return {'error':1, 'msg':'Internal error'}
            level = 0
            try:
                level = int(content['level'])
            except:
                logger.error('setlevel command: unable to convert level to int')
                return {'error':1, 'msg':'Internal error'}
            device = getDevice(ipxIp, internalid)
            if device:
                if device['type']==DEVICE_OUTPUT_DRAPES:
                    if device['duration']>0:
                        interval = int(device['duration']*level/100)
                        if device['state']==STATE_CLOSED:
                            #open drapes until level
                            threading.Timer(interval, setlevelDrapes, ipxIp, internalid)
                        elif device['state']==STATE_OPENED:
                            #close drapes until level
                            threading.Timer(interval, setlevelDrapes, ipxIp, internalid)
                        else:
                            #impossible to setlevel of drapes that is moving
                            logger.warning('Command can be launched only with unmoving drapes')
                            return {'error':1, 'msg':'Command can be launched only with stopped drapes'}
                    else:
                        logger.warning('setlevel command: device duration is not setted. Unable to setlevel')
                        return {'error':1, 'msg':'Drapes must be initialized first (full open or close) before setting level'}
                return {'error':0, 'msg':'Ok'}
            else:
                logger.error('commandHandler: command setlevel: no device found "%s"' % internalid)
                return {'error':1, 'msg':'No device found'}
            
        elif command=='forcestate':
            if not content.has_key('device') or not content.has_key('state'):
                logger.error('forcestate command: missing parameters')
                return {'error':1, 'msg':'Internal error'}
            state = content['state']
            internalid = content['device']
            device = getDevice(ipxIp, internalid)
            if device:
                if device['type']==DEVICE_OUTPUT_DRAPES:
                    if state=='closed':
                        logger.info('Force drapes "%s@%s" state to CLOSED' % (ipxIp, internalid))
                        emitDeviceValueChanged(ipxIp, internalid, STATE_CLOSED)
                    else:
                        logger.info('Force drapes "%s@%s" state to OPENED' % (ipxIp, internalid))
                        emitDeviceValueChanged(ipxIp, internalid, STATE_OPENED)
                elif device['type']==DEVICE_OUTPUT_SWITCH:
                    if state=='closed':
                        logger.info('Force switch "%s@%s" state to OFF' % (ipxIp, internalid))
                        emitDeviceValueChanged(ipxIp, internalid, STATE_OFF)
                    else:
                        logger.info('Force switch "%s@%s" state to ON' % (ipxIp, internalid))
                        emitDeviceValueChanged(ipxIp, internalid, STATE_ON)
                return {'error':0, 'msg':'Ok'}
            else:
                logger.error('commandHandler: command forcestate: no device found "%s"' % internalid)
                return {'error':1, 'msg':'No device found'}
                       
        elif command=='stop':
            device = getDevice(ipxIp, internalid)
            if device:
                if device['type']==DEVICE_OUTPUT_DRAPES:
                    if not stopDrapes(ipxIp, internalid):
                        return {'error':1, 'msg':'Failed to stop drapes'}
                return {'error':0, 'msg':''}
            else:
                logger.error('commandHandler: command stop: no device found "%s"' % internalid)
                return {'error':1, 'msg':'No device found'}

        else:
            return {'error':1, 'msg':'Unknown command'}

def eventHandler(event, content):
    """ago event handler"""
    #logger.info('eventHandler: %s, %s' % (event, content))
    global client
    uuid = None
    internalid = None

    #get uuid
    #if content.has_key('uuid'):
    #    uuid = content['uuid']
    #    internalid = client.uuid_to_internal_id(uuid)
    
    #if uuid and uuid in client.uuids:
    #    #uuid belongs to this handler
    #    if event=='event.device.remove':
    #        logger.info('eventHandler: Removing device %s' % internalid)
    #        client.remove_device(internalid)


#=================================
#main
#=================================
#init
try:
    #connect agoclient
    client = agoclient.AgoConnection('ipx800')

    #get system units
    units = agoclient.get_config_option("system", "units", "SI")
    logger.info('System units: %s' % units)

    #add known devices
    if not loadDevices():
        quit('Unable to load devices. Exit now.')
        
    logger.info('Register existing devices:')
    for ipxIp in devices:
        #register board
        logger.info('  - add board [%s]' % ipxIp)
        client.add_device(ipxIp, 'ipx800v3board')
        for internalid in devices[ipxIp]:
            if devices[ipxIp][internalid]['type']==DEVICE_OUTPUT_SWITCH:
                logger.info('    - add switch [%s]' % internalid)
                client.add_device(internalid, 'switch')
            elif devices[ipxIp][internalid]['type']==DEVICE_OUTPUT_DRAPES:
                logger.info('    - add drapes [%s]' % internalid)
                client.add_device(internalid, 'drapes')
            elif devices[ipxIp][internalid]['type'] in (DEVICE_ANALOG_TEMPERATURE, DEVICE_ANALOG_HUMIDITY, DEVICE_ANALOG_VOLT, DEVICE_ANALOG_LIGHT, DEVICE_ANALOG_BINARY):
                logger.info ('    - add analog [%s]' % internalid)
                if devices[ipxIp][internalid]['type']==DEVICE_ANALOG_TEMPERATURE:
                    client.add_device(internalid, 'temperaturesensor')
                elif devices[ipxIp][internalid]['type']==DEVICE_ANALOG_HUMIDITY:
                    client.add_device(internalid, 'humiditysensor')
                elif devices[ipxIp][internalid]['type']==DEVICE_ANALOG_VOLT:
                    client.add_device(internalid, 'energymeter')
                elif devices[ipxIp][internalid]['type']==DEVICE_ANALOG_LIGHT:
                    client.add_device(internalid, 'brightnesssensor')
                elif devices[ipxIp][internalid]['type']==DEVICE_ANALOG_BINARY:
                    client.add_device(internalid, 'binarysensor')
            elif devices[ipxIp][internalid]['type']==DEVICE_COUNTER:
                logger.info('    - add counter [%s]' % internalid)
                client.add_device(internalid, 'multilevelsensor')
            elif devices[ipxIp][internalid]['type']==DEVICE_DIGITAL_BINARY:
                logger.info('    - add digital [%s]' % internalid)
                client.add_device(internalid, 'binarysensor')
            else:
                logger.error('    - add nothing: unknown device type [%s]' % (internalid))
        
    #create ipx800v3 object
    ipx800v3 = Ipx800v3(IPX_WEBSERVER_PORT, ipxCallback)
    ipx800v3.start()

    #update counter and analog devices value
    getBoardsStatus()

    #add client handlers
    client.add_handler(commandHandler)
    client.add_event_handler(eventHandler)

    #add controller
    logger.info('Add controller')
    client.add_device('ipx800controller', 'ipx800controller')

    #create tasks
    getBoardsStatusTask = Task(GET_STATUS_FREQUENCY, getBoardsStatus)
    getBoardsStatusTask.start()

except Exception as e:
    #init failed
    logger.exception("Exception on init")
    quit('Init failed, exit now.')

#run agoclient
try:
    logger.info('Running agoipx800...')
    client.run()
except KeyboardInterrupt:
    #stopped by user
    quit('agoipx800 stopped by user')
except Exception as e:
    logger.exception("Exception on main:")
    #stop everything
    quit('agoipx800 stopped')

