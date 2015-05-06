#! /usr/bin/python
#-*-coding=utf-8

import time
import threading
import os
import json
import base64
from datetime import datetime
import Queue
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

class FrameReader(threading.Thread):
    """
    Init a video capture and push read frame to queue
    Drop frames if consumer cannot follow camera frame rate
    """

    def __init__(self, log, uri, max_frames=10):
        threading.Thread.__init__(self)
        self.running = True
        self.log = log
        self.uri = uri
        self.q = Queue.Queue(max_frames)
        self.resolution = [0,0]

    def stop(self):
        """
        Stop thread
        """
        self.running = False

    def run(self):
        """
        Read frame
        """
        #init capture
        capture = cv2.VideoCapture(self.uri)
        started = time.time()
        while self.running:
            #get current time
            now = time.time()

            #read frame
            res, frame = capture.read()

            #save resolution
            if self.resolution[0]==0:
                self.log.debug('Capture resolution : %dx%d' % (frame.shape[1], frame.shape[0]))
                self.resolution = (frame.shape[1], frame.shape[0])

            #drop first frames to adjust camera parameters
            if now <= started+5:
                continue

            #queue frame
            if res:
                #check if full
                if self.q.full():
                    #drop first frame
                    self.q.get_nowait()
                #put new frame
                self.q.put_nowait(frame)
                

    def get_frame(self):
        """
        Return oldest frame
        @return: True,oldest frame / False,None
        """
        if not self.q.empty():
            return True, self.q.get_nowait()
        else:
            return False, None

    def get_resolution(self):
        """
        Return frame resolution
        """
        return self.resolution


class FrameRecorder(threading.Thread):
    """
    Frame recorder. Store frames to specified file
    """

    def __init__(self, log, codec=None):
        """
        Constructor
        @param codec: encoding codec (see http://www.fourcc.org/codecs.php). Default is MP4
        """
        threading.Thread.__init__(self)
        self.running = True
        self.log = log
        self.q = Queue.Queue()
        if not codec:
            self.codec = cv2.cv.CV_FOURCC(*'FMP4')
        else:
            self.codec = codec
        self.recorder = None
        self.close_recorder = False
        self.recording = False

    def stop(self):
        """
        Stop thread
        """
        self.running = False
        self.stop_recording(True)

    def run(self):
        """
        Store frames to output file
        """
        while self.running:
            if self.recorder:
                if self.close_recorder and self.close_recorder_force:
                    #do not check queue, stop recorder right now!
                    self.__close()
                elif not self.q.empty():
                    #store frame to recorder
                    self.recorder.write( self.q.get_nowait() )
                elif self.close_recorder:
                    #close recorder only when queue is empty
                    self.__close()
                    
                #small pause. Recorder hasn't priority
                time.sleep(0.1)
            else:
                #long pause
                time.sleep(1)

    def __close(self):
        """
        Close recorder
        """
        if self.recorder:
            self.recording = False
            self.recorder.release()
            self.recorder = None

    def start_recording(self, filename, fps, resolution):
        """
        Start recording (open recorder)
        @param filename: name of output file
        @param fps: video frame rate
        @param resolution: video resolution
        """
        #update members
        self.close_recorder = False
        self.close_recorder_force = False

        #make sure existing recorder doesn't already exist
        if self.recorder and self.recorder.isOpened():
            self.stop_recording(True)

        #wait until frame recorder is available
        while self.recorder:
            time.sleep(0.1)

        #create new one
        self.recorder = cv2.VideoWriter(filename, self.codec, fps, resolution)
        self.recording = True

    def stop_recording(self, force=False):
        """
        Stop recording (saying no more frame will be added)
        """
        self.close_recorder = True
        self.close_recorder_force = force

    def record(self, frame):
        """
        Push specified frame to recorder
        @return False if frame is dropped
        """
        if self.recorder:
            self.q.put_nowait(frame)
            return True
        else:
            return False

    def is_recording(self):
        """
        Return True if recorder is recording
        """
        return self.recording


