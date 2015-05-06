#! /usr/bin/python
# -*- coding: utf-8 -*-

# Agoalert
# copyright (c) 2013 tang
 
import sys
import agoclient
import threading
import time
import logging
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

client = None
twitter = None
mail = None
sms = None
hangout = None
push = None

STATE_MAIL_CONFIGURED = '21'
STATE_MAIL_NOT_CONFIGURED = '20'
STATE_SMS_CONFIGURED = '31'
STATE_SMS_NOT_CONFIGURED = '30'
STATE_TWITTER_CONFIGURED = '41'
STATE_TWITTER_NOT_CONFIGURED = '40'
STATE_PUSH_CONFIGURED = '51'
STATE_PUSH_NOT_CONFIGURED = '50'
STATE_GROWL_CONFIGURED = '61'
STATE_GROWL_NOT_CONFIGURED = '60'

#logging.basicConfig(filename='agoalert.log', level=logging.INFO, format="%(asctime)s %(levelname)s : %(message)s")
logging.basicConfig(level=logging.INFO, format="%(asctime)s %(name)s %(levelname)s : %(message)s")

#=================================
#classes
#=================================
class AgoAlert(threading.Thread):
    """base class for agoalert message"""
    def __init__(self):
        threading.Thread.__init__(self)
        self.__running = True
        self.__queue = Queue()
        self.name = ''

    def __del__(self):
        self.stop()

    def stop(self):
        self.__running = False

    def addMessage(self, message):
        """Queue specified message"""
        self.__queue.put(message)

    def getConfig(self):
        """return if module is configured or not"""
        raise NotImplementedError('getConfig method must be implemented')

    def prepareMessage(self, content):
        """return a message like expected
           @param content: is a map"""
        raise NotImplementedError('prepareMessage method must be implemented')

    def test(self, message):
        """test sending message"""
        error = 0
        msg = ''
        try:
            (r, m) = self._sendMessage(message)
            if not r:
                error = 1
                msg = m
        except Exception as e:
            logging.exception('Error testing alert:')
            error = 1
            msg = str(e.message)
            if len(msg)==0:
                if getattr(e, 'reason', None):
                    msg = str(e.reason)
                else:
                    msg = '<No message>'
        return error, msg

    def _sendMessage(self, message):
        """send message"""
        raise NotImplementedError('_sendMessage method must be implemented')

    def run(self):
        """main process"""
        while self.__running:
            if not self.__queue.empty():
                #message to send
                message = self.__queue.get()
                logging.info('send message: %s' % str(message))
                try:
                    self._sendMessage(message)
                except Exception as e:
                    logging.exception('Unable to send message:')
            #pause
            time.sleep(0.25)

class Dummy(AgoAlert):
    """Do nothing"""
    def __init__(self):
        """Contructor"""
        AgoAlert.__init__(self)

    def getConfig(self):
        return {'configured':0}

    def prepareMessage(self, content):
        pass

    def _sendMessage(self, message):
        pass

class SMSFreeMobile(AgoAlert):
    """Class to send text message (SMS) using 12voip.com provider"""
    def __init__(self, user, apikey):
        """Contructor"""
        AgoAlert.__init__(self)
        self.user = user
        self.apikey = apikey
        self.name = 'freemobile'
        if user and len(user)>0 and apikey and len(apikey)>0:
            self.__configured = True
        else:
            self.__configured = False

    def getConfig(self):
        configured = 0
        if self.__configured:
            configured = 1
        return {'configured':configured, 'user':self.user, 'apikey':self.apikey, 'provider':self.name}

    def setConfig(self, user, apikey):
        """Set config
           @param user: freemobile user
           @param apikey: freemobile apikey"""
        if not user or len(user)==0 or not apikey or len(apikey)==0:
            logging.error('SMSFreeMobile: invalid parameters')
            return False
        if not agoclient.set_config_option('sms', 'provider', self.name, 'alert') or not agoclient.set_config_option(self.name, 'user', user, 'alert') or not agoclient.set_config_option(self.name, 'apikey', apikey, 'alert'):
            logging.error('SMSFreeMobile: unable to save config')
            return False
        self.user = user
        self.apikey = apikey
        self.__configured = True
        return True

    def prepareMessage(self, content):
        """prepare SMS message"""
        if self.__configured:
            #check parameters
            if not content['text'] or len(content['text'])==0:
                msg = 'Unable to add SMS because all parameters are mandatory'
                logging.error('SMSFreeMobile: %s' % msg)
                return None, msg
            if len(content['text'])>160:
                logging.warning('SMSFreeMobile: SMS is too long, message will be truncated')
                content['text'] = content['text'][:159]
            #queue sms
            return {'to':content['to'], 'text':content['text']}, ''
        else:
            msg = 'Unable to add SMS because not configured'
            logging.error('SMSFreeMobile: %s' % msg)
            return None, msg

    def _sendMessage(self, message):
        """url format:
           https://smsapi.free-mobile.fr/sendmsg?user=<user>&pass=<apikey>&msg=<message> """
        params = {'user':self.user, 'pass':self.apikey, 'msg':message['text']}
        url = 'https://smsapi.free-mobile.fr/sendmsg?'
        url += urllib.urlencode(params)
        gcontext = ssl.SSLContext(ssl.PROTOCOL_TLSv1)  # Only for gangstars
        req = urllib2.urlopen(url, context=gcontext)
        lines = req.readlines()
        status = req.getcode()
        req.close()
        logging.debug('url=%s status=%s' % (url, str(status)))
        if status==200:
            return True, ''
        else:
            return False, status

