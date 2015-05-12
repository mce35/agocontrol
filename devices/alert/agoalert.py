#! /usr/bin/python
# -*- coding: utf-8 -*-

# Agoalert
# copyright (c) 2013 tang
 
import sys
import agoclient
import threading
import time
from Queue import Queue
import os
#twitter libs
import tweepy
#mail libs
import smtplib
import mimetypes
from email import encoders
from email.message import Message
from email.mime.audio import MIMEAudio
from email.mime.base import MIMEBase
from email.mime.image import MIMEImage
from email.mime.multipart import MIMEMultipart
from email.mime.text import MIMEText
#sms libs
import urllib
import urllib2
import ssl
#pushover
import httplib
import json
#pushbullet
from pushbullet import PushBullet
#notifymyandroid
from xml.dom import minidom

#=================================
#classes
#=================================
class Alert(threading.Thread):
    """
    Base class for agoalert message
    """
    def __init__(self, log, save_config_callback):
        threading.Thread.__init__(self)
        self.__running = True
        self.__queue = Queue()
        self.name = ''
        self.log = log
        self.save_config = save_config_callback

    def __del__(self):
        self.stop()

    def stop(self):
        self.__running = False

    def add_message(self, message):
        """
        Queue specified message
        """
        self.__queue.put(message)

    def get_config(self):
        """
        Return if module is configured or not
        """
        raise NotImplementedError('get_config method must be implemented')

    def prepare_message(self, content):
        """
        Return a message like expected
        @param content: is a map
        """
        raise NotImplementedError('prepare_message method must be implemented')

    def test(self, message):
        """
        Test sending message
        """
        error = 0
        msg = ''
        try:
            (r, m) = self._sendMessage(message)
            if not r:
                error = 1
                msg = m
        except Exception as e:
            self.log.exception('Error testing alert:')
            error = 1
            msg = str(e.message)
            if len(msg)==0:
                if getattr(e, 'reason', None):
                    msg = str(e.reason)
                else:
                    msg = '<No message>'
        return error, msg

    def _sendMessage(self, message):
        """
        Send message
        """
        raise NotImplementedError('_sendMessage method must be implemented')

    def run(self):
        """
        Main process
        """
        while self.__running:
            if not self.__queue.empty():
                #message to send
                message = self.__queue.get()
                self.log.info('send message: %s' % str(message))
                try:
                    self._sendMessage(message)
                except Exception as e:
                    self.log.exception('Unable to send message:')
            #pause
            time.sleep(0.25)

class Dummy(Alert):
    """
    Do nothing
    """
    def __init__(self, log, save_config_callback):
        Alert.__init__(self, log, save_config_callback)

    def get_config(self):
        return {'configured':0}

    def prepare_message(self, content):
        pass

    def _sendMessage(self, message):
        pass

class SMSFreeMobile(Alert):
    """
    Class to send text message (SMS) using 12voip.com provider
    """
    def __init__(self, log, save_config_callback, user, apikey):
        """
        Contructor
        """
        Alert.__init__(self, log, save_config_callback)
        self.user = user
        self.apikey = apikey
        self.name = 'freemobile'
        if user and len(user)>0 and apikey and len(apikey)>0:
            self.__configured = True
        else:
            self.__configured = False

    def get_config(self):
        configured = 0
        if self.__configured:
            configured = 1
        return {'configured':configured, 'user':self.user, 'apikey':self.apikey, 'provider':self.name}

    def set_config(self, user, apikey):
        """
        Set config
        @param user: freemobile user
        @param apikey: freemobile apikey
        """
        if not user or len(user)==0 or not apikey or len(apikey)==0:
            self.log.error('SMSFreeMobile: invalid parameters')
            return False

        self.user = user
        self.apikey = apikey
        self.__configured = True
        return True

    def prepare_message(self, content):
        """
        Prepare SMS message
        """
        if self.__configured:
            #check parameters
            if not content['text'] or len(content['text'])==0:
                msg = 'Unable to add SMS because all parameters are mandatory'
                self.log.error('SMSFreeMobile: %s' % msg)
                return None, msg
            if len(content['text'])>160:
                self.log.warning('SMSFreeMobile: SMS is too long, message will be truncated')
                content['text'] = content['text'][:159]
            #queue sms
            return {'to':content['to'], 'text':content['text']}, ''
        else:
            msg = 'Unable to add SMS because not configured'
            self.log.error('SMSFreeMobile: %s' % msg)
            return None, msg

    def _sendMessage(self, message):
        """
        Url format:
        https://smsapi.free-mobile.fr/sendmsg?user=<user>&pass=<apikey>&msg=<message>
        """
        params = {'user':self.user, 'pass':self.apikey, 'msg':message['text']}
        url = 'https://smsapi.free-mobile.fr/sendmsg?'
        url += urllib.urlencode(params)
        gcontext = ssl.SSLContext(ssl.PROTOCOL_TLSv1)  # Only for gangstars
        req = urllib2.urlopen(url, context=gcontext)
        lines = req.readlines()
        status = req.getcode()
        req.close()
        self.log.debug('url=%s status=%s' % (url, str(status)))
        if status==200:
            return True, 'SMS sent successfully'
        else:
            return False, status

