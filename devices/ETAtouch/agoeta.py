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
devices = {}

class AgoEta(agoclient.AgoApp):
    def setup_app(self):
        # read IP from config file
        eta_ip = self.get_config_option("IP", "127.0.0.1")
        self.log.info("ETA IP-Address is: %s", eta_ip)

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

                    # check when we get a new value and announce it
                    for a_key in eta_result:
                        # check if key from a exists in b
                        if a_key not in eta_result_report:
                            eta_result_report[a_key] = eta_result[a_key]
                            emit_result(self, a_key)

                        elif eta_result[a_key] != eta_result_report[a_key]:
                            eta_result_report[a_key] = eta_result[a_key]
                            emit_result(self, a_key)


            except urllib2.HTTPError, e:
                print 'We failed with error code - %s.' % e.code

            except urllib2.URLError, e:
                print 'We failed with error - %s.' % e.reason

            time.sleep(10)

if __name__ == "__main__":
    AgoEta().main()

