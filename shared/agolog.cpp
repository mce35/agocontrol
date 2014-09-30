/**
 * @file   agolog.cpp
 * @author Manuel CISSÉ <cisse.manuel+agocontrol@gmail.com>
 * @date   Thu Nov 11 13:05:26 2010
 *
 * @brief  Provide logging facility
 */
#include "agolog.h"
#include <time.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>

static int glog_fd=-1;
static int glog_debug=0;
static int glog_use_syslog=0;
static char glog_file[1024]="";
static char gapp_name[128];
static pthread_mutex_t glog_mutex;

int agolog_open(const char *app_name,int use_syslog,
                const char *log_file,uid_t uid,gid_t gid,
                int log_debug)
{
  glog_debug=log_debug;
  glog_use_syslog=use_syslog;

  strncpy(gapp_name, app_name, sizeof(gapp_name));
  gapp_name[sizeof(gapp_name)-1]=0;

  pthread_mutex_init(&glog_mutex, NULL);

  if(strlen(log_file)>=sizeof(glog_file))
    {
      fprintf(stderr,"Log file '%s' too long, not used\n",
              log_file);
      log_file=NULL;
    }

  if(glog_use_syslog!=0)
    openlog(app_name,LOG_PID,LOG_DAEMON);

  if(log_file!=NULL)
    {
      strcpy(glog_file,log_file);
      if(strcmp(glog_file,"-")==0)
        glog_fd=STDOUT_FILENO;
      else
        {
          glog_fd=open(glog_file,O_WRONLY|O_APPEND|O_CREAT,0640);
          if(glog_fd<0)
            {
              fprintf(stderr,"Cannot open log file '%s': %s\n",
                      glog_file,strerror(errno));
              return -1;
            }
          if((signed) uid!=-1 || (signed) gid!=-1)
            if(fchown(glog_fd,uid,gid)<0)
              agolog(LOG_WARNING,"Cannot change ownership of log file '%s': %s\n",
                     glog_file,strerror(errno));
        }
    }

  agolog(LOG_INFO,"Log system started\n");

  return 0;
}

void agolog(int priority,const char *fmt,...)
{
  char buff[1024];
  va_list list;
  char *ptr;

  if(priority==LOG_DEBUG && glog_debug==0)
    return;

  va_start(list,fmt);
  vsnprintf(buff,sizeof(buff),fmt,list);
  va_end(list);

  ptr = buff;
  while((ptr = strchr(ptr, '\n')) != NULL) *ptr = ' ';

  if(glog_use_syslog!=0)
    syslog(LOG_DAEMON|priority,"%s",buff);
  if(glog_fd>=0)
    {
      char prefix[512];
      struct tm ltime;
      time_t t;
      int last_errno;

      pthread_mutex_lock(&glog_mutex);

      last_errno=errno;
      t=time(NULL);
      localtime_r(&t,&ltime);
      snprintf(prefix,sizeof(prefix),"%.4i-%.2i-%.2i %.2i:%.2i:%.2i %s[%i]: ",
               ltime.tm_year+1900,ltime.tm_mon+1,ltime.tm_mday,
               ltime.tm_hour,ltime.tm_min,ltime.tm_sec,gapp_name,getpid());
      write(glog_fd,prefix,strlen(prefix));
      switch(priority)
        {
        case LOG_EMERG:
        case LOG_ALERT:
        case LOG_CRIT:
          strcpy(prefix,"[##] ");
          break;
        case LOG_ERR:
          strcpy(prefix,"[**] ");
          break;
        case LOG_WARNING:
          strcpy(prefix,"[==] ");
          break;
        default:
        case LOG_NOTICE:
        case LOG_INFO:
          strcpy(prefix,"[--] ");
          break;
        case LOG_DEBUG:
          strcpy(prefix,"[DD] ");
          break;
        };
      write(glog_fd,prefix,strlen(prefix));
      if(strlen(buff)>0)
        {
          write(glog_fd,buff,strlen(buff));
          if(buff[strlen(buff)-1]!='\n')
            write(glog_fd,"\n",1);
        }
      errno=last_errno;

      pthread_mutex_unlock(&glog_mutex);
    }
}

void agolog_close(void)
{
  if(glog_fd>=0)
    close(glog_fd);
  if(glog_use_syslog!=0)
    closelog();
  glog_use_syslog=0;
  glog_fd=-1;
}

int agolog_reopen(void)
{
  int fd;

  if(glog_fd<0 || strcmp(glog_file,"")==0 ||
     strcmp(glog_file,"-")==0)
    return 0;

  fd=open(glog_file,O_WRONLY|O_APPEND|O_CREAT,0640);
  if(fd<0)
    {
      agolog(LOG_WARNING,"Cannot reopen logfile '%s': %s\n",
             glog_file,strerror(errno));
      return -1;
    }
  close(glog_fd);
  glog_fd=fd;

  return 0;
}
