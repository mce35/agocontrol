#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <iostream>
#include <boost/asio.hpp>
#include <boost/system/system_error.hpp>
#include <syslog.h>

#include "agoapp.h"

#ifndef DEVICEMAPFILE
#define DEVICEMAPFILE CONFDIR "/maps/mysensors.json"
#endif
#define RESEND_MAX_ATTEMPTS 30

using namespace agocontrol;
using namespace boost::system;
using namespace qpid::types;

#define MAX_LINE_SIZE 1024

typedef enum {
  ALARM_STATE_ARMED = 0,
  ALARM_STATE_DISARMED = 1,
  ALARM_STATE_ARMED_PRE_TRIGGERED = 2,
  ALARM_STATE_ARMED_TRIGGERED = 3,
} AlarmState;

typedef enum {
  CMD_OK = 0,
  CMD_FAILED,
  CMD_NO_DEV,
  CMD_WRITE_ERR,
  CMD_SELECT_ERR,
  CMD_READ_ERR,
  CMD_TIMEOUT,
} CmdError;

typedef struct ATDevice {
  std::string int_id;
  std::string type;
  time_t last_seen;
  bool stale;
  int time_after_stale;
} ATDevice;

class AgoATAlarm: public AgoApp {
public:
  std::string device;
  int tty_fd;

  pthread_t thread;
  pthread_mutex_t device_mutex;

  char current_line[MAX_LINE_SIZE];
  char *current_line_pos;

  AlarmState cur_state;
  uint8_t last_inputs;
  std::map<std::string, ATDevice> devicemap;

  qpid::types::Variant::Map commandHandler(qpid::types::Variant::Map command);

  void setupApp();
  void cleanupApp();

  static const char *atalarm_state_to_string(AlarmState state);
  static void atalarm_tokenize_res(char *res, int *argc, char *argv[]);
  static const char *atalarm_rf_frame_type_to_str(int type);

  int atalarm_init();
  int atalarm_reopen_device();
  void atalarm_close_device();
  void atalarm_state_changed(AlarmState new_state);
  void atalarm_process_res(int argc, char *argv[]);
  CmdError atalarm_send_command(const char *command, char *res, int res_len);
  CmdError atalarm_gets(char *out, int out_len, int timeout);
  void register_sensor_from_info(char *argv[], int argc);
  void register_rf_sensors();
  void check_staled_device();
  ATDevice *getDeviceInfos(std::string internalid);
  void addDevice(std::string internalid, std::string devicetype, int time_after_stale);
  void add_device_if_not_known(std::string internalid, std::string devicetype,
                               int time_after_stale);
  void *atalarm_thread_real();

public:
  AGOAPP_CONSTRUCTOR_HEAD(AgoATAlarm) {}
};

/**
 * Get infos of specified device
 */
ATDevice *AgoATAlarm::getDeviceInfos(std::string internalid)
{
  std::map<std::string, ATDevice>::iterator it;

  it = this->devicemap.find(internalid);
  if(it == this->devicemap.end())
    return NULL;
  return &(it->second);
  /*qpid::types::Variant::Map out;
  qpid::types::Variant::Map devices = atalarm->devicemap["devices"].asMap();

  if(devices.count(internalid)==1)
    {
      return devices[internalid].asMap();
      }

      return out;*/
}

/**
 * Save all necessary infos for new device and register it to agocontrol
 */
void AgoATAlarm::addDevice(std::string internalid, std::string devicetype, int time_after_stale)
{
  ATDevice dev;
  struct timespec ts;

  clock_gettime(CLOCK_MONOTONIC, &ts);
  dev.int_id = internalid;
  dev.type = devicetype;
  dev.last_seen = ts.tv_sec;
  dev.stale = false;
  dev.time_after_stale = time_after_stale;
  this->devicemap[internalid] = dev;
  /*infos["type"] = devicetype;
  infos["value"] = "0";
  infos["last_seen"] = ts.tv_sec;
  infos["stale"] = false;
  infos["counter_sent"] = 0;
  infos["counter_received"] = 0;
  infos["counter_retries"] = 0;
  infos["counter_failed"] = 0;
  devices[internalid] = infos;
  atalarm->devicemap["devices"] = devices;
  //variantMapToJSONFile(devicemap,DEVICEMAPFILE);*/
  this->agoConnection->addDevice(internalid.c_str(), devicetype.c_str());
}

/**
 * Create new device
 */
void AgoATAlarm::add_device_if_not_known(std::string internalid, std::string devicetype,
                                         int time_after_stale)
{
  ATDevice *dev;

  dev = getDeviceInfos(internalid);
  if(dev != NULL)
    {
      struct timespec ts;

      clock_gettime(CLOCK_MONOTONIC, &ts);
      dev->last_seen = ts.tv_sec;
      if(dev->stale == true)
        {
          dev->stale = false;
          this->agoConnection->resumeDevice(internalid.c_str());
        }
    }
  else
    {
      addDevice(internalid, devicetype, time_after_stale);
     }
  //init
  /*qpid::types::Variant::Map devices = atalarm->devicemap["devices"].asMap();
  qpid::types::Variant::Map infos = getDeviceInfos(atalarm, internalid);

  if(infos.size() > 0)
    {
      struct timespec ts;

      clock_gettime(CLOCK_MONOTONIC, &ts);
      infos["last_seen"] = ts.tv_sec;
    }
  else
    {
      //add new device
      addDevice(atalarm, internalid, devicetype, devices, infos);
      }*/
}

void AgoATAlarm::check_staled_device()
{
  std::map<std::string, ATDevice>::iterator it;
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);

  for(it = this->devicemap.begin(); it != this->devicemap.end(); it++)
    {
      if(it->second.time_after_stale > 0 && ts.tv_sec - it->second.last_seen > it->second.time_after_stale &&
         it->second.stale == false)
        {
          AGO_WARNING() << "Device '" << it->first.c_str() << "' staled!";
          this->agoConnection->suspendDevice(it->first.c_str());
          it->second.stale = true;
        }
    }
}

void AgoATAlarm::atalarm_close_device()
{
  close(this->tty_fd);
  this->tty_fd = -1;
  this->current_line_pos = this->current_line;
}

