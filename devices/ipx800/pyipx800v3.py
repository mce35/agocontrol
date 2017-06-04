#! /usr/bin/python
# -*- coding: utf-8 -*-

"""
pyipx800v3: Python wrapper for IPX800v3
IPX800v3 is a relay board developped by GCE Electronics
More info on website http://www.gce-electronics.com/

This wrapper was only tested on IPX800-V3 firmware v3.05.33
Functions that surely works are: setOutputs, configureOutput, configureDigitalInput, 
configureTimer, setCounter, getStatus
 
Copyright (C) 2013 Tang <tanguy [dot] bonneau [at] gmail [dot] com>
 
LMSServer class is based on JingleManSweep <jinglemansweep [at] gmail [dot] com>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
"""

import logging
import threading
import sys
import telnetlib
import urllib, urllib2
import urlparse
from xml.dom import minidom
from BaseHTTPServer import BaseHTTPRequestHandler, HTTPServer
import time


class Ipx800v3Telnet(threading.Thread):
    """
    Implements a thread to listen to ipx callbacks over telnet connection
    """
    def __init__(self, ip, port, callback, charset='utf-8'):
        """
        Constructor
        """
        threading.Thread.__init__(self)
        self.logger = logging.getLogger('Ipx800v3Telnet')
        self.ip = ip
        self.port = port
        self.__callback = callback
        self.running = True
        self.charset = charset
        self.__telnet = None
        self.__last_response = time.time()

    def __del__(self):
        """
        Destructor
        """
        self.stop()

    def stop(self):
        self.running = False

    def run(self):
        """
        Thread process
        """
        self.logger.info('Thread running for %s:%d' % (self.ip, self.port))
        while self.running:
            try:
                if not self.telnet_is_connected():
                    #not connected, try to connect
                    if not self.telnet_connect():
                        #not connected, retry in few seconds
                        self.logger.info('Unable to connect to ipx800 %s' % self.ip)
                        time.sleep(5)

                if self.telnet_is_connected():
                    response = self.telnet_response(timeout=1)
                    if response:
                        #update last response time
                        self.__last_response = time.time()
                        self.telnet_parse_response(response)
                    elif time.time()>(self.__last_response + 43200.0):
                        #no response for too long (power interrupt?), reconnect to ipx
                        self.logger.warn('No reponse for too long, force disconnection from ipx board "%s"' % self.ip)
                        self.telnet_disconnect()
            except:
                self.logger.exception('Exception in run():')
                #error occured, disconnect
                self.telnet_disconnect()

        self.telnet_disconnect()
        self.logger.info('Thread ended for %s:%d' % (self.ip, self.port))

    def telnet_connect(self):
        """
        Connect to telnet port
        """
        try:
            self.logger.info('Connect to %s:%d' % (self.ip, self.port))
            self.__telnet = telnetlib.Telnet(self.ip, self.port)
            self.__last_response = time.time()
        except Exception as e:
            self.logger.exception('Unable to connect to telnet port %d' % self.port)
            self.__telnet = None
        return self.__telnet

    def telnet_disconnect(self):
	"""
	Disconnect from telnet
	"""
        if self.__telnet:
            self.logger.info('Disconnect from %s:%d' % (self.ip, self.port))
            self.__telnet.close()
            self.__telnet = None

    def telnet_is_connected(self):
        """
        Return true if telnet is connected
        """
        if self.__telnet:
            return True
        else:
            return False

    def telnet_response(self, timeout=0):
        """
        Wait for telnet message
        @return received message
        """
        resp = None
        try:
            if self.telnet_is_connected():
                resp = self.__telnet.read_until( '\n'.encode(self.charset), timeout)
            else:
                resp = None
        except EOFError:
            #telnet failed (not connected?)
            self.__telnet = None #force to reconnect next time
            resp = None
        except Exception as e:
            #something failed
            self.logger.exception("Exception in response:")
            resp = None
        return resp

    def telnet_parse_response(self, message):
        """
        Parse received message from telnet
        @format: I=00000000000000000000000000000000&O=10000000111111111111111111111111&A0=1022&A1=1022&A2=1022&A3=1022&A4=0&A5=0&A6=0&A7=0&A8=0&A9=0&A10=0&A11=0&A12=0&A13=0&A14=0&A15=0&C1=12&C2=3&C3=11&C4=1&C5=12&C6=1&C7=16&C8=3
        """
        self.logger.info(message)
        url = 'http://dummy/?' + message
        params = urlparse.parse_qs(urlparse.urlparse(url).query, keep_blank_values=True)
	output = {}
	for param in params:
            if param == 'I':
                for i in range(len(params[param][0])):
                    output['in%d'%i] = int(params[param][0][i])
            elif param == 'O':
                for i in range(len(params[param][0])):
                    output['out%d'%i] = int(params[param][0][i])
            elif param.startswith('A'):
                output['an%s'%param.replace('A','')] = float(params[param][0])
            elif param.startswith('C'):
                output['cnt%s'%param.replace('C','')] = int(params[param][0])

        #send result
        if self.__callback!=None:
            self.__callback(self.ip, output)


