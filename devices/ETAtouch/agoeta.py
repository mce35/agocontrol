# ETAtouch REST API support for ago control
# More information about ETAtouch: http://www.eta.co.at/255.0.html
#
# copyright (c) 2014 Christoph Jaeger <jaeger@agocontrol.com>
#

import agoclient
import threading
import time
import urllib2
import xml.etree.ElementTree as ET

client = agoclient.AgoConnection("eta")

eta_ip = agoclient.get_config_option("eta", "IP", "127.0.0.1")

base_url = 'http://' + eta_ip + ':8080/user/var/'
eta_result = {}
eta_result_report = {}
eta_url = {'temperatur_aussen': '48/10241/0/0/12197', 'temperatur_raum': '120/10101/0/0/12634', 'puffer_ladezustand': '120/10251/0/0/12528', 'puffer_oben': '120/10251/0/0/12242', 'puffer_mitte': '120/10251/0/0/12522', 'puffer_unten': '120/10251/0/0/12244', 'warmwasserspeicher_temperatur': '120/10111/0/0/12271', 'abgas_restsauerstoff': '48/10391/0/0/12164', 'abgas_temperatur': '48/10391/0/0/12162' }

# add the devices
for key in eta_url:
  client.add_device(key, "multilevelsensor")

# emit result value and do checks 
def emit_result(key):
    if key == "puffer_ladezustand":
        client.emit_event(key, "event.environment.temperaturechanged", eta_result_report[key], "percent")
    elif key == "abgas_restsauerstoff":
        client.emit_event(key, "event.environment.temperaturechanged", eta_result_report[key], "percent")
    else:
        client.emit_event(key, "event.environment.temperaturechanged", eta_result_report[key], "degC")

# check the two dicts if key exists and if value matches
def get_diffs(a,b):
    for a_key in a:
        # check if key from a exists in b
        if a_key not in b:
            #print "key %s in a, but not in b" %a_key
            eta_result_report[a_key] = eta_result[a_key]
            # report result
            emit_result(a_key)

        # check if key from a has same value in key of b
        elif a[a_key] != b[a_key]:
            #print "key %s in a and in b, but values differ (%s in a and %s in b)" %(a_key, a[a_key], b[a_key])
            eta_result_report[a_key] = eta_result[a_key]
            # report result
            emit_result(a_key)
	

class eta_event(threading.Thread):
    def __init__(self,):
        threading.Thread.__init__(self)    
    def run(self):
    	level = 0
        while (True):
			try:
			    for key in eta_url:
			        request = urllib2.urlopen(base_url + eta_url[key])
			        tree = ET.parse(request)
			        root = tree.getroot()
			        result = root.find("{http://www.eta.co.at/rest/v1}value")

			        if result.attrib['strValue'] == "0":
			            eta_result[key] = "0"
			        elif result.attrib['strValue'] == "---":
			            eta_result[key] = "0"
			        else:
			            try:
			                eta_result[key] = float(result.attrib['strValue'].replace(',','.'))
			            except:
			                eta_result[key] = result.attrib['strValue']

			        # compare results and emit on change
			        get_diffs(eta_result,eta_result_report)

			except urllib2.HTTPError, e:
				print 'We failed with error code - %s.' % e.code

			except urllib2.URLError, e:
				print 'We failed with error - %s.' % e.reason

			time.sleep (60)
      
background = eta_event()
background.setDaemon(True)
background.start()

client.run()