int AgoATAlarm::atalarm_reopen_device()
{
  struct termios options;

  if(this->tty_fd >= 0)
    atalarm_close_device();

  this->tty_fd = open(this->device.c_str(), O_RDWR | O_NOCTTY);
  if(this->tty_fd < 0)
    {
      AGO_ERROR() << "Failed to open device '" << this->device.c_str() << "': " << strerror(errno);
      return -1;
    }
  tcflush(this->tty_fd, TCIOFLUSH);
  tcgetattr(this->tty_fd, &options);

  options.c_lflag = 0;
  options.c_oflag = 0;

  options.c_iflag = IGNBRK;

  cfsetospeed(&options, B9600);
  cfsetispeed(&options, B9600);
  options.c_cflag |= (CLOCAL | CREAD);

  options.c_iflag &= ~(IXON | IXOFF | IXANY);
  options.c_cflag &= ~CRTSCTS;
  options.c_cflag &= ~PARENB;
  options.c_cflag &= ~CSTOPB;

  options.c_cflag &= ~CSIZE;
  options.c_cflag |= CS8;

  if(tcsetattr(this->tty_fd, TCSANOW, &options) != 0)
    AGO_ERROR() << "tcsetattr failed: " << strerror(errno);

  tcflush(this->tty_fd, TCIOFLUSH);

  AGO_INFO() << "Using device '" << this->device.c_str() << "'";

  return 0;
}

void AgoATAlarm::atalarm_tokenize_res(char *res, int *argc, char *argv[])
{
  char *ptr;

  *argc = 0;
  ptr = res;
  argv[0] = res;
  while((ptr = strchr(ptr, ' ')) != NULL)
    {
      *ptr++ = 0;
      (*argc)++;
      argv[*argc] = ptr;
    }
  (*argc)++;
}

const char *AgoATAlarm::atalarm_state_to_string(AlarmState state)
{
  switch(state)
    {
    default:
    case ALARM_STATE_ARMED:               return "armed"; break;
    case ALARM_STATE_DISARMED:            return "disarmed"; break;
    case ALARM_STATE_ARMED_PRE_TRIGGERED: return "pretriggered"; break;
    case ALARM_STATE_ARMED_TRIGGERED:     return "triggered"; break;
    }
}

void AgoATAlarm::atalarm_state_changed(AlarmState new_state)
{
  AGO_DEBUG() << "Alarm state changed (" << atalarm_state_to_string(this->cur_state) << " -> " <<
    atalarm_state_to_string(new_state) << ")";

  if(new_state == ALARM_STATE_ARMED_TRIGGERED)
    {
      qpid::types::Variant::Map content;

      AGO_WARNING() << "Alarm triggered!";

      content["zone"] = "House";
      this->agoConnection->emitEvent("ATAlarmController", "event.security.intruderalert", content);
    }
  else if(new_state == ALARM_STATE_ARMED_PRE_TRIGGERED)
    AGO_WARNING() << "Alarm pretriggered!";
  else if(new_state == ALARM_STATE_DISARMED)
    AGO_INFO() << "Alarm disarmed";
  else if(new_state == ALARM_STATE_ARMED)
    AGO_INFO() << "Alarm armed";

  this->cur_state = new_state;
  this->agoConnection->emitEvent("ATAlarm", "event.device.statechanged",
                                 this->cur_state == ALARM_STATE_DISARMED ? 0:255, "");
}

const char *AgoATAlarm::atalarm_rf_frame_type_to_str(int type)
{
  switch(type)
    {
    case 0:  return "new_dev_rq"; break;
    case 1:  return "new_dev_resp"; break;
    case 2:  return "key_exch"; break;
    case 3:  return "dev_info_rq"; break;
    case 4:  return "dev_info_resp"; break;
    case 5:  return "sensor_data"; break;
    case 6:  return "ack"; break;
    case 7:  return "ping_rq"; break;
    case 8:  return "ping_resp"; break;
    case 9:  return "wiegand_data"; break;
    case 10: return "sensor_action"; break;
    default: return "unknown"; break;
    }
}

void AgoATAlarm::atalarm_process_res(int argc, char *argv[])
{
  AlarmState new_state;

  if(argc < 2)
    {
      AGO_ERROR() << "Received incomplete line from device: '" << argv[0] << "'";
      return;
    }

  if(strlen(argv[0]) == 0)
    {
      AGO_ERROR() << "Received empty line from device";
      return;
    }

  if(strcmp(argv[1], "keepalive") == 0 && argc >= 2)
    { // Ignore keepalive packets
    }
  else if(strcmp(argv[1], "alarm_state_changed") == 0 && argc >= 3)
    {
      new_state = (AlarmState) strtol(argv[2], NULL, 10);
      if(new_state != this->cur_state)
        atalarm_state_changed(new_state);
    }
  else if(strcmp(argv[1], "alarm_inputs_changed") == 0 && argc >= 4)
    {
      uint8_t inputs;
      char buff[64];
      int i;

      inputs = strtol(argv[2], NULL, 16);
      for(i = 0; i < 6; i++)
        {
          if((this->last_inputs & (0x01 << i)) != (inputs & (0x01 << i)))
            {
              snprintf(buff, sizeof(buff), "base_%i", i);
              if((inputs & (0x01 << i)) != 0)
                {
                  AGO_DEBUG() << "'" << buff << "' changed to 255";
                  this->agoConnection->emitEvent(buff, "event.device.statechanged", 255, "");
                }
              else
                {
                  AGO_DEBUG() << "'" << buff << "' changed to 0";
                  this->agoConnection->emitEvent(buff, "event.device.statechanged", 0, "");
                }
            }
        }
      this->last_inputs = inputs;
    }
  else if(strcmp(argv[1], "wiegand_accepted") == 0 && argc >= 3)
    {
      AGO_INFO() << "Accepted RFID tag '" << argv[2] << "'";
    }
  else if(strcmp(argv[1], "wiegand_unknown") == 0 && argc >= 3)
    {
      AGO_INFO() << "Unknown RFID tag '" << argv[2] << "'";
      //hasd_notification_queue(this->mod.hasd, HASD_NOTIFY_INFO, "hasd_atalarm", notif);
    }
  else if(strcmp(argv[1], "start") == 0 && argc >= 3)
    {
      AGO_WARNING() << "Device (re)started (version " << argv[2] << ")";
      //hasd_notification_queue(this->mod.hasd, HASD_NOTIFY_INFO, "hasd_atalarm", "Device (re)started");
    }
  else if(strcmp(argv[1], "sensor_data") == 0 && argc >= 7)
    {
      char buff[128];
      int16_t value;
            char temp[16];

      // args: 2 -> ID, 3 -> type, 4 -> n, 5 -> value, 6 -> Vbat
      snprintf(buff, sizeof(buff), "rf_%s_%s", argv[2], argv[4]);
      value = atoi(argv[5]);
      switch(atoi(argv[3]))
        {
        case 1 /* NET_DEV_TYPE_TEMP */:
          AGO_DEBUG() << "Sensor '" << argv[2] << "_" << argv[4] << "' temp " << ((double) value) / 10 <<
            "°C bat " << ((double) atoi(argv[6])) / 100 << "V";
          add_device_if_not_known(buff, "temperaturesensor", 300);
          snprintf(temp, sizeof(temp), "%.1f", ((float) value) / 10); // convert value to string for agocontrol
          this->agoConnection->emitEvent(buff, "event.environment.temperaturechanged", temp, "degC");
          break;
        case 4 /* NET_DEV_TYPE_CONTACT */:
          AGO_DEBUG() << "Sensor '" << argv[2] << "_" << argv[4] << "' contact " << value <<
            " bat " << ((double) atoi(argv[6])) / 100 << "V";
          add_device_if_not_known(buff, "binarysensor", 0);
          this->agoConnection->emitEvent(buff, "event.device.statechanged", value == 0 ? 0:255, "");
          break;
        }
      if(atoi(argv[6]) > 0)
        {
          snprintf(buff, sizeof(buff), "rf_%s_bat", argv[2]);
          add_device_if_not_known(buff, "batterysensor", 1200);
          snprintf(temp, sizeof(temp), "%.2f", ((float) atoi(argv[6])) / 100); // convert value to string for agocontrol
          this->agoConnection->emitEvent(buff, "event.device.batterylevelchanged", temp, "V");
        }
    }
  else if(strcmp(argv[1], "sensor_request") == 0 && argc >= 3)
    {
      AGO_INFO() << "Sensor request '" << argv[2];
    }
  else if(strcmp(argv[1], "sensor_paired") == 0 && argc >= 3)
    {
      AGO_INFO() << "Sensor paired '" << argv[2];
    }
  else if(strcmp(argv[1], "rf_net_dbg") == 0 && argc >= 12)
    {
      AGO_WARNING() << "rf_net invalid frame " << argv[2] << " ttl " << argv[3] << " ciphered " << argv[4] <<
        " retry_nb " << argv[5] << " src1 " << argv[6] << " dst1 " << argv[7] << " src2 " << argv[8] <<
        " dst2 " << argv[9] << " seq " << argv[10] << " type " << atalarm_rf_frame_type_to_str(atoi(argv[11]));
    }
  else
    {
      char buff[512];
      char buff2[512];
      int i;

      buff[0] = 0;
      for(i = 0; i < argc; i++)
        {
          snprintf(buff2, sizeof(buff2), "%s %s", buff, argv[i]);
          strcpy(buff, buff2);
        }
      AGO_WARNING() << "Received unsupported/malformed command '" << buff << "'";
    }
}