class Ipx800v3HttpHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        try:
            self.server.callback(self.client_address[0], self.path, None)
    
            #send basic response
            self.send_response(200)
            self.send_header('Content-type','text/html')
            self.end_headers()
            self.wfile.write('ok')
        except Exception as e:
            logging.error('Exception in do_GET: client_address="%s" path="%s" excep="%s"' % (str(self.client_address), str(self.path), str(e)))
            self.server.callback(None, None, str(e))
        return


class Ipx800v3HttpServer(HTTPServer):
    allow_reuse_address = 1
    timeout = 1
    def __init__(self, server_address, RequestHandlerClass, callback):
        self.callback = callback
        HTTPServer.__init__(self, server_address, RequestHandlerClass)
        

class Ipx800v3(threading.Thread):
    TIMER_RESET = 'http://%s/protect/timers/timer1.htm?'
    TIMER_CONFIGURE = 'http://%s/protect/timers/timer1.htm?'
    INPUT_SET = 'http://%s/leds.cgi?'
    INPUT_DCONFIGURE = 'http://%s/protect/assignio/assign1.htm?'
    INPUT_ACONFIGURE = 'http://%s/protect/assignio/analog1.htm?'
    OUTPUT_SETPULSE = 'http://%s/leds.cgi?'
    OUTPUT_SETNOPULSE = 'http://%s/preset.htm?'
    OUTPUT_CONFIGURE = 'http://%s/protect/settings/output1.htm?'
    COUNTER_SET = 'http://%s/protect/assignio/counter.htm?'
    PINGWATCHDOG = 'http://%s/protect/settings/ping.htm?'
    STATUS = 'http://%s/status.xml'

    def __init__(self, port, ipxCallback, push=False):
        """
        Constructor
        @param port: local port for webserver (used to get M2M return infos) or cli port (telnet)
        @param ipxCallback: result of ipx board m2m push (params: ipxIp[str], url[str], error[str])
        @param cli_port: port to connect to telnet socket. If not provided, use http push that is less responsive than telnet
        """
        threading.Thread.__init__(self)
        self.logger = logging.getLogger('Ipx800v3')
        self.port = port
        self.__callback = ipxCallback
        self.__push = push
        if self.__push:
            #use push instead of telnet
            self.__server = Ipx800v3HttpServer(('', port), Ipx800v3HttpHandler, self.__pushCallback)
        self.__telnet_threads = []
        self.__running = True

    def __del__(self):
        """
        Destructor
        """
        self.stop()

    def run(self):
        """
        Ipx800v3 background process
        """
        try:
            while self.__running:
                if self.__push:
                    #handle push requests
                    self.logger.debug('Serving at localhost:%d' % self.port)
                    self.__server.handle_request()

                else:
                    #no push configured, release cpu
                    time.sleep(1)

        except Exception as e:
            #may be stopped
            self.logger.exception('Exception in run():')

    def stop(self):
        """
        Stop ipx server
        """
        self.logger.debug('Stop')
        self.__running = False
        if self.__push:
            if self.__server:
                #stop local webserver
                self.__server.socket.close()
                sys.stdin.close()
        else:
            #stop all telnet threads
            for thread in self.__telnet_threads:
                thread.stop()

    def add_board(self, ip, port=9870):
        """
        Add board. This connect to telnet port and listen to callbacks
        @param ip: ipx board ip
        @param port: ipx telnet port [default 9870]
        """
	board = Ipx800v3Telnet(ip, port, self.__callback)
        self.__telnet_threads.append(board)
        board.start()

    def __pushCallback(self, board, url, error):
        """
        Push callback
        @param board: board ip
        @param url: board url
        @param error: setted to true if error occured during push
        @return: call specified function (callback) with parameters ipxIp, content (dict)
        """
        #awaited url
        # - from M2M push:
        #00:04:A3:2D:67:9F&In=00000000000000000000000000000000&Out=00000000111111111111111111111111&An1=0&An2=0&An3=0&An4=0&C1=0&C2=0&C3=0
        # - from output push:
        #out<id>=1|0
        # - from input push:
        #in<id>=1|0
        self.logger.debug('board=%s url=%s error=%s' % (board, url, str(error)))
        output = {}
        if not error:
            #split url
            items = url.split('&')
            for item in items:
                if item.find('=')!=-1:
                    (key, val) = item.split('=',1)
                    if key=='In':
                        for i in range(32):
                            try:
                                output['in%d'%i] = int(val[i])
                            except:
                                output['in%d'%i] = val[i]
                    elif key=='Out':
                        for i in range(32):
                            try:
                                output['out%d'%i] = int(val[i])
                            except:
                                output['out%d'%i] = val[i]
                    elif key.startswith('An'):
                        try:
                            ident = int(key.replace('An', ''))
                            ident -= 1
                            output['an%d' % ident] = float(val)
                        except:
                            output[key.lower()] = val
                    elif key.startswith('C'):
                        try:
                            ident = int(key.replace('C', ''))
                            ident -= 1
                            output['cnt%d' % ident] = int(val)
                        except:
                            output[key.lower()] = val
                    else:
                        try:
                            output[key.lower()] = int(val)
                        except:
                            output[key.lower()] = val
                elif item.count(':')==5:
                    #seems to be mac address
                    output['mac'] = item
                else:
                    #unknown item :S
                    self.logger.warning('Unknown item received from push: "%s" [%s]' % (item, url))
        else:
            #error during push
            output.clear()
            self.logger.error('Error during push: %s' % str(error))
        
        #send result
        if board!=None and self.__callback!=None:
            self.__callback(board, output)

    def __sendExtUrl(self, url, params):
        """
        Send url
        """
        try:
            url += urllib.urlencode(params)
            req = urllib2.urlopen(url)
            lines = req.readlines()
            req.close()
            self.logger.debug(url)
            #self.logger.debug('\n'.join(lines))
            return True, lines
        except Exception as e:
            self.logger.exception('Exception in sendUrl:')
        return False, []

    def __sendUrl(self, url, params):
        (result, lines) = self.__sendExtUrl(url, params)
        return result

    """----------TIMERS----------"""
    def resetTimer(self, ipx, timerId):
        """
        Reset specified timer
        @param ipx: ipx ip 
        @param timerId: timer id [0-127]
        """
        #check params
        if not ipx or len(ipx)==0:
            self.logger.error('resetTimer: ipx ip is not valid')
            return False
        if timerId not in range(128):
            self.logger.error('resetTimer: timerId is not valid or not in range [0-127]')
            return False

        #prepare and send url
        url = Ipx800v3.TIMER_RESET % (ipx)
        params = {'erase':timerId}
        return self.__sendUrl(url, params)

    def configureTimer(self, ipx, timerId, day, hour, minute, outputId, action):
        """
        Schedule specified timer
        @param ipx: ipx ip 
        @param timerid: integer [0-127]
        @param day: 0-6 (monday to sunday), 7 (everyday), 8 (workingday), 9 (weekday)
        @param hour: hour of timer
        @param minute: minute of timer
        @param outputId: output id (relay[0-31], counter[32-34])
        @param action: action to apply on relay (0=off, 1=on, 2=invert, 3=pulse, 4=cancel, 7=reset counters)
        """
        #check params
        if not ipx or len(ipx)==0:
            self.logger.error('configureTimer: ipx ip is not valid')
            return False
        if timerId not in range(128):
            self.logger.error('configureTimer: timerId is not valid or not in range [0-127]')
            return False
        if day not in range(10):
            self.logger.error('configureTimer: day is not valid or not in range [0-9]')
            return False
        if hour not in range(24):
            self.logger.error('configureTimer: hour is not valid or not in range [0-23]')
            return False
        if minute not in range(60):
            self.logger.error('configureTimer: minute is not valid or not in range [0-59]')
            return False
        if outputId not in range(35):
            self.logger.error('configureTimer: outputId is not valid or not in range [0-34]')
            return False
        if action not in range(8):
            self.logger.error('configureTimer: action is not valid or not in range [0-7]')
            return False

        #prepare and send url
        url = Ipx800v3.TIMER_CONFIGURE % (ipx)
        params = {'timer':timerId, 'day':day, 'time':'%d:%d'%(hour, minute), 'relay':outputId, 'action':action}
        return self.__sendUrl(url, params)
        

    """----------INPUTS----------"""
    def setInput(self, ipx, inputId):
        """
        Simulate virtual action on specified input
        @param ipx: ipx ip
        @param inputId: input id [0-31]
        """
        #check params
        if not ipx or len(ipx)==0:
            self.logger.error('setInput: ipx ip is not valid')
            return False
        if inputId not in range(8):
            self.logger.error('setInput: inputId is not valid or not in range [0-31]')
            return False

        #prepare and send url
        url = Ipx800v3.INPUT_SET % (ipx)
        params = {'timer':timerId, 'day':day, 'time':'%d:%d'%(hour, minute), 'relay':outputId, 'action':action}
        return self.__sendUrl(url, params)

    def configureDigitalInput(self, ipx, inputId, outputIds, mode, inv=None, name=None):
        """
        Configure digital input
        @param ipx: ipx ip 
        @param inputId: digital input id [0-31]
        @param outputIds: list of outputs to control [0-31]
        @param mode: input mode (0=on/off, 1=switch, 2=shutter, 3=on, 4=off)
        @param inv: inverse mode (facultative)
        @param name: new name of input (facultative)
        """
        #check params
        if not ipx or len(ipx)==0:
            self.logger.error('configureDigitalInput: ipx ip is not valid')
            return False
        if inputId not in range(32):
            self.logger.error('configureDigitalInput: inputId is not valid or not in range [0-31]')
            return False
        if not outputIds or not isinstance(outputIds, list):
            self.logger.error('configureDigitalInput: outputIds is not valid (need list)')
            return False
        for outputId in outputIds:
            if outputId not in range(32):
                self.logger.error('configureDigitalInput: outputId "%d" is not valid or not in range [0-31]' % outputId)
                return False
        if (mode==1 or mode==2) and len(outputIds)!=1:
            #limit number of outputs to 1
            self.logger.warning('configureDigitalInput: only first outputId is kept in mode Shutter or Switch')
            outputIds = [outputIds[0]]
        if mode not in range(5):
            self.logger.error('configureDigitalInput: mode is not valid or not in range [0-5]')
            return False
        if inv and not isinstance(inv, bool):
            self.logger.error('configureDigitalInput: inv is not valid or not boolean')
            return False

        #prepare and send url
        url = Ipx800v3.INPUT_DCONFIGURE % (ipx)
        params = {'input':inputId, 'mode':mode}
        if inv!=None:
            if inv:
                params['inv'] = 1
            #else:
            #    params['inv'] = 0
        if name and len(name)>0:
            #send name in separate query
            self.__sendUrl(url, {'input':inputId, 'inputname': name.strip()})
        for outputId in range(32):
            if outputId in outputIds:
                params['l%d' % outputId] = 1
            #else:
            #    params['l%d' % outputId] = 0
        return self.__sendUrl(url, params)

    def configureAnalogInput(self, ipx, inputId, outputIds, mode, thresholdMax, thresholdMin, actionMax, actionMin, name=None):
        """
        Configure analog input
        @param inputId: analog input id [0-3]
        @param mode: input mode (0=raw value, 1=tension, 2=TC4012, 3=SHT-X3 light, 4=SHT-X3 temperature, 5=SHT-X3 humidity
        @param outputIds: list of output ids [1-8]
        @param thresholdMax: max threshold
        @param thresholdMin: min threshold
        @param actionMax: action when max threshold detected (1=on, 0=off)
        @param actionMin: action when min threshold detected (1=on, 0=off)
        @param name: name of input (facultative)
        """
        #check params
        if not ipx or len(ipx)==0:
            self.logger.error('configureAnalogInput: ipx ip is not valid')
            return False
        if inputId not in range(4):
            self.logger.error('configureAnalogInput: inputId is not valid or not in range [0-3]')
            return False
        if not outputIds or not isinstance(outputIds, list):
            self.logger.error('configureAnalogInput: outputIds is not valid (need list)')
            return False
        for outputId in outputIds:
            if outputId not in range(1,9):
                self.logger.error('configureAnalogInput: outputId "%d" is not valid or not in range [1-8]' % outputId)
                return False
        if mode not in range(6):
            self.logger.error('configureAnalogInput: mode is not valid or not in range [0-6]')
            return False

        #prepare and send url
        url = Ipx800v3.INPUT_ACONFIGURE % (ipx)
        params = {'analog':inputId, 'selectinput':mode, 'hi':thresholdMax, 'lo':thresholdMin, 'mhi':actionMax, 'mlo':actionMin}
        if name and len(name)>0:
            #send name in separate query
            self.__sendUrl(url, {'analog':inputId, 'name': name.strip()})
        for outputId in range(1,9):
            if outputId in outputIds:
                params['lka%d' % outputId] = 1
            else:
                params['lka%d' % outputId] = 0
        return self.__sendUrl(url, params)

    """----------OUTPUTS----------"""
    def setOutput(self, ipx, outputId, state=None):
        """
        Set specified output without pulse
        @param ipx: ips ip
        @param outputid: output id [0-31]
        @param state: force state to value (0=off, 1=on) or just pulse output (=None)
        """
        #check params
        if not ipx or len(ipx)==0:
            self.logger.error('setOutputNoPulse: ipx ip is not valid')
            return False
        if outputId not in range(32):
            self.logger.error('setOutputNoPulse: outputId is not valid or not in range [0-31]')
            return False

        #fix outputId according to mode
        if state!=None:
            #state mode, outputId should be [1-32] and not [0-31]
            outputId += 1

        #prepare and send url
        if state!=None:
            #state mode
            url = Ipx800v3.OUTPUT_SETNOPULSE % (ipx)
            if state:
                params = {'set%d'%outputId:1}
            else:
                params = {'set%d'%outputId:0}
            return self.__sendUrl(url, params)
        else:
            #pulse mode
            url = Ipx800v3.OUTPUT_SETPULSE % (ipx)
            params = {'led':outputId}
            return self.__sendUrl(url, params)

    def configureOutput(self, ipx, outputId, delayOn=None, delayOff=None, name=None):
        """
        Configure output
        @param ipx: ipx ip
        @param outputid: output id [0-31]
        @param delayon: time to wait before starting relay (1=0.1s, max=65535) (facultative)
        @param delayoff: time to wait before stopping relay (1=0.1s, max=65535) (facultative)
        @param name: name of output (facultative)
        """
        #check params
        if not ipx or len(ipx)==0:
            self.logger.error('configureOutput: ipx ip is not valid')
            return False
        if outputId not in range(32):
            self.logger.error('configureOutput: outputId is not valid or not in range [1-33]')
            return False
        if delayOn and (delayOn<0 or delayOn>65535):
            self.logger.error('configureOutput: delayOn is not valid or not in range [0-65535]')
            return False
        if delayOff and (delayOff<0 or delayOff>65535):
            self.logger.error('configureOutput: delayOff is not valid or not in range [0-65535]')
            return False

        #fix output id because ipx800v3 range is [1-32] and not [0-31]
        outputId += 1

        #prepare and send url
        url = Ipx800v3.OUTPUT_CONFIGURE % (ipx)
        params = {'output':outputId}
        if delayOn:
            params['delayon'] = delayOn
        if delayOff:
            params['delayoff'] = delayOff
        if name and len(name)>0:
            params['relayname'] = name.strip()
        return self.__sendUrl(url, params)

    """----------COUNTERS----------"""
    def setCounter(self, ipx, counterId, value, name='counter'):
        """
        Set counter value and/or name
        @info: /!\ name if mandatory otherwise command doesn't work :S 
        @param ipx: ipx ip 
        @param counterId: counter id [0-2] 
        @param value: counter value (0 to reset)
        @param name: new name of counter (facultative)
        """
        #check params
        if not ipx or len(ipx)==0:
            self.logger.error('setCounter: ipx ip is not valid')
            return False
        if counterId not in range(3):
            self.logger.error('setCounter: counterId is not valid or not in range [1-3]')
            return False
        if value==None:
            self.logger.error('setCounter: value is not valid')
            return False

        #update counterid because ipx800v3 range is [1-3] and not [0-2]
        counterId += 1

        #prepare and send url
        url = Ipx800v3.COUNTER_SET % (ipx)
        params = {'counter%d'%counterId:value}
        if name and len(name)>0:
            params['countername%d'%counterId] = name.strip()
        return self.__sendUrl(url, params)

    """----------PING----------"""
    def configurePingWatchdog(self, ipx, ip, interval, retries, outputId):
        """
        Configure integrated ping function
        @param ipx: ipx ip
        @param ip: ip to ping
        @param interval: interval between 2 retries
        @param retries: number of retries before trigger output
        @param outputId: triggered output [0-31]
        """
        #check params
        if not ipx or len(ipx)==0:
            self.logger.error('configurePingWatchdog: ipx ip is not valid')
            return False
        if not ip and len(ip)==0:
            self.logger.error('configurePingWatchdog: ip is not valid')
            return False
        if not interval:
            self.logger.error('configurePingWatchdog: interval is not valid')
            return False
        if not retries:
            self.logger.error('configurePingWatchdog: retries is not valid')
            return False
        if outputId not in range(32):
            self.logger.error('configurePingWatchdog: outputId is not valid or not in range [0-31]')
            return False

        #prepare and send url
        url = Ipx800v3.PINGWATCHDOG % (ipx)
        params = {'pingip':ip.strip(), 'pingtime':interval, 'pingretry':retries, 'prelay':outputId}
        return self.__sendUrl(url, params)


    """----------STATUS----------"""
    def getStatus(self, ipx):
        """
        Return status of all board elements
        @output dict:
        """
        #check parameters
        if not ipx or len(ipx)==0:
            self.logger.error('getStatus: ipx ip is not valid')
            return False

        #prepare and send url
        output = {}
        url = Ipx800v3.STATUS % (ipx)
        (res, lines) = self.__sendExtUrl(url, {})
        if res:
            xml = ''.join(lines)
            dom = minidom.parseString(xml)
            for node in dom.firstChild.childNodes:
                if node.nodeType!=node.TEXT_NODE:
                    #self.logger.debug('nodeName=%s nodeValue=%s' % (node.nodeName, node.firstChild.nodeValue))
                    if node.nodeName.startswith('led'):
                        try:
                            output[node.nodeName.replace('led', 'out')] = int(node.firstChild.nodeValue)
                        except:
                            output[node.nodeName.replace('led', 'out')] = node.firstChild.nodeValue
                    elif node.nodeName.startswith('btn'):
                        if node.firstChild.nodeValue=='up':
                            output[node.nodeName.replace('btn', 'in')] = 0
                        else:
                            output[node.nodeName.replace('btn', 'in')] = 1
                    elif node.nodeName.startswith('analog'):
                        try:
                            output[node.nodeName.replace('analog', 'an')] = float(node.firstChild.nodeValue)
                        except:
                            output[node.nodeName.replace('analog', 'an')] = node.firstChild.nodeValue
                    elif node.nodeName.startswith('count'):
                        try:
                            output[node.nodeName.replace('count', 'cnt')] = int(node.firstChild.nodeValue)
                        except:
                            output[node.nodeName.replace('count', 'cnt')] = node.firstChild.nodeValue
                    else:
                        try:                    
                            output[node.nodeName] = int(node.firstChild.nodeValue)
                        except:
                            output[node.nodeName] = node.firstChild.nodeValue
        else:
            self.logger.error('getStatus: unable to get status')
        return output
            

