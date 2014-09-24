/**
 * @file   agooregon.cpp
 * @author Manuel CISSE <manuel_cisse@yahoo.fr>
 * @date   Wed Sep 24 16:06:38 2014
 *
 * @brief  Oregon RMS300/RMS600/WMR100N/WMRS200 support
 *
 * Protocol description was taken from here :
 * https://github.com/ejeklint/WLoggerDaemon/blob/master/Station_protocol.md
 */
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <linux/hidraw.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "agoclient.h"

static const uint16_t oregon_device_id[][2] = {
  { 0x0FDE /* Oregon */, 0xCA01 /* RMS300 and possibly WMR100N/WMRS200 */ },
};

#define IDENTIFIER_TEMP_HUMIDITY 0x42
#define IDENTIFIER_DATE_TIME     0x60 /* not used at the moment */
#define IDENTIFIER_WIND          0x48 /* not used at the moment */
#define IDENTIFIER_PRESSURE      0x46 /* not used at the moment */
#define IDENTIFIER_RAIN          0x41 /* not used at the moment */
#define IDENTIFIER_UV_RADIATION  0x47 /* not used at the moment */
#define LENGTH_TEMP_HUMIDITY     12
#define LENGTH_DATE_TIME         12 /* not used at the moment */
#define LENGTH_WIND              11 /* not used at the moment */
#define LENGTH_PRESSURE          8  /* not used at the moment */
#define LENGTH_RAIN              17 /* not used at the moment */
#define LENGTH_UV_RADIATION      5  /* not used at the moment */

typedef struct OregonData {
  double          temperature[16];
  uint8_t         humidity[16];
  uint8_t         battery[16];
  time_t          last_update[16];
  bool            staled[16];
  char            hidraw_dev[32];
  int             device_fd;
  int             thread_loop;
  pthread_t       thread_id;
  pthread_mutex_t lock;
} OregonData;

static int DEBUG = 0;
static agocontrol::AgoConnection *agoConnection;
static OregonData gdata;

static int check_device(OregonData *data, const char *path);

static void close_device(OregonData *data)
{
  close(data->device_fd);
}

static int open_device(OregonData *data, int max_retries)
{
  unsigned int retry_count;
  DIR *dir;
  struct dirent *dirent;
  char buff[32];
  char found;

  close_device(data);

  retry_count = 0;
  found = false;
  while(found == false && retry_count <= max_retries)
    {
      /* first try to open last used device */
      if(strlen(data->hidraw_dev) > 0 &&
         check_device(data, data->hidraw_dev) == 0)
        return 0;

      /* if it fails, scan each device */
      dir = opendir("/dev");
      if(dir != NULL)
        {
          while(found == false && (dirent = readdir(dir)) != NULL)
            {
              if(strncmp(dirent->d_name, "hidraw", 6) == 0)
                {
                  snprintf(buff, sizeof(buff), "/dev/%s", dirent->d_name);
                  if(check_device(data, buff) == 0)
                    {
                      strncpy(data->hidraw_dev, buff, sizeof(data->hidraw_dev));
                      data->hidraw_dev[sizeof(data->hidraw_dev) - 1] = 0;
                      found = true;
                    }
                }
            }
          closedir(dir);
        }
      retry_count++;
      if(found == false && retry_count <= max_retries)
        {
          printf("oregon plugin: open failed, will retry in 5s\n");
          sleep(5);
        }
    }
  if(found == false)
    {
      printf("oregon plugin: open failed, aborting\n");

      return -1;
    }
  return 0;
}

static int oregon_checksum(const unsigned char *data, unsigned int len)
{
  unsigned int i;
  uint16_t checksum;

  if(len < 2)
    return -1;

  checksum = 0;
  for(i = 0; i < len - 2; i++)
    checksum += data[i];

  if(checksum != (data[i] + data[i + 1] * 256))
    return -1;

  return 0;
}

