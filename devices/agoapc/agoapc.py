#! /usr/bin/env python
""" APC Switched Rack PDU Device """

import sys
import syslog
import socket

from pysnmp.entity.rfc3413.oneliner import cmdgen
from pysnmp.proto import rfc1902
import threading
import time

import agoclient

CLIENT = agoclient.AgoConnection("apc")

OIDOUTLETCOUNT = "1,3,6,1,4,1,318,1,1,4,5,1,0"  # sPDUOutletConfigTableSize
OIDSTATUS = "1,3,6,1,4,1,318,1,1,4,4,2,1,3"     # sPDUOutletCtl
OIDNAME = "1,3,6,1,4,1,318,1,1,4,5,2,1,3"       # sPDUOutletName
OIDLOAD = "1,3,6,1,4,1,318,1,1,12,2,3,1,1,2,1"  # rPDULoadStatusLoad
LOADOID = (1, 3, 6, 1, 4, 1, 318, 1, 1, 12, 2, 3, 1, 1, 2, 1)

APCHOST = agoclient.get_config_option("apc", "host", "127.0.0.1")
APCPORT = int(agoclient.get_config_option("apc", "port", "161"))
APCVOLTAGE = int(agoclient.get_config_option("apc", "voltage", "220"))
APCCOMMUNITYRO = agoclient.get_config_option("apc", "community_readonly",
    "public")
APCCOMMUNITYRW = agoclient.get_config_option("apc", "community_readwrite",
    "private")


# route stderr to syslog
class LogErr:
    """Log errors."""
    def write(self, data):
        """Log errors."""
        syslog.syslog(syslog.LOG_ERR, data)

syslog.openlog(sys.argv[0], syslog.LOG_PID, syslog.LOG_DAEMON)
# sys.stderr = LogErr()


def set_outlet_state(internalid, command):
    """XXX"""
    #new_state = 1 # 1=enable; 2=disable
    if "on" in command:
        new_state = 1
    else:
        new_state = 2

    myoid = eval(str(OIDSTATUS) + "," + str(internalid))
    cmdgen.CommandGenerator().setCmd(
        cmdgen.CommunityData('my-agent', APCCOMMUNITYRW, 1),
        cmdgen.UdpTransportTarget((APCHOST, APCPORT)),
        (myoid, rfc1902.Integer(new_state)))

    status = get_outlet_status(internalid)
    return(status)


def get_outlet_status(internalid):
    """XXX"""

    myoid = eval(str(OIDSTATUS) + "," + str(internalid))
    errorIndication, errorStatus, errorIndex, var_binds = cmdgen.CommandGenerator().getCmd(
        cmdgen.CommunityData('my-agent', APCCOMMUNITYRO, 0),
        cmdgen.UdpTransportTarget((APCHOST, APCPORT)), myoid)
    output = var_binds[0][1]

    if output == 1:
        status = "on"
    if output == 2:
        status = "off"
    if output == 4:
        status = "unknown"

    return(status)


def get_outlet_name(internalid):
    """XXX"""

    myoid = eval(str(OIDNAME) + "," + str(internalid))
    errorIndication, errorStatus, errorIndex, var_binds = cmdgen.CommandGenerator().getCmd(
        cmdgen.CommunityData('my-agent', APCCOMMUNITYRO, 0),
        cmdgen.UdpTransportTarget((APCHOST, APCPORT)), myoid)
    output = var_binds[0][1]

    return(output)


def get_current_power():
    """XXX"""

    myoid = eval(str(OIDLOAD))
    errorIndication, errorStatus, errorIndex, var_binds = cmdgen.CommandGenerator().getCmd(
            cmdgen.CommunityData('my-agent', APCCOMMUNITYRO, 0),
            cmdgen.UdpTransportTarget((APCHOST, APCPORT)),
            myoid
        )
    myload = var_binds[0][1]

    if myload == "-1":
        return int(-1)
    result = float(float(int(myload)) / 10)

    return(result)


syslog.syslog(syslog.LOG_NOTICE, "agoapc.py startup")


# thread to poll energy level
class EnergyUsage(threading.Thread):
    """XXX"""
    def __init__(self,):
        threading.Thread.__init__(self)

    def run(self):
        old_current_power = 0
        unit = "Wh"
        while (True):
            try:
                time.sleep(5)
                current_power_a = get_current_power()
                current_power = int(float(current_power_a * APCVOLTAGE))
                if current_power != old_current_power:
                    CLIENT.emit_event("powerusage",
                        "event.environment.powerchanged", current_power, unit)
                    old_current_power = current_power
            except:
                time.sleep(1)


def message_handler(internalid, content):
    """XXX"""
    if "command" in content:
        if content["command"] == "on":
            print "device switched on: ", internalid
            result = set_outlet_state(internalid, 'on')
            if "on" in result:
                CLIENT.emit_event(internalid,
                    "event.device.statechanged", 255, "")
        if content["command"] == "off":
            print "device switched off:", internalid
            result = set_outlet_state(internalid, 'off')
            if "off" in result:
                CLIENT.emit_event(internalid,
                    "event.device.statechanged", 0, "")


# get outlets from apc
MYOID = eval(str(OIDOUTLETCOUNT))
errorIndication, errorStatus, errorIndex, VARBINDS = cmdgen.CommandGenerator().getCmd(
    cmdgen.CommunityData('my-agent', APCCOMMUNITYRO, 0),
    cmdgen.UdpTransportTarget((APCHOST, APCPORT)),
    MYOID)

OUTLETCOUNT = VARBINDS[0][1]

for OUTLET in range(1, OUTLETCOUNT + 1):
    CLIENT.add_device(OUTLET, "switch")
    _result = get_outlet_status(OUTLET)
    if "on" in _result:
        CLIENT.emit_event(OUTLET, "event.device.statechanged", "255", "")
    if "off" in _result:
        CLIENT.emit_event(OUTLET, "event.device.statechanged", "0", "")

CLIENT.add_device("powerusage", "multilevelsensor")


BACKGROUND = EnergyUsage()
BACKGROUND.setDaemon(True)
BACKGROUND.start()

CLIENT.add_handler(message_handler)
CLIENT.run()
