#!/usr/bin/env python

#update metadata of specified template
# - only update existing param, it never create new ones
# - search for metadata.json files only

import cgi
import json
import os
import re

def loadFile(f):
    content = {}
    try:
        with open(f) as data:
            content = data.read()
            content = json.loads(content)
    except Exception as error:
        #TODO log error
        return None
    return content

def saveFile(f, m):
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
result = {'result':0, 'error':'', 'content':''}
#allowed content: key=file, fields list with type (* means any field)
allowedContent = {'favorites':{'*':'bool'}}

key = None
param = None
value = None

#get parameters
args = cgi.FieldStorage()
for k in args.keys():
    if k=='key':
        if len(args[k].value.strip())>0:
            key = args[k].value.strip()
    elif k=='param':
        if len(args[k].value.strip())>0:
            param = args[k].value.strip()
    elif k=='value':
        if len(args[k].value.strip())>0:
            value = args[k].value.strip()

#get content
try:
    if key and param and value:
        #check file
        if param in allowedContent.keys():
            path = os.path.join('/etc/opt/agocontrol/ui/', param+'.json')
            if not os.path.exists(path):
                try:
                    f = open(path, 'w')
                    f.write('{}')
                    f.close()
                except Exception as e:
                    result['error'] = 'Unable to create "%s" [%s]' % (path, str(e))

            if os.path.exists(path):
                #load metadata
                metadata = loadFile(path)
                #save metadata
                castType = 'string'
                if allowedContent[param].has_key('*'):
                    castType = allowedContent[param]['*']
                elif allowedContent[param].has_key(key):
                    castType = allowedContent[param][key]
                else:
                    #default cast type (string)
                    pass
                metadata[key] = cast(value, castType)
                if saveFile(path, metadata):
                    result['result'] = 1
                else:
                    result['error'] = 'Unable to save %s.json file' % (param)
        else:
            result['error'] = '%s is not allowed' % (param)

    elif not key and param and not value:
        #get content
        if param in allowedContent.keys():
            path = os.path.join('/etc/opt/agocontrol/ui/', param+'.json')
            result['result'] = 1
            if not os.path.exists(path):
                result['content'] = {}
            else:
                result['content'] = loadFile(path)

    else:
        #missing parameter
        result['error'] = 'Missing parameter'

except Exception as e:
    #TODO add message to agolog
    pass

#send output
print "Content-type: application/json\n"
print json.dumps(result)

