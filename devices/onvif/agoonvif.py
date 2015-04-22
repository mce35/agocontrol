#!/usr/bin/python
#-*-coding=utf-8

import time
import threading
import os
import json
import base64
from datetime import datetime
#ago
import agoclient
#onvif
from onvif import ONVIFCamera, ONVIFError, ONVIFService
from suds import MethodNotFound
from suds.sax.text import Text
from suds.client import Client
#opencv
import cv2
import numpy

DEVICEMAPFILE = os.path.join(agoclient.CONFDIR, 'maps/onvif.json')


class Motion(threading.Thread):
    """
    Code inspired from Robin David (converted to cv2):
    https://github.com/RobinDavid/Motion-detection-OpenCV
    And from Cédric Verstraeten:
    https://blog.cedric.ws/opencv-simple-motion-detection
    """

    def __init__(self, connection, log, camera_internalid, uri, sensitivity=10 , deviation=20, on_duration=300, record_duration=0):
        """
        Constructor
        @param camera_internalid: camera internalid to generate record filename
        @param uri: video stream uri
        @param sensitivity: number of changes on picture detected
        @param deviation: the higher the value, the more motion is allowed
        @param on_duration: time during binary sensor stays on when motion detected
        @param record_duration: 0 to disable recording, otherwise time in seconds
        """
        threading.Thread.__init__(self)
        self.connection = connection
        self.log = log
        self.recorder = None
        self.on_duration = on_duration
        if record_duration<=on_duration:
            self.record_duration = record_duration
        else:
            self.log.warning('"record duration" must be lower than "on duration". "record duration" is set to "on duration"')
            self.record_duration = on_duration
        self.do_record = False
        if self.record_duration>0:
            self.do_record = True
        self.frame = None
        self.sensitivity = sensitivity
        self.deviation = deviation
        self.is_recording = False
        self.trigger_time = 0
        self.trigger_off_timer = None
        self.trigger_enabled = False
        self.internalid = camera_internalid + '_bin'
        self.current_record_filename = None
        self.running = True

        self.kernel_erode = cv2.getStructuringElement(cv2.MORPH_RECT, (2,2))
        self.kernel_morphology = numpy.ones((6,6), dtype='uint8')
        self.contour = None #format (x,y,w,h)
        self.prev_frame = None
        self.current_frame = None
        self.next_frame = None
        self.processed_frame = None

        #init capture and read a frame
        self.capture = cv2.VideoCapture(uri)
        res, self.frame = self.capture.read()
        if not res:
            self.log.error('Motion: Unable to capture frame')
        #gray frame at t=t-1
        self.frame1gray = cv2.cvtColor(self.frame, cv2.COLOR_RGB2GRAY)
        #keep frame result
        self.frameresult = numpy.empty(self.frame.shape, dtype='uint8')
        #gray frame at t=0
        self.frame2gray = numpy.empty(self.frame.shape, dtype='uint8')

        #frame properties
        self.width = self.frame.shape[0]
        self.height = self.frame.shape[1]
        self.nb_pixels = self.width * self.height

        #create binary device
        self.connection.add_device(self.internalid, "binarysensor")

    def trigger_on(self, instant):
        """
        Enable trigger
        """
        self.trigger_time = instant
        self.trigger_enabled = True
        self.log.info('Motion: something detected by "%s" (binary sensor on for %d seconds)' % (self.internalid, self.on_duration))

        #launch timer to disable trigger
        self.trigger_off_timer = threading.Timer(float(self.on_duration), self.trigger_off)
        self.trigger_off_timer.start()

        #start recording
        if self.do_record:
            #init recorder
            self.init_recorder()
            self.log.info('Start recording "%s" during %d seconds' % (self.current_record_filename, self.record_duration))
            #and turn on recording flag
            self.is_recording = True

        #emit event
        self.connection.emit_event(self.internalid, "event.device.statechanged", 255, "")
        #TODO send pictureavailable event

    def trigger_off(self):
        """
        Disable trigger. After that motion can triggers another time
        """
        self.log.info('"%s" is off' % self.internalid)
        self.trigger_enabled = False

        #emit event
        self.connection.emit_event(self.internalid, "event.device.statechanged", 0, "")
        self.log.info('trigger_off done')

    def init_recorder(self):
        """
        Init video recorder or reset new one if already opened
        """
        self.current_record_filename = '%s_%s.%s' % (self.internalid, datetime.now().strftime("%y_%m_%d_%H_%M_%S"), 'avi')
        #@see codec available here: http://www.fourcc.org/codecs.php
        codec = cv2.cv.CV_FOURCC(*'FMP4')
        #codec = cv2.VideoWriter_fourcc(*'XVID')
        fps = 10.0
        size = (self.height, self.width)
        #make sure existing recorder doesn't already exist
        if self.recorder and self.recorder.isOpened():
            self.log.info('release recorder')
            self.recorder.release()
        self.recorder = cv2.VideoWriter(self.current_record_filename, codec, fps, size)

    def process_frame(self, frame):
        """
        Process specified frame
        @return processed frame
        """
        #save frames
        self.prev_frame = self.current_frame
        self.current_frame = self.next_frame
        self.next_frame = cv2.cvtColor(frame, cv2.COLOR_RGB2GRAY)
        if self.prev_frame==None or self.current_frame==None or self.next_frame==None:
            #not ready
            return False
  
        #apply some filter to remove fake
        d1 = cv2.absdiff(self.prev_frame, self.next_frame)
        d2 = cv2.absdiff(self.next_frame, self.current_frame)
        self.processed_frame = cv2.bitwise_and(d1, d2)
        del d1, d2
        self.processed_frame = cv2.threshold(self.processed_frame, 35, 255, cv2.THRESH_BINARY)[1]
        self.processed_frame = cv2.erode(self.processed_frame, self.kernel_erode)
        return True

    def get_contour(self):
        """
        return current motion contour
        """
        self.processed_frame = cv2.morphologyEx(self.processed_frame, cv2.MORPH_OPEN, self.kernel_morphology)
        self.processed_frame = cv2.morphologyEx(self.processed_frame, cv2.MORPH_CLOSE, self.kernel_morphology)
        contours = cv2.findContours(self.processed_frame, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)[0]
        cur_x = self.processed_frame.shape[0]
        cur_y = self.processed_frame.shape[1]
        cur_w = 0
        cur_h = 0
        for i in range(len(contours)):
            contour = contours[i]
            x,y,w,h = cv2.boundingRect(contour)
            if cur_x>x:
                cur_x = x
            if cur_y>y:
                cur_y = y
            if cur_w<(x+w):
                cur_w = x+w
            if cur_h<(y+h):
                cur_h = y+h
        if len(contours)==0:
            return False, 0, 0, 0, 0
        else:
            return True, cur_x, cur_y, cur_w, cur_h

    def something_has_moved(self):
        """
        Check if something has moved
        @see https://blog.cedric.ws/opencv-simple-motion-detection
        """
        motion = numpy.copy(self.processed_frame)
        (out_mean, out_stddev) = cv2.meanStdDev(motion)
        if out_stddev[0][0] < self.deviation:
            number_of_changes = cv2.countNonZero(motion)
            if number_of_changes>self.sensitivity:
                return True
            else:
                return False
        return False

    def run(self):
        """
        Motion process
        """
        self.log.info('Motion thread for device "%s" is running' % self.internalid)
        started = time.time()
        while self.running:
            #get frame
            res,curframe = self.capture.read()
            if not res:
                self.log.error('Motion: Unable to capture frame')
                continue
            instant = time.time()

            #Wait 5 second after the camera start for luminosity adjusting etc..
            if instant <= started+5:
                continue

            #process frame
            if not self.process_frame(curframe):
                continue

            if not self.is_recording and not self.trigger_enabled:
                if self.something_has_moved():
                    #something detected
                    self.trigger_on(instant)
            elif self.is_recording:
                #check recording state
                if instant >= self.trigger_time+self.record_duration:
                    #stop recording
                    self.log.info('Motion: Stop recording')
                    self.is_recording = False
                    self.recorder.release()
                    self.recorder = None

                    #emit event
                    self.connection.emit_event(self.internalid, "event.device.videoavailable", self.current_record_filename, "")

                elif self.recorder and self.recorder.isOpened():
                    #add detected area
                    res, x, y, w, h = self.get_contour()
                    if res:
                        cv2.rectangle(curframe ,(x,y),(w,h),(0,0,255),2)
                    #se = numpy.ones((20,20), dtype='uint8')
                    #self.processed_frame = cv2.morphologyEx(self.processed_frame, cv2.MORPH_CLOSE, se)
                    #contours = cv2.findContours(self.processed_frame, 1,2)[0]
                    #for i in range(len(contours)):
                    #    contour = contours[i]
                    #    x,y,w,h = cv2.boundingRect(contour)
                    #    cv2.rectangle(curframe ,(x,y),(x+w,y+h),(0,0,255),2)
                    #add current date
                    cv2.putText(curframe, datetime.now().strftime("%y/%m/%d %H:%M:%S"), (5,15), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0,0,255))
                    #write frame to recorder
                    self.recorder.write(curframe)

        #end of motion thread
        self.log.info('Motion thread for device "%s" is stopped' % self.internalid)

    def stop(self):
        """
        Stop thread
        """
        #stop timer if necessary
        if self.trigger_off_timer:
            self.trigger_off_timer.cancel()
            del self.trigger_off_timer

        #remove binary device
        try:
            #connection maybe not avaible when crashing
            self.connection.remove_device(self.internalid)
        except:
            pass

        #release recorder
        if self.recorder and self.recorder.isOpened():
            self.recorder.release()
            self.recorder = None

        #stop main process
        self.running = False