/* TODO: process rain/... measurements */
static void oregon_process_measurement(OregonData *data,
                                       unsigned char *msg,
                                       unsigned int len)
{
  if(len == LENGTH_TEMP_HUMIDITY && msg[1] == IDENTIFIER_TEMP_HUMIDITY)
    {                           /* all data received -> parse it */
      unsigned int channel;
      double temperature;
      uint8_t humidity;
      char temp_intid[128];
      char hum_intid[128];
      char bat_intid[128];
      char tmp_val[16];

      if(oregon_checksum(msg, LENGTH_TEMP_HUMIDITY) == 0)
        {
          channel = msg[2] & 0x0F;
          temperature = (((uint16_t) (msg[4] & 0x7F)) << 8 | msg[3]) / (double) 10;
          humidity = msg[5];
          if(msg[4] & 0x80)
            temperature = -temperature;

          snprintf(temp_intid, sizeof(temp_intid), "temp_%i", channel);
          snprintf(hum_intid, sizeof(hum_intid), "humidity_%i", channel);
          snprintf(bat_intid, sizeof(bat_intid), "battery_%i", channel);

          data->temperature[channel] = temperature;
          data->humidity[channel] = humidity;
          if(data->last_update[channel] == 0)
            {
              printf("oregon plugin: now monitoring channel %i\n", channel);
              agoConnection->addDevice(temp_intid, "temperaturesensor");
              if(humidity > 0)
                agoConnection->addDevice(hum_intid, "humiditysensor");
              agoConnection->addDevice(bat_intid, "batterysensor");
            }
          data->last_update[channel] = time(NULL);
          data->staled[channel] = false;
          data->battery[channel] = ((msg[0] >> 6) & 0x01) ^ 1; // revert battery value -> 0 bad, 1 -> OK

          snprintf(tmp_val, sizeof(tmp_val), "%.1f", data->temperature[channel]);
          agoConnection->emitEvent(temp_intid, "event.environment.temperaturechanged",
                                   tmp_val, "degC");

          if(data->humidity[channel] > 0)
            {
              snprintf(tmp_val, sizeof(tmp_val), "%d", data->humidity[channel]);
              agoConnection->emitEvent(hum_intid, "event.environment.humiditychanged",
                                       tmp_val, "%");
            }
          snprintf(tmp_val, sizeof(tmp_val), "%d", data->battery[channel]);
          if(data->battery[channel] == 0)
            agoConnection->emitEvent(bat_intid, "event.device.batterylevelchanged", tmp_val, " (bad)");
          else
            agoConnection->emitEvent(bat_intid, "event.device.batterylevelchanged", tmp_val, " (good)");

          if(DEBUG != 0)
            printf("oregon plugin: value channel %i: %.1f°C %d%% %d\n", channel,
                   data->temperature[channel], data->humidity[channel], data->battery[channel]);
        }
      else
        {
          printf("oregon plugin: invalid checksum!\n");
        }
    }
  else if(len == LENGTH_DATE_TIME && msg[1] == IDENTIFIER_DATE_TIME)
    {
      if(oregon_checksum(msg, LENGTH_DATE_TIME) == 0)
        {
          /* TODO: battery state */
        }
      else
        {
          printf("oregon plugin: invalid checksum!\n");
        }
    }
}

static void check_staled_device(OregonData *data)
{
  int i;

  for(i = 0; i < 16; i++)
    {
      if(data->last_update[i] > 0 && (time(NULL) - data->last_update[i]) > 1800 &&
         data->staled[i] == false)
        {
          char temp_intid[128];
          char hum_intid[128];
          char bat_intid[128];

          snprintf(temp_intid, sizeof(temp_intid), "temp_%i", i);
          snprintf(hum_intid, sizeof(hum_intid), "humidity_%i", i);
          snprintf(bat_intid, sizeof(bat_intid), "battery_%i", i);

          printf("oregon plugin: Channel %i staled!\n", i);
          agoConnection->suspendDevice(temp_intid);
          if(data->humidity[i] > 0)
            agoConnection->suspendDevice(hum_intid);
          agoConnection->suspendDevice(bat_intid);
          data->staled[i] = true;
        }
    }
}

