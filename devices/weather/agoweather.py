#! /usr/bin/env python
#
# ago Weather device
# Copyright (c) 2013 by rages
# Modifications by dinki and Kevin Gottsman
# Release History
# v.1 Initial release by rages
# v.2 Fix exception error plus modifications for using yahoo since weather.com not working and only report when values change
# v.3 Added functions to change between scales, added dewpoint object, renamed humdity to avoid confusion, various code cleanup
# v.3 Calculation code shamelessly taken from http://pydoc.net/Python/weather/0.9.1/weather.units.temp/
#
#
# Create /etc/opt/agocontrol/conf.d/weather.conf
# [weather]
# locations_ID=ITLM2916
# tempunits = f
# waittime = 30
#
import agoclient
import time
import threading
import pywapi
import string

readID = agoclient.get_config_option("weather","locations_ID","90210")
readTempUnits = agoclient.get_config_option("weather","tempunits","f")
readWaitTime = int(agoclient.get_config_option("weather","waittime","300"))
rain = "rain"
temp = "temp"
humidity = "humidity"
dewpoint = "dewpoint"

client = agoclient.AgoConnection("Weather")

client.add_device(rain, "binarysensor")
client.add_device(temp, "temperaturesensor")
client.add_device(humidity, "multilevelsensor")
client.add_device(dewpoint, "multilevelsensor")

def celsius_to_fahrenheit(c):
    'Degrees Celsius (C) to degrees Fahrenheit (F)'
    return (c * 1.8) + 32.0

def fahrenheit_to_celsius(f):
    'Degrees Fahrenheit (F) to degrees Celsius (C)'
    return (f - 32.0) * 0.555556

def calc_dewpoint(temp, hum):
    '''
    calculates the dewpoint via the formula from weatherwise.org
    return the dewpoint in degrees F.
    '''

    c = fahrenheit_to_celsius(temp)
    x = 1 - 0.01 * hum;

    dewpoint = (14.55 + 0.114 * c) * x;
    dewpoint = dewpoint + ((2.5 + 0.007 * c) * x) ** 3;
    dewpoint = dewpoint + (15.9 + 0.117 * c) * x ** 14;
    dewpoint = c - dewpoint;

    return round(celsius_to_fahrenheit(dewpoint))




class testEvent(threading.Thread):
        def __init__(self,):
                threading.Thread.__init__(self)
        def run(self):
		old_temp = 0
		old_humidity = 0
		old_dewpoint = 0
                while (True):

			try:
				yahooweather_result = pywapi.get_weather_from_yahoo(readID)
				if yahooweather_result != "":
					condizioni = yahooweather_result['condition']['text']
					temperatura = float(yahooweather_result['condition']['temp'])
					umidita = float(yahooweather_result['atmosphere']['humidity'])
					dew = calc_dewpoint(temperatura, umidita)
					if temperatura != old_temp:
						if (readTempUnits == 'f' or readTempUnits == 'F'):
							tempF = round(celsius_to_fahrenheit(temperatura))
							client.emit_event(temp, "event.environment.temperaturechanged", tempF, "F")
						else:
       			                 		client.emit_event(temp, "event.environment.temperaturechanged", temperatura, "C")
					if umidita != old_humidity:
		       				client.emit_event(humidity, "event.environment.humiditychanged", umidita, "%")
		                        search_Rain = string.find(condizioni, "Rain")
					search_Drizzle = string.find(condizioni, "Drizzle")
		                        if (search_Rain >= 0) or (search_Drizzle >=0):
						client.emit_event(rain,"event.device.statechanged", "255", "")

		                        else :
						client.emit_event(rain,"event.device.statechanged", "0", "")

			except KeyError:
				print "Error retrieving weather data!"
			old_temp = temperatura
			old_humidity = umidita
			old_dewpoint = dew
			time.sleep (readWaitTime)

background = testEvent()
background.setDaemon(True)
background.start()

client.run()

