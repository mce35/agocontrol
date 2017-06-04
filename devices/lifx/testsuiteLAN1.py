import unittest
import time
from lifxlan2 import LifxLAN2

class lifxTests1(unittest.TestCase):
    def setUp(self):
        self.lifx = LifxLAN2(self)
        self.lifx.init(num_lights=1)
        self.switches = self.lifx.listSwitches()
        self.assertGreater(len(self.switches), 0, "No lamps found. Error in config?")
        print ("# lamps found: %d" % len(self.switches))

    def test1_printinfo(self):
         if len(self.switches) > 0:
            print self.switches

    def test2_listdeviceinfo(self):
        if len(self.switches) > 0:
            #print switches
            for devId, dev in self.switches.iteritems():
                print ('devId={} dev={}'.format(devId, dev))

    def test3a_getColour(self):
        if len(self.switches) > 0:
            for devid in self.switches:
                self.assertTrue(self.lifx.get_colour(devid))

    def test3a_a_setcolour(self):
        if len(self.switches) > 0:
            for devid in self.switches:
                self.assertTrue(self.lifx.turnOn(devid))
                self.assertTrue(self.lifx.set_brightness(devid, 65535))

                # Get colour state
                colour = self.lifx.get_colour(devid)

                # Run through a set of colours
                self.assertIsNone(self.lifx.set_colour(devid, 255, 255, 255))  # White
                time.sleep(1)
                self.assertIsNone(self.lifx.set_colour(devid, 255, 0, 0))      # Red
                time.sleep(1)
                self.assertIsNone(self.lifx.set_colour(devid, 0, 255, 0))      # Green
                time.sleep(1)
                self.assertIsNone(self.lifx.set_colour(devid, 0, 0, 255))      # Blue
                time.sleep(1)
                # Reset colour temp
                #self.assertTrue(self.lifx.set_(devid, colour["kelvin"]))  # TODO: Add set_HSV to lifxlan2
                self.assertTrue(self.lifx.turnOff(devid))


    def test3a_colourtemp(self):
        if len(self.switches) > 0:
            for devid in self.switches:
                self.assertTrue(self.lifx.turnOn(devid))
                self.assertTrue(self.lifx.set_brightness(devid, 65535))

                # Get colour state
                colour = self.lifx.get_colour(devid)

                # Run through a set of colour temperatures
                self.assertTrue(self.lifx.set_colourtemp(devid, 1000))
                time.sleep(1)
                self.assertTrue(self.lifx.set_colourtemp(devid, 5000))
                time.sleep(1)
                self.assertTrue(self.lifx.set_colourtemp(devid, 3000))
                time.sleep(1)
                self.assertTrue(self.lifx.set_colourtemp(devid, 2000))
                time.sleep(1)
                self.assertTrue(self.lifx.set_colourtemp(devid, 5000))
                time.sleep(1)

                # Reset colour temp
                self.assertTrue(self.lifx.set_colourtemp(devid, colour["kelvin"]))
                self.assertTrue(self.lifx.turnOff(devid))

    def test3b_brightness(self):
        if len(self.switches) > 0:
            for devid in self.switches:
                self.assertTrue(self.lifx.turnOn(devid))
                self.assertTrue(self.lifx.set_colourtemp(devid, 2700))

                # Get colour state
                colour = self.lifx.get_colour(devid)

                self.assertTrue(self.lifx.set_brightness(devid, 1000))
                time.sleep(0.5)
                self.assertTrue(self.lifx.set_brightness(devid, 60000))
                time.sleep(0.5)
                self.assertTrue(self.lifx.set_brightness(devid, 20000))
                time.sleep(0.5)
                self.assertTrue(self.lifx.set_brightness(devid, 50000))
                time.sleep(0.5)
                self.assertTrue(self.lifx.set_brightness(devid, 30000))
                time.sleep(0.5)
                self.assertTrue(self.lifx.set_brightness(devid, 1))
                time.sleep(0.5)

                # Reset to starting state
                self.assertTrue(self.lifx.set_brightness(devid, colour["brightness"]))
                self.assertTrue(self.lifx.turnOff(devid))

    def test3c_dim(self):
        if len(self.switches) > 0:
            for devid in self.switches:
                self.assertTrue(self.lifx.dim(devid, 100))
                time.sleep(1)
                self.assertTrue(self.lifx.dim(devid, 90))
                time.sleep(1)
                self.assertTrue(self.lifx.dim(devid, 100))


    def test3c_getLightState(self):
        if len(self.switches) > 0:
            for devid in self.switches:
                self.assertTrue(self.lifx.turnOn(devid))
                time.sleep(1)
                state = self.lifx.getLightState(devid)
                self.assertTrue(state["power"] == u'on')

                self.assertTrue(self.lifx.turnOff(devid))
                time.sleep(1)
                state = self.lifx.getLightState(devid)
                self.assertTrue(state["power"] == u'off')

    def test4_turnon(self):
        for devid in self.switches:
            self.assertTrue(self.lifx.turnOff(devid))
            time.sleep(1)
            self.assertTrue(self.lifx.turnOn(devid))

    def test5_dim(self):
        for devid, dev in self.switches.iteritems():
            if dev["isDimmer"]:
                self.assertTrue(self.lifx.dim(devid, 100))
                time.sleep(1)
                self.assertTrue(self.lifx.dim(devid, 10))
                time.sleep(1)
                self.assertTrue(self.lifx.dim(devid, 100))

    def tearDown(self):
        pass
#        for devid in self.switches:
#            self.assertTrue(self.lifx.turnOff(devid))  # Turn off all lights

if __name__ == "__main__":
    #unittest.main()
    suite = unittest.TestLoader().loadTestsFromTestCase(lifxTests1)
    unittest.TextTestRunner(verbosity=2).run(suite)