static void *oregon_thread(OregonData *data)
{
  unsigned char buff[8];
  unsigned char msg[32];        /* longest msg is 17 bytes, 32 should be enough */
  unsigned int n;
  unsigned int p;
  unsigned int i;
  fd_set fdset;
  struct timeval tv;
  unsigned int no_data_times;

  pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
  pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

  p = 2;
  msg[0] = 0;                   /* flags */
  msg[1] = 0;                   /* identifier */
  no_data_times = 0;
  while(1)
    {
      FD_ZERO(&fdset);
      FD_SET(data->device_fd, &fdset);
      tv.tv_sec = 5;
      tv.tv_usec = 0;
      n = select(data->device_fd + 1, &fdset, NULL, NULL, &tv);
      if(n < 0)
        {
          printf("oregon plugin: device read error: %s\n", strerror(errno));
          printf("oregon plugin: reopening device\n");
          if(open_device(data, 5) == 0)
            {
              p = 2;
              msg[0] = 0;       /* flags */
              msg[1] = 0;       /* identifier */
              no_data_times = 0;
              continue;
            }
          printf("oregon: reopen failed, aborting\n");
          break;
        }
      else if(n == 0)
        {
          no_data_times++;
          if(no_data_times >= 200)
            {
              printf("oregon plugin: no data for 1000s, reopening device\n");
              if(open_device(data, 5) == 0)
                {
                  p = 2;
                  msg[0] = 0;   /* flags */
                  msg[1] = 0;   /* identifier */
                  no_data_times = 0;
                  continue;
                }
              printf("oregon: reopen failed, aborting\n");
              break;
            }
        }
      else
        {
          no_data_times = 0;

          n = read(data->device_fd, buff, sizeof(buff));
          if(n <= 0)
            {
              printf("oregon plugin: read failed (%s), reopening device\n", strerror(errno));
              if(open_device(data, 5) == 0)
                {
                  p = 2;
                  msg[0] = 0;   /* flags */
                  msg[1] = 0;   /* identifier */
                  continue;
                }
              printf("oregon: reopen failed, aborting\n");
              break;
            }
          if(n != 8 || n < buff[0] + (unsigned int) 1)
            {
              printf("oregon plugin: protocol error, reopening device\n");
              if(open_device(data, 5) == 0)
                {
                  p = 2;
                  msg[0] = 0;   /* flags */
                  msg[1] = 0;   /* identifier */
                  continue;
                }
              printf("oregon: reopen failed, aborting\n");
              break;
            }
          for(i = 1; i <= buff[0]; i++)
            {
              msg[p] = buff[i];
              oregon_process_measurement(data, msg, p+1);
              if(p > 0 && msg[p - 1] == 0xFF &&
                 msg[p] == 0xFF)        /* we have found a separator (0xFF 0xFF) */
                p = 0;                  /* -> restart at the beginning of the buffer */
              else
                p++;
              if(p >= sizeof(msg))      /* too much data for us to handle -> cycle through the buffer without  */
                p = 2;                  /* overwriting flags & identifier */
            }
        }
      check_staled_device(data);

      pthread_mutex_lock(&data->lock);
      if(data->thread_loop <= 0)
        {
          pthread_mutex_unlock(&data->lock);
          break;
        }
      pthread_mutex_unlock(&data->lock);
    }

  return NULL;
}

static int start_thread(OregonData *data)
{
  int status;

  pthread_mutex_lock(&data->lock);

  if(data->thread_loop != 0)
    {
      pthread_mutex_unlock(&data->lock);
      return -1;
    }

  data->thread_loop = 1;
  status = pthread_create(&data->thread_id, NULL,
                          (void * (*)(void *)) oregon_thread, data);
  if(status != 0)
    {
      data->thread_loop = 0;
      printf("oregon plugin: Starting thread failed.\n");
      pthread_mutex_unlock(&data->lock);
      return -1;
    }

  pthread_mutex_unlock(&data->lock);
  return 0;
}