class Camera():
    """
    Code inspired from onvif cli
    https://github.com/quatanium/python-onvif/blob/master/onvif/cli.py
    """

    def __init__(self, connection, log, ip, port, login, password, internalid, wsdl_dir='/etc/onvif/wsdl/'):
        """
        Constructor
        @param ip: camera ip
        @param port: camera port
        @param login: camera login
        @param password: camera password
        @param internalid: internalid
        """
        self.log = log
        self.connection = connection
        self.ip = ip
        self.port = port
        self.login = login
        self.password = password
        self.uri = None
        self.uri_token = None
        self.internalid = internalid
        self.motion = False
        self.motion_sensitivity = 10
        self.motion_deviation = 20
        self.motion_on_duration = 300
        self.motion_record = 0
        self.motion_thread = None
        self._camera = None

    def cleanup(self):
        """
        """
        if self.motion_thread:
            self.motion_thread.stop()

    def configure(self):
        """
        Configure camera
        """
        try:
            #create onvif camera instance
            self._camera = ONVIFCamera(self.ip, self.port, self.login, self.password, '/etc/onvif/wsdl/')

            #register supported services
            self.onvifVersion, self.supportedServices = self._get_services()
            for service in self.supportedServices:
                if service=='media':
                    self._camera.create_media_service()
                elif service=='imaging':
                    self._camera.create_imaging_service()

            #last check
            if len(self.supportedServices)==0:
                #no service found, something went wrong with the camera
                self._camera = None

            #configure motion
            self.__configure_motion()
        except:
            self._camera = None
            self.log.exception('Exception during ONVIFCamera creation:')

        return self.is_configured()

    def __configure_motion(self):
        """
        Configure motion thread
        """
        #stop existing thread if necessary
        if self.motion_thread:
            self.log.debug('Stop motion thread')
            self.motion_thread.stop()
            self.motion_thread = None

        #start new thread if necessary
        self.log.debug('uri=%s motion=%s' % (str(self.uri), str(self.motion)))
        if self.uri and self.motion:
            self.log.debug('Start motion thread')
            self.motion_thread = Motion(self.connection, self.log, self.internalid, self.uri, self.motion_sensitivity, self.motion_deviation, self.motion_on_duration, self.motion_record)
            self.motion_thread.start()

    def set_motion(self, enable, sensitivity, deviation, on_duration, record_duration):
        """
        Set motion feature
        """
        self.motion = enable
        self.motion_sensitivity = sensitivity
        self.motion_deviation = deviation
        self.motion_on_duration = on_duration
        self.motion_record = record_duration
        self.__configure_motion()

    def set_uri(self, token, uri):
        """
        Set default uri
        """
        self.uri = uri
        self.uri_token = token

        #configure motion if necessary
        self.__configure_motion()

    def is_configured(self):
        """
        Is camera successfully configured?
        @return True/False
        """
        if self._camera:
            return True
        else:
            return False

    def __check_service(self, service):
        """
        Check specified service
        """
        if not service in self._camera.services_template.keys():
            return False
        if not self._camera.services_template[service]:
            self.log.error('Create service "%s" before trying to get its capabilities')
            return False
        return True

    def __parse_instance(self, data):
        out = {}
        for key in data.__keylist__:
            newkey = key
            if newkey.startswith('_'):
                newkey = newkey[1:]
            if isinstance(data[key], (Text,bool,int,float)):
                out[newkey] = data[key]
            elif isinstance(data[key], (list)):
                out[newkey] = self.__parse_list(data[key])
            elif isinstance(data[key], (dict)):
                out[newkey] = self.__parse_dict(data[key])
            else:
                out[newkey] = self.__parse_instance(data[key])
        return out

    def __parse_list(self, data):
        out = []
        for item in data:
            if isinstance(item, (Text,bool,int,float)):
                out.append(item)
            elif isinstance(item, (list)):
                out.append(self.__parse_list(item))
            elif isinstance(item, (dict)):
                out.append(self.__parse_dict(item))
            else:
                out.append(self.__parse_instance(item))
        return out

    def __parse_dict(self, data):
        out = {}
        for key in data.keys():
            newkey = key
            if newkey.startswith('_'):
                newkey = newkey[1:]
            if isinstance(data[key], (Text,bool,int,float)):
                out[newkey] = data[key]
            elif isinstance(data[key], (list)):
                out[newkey] = self.__parse_list(data[key])
            elif isinstance(data[key], (dict)):
                out[newkey] = self.__parse_dict(data[key])
            else:
                out[newkey] = self.__parse_instance(data[key])
        return out

    def handle_onvif_response(self, data):
        """
        Parse onvif response to something jsonable
        """
        out = None
        try:
            if isinstance(data, (Text,bool,int,float)):
                out = data
            elif isinstance(data, (list)):
                out = self.__parse_list(data)
            elif isinstance(data, (dict)):
                out = self.__parse_dict(data)
            else:
                out = self.__parse_instance(data)
        except:
            self.log.exception('Unable to parse onvif response')
        return out

    def do_operation(self, service, operation, params=None):
        """
        Execute operation on device
        @see available operations: http://www.onvif.org/onvif/ver20/util/operationIndex.html
        @return response or None if request failed
        """
        if not self.__check_service(service):
            return None

        if not params:
            params = {}

        try:
            #get ONVIF service
            service = self._camera.get_service(service)
            #execute operation
            resp = getattr(service, operation)(params)
        except MethodNotFound as err:
            self.log.error('No operation found: %s' % operation)
            return None
        except:
            self.log.exception('Exception during do_operation:')
            return None

        if resp:
            return self.handle_onvif_response(resp)
        else:
            #nothing return but no error
            return {}
        
    def _get_services(self):
        """
        Get device supported services
        @return onvif version, list of supported services
        """
        version = '0.0'
        services = []
        r = self.do_operation('devicemgmt', 'GetServices')
        if r:
            try:
                for item in r:
                    if item.has_key('XAddr'):
                        if item['XAddr'].find('device_service')!=-1:
                            #get onvif version
                            version = '%s.%s' % (item['Version']['Major'], item['Version']['Minor'])
                        else:
                            #get supported service
                            for service in self._camera.services_template.keys():
                                if item['XAddr'].lower().find(service)!=-1:
                                    services.append(service)
            except:
                self.log.exception('Exception during getServices:')
        else:
            self.log.error('Unable to get device supported services')
        return version, services

    def get_service_capabilities(self, service):
        """
        Return specified service capabilities
        """
        if not self.__check_service(service):
            return None

        return self.do_operation(service, 'GetServiceCapabilities')

    def get_profiles(self):
        """
        Return camera profiles
        """
        return self.do_operation('media', 'GetProfiles')

    def get_profile_tokens(self, profiles=None):
        """
        Return profile tokens list
        """
        if not profiles:
            profiles = self.get_profiles()
        tokens = []
        for profile in profiles:
            tokens.append(profile['token'])
        return tokens

    def get_uri(self, token):
        """
        Return stream uri
        """
        uris = self.get_uris([token])
        if uris.has_key(token):
            return uris[token]
        else:
            return None

    def get_uris(self, tokens):
        """
        Return stream uris
        """
        uris = {}
        for token in tokens:
            params = {'ProfileToken': token, 'StreamSetup': {'Stream': 'RTP-Unicast', 'Transport': {'Protocol': 'UDP'}}}
            uri = self.do_operation('media', 'GetStreamUri', params)
            if uri and uri['Uri']:
                uris[token] = uri['Uri']
            else:
                self.log.warning('Invalid structure GetStreamUri.Uri')
        return uris

    def capture_frame(self):
        """
        Get single camera frame
        @return: success(True/False), frame/error message:
        """
        if self.uri:
            cap = cv2.VideoCapture(self.uri)
            ret,frame = cap.read()
            cap.release()
            return ret,frame
        else:
            self.log.warning('No URI configured')
            return False,'No URI configured'

    def save_frame(self, frame, filename):
        """
        Save frame to specified filename
        @return True if file writing succeed   
        """
        return cv2.imwrite(filename, frame)

    def get_string_buffer(self, frame, format='.jpg', quality=50):
        """
        Return frame into jpeg string buffer
        @info if quality is too high, qpid reject picture and crash module
        @return True/False, buffer or None if error occured
        """
        try:
            params = []
            if format=='.jpeg' or format=='.jpg':
                params = [int(cv2.IMWRITE_JPEG_QUALITY), quality]
            elif format=='.png':
                params = [int(cv2.IMWRITE_PNG_COMPRESSION), quality]
    
            #imencode output: result, buffer
            ret,buf = cv2.imencode('.jpg', frame, params)
            if ret:
                return True, buf.tostring()
            else:
                #error during conversion
                return False, None
        except Exception as e:
            self.log.exception('Exception in getBuffer:')
            return False, None 