class SMS12voip(AgoAlert):
    """Class to send text message (SMS) using 12voip.com provider"""
    def __init__(self, username, password):
        """Contructor"""
        AgoAlert.__init__(self)
        self.username = username
        self.password = password
        self.name = '12voip'
        if username and len(username)>0 and password and len(password)>0:
            self.__configured = True
        else:
            self.__configured = False

    def getConfig(self):
        configured = 0
        if self.__configured:
            configured = 1
        return {'configured':configured, 'username':self.username, 'password':self.password, 'provider':self.name}

    def setConfig(self, username, password):
        """Set config
           @param username: 12voip username
           @param password: 12voip password"""
        if not username or len(username)==0 or not password or len(password)==0:
            logging.error('SMS12voip: invalid parameters')
            return False
        if not agoclient.set_config_option('sms', 'provider', self.name, 'alert') or not agoclient.set_config_option(self.name, 'username', username, 'alert') or not agoclient.set_config_option(self.name, 'password', password, 'alert'):
            logging.error('SMS12voip: unable to save config')
            return False
        self.username = username
        self.password = password
        self.__configured = True
        return True

    def prepareMessage(self, content):
        """Add SMS"""
        if self.__configured:
            #check parameters
            if not content['to'] or not content['text'] or len(content['to'])==0 or len(content['text'])==0:
                msg = 'Unable to add SMS because all parameters are mandatory'
                logging.error('SMS12voip: %s' % msg)
                return None, msg
            if not content['to'].startswith('+') and content['to']!=self.username:
                msg = 'Unable to add SMS because "to" number must be international number'
                logging.error('SMS12voip: %s' % msg)
                return None, msg
            if len(content['text'])>160:
                logging.warning('SMS12voip: SMS is too long, message will be truncated')
                content['text'] = content['text'][:159]
            #queue sms
            return {'to':content['to'], 'text':content['text']}, ''
        else:
            msg = 'Unable to add SMS because not configured'
            logging.error('SMS12voip: %s' % msg)
            return None, msg

    def _sendMessage(self, message):
        """url format:
           https://www.12voip.com/myaccount/sendsms.php?username=xxxxxxxxxx&password=xxxxxxxxxx&from=xxxxxxxxxx&to=xxxxxxxxxx&text=xxxxxxxxxx""" 
        params = {'username':self.username, 'password':self.password, 'from':self.username, 'to':message['to'], 'text':message['text']}
        url = 'https://www.12voip.com/myaccount/sendsms.php?'
        url += urllib.urlencode(params)
        gcontext = ssl.SSLContext(ssl.PROTOCOL_TLSv1)  # Only for gangstars
        req = urllib2.urlopen(url, context=gcontext)
        lines = req.readlines()
        status = req.getcode()
        req.close()
        logging.debug('url=%s status=%s' % (url, str(status)))
        if status==200:
            return True, ''
        else:
            return False, status