class SMS12voip(Alert):
    """
    Class to send text message (SMS) using 12voip.com provider
    """
    def __init__(self, log, save_config_callback, username, password):
        """
        Contructor
        """
        Alert.__init__(self, log, save_config_callback)
        self.username = username
        self.password = password
        self.name = '12voip'
        if username and len(username)>0 and password and len(password)>0:
            self.__configured = True
        else:
            self.__configured = False

    def get_config(self):
        configured = 0
        if self.__configured:
            configured = 1
        return {'configured':configured, 'username':self.username, 'password':self.password, 'provider':self.name}

    def set_config(self, username, password):
        """
        Set config
        @param username: 12voip username
        @param password: 12voip password
        """
        if not username or len(username)==0 or not password or len(password)==0:
            self.log.error('SMS12voip: invalid parameters')
            return False

        self.username = username
        self.password = password
        self.__configured = True
        return True

    def prepare_message(self, content):
        """
        Add SMS
        """
        if self.__configured:
            #check parameters
            if not content['to'] or not content['text'] or len(content['to'])==0 or len(content['text'])==0:
                msg = 'Unable to add SMS because all parameters are mandatory'
                self.log.error('SMS12voip: %s' % msg)
                return None, msg
            if not content['to'].startswith('+') and content['to']!=self.username:
                msg = 'Unable to add SMS because "to" number must be international number'
                self.log.error('SMS12voip: %s' % msg)
                return None, msg
            if len(content['text'])>160:
                self.log.warning('SMS12voip: SMS is too long, message will be truncated')
                content['text'] = content['text'][:159]
            #queue sms
            return {'to':content['to'], 'text':content['text']}, ''
        else:
            msg = 'Unable to add SMS because not configured'
            self.log.error('SMS12voip: %s' % msg)
            return None, msg

    def _sendMessage(self, message):
        """
        Url format:
        https://www.12voip.com/myaccount/sendsms.php?username=xxxxxxxxxx&password=xxxxxxxxxx&from=xxxxxxxxxx&to=xxxxxxxxxx&text=xxxxxxxxxx
        """ 
        params = {'username':self.username, 'password':self.password, 'from':self.username, 'to':message['to'], 'text':message['text']}
        url = 'https://www.12voip.com/myaccount/sendsms.php?'
        url += urllib.urlencode(params)
        gcontext = ssl.SSLContext(ssl.PROTOCOL_TLSv1)  # Only for gangstars
        req = urllib2.urlopen(url, context=gcontext)
        lines = req.readlines()
        status = req.getcode()
        req.close()
        self.log.debug('url=%s status=%s' % (url, str(status)))
        if status==200:
            return True, 'SMS sent successfully'
        else:
            return False, status

class Twitter(Alert):
    """
    Class for tweet sending
    """
    def __init__(self, log, save_config_callback, key, secret):
        """
        Constructor
        """
        Alert.__init__(self, log, save_config_callback)
        self.consumerKey = '8SwEenXApf9NNpufRk171g'
        self.consumerSecret = 'VoVGMLU63VThwRBiC1uwN3asR5fqblHBQyn8EZq2Q'
        self.name = 'twitter'
        self.__auth = None
        self.key = key
        self.secret = secret
        if key and len(key)>0 and secret and len(secret)>0:
            self.__configured = True
        else:
            self.__configured = False

    def get_config(self):
        configured = 0
        if self.__configured:
            configured = 1
        return {'configured':configured}

    def get_key_secret(self, code):
        """
        Set twitter access code to get user key and secret
        @param code: code provided by twitter
        @return True/False, key, secret, error message
        """
        if code and len(code)>0:
            try:
                if not self.__auth:
                    self.__auth = tweepy.OAuthHandler(self.consumerKey, self.consumerSecret)
                    self.__auth.secure = True
                #get token
                token = self.__auth.get_access_token(code)
                #save token internally
                self.key = token.key
                self.secret = token.secret
                #return key and secret token
                return True, self.key, self.secret, ''
            except Exception as e:
                self.log.exception('Twitter: get_key_secret exception:')
                return False, None, None, 'Internal error'
        else:
            msg = 'Code is mandatory to get key and secret token'
            self.log.error('Twitter: %s' % msg)
            return False, None, None, msg

    def get_authorization_url(self):
        """
        Get twitter authorization url
        """
        try:
            if not self.__auth:
                self.__auth = tweepy.OAuthHandler(self.consumerKey, self.consumerSecret)
                self.__auth.secure = True
            url = self.__auth.get_authorization_url()
            self.log.trace('twitter url=%s' % url)
            return {'error':0, 'url':url}
        except Exception as e:
            self.log.exception('Twitter: Unable to get Twitter authorization url [%s]' % str(e) ) 
            return {'error':1, 'url':''}

    def prepare_message(self, content):
        """
        Prepare tweet message
        """
        if self.__configured:
            #check parameters
            if not content['tweet'] and len(content['tweet'])==0:
                msg = 'Unable to add tweet (all parameters are mandatory)'
                self.log.error('Twitter: %s' % msg)
                return None, msg
            if len(content['tweet'])>140:
                self.log.warning('Twitter: Tweet is too long, message will be truncated')
                content['tweet'] = content['tweet'][:139]
            #queue message
            return {'tweet':content['tweet']}, ''
        else:
            msg = 'Unable to add tweet because not configured'
            self.log.error('Twitter: %s' % msg)
            return None, msg

    def _sendMessage(self, message):
        #connect using OAuth auth (basic auth deprecated)
        auth = tweepy.OAuthHandler(self.consumerKey, self.consumerSecret)
        auth.secure = True
        self.log.debug('key=%s secret=%s' % (self.key, self.secret))
        auth.set_access_token(self.key, self.secret)
        api = tweepy.API(auth)
        res = api.update_status(message['tweet'])
        self.log.debug(res);
        #check result
        if res.text.find(message['tweet'])!=-1:
            return True, 'Tweet successful'
        else:
            return False, 'Message not tweeted'

