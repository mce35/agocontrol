#!/usr/bin/env python

#Returns all or specified directory structure
#@params get: all|plugins|configs|helps|supported
# - all: returns all items
# - plugins: returns plugins list
# - config: returns configuration pages
# - help: returns help pages
# - supported: returns supported devices

import cgi
import json
import os
import re

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
                        obj["_name"] = os.path.basename(path)
                        items[obj["_name"]] = obj
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

#globals
result = {}
get = 'all'

#get parameters
args = cgi.FieldStorage()
for key in args.keys():
    if key=='get':
        if args[key].value in ['all', 'plugins', 'config', 'help', 'supported']:
            get = args[key].value
            break

#get content
try:
    if get=='all':
        result['plugins'] = loadMetadatasInDir('../plugins/')
        result['config'] = loadMetadatasInDir('../configuration/')
        result['help'] = loadMetadatasInDir('../help/')
        result['supported'] = getSupportedDevices()
    elif get=='config':
        result = loadMetadatasInDir('../configuration/')
    elif get=='help':
        result = loadMetadatasInDir('../help/')
    elif get=='plugins':
        result = loadMetadatasInDir('../plugins/')
    elif get=='supported':
        result = getSupportedDevices()

except Exception as e:
    #TODO add message to agolog
    pass

#send output
print "Content-type: application/json\n"
print json.dumps(result)

