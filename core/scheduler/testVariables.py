__author__     = "Joakim Lindbom"
__copyright__  = "Copyright 2017, Joakim Lindbom"
__date__       = "2017-01-27"
__license__    = "GPL Public License Version 3"

import unittest
import time
from datetime import datetime
from variables import Variables

vmap = "variablesmap.json"


class scheduler_test1(unittest.TestCase):
    def setUp(self):
        self.v = Variables(vmap)

    def test1_check_loading(self):
        """ Check that the loading f the variables went OK
        """
        self.assertEqual(len(self.v.variables), 11)
        self.assertEqual(self.v.variables["test"], "False")
        self.assertEqual(self.v.variables["housemode"], "At home")

    def test2_check_get_variable(self):
        """ Check get_variable
        """
        val = self.v.get_variable("test")
        self.assertEqual(val, "False")

    def test3_check_get_variable_negative(self):
        """ Check get_variable, negative test case
        """
        val = self.v.get_variable("should-not-be-there")
        self.assertIsNone(val)

    def test4_add_del_update(self):
        """ Check adding, changing and deleting a variable
        """
        self.assertEqual(self.v.variables["housemode"], "At home")
        self.v.set_variable("housemode", "Away")
        self.assertEqual(self.v.variables["housemode"], "Away")
        val = self.v.get_variable("should-not-be-there")
        self.assertRaises(KeyError)
        self.v.set_variable("should-not-be-there", "now it's there")
        val = self.v.get_variable("should-not-be-there")
        self.assertEqual(val, "now it's there")
        self.v.del_variable("should-not-be-there")
        val = self.v.get_variable("should-not-be-there")
        self.assertRaises(KeyError)  # OK, it's gone again

    def tearDown(self):
        pass

if __name__ == "__main__":
    suite = unittest.TestLoader().loadTestsFromTestCase(scheduler_test1)
    unittest.TextTestRunner(verbosity=2).run(suite)


