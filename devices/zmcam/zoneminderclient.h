/*
     Copyright (C) 2014 Jimmy Rentz <rentzjam@gmail.com>

     This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License.
     This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
     of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

     See the GNU General Public License for more details.

     curl helpers from User Galik on http://www.cplusplus.com/user/Galik/ - http://www.cplusplus.com/forum/unices/45878/

*/

#if !defined(ZONEMINDER_CLIENT_H_INCLUDED_)
#define ZONEMINDER_CLIENT_H_INCLUDED_

#include <string>
#include <sstream>

using namespace std;

enum ZM_AUTH_TYPE
{
    ZM_AUTH_NONE = 0,
    ZM_AUTH_PLAIN = 1,
    ZM_AUTH_HASH = 2,
};

class ZoneminderClient
{
public:
    ZoneminderClient();
    ~ZoneminderClient();
    
    bool create(string webProtocol,
               string hostName,
               unsigned int webPort,
               ZM_AUTH_TYPE webAuthType,
               bool webAuthUseLocalAddress,
               string webAuthUserName,
               string webAuthPasword,
               string webAuthHashSecret,
               unsigned int triggerPort);
    void destroy(void);
    
    bool getVideoFrame(int monitorId, ostringstream& outputStream);
    bool setMonitorAlert(int monitorId,
                         unsigned int durationSeconds,
                         int score,
                         string causeText,
                         string eventText,
                         string showText);
    bool clearMonitorAlert(int monitorId);
    
private:
    bool m_initialized;
    
    string m_hostName;
    
    unsigned int m_webPort;
    string m_webUrlBase;
    ZM_AUTH_TYPE m_webAuthType;
    string m_webAuthBase;
    string m_webAuthLocalAddress;
    bool m_webAuthUseLocalAddress;
    
    unsigned int m_triggerPort;
    
    void buildWebUrlBaseString(string webProtocol,
                               string hostName,
                               unsigned int webPort);
    string generateMySqlPassword(string password);
    bool getLocalWebSocketAddress(void);
    string buildWebAuthString(void);
    
    bool sendTriggerCommand(string command);
};


#endif // ZONEMINDER_CLIENT_H_INCLUDED_