CmdError AgoATAlarm::atalarm_gets(char *out, int out_len, int timeout)
{
  char *ptr;
  fd_set fdset;
  struct timeval tv;
  char buff[16];
  int n;

  do
    {
      FD_ZERO(&fdset);
      FD_SET(this->tty_fd, &fdset);
      tv.tv_sec = timeout;
      tv.tv_usec = 0;
      n = select(this->tty_fd + 1, &fdset, NULL, NULL, &tv);
      if(n < 0)
        {
          AGO_ERROR() << "select error: " << strerror(errno);
          return CMD_SELECT_ERR;
        }
      if(n == 0)
        {
          if(timeout > 0)
            AGO_ERROR() << "Device read timeout";
          return CMD_TIMEOUT;
        }
      timeout = 1;

      n = read(this->tty_fd, buff, sizeof(buff));
      if(n <= 0)
        {
          AGO_ERROR() << "Device read error: " << strerror(errno);
          return CMD_READ_ERR;
        }
      ptr = buff;
      while(n > 0)
        {
          if(*ptr != '\n')
            {
              *this->current_line_pos++ = *ptr++;
              if(this->current_line_pos >= this->current_line + sizeof(this->current_line))
                this->current_line_pos = this->current_line;
            }
          else if(out_len > 0)
            {
              *this->current_line_pos = 0;
              strncpy(out, this->current_line, out_len);
              out[out_len - 1] = 0;
              out_len = -1;
              this->current_line_pos = this->current_line;
              ptr++;
            }
          n--;
        }
    }
  while(out_len > 0);

  return CMD_OK;
}

CmdError AgoATAlarm::atalarm_send_command(const char *command,
                                          char *res, int res_len)
{
  CmdError err;
  int argc;
  char *argv[256];
  int n;

  *res = 0;

  if(this->tty_fd < 0)
    {
      AGO_ERROR() << "Cannot exec command '" << command << "': No device found";
      return CMD_NO_DEV;
    }

  AGO_DEBUG() << "Exec command '" << command << "'";

  pthread_mutex_lock(&this->device_mutex);
  if(write(this->tty_fd, command, strlen(command)) < 0)
    {
      pthread_mutex_unlock(&this->device_mutex);
      AGO_ERROR() << "Cannot exec command '" << command << "': write error: " << strerror(errno);
      return CMD_WRITE_ERR;
    }
  n = 0;
  do
    {
      err = atalarm_gets(res, res_len, 5);
      if(err != CMD_OK)
        {
          pthread_mutex_unlock(&this->device_mutex);
          return err;
        }
      if(res[0] != '>')
        {
          AGO_DEBUG() << "Received data ('" << res << "') while executing command";
          atalarm_tokenize_res(res, &argc, argv);
          atalarm_process_res(argc, argv);
          n++;
          if(n > 2)
            {
              pthread_mutex_unlock(&this->device_mutex);
              AGO_WARNING() << "Command timeout (" << command << ")";
              return CMD_TIMEOUT;
            }
        }
    }
  while(res[0] != '>');
  pthread_mutex_unlock(&this->device_mutex);

  AGO_DEBUG() << "Command finished cmd='" << command << "', res='" << res << "'";

  if(strncmp(res, ">OK", 3) == 0)
    return CMD_OK;
  return CMD_FAILED;
}

/**
 * Split specified string
 */
/*std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems)
{
  std::stringstream ss(s);
  std::string item;
  while(std::getline(ss, item, delim))
    {
      elems.push_back(item);
    }
  return elems;
}
std::vector<std::string> split(const std::string &s, char delimiter)
{
  std::vector<std::string> elements;
  split(s, delimiter, elements);
  return elements;
  }*/

/**
 * Make readable device infos
 */
