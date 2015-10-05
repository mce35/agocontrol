#! /usr/bin/python
# ETAtouch REST API support for ago control
# More information about ETAtouch: http://www.eta.co.at/255.0.html
#
# copyright (c) 2015 Christoph Jaeger <jaeger@agocontrol.com>
#

import agoclient
import threading
import time
import os
import json
import urllib2
import xml.etree.ElementTree as ET

eta_result = {}
eta_result_report = {}
eta_result_timer = {}
devices = {}

class AgoEta(agoclient.AgoApp):
    def setup_app(self):
        # read IP from config file
        eta_ip = self.get_config_option("IP", "127.0.0.1")
        poll_delay = self.get_config_option("poll_delay", "60")
        result_delay = self.get_config_option("result_delay", "3600")
        self.log.debug("ETA IP-Address is: %s", eta_ip)
        self.log.debug("ETA poll delay is: %s", poll_delay)
        self.log.debug("ETA min. report result delay is: %s", result_delay)
        global my_poll_delay
        my_poll_delay = float(poll_delay)
        global my_result_delay
        my_result_delay = float(result_delay)

        # define Base URL
        global base_url
        base_url = 'http://' + eta_ip + ':8080/user/var/'

        # JSON Device Config 
        devices_config_file = agoclient.config.get_config_path('maps/eta.json')
        if os.path.isfile(devices_config_file) and os.access(devices_config_file, os.R_OK):
            self.log.debug("JSON config file %s exists" % devices_config_file)
            with open(devices_config_file) as devices_config_file_json:
                devices_config = json.load(devices_config_file_json)
                global devices
                devices = devices_config['devices']
                for device in devices:
                    value = devices[device]
                    self.log.debug("ID=({}) URL=({}) DEVICETYPE=({})".format(device, devices[device]["url"], devices[device]["devicetype"]))
                    self.connection.add_device(device, devices[device]["devicetype"])
        else:
            self.log.warning("JSON config file %s NOT exist" % DevicesConfigFile)

        BACKGROUND = AgoEtaEvent(self)
        BACKGROUND.setDaemon(True)
        BACKGROUND.start()

    def app_cleanup(self):
        pass


def emit_result(self,key):
    if devices[key]["devicetype"] == "multilevelsensor":
        self.app.connection.emit_event(key, "event.environment.counterchanged", eta_result_report[key], "percent")
    else:
        self.app.connection.emit_event(key, "event.environment.temperaturechanged", eta_result_report[key], "degC")

class AgoEtaEvent(threading.Thread):
    def __init__(self, app):
        threading.Thread.__init__(self)
        self.app = app

    def run(self):
        level = 0
        while not self.app.is_exit_signaled():
            try:
                for device in devices:
                    request = urllib2.urlopen(base_url + devices[device]["url"])
                    tree = ET.parse(request)
                    root = tree.getroot()
                    result = root.find("{http://www.eta.co.at/rest/v1}value")
                    
                    if result.attrib['strValue'] == "0":
                        eta_result[device] = "0"
                    elif result.attrib['strValue'] == "---":
                        eta_result[device] = "0"
                    else:
                        try:
                            eta_result[device] = float(result.attrib['strValue'].replace(',','.'))
                        except:
                            eta_result[device] = result.attrib['strValue']

                    # check result timer
                    if device not in eta_result_timer:
                        eta_result_timer[device] = my_poll_delay
                    elif eta_result_timer[device] > my_result_delay:
                        eta_result_timer[device] = my_poll_delay 
                        emit_result(self, device)
                    elif eta_result_timer[device] >= my_poll_delay:
                        eta_result_timer[device] = eta_result_timer[device] + my_poll_delay 


                    # check when we get a new value and announce it
                    for a_key in eta_result:
                        # check if key from a exists in b
                        if a_key not in eta_result_report:
                            eta_result_report[a_key] = eta_result[a_key]
                            emit_result(self, a_key)
                            eta_result_timer[device] = my_poll_delay 

                        elif eta_result[a_key] != eta_result_report[a_key]:
                            eta_result_report[a_key] = eta_result[a_key]
                            emit_result(self, a_key)
                            eta_result_timer[device] = my_poll_delay 


            except urllib2.HTTPError, e:
                print 'We failed with error code - %s.' % e.code

            except urllib2.URLError, e:
                print 'We failed with error - %s.' % e.reason

            time.sleep(my_poll_delay)

if __name__ == "__main__":
    AgoEta().main()

