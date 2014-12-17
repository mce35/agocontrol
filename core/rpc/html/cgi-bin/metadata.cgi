#!/usr/bin/env python

#update metadata of specified template
# - only update existing param, it never create new ones
# - search for metadata.json files only

import cgi
import json
import os
import re

def loadMetadataInFile(f):
    content = {}
    try:
        with open(f) as data:
            content = data.read()
            content = json.loads(content)
    except Exception as error:
        #TODO log error
        return None
    return content

def saveMetadataInFile(f, m):
    try:
        with open(os.path.join(f), 'w') as outfile:
            json.dump(m, outfile, sort_keys=True, indent=4, separators=(',', ': '))
    except Exception as error:
        #TODO log error
        return False
    return True

def cast(in_, format_):
    try:
        if format_=='bool':
            if in_=='false':
                return False
            elif in_=='true':
                return True
            else:
                return bool(in_)
        elif format_=='int':
            return int(in_)
        elif format_=='float':
            return float(in_)
        else:
            return str(in_)
    except:
        if format_=='bool':
            return False
        elif format_=='int':
            return 0
        elif format_=='float':
            return 0.0
        else:
            return ''
    return None

#globals
result = {'result':0, 'error':''}
allowedParams = {'favorite':'bool'}
type_ = None
module = None
param = None
value = None

#get parameters
args = cgi.FieldStorage()
for key in args.keys():
    if key=='type':
        if args[key].value in ['plugins', 'config', 'help']:
            type_ = args[key].value
    elif key=='module':
        if len(args[key].value.strip())>0:
            module = args[key].value.strip()
    elif key=='param':
        if len(args[key].value.strip())>0:
            param = args[key].value.strip()
    elif key=='value':
        if len(args[key].value.strip())>0:
            value = args[key].value.strip()

#get content
try:
    if type_ and module and param and value:
        #check if param update is allowed
        if param in allowedParams.keys():
            #build metadata.json file path
            path = os.path.join('/opt/agocontrol/html/', type_, module, 'metadata.json')
            if os.path.exists(path):
                #load metadata
                metadata = loadMetadataInFile(path)
                #set param
                if metadata.has_key(param):
                    metadata[param] = cast(value, allowedParams[param])
                    #save metadata
                    if saveMetadataInFile(path, metadata):
                        result['result'] = 1
                    else:
                        result['error'] = 'Unable to save metadata.json file'
                else:
                    result['error'] = 'Param "%s" doesn\'t exist in metadata.json file' % param
            else:
                #file not exists
                result['error'] = 'No metadata.json file for specified type/module'
        else:
            #param not allowed for update
            result['error'] = 'Parameter not allowed for modification'
    else:
        #type not specified
        result['error'] = 'Request parameter is missing'

except Exception as e:
    #TODO add message to agolog
    pass

#send output
print "Content-type: application/json\n"
print json.dumps(result)