/*std::string printDeviceInfos(std::string internalid, qpid::types::Variant::Map infos)
{
  std::stringstream result;
  result << "Infos of device internalid '" << internalid << "'" << endl;
  if( !infos["type"].isVoid() )
    result << " - type=" << infos["type"] << endl;
  if( !infos["value"].isVoid() )
    result << " - value=" << infos["value"] << endl;
  if( !infos["counter_sent"].isVoid() )
    result << " - counter_sent=" << infos["counter_sent"] << endl;
  if( !infos["counter_retries"].isVoid() )
    result << " - counter_retries=" << infos["counter_retries"] << endl;
  if( !infos["counter_received"].isVoid() )
    result << " - counter_received=" << infos["counter_received"] << endl;
  if( !infos["counter_failed"].isVoid() )
    result << " - counter_failed=" << infos["counter_failed"] << endl;
  return result.str();
  }*/

/**
 * Make readable received message from MySensor gateway
 */
/*std::string prettyPrint(std::string message)
{
  int radioId, childId, messageType, subType;
  std::stringstream result;

  std::vector<std::string> items = split(message, ';');
  if (items.size() < 4 || items.size() > 5)
    {
      result << "ERROR, malformed string: " << message << endl;
    }
  else
    {
      std::string payload;
      if (items.size() == 5) payload=items[4];
      result << items[0] << "/" << items[1] << ";" << getMsgTypeName((msgType)atoi(items[2].c_str())) << ";";
      switch (atoi(items[2].c_str()))
        {
        case PRESENTATION:
          result << getDeviceTypeName((deviceTypes)atoi(items[3].c_str()));
          break;
        case SET_VARIABLE:
          result << getVariableTypeName((varTypes)atoi(items[3].c_str()));
          break;
        case REQUEST_VARIABLE:
          result << getVariableTypeName((varTypes)atoi(items[3].c_str()));
          break;
        case VARIABLE_ACK:
          result << getVariableTypeName((varTypes)atoi(items[3].c_str()));
          break;
        case INTERNAL:
          result << getInternalTypeName((internalTypes)atoi(items[3].c_str()));
          break;
        default:
          result << items[3];
        }
      result <<  ";" << payload << std::endl;
    }

  return result.str();
  }*/

/**
 * Save specified device infos
 */
 /*void setDeviceInfos(std::string internalid, qpid::types::Variant::Map* infos)
{
  qpid::types::Variant::Map device = devicemap["devices"].asMap();
  device[internalid] = (*infos);
  devicemap["devices"] = device;
  variantMapToJSONFile(devicemap,DEVICEMAPFILE);
  }*/

/**
 * Delete device
 */
  /*static bool deleteDevice(ATAlarm *atalarm, std::string internalid)
{
  qpid::types::Variant::Map devices = atalarm->devicemap["devices"].asMap();
  bool result = true;

  if(!devices[internalid].isVoid())
    {
      devices.erase(internalid);
      atalarm->devicemap["devices"] = devices;
      agolog(LOG_INFO, "Device '%s' removed", internalid.c_str());
    }
  else
    {
      agolog(LOG_WARNING, "Internalid '%s' not found during device deletion", internalid.c_str());
      result = false;
    }

  return result;
  }*/

/**
 * Agocontrol command handler
 */
