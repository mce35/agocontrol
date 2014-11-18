import sys
import logging
import os.path
from logging.handlers import SysLogHandler
import argparse
import signal

import _logging
import _directories
import config
from agoconnection import AgoConnection

__all__ = ["AgoApp"]

class StartupError(Exception):
    pass
class ConfigurationError(Exception):
    pass

class AgoApp:
    """This is a base class for all Python AgoControl applications

    An app implementation should create one main class which extends
    this class.
    Also, it is responsible for calling the main function:

        if __name__ == "__main__":
            TheApp().main()

    """
    def __init__(self):
        self.app_name = self.__class__.__name__
        if self.app_name.find("Ago") == 0:
            self.app_short_name = self.app_name[3:].lower()
        else:
            self.app_short_name = self.app_name.lower()

        self.log = logging.getLogger(self.app_name)

    def parse_command_line(self, argv):
        parser = argparse.ArgumentParser(add_help=False)
        LOG_LEVELS = ['TRACE', 'DEBUG', 'INFO', 'WARNING', 'ERROR', 'FATAL']

        parser.add_argument('-h', '--help', action='store_true',
                help='show this help message and exit')

        parser.add_argument('--log-level', dest="log_level",
                help='Log level', choices = LOG_LEVELS)
        parser.add_argument('--log-method', dest="log_method",
                help='Where to log', choices = ['console', 'syslog'])

        facilities = SysLogHandler.facility_names.keys()
        facilities.sort()
        parser.add_argument('--log-syslog-facility', dest="syslog_facility",
                help='Which syslog facility to log to.',
                choices = facilities)

        parser.add_argument('-d', '--debug', action='store_true',
                help='Shortcut to set console logging with level DEBUG')
        parser.add_argument('-t', '--trace', action='store_true',
                help='Shortcut to set console logging with level TRACE')

        parser.add_argument('--config-dir', dest="config_dir",
                help='Directory with configuration files')
        parser.add_argument('--state-dir', dest="state_dir",
                help='Directory with local state files')

        self.app_cmd_line_options(parser)

        # If parsing fails, this will print error and exit
        args = parser.parse_args(argv[1:])

        if args.config_dir:
            config.set_config_dir(args.config_dir)

        if args.state_dir:
            config.set_localstate_dir(args.state_dir)

        if args.help:
            config.init_directories()

            parser.print_help()

            print
            print "Paths"
            print "  Default config dir: %s" % _directories.DEFAULT_CONFDIR
            print "  Default state dir : %s" % _directories.DEFAULT_LOCALSTATEDIR
            print "  Active config dir : %s" % config.CONFDIR
            print "  Active state dir  : %s" % config.LOCALSTATEDIR
            print
            print "System configuration file      : %s" % config.get_config_path('conf.d/system.conf')
            print "App-specific configuration file: %s" % config.get_localstate_path('conf.d/%s.conf'% self.app_short_name)
            print

            return False

        self.args = args
        return True

    def app_cmd_line_options(self, parser):
        """Override this to add your own command line options"""
        pass

    def setup(self):
        self.setup_logging()
        self.setup_connection()
        self.setup_signals()
        self.setup_app()

    def cleanup(self):
        self.log.trace("Cleaning up")
        self.cleanup_app()
        self.cleanup_connection()

    def setup_logging(self):
        root = logging.getLogger()
        # Find log level..
        if self.args.trace:
            lvl_name = "TRACE"
        elif self.args.debug:
            lvl_name = "DEBUG"
        elif self.args.log_level:
            lvl_name = self.args.log_level
        else:
            lvl_name = config.get_config_option(self.app_short_name, "log_level", "INFO",
                    fallback_section="system")

            if lvl_name.upper() not in logging._levelNames:
                raise ConfigurationError("Invalid log_level %s" % lvl_name)

        # ..and set it
        lvl = logging._levelNames[lvl_name.upper()]
        root.setLevel(lvl)

        # Find log method..
        if self.args.trace or self.args.debug:
            log_method = 'console'
        elif self.args.log_method:
            log_method = self.args.log_method
        else:
            log_method = config.get_config_option(self.app_short_name, "log_method", "console",
                    fallback_section="system")

            if log_method not in ['console', 'syslog']:
                raise ConfigurationError("Invalid log_method %s" % log_method)

        # ..and set it
        if log_method == 'console':
            self.log_handler = logging.StreamHandler(None)
            self.log_formatter = logging.Formatter("%(asctime)-15s %(name)-10s %(levelname)-5s %(message)s")
        elif log_method == 'syslog':
            if self.args.syslog_facility:
                syslog_fac = self.args.syslog_facility
            else:
                syslog_fac = config.get_config_option(self.app_short_name, "syslog_facility",
                        "local0", fallback_section = "system")

                if syslog_fac not in SysLogHandler.facility_names:
                    raise ConfigurationError("Invalid syslog_facility %s" % syslog_fac)

            # Try to autodetect OS syslog unix socket
            for f in ["/dev/log", "/var/run/log"]:
                if os.path.exists(f):
                    syslog_path = f
                    break

            self.log_handler = SysLogHandler(
                    address = syslog_path,
                    facility = SysLogHandler.facility_names[syslog_fac])

            self.log_formatter = logging.Formatter(self.app_name.lower() + " %(name)-10s %(levelname)-5s %(message)s")

        self.log_handler.setFormatter(self.log_formatter)

        # Forcibly limit QPID logging to INFO
        logging.getLogger('qpid').setLevel(max(root.level, logging.INFO))

        root.addHandler(self.log_handler)

    def setup_connection(self):
        self.connection = AgoConnection(self.app_short_name)

    def cleanup_connection(self):
        if self.connection:
            self.connection.shutdown()
            self.connection = None

    def setup_signals(self):
        signal.signal(signal.SIGINT, self._sighandler)
        signal.signal(signal.SIGQUIT, self._sighandler)

    def _sighandler(self, signal, frame):
        self.log.debug("Exit signal catched, shutting down")
        self.signal_exit()

    def signal_exit(self):
        """Call this to begin shutdown procedures"""
        self.exit_signaled = True
        self._do_shutdown()

    def _do_shutdown(self):
        if self.connection:
            self.connection.shutdown()

    def setup_app(self):
        """This should be overriden by the application to setup app specifics"""
        pass

    def cleanup_app(self):
        """This should be overriden by the application to cleanup app specifics"""
        pass

    def main(self, argv=None):
        """Main entrypoint, called by the application
        If argv is not set, sys.argv is used.
        """
        if not argv:
            argv = sys.argv

        ret = self._main(argv)
        sys.exit(ret)

    def _main(self, argv):
        """Internal main function, return value will be the OS exit code"""
        if not self.parse_command_line(argv):
            return 1

        try:
            self.setup()
        except StartupError:
            self.cleanup()
            return 1

        except ConfigurationError,e:
            self.log.error("Failed to start %s due to configuration error: %s",
                    self.app_name, e.message)
            self.cleanup()
            return 1

        try:
            self.log.warning("Starting %s", self.app_name)
            ret = self.appMain()
            self.log.debug("Shutting down %s", self.app_name)

            self.cleanup()

            if ret == 0:
                self.log.info("Exiting %s", self.app_name)
            else:
                self.log.warn("Exiting %s (code %d)", self.app_name, ret)
        except:
            self.log.critical("Unhandled exception, crashing",
                    exc_info=True)

            return 1


    def appMain(self):
        self.connection.run()
        return 0

