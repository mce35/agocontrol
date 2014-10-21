#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>

#include <cstdlib>
#include <iostream>

#include <boost/thread.hpp>

#include "sunrise.h"
#include "agoapp.h"

using namespace qpid::messaging;
using namespace qpid::types;
using namespace agocontrol;
using namespace std;
using std::string;

typedef struct { float lat; float lon;} latlon_struct;

class AgoTimer: public AgoApp {

private:
	std::string agocontroller;

	int sunsetoffset;
	int sunriseoffset;

	latlon_struct latlon;

	boost::thread timerThread;
	boost::thread suntimerThread;

	void timer() ;
	void suntimer() ;

public:
	AGOAPP_CONSTRUCTOR(AgoTimer);
protected:

	void setupApp();
	void cleanupApp();
};



void AgoTimer::timer() {
	time_t now;
	struct tm *tms;
	int waitsec;
	AGO_DEBUG() << "Timer thread started";
	while (!isExitSignaled()) {
		Variant::Map content;
		now = time(NULL);
		tms = localtime(&now);
		waitsec = 60-tms->tm_sec;
		if (waitsec == 60) {
			// just hit the full minute
			//printf("MINUTE %i:%i\n",tms->tm_min,tms->tm_sec);
		} else {
			sleep(waitsec);
			now = time(NULL);
			tms = localtime(&now);
			// printf("MINUTE %i:%i\n",tms->tm_min,tms->tm_sec);
		}
		content["minute"]=tms->tm_min;
		content["second"]=tms->tm_sec;
		content["hour"]=tms->tm_hour;
		content["month"]=tms->tm_mon+1;
		content["day"]=tms->tm_mday;
		content["year"]=tms->tm_year+1900;
		content["weekday"]= tms->tm_wday == 0 ? 7 : tms->tm_wday;
		content["yday"]=tms->tm_yday+1;
		agoConnection->sendMessage("event.environment.timechanged", content);
		sleep(2);
	}
	AGO_DEBUG() << "Timer thread exited";
}

void AgoTimer::suntimer() {
	time_t seconds;
	time_t sunrise, sunset,sunrise_tomorrow,sunset_tomorrow;


	float lat = latlon.lat;
	float lon = latlon.lon;

	AGO_DEBUG() << "Suntimer thread started";
	while(!isExitSignaled()) {
		Variant::Map content;
		Variant::Map setvariable;
		setvariable["uuid"] = agocontroller;
		setvariable["command"] = "setvariable";
		setvariable["variable"] = "isDaytime";
		std::string subject;
		seconds = time(NULL);
		int left;
#define VERIFYINGSLEEP(delay) if((left=sleep((delay))) > 0) { \
	AGO_INFO() << "agotimer sleep aborted, "<<left<<" seconds left. relading loop"; \
	continue;\
 }
		if (GetSunriseSunset(sunrise,sunset,sunrise_tomorrow,sunset_tomorrow,lat,lon)) {
			AGO_DEBUG() << "Now:" << seconds << " Sunrise: " << sunrise << " Sunset: " << sunset << " SunriseT: " << sunrise_tomorrow << " SunsetT: " << sunrise_tomorrow;
			if (seconds < (sunrise + sunriseoffset)) {
				// it is night, we're waiting for the sunrise
				// set global variable
				setvariable["value"] = false;
				agoConnection->sendMessage("", setvariable);
				// now wait for sunrise
				AGO_DEBUG() << "sunrise at: " << asctime(localtime(&sunrise)) << " - minutes to wait for sunrise: " << (sunrise-seconds+sunriseoffset)/60;
				VERIFYINGSLEEP(sunrise-seconds + sunriseoffset);
				AGO_INFO() << "sending sunrise event";
				agoConnection->sendMessage("event.environment.sunrise", content);
			} else if (seconds > (sunset + sunsetoffset)) {
				if (seconds > (sunrise_tomorrow+sunriseoffset)) {
					AGO_TRACE() << "sunrise_tomorrow was calculated wrong, recalculating";
				} else {
					setvariable["value"] = false;
					agoConnection->sendMessage("", setvariable);
					AGO_DEBUG() << "sunrise at: " << asctime(localtime(&sunrise_tomorrow)) <<  " - minutes to wait for sunrise: " << (sunrise_tomorrow-seconds+sunriseoffset)/60;
					VERIFYINGSLEEP(sunrise_tomorrow-seconds + sunriseoffset);
					AGO_INFO() << "sending sunrise event";
					agoConnection->sendMessage("event.environment.sunrise", content);
				}
			} else {
				setvariable["value"] = true;
				agoConnection->sendMessage("", setvariable);
				AGO_DEBUG() << "sunset at: " << asctime(localtime(&sunset)) << " - minutes to wait for sunset: " << (sunset-seconds+sunsetoffset)/60;
				VERIFYINGSLEEP(sunset-seconds+sunsetoffset);
				AGO_INFO() << "sending sunset event";
				agoConnection->sendMessage("event.environment.sunset", content);
			}
			sleep(120);
		} else {
			AGO_FATAL() << "cannot determine sunrise/sunset time";
			sleep(60);
		}
#undef VERIFYINGSLEEP
	}

	AGO_DEBUG() << "Suntimer thread exited";
}

void AgoTimer::setupApp() {
	agocontroller = agoConnection->getAgocontroller();

	latlon.lat=atof(getConfigOption("system", "lat", "47.07").c_str());
	latlon.lon=atof(getConfigOption("system", "lon", "15.42").c_str());
	sunriseoffset=atoi(getConfigOption("system", "sunriseoffset", "0").c_str());
	sunsetoffset=atoi(getConfigOption("system", "sunsetoffset", "0").c_str());

	timerThread = boost::thread(boost::bind(&AgoTimer::suntimer, this));
	suntimerThread = boost::thread(boost::bind(&AgoTimer::timer, this));
}

void AgoTimer::cleanupApp() {
	/*
	 * TODO: implement proper timers...
	AGO_DEBUG() << "Waiting for threads to exit...";
	timerThread.join();
	suntimerThread.join();
	AGO_DEBUG() << "Threads gone";
	*/
}

AGOAPP_ENTRY_POINT(AgoTimer);