class Mail(Alert):
    """
    Class for mail sending
    """
    def __init__(self, log, save_config_callback, smtp, sender, login, password, tls):
        """
        Constructor
        """
        Alert.__init__(self, log, save_config_callback)
        self.smtp = smtp
        self.sender = sender
        self.login = login
        self.password = password
        self.tls = False
        if tls=='1':
            self.tls = True
        self.name = 'mail'
        if smtp and len(smtp)>0 and sender and len(sender)>0:
            self.__configured = True
        else:
            self.__configured = False

    def get_config(self):
        configured = 0
        if self.__configured:
            configured = 1
        tls = 0
        if self.tls:
            tls = 1
        return {'configured':configured, 'sender':self.sender, 'smtp':self.smtp, 'login':self.login, 'password':self.password, 'tls':tls}

    def set_config(self, smtp, sender, login, password, tls):
        """
        Set config
        @param smtp: smtp server address
        @param sender: mail sender
        """ 
        if not smtp or len(smtp)==0 or not sender or len(sender)==0:
            self.log.error('Mail: all parameters are mandatory')
            return False

        self.smtp = smtp
        self.sender = sender
        self.login = login
        self.password = password
        self.tls = False
        if tls=='1':
            self.tls = True
        self.__configured = True
        return True

    def prepare_message(self, content):
        """
        Prepare mail message
        @param subject: mail subject
        @param tos: send mail to list of tos
        @param content: mail content
        @param attachment: mail attachment
        """
        if self.__configured:
            #check params
            if not content.has_key('tos') or not content.has_key('body') or (content.has_key('tos') and len(content['tos'])==0) or (content.has_key('body') and len(content['body'])==0):
                msg = 'Unable to add mail (all parameters are mandatory)'
                self.log.error('Mail: %s' % msg)
                return None, msg
            if not content.has_key('subject') or (content.has_key('subject') and len(content['subject'])==0):
                #no subject specified, add default one
                content['subject'] = 'Agocontrol alert'
            #queue mail
            return {'subject':content['subject'], 'tos':content['tos'], 'content':content['body'], 'attachment':content['attachment']}, ''
        else:
            msg = 'Unable to add mail because not configured'
            self.log.error('Mail: %s' % msg)
            return None, msg

    def _sendMessage(self, message):
        mails = smtplib.SMTP(self.smtp)
        if self.tls:
            mails.starttls()
        if len(self.login)>0:
            mails.login(self.login, self.password)
        mail = MIMEMultipart('alternative')
        mail['Subject'] = message['subject']
        mail['From'] = self.sender
        mail['To'] = message['tos'][0]
        text = """%s""" % (message['content'])
        html  = "<html><head></head><body>%s</body>" % (message['content'])
        part1 = MIMEText(text, 'plain')
        part2 = MIMEText(html, 'html')
        mail.attach(part1)
        mail.attach(part2)

        #append attachment
        #@see https://docs.python.org/2/library/email-examples.html
        if message.has_key('attachment') and message['attachment'] and len(message['attachment'])>0:
            #there is something to attach
            #file exists?
            if os.path.isfile(message['attachment']):
                ctype, encoding = mimetypes.guess_type(message['attachment'])
                if ctype is None or encoding is not None:
                    ctype = 'application/octet-stream'
                maintype, subtype = ctype.split('/', 1)
                if maintype == 'text':
                    fp = open(message['attachment'])
                    msg = MIMEText(fp.read(), _subtype=subtype)
                    fp.close()
                elif maintype == 'image':
                    fp = open(message['attachment'], 'rb')
                    msg = MIMEImage(fp.read(), _subtype=subtype)
                    fp.close()
                elif maintype == 'audio':
                    fp = open(message['attachment'], 'rb')
                    msg = MIMEAudio(fp.read(), _subtype=subtype)
                    fp.close()
                else:
                    fp = open(message['attachment'], 'rb')
                    msg = MIMEBase(maintype, subtype)
                    msg.set_payload(fp.read())
                    fp.close()
                    #encode the payload using Base64
                    encoders.encode_base64(msg)
                #set the filename parameter
                msg.add_header('Content-Disposition', 'attachment', filename=os.path.basename(message['attachment']))
                mail.attach(msg)

        mails.sendmail(self.sender, message['tos'], mail.as_string())
        mails.quit()
        #if error occured, exception is throw, so return true is always true if mail succeed
        return True, 'Mail sent successfully'

class Pushover(Alert):
    """
    Class for push message sending for ios and android
    """
    def __init__(self, log, save_config_callback, userid, token):
        """
        Constructor
        @see https://pushover.net/
        """
        Alert.__init__(self, log, save_config_callback)
        self.name = 'pushover'
        self.token = token
        self.userid = userid
        self.pushTitle = 'Agocontrol'
        if userid and len(userid)>0:
            self.__configured = True
        else:
            self.__configured = False

    def get_config(self):
        configured = 0
        if self.__configured:
            configured = 1
        return {'configured':configured, 'userid':self.userid, 'provider':self.name, 'token':self.token}

    def set_config(self, userid, token):
        """
        Set config
        @param userid: pushover userid (available on https://pushover.net/dashboard)
        @param token: pushover app token
        """
        if not userid or len(userid)==0 or not token or len(token)==0:
            self.log.error('Pushover: all parameters are mandatory')
            return False

        self.userid = userid
        self.token = token
        self.__configured = True
        return True

    def prepare_message(self, content):
        """
        Add push
        @param message: push notification
        """
        if self.__configured:
            #check params
            if not content['message'] or len(content['message'])==0 or not content.has_key('priority'):
                msg = 'Unable to add push (all parameters are mandatory)'
                self.log.error('Pushover: %s' % msg)
                return None, msg
            #queue push message
            return {'message':content['message'], 'priority':content['priority']}, ''
        else:
            msg = 'Unable to add message because not configured'
            self.log.error('Pushover: %s' % msg)
            return None, msg

    def _sendMessage(self, message):
        conn = httplib.HTTPSConnection("api.pushover.net:443")
        conn.request("POST", "/1/messages.json",
        urllib.urlencode({
            'token': self.token,
            'user':self.userid,
            'message':message['message'],
            'priority':message['priority'],
            'title':self.pushTitle,
            'timestamp': int(time.time())
        }), { "Content-type": "application/x-www-form-urlencoded" })
        resp = conn.getresponse()
        self.log.info(resp)
        #check response
        if resp:
            try:
                read = resp.read()
                self.log.debug(read)
                resp = json.loads(read)
                #TODO handle receipt https://pushover.net/api#receipt
                if resp['status']==0:
                    #error occured
                    self.log.error('Pushover: %s' % (str(resp['errors'])))
                    return False, str(resp['errors'])
                else:
                    self.log.info('Pushover: message received by user')
                    return True, 'Message pushed successfully'
            except:
                self.log.exception('Pushover: Exception push message')
                return False, str(e.message)