class Twitter(AgoAlert):
    """Class for tweet sending"""
    def __init__(self, key, secret):
        """constructor"""
        AgoAlert.__init__(self)
        global client
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

    def getConfig(self):
        configured = 0
        if self.__configured:
            configured = 1
        return {'configured':configured}

    def setAccessCode(self, code):
        """Set twitter access code to get user key and secret
           @param code: code provided by twitter """
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
                #save token in config file
                if agoclient.set_config_option(self.name, 'key', self.key, 'alert') and agoclient.set_config_option(self.name, 'secret', self.secret, 'alert'):
                    self.__configured = True
                    return {'error':0, 'msg':''}
                else:
                    return {'error':0, 'msg':'Unable to save Twitter token in config file.'}
            except Exception as e:
                logging.error('Twitter: config exception [%s]' % str(e))
                return {'error':1, 'msg':'Internal error'}
        else:
            logging.error('Twitter: code is mandatory')
            return {'error':1, 'msg':'Internal error'}

    def getAuthorizationUrl(self):
        """get twitter authorization url"""
        try:
            if not self.__auth:
                self.__auth = tweepy.OAuthHandler(self.consumerKey, self.consumerSecret)
                self.__auth.secure = True
            url = self.__auth.get_authorization_url()
            #logging.info('twitter url=%s' % url)
            return {'error':0, 'url':url}
        except Exception as e:
            logging.exception('Twitter: Unable to get Twitter authorization url [%s]' % str(e) ) 
            return {'error':1, 'url':''}

    def prepareMessage(self, content):
        """prepare tweet message"""
        if self.__configured:
            #check parameters
            if not content['tweet'] and len(content['tweet'])==0:
                msg = 'Unable to add tweet (all parameters are mandatory)'
                logging.error('Twitter: %s' % msg)
                return None, msg
            if len(content['tweet'])>140:
                logging.warning('Twitter: Tweet is too long, message will be truncated')
                content['tweet'] = content['tweet'][:139]
            #queue message
            return {'tweet':content['tweet']}, ''
        else:
            msg = 'Unable to add tweet because not configured'
            logging.error('Twitter: %s' % msg)
            return None, msg

    def _sendMessage(self, message):
        #connect using OAuth auth (basic auth deprecated)
        auth = tweepy.OAuthHandler(self.consumerKey, self.consumerSecret)
        auth.secure = True
        logging.debug('key=%s secret=%s' % (self.key, self.secret))
        auth.set_access_token(self.key, self.secret)
        api = tweepy.API(auth)
        res = api.update_status(message['tweet'])
        logging.debug(res);
        #check result
        if res.text.find(message['tweet'])!=-1:
            return True, ''
        else:
            return False, 'Message not tweeted'

class Mail(AgoAlert):
    """Class for mail sending"""
    def __init__(self, smtp, sender, login, password, tls):
        """Constructor"""
        AgoAlert.__init__(self)
        global client
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

    def getConfig(self):
        configured = 0
        if self.__configured:
            configured = 1
        tls = 0
        if self.tls:
            tls = 1
        return {'configured':configured, 'sender':self.sender, 'smtp':self.smtp, 'login':self.login, 'password':self.password, 'tls':tls}

    def setConfig(self, smtp, sender, login, password, tls):
        """set config
           @param smtp: smtp server address
           @param sender: mail sender""" 
        if not smtp or len(smtp)==0 or not sender or len(sender)==0:
            logging.error('Mail: all parameters are mandatory')
            return False
        if not agoclient.set_config_option(self.name, 'smtp', smtp, 'alert') or not agoclient.set_config_option(self.name, 'sender', sender, 'alert') or not agoclient.set_config_option(self.name, 'login', login, 'alert') or not agoclient.set_config_option(self.name, 'password', password, 'alert') or not agoclient.set_config_option(self.name, 'tls', tls, 'alert'):
            logging.error('Mail: unable to save config')
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

    def prepareMessage(self, content):
        """prepare mail message
           @param subject: mail subject
           @param tos: send mail to list of tos
           @param content: mail content"""
        if self.__configured:
            #check params
            if not content.has_key('tos') or not content.has_key('body') or (content.has_key('tos') and len(content['tos'])==0) or (content.has_key('body') and len(content['body'])==0):
                msg = 'Unable to add mail (all parameters are mandatory)'
                logging.error('Mail: %s' % msg)
                return None, msg
            if not content.has_key('subject') or (content.has_key('subject') and len(content['subject'])==0):
                #no subject specified, add default one
                content['subject'] = 'Agocontrol alert'
            #queue mail
            return {'subject':content['subject'], 'tos':content['tos'], 'content':content['body']}, ''
        else:
            msg = 'Unable to add mail because not configured'
            logging.error('Mail: %s' % msg)
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
        message['attachment'] = '/home/tang/japan.jpg'
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
        return True, ''

