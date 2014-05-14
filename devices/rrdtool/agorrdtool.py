#!/usr/bin/env python
# -*- coding: utf-8 -*-

# agorrdtool
# Addon that log environment event on rrdtool database
# It also generates on the fly rrdtool graphs
# copyright (c) 2014 tang (tanguy.bonneau@gmail.com) 
 
import sys
import os
import agoclient
import threading
import time
import logging
import json
import rrdtool
import RRDtool #@doc https://github.com/commx/python-rrdtool/blob/master/RRDtool.py
from BaseHTTPServer import BaseHTTPRequestHandler,HTTPServer
import urlparse

from qpid.datatypes import uuid4

HTTP_PORT = 8011
RRD_PATH = agoclient.LOCALSTATEDIR 
client = None
server = None
server_port = HTTP_PORT
rrds = {}

#logging.basicConfig(filename='/opt/agocontrol/agoscheduler.log', level=logging.INFO, format="%(asctime)s %(levelname)s : %(message)s")
logging.basicConfig(level=logging.INFO, format="%(asctime)s %(name)s %(levelname)s : %(message)s")

#=================================
#classes
#=================================
class RrdtoolGraphHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        #init
        error = 0

        #parse url then query string
        url = urlparse.urlparse(self.path)
        qs = urlparse.parse_qs(url.query)

        #generate graph
        gfx = None
        if len(url.query)>0 and qs.has_key('uuid'):
            #get rrd filename according to uuid
            uuid = qs['uuid'][0]
            if rrds.has_key(uuid):
                #get parameters
                start = ""
                if qs.has_key('start') and len(qs['start'])==1 and len(qs['start'][0])>0:
                    start = qs['start'][0]
                end = ""
                if qs.has_key('end') and len(qs['end'])==1 and len(qs['end'][0])>0:
                    end = qs['end'][0]
                if len(start)==0 or len(end)==0:
                    start = '-1d'
                    end = 'N'
                #get graph infos
                (_, kind, unit) = rrds[uuid].replace('.rrd','').split('_')
                colorL = '#000000'
                colorA = '#A0A0A0'
                if kind in ['humidity']:
                    #blue
                    colorL = '#0000FF'
                    colorA = '#7777FF'
                elif kind in ['temperature']:
                    #red
                    colorL = '#FF0000'
                    colorA = '#FF8787'
                elif kind in ['energy']:
                    #green
                    colorL = '#007A00'
                    colorA = '#00BB00'
                elif kind in ['brightness']:
                    #orange
                    colorL = '#CCAA00'
                    colorA = '#FFD400'

                logging.info('generate graph: uuid=%s start=%s end=%s unit=%s kind=%s' % (uuid, str(start), str(end),str(unit), str(kind)))
                #generate graph
                rrd = RRDtool.RRD(str(rrds[uuid]))
                gfx = rrd.graph( None, "--start", "epoch+%ds" % int(start), "--end", "epoch+%ds" % int(end), "--vertical-label=%s" % str(unit),
                                "-w 800", "-h 300",
                                #"--alt-autoscale",
                                #"--alt-y-grid",
                                #"--rigid",
                                "DEF:val=%s:level:AVERAGE" % str(rrds[uuid]),
                                "LINE2:val%s:%s" % (colorL, str(kind)),
                                "AREA:val%s" % colorA,
                                "GPRINT:val:AVERAGE:Avg\: %0.2lf ",
                                "GPRINT:val:MAX:Max\: %0.2lf ",
                                "GPRINT:val:MIN:Min\: %0.2lf")
            else:
                #no file
                error = 404
        else:
            #bad request
            error = 400
            
        #generate output
        if gfx:
            self.send_response(200)        
            self.send_header('Content-type', 'image/png')
            self.send_header('Content-Length', len(gfx))
            self.end_headers()
            self.wfile.write(gfx)
        else:
            #something failed
            self.send_error(error)
        return

    #overwrite log_message to avoid log flood
    def log_message(self, format, *args):
        return

class RrdtoolHttpServer(HTTPServer):
    allow_reuse_address = 1
    timeout = 1
    def __init__(self, server_address, RequestHandlerClass):
        HTTPServer.__init__(self, server_address, RequestHandlerClass)

class RrdtoolDaemon(threading.Thread):
    def __init__(self, port):
        threading.Thread.__init__(self)
        self.logger = logging.getLogger('RrdtoolDaemon')
        self.port = port
        self.__server = RrdtoolHttpServer(('', port), RrdtoolGraphHandler)
        self.__running = True

    def __del__(self):
        self.stop()

    def run(self):
        self.logger.info('Serving at localhost:%d' % self.port)
        try:
            while self.__running:
                self.__server.handle_request()
        except Exception as e:
            #may be stopped
            self.logger.exception('Exception in run():')

    def stop(self):
        """stop server"""
        self.logger.info('Stop')
        self.__running = False
        if self.__server:
            self.__server.socket.close()
            #sys.stdin.close()