class Pushbullet(Alert):
    """
    Class for push message sending for ios and android
    @info https://www.pushbullet.com
    """
    def __init__(self, log, save_config_callback, apikey, devices):
        """
        Constructor
        """
        Alert.__init__(self, log, save_config_callback)
        self.name = 'pushbullet'
        self.pushbullet = None
        self.apikey = apikey
        if len(devices)==0:
            self.devices = []
        else:
            self.devices = json.loads(devices)
        self.pbdevices = {}
        self.pushTitle = 'Agocontrol'
        if apikey and len(apikey)>0 and devices and len(devices)>0:
            self.__configured = True
            self.pushbullet = PushBullet(self.apikey)
        else:
            self.__configured = False

    def get_config(self):
        configured = 0
        if self.__configured:
            configured = 1
        return {'configured':configured, 'apikey':self.apikey, 'devices':self.devices, 'provider':self.name}

    def getPushbulletDevices(self, apikey=None):
        """
        Request pushbullet to get its devices
        """
        if not self.__configured and not apikey:
            self.log.error('Pushbullet: unable to get devices. Not configured and no apikey specified')
            return {}
        
        if apikey:
            self.pushbullet = PushBullet(apikey)

        devices = []
        if self.pushbullet:
            devs = self.pushbullet.getDevices()
            self.log.debug('pushbullet devs=%s' % str(devs))
            for dev in devs:
                name = '%s %s (%s)' % (dev['manufacturer'], dev['model'], dev['iden'])
                self.pbdevices[name] = {'name':name, 'id':dev['iden']}
                devices.append(name)
        else:
            self.log.error('Pushbullet: unable to get devices because not configured')
        return devices

    def set_config(self, apikey, devices):
        """
        Set config
        @param apikey: pushbullet apikey (available on https://www.pushbullet.com/account)
        @param devices: array of devices (id) to send notifications
        """
        if not apikey or len(apikey)==0 or not devices or len(devices)==0:
            self.log.error('Pushbullet: all parameters are mandatory')
            return False

        if not devices or len(devices)==0:
            self.devices = []
        else:
            self.devices = json.loads(devices)
        self.apikey = apikey
        self.pbdevices = {}
        self.pushbullet = PushBullet(self.apikey)

        self.__configured = True
        return True

    def prepare_message(self, content):
        """
        Add push
        @param message: push notification
        @file: full file path to send
        """
        if self.__configured:
            #check params
            if not content['message'] or len(content['message'])==0:
                if not content.has_key('file') or (content.has_key('file') and len(content['file'])==0):
                    msg = 'Unable to add push (at least one parameter is mandatory)'
                    self.log.error('Pushbullet: %s' % msg)
                    return None, msg
            elif not content.has_key('file') or (content.has_key('file') and len(content['file'])==0):
                if not content['message'] or len(content['message'])==0:
                    msg = 'Unable to add push (at least one parameter is mandatory)'
                    self.log.error('Pushbullet: %s' % msg)
                    return None, msg
            if not content.has_key('message'):
                content['message'] = ''
            if not content.has_key('file'):
                content['file'] = ''
            #queue push message
            return {'message':content['message'], 'file':content['file']}, ''
        else:
            msg = 'Unable to add message because not configured'
            self.log.error('Pushover: %s' % msg)
            return None, msg

    def _sendMessage(self, message):
        #get devices from pushbullet if necessary
        if len(self.pbdevices)==0:
            self.getPushbulletDevices()

        #push message
        count = 0
        error = ''
        for device in self.devices:
            #get device id
            if self.pbdevices.has_key(device):
                if len(message['file'])==0:
                    #send a note
                    resp = self.pushbullet.pushNote(self.pbdevices[device]['id'], self.pushTitle, message['message'])
                    self.log.debug(resp)
                    if resp.has_key('error'):
                        #error occured
                        error = resp['error']['message']
                        self.log.error('Pushbullet: Unable to push note to device "%s" [%s]' % (self.pbdevices[device]['id'], resp['error']['message']))
                    else:
                        #no pb
                        count += 1
                else:
                    #send a file
                    resp = self.pushbullet.pushFile(self.pbdevices[device]['id'], message['file'])
                    self.log.debug(resp)
                    if resp.has_key('error'):
                        #error occured
                        error = resp['error']['message']
                        self.log.error('Pushbullet: Unable to push file to device "%s" [%s]' % (self.pbdevices[device]['id'], resp['error']['message']))
                    else:
                        #no pb
                        count += 1
            else:
                self.log.warning('Pushbullet: unable to push notification to device "%s" because not found' % (device))

        if count==0:
            return False, 'Nothing pushed'
        else:
            return True, 'Message pushed successfully'

