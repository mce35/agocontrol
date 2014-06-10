/*
     Copyright (C) 2014 Jimmy Rentz <rentzjam@gmail.com>

     This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License.
     This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
     of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

     See the GNU General Public License for more details.

     curl helpers from User Galik on http://www.cplusplus.com/user/Galik/ - http://www.cplusplus.com/forum/unices/45878/

*/

#include "zoneminderclient.h"

#include <algorithm>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstring>

#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netdb.h>
#include <time.h>

#include <curl/curl.h>

#include "sha1.h"
#include "md5.h"

static std::string uintToString(unsigned int v)
{
    stringstream ss;
    string s;
    ss << v;
    s = ss.str();

    return s;
}

// callback function writes data to a std::ostream
static size_t data_write(void* buf, size_t size, size_t nmemb, void* userp) {
	if(userp) {
		std::ostream& os = *static_cast<std::ostream*>(userp);
		std::streamsize len = size * nmemb;
		if(os.write(static_cast<char*>(buf), len))
			return len;
	}

	return 0;
}

CURLcode curl_read(const std::string& url, std::ostream& os, long timeout = 30) {
	CURLcode code(CURLE_FAILED_INIT);
	CURL* curl = curl_easy_init();

	if(curl)
	{
		if(CURLE_OK == (code = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &data_write))
		&& CURLE_OK == (code = curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L))
		&& CURLE_OK == (code = curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L))
		&& CURLE_OK == (code = curl_easy_setopt(curl, CURLOPT_FILE, &os))
		&& CURLE_OK == (code = curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout))
		&& CURLE_OK == (code = curl_easy_setopt(curl, CURLOPT_URL, url.c_str())))
		{
            code = curl_easy_perform(curl);
		} 
		curl_easy_cleanup(curl);
	}
    else
        cout << "curl_read - curl_easy_init failure." << endl;
	return code;
}

ZoneminderClient::ZoneminderClient():
m_initialized(false),
m_webPort(0),
m_webAuthType(ZM_AUTH_NONE),
m_webAuthUseLocalAddress(false),
m_triggerPort(false)
{
}

ZoneminderClient::~ZoneminderClient()
{
    destroy();
}
    
bool ZoneminderClient::create(string webProtocol,
                              string hostName,
                              unsigned int webPort,
                              ZM_AUTH_TYPE webAuthType,
                              bool webAuthUseLocalAddress,
                              string webAuthUserName,
                              string webAuthPasword,
                              string webAuthHashSecret,
                              unsigned int triggerPort)
{
    m_initialized = false;
    
    if (hostName.empty())
    {
        cout << "initialize - No hostname provided." << endl;
        return false;
    }
    
    m_hostName = hostName;
    
    buildWebUrlBaseString(webProtocol, hostName, webPort);
    
    if (webAuthUserName.empty())
    {
        cout << "initialize - No web username provided." << endl;
        return false;
    }
    
    if (webAuthUseLocalAddress)
    {
        if (!getLocalWebSocketAddress())
        {
            cout << "initialize - Could not connect to server and get local network address." << endl;
            return false;
        }
        
        m_webAuthUseLocalAddress = webAuthUseLocalAddress;
    }
    m_webAuthBase.clear();
    
    m_webAuthType = webAuthType;

    // NOTE: Zoneminder doesn't support special characters passed as username/password.
    // If those exist then change Zoneminder to use hash mode (which is always recommended for security reasons).
    switch (webAuthType)
    {
        case ZM_AUTH_NONE:
            m_webAuthBase = "user=" + webAuthUserName;
            break;
        case ZM_AUTH_PLAIN:
            m_webAuthBase = "user=" + webAuthUserName + "&pass=" + webAuthPasword;
            break;
        case ZM_AUTH_HASH:
            m_webAuthBase = webAuthHashSecret + webAuthUserName + generateMySqlPassword(webAuthPasword) + m_webAuthLocalAddress;
            break;
    }
    
    if (triggerPort)
        m_triggerPort = triggerPort;
    else
        m_triggerPort = 6802;
        
    m_initialized = true;
    
    return true;
}

void ZoneminderClient::destroy(void)
{
    m_initialized = false;
}

