__author__     = "Joakim Lindbom"
__copyright__  = "Copyright 2017, Joakim Lindbom"
__date__       = "2017-02-02"
__license__    = "GPL Public License Version 3"

import unittest
import time
from datetime import datetime
from scheduler import Scheduler

class scheduler_test1(unittest.TestCase):
    def setUp(self):
        self.s = Scheduler()
        self.s.parse_conf_file("winter.json")  # Prepped schedule with 9 schedule items and 2 rules

    def test1_start_at_midnight(self):
        """ This is a normal situation, where the schedules are reloaded when it's 00:00/a new day
        """
        no = self.s.new_day("mo")
        self.assertEqual(no, 7)  # 7 items expected for Monday

        # Start at midnight
        item = self.s.get_first()
        self.assertEqual(item["time"], "06:15")  # Monday, first item should be 06:15
        self.assertEqual(item["scenario"], "ce52eea8-8889-4dc6-b578-7d049b5a64ab")   # Plant lights on"

        # Start later in the day, search for exact match in time
        item = self.s.get_first("08:10")
        self.assertEqual(item["time"], "17:00")
        self.assertEqual(item["group"], "5555-5555")

    def test2_start_in_evening(self):
        """ Start later in the day. The search is inexact, expecting a schedule item at a later point-in-time
        """
        self.s.new_day("mo")

        item = self.s.get_first("21:35")
        self.assertEqual(item["time"], "21:45")
        item = self.s.get_next()
        self.assertEqual(item["time"], "22:00")

    def test2b_start_with_errors(self):
        """ Start with invalid days. 0 scheduled items expected as result
        """
        no = self.s.new_day("xx")  # Needs to be "mo", "tu", etc.
        self.assertEqual(no, 0)

        no = self.s.new_day(9)  # Needs to be 1..7
        self.assertEqual(no, 0)

    def test3_new_day(self):
        """ Normal shift of day.
        """
        self.s.new_day("mo")

        # Get last item for Monday
        item = self.s.get_first("22:00")
        self.assertEqual(item["time"], "22:00")

        # This should fail, there are no more items
        self.assertIsNone(self.s.get_next())

        # It's midnight, let's shift over to a new day
        no = self.s.new_day("tu")
        self.assertEqual(no, 10)  # 10 items expected for Tuesday

        item = self.s.get_next()
        self.assertEqual(item["time"], "06:00")  # Tuesday, first item should be 06:00

        item = self.s.get_next()
        self.assertEqual(item["time"], "06:00")  # Tuesday, second item should also be 06:00

    def test3_new_day2(self):
        """ Normal shift of day.
        """
        self.s.new_day("we")

        # Get last item for Wednesday
        item = self.s.get_first("22:00")
        self.assertEqual(item["time"], "22:00")

        # This should fail, there are no more items
        self.assertIsNone(self.s.get_next())

        # It's midnight, let's shift over to a new day
        no = self.s.new_day("th")
        self.assertEqual(no, 7)  # 10 items expected for Thursday

        item = self.s.get_next()
        self.assertEqual(item["time"], "06:25")  # Tuesday, first item should be 06:00

        item = self.s.get_next()
        self.assertEqual(item["time"], "07:25")  # Tuesday, second item should also be 06:00

    def test4_list(self):
        """ Test sunrise calculation"""
        self.s.schedules.list_full_day()


    def test5_sunrise(self):
        """ Test sunrise calculation"""

        self.s.sunrise()

    def tearDown(self):
        pass

if __name__ == "__main__":
    suite = unittest.TestLoader().loadTestsFromTestCase(scheduler_test1)
    unittest.TextTestRunner(verbosity=2).run(suite)