qpid::types::Variant::Map AgoATAlarm::commandHandler(qpid::types::Variant::Map command)
{
  qpid::types::Variant::Map returnval;
  qpid::types::Variant::Map infos;
  std::string deviceType = "";
  std::string cmd = "";
  std::string internalid = "";

  std::cout << "CommandHandler" << command << std::endl;

  returnval["error"] = 5;
  returnval["msg"] = "Unsupported command";

  if(command.count("internalid") == 1 && command.count("command") == 1)
    {
      cmd = command["command"].asString();

      if(command["internalid"] == "ATAlarm")
        {
          char res[MAX_LINE_SIZE];

          if(cmd == "on")
            {
              atalarm_send_command("alarm_set_state 0\n", res, sizeof(res));
            }
          else /* if(cmd == "off") */
            {
              atalarm_send_command("alarm_set_state 1\n", res, sizeof(res));
            }
          returnval["error"] = 0;
          returnval["msg"] = "";
        }
      else if(cmd == "getport")
        {
          //get serial port
          returnval["error"] = 0;
          returnval["msg"] = "";
          returnval["port"] = this->device;
        }
      else if(cmd == "setport")
        {
          //set serial port
          if(!command["port"].isVoid() )
            {
              std::string old_device;

              pthread_mutex_lock(&this->device_mutex);
              old_device = this->device;
              this->device = command["port"].asString();
              if(atalarm_reopen_device() != 0)
                {
                  this->device = old_device;
                  atalarm_reopen_device();
                  returnval["error"] = 1;
                  returnval["msg"] = "Unable to open specified port";
                }
              else
                {
                  if(!setConfigOption("atalarm", "device", this->device.c_str()) )
                    {
                      returnval["error"] = 2;
                      returnval["msg"] = "Unable to save serial port to config file";
                    }
                  else
                    {
                      returnval["error"] = 0;
                      returnval["msg"] = "";
                    }
                }
              pthread_mutex_unlock(&this->device_mutex);
            }
          else
            { // port is missing
              returnval["error"] = 3;
              returnval["msg"] = "No port specified";
            }
        }
      else if(cmd == "getwgtags")
        {
          qpid::types::Variant::List tag_list;
          int i;
          char cmd[64];
          char args[256];
          int argc;
          char *argv[256];
          int n;
          int free_tag = -1;

          i = 0;
          do
            {
              snprintf(cmd, sizeof(cmd), "wiegand_get_tag %X\n", i);
              n = atalarm_send_command(cmd, args, sizeof(args));
              atalarm_tokenize_res(args, &argc, argv);
              if(n == CMD_OK && argc == 2)
                {
                  if(strtoul(argv[1], NULL, 16) != 0)
                    {
                      qpid::types::Variant::Map item;
                      item["tag"] = argv[1];
                      item["pos"] = i;
                      tag_list.push_back(item);
                    }
                  else if(free_tag < 0)
                    free_tag = i;
                }
              i++;
            }
          while(n == CMD_OK);

          returnval["error"] = 0;
          returnval["msg"] = "";
          returnval["wgtags"] = tag_list;
          returnval["freetag"] = free_tag;
        }
      else if(cmd == "setwgtag")
        {
          qpid::types::Variant::List tag_list;
          char cmd[64];
          char args[256];
          int argc;
          char *argv[256];

          if(!command["wgtagpos"].isVoid() && !command["wgtag"].isVoid())
            {
              int n;
              uint32_t tag;

              n = strtol(command["wgtagpos"].asString().c_str(), NULL, 10);
              tag = strtol(command["wgtag"].asString().c_str(), NULL, 16);

              snprintf(cmd, sizeof(cmd), "wiegand_set_tag %X %X\n", n, tag);
              n = atalarm_send_command(cmd, args, sizeof(args));
              atalarm_tokenize_res(args, &argc, argv);
              if(n == CMD_OK)
                {
                  returnval["error"] = 0;
                  returnval["msg"] = "";
                }
              else if(n == CMD_FAILED && argc == 2)
                {
                  returnval["error"] = 1;
                  returnval["msg"] = "Failed to set tag";
                }
              else
                {
                  returnval["error"] = 1;
                  returnval["msg"] = "Failed to set tag";
                }
            }
          else
            {
              returnval["error"] = 3;
              returnval["msg"] = "No tag/pos specified";
            }
        }
      else
        {
          char int_id[128];
          uint32_t dev_id;
          int sensor_nb;
          int n;
          char args[256];
          int argc;
          char *argv[256];

          strncpy(int_id, std::string(command["internalid"]).c_str(), sizeof(int_id));
          int_id[sizeof(int_id) - 1] = 0;
          returnval["error"] = 1;
          returnval["msg"] = "Invalid command";
          if(strncmp(int_id, "rf_", 3) == 0 && strchr(int_id + 3, '_') != NULL)
            {
              dev_id = strtoul(int_id + 3, NULL, 16);
              sensor_nb = strtoul(strchr(int_id + 3, '_') + 1, 0, 10);
              printf("cmd '%s' %X %i (%s)\n", cmd.c_str(), dev_id, sensor_nb, int_id);
              if(cmd == "on")
                {
                  char buff[128];

                  snprintf(buff, sizeof(buff), "net_dev_action %X %X 0 1\n", dev_id, sensor_nb);
                  n = atalarm_send_command(buff, args, sizeof(args));
                  atalarm_tokenize_res(args, &argc, argv);
                }
              else if(cmd == "off")
                {
                  char buff[128];

                  snprintf(buff, sizeof(buff), "net_dev_action %X %X 0 0\n", dev_id, sensor_nb);
                  n = atalarm_send_command(buff, args, sizeof(args));
                  atalarm_tokenize_res(args, &argc, argv);
                  if(n == CMD_OK)
                    {
                      returnval["error"] = 0;
                      returnval["msg"] = "";
                    }
                  else if(n == CMD_FAILED && argc == 2)
                    {
                      returnval["error"] = strtoul(argv[1], NULL, 10);
                      switch(strtoul(argv[1], NULL, 10))
                        {
                        case 1: returnval["msg"] = "Invalid args"; break;
                        case 2: returnval["msg"] = "No such device"; break;
                        case 3: returnval["msg"] = "Device error"; break;
                        case 4: returnval["msg"] = "No route to device found"; break;
                        case 5: returnval["msg"] = "Timeout"; break;
                        case 6: returnval["msg"] = "NRF error"; break;
                        default:
                        case 7: returnval["msg"] = "Other error"; break;
                        }
                    }
                  else
                    {
                      returnval["error"] = 1;
                      returnval["msg"] = "Failed to set tag";
                    }
                }
            }
        }
    }
  // if( command.count("internalid")==1 && command.count("command")==1 )
  //   {
  //     //get values
  //     cmd = command["command"].asString();
  //     internalid = command["internalid"].asString();

  //     //switch to specified command
  //     if( cmd=="getcounters" )
  //       {
  //         //return devices counters
  //         returnval["error"] = 0;
  //         returnval["msg"] = "";
  //         qpid::types::Variant::Map counters;
  //         qpid::types::Variant::Map devices = devicemap["devices"].asMap();
  //         for (qpid::types::Variant::Map::const_iterator it = devices.begin(); it != devices.end(); it++)
  //           {
  //             qpid::types::Variant::Map content = it->second.asMap();
  //             content["device"] = it->first.c_str();
  //             counters[it->first.c_str()] = content;
  //           }
  //         returnval["counters"] = counters;
  //       }
  //     else if( cmd=="resetcounters" )
  //       {
  //         //reset all counters
  //         qpid::types::Variant::Map devices = devicemap["devices"].asMap();
  //         for (qpid::types::Variant::Map::iterator it = devices.begin(); it != devices.end(); it++)
  //           {
  //             infos = getDeviceInfos(it->first);
  //             if( infos.size()>0 )
  //               {
  //                 infos["counter_received"] = 0;
  //                 infos["counter_sent"] = 0;
  //                 infos["counter_retries"] = 0;
  //                 infos["counter_failed"] = 0;
  //                 setDeviceInfos(it->first, &infos);
  //               }
  //           }
  //         returnval["error"] = 0;
  //         returnval["msg"] = "";
  //       }
  //     else if( cmd=="getdevices" )
  //       {
  //         //return list of devices
  //         returnval["error"] = 0;
  //         returnval["msg"] = "";
  //         qpid::types::Variant::List devicesList;
  //         qpid::types::Variant::Map devices = devicemap["devices"].asMap();
  //         for (qpid::types::Variant::Map::const_iterator it = devices.begin(); it != devices.end(); it++)
  //           {
  //             infos = getDeviceInfos(it->first);
  //             qpid::types::Variant::Map item;
  //             item["internalid"] = it->first.c_str();
  //             if( infos.size()>0 && !infos["type"].isVoid() )
  //               {
  //                 item["type"] = infos["type"];
  //               }
  //             else
  //               {
  //                 item["type"] = "unknown";
  //               }
  //             devicesList.push_back(item);
  //           }
  //         returnval["devices"] = devicesList;
  //       }
  //     else if( cmd=="remove" )
  //       {
  //         //remove specified device
  //         if( !command["device"].isVoid() )
  //           {
  //             deleteDevice(command["device"].asString());
  //             returnval["error"] = 0;
  //             returnval["msg"] = "";
  //           }
  //         else
  //           {
  //             //device id is missing
  //             returnval["error"] = 4;
  //             returnval["msg"] = "Device is missing";
  //           }
  //       }
  //     else
  //       {
  //         //get device infos
  //         infos = getDeviceInfos(command["internalid"].asString());
  //         //check if device found
  //         if( infos.size()>0 )
  //           {
  //             deviceType = infos["type"].asString();
  //             //switch according to specific device type
  //             if( deviceType=="switch" )
  //               {
  //                 if( cmd=="off" )
  //                   {
  //                     infos["value"] = "0";
  //                     setDeviceInfos(internalid, &infos);
  //                     sendcommand(internalid, SET_VARIABLE, V_LIGHT, "0");
  //                   }
  //                 else if( cmd=="on" )
  //                   {
  //                     infos["value"] = "1";
  //                     setDeviceInfos(internalid, &infos);
  //                     sendcommand(internalid, SET_VARIABLE, V_LIGHT, "1");
  //                   }
  //               }
  //             //TODO add more device type here
  //             returnval["error"] = 0;
  //             returnval["msg"] = "";
  //           }
  //         else
  //           {
  //             //internalid doesn't belong to this controller
  //             returnval["error"] = 5;
  //             returnval["msg"] = "Unmanaged internalid";
  //           }
  //       }
  //   }

  return returnval;
}