class Pushover(AgoAlert):
    """Class for push message sending for ios and android"""
    def __init__(self, userid, token):
        """Constructor"""
        """https://pushover.net/"""
        AgoAlert.__init__(self)
        global client
        self.name = 'pushover'
        self.token = token
        self.userid = userid
        self.pushTitle = 'Agocontrol'
        if userid and len(userid)>0:
            self.__configured = True
        else:
            self.__configured = False

    def getConfig(self):
        configured = 0
        if self.__configured:
            configured = 1
        return {'configured':configured, 'userid':self.userid, 'provider':self.name, 'token':self.token}

    def setConfig(self, userid, token):
        """set config
           @param userid: pushover userid (available on https://pushover.net/dashboard)
           @param token: pushover app token"""
        if not userid or len(userid)==0 or not token or len(token)==0:
            logging.error('Pushover: all parameters are mandatory')
            return False
        if not agoclient.set_config_option('push', 'provider', self.name, 'alert') or not agoclient.set_config_option(self.name, 'userid', userid, 'alert') or not agoclient.set_config_option(self.name, 'token', token, 'alert'):
            logging.error('Pushover: unable to save config')
            return False
        self.userid = userid
        self.token = token
        self.__configured = True
        return True

    def prepareMessage(self, content):
        """Add push
           @param message: push notification"""
        if self.__configured:
            #check params
            if not content['message'] or len(content['message'])==0 or not content.has_key('priority'):
                msg = 'Unable to add push (all parameters are mandatory)'
                logging.error('Pushover: %s' % msg)
                return None, msg
            #queue push message
            return {'message':content['message'], 'priority':content['priority']}, ''
        else:
            msg = 'Unable to add message because not configured'
            logging.error('Pushover: %s' % msg)
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
        logging.info(resp)
        #check response
        if resp:
            try:
                read = resp.read()
                logging.debug(read)
                resp = json.loads(read)
                #TODO handle receipt https://pushover.net/api#receipt
                if resp['status']==0:
                    #error occured
                    logging.error('Pushover: %s' % (str(resp['errors'])))
                    return False, str(resp['errors'])
                else:
                    logging.info('Pushover: message received by user')
                    return True, ''
            except:
                logging.exception('Pushover: Exception push message')
                return False, str(e.message)

class Pushbullet(AgoAlert):
    """Class for push message sending for ios and android
       @info https://www.pushbullet.com """
    def __init__(self, apikey, devices):
        """Constructor"""
        AgoAlert.__init__(self)
        global client
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

    def getConfig(self):
        configured = 0
        if self.__configured:
            configured = 1
        return {'configured':configured, 'apikey':self.apikey, 'devices':self.devices, 'provider':self.name}

    def getPushbulletDevices(self, apikey=None):
        """request pushbullet to get its devices"""
        if not self.__configured and not apikey:
            logging.error('Pushbullet: unable to get devices. Not configured and no apikey specified')
            return {}
        
        if apikey:
            self.pushbullet = PushBullet(apikey)

        devices = []
        if self.pushbullet:
            devs = self.pushbullet.getDevices()
            logging.debug('pushbullet devs=%s' % str(devs))
            for dev in devs:
                name = '%s %s (%s)' % (dev['manufacturer'], dev['model'], dev['iden'])
                self.pbdevices[name] = {'name':name, 'id':dev['iden']}
                devices.append(name)
        else:
            logging.error('Pushbullet: unable to get devices because not configured')
        return devices

    def setConfig(self, apikey, devices):
        """set config
           @param apikey: pushbullet apikey (available on https://www.pushbullet.com/account)
           @param devices: array of devices (id) to send notifications """
        if not apikey or len(apikey)==0 or not devices or len(devices)==0:
            logging.error('Pushbullet: all parameters are mandatory')
            return False
        if not agoclient.set_config_option('push', 'provider', self.name, 'alert') or not agoclient.set_config_option(self.name, 'apikey', apikey, 'alert') or not agoclient.set_config_option(self.name, 'devices', json.dumps(devices), 'alert'):
            logging.error('Pushbullet: unable to save config')
            return False
        self.apikey = apikey
        self.devices = devices
        self.pbdevices = {}
        self.pushbullet = PushBullet(self.apikey)
        self.__configured = True
        return True

    def prepareMessage(self, content):
        """Add push
           @param message: push notification
           @file: full file path to send"""
        if self.__configured:
            #check params
            if not content['message'] or len(content['message'])==0:
                if not content.has_key('file') or (content.has_key('file') and len(content['file'])==0):
                    msg = 'Unable to add push (at least one parameter is mandatory)'
                    logging.error('Pushbullet: %s' % msg)
                    return None, msg
            elif not content.has_key('file') or (content.has_key('file') and len(content['file'])==0):
                if not content['message'] or len(content['message'])==0:
                    msg = 'Unable to add push (at least one parameter is mandatory)'
                    logging.error('Pushbullet: %s' % msg)
                    return None, msg
            if not content.has_key('message'):
                content['message'] = ''
            if not content.has_key('file'):
                content['file'] = ''
            #queue push message
            return {'message':content['message'], 'file':content['file']}, ''
        else:
            msg = 'Unable to add message because not configured'
            logging.error('Pushover: %s' % msg)
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
                    logging.debug(resp)
                    if resp.has_key('error'):
                        #error occured
                        error = resp['error']['message']
                        logging.error('Pushbullet: Unable to push note to device "%s" [%s]' % (self.pbdevices[device]['id'], resp['error']['message']))
                    else:
                        #no pb
                        count += 1
                else:
                    #send a file
                    resp = self.pushbullet.pushFile(self.pbdevices[device]['id'], message['file'])
                    logging.debug(resp)
                    if resp.has_key('error'):
                        #error occured
                        error = resp['error']['message']
                        logging.error('Pushbullet: Unable to push file to device "%s" [%s]' % (self.pbdevices[device]['id'], resp['error']['message']))
                    else:
                        #no pb
                        count += 1
            else:
                logging.warning('Pushbullet: unable to push notification to device "%s" because not found' % (device))

        if count==0:
            return False, 'Nothing pushed'
        else:
            return True, ''

