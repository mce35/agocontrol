/**
 * @file   agolog.h
 * @author Manuel CISSÉ <cisse.manuel+agocontrol@gmail.com>
 * @date   Thu Nov 11 13:04:33 2010
 *
 * @brief  Include file of the logging facility
 */
#ifndef __AGOLOG_H__
#define __AGOLOG_H__

#include <unistd.h>
#include <syslog.h>

/**
 * Open log file according to the options specified
 *
 * @param app_name    application name
 * @param use_syslog  log to syslog if != 0
 * @param log_file    log file to use, NULL -> no log file, "-" -> stdout
 * @param uid         uid for the log file, -1 -> current uid
 * @param gid         gid for the log file, -1 -> current gid
 * @param log_debug   log debug messages if != 0
 *
 * @return 0 on success, -1 on error
 */
int  agolog_open(const char *app_name,int use_syslog,
                 const char *log_file,uid_t uid,gid_t gid,
                 int log_debug);

/**
 * Log a message
 *
 * @param priority message priority, one of:
 *                  - @c LOG_DEBUG   debug-level message
 *                  - @c LOG_INFO    informational message
 *                  - @c LOG_NOTICE  normal, but significant, condition
 *                  - @c LOG_WARNING warning conditions
 *                  - @c LOG_ERR     error conditions
 *                  - @c LOG_CRIT    critical conditions
 *                  - @c LOG_ALERT   action must be taken immediately
 *                  - @c LOG_EMERG   system is unusable
 * @param fmt  format of the message (cf. printf)
 */
void agolog(int priority,const char *fmt,...);

/**
 * Close log file
 */
void agolog_close(void);

#endif /* __LOG_H__ */