/**
 * Resend function (threaded)
 * Allow to send again a command until ack is received (only for certain device type)
 */
// void* resendFunction(void* param) {
//   qpid::types::Variant::Map infos;
//   while(1) {
//     pthread_mutex_lock(&resendMutex);
//     for( std::map<std::string,T_COMMAND>::iterator it=commandsmap.begin(); it!=commandsmap.end(); it++ ) {
//       if( it->second.attemp5Bts<RESEND_MAX_ATTEMPTS ) {
//         //resend command
//         sendcommand(it->second.command);
//         it->second.attempts++;
//         //update counter
//         infos = getDeviceInfos(it->first);
//         if( infos.size()>0 ) {
//           if( infos["counter_retries"].isVoid() ) {
//             infos["counter_retries"] = 1;
//           }
//           else {
//             infos["counter_retries"] = infos["counter_retries"].asUint64()+1;
//           }
//           setDeviceInfos(it->first, &infos);
//         }
//       }
//       else {
//         //max attempts reached, log and delete command from list
//         commandsmap.erase(it->first);

//         //increase counter
//         qpid::types::Variant::Map infos = getDeviceInfos(it->first);
//         if( infos.size()>0 ) {
//           if( infos["counter_failed"].isVoid() ) {
//             infos["counter_failed"] = 1;
//           }
//           else {
//             infos["counter_failed"] = infos["counter_failed"].asUint64()+1;
//           }
//           setDeviceInfos(it->first, &infos);
//         }
//       }
//     }
//     pthread_mutex_unlock(&resendMutex);
//     usleep(500000);
//   }
//   return NULL;
// }

/**
 * Serial read function (threaded)
 */
// void *receiveFunction(void *param) {
//   bool error = false;
//   while (1) {
//     pthread_mutex_lock (&serialMutex);

//     //read line
//     std::string line = readLine(&error);

//     //check errors on serial port
//     if( error ) {
//       //error occured! close port
//       cout << "Reconnecting to serial port" << endl;
//       closeSerialPort();
//       //pause (100ms)
//       usleep(100000);
//       //and reopen it
//       openSerialPort(device);

//       pthread_mutex_unlock (&serialMutex);
//       continue;
//     }

//     if( DEBUG )
//       cout << " => RECEIVING: " << prettyPrint(line);
//     std::vector<std::string> items = split(line, ';');
//     if (items.size() > 3 && items.size() < 6) {
//       int radioId = atoi(items[0].c_str());
//       int childId = atoi(items[1].c_str());
//       string internalid = items[0] + "/" + items[1];
//       int messageType = atoi(items[2].c_str());
//       int subType = atoi(items[3].c_str());
//       int valid = 0;
//       string payload;
//       qpid::types::Variant::Map infos;

//       //get device infos
//       infos = getDeviceInfos(internalid);

//       if (items.size() ==5) payload = items[4];
//       switch (messageType) {
//       case INTERNAL:
//         switch (subType) {
//         case I_BATTERY_LEVEL:
//           break; // TODO: emit battery level event
//         case I_TIME:
//           {
//             stringstream timestamp;
//             timestamp << time(NULL);
//             sendcommand(internalid, INTERNAL, I_TIME, timestamp.str());
//           }
//           break;
//         case I_REQUEST_ID:
//           {
//             //return radio id to sensor
//             stringstream id;
//             int freeid = devicemap["nextid"];
//             //@info radioId - The unique id (1-254) for this sensor. Default 255 (auto mode).
//             if( freeid>=254 ) {
//               cerr << "FATAL: no radioId available!" << endl;
//             }
//             else {
//               devicemap["nextid"]=freeid+1;
//               variantMapToJSONFile(devicemap,DEVICEMAPFILE);
//               id << freeid;
//               sendcommand(internalid, INTERNAL, I_REQUEST_ID, id.str());
//             }
//           }
//           break;
//         case I_PING:
//           sendcommand(internalid, INTERNAL, I_PING_ACK, "");
//           break;
//         case I_UNIT:
//           sendcommand(internalid, INTERNAL, I_UNIT, units);
//           break;
//         }
//         break;
//       case PRESENTATION:
//         cout << "PRESENTATION: " << subType << endl;
//         switch (subType) {
//         case S_DOOR:
//         case S_MOTION:
//           newDevice(internalid, "binarysensor");
//           break;
//         case S_SMOKE:
//           newDevice(internalid, "smokedetector");
//           break;
//         case S_LIGHT:
//         case S_HEATER:
//           newDevice(internalid, "switch");
//           break;
//         case S_DIMMER:
//           newDevice(internalid, "dimmer");
//           break;
//         case S_COVER:
//           newDevice(internalid, "drapes");
//           break;
//         case S_TEMP:
//           newDevice(internalid, "temperaturesensor");
//           break;
//         case S_HUM:
//           newDevice(internalid, "humiditysensor");
//           break;
//         case S_BARO:
//           newDevice(internalid, "barometricsensor");
//           break;
//         case S_WIND:
//           newDevice(internalid, "windsensor");
//           break;
//         case S_RAIN:
//           newDevice(internalid, "rainsensor");
//           break;
//         case S_UV:
//           newDevice(internalid, "uvsensor");
//           break;
//         case S_WEIGHT:
//           newDevice(internalid, "weightsensor");
//           break;
//         case S_POWER:
//           newDevice(internalid, "powermeter");
//           break;
//         case S_DISTANCE:
//           newDevice(internalid, "distancesensor");
//           break;
//         case S_LIGHT_LEVEL:
//           newDevice(internalid, "brightnesssensor");
//           break;
//         case S_LOCK:
//           newDevice(internalid, "lock");
//           break;
//         case S_IR:
//           newDevice(internalid, "infraredblaster");
//           break;
//         case S_WATER:
//           newDevice(internalid, "watermeter");
//           break;
//         }
//         break;
//       case REQUEST_VARIABLE:
//         if( infos.size()>0 ) {
//           //increase counter
//           if( infos["counter_sent"].isVoid() ) {
//             infos["counter_sent"] = 1;
//           }
//           else {
//             infos["counter_sent"] = infos["counter_sent"].asUint64()+1;
//           }
//           setDeviceInfos(internalid, &infos);
//           //send value
//           sendcommand(internalid, SET_VARIABLE, subType, infos["value"]);
//         }
//         else {
//           //device not found
//           //TODO log flood!
//           cerr  << "Device not found: unable to get its value" << endl;
//         }
//         break;
//       case SET_VARIABLE:
//         //remove command from map to avoid sending command again
//         pthread_mutex_lock(&resendMutex);
//         if( commandsmap.count(internalid)!=0 ) {
//           commandsmap.erase(internalid);
//         }
//         pthread_mutex_unlock(&resendMutex);

