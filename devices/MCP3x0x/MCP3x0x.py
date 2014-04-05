#! /usr/bin/env python
#
#
# Copyright (C) 2012 Andreas Pagander
#
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License.
# This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
# of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#    
# See the GNU General Public License for more details.
#
#
# /etc/opt/agocontrol/conf.d/MCP3x0x.conf
#
# [MCP3x0x]
#
# voltage_divider = 1
# inputs = 0,0
# interval=600
# change=0.1
#


import agoclient
import threading
import time
import subprocess
import re
import string
from time import gmtime, strftime
import spidev

spi = spidev.SpiDev()
spi.open(0, 0)

client = agoclient.AgoConnection("MCP3x0x")

vDiv = float(agoclient.getConfigOption("MCP3x0x", "voltage_divider", "1"))
readInputs = agoclient.getConfigOption("MCP3x0x", "inputs", "0,1")
interval = int(agoclient.getConfigOption("MCP3x0x", "interval", "60"))
change = float(agoclient.getConfigOption("MCP3x0x", "change", "0.1"))

inputs = map(int, readInputs.split(','))

deviceconfig = {}

for adcCh in inputs:
    deviceconfig[(adcCh, 'value')] = 0 
    deviceconfig[(adcCh, 'lastreporttime')] = time.time()
    client.addDevice(adcCh, "energysensor")

def readadc(adcnum):
    read = spi.xfer2([1, 8 + adcnum << 4, 0])
    adcout = ((read[1] & 3) << 8) + read[2]
    return adcout


def readadc12(adcnum):
    read = spi.xfer2([4 | 2 | (adcnum >> 2), (adcnum & 3) << 6, 0])
    adcout = ((read[1] & 15) << 8) + read[2] 
    return adcout
   
class readMCP3x0xGPIO(threading.Thread):
    def __init__(self,):
        threading.Thread.__init__(self)    
    def run(self):
        while True:
            for adcCh in inputs:
                adctot = 0
                for i in range(5):
                    read_adc = readadc(adcCh)
                    print 'read_adc', read_adc
                    adctot += read_adc
                    time.sleep(0.05)
                read_adc = adctot / 5 / 1.0
                print read_adc
                volts = round(read_adc*(3.33 / 1024.0)*vDiv, 2)
                print "Battery Voltage:", adcCh, volts
                if abs(deviceconfig[(adcCh, 'value')] - volts) > change:
                    print 'level change:', deviceconfig[(adcCh, 'value')]
                    client.emitEvent(adcCh , "event.environment.energychanged", volts, "V") 
                    deviceconfig[(adcCh, 'value')] = volts
                    deviceconfig[(adcCh, 'lastreporttime')] = time.time()
                if time.time() > deviceconfig[(adcCh, 'lastreporttime')] + interval:
                    print 'interval:', deviceconfig[(adcCh, 'value')]
                    client.emitEvent(adcCh , "event.environment.energychanged", volts, "V") 
                    deviceconfig[(adcCh, 'temp')] = volts
                    deviceconfig[(adcCh, 'lastreporttime')] = time.time()
            time.sleep(10)
      
background = readMCP3x0xGPIO()
background.setDaemon(True)
background.start()

client.run()