static int stop_thread(OregonData *data)
{
  int status;

  pthread_mutex_lock(&data->lock);

  if(data->thread_loop == 0)
    {
      pthread_mutex_unlock(&data->lock);
      return -1;
    }

  data->thread_loop = 0;
  pthread_mutex_unlock(&data->lock);

  pthread_cancel(data->thread_id);
  status = pthread_join(data->thread_id, NULL);
  if(status != 0)
    {
      printf("oregon plugin: Stopping thread failed.\n");
      status = -1;
    }

  return status;
}

static int check_device(OregonData *data, const char *path)
{
  struct hidraw_devinfo devinfo;
  unsigned int i;

  data->device_fd = open(path, O_RDWR);
  if(data->device_fd < 0)
    {
      printf("oregon plugin: failed to open device '%s': %s\n", path, strerror(errno));
      return -1;
    }

  if(ioctl(data->device_fd, HIDIOCGRAWINFO, &devinfo) < 0)
    {
      printf("oregon plugin: RAWINFO ioctl failed: %s\n", strerror(errno));
      close(data->device_fd);
      return -1;
    }

  for(i = 0; i < sizeof(oregon_device_id)/sizeof(*oregon_device_id); i++)
    if(oregon_device_id[i][0] == (uint16_t) devinfo.vendor &&
       oregon_device_id[i][1] == (uint16_t) devinfo.product)
      break;
  if(i == sizeof(oregon_device_id)/sizeof(*oregon_device_id))
    {
      close(data->device_fd);
      return -1;
    }
  printf("oregon plugin: using device '%s'\n", path);

  /* RMS300 works fine without sending this init packet but WMR100/200 may need it */
  if(write(data->device_fd, "\x20\x00\x08\x01\x00\x00\x00\x00", 8) != 8)
    {
      printf("oregon plugin: write init failed: %s!\n", strerror(errno));
    }

  return 0;
}

static int oregon_init(void)
{
  unsigned int i;

  for(i = 0; i < sizeof(gdata.last_update)/sizeof(*gdata.last_update); i++)
    {
      gdata.last_update[i] = 0;
      gdata.staled[i] = false;
    }
  gdata.thread_loop = 0;
  pthread_mutex_init(&gdata.lock, NULL);

  if(open_device(&gdata, 0) < 0)
    {
      printf("oregon plugin: no suitable device found\n");

      return -1;
    }

  if(start_thread(&gdata) != 0)
    {
      close_device(&gdata);
      return -1;
    }

  return 0;
}

/**
 * Agocontrol command handler
 */
static qpid::types::Variant::Map commandHandler(qpid::types::Variant::Map command)
{
  qpid::types::Variant::Map returnval;
  std::string cmd = "";
  std::string internalid = "";

  if(DEBUG)
    std::cout << "CommandHandler" << command << std::endl;

  if(command.count("internalid")==1 && command.count("command")==1)
    {
      //get values
      cmd = command["command"].asString();
      internalid = command["internalid"].asString();

      returnval["error"] = 1;
      returnval["msg"] = "Not implemented";
    }

  return returnval;
}

/**
 * main
 */
int main(int argc, char **argv)
{
  //get command line parameters
  bool continu = true;
  do
    {
      switch(getopt(argc,argv,"dh"))
        {
        case 'd':
          //activate debug
          DEBUG = 1;
          std::cout << "DEBUG activated" << std::endl;
          break;
        case 'h':
          //usage
          std::cout << "Usage: agoOregon [-dgh]" << std::endl;
          std::cout << "Options:" << std::endl;
          std::cout << " -d: display debug message" << std::endl;
          std::cout << " -h: this help" << std::endl;
          exit(0);
          break;
        default:
          continu = false;
          break;
        }
    }
  while(continu);

  //init agocontrol client
  std::cout << "Initializing oregon controller" << std::endl;
  agoConnection = new agocontrol::AgoConnection("oregon");
  agoConnection->addDevice("oregoncontroller", "oregoncontroller");
  agoConnection->addHandler(commandHandler);

  oregon_init();

  //run client
  std::cout << "Running oregon controller..." << std::endl;
  agoConnection->run();
}