//         //increase counter
//         if( infos.size()>0 ) {
//           if( infos["counter_received"].isVoid() ) {
//             infos["counter_received"] = 1;
//           }
//           else {
//             infos["counter_received"] = infos["counter_received"].asUint64()+1;
//           }
//           setDeviceInfos(internalid, &infos);
//         }

//         //do something on received event
//         switch (subType) {
//         case V_TEMP:
//           valid = 1;
//           if (units == "M") {
//             agoConnection->emitEvent(internalid.c_str(), "event.environment.temperaturechanged", payload.c_str(), "degC");
//           } else {
//             agoConnection->emitEvent(internalid.c_str(), "event.environment.temperaturechanged", payload.c_str(), "degF");
//           }
//           break;
//         case V_TRIPPED:
//           valid = 1;
//           agoConnection->emitEvent(internalid.c_str(), "event.security.sensortriggered", payload == "1" ? 255 : 0, "");
//           break;
//         case V_HUM:
//           valid = 1;
//           agoConnection->emitEvent(internalid.c_str(), "event.environment.humiditychanged", payload.c_str(), "percent");
//           break;
//         case V_LIGHT:
//           valid = 1;
//           agoConnection->emitEvent(internalid.c_str(), "event.device.statechanged", payload=="1" ? 255 : 0, "");
//           break;
//         case V_DIMMER:
//           valid = 1;
//           agoConnection->emitEvent(internalid.c_str(), "event.device.statechanged", payload.c_str(), "");
//           break;
//         case V_PRESSURE:
//           valid = 1;
//           agoConnection->emitEvent(internalid.c_str(), "event.environment.pressurechanged", payload.c_str(), "mBar");
//           break;
//         case V_FORECAST: break;
//         case V_RAIN: break;
//         case V_RAINRATE: break;
//         case V_WIND: break;
//         case V_GUST: break;
//         case V_DIRECTION: break;
//         case V_UV: break;
//         case V_WEIGHT: break;
//         case V_DISTANCE:
//           valid = 1;
//           if (units == "M") {
//             agoConnection->emitEvent(internalid.c_str(), "event.environment.distancechanged", payload.c_str(), "cm");
//           } else {
//             agoConnection->emitEvent(internalid.c_str(), "event.environment.distancechanged", payload.c_str(), "inch");
//           }
//           break;
//         case V_IMPEDANCE: break;
//         case V_ARMED: break;
//         case V_WATT: break;
//         case V_KWH: break;
//         case V_SCENE_ON: break;
//         case V_SCENE_OFF: break;
//         case V_HEATER: break;
//         case V_HEATER_SW: break;
//         case V_LIGHT_LEVEL:
//           valid = 1;
//           agoConnection->emitEvent(internalid.c_str(), "event.environment.brightnesschanged", payload.c_str(), "lux");
//           break;
//         case V_VAR1: break;
//         case V_VAR2: break;
//         case V_VAR3: break;
//         case V_VAR4: break;
//         case V_VAR5: break;
//         case V_UP: break;
//         case V_DOWN: break;
//         case V_STOP: break;
//         case V_IR_SEND: break;
//         case V_IR_RECEIVE: break;
//         case V_FLOW: break;
//         case V_VOLUME: break;
//         case V_LOCK_STATUS: break;
//         default:
//           break;
//         }

//         if( valid==1 ) {
//           //save current device value
//           infos = getDeviceInfos(internalid);
//           if( infos.size()>0 ) {
//             infos["value"] = payload;
//             setDeviceInfos(internalid, &infos);
//           }
//         }
//         else {
//           //unsupported sensor
//           cerr << "WARN: sensor with subType=" << subType << " not supported yet" << endl;
//         }

//         //send ack
//         sendcommand(internalid, VARIABLE_ACK, subType, payload);
//         break;
//       case VARIABLE_ACK:
//         //TODO useful on controller?
//         cout << "VARIABLE_ACK" << endl;
//         break;
//       default:
//         break;
//       }

//     }
//     pthread_mutex_unlock(&serialMutex);
//   }
//   return NULL;
// }

// args : >OK <devID> <sens1_type> <sens1_nb> <sens2_type> <sens2_nb> <sens3_type> <sens3_nb> <sens4_type> <sens4_nb> <sens5_type> <sens5_nb> <sens6_type> <sens6_nb>
void AgoATAlarm::register_sensor_from_info(char *argv[], int argc)
{
  uint32_t dev_id;
  int sens_type;
  int sens_nb;
  int i;
  int n;
  char sens_name[256];
  const char *type;
  int time_after_stale;

  dev_id = strtol(argv[1], NULL, 10);
  n = 0;
  for(i = 2; i < 14; i += 2)
    {
      sens_type = strtol(argv[i], NULL, 10);
      sens_nb = strtol(argv[i + 1], NULL, 10);
      while(sens_nb > 0)
        {
          type = NULL;
          time_after_stale = 300;
          switch(sens_type)
            {
            case 1: type = "temperaturesensor"; break;
            case 2: type = "humiditysensor"; break;
            case 3: type = "rainsensor"; break;
            case 4: type = "binarysensor"; time_after_stale = 0; break;
            case 5: type = "motionsensor"; time_after_stale = 0; break;
            case 6: type = NULL; time_after_stale = 0; break; // Wiegand
            case 7: type = "watersensor"; break;
            case 8: type = "energysensor"; break;
            case 9: type = "switch"; time_after_stale = 0; break;
            }
          if(type != NULL)
            {
              snprintf(sens_name, sizeof(sens_name), "rf_%X_%i", dev_id, n);
              AGO_INFO() << "Register sensor '" << sens_name << "' type '" << type << "'";
              add_device_if_not_known(sens_name, type, time_after_stale);
            }
          n++;
          sens_nb--;
        }
    }
}

void AgoATAlarm::register_rf_sensors()
{
  int i;
  char cmd[64];
  char args[256];
  int argc;
  char *argv[256];
  int n;

  do
    {
      snprintf(cmd, sizeof(cmd), "net_dev_info nb %i\n", i);
      n = atalarm_send_command(cmd, args, sizeof(args));
      atalarm_tokenize_res(args, &argc, argv);
      if(n == CMD_OK && argc == 14)
        {
          register_sensor_from_info(argv, argc);
        }
      i++;
    }
  while(n == CMD_OK);
  add_device_if_not_known("rf_123456_0", "switch", 0);
}