class AgoOnvif(agoclient.AgoApp):
    """
    Agocontrol ONVIF
    """
    def setup_app(self):
        """
        Configure application
        """
        #declare members
        self.config = {}
        self.cameras = {}
        self.__configLock = threading.Lock()

        #load config
        self.load_config()

        #add handlers
        self.connection.add_handler(self.message_handler)
        #add controller
        self.connection.add_device('pyonvifcontroller', 'pyonvifcontroller')

        #restore existing devices
        self.log.info('Restoring cameras:')
        self.__configLock.acquire()
        for internalid in self.config:
            res, msg = self.create_camera(self.config[internalid]['ip'], self.config[internalid]['port'], self.config[internalid]['login'], self.config[internalid]['password'], False)
            if not res:
                self.log.warning(' - Camera "%s" [FAILED]' % internalid)
            else:
                #get camera
                self.log.info(' - Camera "%s"' % internalid)
                camera = self.cameras[internalid]
                camera.set_uri(self.config[internalid]['uri_token'], self.config[internalid]['uri'])
                camera.set_motion(self.config[internalid]['motion'], self.config[internalid]['motion_sensitivity'], self.config[internalid]['motion_deviation'], self.config[internalid]['motion_on_duration'], self.config[internalid]['motion_record'])
                self.connection.add_device(internalid, 'camera')
        self.__configLock.release()

    def cleanup_app(self):
        """
        Clean all needed
        """
        #cleanup all camera instances
        for internalid in self.cameras:
            self.cameras[internalid].cleanup()
        self.cameras.clear()

    def save_config(self):
        """
        Save config to map file
        """
        global DEVICEMAPFILE
        self.__configLock.acquire()
        error = False
        try:
            f = open(DEVICEMAPFILE, "w")
            f.write(json.dumps(self.config))
            f.close()
        except:
            self.log.exception('Unable to save config infos')
            error = True
        self.__configLock.release()
        return not error

    def load_config(self):
        """
        Load config from map file
        """
        global DEVICEMAPFILE
        self.__configLock.acquire()
        error = False
        try:
            if os.path.exists(DEVICEMAPFILE):
                f = open(DEVICEMAPFILE, "r")
                j = f.readline()
                f.close()
                self.config = json.loads(j)
            else:
                self.log.debug('Create default empty config file "%s"' % DEVICEMAPFILE)
                f = open(DEVICEMAPFILE, "w")
                f.write(json.dumps({}))
                f.close()
                self.config = {}
        except:
            self.log.exception('Unable to load devices infos')
            error = True
        self.__configLock.release()
        return not error

    def create_camera(self, ip, port, login, password, saveConfig=False):
        """
        Create new camera instance and store it into cameras member
        Can save camera instance to config file too
        @return True/False, instance or error message
        """
        internalid = ip

        #check if camera not already added
        if self.cameras.has_key(internalid):
            msg = 'Camera "%s" already added. Nothing done' % internalid
            self.log.error(msg)
            return False, msg
    
        #create new instance
        camera = Camera(self.connection, self.log, ip, port, login, password, internalid)
        if not camera.configure():
            #problem during camera configuration!
            msg = 'Unable to create camera "%s". Check parameters'
            self.log.error(msg)
            return False, msg
        else:
            #save new instance locally
            self.cameras[internalid] = camera
            config = {'ip':ip, 'port':port, 'login':login, 'password':password, 'uri_token':None, 'uri':None, 'motion':False, 'motion_sensitivity':10, 'motion_deviation':20, 'motion_record':0, 'motion_on_duration':300}

            #save device map
            if saveConfig:
                self.config[internalid] = config
                self.save_config()

            return True, ''

    def create_temp_camera(self, ip, port, login, password):
        """
        Create temp camera instance and return it
        @return instance or None if problem
        """
        internalid = ip

        #create new instance
        camera = Camera(self.connection, self.log, ip, port, login, password, ip)
        if not camera.configure():
            #problem during camera configuration!
            return None
        else:
            #return instance
            return camera

    def delete_camera(self, internalid):
        """
        Delete camera
        @return True/False, '' or error message
        """
        if not self.cameras.has_key(internalid):
            msg = 'Camera "%s" not found. Unable to delete it' % internalid
            self.log.error(msg)
            return False, msg

        #delete camera from internal structures
        self.cameras.pop(internalid)
        self.config.pop(internalid)
        self.save_config()

        return True, ''

    def __check_command_params(self, content, params):
        """
        Check if content is containing awaited parameters
        @return True if all params are in content, False otherwise
        """
        for param in params:
            if not content.has_key(param):
                return False
        return True

    def __check_ip_port(self, ip, port):
        """
        Check if port is integer and ip is valid
        @return True/False, '' or error message
        """
        try:
            int(port)
        except ValueError:
            return False, 'Port must be a number'

        #TODO check ipv6
        if ip.count('.')!=3:
            return False, 'Ip must a valid IPv4'

        return True, ''

    def message_handler(self, internalid, content):
        """
        Message handler
        """
        self.log.debug('internalid=%s : %s' % (internalid, content))

        #get command
        command = None
        if content.has_key('command'):
            command = content['command']
        if not command:
            self.log.error('No command specified')
            return self.connection.response_failed(mess='No command specified')

        if internalid=='pyonvifcontroller':
            #handle controller commands

            if command=='addcamera':
                if self.__check_command_params(content, ['ip', 'port', 'login', 'password', 'uri_token', 'uri_desc']):
                    #check params
                    res,msg = self.__check_ip_port(content['ip'], content['port'])
                    if not res:
                        return self.connection.response_failed(mess=msg)

                    #create new camera
                    content['port'] = int(content['port'])
                    internalid = content['ip']
                    res, msg = self.create_camera(content['ip'], content['port'], content['login'], content['password'], True)
                    if res:
                        #get created camera
                        camera = self.cameras[internalid]

                        #get uri from token
                        uri = camera.get_uri(content['uri_token'])
                        self.log.debug('URI=%s' % uri)
                        if uri:
                            #set and save uri
                            camera.set_uri(content['uri_token'], uri)
                            self.config[internalid]['uri'] = uri
                            self.config[internalid]['uri_token'] = content['uri_token']
                            self.config[internalid]['uri_desc'] = content['uri_desc']
                            self.save_config()
                        else:
                            msg = 'Problem getting camera URI with token "%s"' % content['uri_token']
                            self.log.error(msg)
                            return self.connection.response_failed(mess='Camera not added: %s' % msg)

                        #create new camera device
                        self.connection.add_device(internalid, 'camera')
                        self.log.info('New camera "%s" added' % internalid)

                        return self.connection.response_success()
                    else:
                        self.log.error('Camera not added: %s' % msg)
                        return self.connection.response_failed(mess='camera not added: %s' % msg)
                else:
                    self.log.error('Parameters are missing')
                    return self.connection.response_error(iden=self.connection.RESPONSE_ERR_MISSING_PARAMETERS)

            elif command=='deletecamera':
                if self.__check_command_params(content, ['internalid']):
                    if self.cameras.has_key(content['internalid']):
                        #delete local content
                        self.delete_camera(content['internalid'])

                        #delete device from ago
                        self.connection.remove_device(content['internalid'])

                        return self.connection.response_success()
                    else:
                        self.log.error('Camera with internalid "%s" was not found' % internalid)
                        return self.connection.response_failed(mess='Camera with internalid "%s" was not found' % internalid)
                else:
                    self.log.error('Parameters are missing')
                    return self.connection.response_error(iden=self.connection.RESPONSE_ERR_MISSING_PARAMETERS)

            elif command=='getprofiles':
                #different behaviour according to specified parameters
                fromExisting = False
                if self.__check_command_params(content, ['ip', 'port', 'login', 'password']):
                    fromExisting = False
                elif self.__check_command_params(content, ['internalid']):
                    fromExisting = True
                else:
                    self.log.error('Parameters are missing')
                    return self.connection.response_error(iden=self.connection.RESPONSE_ERR_MISSING_PARAMETERS)

                if not fromExisting:
                    #create temp camera
                    #check params
                    res,msg = self.__check_ip_port(content['ip'], content['port'])
                    if not res:
                        return self.connection.response_failed(mess=msg)

                    content['port'] = int(content['port'])
                    camera = self.create_temp_camera(content['ip'], content['port'], content['login'], content['password'])
                    if not camera:
                        msg = 'Unable to connect to camera. Check parameters'
                        self.log.warning(msg)
                        return self.connection.response_failed(mess=msg)
                else:
                    #get camera instance from instance container
                    internalid = content['internalid']
                    if not self.cameras.has_key(internalid):
                        self.log.error('Camera with internalid "%s" was not found' % internalid)
                        return self.connection.response_failed(mess='Camera with internalid "%s" was not found' % internalid)
                    camera = self.cameras[internalid]

                ps = camera.get_profiles()
                profiles = []
                if ps:
                    #keep usefull profile infos
                    for p in ps:
                        profile = {}
                        if p.has_key('VideoEncoderConfiguration'):
                            profile['name'] = p['Name']
                            profile['uri_token'] = p['token']
                            profile['encoding'] = p['VideoEncoderConfiguration']['Encoding']
                            resol = {}
                            resol['width'] = p['VideoEncoderConfiguration']['Resolution']['Width']
                            resol['height'] = p['VideoEncoderConfiguration']['Resolution']['Height']
                            profile['resolution'] = resol
                            profile['quality'] = p['VideoEncoderConfiguration']['Quality']
                        if p.has_key('VideoSourceConfiguration'):
                            bounds = {}
                            bounds['x'] = p['VideoSourceConfiguration']['Bounds']['x']
                            bounds['y'] = p['VideoSourceConfiguration']['Bounds']['x']
                            bounds['width'] = p['VideoSourceConfiguration']['Bounds']['width']
                            bounds['height'] = p['VideoSourceConfiguration']['Bounds']['height']
                            profile['boundaries'] = bounds
                        profiles.append(profile)
                    return self.connection.response_success(data=profiles)
                else:
                    #no profiles (parsing error?)
                    self.log.error('No profile found')
                    return self.connection.response_failed(mess='No profile found')

            elif command=='dooperation':
                if not self.__check_command_params(content, ['internalid', 'service', 'operation', 'params']):
                    self.log.error('Parameters are missing')
                    return self.connection.response_error(iden=self.connection.RESPONSE_ERR_MISSING_PARAMETERS)
                internalid = content['internalid']

                if not self.cameras.has_key(internalid):
                    self.log.error('Camera with internalid "%s" was not found' % internalid)
                    return self.connection.response_failed(mess='Camera with internalid "%s" was not found' % internalid)
                camera = self.cameras[internalid]
                
                #execute command
                resp = camera.do_operation(content['service'], content['operation'], content['params'])
                if resp==None:
                    self.log.error('Operation failed')
                    return self.connection.response_failed(mess='Operation failed')
                else:
                    return self.connection.response_success(data=resp)

            elif command=='getcameras':
                return self.connection.response_success(data=self.config)

            elif command=='updatecredentials':
                if not self.__check_command_params(content, ['internalid', 'login', 'password']):
                    self.log.error('Parameters are missing')
                    return self.connection.response_error(iden=self.connection.RESPONSE_ERR_MISSING_PARAMETERS)
                internalid = content['internalid']

                if not self.cameras.has_key(internalid):
                    self.log.error('Camera with internalid "%s" was not found' % internalid)
                    return self.connection.response_failed(mess='Camera with internalid "%s" was not found' % internalid)
                camera = self.cameras[internalid]

                #update credentials on camera first
                resp = camera.do_operation('devicemgmt', 'SetUser', {'User':{'Username':content['login'], 'Password':content['password'], 'UserLevel':'Administrator'}})
                if not resp:
                    msg = 'Unable to set credentials on camera'
                    self.log.error(msg)
                    return self.connection.response_failed(mess=msg)

                #TODO update uri

                #then update on controller
                self.config[internalid].login = content['login']
                self.config[internalid].password = content['password']
                self.save_config()

                return self.connection.response_success()

            elif command=='getdeviceinfos':
                if not self.__check_command_params(content, ['internalid']):
                    self.log.error('Parameters are missing')
                    return self.connection.response_error(iden=self.connection.RESPONSE_ERR_MISSING_PARAMETERS)
                internalid = content['internalid']

                if not self.cameras.has_key(internalid):
                    self.log.error('Camera with internalid "%s" was not found' % internalid)
                    return self.connection.response_failed(mess='Camera with internalid "%s" was not found' % internalid)
                camera = self.cameras[internalid]

                #get device infos
                resp = camera.do_operation('devicemgmt', 'GetDeviceInformation')
                if not resp:
                    msg = 'Unable to get device infos'
                    self.log.error(msg)
                    return self.connection.response_failed(mess=msg)

                #prepare output
                self.log.debug(resp)
                out = {}
                out['manufacturer'] = resp['Manufacturer']
                out['model'] = resp['Model']
                out['firmwareversion'] = resp['FirmwareVersion']
                out['serialnumber'] = resp['SerialNumber']
                out['hardwareid'] = resp['HardwareId']
                
                return self.connection.response_success(data=out)

            elif command=='setcameraprofile':
                if not self.__check_command_params(content, ['internalid']):
                    self.log.error('Parameters are missing')
                    return self.connection.response_error(iden=self.connection.RESPONSE_ERR_MISSING_PARAMETERS)
                internalid = content['internalid']

                if not self.cameras.has_key(internalid):
                    self.log.error('Camera with internalid "%s" was not found' % internalid)
                    return self.connection.response_failed(mess='Camera with internalid "%s" was not found' % internalid)
                camera = self.cameras[internalid]

                #get uri from token
                uri = camera.get_uri(content['uri_token'])
                self.log.debug('URI=%s' % uri)
                if uri:
                    #set and save uri
                    camera.set_uri(content['uri_token'], uri)
                    self.config[internalid]['uri'] = uri
                    self.config[internalid]['uri_token'] = content['uri_token']
                    self.config[internalid]['uri_desc'] = content['uri_desc']
                    self.save_config()
                else:
                    msg = 'Problem getting camera URI with token "%s"' % content['uri_token']
                    self.log.error(msg)
                    return self.connection.response_failed(mess='Camera not added: %s' % msg)

                return self.connection.response_success()

            elif command=='setmotion':
                if not self.__check_command_params(content, ['internalid', 'enable', 'sensitivity', 'deviation', 'onduration', 'recordingduration']):
                    self.log.error('Parameters are missing')
                    return self.connection.response_error(iden=self.connection.RESPONSE_ERR_MISSING_PARAMETERS)
                internalid = content['internalid']

                if not self.cameras.has_key(internalid):
                    self.log.error('Camera with internalid "%s" was not found' % internalid)
                    return self.connection.response_failed(mess='Camera with internalid "%s" was not found' % internalid)
                camera = self.cameras[internalid]

                #check parameters
                if content['enable']:
                    content['enable'] = True
                else:
                    content['enable'] = False

                try:
                    content['sensitivity'] = int(content['sensitivity'])
                except ValueError:
                    msg = 'Invalid "sensitivity" parameter. Must be integer'
                    self.log.error(msg)
                    self.connection.response_error(msg=msg)

                try:
                    content['deviation'] = int(content['deviation'])
                except ValueError:
                    msg = 'Invalid "deviation" parameter. Must be integer'
                    self.log.error(msg)
                    self.connection.response_error(msg=msg)
                    
                try:
                    content['onduration'] = int(content['onduration'])
                except ValueError:
                    msg = 'Invalid "on duration" parameter. Must be integer'
                    self.log.error(msg)
                    self.connection.response_error(msg=msg)

                try:
                    content['recordingduration'] = int(content['recordingduration'])
                except ValueError:
                    msg = 'Invalid "recording duration" parameter. Must be integer'
                    self.log.error(msg)
                    self.connection.response_error(msg=msg)

                #configure motion
                camera.set_motion(content['enable'], content['sensitivity'], content['deviation'], content['onduration'], content['recordingduration'])

                #save new config
                self.config[internalid]['motion'] = content['enable']
                self.config[internalid]['motion_sensitivity'] = content['sensitivity']
                self.config[internalid]['motion_deviation'] = content['deviation']
                self.config[internalid]['motion_on_duration'] = content['onduration']
                self.config[internalid]['motion_record'] = content['recordingduration']
                self.save_config()

            else:
                #request not handled
                self.log.error('Unhandled request')
                return self.connection.response_error(iden=self.connection.RESPONSE_ERR_UNKNOWN_COMMAND)

        elif self.cameras.has_key(internalid):
            #handle device command
            camera = self.cameras[internalid]

            if command=='getvideoframe':
                #get uri
                ret,data = camera.capture_frame()
                if ret:
                    #get sendable frame as buffer
                    ret,buf = camera.get_string_buffer(data)
                    if ret:
                        data = {}
                        data['image'] = base64.b64encode(buf)
                        return self.connection.response_success(data=data)
                    else:
                        self.log.error('No buffer retrieved [%s]' % data)
                        return self.connection.response_failed(mess=data)
                else:
                    self.log.error('No buffer retrieved [%s]' % data)
                    return self.connection.response_failed(mess=data)

            else:
                #request not handled
                self.log.error('Unhandled request')
                return self.connection.response_error(iden=self.connection.RESPONSE_ERR_UNKNOWN_COMMAND)

        else:
            #request not handled
            self.log.error('Unhandled request')
            return self.connection.response_error(iden=self.connection.RESPONSE_ERR_UNKNOWN_COMMAND)




