import logging

# We want to keep C++ and Python ago-coding as similar as possible.
# In C++ we have levels TRACE and CRITICAL.
#

# Add extra Trace level
TRACE = logging.TRACE = 5
logging._levelNames[TRACE] = TRACE
logging._levelNames['TRACE'] = TRACE

# C++ impl has FATAL, in python we have CRITICAL
# Add this alias so we can use the same logging consts
FATAL = logging.FATAL = logging.CRITICAL
logging._levelNames[FATAL] = FATAL
logging._levelNames['FATAL'] = FATAL

LOGGING_LOGGER_CLASS = logging.getLoggerClass()

class AgoLoggingClass(LOGGING_LOGGER_CLASS):
    def trace(self, msg, *args, **kwargs):
        if self.isEnabledFor(TRACE):
            self._log(TRACE, msg, args, **kwargs)

    def fatal(self, msg, *args, **kwargs):
        if self.isEnabledFor(FATAL):
            self._log(FATAL, msg, args, **kwargs)


if logging.getLoggerClass() is not AgoLoggingClass:
    logging.setLoggerClass(AgoLoggingClass)
    logging.addLevelName(TRACE, 'TRACE')
    logging.addLevelName(FATAL, 'FATAL')


# TODO: Syslog priority mapping