class Motion(threading.Thread):
    """
    Code inspired from Robin David (converted to cv2):
    https://github.com/RobinDavid/Motion-detection-OpenCV
    And from Cédric Verstraeten:
    https://blog.cedric.ws/opencv-simple-motion-detection
    """

    CONTOUR_NONE = 0
    CONTOUR_SINGLE = 1
    CONTOUR_MULTIPLE = 2

    RECORD_NONE = 0
    RECORD_MOTION = 1
    RECORD_DAY = 2
    RECORD_ALL = 3

    def __init__(self, connection, log, camera_internalid, uri, name, sensitivity=10 , deviation=20, on_duration=300, record_type=0, record_uri=None, record_fps=10, record_dir=None, record_duration=15, record_contour=0):
        """
        Constructor
        @param camera_internalid: camera internalid to generate record filename
        @param uri: video stream uri
        @param name: device name (inventory)
        @param sensitivity: number of changes on picture detected
        @param deviation: the higher the value, the more motion is allowed
        @param on_duration: time during binary sensor stays on when motion detected
        @param record_type: recording type [Disabled, Only motion, Only all day, All]
        @param record_uri: recording uri
        @param record_fps: camera fps rate
        @param record_dir: directory to save recordings
        @param record_duration: duration of recording
        @param record_contour: contour of detected area
        """
        threading.Thread.__init__(self)

        #parameters
        self.connection = connection
        self.log = log
        self.internalid_camera = camera_internalid
        self.internalid_binary = camera_internalid + '.motion'
        self.uri = uri
        self.sensitivity = sensitivity
        self.deviation = deviation
        self.on_duration = on_duration
        self.record_type = record_type
        self.record_uri = record_uri
        self.record_fps = record_fps
        self.record_dir = record_dir
        self.record_duration = record_duration
        self.record_contour = record_contour
        self.name = name

        #members
        self.trigger_time = 0
        self.trigger_off_timer = None
        self.trigger_enabled = False
        self.record_filename = None
        self.running = True
        self.recorders = {'record':None, 'timelapse':None}
        self.is_recording = False
        self.frame_readers = {'motion':None, 'record':None}
        self.record_resolution = [0,0]
        self.record_resolution_ratio = [0,0]

        #opencv
        self.kernel_erode = cv2.getStructuringElement(cv2.MORPH_RECT, (2,2))
        self.kernel_morphology = numpy.ones((6,6), dtype='uint8')
        self.prev_frame = None
        self.current_frame = None
        self.next_frame = None
        self.processed_frame = None

        #check members
        if not self.record_uri and self.record_type!=self.RECORD_NONE:
            #no record uri
            self.log.warning('No record uri specified while recording is enabled. Recording is disabled.')
            self.record_type = self.RECORD_NONE

        if not self.record_dir or not os.path.exists(self.record_dir):
            #record dir doesn't exist
            self.log.warning('Recording dir "%s" doens\'t exist. Recording is disabled' % self.record_dir)
            self.record_type = self.RECORD_NONE

        #create binary device
        self.connection.add_device(self.internalid_binary, "binarysensor")

    def trigger_on(self, current_time):
        """
        Enable trigger
        """
        self.trigger_time = current_time
        self.trigger_enabled = True
        self.log.info('Motion: something detected by "%s" (binary sensor on for %d seconds)' % (self.internalid_binary, self.on_duration))

        #launch timer to disable trigger
        self.trigger_off_timer = threading.Timer(float(self.on_duration), self.trigger_off)
        self.trigger_off_timer.start()

        #start recording
        if self.record_type in (self.RECORD_MOTION, self.RECORD_ALL):
            #get frame resolution
            if self.frame_readers['record']:
                self.record_resolution = self.frame_readers['record'].get_resolution()
                #and compute ratio
                motion_resolution = self.frame_readers['motion'].get_resolution()
                self.record_resolution_ratio[0] = self.record_resolution[0] / motion_resolution[0]
                self.record_resolution_ratio[1] = self.record_resolution[1] / motion_resolution[1]
            else:
                self.record_resolution = self.frame_readers['motion'].get_resolution()
                #neutral ratio
                self.record_resolution_ratio = (1,1)

            #init recorder
            self.record_filename = os.path.join(self.record_dir, 'motion_%s_%s.%s' % (self.internalid_camera, datetime.now().strftime("%Y_%m_%d_%H_%M_%S"), 'avi'))
            self.recorders['record'].start_recording(self.record_filename, self.record_fps, self.record_resolution)
            self.log.info('Start recording "%s" [%dx%d@%dfps] during %d seconds' % (self.record_filename, self.record_resolution[0], self.record_resolution[1], self.record_fps, self.record_duration))

            #finally turns on recording flag
            self.is_recording = True

        #emit event
        self.connection.emit_event(self.internalid_binary, "event.device.statechanged", 255, "")
        #TODO send pictureavailable event

    def trigger_off(self):
        """
        Disable trigger. After that motion can triggers another time
        """
        self.log.debug('"%s" is off' % self.internalid_binary)
        self.trigger_enabled = False

        #emit event
        self.connection.emit_event(self.internalid_binary, "event.device.statechanged", 0, "")
        self.log.info('trigger_off done')

    def init_frame_readers(self):
        """
        Init frame readers
        """
        #create motion frame reader
        self.log.debug('Init motion frame reader')
        self.frame_readers['motion'] = FrameReader(self.log, self.uri)
        self.frame_readers['motion'].start()

        #create recording frame reader if necessary
        if self.record_type!=self.RECORD_NONE and self.uri!=self.record_uri:
            #need to create another frame reader
            self.log.debug('Init record frame reader')
            self.frame_readers['record'] = FrameReader(self.log, self.record_uri, 2)
            self.frame_readers['record'].start()

    def init_frame_recorders(self):
        """
        Init frame recorders
        """
        #create record recorder if necessary
        if self.record_type in (self.RECORD_MOTION, self.RECORD_ALL):
            self.recorders['record'] = FrameRecorder(self.log)
            self.recorders['record'].start()

        #create day recorder if necessary
        if self.record_type in (self.RECORD_DAY, self.RECORD_ALL):
            self.recorders['timelapse'] = FrameRecorder(self.log)
            self.recorders['timelapse'].start()

    def process_frame(self, frame):
        """
        Process specified frame
        @return False if computation is not possible
        """
        #self.log.info('process frame %dx%d' % (frame.shape[0], frame.shape[1]))
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

    def get_contours(self):
        """
        Return current motion contour (based on processed_frame, so use process_frame before using it)
        """
        rects = []

        #get all contours
        #TODO check if creating new instance is necessary
        processed_frame = cv2.morphologyEx(self.processed_frame, cv2.MORPH_OPEN, self.kernel_morphology)
        processed_frame = cv2.morphologyEx(processed_frame, cv2.MORPH_CLOSE, self.kernel_morphology)
        contours = cv2.findContours(processed_frame, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)[0]

        #return error if no coutours found
        if len(contours)==0:
            return False, rects

        #merge all countours in single one if necessary
        if self.record_contour==self.CONTOUR_SINGLE:
            cur_x = processed_frame.shape[0]
            cur_y = processed_frame.shape[1]
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
            rects.append({'x':cur_x, 'y':cur_y, 'w':cur_w, 'h':cur_h})
        else:
            #return all contours
            for i in range(len(contours)):
                contour = contours[i]
                x,y,w,h = cv2.boundingRect(contour)
                rects.append({'x':x, 'y':y, 'w':w, 'h':h})

        return True, rects

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
        self.log.info('Motion thread for device "%s" is running' % self.internalid_camera)

        #init frame readers
        self.init_frame_readers()

        #init recorders
        self.init_frame_recorders()

        #main loop
        started = time.time()
        last_day_frame = started
        while self.running:
            #get current time
            now = int(time.time())

            #get motion frame
            res, frame = self.frame_readers['motion'].get_frame()
            if not res:
                #no frame
                time.sleep(0.1)
                continue

            #process motion frame
            if not self.process_frame(frame):
                continue

            if not self.is_recording and not self.trigger_enabled:
                #not recording, is something moved?
                if self.something_has_moved(): #debug or now>=started+15:
                    #something detected
                    self.trigger_on(now)

            elif self.is_recording:
                #recording in progress
                #check if something moved
                if self.something_has_moved():
                    #extend recording time
                    self.trigger_time = now
            
                #check its state
                if now >= self.trigger_time+self.record_duration:
                    #stop recording
                    self.log.info('Motion: Stop recording')
                    self.is_recording = False
                    self.recorders['record'].stop_recording()

                    #emit event
                    self.connection.emit_event(self.internalid_camera, "event.device.videoavailable", self.record_filename, "")

                else:
                    #get frame to store
                    ok = True
                    if self.frame_readers['record']:
                        #and get frame to record
                        ok, frame = self.frame_readers['record'].get_frame()

                    #add detected area
                    if ok:
                        res = False
                        if self.record_contour!=self.CONTOUR_NONE:
                            res, rects = self.get_contours()
                        if res:
                            for i in range(len(rects)):
                                cv2.rectangle(frame,
                                        (rects[i]['x']*self.record_resolution_ratio[0], rects[i]['y']*self.record_resolution_ratio[1]),
                                        (rects[i]['w']*self.record_resolution_ratio[0], rects[i]['h']*self.record_resolution_ratio[1]),
                                        (0,0,255), #red color
                                        2) #line thickness

                        #add some text
                        text = datetime.now().strftime("%Y/%m/%d %H:%M:%S")
                        if self.name and len(self.name)>0:
                            text += ' - %s' % self.name
                        cv2.putText(frame, text, (20,20), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0,0,0), 4, cv2.CV_AA)
                        cv2.putText(frame, text, (20,20), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255,255,255), 1, cv2.CV_AA)

                        #write frame to recorder
                        self.recorders['record'].record(frame)

            #handle day recorder
            if self.record_type in (self.RECORD_DAY, self.RECORD_ALL):
                #record timelapse
                if not self.recorders['timelapse'].is_recording():
                    #get video resolution
                    resolution = [0,0]
                    if self.frame_readers['record']:
                        resolution = self.frame_readers['record'].get_resolution()
                    else:
                        resolution = self.frame_readers['motion'].get_resolution()
                    if resolution[0]!=0:
                        #resolution available, start recording timelapse
                        self.log.info('Start recording timelapse')
                        filename = os.path.join(self.record_dir, 'timelapse_%s_%s.%s' % (self.internalid_camera, datetime.now().strftime("%Y_%m_%d"), 'avi'))
                        self.recorders['timelapse'].start_recording(filename, 24, resolution)

                elif last_day_frame!=now:
                    #configured, record frame

                    #get frame to store
                    ok = True
                    if self.frame_readers['record']:
                        #and get frame to record
                        ok, frame = self.frame_readers['record'].get_frame()

                    #add some text
                    text = datetime.now().strftime("%Y/%m/%d %H:%M:%S")
                    if self.name and len(self.name)>0:
                        text += ' - %s' % self.name
                    cv2.putText(frame, text, (20,20), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0,0,0), 4, cv2.CV_AA)
                    cv2.putText(frame, text, (20,20), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255,255,255), 1, cv2.CV_AA)

                    #write frame to recorder
                    self.recorders['timelapse'].record(frame)
                    last_day_frame = now

        #end of motion thread
        self.log.info('Motion thread for device "%s" is stopped' % self.internalid_camera)

    def stop(self):
        """
        Stop thread
        """
        #stop timer if necessary
        if self.trigger_off_timer:
            self.trigger_off_timer.cancel()
            del self.trigger_off_timer

        #stop frame readers
        if self.frame_readers['motion']:
            self.frame_readers['motion'].stop()
        if self.frame_readers['record']:
            self.frame_readers['record'].stop()

        #remove binary device
        try:
            #connection maybe not avaible when crashing
            self.connection.remove_device(self.internalid_binary)
        except:
            pass

        #release recorders
        if self.recorders['record']:
            self.recorders['record'].stop()
        if self.recorders['timelapse']:
            self.recorders['timelapse'].stop()

        #stop main process
        self.running = False

    def reset_timelapse(self):
        """
        Reset timelapse (when new day)
        """
        if self.record_type in (self.RECORD_DAY, self.RECORD_ALL):
            self.recorders['timelapse'].stop_recording()

    def set_name(self, name):
        """
        Set name. The name will be displayed on recorded frames
        """
        self.name = name


