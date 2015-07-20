#!/usr/bin/env python

#Returns all or specified directory structure
#@params get: all|plugins|configs|helps|supported
# - all: returns all items
# - applications: returns applications list
# - config: returns configuration pages
# - help: returns help pages
# - supported: returns supported devices
# - time : returns current server timestamp

import cgi
import json
import os
import re
import time
import sqlite3

def loadMetadatasInDir(d):
    items = {}
    out = []
    for path, dirs, files in os.walk(d):
        for fn in files:
            if fn == "metadata.json":
                try:
                    with open(path + "/" + fn) as data:
                        content = data.read()
                        obj = json.loads(content)
                        obj["dir"] = os.path.basename(path)
                        items[obj["dir"]] = obj
                except Exception as error:
                    pass
    for key in sorted(items.iterkeys()):
        out.append(items[key])
    return out

def getSupportedDevices():
    pattern = re.compile("^([^.]+)\.html$")
    result = []
    for path, dirs, files in os.walk('../templates/devices'):
        for fn in files:
            match = pattern.findall(fn)
            if len(match) > 0:
                result.append(match[0])
    return result

def getServerTime():
    return int(time.time())

#globals
result = {}
get = 'all'

#get parameters
args = cgi.FieldStorage()
for key in args.keys():
    if key=='get':
        if args[key].value in ['all', 'applications', 'config', 'help', 'supported']:
            get = args[key].value
            break

#get content
try:
    if get=='all':
        result['applications'] = loadMetadatasInDir('../applications/')
        result['config'] = loadMetadatasInDir('../configuration/')
        result['help'] = loadMetadatasInDir('../help/')
        result['supported'] = getSupportedDevices()
        result['server_time'] = getServerTime()
    elif get=='config':
        result = loadMetadatasInDir('../configuration/')
    elif get=='help':
        result = loadMetadatasInDir('../help/')
    elif get=='applications':
        result = loadMetadatasInDir('../applications/')
    elif get=='supported':
        result = getSupportedDevices()
    elif get=='time':
        result = getServerTime()

except Exception as e:
    #TODO add message to agolog
    pass

#send output
print "Content-type: application/json\n"
print json.dumps(result)