class Notifymyandroid(Alert):
    """
    Class push notifications using notifymyandroid
    """
    def __init__(self, log, save_config_callback, apikeys):
        """
        Constructor
        @see http://www.notifymyandroid.com
        """
        Alert.__init__(self, log, save_config_callback)
        self.name = 'notifymyandroid'
        if not apikeys or len(apikeys)==0:
            self.apikeys = []
            self.__configured = False
        else:
            self.apikeys = json.loads(apikeys)
            self.__configured = True
        self.pushTitle = 'Agocontrol'

    def get_config(self):
        configured = 0
        if self.__configured:
            configured = 1
        return {'configured':configured, 'apikeys':self.apikeys, 'provider':self.name}

    def set_config(self, apikeys):
        """
        Set config
        @param apikey: notifymyandroid apikey (available on https://www.notifymyandroid.com/account.jsp)
        """
        self.log.info('DEBUG: %s %s' % (type(apikeys), apikeys))
        if not apikeys or len(apikeys)==0:
            self.__configured = False
        else:
            self.__configured = True
        self.apikeys = apikeys
        return True

    def prepare_message(self, content, priority='0'):
        """
        Add push
        @param message: push notification
        @param priority: push priority
        """
        if self.__configured:
            #check params
            if not content.has_key('message') or (content.has_key('message') and len(content['message'])==0) or not content.has_key('priority') or (content.has_key('priority') and len(content['priority'])==0):
                msg = 'Unable to add push (all parameters are mandatory)'
                self.log.error('Notifymyandroid: %s' % msg)
                return None, msg
            #queue push message
            return {'message':content['message'], 'priority':content['priority']}, ''
        else:
            msg = 'Unable to add message because not configured'
            self.log.error('Notifymyandroid: %s' % msg)
            return None, msg

    def _sendMessage(self, message):
        for apikey in self.apikeys:
            conn = httplib.HTTPSConnection("www.notifymyandroid.com:443")
            conn.request("POST", "/publicapi/notify",
            urllib.urlencode({
                'apikey': apikey,
                'application':self.pushTitle,
                'event':'Agocontrol alert',
                'description':message['message'],
                'priority':message['priority'],
                'content-type': 'text/html'
            }), { "Content-type": "application/x-www-form-urlencoded" })
            resp = conn.getresponse()
            #check response
            if resp:
                try:
                    xml = resp.read()
                    dom = minidom.parseString(xml)
                    result = dom.firstChild.childNodes[0].tagName
                    code = dom.firstChild.childNodes[0].getAttribute('code')
                    if result=='success':
                        self.log.info('Notifymyandoid: message pushed successfully')
                        return True, 'Message pushed successfully'
                    elif result=='error':
                        self.log.error('Notifymyandroid: received error code "%s"' % code)
                        return False, 'error code %s' % code
                except:
                    self.log.exception('Notifymyandroid: Exception push message')
                    return False, str(e.message)



