import os
import os.path as ospath
from augeas import Augeas
from threading import Lock

from _directories import *

# For old-style * exports
# Do not add new methods here; instead import what you really need
__all__ = ["get_config_option", "set_config_option", "BINDIR", "CONFDIR", "LOCALSTATEDIR"]

CONFDIR = None
LOCALSTATEDIR = None

def set_config_dir(path):
    global CONFDIR
    # TODO: Verify path
    CONFDIR = path

def set_localstate_dir(path):
    global LOCALSTATEDIR
    # TODO: Verify path
    LOCALSTATEDIR = path

def get_config_path(subpath=None):
    if not _directories_inited:
        init_directories()

    if not subpath:
        return CONFDIR

    return ospath.join(CONFDIR, subpath)

def get_localstate_path(subpath=None):
    if not _directories_inited:
        init_directories()

    if not subpath:
        return LOCALSTATEDIR

    return ospath.join(LOCALSTATEDIR, subpath)

_directories_inited = False
def init_directories():
    _directories_inited = True
    if CONFDIR is None:
        set_config_dir(os.environ.get('AGO_CONFDIR', DEFAULT_CONFDIR))

    if LOCALSTATEDIR is None:
        set_localstate_dir(os.environ.get('AGO_LOCALSTATEDIR', DEFAULT_LOCALSTATEDIR))


# Postpone until after CONFDIR has been configured
augeas = None
def _augeas_init():
    if not _directories_inited:
        init_directories()

    global augeas
    augeas = Augeas(loadpath=get_config_path(),
            flags=Augeas.SAVE_BACKUP | Augeas.NO_MODL_AUTOLOAD)
    augeas.set("/augeas/load/Agocontrol/lens", "agocontrol.lns");
    augeas.set("/augeas/load/Agocontrol/incl[1]", get_config_path("conf.d") + "/*.conf");
    augeas.load()

CONFIG_LOCK = Lock()

def _augeas_path(section, option, filename=None):
    if filename == None:
        filename = section
    return "/files%s/conf.d/%s.conf/%s/%s" % (get_config_path(), filename, section, option)


# TODO: Clean up filename vs section handling
def get_config_option(section, option, default, filename=None, fallback_section = None):
    """Read a config option from a .ini style file."""
    if not augeas: _augeas_init()

    CONFIG_LOCK.acquire(True)
    try:
        value = augeas.get(_augeas_path(section, option, filename=filename))
        if not value and fallback_section:
            value = augeas.get(_augeas_path(fallback_section, option))
    finally:
        CONFIG_LOCK.release()

    if not value:
        value = default
    return value


def set_config_option(section, option, value, filename=None):
    """Write out a config option to a .ini style file."""
    if not augeas: _augeas_init()

    CONFIG_LOCK.acquire(True)
    try:
        if filename:
            path = "/files" + CONFDIR + '/conf.d/' + filename + '.conf/' + section + '/' + option
        else:
            path = "/files" + CONFDIR + '/conf.d/' + section + '.conf/' + section + '/' + option
        print path
        augeas.set(path, value)
        augeas.save()

        return True
    except IOError, exception:
        logging.error("Failed to write configuration file: %s", exception)
        return False
    finally:
        CONFIG_LOCK.release()


