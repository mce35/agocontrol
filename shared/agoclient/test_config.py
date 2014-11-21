import unittest
import config
import agoapp

import tempfile
import os, os.path, shutil

devdir = os.path.realpath(os.path.dirname(os.path.realpath(__file__)) + "/../../")

class AgoTest(agoapp.AgoApp):
    pass

class ConfigTestBase(unittest.TestCase):
    def setUp(self):
        # Create dummy conf
        self.tempdir = tempfile.mkdtemp()

        os.mkdir("%s/conf.d" % self.tempdir)
        # Custom dir, we must place agocontrol.aug into this dir
        # TODO: or add some dir in loadpath?
        shutil.copyfile("%s/conf/agocontrol.aug" % devdir, "%s/agocontrol.aug" % self.tempdir)

        config.set_config_dir(self.tempdir)
        with open(config.get_config_path('conf.d/system.conf'), 'w') as f:
            f.write("[system]\ntest_value_0=42\ntest_value_blank=\ntest_string=hello there\n")

        with open(config.get_config_path('conf.d/test.conf'), 'w') as f:
            f.write("[test]\ntest_value_0=100\ntest_value_blank=\ntest_string=\nlocal=4\n"+
                    "[sec2]\nsome=value\ntest_value_blank=not blank\n")

        # Kill augeas
        config.augeas = None

    def tearDown(self):
        shutil.rmtree(self.tempdir)

class ConfigTest(ConfigTestBase):
    def setUp(self):
        super(ConfigTest, self).setUp()
        global gco, sco
        gco = config.get_config_option
        sco = config.set_config_option

    def test_get_path(self):
        self.assertEqual(config.get_config_path("conf.d"), "%s/conf.d" % self.tempdir)
        self.assertEqual(config.get_config_path("conf.d/"), "%s/conf.d/" % self.tempdir)
        self.assertEqual(config.get_config_path("/conf.d/"), "%s/conf.d/" % self.tempdir)

    def test_get_basic(self):
        self.assertEqual(gco('system', 'test_value_0'), '42')
        self.assertEqual(gco('system', 'test_value_blank'), None)
        self.assertEqual(gco('system', 'test_string'), 'hello there')
        self.assertEqual(gco('system', 'not_set'), None)

        self.assertEqual(gco('test', 'test_string'), None)
        self.assertEqual(gco('test', 'local'), '4')

        # Test with defaults
        self.assertEqual(gco('system', 'test_string', 'def'), 'hello there')
        self.assertEqual(gco('system', 'not_set', 'def'), 'def')

    def test_get_multiapp(self):
        self.assertEqual(gco('system', 'test_value_0', app="test"), None)
        self.assertEqual(gco('system', 'test_value_0', app=["test", "system"]), '42')

    def test_get_multisection(self):
        self.assertEqual(gco('sec2', 'some', app='test'), 'value')
        self.assertEqual(gco(['sec2', 'test'], 'test_value_blank', app='test'), 'not blank')
        self.assertEqual(gco(['test', 'system'], 'test_string'), 'hello there')


    def test_set_basic(self):
        self.assertEqual(sco('system', 'new_value', '666'), True)
        self.assertEqual(gco('system', 'new_value'), '666')

        self.assertEqual(sco('system', 'test_string', 'goodbye'), True)
        self.assertEqual(gco('system', 'test_string'), 'goodbye')

    def test_set_new(self):
        self.assertFalse(os.path.exists(config.get_config_path("conf.d/newfile.conf")))
        self.assertEqual(sco('newfile', 'new_value', '666'), True)
        self.assertTrue(os.path.exists(config.get_config_path("conf.d/newfile.conf")))

        self.assertEqual(gco('newfile', 'new_value'), '666')

    def test_unwritable(self):
        with open(config.get_config_path('conf.d/blocked.conf'), 'w') as f:
            f.write("[blocked]\nnop=nop\n")

        os.chmod(config.get_config_path('conf.d/blocked.conf'), 0444)

        self.assertEqual(gco('blocked', 'nop'), 'nop')

        # This shall fail, and write log msg
        self.assertEqual(sco('blocked', 'new_value', '666'), False)

        os.chmod(config.get_config_path('conf.d/blocked.conf'), 0)

        # reload
        config.augeas = None
        self.assertEqual(gco('blocked', 'nop'), None)
        self.assertEqual(gco('blocked', 'nop', 'def'), 'def')



class AppConfigTest(ConfigTestBase):
    def setUp(self):
        super(AppConfigTest, self).setUp()
        self.app = AgoTest()

        global gco, sco
        gco = self.app.get_config_option
        sco = self.app.set_config_option

    def test_get_basic(self):
        self.assertEqual(gco('test_value_0'), '100')
        self.assertEqual(gco('test_value_1'), None)
        self.assertEqual(gco('test_string'), None)

    def test_get_section(self):
        # Only read system
        self.assertEqual(gco('test_value_0', section='system'), '42')

        # Fallback on system
        self.assertEqual(gco('test_value_0', section=[None, 'system']), '100')
        self.assertEqual(gco('test_string', section=[None, 'system']), 'hello there')

    def test_get_app(self):
        self.assertEqual(gco('test_value_0', app=[None, 'system']), '100')

        # Will give None, since we're looking in section test, which is not in system.conf
        self.assertEqual(gco('test_value_0', app=['other', 'system']), None)

        self.assertEqual(gco('test_value_0', app=['other', 'system'], section='system'), '42')

    def test_set_basic(self):
        self.assertEqual(sco('new_value', '666'), True)
        self.assertEqual(gco('new_value'), '666')

        self.assertEqual(sco('test_string', 'goodbye'), True)
        self.assertEqual(gco('test_string'), 'goodbye')

    def test_set_section(self):
        self.assertEqual(sco('some_value', '0', 'sec2'), True)
        self.assertEqual(gco('some_value'), None)
        self.assertEqual(gco('some_value', section='sec2'), '0')

    def test_set_new_file(self):
        self.assertFalse(os.path.exists(config.get_config_path("conf.d/newfile.conf")))
        self.assertEqual(sco('new_value', '666', section='newfile'), True)
        self.assertTrue(os.path.exists(config.get_config_path("conf.d/newfile.conf")))

        self.assertEqual(gco('new_value', section='newfile'), '666')

