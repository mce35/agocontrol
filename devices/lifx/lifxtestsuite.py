import unittest
import time
from lifxnet import LifxNet

from lifxtestsuite1 import API_KEY

class lifxTests(unittest.TestCase):
    def setUp(self):
        #print ("Configuration parameter 'APIKEY'=%s", API_KEY)
        self.lifx = LifxNet(self)
        self.lifx.init(API_KEY=API_KEY)
        self.switches = self.lifx.listSwitches()
        self.assertGreater(len(self.switches), 0, "No lamps found. Error in config?")
        print ("No lamps found %d" % len(self.switches))

    def test_printinfo(self):
        if len(self.switches) > 0:
            print self.switches

    def test_getLightState(self):
        if len(self.switches) > 0:
            for devid in self.switches:
                self.assertTrue(self.lifx.turnOn(devid))
                time.sleep(1)
                state = self.lifx.getLightState(devid)
                self.assertEqual(state["power"], u'on')

                self.assertTrue(self.lifx.turnOff(devid))
                time.sleep(1)
                state = self.lifx.getLightState(devid)
                self.assertEqual(state["power"], u'off')

    def test_listdeviceinfo(self):
        if len(self.switches) > 0:
            #print switches
            for devId, dev in self.switches.iteritems():
                print ('devId={}'.format(devId))
                print ('dev={}'.format(dev))
                print ('MODEL={}'.format(dev["model"]))

    def test_turnon(self):
        for devid in self.switches:
            self.assertTrue(self.lifx.turnOff(devid))
            time.sleep(1)
            self.assertTrue(self.lifx.turnOn(devid))

    def test_dim(self):
        for devid in self.switches:
            self.assertTrue(self.lifx.dim(devid, 100))
            time.sleep(1)
            self.assertTrue(self.lifx.dim(devid, 10))
            time.sleep(1)
            self.assertTrue(self.lifx.dim(devid, 100))

    def tearDown(self):
        for devid in self.switches:
            self.assertTrue(self.lifx.turnOff(devid))  # Turn off all lights

if __name__ == "__main__":
    #unittest.main()
    suite = unittest.TestLoader().loadTestsFromTestCase(lifxTests)
    unittest.TextTestRunner(verbosity=2).run(suite)