class Notifymyandroid(AgoAlert):
    """Class push notifications using notifymyandroid"""
    def __init__(self, apikeys):
        """Constructor"""
        """http://www.notifymyandroid.com"""
        AgoAlert.__init__(self)
        global client
        self.name = 'notifymyandroid'
        if not apikeys or len(apikeys)==0:
            self.apikeys = []
        else:
            self.apikeys = json.loads(apikeys)
        self.pushTitle = 'Agocontrol'
        if apikeys and len(apikeys)>0:
            self.__configured = True
        else:
            self.__configured = False

    def getConfig(self):
        configured = 0
        if self.__configured:
            configured = 1
        return {'configured':configured, 'apikeys':self.apikeys, 'provider':self.name}

    def setConfig(self, apikeys):
        """set config
           @param apikey: notifymyandroid apikey (available on https://www.notifymyandroid.com/account.jsp)"""
        if not apikeys or len(apikeys)==0:
            logging.error('Notifymyandroid: all parameters are mandatory')
            return False
        if not agoclient.set_config_option('push', 'provider', self.name, 'alert') or not agoclient.set_config_option(self.name, 'apikeys', json.dumps(apikeys) , 'alert'):
            logging.error('Notifymyandroid: unable to save config')
            return False
        self.apikeys = apikeys
        self.__configured = True
        return True

    def prepareMessage(self, message, priority='0'):
        """Add push
           @param message: push notification"""
        if self.__configured:
            #check params
            if not content.has_key('message') or (content.has_key('message') and len(content['message'])==0) or not content.has_key('priority') or (content.has_key('priority') and len(content['priority'])==0):
                msg = 'Unable to add push (all parameters are mandatory)'
                logging.error('Notifymyandroid: %s' % msg)
                return None, msg
            #queue push message
            return {'message':content['message'], 'priority':content['priority']}, ''
        else:
            msg = 'Unable to add message because not configured'
            logging.error('Notifymyandroid: %s' % msg)
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
                        logging.info('Notifymyandoid: message pushed successfully')
                        return True, ''
                    elif result=='error':
                        logging.error('Notifymyandroid: received error code "%s"' % code)
                        return False, 'error code %s' % code
                except:
                    logging.exception('Notifymyandroid: Exception push message')
                    return False, str(e.message)


#=================================
#utils
#=================================
def quit(msg):
    """Exit application"""
    global sms, hangout, mail, twitter, push
    global client
    if client:
        del client
        client = None
    if sms:
        sms.stop()
        del sms
        sms = None
    if twitter:
        twitter.stop()
        del twitter
        twitter = None
    if hangout:
        hangout.stop()
        del hangout
        hangout = None
    if mail:
        mail.stop()
        del mail
        mail = None
    if push:
        push.stop()
        del push
        push = None
    logging.fatal(msg)
    sys.exit(0)


