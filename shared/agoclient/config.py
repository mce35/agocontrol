import syslog
import augeas
from threading import Lock

from directories import *
__all__ = ["get_config_option", "set_config_option", "BINDIR", "CONFDIR", "LOCALSTATEDIR"]


augeas =  augeas.Augeas()
CONFIG_LOCK = Lock()
def get_config_option(section, option, default, filename=None):
    """Read a config option from a .ini style file."""
    CONFIG_LOCK.acquire(True)
    try:
        if filename:
            value = augeas.get("/files" + CONFDIR + '/conf.d/' + filename + '.conf/' + section + '/' + option)
        else:
            value = augeas.get("/files" + CONFDIR + '/conf.d/' + section + '.conf/' + section + '/' + option)
    except:
        syslog.syslog(syslog.LOG_WARNING, "Can't parse config file")
    finally:
        CONFIG_LOCK.release()
    if not value:
        value = default
    return value


def set_config_option(section, option, value, filename=None):
    """Write out a config option to a .ini style file."""
    result = False
    CONFIG_LOCK.acquire(True)
    try:
        if filename:
            path = "/files" + CONFDIR + '/conf.d/' + filename + '.conf/' + section + '/' + option
        else:
            path = "/files" + CONFDIR + '/conf.d/' + section + '.conf/' + section + '/' + option
        print path
        augeas.set(path, value)
        augeas.save()
        result = True
    except IOError, exception:
        syslog.syslog(syslog.LOG_ERR,
            "Can't write config file: " + str(exception))
        result = False
    finally:
        CONFIG_LOCK.release()
    return result