#=================================
#utils
#=================================
def quit(msg):
    """Exit application"""
    global client, server
    if client:
        del client
        client = None
    if server:
        #server.socket.close()
        server.stop()
        del server
        server = None
    logging.fatal(msg)
    sys.exit(0)

def getScenarioControllerUuid():
    """get scenariocontroller uuid"""
    global client, scenarioControllerUuid
    inventory = client.get_inventory()
    for uuid in inventory.content['devices']:
        if inventory.content['devices'][uuid]['devicetype']=='scenariocontroller':
            scenarioControllerUuid = uuid
            break
    if not scenarioControllerUuid:
        raise Exception('scenariocontroller uuid not found!')


def checkContent(content, params):
    """Check if all params are in content"""
    for param in params:
        if not content.has_key(param):
            return False
    return True


#=================================
#functions
#=================================
def commandHandler(internalid, content):
    """command handler"""
    logging.info('commandHandler: %s, %s' % (internalid,content))
    global client, server_port
    command = None

    if content.has_key('command'):
        command = content['command']
    else:
        logging.error('No command specified')
        return None

    if internalid=='rrdtoolcontroller':
        if command=='getPort':
            return {'error':0, 'msg':'', 'port':server_port}
    return None

def eventHandler(event, content):
    """ago event handler"""
    global client, rrds

    #format event.environment.humiditychanged, {u'uuid': '506249e2-1852-4de7-8554-93f5b9354a20', u'unit': '', u'level': 49.8}
    if event.startswith('event.environment.') and checkContent(content, ['level', 'uuid']):
        #loggable data
        logging.info('eventHandler: %s, %s' % (event, content))

        try:
            #generate rrd filename
            kind = event.replace('event.environment.', '').replace('changed', '')
            unit = ''
            if content.has_key('unit'):
                unit = content['unit']
            rrdfile = os.path.join(RRD_PATH, '%s_%s_%s.rrd' % (content['uuid'], kind, unit))

            #create rrd file if necessary
            if not os.path.exists(rrdfile):
                logging.info('create rrd file %s' % rrdfile)
                ret = rrdtool.create(str(rrdfile), "--step", "60", "--start", "0",
                        "DS:level:GAUGE:3600:U:U",
                        "RRA:AVERAGE:0.5:1:1440",
                        "RRA:AVERAGE:0.5:5:2016",
                        "RRA:AVERAGE:0.5:30:1488",
                        "RRA:AVERAGE:0.5:60:8760",
                        "RRA:AVERAGE:0.5:360:2920",
                        "RRA:MIN:0.5:1:1440",
                        "RRA:MIN:0.5:5:2016",
                        "RRA:MIN:0.5:30:1488",
                        "RRA:MIN:0.5:60:8760",
                        "RRA:MIN:0.5:360:2920",
                        "RRA:MAX:0.5:1:1440",
                        "RRA:MAX:0.5:5:2016",
                        "RRA:MAX:0.5:30:1488",
                        "RRA:MAX:0.5:60:8760",
                        "RRA:MAX:0.5:360:2920")
                rrds[content['uuid']] = rrdfile

            #update rrd
            logging.info('level=%f' % content['level'])
            ret = rrdtool.update(str(rrdfile), 'N:%f' % content['level']);
            logging.info(rrds)
        except:
            logging.exception('Exception on eventHandler:')



#=================================
#main
#=================================
#init
try:
    #connect agoclient
    client = agoclient.AgoConnection('agorrdtool')

    #read config file
    server_port = agoclient.get_config_option("rrdtool", "port", HTTP_PORT)

    #get existing rrd files
    rrdfiles = os.listdir(RRD_PATH)
    for rrdfile in rrdfiles:
        if rrdfile.endswith('.rrd'):
            try:
                (uuid, kind, unit) = rrdfile.replace('.rrd','').split('_')
                rrds[uuid] = os.path.join(RRD_PATH,rrdfile)
            except:
                #bad rrd file
                pass
    logging.info(rrds)

    #create http server
    server = RrdtoolDaemon(server_port)
    server.start()

    #add client handlers
    client.add_handler(commandHandler)
    client.add_event_handler(eventHandler)

    #add controller
    client.add_device('rrdtoolcontroller', 'rrdtoolcontroller')

except Exception as e:
    #init failed
    logging.exception("Exception on init")
    quit('Init failed, exit now.')


#run agoclient
try:
    logging.info('Running agorrdtool...')
    client.run()
except KeyboardInterrupt:
    #stopped by user
    quit('agorrdtool stopped by user')
except Exception as e:
    logging.exception("Exception on main:")
    #stop everything
    quit('agorrdtool stopped')

