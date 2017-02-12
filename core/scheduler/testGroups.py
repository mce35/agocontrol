__author__     = "Joakim Lindbom"
__copyright__  = "Copyright 2017, Joakim Lindbom"
__date__       = "2017-02-02"
__license__    = "GPL Public License Version 3"

import unittest
import time
from datetime import datetime
from groups import Groups


class scheduler_test1(unittest.TestCase):
    def setUp(self):
        self.g = Groups("groups.json")

    def test1_find(self):
        """
        """
        grp = self.g.find("5555-5555")
        self.assertEqual(grp.name, "Living room")
        self.assertEqual(len(grp.devices), 2)

    def test1_iterate(self):
        """
        """
        grp = self.g.find("5555-5555")
        self.assertEqual(grp.name, "Living room")
        #for dev in grp.devices:
        #    print dev
        self.assertEqual(grp.devices[0], '1111-1111')
        self.assertEqual(grp.devices[1], '2222-2222')



    def tearDown(self):
        pass

if __name__ == "__main__":
    suite = unittest.TestLoader().loadTestsFromTestCase(scheduler_test1)
    unittest.TextTestRunner(verbosity=2).run(suite)


