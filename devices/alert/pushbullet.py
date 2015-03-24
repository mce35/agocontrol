# -*- coding: utf-8 -*-
#try:
#    from urllib.request import Request, urlopen
#except:
#    from urllib2 import Request, urlopen

from base64 import encodestring, b64encode
import json
import mimetypes
import os
from mimetypes import MimeTypes
import requests
from requests.auth import HTTPBasicAuth

HOST = "https://api.pushbullet.com/v2";

class PushBulletError():
    def __init__(self, value):
        self.value = value

    def __str__(self):
        return self.value

class PushBullet():
    def __init__(self, apiKey):
        self.apiKey = apiKey

    def _request(self, url, payload=None, post=True):
        auth = HTTPBasicAuth(self.apiKey, '')
        headers = {'Content-Type': 'application/json'}
        if post:
            if payload:
                payload = json.dumps(payload)
                r = requests.post(url, data=payload, headers=headers, auth=auth)
            else:
                r = requests.post(url, headers=headers, auth=auth)
        else:
            if payload:
                payload = json.dumps(payload)
                r = requests.get(url, headers=headers, auth=auth)
            else:
                r = requests.get(url, data=payload, headers=headers, auth=auth)
        if r.status_code==200:
            return r.status_code, r.json
        else:
            return r.status_code, self.getResponseMessage(r.status_code)
        
    def _request_multiform(self, url, payload, files):
        return requests.post(url, data=payload, files=files)
        
    def getDevices(self):
        (s,r) = self._request(HOST + "/devices", None, False)
	if s==200 and r.has_key('devices'):
            return r['devices']
        else:
            return []
       
    #https://docs.pushbullet.com/#http
    def getResponseMessage(self, status):
        if status==200:
            return "OK"
        elif status==400:
            return "Bad request"
        elif status==401:
            return "Unauthorized"
        elif status==403:
            return "Forbidden"
        elif status==404:
            return "Not found"
        elif str(status).startswith("5"):
            return "Server error"
        else:
            return "Unknown"

    def pushNote(self, device, title, body):
        data = {'type'       : 'note',
                'device_iden': device,
                'title'      : title,
                'body'       : body}
        (s,r) = self._request(HOST + "/pushes", data)
        return r

    def pushAddress(self, device, name, address):
        data = {'type'       : 'address',
                'device_iden': device,
                'name'       : name,
                'address'    : address}
        (s,r) = self._request(HOST + "/pushes", data)
        return r

    def pushList(self, device, title, items):
        data = {'type'       : 'list',
                'device_iden': device,
                'title'      : title,
                'items'      : items}
        (s,r) = self._request(HOST + "/pushes", data)
        return r

    def pushLink(self, device, title, url):
        data = {'type'       : 'link',
                'device_iden': device,
                'title'      : title,
                'url'        : url}
        (s,r) = self._request(HOST + "/pushes", data)
        return r

    def pushFile(self, device, file):
        #get upload file authorization
        mimes = MimeTypes()
        mimetype = mimes.guess_type(file)[0]
        filename = os.path.basename(file)
        if not mimetype:
            mimetype = ''
        data = { 'file_name' : filename,
                 'file_type' : mimetype}
        try:
            authResp = self._request(HOST + "/upload-request", data)

            #upload file now
            resp = self._request_multiform(authResp['upload_url'], authResp['data'], {'file': open(file, 'rb')})
            if resp:
                if resp.status_code==204:
                    #file uploaded successfully, push file now
                    data = { 'type'     : 'file',
                             'file_name': filename,
                             'file_type': mimetype,
                             'file_url' : authResp['file_url']}
                    (s,r) = self._request(HOST + '/pushes', data)
                    return r
        except:
            return None

