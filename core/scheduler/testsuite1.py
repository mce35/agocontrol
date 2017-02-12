__author__     = "Joakim Lindbom"
__copyright__  = "Copyright 2017, Joakim Lindbom"
__date__       = "2017-01-27"
__license__    = "GPL Public License Version 3"

import unittest
from scheduler import Scheduler, all_days
from groups import Groups

class scheduler_test1(unittest.TestCase):
    def setUp(self):
        self.groups = Groups("groups.json")
        self.s = Scheduler(12.7, 56.6, groups=self.groups)
        self.s.parse_conf_file("schedule.json")  # Prepped schedule with 11 schedule items and 2 rules

    def test1_start_at_midnight(self):
        """ This is a normal situation, where the schedules are reloaded when it's 00:00/a new day
        """
        no = self.s.new_day("mo")
        self.assertEqual(no, 9)  # 9 items expected for Monday

        # Start at midnight
        item = self.s.get_first()
        self.assertEqual(item["time"], "06:00")  # Monday, first item should be 06:00
        self.assertEqual(item["scenario"], "Plant lights on")

        # Start later in the day, search for exact match in time
        item = self.s.get_first("08:10")
        self.assertEqual(item["time"], "08:10")
        self.assertEqual(item["device"], "2222-2222")

    def test2_start_in_evening(self):
        """ Start later in the day. The search is inexact, expecting a schedule item at a later point-in-time
        """
        self.s.new_day("mo")

        item = self.s.get_first("21:35")
        self.assertEqual(item["time"], "21:40")
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
        item = self.s.get_first("23:00")
        self.assertEqual(item["time"], "23:00")

        # This should fail, there are no more items
        self.assertIsNone(self.s.get_next())

        # It's midnight, let's shift over to a new day
        no = self.s.new_day("tu")
        self.assertEqual(no, 11)  # 11 items expected for Tuesday
        #self.s.schedules.list_full_day()

        item = self.s.get_next()
        self.assertEqual(item["time"], "06:00")  # Tuesday, first item should be 06:00

        item = self.s.get_next()
        self.assertEqual(item["time"], "06:00")  # Tuesday, second item should also be 06:00

    def test3b_new_day(self):
        """ Normal shift of day.
        """
        self.s.new_day("mo")

        # Get last item for Monday
        item = self.s.get_first("23:00")
        self.assertEqual(item["time"], "23:00")

        # This should fail, there are no more items
        self.assertIsNone(self.s.get_next())

        # It's midnight, let's shift over to a new day
        no = self.s.new_day("tu")
        self.assertEqual(no, 11)  # 11 items expected for Tuesday

        item = self.s.get_next()
        self.assertEqual(item["time"], "06:00")  # Tuesday, first item should be 06:00

        item = self.s.get_next()
        self.assertEqual(item["time"], "06:00")  # Tuesday, second item should also be 06:00


    def test4_execute_rule(self):
        """ Locate and execute a rule, use day index to locate Monday
        """
        self.s.new_day(1)
        item = self.s.get_first("00:00")
        self.assertEqual(item["time"], "06:00")

        r = item["rule"]

        self.assertFalse(r.execute())

    def test5_get_current_weekday(self):
        """ Get current weekday. Cannot be asserted
        """
        dl, d = self.s.get_weekday()
        print ("dl={}, d={}".format(dl, d))

    def test6_list_items(self):
        """ List all items for this day
        """
        self.s.new_day("mo")
        item = self.s.schedules.list_full_day(now="04:01")  # Before first
        item = self.s.schedules.list_full_day(now="09:00")  # In between
        item = self.s.schedules.list_full_day(now="23:01")  # After last
        item = self.s.schedules.list_full_day()             # Should insets now based on current time. Cannot be validated easily
        self.s.new_day("fr")
        item = self.s.schedules.list_full_day(now="00:00")  # Should contain 2 sunset/sunrise based times


    def test7_double_assign(self):
        self.weekday = weekday = all_days[2-1]
        self.assertEqual(self.weekday, "tu")
        self.assertEqual(weekday, "tu")


    def tearDown(self):
        pass

if __name__ == "__main__":
    suite = unittest.TestLoader().loadTestsFromTestCase(scheduler_test1)
    unittest.TextTestRunner(verbosity=2).run(suite)