void *AgoATAlarm::atalarm_thread_real()
{
  char buff[MAX_LINE_SIZE];
  int argc;
  char *argv[256];
  int n;
  int nb_read_err;
  fd_set fdset;
  struct timeval tv;

  this->add_device_if_not_known("ATAlarm", "switch", 0);

  n = this->atalarm_send_command("alarm_get_state", buff, sizeof(buff));
  atalarm_tokenize_res(buff, &argc, argv);
  if(n == CMD_OK && argc == 2)
    {
      this->cur_state = (AlarmState) strtol(argv[1], NULL, 10);
      this->agoConnection->emitEvent("ATAlarm", "event.device.statechanged",
                                      this->cur_state == ALARM_STATE_DISARMED ? 0:255, "");

      AGO_INFO() << "Initial alarm state : " << atalarm_state_to_string(this->cur_state);
      if(this->cur_state == ALARM_STATE_ARMED_TRIGGERED)
        {
          qpid::types::Variant::Map content;

          content["zone"] = "House";
          this->agoConnection->emitEvent(buff, "event.security.intruderalert", content);
          AGO_WARNING() << "Initial alarm triggered!";
          //hasd_notification_queue(this->mod.hasd, HASD_NOTIFY_URGENT,
          //                      "hasd_atalarm",
          //                      "Initial alarm triggered!");
        }
    }
  else
    {
      this->agoConnection->emitEvent("ATAlarm", "event.device.statechanged", 0, "");
      AGO_ERROR() << "Cannot get initial alarm state";
    }

  n = this->atalarm_send_command("alarm_get_inputs", buff, sizeof(buff));
  atalarm_tokenize_res(buff, &argc, argv);
  if(n == CMD_OK && argc == 3)
    {
      uint8_t inputs = strtol(argv[1], NULL, 16);
      char buff[64];
      int i;

      for(i = 0; i < 6; i++)
        {
          snprintf(buff, sizeof(buff), "base_%i", i);
          this->add_device_if_not_known(buff, "binarysensor", 0);
          if((inputs & (0x01 << i)) != 0)
            this->agoConnection->emitEvent(buff, "event.device.statechanged", 255, "");
          else
            this->agoConnection->emitEvent(buff, "event.device.statechanged", 0, "");
        }
      this->last_inputs = inputs;
    }
  else
    {
      this->last_inputs = 0;
      AGO_ERROR() << "Cannot get initial inputs state";
    }

  this->register_rf_sensors();

  //this->agoConnection->emitEvent("base_0", "event.device.statechanged", 0, "");
  //this->agoConnection->emitEvent("base_0", "event.device.statechanged", 255, "");
  //this->agoConnection->emitEvent("base_0", "event.device.statechanged", 0, "");

  nb_read_err = 0;
  while(1)
    {
      FD_ZERO(&fdset);
      FD_SET(this->tty_fd, &fdset);
      tv.tv_sec = 10;
      tv.tv_usec = 0;
      n = select(this->tty_fd + 1, &fdset, NULL, NULL, &tv);
      if(n <= 0)
        {
          if(n < 0)
            AGO_ERROR() << "select error: " << strerror(errno);
          else
            AGO_ERROR() << "select timeout";

          nb_read_err++;
          if(nb_read_err >= 20 || n < 0)
            { // select error or read timeout
              //hasd_notification_queue(this->mod.hasd,
              //                      HASD_NOTIFY_INFO,
              //                      "hasd_atalarm",
              //                      "Device communication error or timeout, reopening");
              nb_read_err = 0;
              do
                {
                  nb_read_err++;
                  AGO_ERROR() << "Too many read errors, reopening device (try " << nb_read_err << ")";
                  pthread_mutex_lock(&this->device_mutex);
                  n = this->atalarm_reopen_device();
                  pthread_mutex_unlock(&this->device_mutex);
                  if(n != 0)
                    {
                      AGO_ERROR() << "Device not found while reopening device!";
                      sleep(5);
                    }
                }
              while(n != 0);
              //if(this->tty_fd < 0)
              //{
              //  agolog(LOG_CRIT, "Too many failures, giving up\n");
                  //hasd_notification_queue(this->mod.hasd,
                  //                      HASD_NOTIFY_URGENT,
                  //                      "hasd_atalarm",
                  //                      "Too many errors, giving up");
              //  break;
              //}
            }
          //hasd_notification_queue(this->mod.hasd,
          //                      HASD_NOTIFY_INFO,
          //                      "hasd_atalarm",
          //                      "Device communication re-established");
          continue;
        }
      this->check_staled_device();

      pthread_mutex_lock(&this->device_mutex);
      n = this->atalarm_gets(buff, sizeof(buff), 0);
      pthread_mutex_unlock(&this->device_mutex);
      if(n == CMD_OK)
        {
          atalarm_tokenize_res(buff, &argc, argv);
          this->atalarm_process_res(argc, argv);

          nb_read_err = 0;
        }
      else if(n != CMD_TIMEOUT)
        nb_read_err++;
    }

  if(this->tty_fd >= 0)
    this->atalarm_close_device();

  return NULL;
}

static void *atalarm_thread(void *param)
{
  AgoATAlarm *alarm = (AgoATAlarm *) param;

  return alarm->atalarm_thread_real();
}

int AgoATAlarm::atalarm_init()
{
  this->cur_state = ALARM_STATE_DISARMED;
  this->tty_fd = -1;
  this->current_line_pos = this->current_line;
  pthread_mutex_init(&this->device_mutex, NULL);

  this->device = getConfigOption("atalarm", "device", "/dev/serial/by-id/usb-4348_5523-if00-port0");

  if(atalarm_reopen_device() != 0)
    {
      AGO_FATAL() << "No device found";
      return -1;
    }

  if(pthread_create(&this->thread, NULL, atalarm_thread, this) < 0)
    {
      AGO_FATAL() << "Unable to create thread: " << strerror(errno);
      return -1;
    }

  return 0;
}

void AgoATAlarm::setupApp()
{
  AGO_INFO() << "Initializing ATAlarm controller";

  addCommandHandler();

  if(atalarm_init() < 0)
    {
      AGO_FATAL() << "Init error";
      throw StartupError();
    }

  AGO_INFO() << "Running ATAlarm controller...";
}

void AgoATAlarm::cleanupApp()
{
  AGO_TRACE() << "ATAlarm exiting...";
}

AGOAPP_ENTRY_POINT(AgoATAlarm);