bool ZoneminderClient::getVideoFrame(int monitorId, ostringstream& outputStream)
{
    if (!m_initialized)
    {
        cout << "getVideoFrame - Not initialized." << endl;
        return false;
    }
    
    bool result = false;
    outputStream.clear();
    
    CURLcode curlResult;
    string url;
    
    url = m_webUrlBase + uintToString(monitorId) + "&" + buildWebAuthString();
    
    curlResult = curl_read(url, outputStream, 2);
    
    if (curlResult == CURLE_OK) 
        result = true;
    else
        cout << "getVideoFrame - Curl error = " << curlResult << endl;
    
    return result;
}

bool ZoneminderClient::setMonitorAlert(int monitorId,
                                        unsigned int durationSeconds,
                                        int score,
                                        string causeText,
                                        string eventText,
                                        string showText)
{
    if (!m_initialized)
    {
        cout << "setMonitorAlert - Not initialized." << endl;
        return false;
    }
    
    stringstream ss;
    
    ss << monitorId << "|on";
    
    if (durationSeconds)
        ss << "+" << durationSeconds;
       
    if (score > 0)
        ss << "|" << score << "|";
    
    if (!causeText.empty())
        ss << causeText << "|";
        
    if (!eventText.empty())
        ss << eventText << "|";
    
    if (!showText.empty())
        ss << showText << "|";
    
    if (sendTriggerCommand(ss.str()))
        return true;
    else
    {
        cout << "setMonitorAlert - Could not send trigger command to monitor " << monitorId << endl;
        return false;
    }
}

bool ZoneminderClient::clearMonitorAlert(int monitorId)
{
    if (!m_initialized)
    {
        cout << "clearMonitorAlert - Not initialized." << endl;
        return false;
    }
    stringstream ss;
    
    ss << monitorId << "|off";
    
    if (sendTriggerCommand(ss.str()))
        return true;
    else
    {
        cout << "clearMonitorAlert - Could not send trigger command to monitor " << monitorId << endl;
        return false;
    }
}

void ZoneminderClient::buildWebUrlBaseString(string webProtocol,
                                              string hostName,
                                              unsigned int webPort)
{
    m_webUrlBase.clear();
    
    if (!webProtocol.empty())
        m_webUrlBase = webProtocol;
    else
        m_webUrlBase = "http";
        
    m_webUrlBase += "://" + hostName;
    
    if (webPort)
        m_webPort = webPort;
    else
        m_webPort = 80;
    
    m_webUrlBase += ":" + uintToString(webPort);
        
    m_webUrlBase += "/cgi-bin/nph-zms?mode=single&monitor=";
}

string ZoneminderClient::generateMySqlPassword(string password)
{
    string result;
    unsigned char hash[21];
    char hex[43];
    int i;
    
    result.reserve(43);
    
    // mysql PASSWORD() function is generated as follows:
    // 1. 2 sha1 hashes.
    // 2. Convert hex result string to uppercase.
    // 3. Add '*' at front.
    memset(hash, 0, sizeof(hash));
    memset(hex, 0, sizeof(hex));
    
    sha1::calc(password.c_str(), password.length(), hash);
    sha1::calc(hash, 20, hash);
    
    hex[0] = '*';
    
    sha1::toHexString(hash, &hex[1]);
    
    result = hex;
    
    transform(result.begin(), result.end(), result.begin(), ::toupper);
 
    return result;
}