#=================================
#functions
#=================================
def commandHandler(internalid, content):
    """ago command handler"""
    logging.info('commandHandler: %s, %s' % (internalid,content))
    global twitter, push, mail, sms
    global client
    command = None

    if content.has_key('command'):
        command = content['command']
    else:
        logging.error('No command specified')
        return None

    #=========================================
    if command=='status':
        #return module status
        try:
            return {'error':0, 'msg':'', 'twitter':twitter.getConfig(), 'mail':mail.getConfig(), 'sms':sms.getConfig(), 'push':push.getConfig()}
        except Exception as e:
            logging.exception('commandHandler: configured exception [%s]' % str(e))
            return {'error':1, 'msg':'Internal error'}

    #=========================================
    elif command=='test':
        if not content.has_key('param1'):
            logging.error('test command: missing parameters')
            return {'error':1, 'msg':'Internal error'}
        type = content['param1']
        if type=='twitter':
            #twitter test
            (msg, error) = twitter.prepareMessage({'tweet':'agocontrol test tweet @ %s' % time.strftime('%H:%M:%S')})
            if msg:
                (error, msg) = twitter.test(msg)
                if not error:
                    return {'error':0, 'msg':'Tweet successful'}
                else:
                    logging.error('CommandHandler: failed to tweet')
                    return {'error':1, 'msg':'Failed to tweet (%s)' % msg}
            else:
                return {'error':1, 'msg':error}

        elif type=='sms':
            #test sms
            if sms.name=='12voip':
                (msg, error) = sms.prepareMessage({'to':sms.username, 'text':'agocontrol sms test'})
                if msg:
                    (error, msg) = sms.test(msg)
                    if not error:
                        return {'error':0, 'msg':'SMS sent successfully'}
                    else:
                        logging.error('CommandHandler: unable to send test SMS')
                        return {'error':1, 'msg':'Failed to send SMS (%s)' % msg}
                else:
                    return {'error':1, 'msg':error}
            elif sms.name=='freemobile':
                (msg, error) = sms.prepareMessage({'to':'', 'text':'agocontrol sms test'})
                if msg:
                    (error, msg) = sms.test(msg)
                    if not error:
                        return {'error':0, 'msg':'SMS sent successfully'}
                    else:
                        logging.error('CommandHandler: unable to send test SMS')
                        return {'error':1, 'msg':'Failed to send SMS (%s)' % msg}
                else:
                    return {'error':1, 'msg':error}

        elif type=='mail':
            #mail test
            if content.has_key('param2'):
                tos = content['param2'].split(';')
                (msg, error) = mail.prepareMessage({'tos':tos, 'subject':'agocontrol mail test', 'content':'If you receive this email it means agocontrol alert is working fine!'})
                if msg:
                    (error, msg) = mail.test(msg)
                    if not error:
                        return {'error':0, 'msg':'Email sent successfully'}
                    else:
                        logging.error('CommandHandler: failed to send email [%s, test]' % (str(tos)))
                        return {'error':1, 'msg':'Failed to send email (%s)' % msg}
                else:
                    return {'error':1, 'msg':error}
            else:
                logging.error('commandHandler: parameters missing for SMS')
                return {'error':1, 'msg':'Internal error'}

        elif type=='push':
            #test push
            if push.name=='pushbullet':
                (msg, error) = push.prepareMessage({'message':'This is an agocontrol test notification', 'file':''})
                if msg:
                    (error, msg) = push.test(msg)
                    if not error:
                        return {'error':0, 'msg':'Push notification sent'}
                    else:
                        logging.error('CommandHandler: failed to send push message with pushbullet [test]')
                        return {'error':1, 'msg':'Failed to send push notification (%s)' % msg}
                else:
                    return {'error':1, 'msg':error}
            elif push.name=='pushover':
                (msg, error) = push.prepareMessage({'message':'This is an agocontrol test notification', 'priority':'0'})
                if msg:
                    (error, msg) = push.test(msg)
                    if not error:
                        return {'error':0, 'msg':'Push sent successfully'}
                    else:
                        logging.error('CommandHandler: failed to send push message with pushover [test]')
                        return {'error':1, 'msg':'Failed to send push notification (%s)' % msg}
                else:
                    return {'error':1, 'msg':error}
            elif push.name=='notifymyandroid':
                (msg, error) = push.prepareMessage({'message':'This is an agocontrol test notification', 'priority':'0'})
                if msg:
                    (error, msg) = push.test(msg)
                    if not error:
                        return {'error':0, 'msg':''}
                    else:
                        logging.error('CommandHandler: failed to send push message with notifymyandroid [test]')
                        return {'error':1, 'msg':'Failed to send push notification (%s)' % msg}
                else:
                    return {'error':1, 'msg':error}
        else:
            #TODO add here new alert test
            pass

    #=========================================
    elif command=='sendtweet':
        #send tweet
        if content.has_key('tweet'):
            (msg, error) = twitter.prepareMessage({'tweet':content['tweet']})
            if msg:
                twitter.addMessage(msg)
            else:
                return {'error':1, 'msg':error}
        else:
            logging.error('commandHandler: parameters missing for tweet')
            return {'error':1, 'msg':'Internal error'}
    elif command=='sendsms':
        #send sms
        if content.has_key('text') and content.has_key('to'):
            (msg, error) = sms.prepareMessage({'to':content['to'], 'text':content['text']})
            if msg:
                sms.addMessage(msg)
            else:
                return {'error':1, 'msg':error}
        else:
            logging.error('commandHandler: parameters missing for SMS')
            return {'error':1, 'msg':'Internal error'}
    elif command=='sendmail':
        #send mail
        if content.has_key('to') and content.has_key('subject') and content.has_key('body'):
            tos = content['to'].split(';')
            (msg, error) = mail.prepareMessage({'tos':tos, 'subject':content['subject'], 'body':content['body']})
            if msg:
                mail.addMessage(msg)
            else:
                return {'error':1, 'msg':error}
        else:
            logging.error('commandHandler: parameters missing for email')
            return {'error':1, 'msg':'Internal error'}
    elif command=='sendpush':
        #send push
        if push.name=='pushbullet':
            if content.has_key('message'):
                (msg, error) = push.prepareMessage({'message':content['message']})
                if msg:
                    push.addMessage(msg)
                else:
                    return {'error':1, 'msg':error}
            else:
                logging.error('commandHandler: parameters missing for pushbullet')
                return {'error':1, 'msg':'Internal error'}
        elif push.name=='pushover':
            if content.has_key('message'):
                (msg, error) = push.prepareMessage({'message':content['message']})
                if msg:
                    push.addMessage(msg)
                else:
                    return {'error':1, 'msg':error}
            else:
                logging.error('commandHandler: parameters missing for pushover')
                return {'error':1, 'msg':'Internal error'}
        elif push.name=='notifymyandroid':
            if content.has_key('message'):
                (msg, error) = push.prepareMessage({'message':content['message']})
                if msg:
                    (error, msg) = push.addMessage(msg)
                    if not error:
                        return {'error':0, 'msg':''}
                    else:
                        logging.error('CommandHandler: failed to send push message with notifymyandroid [message:%s priority:%s]'% (str(content['message'])))
                        return {'error':1, 'msg':'Failed to send push notification'}
                else:
                    return {'error':1, 'msg':error}
            else:
                logging.error('commandHandler: parameters missing for notifymyandroid')
                return {'error':1, 'msg':'Internal error'}
        else:
            #TODO add here new alert sending
            pass

    #=========================================
    elif command=='setconfig':
        if not content.has_key('param1'):
            logging.error('test command: missing parameters')
            return {'error':1, 'msg':'Internal error'}
        type = content['param1']
        if type=='twitter':
            if not content.has_key('param2'):
                logging.error('test command: missing parameters "param2"')
                return {'error':1, 'msg':'Internal error'}
            accessCode = content['param2'].strip()
            if len(accessCode)==0:
                #get authorization url
                return twitter.getAuthorizationUrl()
            elif len(accessCode)>0:
                #set twitter config
                return twitter.setAccessCode(accessCode)
        elif type=='sms':
            #set sms config
            if content.has_key('param2'):
                provider = content['param2']
                if provider!=sms.name:
                    #destroy existing push
                    sms.stop()
                    del sms
                    #create new push
                    if provider=='freemobile':
                        sms = SMSFreeMobile('', '')
                    elif provider=='12voip':
                        sms = SMS12voip('', '')
                    else:
                        #TODO add here new provider
                        pass
                if provider=='12voip':
                    if content.has_key('param3') and content.has_key('param4'):
                        if sms.setConfig(content['param3'], content['param4']):
                            return {'error':0, 'msg':''}
                        else:
                            return {'error':1, 'msg':'Unable to save config'}
                    else:
                        logging.error('commandHandler: parameter missing for %s' % provider)
                        return {'error':1, 'msg':'Internal error'}
                elif provider=='freemobile':
                    if content.has_key('param3') and content.has_key('param4'):
                        if sms.setConfig(content['param3'], content['param4']):
                            return {'error':0, 'msg':''}
                        else:
                            return {'error':1, 'msg':'Unable to save config'}
                    else:
                        logging.error('commandHandler: parameter missing for %s' % provider)
                        return {'error':1, 'msg':'Internal error'}
                else:
                    #TODO add here new provider
                    pass
            else:
                logging.error('commandHandler: parameters missing for SMS config')
                return {'error':1, 'msg':'Internal error'}
        elif type=='mail':
            #set mail config
            if content.has_key('param2') and content.has_key('param3'):
                login = ''
                password = ''
                tls = ''
                if content.has_key('param4'):
                    #format login%_%password
                    try:
                        (login, password) = content['param4'].split('%_%')
                    except:
                        logging.warning('commandHandler: unable to split login%_%password [%s]' % content['param4'])
                if content.has_key('param5'):
                    tls = content['param5']
                if mail.setConfig(content['param2'], content['param3'], login, password, tls):
                    return {'error':0, 'msg':''}
                else:
                    return {'error':1, 'msg':'Unable to save config'}
            else:
                logging.error('commandHandler: parameters missing for Mail config')
                return {'error':1, 'msg':'Internal error'}
        elif type=='push':
            #set push config
            if content.has_key('param2'):
                provider = content['param2']
                if provider!=push.name:
                    #destroy existing push
                    push.stop()
                    del push
                    #create new push
                    if provider=='pushbullet':
                        push = Pushbullet('', '')
                    elif provider=='pushover':
                        push = Pushover('', '')
                    elif provider=='notifymyandroid':
                        push = Notifymyandroid('')
                    else:
                        #TODO add here new provider
                        pass
                if provider=='pushbullet':
                    if content.has_key('param3'):
                        subCmd = content['param3']
                        if subCmd=='getdevices':
                            if content.has_key('param4'):
                                devices = push.getPushbulletDevices(content['param4'])
                                return {'error':0, 'msg':'', 'devices':devices}
                            else:
                                logging.error('commandHandler: parameter "param4" missing for getPushbulletDevices()')
                                return {'error':1, 'msg':'Internal error'}
                        elif subCmd=='save':
                            if content.has_key('param4') and content.has_key('param5'):
                                if push.setConfig(content['param4'], content['param5']):
                                    return {'error':0, 'msg':''}
                                else:
                                    logging.error('Unable to save config')
                                    return {'error':1, 'msg':'Unable to save config'}
                            else:
                                logging.error('commandHandler: parameters missing for Pushbullet config')
                                return {'error':1, 'msg':'Internal error'}
                elif provider=='pushover' or provider=='notifymyandroid':
                    if content.has_key('param3'):
                        if push.setConfig(content['param3'], content['param4']):
                            return {'error':0, 'msg':''}
                        else:
                            logging.error('Unable to save config')
                            return {'error':1, 'msg':'Unable to save config'}
                    else:
                        logging.error('commandHandler: parameter "param3" missing for %s' % provider)
                        return {'error':1, 'msg':'Internal error'}
                else:
                    #TODO add here new provider
                    pass
        else:
            #TODO add here other alert configuration
            pass
    else:
        logging.warning('Unmanaged command "%s"' % content['command'])

