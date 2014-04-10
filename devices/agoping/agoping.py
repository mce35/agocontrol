# This program is used to monitor hosts through TCP/IP ping. 
# The code for this originated from the wifi_detect_device
# module written by dinki. That plugin's code originated from 
# https://github.com/blackairplane/pydetect but was heavily 
# modified for use with Ago Control
# 
# Create CONFDIR/conf.d/agoping.conf
# [agoping]
# hostaddress=8.8.8.8,192.168.1.1
# devicename=Google,router
# check_time=15
#
# *NOTE*  First host address matches first devicename, second matches second, etc
#         check_time is the time (seconds) between each check 		


import agoclient
import threading
import time
import os
import sys

readHostaddress = agoclient.get_config_option("agoping","hostaddress","00:00:00:00:00:00, FF:FF:FF:FF:FF:FF")
hostaddress = map(str, readHostaddress.split(','))

readDevicename = agoclient.get_config_option("agoping","devicename","Google, Router")
devicename =  map(str, readDevicename.split(','))

client = agoclient.AgoConnection("agoping")
for name in devicename:
	client.add_device(name, "binarysensor")

hostdevices = dict(zip(hostaddress, devicename))

last_seen = {}
for address in hostaddress:
	last_seen[address] = 0

check_time = float(agoclient.get_config_option("agoping","check_time","15"))


def checkForDevice(deviceaddress):
        # See if the device has connected to the network
        systemCommand = "ping -c 1 " + deviceaddress + " > /dev/null 2>&1"
        #print systemCommand
        check_status = os.system(systemCommand) # Will return 0 if the line is found 1 if not found
	if check_status == 0:
		last_seen[deviceaddress] = time.time();
		success = True
	else:
		success = False

        return success


def connectionDaemon(deviceaddress):
	connected = checkForDevice(deviceaddress)
	if (connected == True):
		#print hostdevices[deviceaddress] + " is online"  # Uncomment for debug
		client.emit_event(hostdevices[deviceaddress], "event.device.statechanged", "255", "")
	if (connected == False):
		#print hostdevices[deviceaddress] + " is offline" # Uncomment for debug
		client.emit_event(hostdevices[deviceaddress], "event.device.statechanged", "0", "")

def initDevices():
	for deviceaddress in hostaddress:
		last_seen[deviceaddress] = time.time();	
		client.emit_event(hostdevices[deviceaddress], "event.device.statechanged", "0", "")


class agoping(threading.Thread):
    def __init__(self,):
        threading.Thread.__init__(self)    
    def run(self):
	initDevices()
	i = 0
	while (i == 0):
		for deviceaddress in hostaddress:
			connectionDaemon(deviceaddress)
		time.sleep(check_time)

 
background = agoping()
background.setDaemon(True)
background.start()

client.run()

