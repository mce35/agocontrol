# light.py
# Author: Meghan Clark

from device import Device, WorkflowException
from msgtypes import *
import colorsys
from time import sleep

RED = [65535, 65535, 65535, 3500]
ORANGE = [5525, 65535, 65535, 3500]
YELLOW = [7000, 65535, 65535, 3500]
GREEN = [16173, 65535, 65535, 3500]
CYAN = [29814, 65535, 65535, 3500]
BLUE = [43634, 65535, 65535, 3500]
PURPLE = [50486, 65535, 65535, 3500]
PINK = [58275, 65535, 47142, 3500]
WHITE = [58275, 0, 65535, 5500]
COLD_WHITE = [58275, 0, 65535, 9000]
WARM_WHITE = [58275, 0, 65535, 3200]
GOLD = [58275, 0, 65535, 2500]

class Light(Device):
    def __init__(self, mac_addr, ip_addr, service=1, port=56700, source_id=0, verbose=False):
        mac_addr = mac_addr.lower()
        super(Light, self).__init__(mac_addr, ip_addr, service, port, source_id, verbose)
        self.color = None
        self.verbose2 = True

    ############################################################################
    #                                                                          #
    #                            Light API Methods                             #
    #                                                                          #
    ############################################################################

    # GetPower - power level
    def get_power(self):
        try:
            response = self.req_with_resp(LightGetPower, LightStatePower)
            self.power_level = response.power_level
        except WorkflowException as e:
            if self.verbose2:
                print('get_power - {}'.format(e))
            sleep(0.5)
        return self.power_level

    def set_power(self, power, duration=0, rapid=False):
        on = [True, 1, "on", 65535]
        off = [False, 0, "off"]
        try:
            if power in on and not rapid:
                self.req_with_ack(LightSetPower, {"power_level": 65535, "duration": duration})
            elif power in on and rapid:
                self.fire_and_forget(LightSetPower, {"power_level": 65535, "duration": duration}, num_repeats=5)
            elif power in off and not rapid:
                self.req_with_ack(LightSetPower, {"power_level": 0, "duration": duration})
            elif power in off and rapid:
                self.fire_and_forget(LightSetPower, {"power_level": 0, "duration": duration}, num_repeats=5)
            else:
                print("{} is not a valid power level.".format(power))
        except WorkflowException as e:
            if self.verbose2:
                print('set_power - {}'.format(e))

    # color is [Hue, Saturation, Brightness, Kelvin]
    def set_waveform(self, is_transient, color, period, cycles, duty_cycle, waveform, rapid=False):
        if len(color) == 4:
            try:
                if rapid:
                    self.fire_and_forget(LightSetWaveform, {"transient": is_transient, "color": color, "period": period, "cycles": cycles, "duty_cycle": duty_cycle, "waveform": waveform}, num_repeats=5)
                else:
                    self.req_with_ack(LightSetWaveform, {"transient": is_transient, "color": color, "period": period, "cycles": cycles, "duty_cycle": duty_cycle, "waveform": waveform})
            except WorkflowException as e:
                if self.verbose2:
                    print('set_waveform - {}'.format(e))

    # color is [Hue, Saturation, Brightness, Kelvin], duration in ms
    def set_color(self, color, duration=0, rapid=False):
        if len(color) == 4:
            try:
                if rapid:
                    self.fire_and_forget(LightSetColor, {"color": color, "duration": duration}, num_repeats=5)
                else:
                    self.req_with_ack(LightSetColor, {"color": color, "duration": duration})
            except WorkflowException as e:
                if self.verbose2:
                    print('set_color - {}'.format(e))

    # LightGet, color, power_level, label
    def get_color(self):
        try:
            response = self.req_with_resp(LightGet, LightState)
            self.color = response.color
            self.power_level = response.power_level
            self.label = response.label
        except WorkflowException as e:
            if self.verbose2:
                print('get_color - {}'.format(e))
            sleep(0.5)

        return self.color

    def set_HSB(self, hue, saturation, brightness, duration=0, rapid=False):
        """ Set colour according to HSV scheme
            hue, saturation, brightness: 0-65535
            duration in ms"""
        color = self.get_color()
        color2 = (hue, saturation, brightness, color[3])
        try:
            if rapid:
                self.fire_and_forget(LightSetColor, {"color": color2, "duration": duration}, num_repeats=5)
            else:
                self.req_with_ack(LightSetColor, {"color": color2, "duration": duration})
        except WorkflowException as e:
            if self.verbose2:
                print('set_HSB - {}'.format(e))

    def set_RGB(self, red, green, blue, duration=0, rapid=False):
        """ Set colour according to RGB scheme
            red, green, blue: 0-255
            duration in ms"""
        color = self.get_color()

        h, s, b = colorsys.rgb_to_hsv(red/255.0, green/255.0, blue/255.0)

        hue = int(round(h * 65535.0, 0))
        saturation = int(round(s * 65535.0, 0))
        brightness = int(round(b * 65535.0, 0))

        color2 = (hue, saturation, brightness, color[3])
        try:
            if rapid:
                self.fire_and_forget(LightSetColor, {"color": color2, "duration": duration}, num_repeats=5)
            else:
                self.req_with_ack(LightSetColor, {"color": color2, "duration": duration})
        except WorkflowException as e:
            if self.verbose2:
                print('set_RGB - {}'.format(e))


    def set_hue(self, hue, duration=0, rapid=False):
        """ hue to set
            duration in ms"""
        color = self.get_color()
        color2 = (hue, color[1], color[2], color[3])
        try:
            if rapid:
                self.fire_and_forget(LightSetColor, {"color": color2, "duration": duration}, num_repeats=5)
            else:
                self.req_with_ack(LightSetColor, {"color": color2, "duration": duration})
        except WorkflowException as e:
            if self.verbose2:
                print('set_hue - {}'.format(e))

    def set_saturation(self, saturation, duration=0, rapid=False):
        """ saturation to set
            duration in ms"""
        color = self.get_color()
        color2 = (color[0], saturation, color[2], color[3])
        try:
            if rapid:
                self.fire_and_forget(LightSetColor, {"color": color2, "duration": duration}, num_repeats=5)
            else:
                self.req_with_ack(LightSetColor, {"color": color2, "duration": duration})
        except WorkflowException as e:
            if self.verbose2:
                print('set_saturation - {}'.format(e))

    def set_brightness(self, brightness, duration=0, rapid=False):
        """ brightness to set
            duration in ms"""
        color = self.get_color()
        color2 = (color[0], color[1], brightness, color[3])
        try:
            if rapid:
                self.fire_and_forget(LightSetColor, {"color": color2, "duration": duration}, num_repeats=5)
            else:
                self.req_with_ack(LightSetColor, {"color": color2, "duration": duration})
        except WorkflowException as e:
            if self.verbose2:
                print('set_brightness - {}'.format(e))


    def set_colortemp(self, kelvin, duration=0, rapid=False):
        """ kelvin: color temperature to set 2500-9000
            duration in ms"""
        color = self.get_color()
        color2 = (color[0], color[1], color[2], kelvin if kelvin>=2500 and kelvin <= 9000 else 2500 if kelvin < 2500 else 9000)
        try:
            if rapid:
                self.fire_and_forget(LightSetColor, {"color": color2, "duration": duration}, num_repeats=5)
            else:
                self.req_with_ack(LightSetColor, {"color": color2, "duration": duration})
        except WorkflowException as e:
            if self.verbose2:
                print('set_colortemp - {}'.format(e))


    ############################################################################
    #                                                                          #
    #                            String Formatting                             #
    #                                                                          #
    ############################################################################

    def __str__(self):
        self.refresh()
        indent = "  "
        s = self.device_characteristics_str(indent)
        s += indent + "Color (HSBK): {}\n".format(self.get_color())
        s += indent + self.device_firmware_str(indent)
        s += indent + self.device_product_str(indent)
        s += indent + self.device_time_str(indent)
        s += indent + self.device_radio_str(indent)
        return s
