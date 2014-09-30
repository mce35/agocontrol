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
import colorsys
from qpid.datatypes import uuid4

RRD_PATH = agoclient.LOCALSTATEDIR 
client = None
server = None
units = {}
rrds = {}
multi = {}

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

def rainbow_colors(n):
    HSV_tuples = [(x*1.0/n, 1, 1) for x in range(n)]
    RGB_tuples = map(lambda x: colorsys.hsv_to_rgb(*x), HSV_tuples)

    ret = []
    for t in RGB_tuples:
        ret.append("%02X%02X%02X" % (t[0] * 255, t[1] * 255, t[2] * 255))
    return ret

def generateGraph(uuids, start, end, legend):
    #init
    global rrds, units
    gfx = None
    error = False
    errorMsg = ''

    colors = rainbow_colors(len(uuids))

    #get rrd filename according to uuid
    try:
        rrd_args = [ "-D", "--start", "%d" % int(start), "--end", "%d" % int(end),
                     "-w 900", "-h 380",
                     #"--alt-autoscale",
                     #"--alt-y-grid",
                     #"--rigid"
                     ]
        if legend == False:
            rrd_args.append("-g")
        rrd_lines = []
        graph_nb = 0
        inventory = client.get_inventory()
        colorMax = '#FF0000'
        colorMin = '#00FF00'
        colorAvg = '#0000FF'

        for uuid in uuids:
            if not rrds.has_key(uuid):
                continue
            #get graph infos
            (_, kind, vertical_unit) = rrds[uuid].replace('.rrd','').split('_')

            colorL = "#%s" % colors[graph_nb]
            colorA = '#%s10' % colors[graph_nb]

            logging.info('Generate graph: uuid=%s start=%s end=%s unit=%s kind=%s' % (uuid, str(start), str(end),str(vertical_unit), str(kind)))

            #fix unit if necessary
            if units.has_key(vertical_unit):
                vertical_unit = str(units[vertical_unit])
            if vertical_unit=='%':
                unit = '%%'
            else:
                unit = vertical_unit
            if graph_nb == 0 and legend == True:
                rrd_args.append("--vertical-label=%s" % str(vertical_unit))

            uni_name_fixed = inventory.content['devices'][uuid]['name']
            if(len(uni_name_fixed) > 25):
                uni_name_fixed = uni_name_fixed[0:24]
            else:
                while(len(uni_name_fixed) < 25):
                    uni_name_fixed += ' '
            rrd_args.extend([ "DEF:level%d=%s:level:AVERAGE" % (graph_nb,str(rrds[uuid])),
                              "VDEF:levellast%d=level%d,LAST" % (graph_nb, graph_nb),
                              "VDEF:levelavg%d=level%d,AVERAGE" % (graph_nb, graph_nb),
                              "VDEF:levelmax%d=level%d,MAXIMUM" % (graph_nb, graph_nb),
                              "VDEF:levelmin%d=level%d,MINIMUM" % (graph_nb, graph_nb),
                              "AREA:level%d%s" % (graph_nb, colorA) ])
            rrd_lines.append("LINE1:level%d%s:%s" % (graph_nb, colorL, uni_name_fixed.encode('utf8')))
            if(len(uuids) == 1):
                rrd_lines.extend([ "LINE1:levelmin%d%s:Min:dashes" % (graph_nb, colorAvg),
                                   "GPRINT:levelmin%d:%%6.2lf%%s%s" % (graph_nb, unit),
                                   "LINE1:levelmax%d%s:Max:dashes" % (graph_nb, colorMax),
                                   "GPRINT:levelmax%d:%%6.2lf%%s%s" % (graph_nb, unit),
                                   "LINE1:levelavg%d%s:Avg:dashes" % (graph_nb, colorMin),
                                   "GPRINT:levelavg%d:%%6.2lf%%s%s" % (graph_nb, unit) ])
            else:
                rrd_lines.extend([ "GPRINT:levelmin%d: Min %%6.2lf%%s%s" % (graph_nb, unit),
                                   "GPRINT:levelmax%d: Max %%6.2lf%%s%s" % (graph_nb, unit),
                                   "GPRINT:levelavg%d: Avg %%6.2lf%%s%s" % (graph_nb, unit) ])
            rrd_lines.extend([ "GPRINT:levellast%d: Last %%6.2lf%%s%s" % (graph_nb, unit),
                               "COMMENT:\\n" ])
            graph_nb += 1

        if(graph_nb > 0):
            #generate graph
            rrd = RRDtool.RRD(str(rrds[uuid]))
            gfx = rrd.graph( None, rrd_args, rrd_lines)
        else:
            #no file for specified uuids
            error = True
            errorMsg = 'No data for specified device(s)'
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
            if multi.has_key(content['deviceUuid']):
                uuids = multi[content['deviceUuid']]['uuids']
            else:
                uuids = [ content['deviceUuid'] ]
            (error, msg, graph) = generateGraph(uuids, content['start'], content['end'], True)
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
    if (event=='event.device.batterylevelchanged' or event.startswith('event.environment.')) and checkContent(content, ['level', 'uuid']):
        #graphable data
        #logging.info('eventHandler: %s, %s' % (event, content))

        try:
            #generate rrd filename
            kind = event.replace('event.environment.', '').replace('event.device.', '').replace('changed', '')
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

    try:
        multi_nb = int(agoclient.get_config_option("multi", "nb_multi", "", "rrd"))
    except ValueError:
        multi_nb = 0
    for i in range(multi_nb):
        key = "multi%d" % i
        uuids = agoclient.get_config_option("multi", key, "", "rrd")
        if uuids != "":
            uuids_list = uuids.split(",")
            client.add_device(key, 'multisensor')
            uuid = client.internal_id_to_uuid(key)
            multi[uuid] = {}
            multi[uuid]['internalid'] = key
            multi[uuid]['uuids'] = uuids_list

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