def eventHandler(event, content):
    """ago event handler"""
    #logging.info('eventHandler: %s, %s' % (event, content))
    global client
    uuid = None
    internalid = None

    #get uuid
    if content.has_key('uuid'):
        uuid = content['uuid']
        internalid = client.uuid_to_internal_id(uuid)
    
    if uuid and uuid in client.uuids:
        #uuid belongs to this handler
        #TODO manage events here
        pass


#=================================
#main
#=================================
#init
try:
    #connect agoclient
    client = agoclient.AgoConnection('alert')

    #load config
    configMailSmtp = agoclient.get_config_option("mail", "smtp", "", "alert")
    configMailSender = agoclient.get_config_option("mail", "sender", "", "alert")
    configMailLogin = agoclient.get_config_option("mail", "login", "", "alert")
    configMailPassword = agoclient.get_config_option("mail", "password", "", "alert")
    configMailTls = agoclient.get_config_option("mail", "tls", "", "alert")
    configTwitterKey = agoclient.get_config_option("twitter", "key", "", "alert")
    configTwitterSecret = agoclient.get_config_option("twitter", "secret", "", "alert")
    configSmsProvider = agoclient.get_config_option("sms", "provider", "", "alert")
    config12VoipUsername = agoclient.get_config_option("12voip", "username", "", "alert")
    config12VoipPassword = agoclient.get_config_option("12voip", "password", "", "alert")
    configFreeMobileUser = agoclient.get_config_option("freemobile", "user", "", "alert")
    configFreeMobileApikey = agoclient.get_config_option("freemobile", "apikey", "", "alert")
    configPushProvider = agoclient.get_config_option("push", "provider", "", "alert")
    configPushbulletApikey = agoclient.get_config_option("pushbullet", "apikey", "", "alert")
    configPushbulletDevices = agoclient.get_config_option("pushbullet", "devices", "", "alert")
    configPushoverUserid = agoclient.get_config_option("pushover", "userid", "", "alert")
    configPushoverToken = agoclient.get_config_option("pushover", "token", "", "alert")
    configNotifymyandroidApikeys = agoclient.get_config_option("notifymyandroid", "apikeys", "", "alert")

    #create objects
    mail = Mail(configMailSmtp, configMailSender, configMailLogin, configMailPassword, configMailTls)
    twitter = Twitter(configTwitterKey, configTwitterSecret)
    if configSmsProvider=='':
        logging.info('Create dummy sms')
        sms = Dummy()
    elif configSmsProvider=='12voip':
        sms = SMS12voip(config12VoipUsername, config12VoipPassword)
    elif configSmsProvider=='freemobile':
        sms = SMSFreeMobile(configFreeMobileUser, configFreeMobileApikey)
    if configPushProvider=='':
        logging.info('Create dummy push')
        push = Dummy()
    elif configPushProvider=='pushbullet':
        push = Pushbullet(configPushbulletApikey, configPushbulletDevices)
    elif configPushProvider=='pushover':
        push = Pushover(configPushoverUserid, configPushoverToken)
    elif configPushProvider=='notifymyandroid':
        push = Notifymyandroid(configNotifymyandroidApikeys)

    #start services
    mail.start()
    twitter.start()
    sms.start()
    push.start()

    #add client handlers
    client.add_handler(commandHandler)

    #add controller
    logging.info('Add controller')
    client.add_device('alertcontroller', 'alertcontroller')


except Exception as e:
    #init failed
    logging.exception('Exception during init')
    quit('Init failed, exit now.')

#run agoclient
try:
    logging.info('Running agoalert...')
    client.run()
except KeyboardInterrupt:
    #stopped by user
    quit('agoalert stopped by user')
except Exception as e:
    logging.exception('Exception on main')
    #stop everything
    quit('agoalert stopped')

