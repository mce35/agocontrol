#!/usr/bin/env python

import gammu
import sys

import agoclient

class AgoGammu(agoclient.AgoApp):
    def message_handler(self, internalid, content):
        if "command" in content:
            if content["command"] == "sendsms":
                if 'text' in content:
                    if 'to' in content:
                        message = {
                                'Text': content["text"],
                                'SMSC': {'Location': 1},
                                'Number': content["to"],
                        }
                        self.sm.SendSMS(message)
    def setup_app(self):
        self.connection.add_handler(self.message_handler)
        self.connection.add_device("smsgateway", "smsgateway")

        self.log.info("Initialising gammu")
        # Create object for talking with phone
        self.sm = gammu.StateMachine()
        # Read the configuration (~/.gammurc)
        self.sm.ReadConfig()
        # Connect to the phone
        self.sm.Init()

    def cleanup_app(self):
        pass

if __name__ == "__main__":
    AgoGammu().main()

