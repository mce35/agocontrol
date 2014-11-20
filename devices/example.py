"""Agoclient example."""

# ago client library example device
#
# copyright (c) 2013 Harald Klein <hari+ago@vt100.at>
#

# to use the client library we have to import it

import agoclient

# we'll also use a BACKGROUND thread in this example

import threading
import time


# Every app should have one main class, which extends
# the AgoApp class.
# This provides boiler plate code common for all applications.
#
# The class name should be Ago<CLIENT>, and will be used
# to uniquely name the mappings file for internal id-to-uuid
# mapping, and to identify the CLIENT devices ("handled-by" parameter)
#
# Examples for the instance name would be "zwave" for
# our z-wave device, "knx" for KNX, "owfs" for the one
# wire device, and so on.
#
# Just chose a name that is unique and speaks for itself.
# It is best practice to also use that instance name as
# section name in the config file if you need any config parameters.
#
# When developing, it is recommended to run with -d or -t argument,
# to enable debug/trace logging and log to console.
# Please refer to -h for other options.
#
class AgoExample(agoclient.AgoApp):
    """AgoControl device with support for the Example protocol"""
    # If your application has any custom command line parameters, implement
    # the app_cmd_line_options method. Else, skip it
    def app_cmd_line_options(self, parser):
        """App-specific command line options"""
        parser.add_argument('-T', '--test', action="store_true",
                help="This can be set or not set")

        parser.add_argument('--set-parameter', 
                help="Set this to do set_config_option with the value upon startup")


    # the message_handler method will be called by the CLIENT
    # library when a message comes in that is destined for
    # one of the child devices you're handling
    # the first parameter is your internal id (all the mapping
    # from ago control uuids to the internal ids is handled
    # transparently for you)
    # the second parameter is a dict with the message content
    def message_handler(self, internalid, content):
        """The messagehandler."""
        if "command" in content:
            if content["command"] == "on":
                # If we want to log state, we shall use self.log
                # This supports multiple levels:
                #   trace, debug, info, warning, error, fatal
                #
                self.log.debug("switching on: %s", internalid)

                # TODO: insert real stuff here, this
                            # is where you would do action and
                            # switch on your child device
                # depending on if the operation was
                            # successful, you want to report state
                            # back to ago control. We just send
                            # 255 in this case so that ago control
                            # changes the device state in the
                            # inventory to on

                # Emit an event which notifies all other components of the state change
                self.connection.emit_event(internalid,
                    "event.device.statechanged", "255", "")

            elif content["command"] == "off":
                self.log.debug("switching off: %s", internalid)
                self.connection.emit_event(internalid, "event.device.statechanged", "0", "")

    def setup_app(self):
        # specify our message handler method
        self.connection.add_handler(self.message_handler)

        # If we want to use the custom command line argument, we look at
        # self.args:
        if self.args.test:
            self.log.info("Test argument was set")
        else:
            self.log.warning("Test argument was NOT set")

        # if you need to fetch any settings from config.ini,
        # use the self.get_config_option method.
        # This will look in the configuration file conf.d/<app name>.conf
        # under the [<app name>] section.
        # In this example, this means example.conf and [example] section
        param = self.get_config_option("some_key", "0")
        self.log.info("Configuration parameter 'some_key' was set to %s", param)

        if self.args.set_parameter:
            self.log.info("Setting configuration parameter 'some_key' to %s", self.args.set_parameter)
            self.set_config_option("some_key", self.args.set_parameter)


            param = self.get_config_option("some_key", "0")
            self.log.info("Configuration parameter 'some_key' is now set to %s", param)

        # Now you need to tell the AgoConnection object about the
        # devices you provide.
        #
        # The add_device call expects a internal id
        # and a device type (you can find all valid types in the
        # schema.yaml configuration file). The internal id is whatever
        # you're using in your code to distinct your devices.
        # Or the pin number of some GPIO output. Or the IP of a
        # networked device. Whatever fits your device specific stuff.
        # The persistent translation to a ago control uuid will be
        # done by the AgoConnection object, by storing the mappings as a json
        # file in CONFDIR/uuidmap/<instance name>.json.
        # You don't need to worry at all about this, when the
        # message_handler is called, you'll be passed the internalid
        # for the device that you did specifiy when using add_device()

        # we add a switch and a dimmer
        self.connection.add_device("123", "dimmer")
        self.connection.add_device("124", "switch")

        # for our threading example in the next section we also add a binary sensor:
        self.connection.add_device("125", "binarysensor")

        # then we add a BACKGROUND thread. This is not required and
        # just shows how to send events from a separate thread. This might
        # be handy when you have to poll something in the BACKGROUND or need
        # to handle some other communication. If you don't need one or if
        # you want to keep things simple at the moment just skip this section.
        BACKGROUND = TestEvent(self)
        BACKGROUND.setDaemon(True)
        BACKGROUND.start()

        # now you should have added all devices, set up all your internal
        # and device specific stuff, started everything like listener threads
        # or whatever.
        # setup_app will now return, and AgoApp will do the rest.
        # This normally means calling app_main, which calls the run() method
        # on the AgoConnection object, which will block and start message handling.

    def app_cleanup(self):
        # When our app is about to shutdown, we should clean up any resources we've
        # allocated in app_setup. This is done here.
        # In this example, we do not have any resources..
        pass



class TestEvent(threading.Thread):
    """Test Event."""
    def __init__(self, app):
        threading.Thread.__init__(self)
        self.app = app

    def run(self):
        level = 0
        # This is a dummy backend thread. It loops forever.
        # It is important that we exit when told to, by checking is_exit_signaled.
        #
        # TODO: Improve this example; we do not handle proper shutdown..
        while not self.app.is_exit_signaled():
            self.app.connection.emit_event("125",
                "event.security.sensortriggered", level, "")
            if (level == 0):
                level = 255
            else:
                level = 0
            time.sleep(5)

# Finally, but very important.
# We must call the main function of the application object
if __name__ == "__main__":
    AgoExample().main()

