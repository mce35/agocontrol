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
        auth = "%s:" % (self.apiKey)
        auth = auth.encode('ascii')
        auth = b64encode(auth)
        auth = b"Basic "+auth
        headers = {}
        headers['Accept'] = 'application/json'
        headers['Content-type'] = 'application/json'
        headers['Authorization'] = auth
        headers['User-Agent'] = 'pyPushBullet'
        if post:
            if payload:
                payload = json.dumps(payload)
                r = requests.post(url, data=payload, headers=headers)
            else:
                r = requests.post(url, headers=headers)
        else:
            if payload:
                payload = json.dumps(payload)
                r = requests.get(url, headers=headers)
            else:
                r = requests.get(url, data=payload, headers=headers)
        return r.json()
        
    def _request_multiform(self, url, payload, files):
        return requests.post(url, data=payload, files=files)
        
    def getDevices(self):
        r = self._request(HOST + "/devices", None, False)
        if r.has_key('devices'):
            return r['devices']
        else:
            return []

    def pushNote(self, device, title, body):
        data = {'type'       : 'note',
                'device_iden': device,
                'title'      : title,
                'body'       : body}
        return self._request(HOST + "/pushes", data)

    def pushAddress(self, device, name, address):
        data = {'type'       : 'address',
                'device_iden': device,
                'name'       : name,
                'address'    : address}
        return self._request(HOST + "/pushes", data)

    def pushList(self, device, title, items):
        data = {'type'       : 'list',
                'device_iden': device,
                'title'      : title,
                'items'      : items}
        return self._request(HOST + "/pushes", data)


    def pushLink(self, device, title, url):
        data = {'type'       : 'link',
                'device_iden': device,
                'title'      : title,
                'url'        : url}
        return self._request(HOST + "/pushes", data)

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
                    return self._request(HOST + '/pushes', data)
        except:
            return None

