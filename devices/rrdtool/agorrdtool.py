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
import base64
from qpid.datatypes import uuid4

RRD_PATH = agoclient.LOCALSTATEDIR 
client = None
server = None
units = {}
rrds = {}

#logging.basicConfig(filename='/opt/agocontrol/agoscheduler.log', level=logging.INFO, format="%(asctime)s %(levelname)s : %(message)s")
logging.basicConfig(level=logging.INFO, format="%(asctime)s %(name)s %(levelname)s : %(message)s")

#=================================
#classes
#=================================


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

def generateGraph(uuid, start, end):
    #init
    global rrds, units
    gfx = None
    error = False
    errorMsg = ''

    #get rrd filename according to uuid
    try:
        if rrds.has_key(uuid):
            #get graph infos
            (_, kind, vertical_unit) = rrds[uuid].replace('.rrd','').split('_')
            colorL = '#000000'
            colorA = '#A0A0A0'
            colorMax = '#FF0000'
            colorMin = '#00FF00'
            colorAvg = '#0000FF'
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
            logging.info('Generate graph: uuid=%s start=%s end=%s unit=%s kind=%s' % (uuid, str(start), str(end),str(vertical_unit), str(kind)))

            #fix unit if necessary
            if units.has_key(vertical_unit):
                vertical_unit = str(units[vertical_unit])
            if vertical_unit=='%':
                unit = '%%'
            else:
                unit = vertical_unit

            #generate graph
            rrd = RRDtool.RRD(str(rrds[uuid]))
            gfx = rrd.graph( None, "--start", "%d" % int(start), "--end", "%d" % int(end), "--vertical-label=%s" % str(vertical_unit),
                                "-w 850", "-h 300",
                                #"--alt-autoscale",
                                #"--alt-y-grid",
                                #"--rigid",
                                "DEF:level=%s:level:AVERAGE" % str(rrds[uuid]),
                                "VDEF:levelavg=level,AVERAGE",
                                "VDEF:levelmax=level,MAXIMUM",
                                "VDEF:levelmin=level,MINIMUM",
                                "AREA:level%s" % colorA,
                                "LINE1:level%s:%s" % (colorL, str(kind)),
                                "COMMENT:\\n",
                                "LINE1:levelavg%s:Average \::dashes" % colorAvg,
                                "GPRINT:level:AVERAGE:%0.2lf"+str(unit),
                                "COMMENT:\\n",
                                "LINE1:levelmax%s:Maximum \::dashes" % colorMax,
                                "GPRINT:level:MAX:%0.2lf"+str(unit),
                                "COMMENT:\\n",
                                "LINE1:levelmin%s:Minimum \::dashes" % colorMin,
                                "GPRINT:level:MIN:%0.2lf"+str(unit),
                                "COMMENT:\\n")
        
        else:
            #no file for specified uuid
            error = True
            errorMsg = 'No data for specified device'
    except:
        logging.exception('Error during graph generation:')
        error = True
        errorMsg = 'Internal error'
            
    return error, errorMsg, gfx


#=================================
#functions
#=================================
def commandHandler(internalid, content):
    """command handler"""
    #logging.info('commandHandler: %s, %s' % (internalid,content))
    global client
    command = None

    if content.has_key('command'):
        command = content['command']
    else:
        logging.error('No command specified')
        return None

    if internalid=='rrdtoolcontroller':
        if command=='getgraph' and checkContent(content, ['deviceUuid','start','end']):
            (error, msg, graph) = generateGraph(content['deviceUuid'], content['start'], content['end'])
            if not error:
                return {'error':0, 'msg':'', 'graph':base64.b64encode(graph)}
            else:
                return {'error':1, 'msg':msg}
    logging.warning('Unsupported command received: internalid=%s content=%s' % (internalid, content))
    return None

def eventHandler(event, content):
    """ago event handler"""
    global client, rrds

    #format event.environment.humiditychanged, {u'uuid': '506249e2-1852-4de7-8554-93f5b9354a20', u'unit': '', u'level': 49.8}
    if event.startswith('event.environment.') and checkContent(content, ['level', 'uuid']):
        #graphable data
        #logging.info('eventHandler: %s, %s' % (event, content))

        try:
            #generate rrd filename
            kind = event.replace('event.environment.', '').replace('changed', '')
            unit = ''
            if content.has_key('unit'):
                unit = content['unit']
            rrdfile = os.path.join(RRD_PATH, '%s_%s_%s.rrd' % (content['uuid'], kind, unit))

            #create rrd file if necessary
            if not os.path.exists(rrdfile):
                logging.info('New device detected, create rrd file %s' % rrdfile)
                ret = rrdtool.create(str(rrdfile), "--step", "60", "--start", "0",
                        "DS:level:GAUGE:21600:U:U",
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
            logging.info('Update rrdfile "%s" with level=%f' % (str(rrdfile), content['level']))
            ret = rrdtool.update(str(rrdfile), 'N:%f' % content['level']);
        except:
            logging.exception('Exception on eventHandler:')



#=================================
#main
#=================================
#init
try:
    #connect agoclient
    client = agoclient.AgoConnection('agorrdtool')

    #get units
    inventory = client.get_inventory()
    for unit in inventory.content['schema']['units']:
        units[unit] = inventory.content['schema']['units'][unit]['label']

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
    logging.info('Found rrd files:')
    for rrd in rrds.values():
        logging.info(' - %s' % rrd)

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