class AgoAlert(agoclient.AgoApp):
    """
    Agocontrol Alert
    """
    def setup_app(self):
        """
        Configure application
        """
        #declare members
        self.config = {}
        self.__config_lock = threading.Lock()
        self.sms = None
        self.twitter = None
        self.mail = None
        self.push = None
                
        #load config
        self.load_config()
        
        #add handlers
        self.connection.add_handler(self.message_handler)
        #add controller
        self.connection.add_device('alertcontroller', 'alertcontroller')
        
        #create alert instances
        self.create_instances()
        
    def cleanup_app(self):
        """
        Clean application
        """
        if self.sms:
            self.sms.stop()
        if self.twitter:
            self.twitter.stop()
        if self.mail:
            self.mail.stop()
        if self.push:
            self.push.stop()
        
    def save_config(self):
        """
        Save config to map file
        """
        self.__config_lock.acquire()
        error = False
        try:
            #save main config
            self.set_config_option('smtp', self.config['mail']['smtp'], "mail", "alert")
            self.set_config_option('sender', self.config['mail']['sender'], "mail", "alert")
            self.set_config_option('login', self.config['mail']['login'], "mail", "alert")
            self.set_config_option('password', self.config['mail']['password'], "mail", "alert")
            self.set_config_option('tls', self.config['mail']['tls'], "mail", "alert")
            self.set_config_option('key', self.config['twitter']['key'], "twitter", "alert")
            self.set_config_option('secret', self.config['twitter']['secret'], "twitter", "alert")
            self.set_config_option('provider', self.config['sms']['provider'], "sms", "alert")
            self.set_config_option('username', self.config['12voip']['username'], "12voip", "alert")
            self.set_config_option('password', self.config['12voip']['password'], "12voip", "alert")
            self.set_config_option('user', self.config['freemobile']['user'], "freemobile", "alert")
            self.set_config_option('apikey', self.config['freemobile']['apikey'], "freemobile", "alert")
            self.set_config_option('provider', self.config['push']['provider'], "push", "alert")
            self.set_config_option('apikey', self.config['pushbullet']['apikey'], "pushbullet", "alert")
            self.set_config_option('devices', self.config['pushbullet']['devices'], "pushbullet", "alert")
            self.set_config_option('userid', self.config['pushover']['userid'], "pushover", "alert")
            self.set_config_option('token', self.config['pushover']['token'], "pushover", "alert")
            self.set_config_option('apikeys', self.config['notifymyandroid']['apikeys'], "notifymyandroid", "alert")

        except:
            self.log.exception('Unable to save config infos')
            error = True
        self.__config_lock.release()
        
        return not error

    def load_config(self):
        """
        Load config from map file
        """
        self.__config_lock.acquire()
        error = False
        try:
            #load main config
            self.config = {}
            self.config['mail'] = {}
            self.config['mail']['smtp'] = self.get_config_option("smtp", "", "mail", "alert")
            self.config['mail']['sender'] = self.get_config_option("sender", "", "mail", "alert")
            self.config['mail']['login'] = self.get_config_option("login", "", "mail", "alert")
            self.config['mail']['password'] = self.get_config_option("password", "", "mail", "alert")
            self.config['mail']['tls'] = self.get_config_option("tls", "", "mail", "alert")
            self.config['twitter'] = {}
            self.config['twitter']['key'] = self.get_config_option("key", "", "twitter", "alert")
            self.config['twitter']['secret'] = self.get_config_option("secret", "", "twitter", "alert")
            self.config['sms'] = {}
            self.config['sms']['provider'] = self.get_config_option("provider", "", "sms", "alert")
            self.config['12voip'] = {}
            self.config['12voip']['username'] = self.get_config_option("username", "", "12voip", "alert")
            self.config['12voip']['password'] = self.get_config_option("password", "", "12voip", "alert")
            self.config['freemobile'] = {}
            self.config['freemobile']['user'] = self.get_config_option("user", "", "freemobile", "alert")
            self.config['freemobile']['apikey'] = self.get_config_option("apikey", "", "freemobile", "alert")
            self.config['push'] = {}
            self.config['push']['provider'] = self.get_config_option("provider", "", "push", "alert")
            self.config['pushbullet'] = {}
            self.config['pushbullet']['apikey'] = self.get_config_option("apikey", "", "pushbullet", "alert")
            self.config['pushbullet']['devices'] = self.get_config_option("devices", "", "pushbullet", "alert")
            self.config['pushover'] = {}
            self.config['pushover']['userid'] = self.get_config_option("userid", "", "pushover", "alert")
            self.config['pushover']['token'] = self.get_config_option("token", "", "pushover", "alert")
            self.config['notifymyandroid'] = {}
            self.config['notifymyandroid']['apikeys'] = self.get_config_option("apikeys", "", "notifymyandroid", "alert")
            
        except:
            self.log.exception('Unable to load devices infos')
            error = True
        self.__config_lock.release()
        return not error
        
    def message_handler(self, internalid, content):
        """
        Message handler
        """
        self.log.debug('Message handler: internalid=%s : %s' % (internalid, content))

        #get command
        command = None
        if content.has_key('command'):
            command = content['command']
        if not command:
            self.log.error('No command specified')
            return self.connection.response_failed('No command specified')

        if command=='status':
            #return module status
            try:
                return self.connection.response_success({'twitter':self.twitter.get_config(), 'mail':self.mail.get_config(), 'sms':self.sms.get_config(), 'push':self.push.get_config()})
            except Exception as e:
                self.log.exception('commandHandler: status exception:')
                return self.connection.response_failed('Internal error')
                    
        #=========================================
        elif command=='test':
            if not self.check_command_param(content, 'type')[0]:
                msg = 'Missing "type" parameter'
                self.log.error(msg)
                return self.connection.response_missing_parameters(msg)
                
            type = content['type']
            if type=='twitter':
                #test twitter
                (msg, error) = self.twitter.prepare_message({'tweet':'agocontrol test tweet @ %s' % time.strftime('%H:%M:%S')})
                if msg:
                    (error, msg) = self.twitter.test(msg)
                    if not error:
                        return self.connection.response_success(None, 'Tweet successful')
                    else:
                        self.log.error('CommandHandler: failed to tweet [%s]' % msg)
                        return self.connection.response_failed('Failed to tweet (%s)' % msg)
                else:
                    self.log.error('CommandHandler: failed to tweet [%s]' % msg)
                    return self.connection.response_failed(error)

            elif type=='sms':
                #test sms
                if self.sms.name=='12voip':
                    (msg, error) = self.sms.prepare_message({'to':sms.username, 'text':'agocontrol sms test'})
                elif self.sms.name=='freemobile':
                    (msg, error) = self.sms.prepare_message({'to':'', 'text':'agocontrol sms test'})
                if msg:
                    (error, msg) = self.sms.test(msg)
                    if not error:
                        return self.connection.response_success(None, 'SMS sent successfully')
                    else:
                        self.log.error('CommandHandler: Failed to send SMS (%s)' % msg)
                        return self.connection.response_failed('Failed to send SMS (%s)' % msg)
                else:
                    self.log.error('CommandHandler: failed to tweet [%s]' % error)
                    return self.connection.response_failed(error)

            elif type=='mail':
                #mail test
                if not self.check_command_param(content, 'tos')[0]:
                    msg = 'Missing "tos" parameter'
                    self.log.error(msg)
                    return self.connection.response_missing_parameters(msg)

                tos = content['tos'].split(';')
                (msg, error) = self.mail.prepare_message({'tos':tos, 'subject':'agocontrol mail test', 'body':'If you receive this email it means agocontrol alert is working fine!', 'attachment':''})
                if msg:
                    (error, msg) = self.mail.test(msg)
                    if not error:
                        return self.connection.response_success(None, 'Email sent successfully')
                    else:
                        self.log.error('CommandHandler: Failed to send email (%s)' % msg)
                        return self.connection.response_failed('Failed to send email (%s)' % msg)
                else:
                    self.log.error('CommandHandler: failed to send email [%s]' % error)
                    return self.connection.response_failed(error)
                

            elif type=='push':
                #test push
                if self.push.name=='pushbullet':
                    (msg, error) = self.push.prepare_message({'message':'This is an agocontrol test notification', 'file':''})
                elif self.push.name=='pushover':
                    (msg, error) = self.push.prepare_message({'message':'This is an agocontrol test notification', 'priority':'0'})
                elif self.push.name=='notifymyandroid':
                    (msg, error) = self.push.prepare_message({'message':'This is an agocontrol test notification', 'priority':'0'})
                if msg:
                    (error, msg) = self.push.test(msg)
                    if not error:
                        return self.connection.response_success(None, 'Message pushed successfully')
                    else:
                        self.log.error('CommandHandler: Failed to push message (%s)' % msg)
                        return self.connection.response_failed('Failed to push message (%s)' % msg)
                else:
                    self.log.error('CommandHandler: failed to push message [%s]' % error)
                    return self.connection.response_failed(error)
                    
            else:
                #TODO add here new alert test
                pass

        #=========================================
        elif command=='sendtweet':
            #send tweet
            if not self.check_command_param(content, 'tweet')[0]:
                msg = 'Missing "tweet" parameter'
                self.log.error(msg)
                return self.connection.response_missing_parameters(msg)
            
            (msg, error) = self.twitter.prepare_message({'tweet':content['tweet']})
            if msg:
                self.twitter.add_message(msg)
            else:
                self.log.error('CommandHandler: failed to tweet [%s]' % error)
                return self.connection.response_failed(error)
            
        elif command=='sendsms':
            #send sms
            if not self.check_command_param(content, 'text')[0]:
                msg = 'Missing "text" parameter'
                self.log.error(msg)
                return self.connection.response_missing_parameters(msg)
            if not self.check_command_param(content, 'to')[0]:
                msg = 'Missing "to" parameter'
                self.log.error(msg)
                return self.connection.response_missing_parameters(msg)

            (msg, error) = self.sms.prepare_message({'to':content['to'], 'text':content['text']})
            if msg:
                self.sms.add_message(msg)
            else:
                self.log.error('CommandHandler: failed to send SMS [%s]' % error)
                return self.connection.response_failed(error)
                
        elif command=='sendmail':
            #send mail
            if not self.check_command_param(content, 'to')[0]:
                msg = 'Missing "to" parameter'
                self.log.error(msg)
                return self.connection.response_missing_parameters(msg)
            if not self.check_command_param(content, 'subject')[0]:
                msg = 'Missing "subject" parameter'
                self.log.error(msg)
                return self.connection.response_missing_parameters(msg)
            if not self.check_command_param(content, 'body')[0]:
                msg = 'Missing "body" parameter'
                self.log.error(msg)
                return self.connection.response_missing_parameters(msg)
            
            tos = content['to'].split(';')
            if not content.has_key('attachment'):
                content['attachment'] = ''
            (msg, error) = self.mail.prepare_message({'tos':tos, 'subject':content['subject'], 'body':content['body'], 'attachment':content['attachment']})
            if msg:
                self.mail.add_message(msg)
            else:
                self.log.error('CommandHandler: failed to send email [%s]' % error)
                return self.connection.response_failed(error)
                
        elif command=='sendpush':
            #send push
            if not self.check_command_param(content, 'message')[0]:
                msg = 'Missing "message" parameter'
                self.log.error(msg)
                return self.connection.response_missing_parameters(msg)
                
            (msg, error) = self.push.prepare_message({'message':content['message']})
            if msg:
                (error, msg) = self.push.add_message(msg)
                if not error:
                    return self.connection.response_success(None, 'Message pushed successfully')
                else:
                    self.log.error('CommandHandler: Failed to push message (%s)' % msg)
                    return self.connection.response_failed('Failed to push message (%s)' % msg)
            else:
                self.log.error('CommandHandler: failed to push message [%s]' % error)
                return self.connection.response_failed(error)
                    
        #=========================================
        elif command=='setconfig':
            if not self.check_command_param(content, 'type')[0]:
                msg = 'Missing "type" parameter'
                self.log.error(msg)
                return self.connection.response_missing_parameters(msg)
            type = content['type']
            
            if type=='twitter':
                if not self.check_command_param(content, 'accesscode')[0]:
                    msg = 'Missing "accesscode" parameter'
                    self.log.error(msg)
                    return self.connection.response_missing_parameters(msg)
                    
                accessCode = content['accesscode'].strip()
                if len(accessCode)==0:
                    #get authorization url
                    return self.connection.response_success(self.twitter.get_authorization_url())
                    
                elif len(accessCode)>0:
                    #set twitter config
                    (res, key, secret, msg) = self.twitter.get_key_secret(accessCode)
                    if res:
                        self.config['twitter']['key'] = key
                        self.config['twitter']['secret'] = secret
                        if self.twitter.set_config(key, secret) and self.save_config():
                            return self.connection.response_success(None, 'Configuration saved successfully')
                        else:
                            return self.connection.response_failed('Failed to save configuration')
                    else:
                        return self.connection.response_failed('Unable to get credentials from access code (%s)' % msg)
                    
            elif type=='sms':
                #set sms config
                if not self.check_command_param(content, 'provider')[0]:
                    msg = 'Missing "provider" parameter'
                    self.log.error(msg)
                    return self.connection.response_missing_parameters(msg)

                provider = content['provider']
                if provider!=self.sms.name:
                    #destroy existing push
                    self.sms.stop()
                    #create new push
                    if provider=='freemobile':
                        sms = SMSFreeMobile(self.log, self.save_config, '', '')
                    elif provider=='12voip':
                        sms = SMS12voip(self.log, self.save_config, '', '')
                    else:
                        #TODO add here new provider
                        pass
                    #save new sms provider
                    self.config['sms']['provider'] = provider
                        
                if provider=='12voip':
                    if not self.check_command_param(content, 'username')[0]:
                        msg = 'Missing "uername" parameter'
                        self.log.error(msg)
                        return self.connection.response_missing_parameters(msg)
                    if not self.check_command_param(content, 'password')[0]:
                        msg = 'Missing "password" parameter'
                        self.log.error(msg)
                        return self.connection.response_missing_parameters(msg)
                        
                    self.config['12voip']['username'] = content['username']
                    self.config['12voip']['password'] = content['password']
                    if self.sms.set_config(content['username'], content['password']) and self.save_config():
                        return self.connection.response_success(None, 'Configuration saved successfully')
                    else:
                        return self.connection.response_failed('Failed to save configuration')
                                                
                elif provider=='freemobile':
                    if not self.check_command_param(content, 'user')[0]:
                        msg = 'Missing "user" parameter'
                        self.log.error(msg)
                        return self.connection.response_missing_parameters(msg)
                    if not self.check_command_param(content, 'apikey')[0]:
                        msg = 'Missing "apikey" parameter'
                        self.log.error(msg)
                        return self.connection.response_missing_parameters(msg)
                            
                    self.config['freemobile']['user'] = content['user']
                    self.config['freemobile']['apikey'] = content['apikey']
                    if self.sms.set_config(content['user'], content['apikey']) and self.save_config():
                        return self.connection.response_success(None, 'Configuration saved successfully')
                    else:
                        return self.connection.response_failed('Failed to save configuration')

                else:
                    #TODO add here new provider
                    pass

            elif type=='mail':
                #set mail config
                if not self.check_command_param(content, 'smtp')[0]:
                    msg = 'Missing "smtp" parameter'
                    self.log.error(msg)
                    return self.connection.response_missing_parameters(msg)
                if not self.check_command_param(content, 'sender')[0]:
                    msg = 'Missing "sender" parameter'
                    self.log.error(msg)
                    return self.connection.response_missing_parameters(msg)
                if not self.check_command_param(content, 'loginpassword')[0]:
                    msg = 'Missing "loginpassword" parameter'
                    self.log.error(msg)
                    return self.connection.response_missing_parameters(msg)
                if not self.check_command_param(content, 'tls')[0]:
                    msg = 'Missing "tls" parameter'
                    self.log.error(msg)
                    return self.connection.response_missing_parameters(msg)
                    

                #format login%_%password
                login = ''
                password = ''
                try:
                    (login, password) = content['loginpassword'].split('%_%')
                except:
                    self.log.error('commandHandler: unable to split login%_%password [%s]' % content['loginpassword'])
                    return self.connection.response_failed('Internal error')
                tls = content['tls']
                    
                self.config['mail']['smtp'] = content['smtp']
                self.config['mail']['sender'] = content['sender']
                self.config['mail']['login'] = login
                self.config['mail']['password'] = password
                self.config['mail']['tls'] = tls
                if self.mail.set_config(content['smtp'], content['sender'], login , password, tls) and self.save_config():
                    return self.connection.response_success(None, 'Configuration saved successfully')
                else:
                    return self.connection.response_failed('Failed to save configuration')

            elif type=='push':
                #set push config
                if not self.check_command_param(content, 'provider')[0]:
                    msg = 'Missing "provider" parameter'
                    self.log.error(msg)
                    return self.connection.response_missing_parameters(msg)
                
                provider = content['provider']
                if provider!=self.push.name:
                    #destroy existing push
                    self.push.stop()
                    #create new push
                    if provider=='pushbullet':
                        self.push = Pushbullet(self.log, self.save_config, '', '')
                    elif provider=='pushover':
                        self.push = Pushover(self.log, self.save_config, '', '')
                    elif provider=='notifymyandroid':
                        self.push = Notifymyandroid(self.log, self.save_config, '')
                    else:
                        #TODO add here new provider
                        pass
                    #save new push provider
                    self.config['push']['provider'] = provider
                        
                if provider=='pushbullet':
                    if not self.check_command_param(content, 'subcmd')[0]:
                        msg = 'Missing "subcmd" parameter'
                        self.log.error(msg)
                        return self.connection.response_missing_parameters(msg)
                            
                    subCmd = content['subcmd']
                    if subCmd=='getdevices':
                        if not self.check_command_param(content, 'apikey')[0]:
                            msg = 'Missing "apikey" parameter'
                            self.log.error(msg)
                            return self.connection.response_missing_parameters(msg)
                            
                        devices = self.push.getPushbulletDevices(content['apikey'])
                        return self.connection.response_success({'devices':devices}, 'Device list retrieved successfully')
                            
                    elif subCmd=='save':
                        if not self.check_command_param(content, 'apikey')[0]:
                            msg = 'Missing "apikey" parameter'
                            self.log.error(msg)
                            return self.connection.response_missing_parameters(msg)
                        if not self.check_command_param(content, 'devices')[0]:
                            msg = 'Missing "devices" parameter'
                            self.log.error(msg)
                            return self.connection.response_missing_parameters(msg)
                            
                        self.config['pushbullet']['apikey'] = content['apikey']
                        devices = json.dumps(content['devices'])
                        self.config['pushbullet']['devices'] = devices
                        if self.push.set_config(content['apikey'], devices) and self.save_config():
                            return self.connection.response_success(None, 'Configuration saved successfully')
                        else:
                            return self.connection.response_failed('Failed to save configuration')

                elif provider=='pushover':
                    if not self.check_command_param(content, 'userid')[0]:
                        msg = 'Missing "userid" parameter'
                        self.log.error(msg)
                        return self.connection.response_missing_parameters(msg)
                    if not self.check_command_param(content, 'token')[0]:
                        msg = 'Missing "token" parameter'
                        self.log.error(msg)
                        return self.connection.response_missing_parameters(msg)
                            
                    self.config['pushover']['userid'] = content['userid']
                    self.config['pushover']['token'] = content['token']
                    if self.push.set_config(content['userid'], content['token']) and self.save_config():
                        return self.connection.response_success(None, 'Configuration saved successfully')
                    else:
                        return self.connection.response_failed('Failed to save configuration')

                elif provider=='notifymyandroid':
                    if not self.check_command_param(content, 'apikeys')[0]:
                        msg = 'Missing "apikeys" parameter'
                        self.log.error(msg)
                        return self.connection.response_missing_parameters(msg)
                        
                    self.config['notifymyandroid']['apikeys'] = json.dumps(content['apikeys'])
                    if self.push.set_config(content['apikeys']) and self.save_config():
                        return self.connection.response_success(None, 'Configuration saved successfully')
                    else:
                        return self.connection.response_failed('Failed to save configuration')

                else:
                    #TODO add here new provider
                    pass
                        
            else:
                #TODO add here other alert configuration
                pass

    def create_instances(self):
        """
        Create alert instances
        """
        #mail
        self.mail = Mail(self.log, self.save_config, self.config['mail']['smtp'], self.config['mail']['sender'], self.config['mail']['login'], self.config['mail']['password'], self.config['mail']['tls'])
        
        #twitter
        self.twitter = Twitter(self.log, self.save_config, self.config['twitter']['key'], self.config['twitter']['secret'])
        
        #sms
        if self.config['sms']['provider']=='':
            self.log.info('Create dummy sms')
            self.sms = Dummy(self.log, self.save_config)
        elif self.config['sms']['provider']=='12voip':
            self.sms = SMS12voip(self.log, self.save_config, self.config['12voip']['username'], self.config['12voip']['password'])
        elif self.config['sms']['provider']=='freemobile':
            self.sms = SMSFreeMobile(self.log, self.save_config, self.config['freemobile']['user'], self.config['freemobile']['apikey'])
            
        #push
        if self.config['push']['provider']=='':
            self.log.info('Create dummy push')
            self.push = Dummy(self.log, self.save_config)
        elif self.config['push']['provider']=='pushbullet':
            self.push = Pushbullet(self.log, self.save_config, self.config['pushbullet']['apikey'], self.config['pushbullet']['devices'])
        elif self.config['push']['provider']=='pushover':
            self.push = Pushover(self.log, self.save_config, self.config['pushover']['userid'], self.config['pushover']['token'])
        elif self.config['push']['provider']=='notifymyandroid':
            self.push = Notifymyandroid(self.log, self.save_config, self.config['notifymyandroid']['apikeys'])
            
        #start services
        self.mail.start()
        self.twitter.start()
        self.sms.start()
        self.push.start()
            
            
if __name__=="__main__":
    a = AgoAlert();
    a.main()
