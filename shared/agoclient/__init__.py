"""@package agoclient
This is the agocontrol python client library.

An example device can be found here:
http://wiki.agocontrol.com/index.php/Example_Device
"""

import syslog
import sys

# Re-export each export module
from agoapp import *
from config import *
from agoconnection import *

syslog.openlog(sys.argv[0], syslog.LOG_PID, syslog.LOG_DAEMON)