"""TESTS"""
if __name__ == '__main__':
    import gobject; gobject.threads_init()
    logger = logging.getLogger('')
    logger.setLevel(logging.DEBUG)
    console_sh = logging.StreamHandler()
    console_sh.setLevel(logging.DEBUG)
    console_sh.setFormatter(logging.Formatter('%(asctime)s %(name)-20s %(levelname)-8s %(message)s'))
    logger.add_handler(console_sh)

    mainloop = gobject.MainLoop()

    def callbck(push):
        logger.info('==> %s' % str(push))

    board = '192.168.1.80'
    ipx = Ipx800v3(8020, callbck)
    try:

        #ipx.start()
        #get status ==> passed
        #logger.info(ipx.getStatus('192.168.1.80'))
        #set counters ==> passed
        #ipx.setCounter(board, 0, 0, 'counter0')
        #ipx.setCounter(board, 2, 666)
        #close shutter
        #ipx.setOutput(board, 0) #close parents shutter
        #ipx.setOutput(board, 3, 1) #close dressing shutter
        #configure output
        #ipx.configureOutput(board, 10, 987, 654, 'dummy')
        #configure input
        #ipx.configureDigitalInput(board, 0, [15,25,29], 2, False, 'tata')
        #configure timer
        #ipx.configureTimer(board, 0, 7, 12, 5, 6, 3)
        mainloop.run()
    except KeyboardInterrupt:
        logger.debug('====> KEYBOARD INTERRUPT <====')
        logger.debug('Waiting for threads to stop...')
        ipx.stop()
        mainloop.quit()