class Camera():
    """
    Code inspired from onvif cli
    https://github.com/quatanium/python-onvif/blob/master/onvif/cli.py
    """

    def __init__(self, connection, log, ip, port, login, password, internalid, name, wsdl_dir='/etc/onvif/wsdl/'):
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
        self.internalid = internalid
        self.motion = False
        self.motion_uri = None
        self.motion_uri_token = None
        self.motion_sensitivity = 10
        self.motion_deviation = 20
        self.motion_on_duration = 300
        self.record_type = Motion.RECORD_NONE
        self.record_uri = None
        self.record_uri_token = None
        self.record_fps = 0
        self.record_duration = 15
        self.record_dir = None
        self.record_contour = Motion.CONTOUR_SINGLE
        self.motion_thread = None
        self._camera = None
        self.name = name

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
            self.log.info('Stop motion thread')
            self.motion_thread.stop()
            self.motion_thread = None

        #start new thread if necessary
        if self.motion_uri and self.motion:
            self.log.info('Start motion thread')
            self.motion_thread = Motion(
                self.connection,
                self.log,
                self.internalid,
                self.motion_uri,
                self.name,
                self.motion_sensitivity,
                self.motion_deviation,
                self.motion_on_duration,
                self.record_type,
                self.record_uri,
                self.record_fps,
                self.record_dir,
                self.record_duration,
                self.record_contour
            )
            self.motion_thread.start()

    def set_motion(self, enable, uri, uri_token, sensitivity=10, deviation=20, on_duration=300, force_restart=True):
        """
        Set motion feature
        """
        self.motion = enable
        self.motion_uri = uri
        self.motion_uri_token = uri_token
        self.motion_sensitivity = sensitivity
        self.motion_deviation = deviation
        self.motion_on_duration = on_duration
        if force_restart:
            self.__configure_motion()

    def set_recording(self, type_, uri, uri_token, directory, duration, contour, force_restart=True):
        """
        Set recording
        """
        self.record_type = type_
        self.record_uri = uri
        self.record_uri_token = uri_token
        self.record_fps = self.__get_fps(uri_token)
        self.record_dir = directory
        self.record_duration = duration
        self.record_contour = contour
        if force_restart:
            self.__configure_motion()

    def __get_fps(self, token):
        """
        Get profile FPS rate
        """
        fps = 0
        profile = self.get_profile(token)
        if profile:
            if profile.has_key('VideoEncoderConfiguration') and profile['VideoEncoderConfiguration'].has_key('RateControl') and profile['VideoEncoderConfiguration']['RateControl'] and profile['VideoEncoderConfiguration']['RateControl'].has_key('FrameRateLimit'):
                try:
                    fps = int(profile['VideoEncoderConfiguration']['RateControl']['FrameRateLimit'])
                except ValueError:
                    self.log.exception('Unable to get uri fps')
            else:
                self.log.error('No uri fps found!')
        return fps

    def set_record_dir(self, record_dir):
        """
        Set directory to save recordings
        """
        self.motion_record_dir = record_dir

        #update motion
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
                    #get onvif version
                    if item.has_key('Version') and version=='0.0':
                        version = '%s.%s' % (item['Version']['Major'], item['Version']['Minor'])
                    
                    #append supported service
                    if item.has_key('Namespace'):
                        namespace = item['Namespace'].lower()
                        for service in self._camera.services_template.keys():
                            if namespace.find(service)!=-1:
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

    def get_profile(self, token):
        """
        Return camera profile by its token
        """
        ps = self.get_profiles()
        if ps:
            for p in ps:
                if p.has_key('token') and p['token']==token:
                    return p
        return None

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

    def get_uri(self, token, login, password):
        """
        Return stream uri
        """
        uris = self.get_uris([token], login, password)
        if uris.has_key(token):
            return uris[token]
        else:
            return None

    def get_uris(self, tokens, login, password):
        """
        Return stream uris
        """
        uris = {}
        for token in tokens:
            params = {'ProfileToken': token, 'StreamSetup': {'Stream': 'RTP-Unicast', 'Transport': {'Protocol': 'UDP'}}}
            uri = self.do_operation('media', 'GetStreamUri', params)

            if uri and uri['Uri']:
                uri_ = uri['Uri']

                #add credential infos to uri if necessary
                if login and len(login)>0:
                    try:
                        (protocol, url) = uri_.split('://')
                        uri_ = '%s://%s:%s@%s' % (protocol, login, password, url)
                    except:
                        self.log.exception('Unable to append credential infos to uri:')

                uris[token] = uri_
            else:
                self.log.warning('Invalid structure GetStreamUri.Uri')
        return uris

    def capture_frame(self):
        """
        Get single camera frame
        @return: success(True/False), frame/error message:
        """
        if self.motion_uri:
            cap = cv2.VideoCapture(self.motion_uri)
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

    def get_string_buffer(self, frame, format='.jpg', quality=100):
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

    def reset_timelapse(self):
        """
        Reset all camera timelapses
        """
        if self.motion_thread:
            self.motion_thread.reset_timelapse()

    def set_name(self, name):
        """
        Set device name specified by user (inventory)
        """
        self.name = name
        #update motion thread
        if self.motion_thread:
            self.motion_thread.set_name(name)


