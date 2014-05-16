#! /usr/bin/env python 
#
# gps device for agocontrol
#
# Copyright (C) 2014 Andreas Pagander <andreas.pagander@gamil.com>
#
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License.
# This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
# of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#    
# See the GNU General Public License for more details.

import agoclient
import threading
import time
import gps
import math



client = agoclient.AgoConnection("gps")

device = "gps-1"

client.add_device(device, "gpssensor")

#Init vars
core_lat	=	float(agoclient.get_config_option("system", "lat", ""))
core_long	=	float(agoclient.get_config_option("system", "lon", ""))


lastreporttime = time.time()

def calculateDistance(lat1, long1, lat2, long2):

    # Convert latitude and longitude to 
    # spherical coordinates in radians.
    try:
        degrees_to_radians = math.pi/180.0
            
        phi1 = (90.0 - lat1)*degrees_to_radians
        phi2 = (90.0 - lat2)*degrees_to_radians
            
        theta1 = long1*degrees_to_radians
        theta2 = long2*degrees_to_radians
            
        
        cos = (math.sin(phi1)*math.sin(phi2)*math.cos(theta1 - theta2) + 
               math.cos(phi1)*math.cos(phi2))
        arc = math.acos( cos )
    except:
        print 'calculate error'
        arc = 0
    # calculate distance in meters
    return arc * 6373000



class readGps(threading.Thread):
    def __init__(self,):
        threading.Thread.__init__(self)    
    def run(self):
        session = gps.gps("localhost", "2947")
        session.stream(gps.WATCH_ENABLE | gps.WATCH_NEWSTYLE)
        lastLat = 0
        lastLong = 0
        while True:
            try:
                report = session.next()
                if report['class'] == 'TPV':
                    if hasattr(report, 'lat'):	
                        move = calculateDistance(lastLat, lastLong, report.lat, report.lon)
                        if (move > 2):
                            print 'Move-Time:', report.time
                            print report
                            lastLat = report.lat
                            lastLong = report.lon
                            lastreporttime = time.time()
                            coordinates = {}
                            coordinates['latitude'] = report.lat
                            coordinates['longitude'] = report.lon
                            coordinates['unit'] = 'deg'
                            client.emit_event_raw(device, "event.environment.positionchanged", coordinates)
                            client.emit_event(device, "event.device.distancechanged", calculateDistance(core_lat, core_long, report.lat, report.lon), "m")
                            client.emit_event(device, "event.device.speedchanged", report.speed, "")

                        if (time.time() > lastreporttime + 600):
                            print 'Time-Time:', report.time
                            print report
                            lastreporttime = time.time()
                            coordinates = {}
                            coordinates['latitude'] = report.lat
                            coordinates['longitude'] = report.lon
                            coordinates['unit'] = 'deg'
                            client.emit_event_raw(device, "event.environment.positionchanged", coordinates)
                            client.emit_event(device, "event.device.distancechanged", calculateDistance(core_lat, core_long, report.lat, report.lon), "m")
                            client.emit_event(device, "event.device.speedchanged", report.speed, "")
                            
            except StopIteration:
                session = None
                print "GPSD has terminated"

background = readGps()
background.setDaemon(True)
background.start()

client.run()
