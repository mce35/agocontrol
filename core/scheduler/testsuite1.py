import unittest
import time
from datetime import datetime
from scheduler import Scheduler

class scheduler_test1(unittest.TestCase):
    def setUp(self):
        self.s = Scheduler(app=None)
        #self.s.parseJSON("schedule.json")

    def test1_load(self):
        self.s.parse_conf_file("schedule.json")

        self.s.weekday = "mo"
        self.s.new_day("mo")
        item = self.s.get_first("00:00")
        self.assertEqual(item["time"], "06:00")
        self.assertEqual(item["scenario"], "Plant lights on")

        item = self.s.get_first("08:10")
        self.assertEqual(item["time"], "08:10")
        self.assertEqual(item["device"], "2222-2222")

        d = self.s.get_weekday()

        h = datetime.now().strftime('%H')
        m = datetime.now().strftime('%M')
        now = h.zfill(2) + ":" + m.zfill(2)

        #now = datetime.now().strftime('%H:%M')
        #item = self.s.get_first(now)
        #print item

        item = self.s.get_first("21:35")
        self.assertEqual(item["time"], "21:40")
        print item
        item = self.s.get_next()
        self.assertEqual(item["time"], "22:00")
        print item


        #r = self.s.rules.find("1234-1234")
        #self.assertTrue(r.execute())


    #def test2_execute_rule(self):
    #    r = self.s.rules.find("1234-1234")
    #    self.assertTrue(r.execute())


    def tearDown(self):
        pass

if __name__ == "__main__":
    suite = unittest.TestLoader().loadTestsFromTestCase(scheduler_test1)
    unittest.TextTestRunner(verbosity=2).run(suite)