bool ZoneminderClient::getLocalWebSocketAddress(void)
{
    struct addrinfo host_info;
    struct addrinfo *host_info_list;
    struct sockaddr_storage local_address;
    socklen_t slen;
    
    m_webAuthLocalAddress.clear();

	memset(&host_info, 0, sizeof host_info);

	host_info.ai_family = AF_UNSPEC;
	host_info.ai_socktype = SOCK_STREAM;

	int status = getaddrinfo(m_hostName.c_str(), uintToString(m_webPort).c_str(), &host_info, &host_info_list);
	if (status != 0) 
    {
		cout << "getLocalWebSocketAddress - getaddrinfo error: " << gai_strerror(status) << std::endl;
		cout << "getLocalWebSocketAddress - can't get address for " << m_hostName << std::endl;
        return false;
	}

	int socketfd = socket(host_info_list->ai_family, host_info_list->ai_socktype, host_info_list->ai_protocol);
	if (socketfd == -1) 
    {
        int c_errno = errno;
		std::cout << "getLocalWebSocketAddress - socket error (" << c_errno << ") = " << strerror(c_errno)<< std::endl;
        freeaddrinfo(host_info_list);
        return false;
	}
    
	status = connect(socketfd, host_info_list->ai_addr, host_info_list->ai_addrlen);
	if (status == -1) 
    {
        int c_errno = errno;
		std::cout << "getLocalWebSocketAddress - connect error (" << c_errno << ") = " << strerror(c_errno)<< std::endl;
        freeaddrinfo(host_info_list);
        close(socketfd);
        return false;
	}
    
    freeaddrinfo(host_info_list);
    
    slen = sizeof(local_address);
    
    status = getsockname(socketfd, (struct sockaddr*)&local_address, &slen);
    if (status == -1) 
    {
        int c_errno = errno;
		std::cout << "getLocalWebSocketAddress - getsockname error (" << c_errno << ") = " << strerror(c_errno)<< std::endl;
        freeaddrinfo(host_info_list);
        close(socketfd);
        return false;
	}
    
    char buffer[64];
    
    memset(buffer, 0, sizeof(buffer));
    
    if (local_address.ss_family == AF_INET)
        inet_ntop(AF_INET, &(((struct sockaddr_in *)&local_address)->sin_addr), buffer, 63);
    else if (local_address.ss_family == AF_INET6)
        inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)&local_address)->sin6_addr), buffer, 63);
     
    m_webAuthLocalAddress = buffer;
    
    close(socketfd);
    
    return true;
}

string ZoneminderClient::buildWebAuthString(void)
{
    string result;
    
    if (m_webAuthType == ZM_AUTH_HASH)
    {
        time_t rawtime;
        struct tm* timeinfo;
        string webAuthCurrent;

        time(&rawtime);
        timeinfo = localtime(&rawtime);
        
        //$time[2].$time[3].$time[4].$time[5]
        //hour, day of month, month of year, years since 1900
        webAuthCurrent = m_webAuthBase + uintToString(timeinfo->tm_hour) + uintToString(timeinfo->tm_mday);
        webAuthCurrent += uintToString(timeinfo->tm_mon) + uintToString(timeinfo->tm_year);
        
        result = "auth=" + md5(webAuthCurrent);
    }
    else
        result = m_webAuthBase;
            
    return result;
}

bool ZoneminderClient::sendTriggerCommand(string command)
{
    bool result = false;
    struct addrinfo host_info;
    struct addrinfo *host_info_list;

	memset(&host_info, 0, sizeof host_info);

	host_info.ai_family = AF_UNSPEC;
	host_info.ai_socktype = SOCK_STREAM;
    
    //cout << "sendTriggerCommand - trigger " << command << std::endl;

	int status = getaddrinfo(m_hostName.c_str(), uintToString(m_triggerPort).c_str(), &host_info, &host_info_list);
	if (status != 0) 
    {
		cout << "sendTriggerCommand - getaddrinfo error: " << gai_strerror(status) << std::endl;
		cout << "sendTriggerCommand - can't get address for " << m_hostName << std::endl;
        return false;
	}

	int socketfd = socket(host_info_list->ai_family, host_info_list->ai_socktype, host_info_list->ai_protocol);
	if (socketfd == -1) 
    {
        int c_errno = errno;
		std::cout << "sendTriggerCommand - socket error (" << c_errno << ") = " << strerror(c_errno)<< std::endl;
        freeaddrinfo(host_info_list);
        return false;
	}
    
	status = connect(socketfd, host_info_list->ai_addr, host_info_list->ai_addrlen);
	if (status == -1) 
    {
        int c_errno = errno;
		std::cout << "sendTriggerCommand - connect error (" << c_errno << ") = " << strerror(c_errno)<< std::endl;
        freeaddrinfo(host_info_list);
        close(socketfd);
        return false;
	}
    
    unsigned int bytes_sent = send(socketfd, command.c_str(), command.length(), 0);
	if (bytes_sent == command.size()) 
        result = true;
    else
		std::cout << "sendTriggerCommand - error sending data.  Only sent "<< bytes_sent << " bytes" << std::endl;
    
    freeaddrinfo(host_info_list);
    
    shutdown(socketfd, SHUT_RDWR);
    
    close(socketfd);
    
    return result;
}


