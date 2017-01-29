import unittest
import time
from scheduler import Scheduler

class scheduler_test1(unittest.TestCase):
    def setUp(self):
        self.s = Scheduler(app=None)
        #self.s.parseJSON("schedule.json")

    def test1_load(self):
        self.s.parse_conf_file("schedule.json")

        self.s.weekday = "mo"
        self.s.new_day("mo")
        r = self.s.rules.find("1234-1234")
        self.assertTrue(r.execute())


    #def test2_execute_rule(self):
    #    r = self.s.rules.find("1234-1234")
    #    self.assertTrue(r.execute())


    def tearDown(self):
        pass

if __name__ == "__main__":
    suite = unittest.TestLoader().loadTestsFromTestCase(scheduler_test1)
    unittest.TextTestRunner(verbosity=2).run(suite)


