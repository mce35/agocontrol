
# Device wrapper for Python-TStat
# copyright (c) 2013 Michael Dingman <michael@mdingman.com>

import agoclient
import threading
import time

from TStat import *

client = agoclient.AgoConnection("radiothermostat")

#currentTemp = float(t.getCurrentTemp())
#tstatMode = t.getTstatMode()
#fanMode = t.getFanMode()
#holdState = bool(t.getHoldState())
#fanState = t.getFanState()
#heatPoint = float(t.getHeatPoint())
#coolPoint = float(t.getCoolPoint())

def messageHandler(internalid, content):
	tstatMode = t.getTstatMode()
	if "command" in content:     	
	 	if content["command"] == "settemperature":
	         	print "setting temp: " + internalid + " Current mode: " + tstatMode
			if tstatMode == "Heat":
				print "set heat pont: " + content["temperature"]
				t.setHeatPoint(float(content["temperature"]))
	                	client.emit_event(internalid, "event.environment.temperaturechanged", content["temperature"], "")
			elif tstatMode =="Cool":
				t.setCoolPoint(float(content["temperature"]))
				client.emit_event(internalid, "event.environment.temperaturechanged", content["temperature"], "")
	      	if content["command"] == "setthermostatmode":
	                print "set thermostat mode: " + internalid
	                #client.emit_event(internalid, "event.device.statechanged", "0", "")
    		if content["command"] == "setthermostatfanmode":
			print "set radio thermostat fan mode: " + internalid
		if content["command"] == "setthermostathold":
			print "set radio thermostat hold: " + internalid


	
# specify our message handler method
client.add_handler(messageHandler)

ipAddress =  agoclient.get_config_option("radiothermostat", "ipaddress", "0.0.0.0")
tempUnit = agoclient.get_config_option("system", "units", "SI")
#print "IP: ", ipAddress
#print "UNIT: ", tempUnit
t = TStat(ipAddress)
t.setCacheExpiry(15)

client.add_device(ipAddress, "thermostat")

#TODO implement thread here for polling
# in the background or need to handle some other communication. If you don't need one or if you want to keep things simple at the moment just skip this section.

class readThermostat(threading.Thread):
    #def cToF(tempC):
        #retval = float((float(tempC) * (9.0/5/0)) + 32)
        #return retval
    #def fToC(tempF):
        #retval = float((float(tempF) - 32) / (9.0/5.0))
        #return retval	
    def __init__(self,):
        threading.Thread.__init__(self)    
    def run(self):
    	#level = 0
	tTemp = float(0)
	currentTemp = float(0)  
        while (True):
			tTemp = float(t.getCurrentTemp())
			if (tempUnit == "SI"):
				tTemp = round((tTemp - 32.0) / (9.0/5.0),1)
      			if (tTemp != currentTemp):
				client.emit_event(ipAddress, "event.environment.temperaturechanged", str(tTemp), tempUnit)
				currentTemp = tTemp
			time.sleep (30)
      
background = readThermostat()
background.setDaemon(True)
background.start()

client.run()

