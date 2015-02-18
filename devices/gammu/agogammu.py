#!/usr/bin/env python

import gammu
import sys

import agoclient

CONFIGFILE = CONFDIR + "/gammu/gammu.rc"

class AgoGammu(agoclient.AgoApp):
    def message_handler(self, internalid, content):
        returnvalue = {}
        returncode = {}
        if "command" in content:
            if content["command"] == "sendsms":
                if 'text' in content:
                    if 'to' in content:
                        message = {
                                'Text': content["text"],
                                'SMSC': {'Location': 1},
                                'Number': content["to"],
                        }
                        try:
                            self.sm.SendSMS(message)
                            returncode["code"]="success"
                            returncode["message"]="SMS was passed to the Gammu stack"
                            returnvalue["result"]=returncode
                        except gammu.GSMError:
                            returncode["code"]="error.cannot.send.SMS"
                            returncode["message"]="Cannot send SMS via Gammu"
                            returnvalue["error"]=returncode
                    else:
                        returncode["code"]="error.parameter.missing"
                        returncode["message"]="paramter 'to' is missing"
                        returnvalue["error"]=returncode
                else:
                    returncode["code"]="error.parameter.missing"
                    returncode["message"]="paramter 'text' is missing"
                    returnvalue["error"]=returncode
            else:
                returncode["code"]="error.command.invalid"
                returnvalue["error"]=returncode
        else:
            returncode["code"]="error.command.missing"
            returnvalue["error"]=returncode

        return returnvalue
    def setup_app(self):
        self.connection.add_handler(self.message_handler)
        self.connection.add_device("smsgateway", "smsgateway")

        self.log.info("Initialising gammu")
        # Create object for talking with phone
        self.sm = gammu.StateMachine()
        # Read the configuration
        self.sm.ReadConfig(CONFIGFILE)
        # Connect to the phone
        self.sm.Init()

    def cleanup_app(self):
        pass

if __name__ == "__main__":
    AgoGammu().main()