class AgoOnvif(agoclient.AgoApp):

    DEFAULT_RECORD_DIR = '/var/opt/agocontrol/recordings'

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
        self.last_yday = int(time.strftime('%j'))
        self.__config_lock = threading.Lock()
        self.inventory = {}
        self.__inventory_lock = threading.Lock()

        #load config
        self.load_config()

        #add handlers
        self.connection.add_handler(self.message_handler)
        self.connection.add_event_handler(self.event_handler)
        #add controller
        self.connection.add_device('onvifcontroller', 'onvifcontroller')

        #check config
        if not self.config.has_key('cameras') or not self.config.has_key('general'):
            self.log.fatal('Configuration file is corrupted!')
            return False

        #check default recordings dir
        if self.config['general']['record_dir']==self.DEFAULT_RECORD_DIR and not os.path.exists(self.DEFAULT_RECORD_DIR):
            #create default record dir
            try:
                self.log.info('Create default recordings directory [%s]' % self.DEFAULT_RECORD_DIR)
                os.mkdir(self.DEFAULT_RECORD_DIR)
            except:
                self.log.exception('Unable to create default recordings directory [%s]:' % self.DEFAULT_RECORD_DIR)

        #fill inventory
        self.update_camera_names()

        #restore existing devices
        self.log.info('Restoring cameras:')
        self.__config_lock.acquire()
        for internalid in self.config['cameras']:
            res, msg = self.create_camera(self.config['cameras'][internalid]['ip'], self.config['cameras'][internalid]['port'], self.config['cameras'][internalid]['login'], self.config['cameras'][internalid]['password'], False)
            if not res:
                self.log.warning(' - Camera "%s" [FAILED]' % internalid)
            else:
                #get camera
                self.log.info(' - Camera "%s"' % internalid)
                camera = self.cameras[internalid]
                camera.set_motion(
                    self.config['cameras'][internalid]['motion'],
                    self.config['cameras'][internalid]['motion_uri'],
                    self.config['cameras'][internalid]['motion_uri_token'],
                    self.config['cameras'][internalid]['motion_sensitivity'],
                    self.config['cameras'][internalid]['motion_deviation'],
                    self.config['cameras'][internalid]['motion_on_duration'],
                    False
                )
                camera.set_recording(
                    self.config['cameras'][internalid]['record_type'],
                    self.config['cameras'][internalid]['record_uri'],
                    self.config['cameras'][internalid]['record_uri_token'],
                    self.config['general']['record_dir'],
                    self.config['cameras'][internalid]['record_duration'],
                    self.config['cameras'][internalid]['record_contour']
                )
                self.connection.add_device(internalid, 'camera')
        self.__config_lock.release()

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
        self.__config_lock.acquire()
        error = False
        try:
            f = open(DEVICEMAPFILE, "w")
            f.write(json.dumps(self.config))
            f.close()
        except:
            self.log.exception('Unable to save config infos')
            error = True
        self.__config_lock.release()
        return not error

    def load_config(self):
        """
        Load config from map file
        """
        global DEVICEMAPFILE
        self.__config_lock.acquire()
        error = False
        try:
            if os.path.exists(DEVICEMAPFILE):
                f = open(DEVICEMAPFILE, "r")
                j = f.readline()
                f.close()
                self.config = json.loads(j)
            else:
                default = {'general':{'record_dir':self.DEFAULT_RECORD_DIR}, 'cameras':{}}
                self.log.debug('Create default empty config file "%s"' % DEVICEMAPFILE)
                f = open(DEVICEMAPFILE, "w")
                f.write(json.dumps(default))
                f.close()
                self.config = default
        except:
            self.log.exception('Unable to load devices infos')
            error = True
        self.__config_lock.release()
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

        #get device name
        name = self.get_device_name_from_inventory(internalid)
    
        #create new instance
        camera = Camera(self.connection, self.log, ip, port, login, password, internalid, name)
        if not camera.configure():
            #problem during camera configuration!
            msg = 'Unable to create camera "%s". Check parameters'
            self.log.error(msg)
            return False, msg
        else:
            #save new instance locally
            self.cameras[internalid] = camera
            config = {
                'ip':ip,
                'port':port,
                'login':login,
                'password':password,
                'motion':False,
                'motion_uri': None,
                'motion_uri_token': None,
                'motion_uri_desc': '',
                'motion_sensitivity':10,
                'motion_deviation':20,
                'motion_on_duration':300,
                'record_type':Motion.RECORD_NONE,
                'record_uri':None,
                'record_uri_token':None,
                'record_uri_desc':'',
                'record_duration':15,
                'record_contour':Motion.CONTOUR_SINGLE
            }

            #save device map
            if saveConfig:
                self.config['cameras'][internalid] = config
                self.save_config()

            return True, ''

    def create_temp_camera(self, ip, port, login, password):
        """
        Create temp camera instance and return it
        @return instance or None if problem
        """
        internalid = ip

        #create new instance
        camera = Camera(self.connection, self.log, ip, port, login, password, ip, "")
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
        self.config['cameras'].pop(internalid)
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

    def get_recordings(self):
        """
        Return all recordings
        """
        try:
            recordings = []
            for filename in os.listdir(self.config['general']['record_dir']):
                try:
                    extension = os.path.splitext(filename)[1]
                    if extension=='.avi':
                        fullname = os.path.join(self.config['general']['record_dir'], filename)
                        size = os.path.getsize(fullname)
                        if filename.startswith('timelapse'):
                            (_, _, year, month, day) = os.path.splitext(filename)[0].split('_')
                            ts = time.strptime("%s %s %s" % (year, month, day), "%Y %m %d")
                            type_ = 'timelapse'
                        else:
                            (_, _, year, month, day, hour, minute, second) = os.path.splitext(filename)[0].split('_')
                            ts = time.strptime("%s %s %s %s %s %s" % (year, month, day, hour, minute, second), "%Y %m %d %H %M %S")
                            type_ = 'motion'
                        timestamp = int(time.mktime(ts))
                        recordings.append({
                            'filename': filename,
                            'size': size,
                            'timestamp': timestamp,
                            'type': type_
                        })
                except:
                    pass
            return recordings
        except:
            self.log.exception('Exception while getting recordings list:')
            return None

    def update_camera_names(self):
        """
        Update all camera names according to device name specified by user in inventory
        """
        #get inventory
        self.log.info('update_camera_names')
        inventory = self.connection.get_inventory()
        stop = False
        if len(inventory)>0:
            #inventory seems valid
            self.__inventory_lock.acquire()
            self.inventory = inventory

            #check inventory
            if not self.inventory.has_key('devices'):
                stop = True

            #check cameras
            if not stop:
                internalids = self.cameras.keys()
                self.log.info('keys=%s' % internalids)
                if len(internalids)==0:
                    stop = True

            #and update all camera names
            if not stop:
                for uuid in self.inventory['devices'].keys():
                    if self.inventory['devices'][uuid] and self.inventory['devices'][uuid].has_key('internalid') and self.inventory['devices'][uuid]['internalid'] in internalids:
                        if self.inventory['devices'][uuid].has_key('name'):
                            name = self.inventory['devices'][uuid]['name']
                            if len(name.strip())>0:
                                self.log.info('set name of device "%s" with "%s"' % (self.inventory['devices'][uuid]['internalid'], name))
                                self.cameras[self.inventory['devices'][uuid]['internalid']].set_name(name)
            self.__inventory_lock.release()

    def get_device_name_from_inventory(self, internalid):
        """
        Return device name from inventory.
        No process done here, it return empty string if no name was specified by user
        """
        name = ''
        self.__inventory_lock.acquire()
        for uuid in self.inventory['devices']:
            if self.inventory['devices'][uuid]['internalid']==internalid:
                name = self.inventory['devices'][uuid]['name']
                break
        self.__inventory_lock.release()
        return name

    def event_handler(self, subject, content):
        """
        Event handler
        """
        self.log.debug('Event handler: subject=%s : %s' % (subject, content))
        
        if subject=='event.environment.timechanged':
            if int(content['minute'])%5==0:
                #refresh camera names periodically
                self.update_camera_names()

            if content['yday']!=self.last_yday:
                #new day detected

                #set current yday
                self.last_yday = int(content['yday'])

                #new day detected, reset all timelapses
                for internalid in self.cameras:
                    self.cameras[internalid].reset_timelapse()

                #purge oldest recordings
                recordings = self.get_recordings()
                if recordings:
                    now = int(time.time())
                    delay = self.config['general']['record_delay'] * 60 #minutes to seconds
                    for recording in recordings:
                        if now>=recording['timestamp']+delay:
                            #delete recording
                            try:
                                fullpath = os.path.join(self.config['general']['record_dir'], recording['filename'])
                                self.log.info('Delete recording "%s"' % fullpath)
                                os.remove(fullpath)
                            except:
                                self.log.exception('Unable to remove recording "%s"' % fullpath)

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

        if internalid=='onvifcontroller':
            #handle controller commands

            if command=='addcamera':
                if self.__check_command_params(content, ['ip', 'port', 'login', 'password', 'uri_token', 'uri_desc']):
                    #check params
                    res,msg = self.__check_ip_port(content['ip'], content['port'])
                    if not res:
                        return self.connection.response_failed(msg)

                    try:
                        content['port'] = int(content['port'])
                    except ValueError:
                        msg = 'Invalid "port" parameter. Must be integer'
                        self.log.error(msg)
                        self.connection.response_bad_parameters(msg)

                    #create new camera
                    internalid = content['ip']
                    res, msg = self.create_camera(content['ip'], content['port'], content['login'], content['password'], True)
                    if res:
                        #get created camera
                        camera = self.cameras[internalid]

                        #get uri from token
                        uri = camera.get_uri(content['uri_token'], content['login'], content['password'])
                        if uri:

                            #set and save uri
                            camera.set_motion(False, uri, content['uri_token'])
                            self.config['cameras'][internalid]['motion_uri'] = uri
                            self.config['cameras'][internalid]['motion_uri_token'] = content['uri_token']
                            self.config['cameras'][internalid]['motion_uri_desc'] = content['uri_desc']
                            self.save_config()
                        else:
                            msg = 'Problem getting camera URI with token "%s"' % content['uri_token']
                            self.log.error(msg)
                            return self.connection.response_failed('Camera not added: %s' % msg)

                        #create new camera device
                        self.connection.add_device(internalid, 'camera')
                        self.log.info('New camera "%s" added' % internalid)

                        return self.connection.response_success(None, 'Camera added')
                    else:
                        self.log.error('Camera not added: %s' % msg)
                        return self.connection.response_failed('camera not added: %s' % msg)
                else:
                    self.log.error('Parameters are missing')
                    return self.connection.response_missing_parameters()

            elif command=='deletecamera':
                if self.__check_command_params(content, ['internalid']):
                    if self.cameras.has_key(content['internalid']):
                        #delete local content
                        self.delete_camera(content['internalid'])

                        #delete device from ago
                        self.connection.remove_device(content['internalid'])

                        return self.connection.response_success(None, 'Camera deleted')
                    else:
                        self.log.error('Camera with internalid "%s" was not found' % internalid)
                        return self.connection.response_failed('Camera with internalid "%s" was not found' % internalid)
                else:
                    self.log.error('Parameters are missing')
                    return self.connection.response_missing_parameters()

            elif command=='getprofiles':
                #different behaviour according to specified parameters
                fromExisting = False
                if self.__check_command_params(content, ['ip', 'port', 'login', 'password']):
                    fromExisting = False
                elif self.__check_command_params(content, ['internalid']):
                    fromExisting = True
                else:
                    self.log.error('Parameters are missing')
                    return self.connection.response_missing_parameters()

                if not fromExisting:
                    #create temp camera
                    #check params
                    res,msg = self.__check_ip_port(content['ip'], content['port'])
                    if not res:
                        return self.connection.response_failed(msg)

                    content['port'] = int(content['port'])
                    self.log.info('Connecting to %s:%s@%s:%d' % (content['login'], content['password'], content['ip'], content['port']))
                    camera = self.create_temp_camera(content['ip'], content['port'], content['login'], content['password'])
                    if not camera:
                        msg = 'Unable to connect to camera. Check parameters'
                        self.log.warning(msg)
                        return self.connection.response_failed(msg)
                else:
                    #get camera instance from instance container
                    internalid = content['internalid']
                    if not self.cameras.has_key(internalid):
                        self.log.error('Camera with internalid "%s" was not found' % internalid)
                        return self.connection.response_failed('Camera with internalid "%s" was not found' % internalid)
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
                            profile['fps'] = p['VideoEncoderConfiguration']['RateControl']['FrameRateLimit']
                        if p.has_key('VideoSourceConfiguration'):
                            bounds = {}
                            bounds['x'] = p['VideoSourceConfiguration']['Bounds']['x']
                            bounds['y'] = p['VideoSourceConfiguration']['Bounds']['x']
                            bounds['width'] = p['VideoSourceConfiguration']['Bounds']['width']
                            bounds['height'] = p['VideoSourceConfiguration']['Bounds']['height']
                            profile['boundaries'] = bounds
                        profiles.append(profile)
                    return self.connection.response_success(profiles)
                else:
                    #no profiles (parsing error?)
                    self.log.error('No profile found')
                    return self.connection.response_failed('No profile found')

            elif command=='dooperation':
                if not self.__check_command_params(content, ['internalid', 'service', 'operation', 'params']):
                    self.log.error('Parameters are missing')
                    return self.connection.response_missing_parameters()
                internalid = content['internalid']

                if not self.cameras.has_key(internalid):
                    self.log.error('Camera with internalid "%s" was not found' % internalid)
                    return self.connection.response_failed('Camera with internalid "%s" was not found' % internalid)
                camera = self.cameras[internalid]
                
                #execute command
                resp = camera.do_operation(content['service'], content['operation'], content['params'])
                if resp==None:
                    self.log.error('Operation failed')
                    return self.connection.response_failed('Operation failed')
                else:
                    return self.connection.response_success(resp)

            elif command=='getcameras':
                return self.connection.response_success(self.config['cameras'])

            elif command=='getconfig':
                #append recordings to config
                config = self.config
                config['recordings'] = []
                recordings = self.get_recordings()
                if recordings:
                    config['recordings'] = recordings
                return self.connection.response_success(self.config)

            elif command=='updatecredentials':
                if not self.__check_command_params(content, ['internalid', 'login', 'password']):
                    self.log.error('Parameters are missing')
                    return self.connection.response_missing_parameters()
                internalid = content['internalid']

                if not self.cameras.has_key(internalid):
                    self.log.error('Camera with internalid "%s" was not found' % internalid)
                    return self.connection.response_failed('Camera with internalid "%s" was not found' % internalid)
                camera = self.cameras[internalid]

                #update credentials on camera first
                resp = camera.do_operation('devicemgmt', 'SetUser', {'User':{'Username':content['login'], 'Password':content['password'], 'UserLevel':'Administrator'}})
                if not resp:
                    msg = 'Unable to set credentials on camera'
                    self.log.error(msg)
                    return self.connection.response_failed(msg)

                #TODO update uri

                #then update on controller
                self.config['cameras'][internalid]['login'] = content['login']
                self.config['cameras'][internalid]['password'] = content['password']
                self.save_config()

                return self.connection.response_success(None, 'Credentials saved')

            elif command=='getdeviceinfos':
                if not self.__check_command_params(content, ['internalid']):
                    self.log.error('Parameters are missing')
                    return self.connection.response_missing_parameters()
                internalid = content['internalid']

                if not self.cameras.has_key(internalid):
                    self.log.error('Camera with internalid "%s" was not found' % internalid)
                    return self.connection.response_failed('Camera with internalid "%s" was not found' % internalid)
                camera = self.cameras[internalid]

                #get device infos
                resp = camera.do_operation('devicemgmt', 'GetDeviceInformation')
                if not resp:
                    msg = 'Unable to get device infos'
                    self.log.error(msg)
                    return self.connection.response_failed(msg)

                #prepare output
                self.log.debug(resp)
                out = {}
                out['manufacturer'] = resp['Manufacturer']
                out['model'] = resp['Model']
                out['firmwareversion'] = resp['FirmwareVersion']
                out['serialnumber'] = resp['SerialNumber']
                out['hardwareid'] = resp['HardwareId']
                
                return self.connection.response_success(out)

            elif command=='setcameraprofile':
                if not self.__check_command_params(content, ['internalid']):
                    self.log.error('Parameters are missing')
                    return self.connection.response_missing_parameters()
                internalid = content['internalid']

                if not self.cameras.has_key(internalid):
                    self.log.error('Camera with internalid "%s" was not found' % internalid)
                    return self.connection.response_failed('Camera with internalid "%s" was not found' % internalid)
                camera = self.cameras[internalid]

                #get uri from token
                uri = camera.get_uri(content['uri_token'], self.config['cameras'][internalid]['login'], self.config['cameras'][internalid]['password'])
                if uri:
                    #set and save uri
                    camera.set_motion(self.config['cameras'][internalid]['motion'], uri, content['uri_token'])
                    self.config['cameras'][internalid]['motion_uri'] = uri
                    self.config['cameras'][internalid]['motion_uri_token'] = content['uri_token']
                    self.config['cameras'][internalid]['motion_uri_desc'] = content['uri_desc']
                    self.save_config()
                else:
                    msg = 'Problem getting camera URI with token "%s"' % content['uri_token']
                    self.log.error(msg)
                    return self.connection.response_failed('Camera not added: %s' % msg)

                return self.connection.response_success(None, 'Configuration saved')

            elif command=='setmotion':
                if not self.__check_command_params(content, ['internalid', 'enable', 'uri_desc', 'uri_token', 'sensitivity', 'deviation', 'onduration']):
                    self.log.error('Parameters are missing')
                    return self.connection.response_missing_parameters()
                internalid = content['internalid']

                if not self.cameras.has_key(internalid):
                    self.log.error('Camera with internalid "%s" was not found' % internalid)
                    return self.connection.response_failed('Camera with internalid "%s" was not found' % internalid)
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
                    self.connection.response_bad_parameters(msg)

                try:
                    content['deviation'] = int(content['deviation'])
                except ValueError:
                    msg = 'Invalid "deviation" parameter. Must be integer'
                    self.log.error(msg)
                    self.connection.response_bad_parameters(msg)
                    
                try:
                    content['onduration'] = int(content['onduration'])
                except ValueError:
                    msg = 'Invalid "on duration" parameter. Must be integer'
                    self.log.error(msg)
                    self.connection.response_bad_parameters(msg)

                #get uri from token
                uri = camera.get_uri(content['uri_token'], self.config['cameras'][internalid]['login'], self.config['cameras'][internalid]['password'])
                if not uri:
                    msg = 'Problem getting camera URI with token "%s"' % content['uri_token']
                    self.log.error(msg)
                    return self.connection.response_failed('Config not saved: %s' % msg)

                #configure motion
                camera.set_motion(
                    content['enable'],
                    uri,
                    content['uri_token'],
                    content['sensitivity'],
                    content['deviation'],
                    content['onduration']
                )

                #save new config
                self.config['cameras'][internalid]['motion'] = content['enable']
                self.config['cameras'][internalid]['motion_uri'] = uri
                self.config['cameras'][internalid]['motion_uri_token'] = content['uri_token']
                self.config['cameras'][internalid]['motion_uri_desc'] = content['uri_desc']
                self.config['cameras'][internalid]['motion_sensitivity'] = content['sensitivity']
                self.config['cameras'][internalid]['motion_deviation'] = content['deviation']
                self.config['cameras'][internalid]['motion_on_duration'] = content['onduration']
                self.save_config()

                return self.connection.response_success(None, 'Configuration saved')

            elif command=='setrecording':
                if not self.__check_command_params(content, ['internalid', 'type', 'uri_desc', 'uri_token', 'duration', 'contour']):
                    self.log.error('Parameters are missing')
                    return self.connection.response_missing_parameters()
                internalid = content['internalid']

                if not self.cameras.has_key(internalid):
                    self.log.error('Camera with internalid "%s" was not found' % internalid)
                    return self.connection.response_failed('Camera with internalid "%s" was not found' % internalid)
                camera = self.cameras[internalid]

                #check parameters
                try:
                    content['duration'] = int(content['duration'])
                except ValueError:
                    msg = 'Invalid "duration" parameter. Must be integer'
                    self.log.error(msg)
                    self.connection.response_bad_parameters(msg)

                try:
                    content['contour'] = int(content['contour'])
                    if content['contour'] not in (Motion.CONTOUR_NONE, Motion.CONTOUR_SINGLE, Motion.CONTOUR_MULTIPLE):
                        raise Exception('Invalid contour value')
                except Exception as e:
                    msg = 'Invalid "contour" parameter.'
                    self.log.error(e.msg)
                    self.connection.response_bad_parameters(msg)

                try:
                    content['type'] = int(content['type'])
                    if content['type'] not in (Motion.RECORD_NONE, Motion.RECORD_MOTION, Motion.RECORD_DAY, Motion.RECORD_ALL):
                        raise Exception('Invalid type value')
                except Exception as e:
                    msg = 'Invalid "type" parameter.'
                    self.log.error(e.msg)
                    self.connection.response_bad_parameters(msg)

                #get uri from token
                uri = camera.get_uri(content['uri_token'], self.config['cameras'][internalid]['login'], self.config['cameras'][internalid]['password'])
                if not uri:
                    msg = 'Problem getting camera URI with token "%s"' % content['uri_token']
                    self.log.error(msg)
                    return self.connection.response_failed('Config not saved: %s' % msg)

                #configure motion
                camera.set_recording(
                    content['type'],
                    uri,
                    content['uri_token'],
                    self.config['general']['record_dir'],
                    content['duration'],
                    content['contour']
                )

                #save new config
                self.config['cameras'][internalid]['record_type'] = content['type']
                self.config['cameras'][internalid]['record_uri'] = uri
                self.config['cameras'][internalid]['record_uri_token'] = content['uri_token']
                self.config['cameras'][internalid]['record_uri_desc'] = content['uri_desc']
                self.config['cameras'][internalid]['record_duration'] = content['duration']
                self.config['cameras'][internalid]['record_contour'] = content['contour']
                self.save_config()

                return self.connection.response_success(None, 'Configuration saved')

            elif command=='setrecordingconfig':
                if not self.__check_command_params(content, ['dir', 'delay']):
                    self.log.error('Parameters are missing')
                    return self.connection.response_missing_parameters()

                #check if dir exists
                if not os.path.exists(content['dir']):
                    return self.connection.response_failed('Specified directory "%s" doesn\'t exist. Please create it manually first.' % content['dir'])

                try:
                    content['delay'] = int(content['delay'])
                except Exception as e:
                    msg = 'Invalid "delay" parameter.'
                    self.log.error(e.msg)
                    self.connection.response_bad_parameters(msg)

                #update all cameras
                for internalid in self.cameras:
                    self.cameras[internalid].set_record_dir(content['dir'])

                #save to config dir
                self.config['general']['record_dir'] = content['dir']
                self.config['general']['record_delay'] = content['delay']
                self.save_config()

                return self.connection.response_success(None, 'Configuration saved')

            elif command=='getrecordings':
                recordings = self.get_recordings()
                if recordings!=None:
                    return self.connection.response_success(recordings)
                else:
                    return self.connection.response_failed('Unable to get recording list')

            elif command=='downloadfile':
                if not self.__check_command_params(content, ['filename']):
                    self.log.error('Parameters are missing')
                    return self.connection.response_missing_parameters()

                #get recording filename
                filename = os.path.join(self.config['general']['record_dir'], content['filename'])
                
                #check if file exists
                if os.path.exists(filename):
                    #file exists, return full path
                    self.log.info('Send fullpath of file to download "%s"' % filename)
                    resp = {'filepath': filename}
                    return self.connection.response_success(resp)
                else:
                    #not exists
                    self.log.error('Trying to download unknown file "%s"' % filename)
                    return self.connection.response_failed('File doesn\'t exist')

            else:
                #request not handled
                self.log.error('Unhandled request "%s"' % command)
                return self.connection.response_unknown_command()

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
                        data['image'] = buffer(base64.b64encode(buf)) #buffer: workaround to avoid qpid 65k limitation
                        return self.connection.response_success(data)
                    else:
                        self.log.error('No buffer retrieved [%s]' % data)
                        return self.connection.response_failed(data)
                else:
                    self.log.error('No buffer retrieved [%s]' % data)
                    return self.connection.response_failed(data)

            else:
                #request not handled
                self.log.error('Unhandled request "%s"' % command)
                return self.connection.response_unknown_command()

        else:
            #request not handled
            self.log.error('Unhandled request "%s"' % command)
            return self.connection.response_unknown_command()




if __name__=="__main__":
    a = AgoOnvif();
    a.main()