if __name__=="__main__":
    a = AgoOnvif();
    a.main()

    """
    import logging
    logger = logging.getLogger('TEST')
    cam = Camera(logger, '192.168.1.12', 8899, 'admin', 'plijygr', '192.168.1.12', None)
    cam.configure()

    #get tokens
    tokens = cam.get_profile_tokens()
    logging.info('Tokens=%s' % str(tokens))

    #get uris
    uris = cam.get_uris(tokens)
    logging.info('URIS: %s' % str(uris))
    """

    """
    #get profiles
    profiles = cam.get_profiles()
    logging.info(profiles)
    """

    """
    #get capabilities
    capa = cam.getServiceCapabilities('devicemgmt')
    #logging.info(capa)
    """

    """
    #get services
    v,s = cam._getServices()
    logging.info('onvif version: %s' % v)
    logging.info('supported services: %s' % s)
    """

    """
    #capture frame
    r,f = cam.captureFrame(uris[tokens[0]])
    if r:
        r = cam.saveFrame(f, 'capture.jpg')
    if r:
        logging.info('Frame captured and saved successfully')
    else:
        logging.error('Error during frame capture')
    """

    """
    #get capability
    r,v = cam.doOperation('devicemgmt', 'GetDeviceInformation')
    if r:
        logging.info('DeviceMgmt.GetHostname = %s' % str(v))
    """

    """
    #set capability
    r,v = cam.doOperation('devicemgmt', 'SetHostname', {'Name':'Tang'})
    if r:
        logging.info('DeviceMgmt.SetHostname updated successfully')
    """



    #motion detection
    #cap = cv2.VideoCapture(uris['000'])
    #winName = "Movement Indicator"
    #cv2.namedWindow(winName, cv2.CV_WINDOW_AUTOSIZE)
    #t_minus = cv2.cvtColor(cap.read()[1], cv2.COLOR_RGB2GRAY)
    #t = cv2.cvtColor(cap.read()[1], cv2.COLOR_RGB2GRAY)
    #t_plus = cv2.cvtColor(cap.read()[1], cv2.COLOR_RGB2GRAY)

    #count = 0

    #while True:
    #dimg = diffImg(t_minus, t, t_plus)
    #cv2.imshow( winName, dimg )

    #print "%d - %d" % (cv2.countNonZero(dimg), dimg.size)
    #per = float(cv2.countNonZero(dimg)) / float(dimg.size) * float(100)
    #print per

    #threshold
    #if per>10:
    #    count += 1
    #else:
    #    count = 0
    #remove fake
    #if count>=10:
    #    print 'intruder'
    #    count = 0

    # Read next image
    #t_minus = t
    #t = t_plus
    #t_plus = cv2.cvtColor(cap.read()[1], cv2.COLOR_RGB2GRAY)

    #ret,frame = cap.read()
    #key = cv2.waitKey(10)
    #if key%256 == 27:
    #    cv2.destroyWindow(winName)
    #    break
    #time.sleep(0.1)
        
