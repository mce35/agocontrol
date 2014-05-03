#! /usr/bin/env python 
#
#
# Copyright (C) 2012 Andreas Pagander
#
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License.
# This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
# of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#    
# See the GNU General Public License for more details.
#
# uses smbus
#
#
# /etc/opt/agocontrol/conf.d/raspilcd.conf
#
# [raspiLCD]
# lines=4
# rows=20

import agoclient
import time
from datetime import datetime
import threading
import sys
import lcddriver



client = agoclient.AgoConnection("raspilcd")
lines = int(agoclient.get_config_option("raspilcd", "lines", "2"))
rows = int(agoclient.get_config_option("raspilcd", "rows", "16"))

lcd = lcddriver.lcd()
client.add_device(0x27, "raspilcd")

class G():
    text = {}
    clock = 0
    clock_text = ""
    update = False

class clock(threading.Thread):
    def __init__(self,):
        threading.Thread.__init__(self)    
    def run(self):
        second = 0
        while(True):
            if G.clock > 0:
                now = datetime.now()
                if now.second != second:
                    hour = now.hour
                    minute = now.minute
                    second = now.second
                    split_str = G.clock_text.split("clock")
                    clock_str = str('%02d' % hour) + ":" + str('%02d' % minute) + ":" + str('%02d' % second)
                    display_str = split_str[0] + clock_str + split_str[1]
                    lcd.lcd_display_string(display_str.ljust(rows)[0:rows], G.clock)
            if G.update:
                for line in G.text.keys():
                    lcd.lcd_display_string(G.text[line].ljust(rows)[0:rows], line)
                G.update = False
            time.sleep(0.1)

def messageHandler(internalid, content):
    if "command" in content:
        if content["command"] == "lcdtext":
            if 1 <= int(content["line"]) <= lines:
                if "clock" in content["text"]:
                    G.clock = int(content["line"])
                    G.clock_text = content["text"]
                else:
                    G.text[int(content["line"])] = content["text"]
                G.update = True

client.add_handler(messageHandler)
            
background = clock()
background.setDaemon(True)
background.start()

client.run()





