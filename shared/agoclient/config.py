import os
import os.path as ospath
from augeas import Augeas
from threading import Lock
import logging

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

    if subpath[0] == os.sep: subpath = subpath[1:]
    return ospath.join(CONFDIR, subpath)

def get_localstate_path(subpath=None):
    if not _directories_inited:
        init_directories()

    if not subpath:
        return LOCALSTATEDIR

    if subpath[0] == os.sep: subpath = subpath[1:]
    return ospath.join(LOCALSTATEDIR, subpath)

_directories_inited = False
def init_directories():
    _directories_inited = True
    if CONFDIR is None:
        set_config_dir(os.environ.get('AGO_CONFDIR', DEFAULT_CONFDIR))

    if LOCALSTATEDIR is None:
        set_localstate_dir(os.environ.get('AGO_LOCALSTATEDIR', DEFAULT_LOCALSTATEDIR))

# TODO: Remove when all apps have been verified to NOT use CONFDIR without
# first calling init_directories one way or another. Preferably by using get_config_path instead..
init_directories()

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

def _augeas_path(filename, section, option):
    return "/files%s/conf.d/%s.conf/%s/%s" % (get_config_path(), filename, section, option)


def get_config_option(section, option, default_value=None, app=None):
    """Read a config option from the configuration subsystem.

    The system is based on per-app configuration files, which has sections
    and options.

    This is a low-level implementation, please try to use AgoApp.get_config_option
    instead.

    Note that "option not set" means not set, or empty value!

    Arguments:
        section -- A string section to look for the option in, or an iterable
            with multiple sections to try in the defined order, in case the option
            was not set.

        option -- The name of the option to retreive

        default_value -- If the option can not be found in any of the specified
            sections, fall back to this value.

        app -- A string identifying the configuration storage unit to look in.
            Can also be an iterable with multiple units to look at. If the option
            was not found in any of the sections specified, we test the next app.
            If omited, it defaults to the section.

    Returns:
        A unicode object with the value found in the data store, if found.
        If not found, default_value is passed through unmodified.
    """
    if not augeas: _augeas_init()

    if type(section) == str:
        section = (section,)

    if app is None:
        app = section
    elif type(app) == str:
        app = (app,)

    CONFIG_LOCK.acquire(True)
    try:
        for fn in app:
            for s in section:
                path = _augeas_path(fn, s, option)
                value = augeas.get(path)
                if value:
                    # First match
                    return value
    finally:
        CONFIG_LOCK.release()

    # Fall back on default value
    return default_value


def set_config_option(section, option, value, app=None):
    """Write a config option to the configuration subsystem.

    The system is based on per-app configuration files, which has sections
    and options.

    This is a low-level implementation, please try to use AgoApp.get_config_option
    instead.

    Arguments:
        section -- A string section in which to store the option in.

        option -- The name of the option to set

        value -- The value of the option.

        app -- A string identifying the configuration storage unit to look in.
            If omited, it defaults to the section.

    Returns:
        True if succesfully stored, False otherwise.
        Please refer to the error log for failure indication.

    """
    if not augeas: _augeas_init()

    if app is None:
        app = section

    CONFIG_LOCK.acquire(True)
    try:
        path = _augeas_path(app, section, option)
        augeas.set(path, value)
        augeas.save()

        return True
    except IOError, exception:
        logging.error("Failed to write configuration file: %s", exception)
        return False
    finally:
        CONFIG_LOCK.release()


def _iterable_replace_none(iterable, value):
    if type(iterable) not in (list, tuple):
        return

    for i in range(0, len(iterable)):
        if iterable[i] is None:
            iterable[i] = value

