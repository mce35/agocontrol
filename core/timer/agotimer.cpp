#include <stdio.h>
#include <unistd.h>
#include <time.h>

#include <cstdlib>
#include <iostream>

#include <boost/thread.hpp>
#include <boost/asio/deadline_timer.hpp>
#include "boost/date_time/posix_time/posix_time.hpp"
#include "boost/date_time/gregorian/gregorian.hpp"
#include "boost/date_time/local_time_adjustor.hpp"
#include "boost/date_time/c_local_time_adjustor.hpp"



#include "sunrise.h"
#include "agoapp.h"

using namespace qpid::messaging;
using namespace qpid::types;
using namespace agocontrol;
using namespace std;
using std::string;

namespace pt = boost::posix_time;
namespace greg = boost::gregorian;


typedef struct { float lat; float lon;} latlon_struct;

typedef enum {
    UNKNOWN, SUNRISE, SUNSET
} sunstate_t;

typedef struct {
    sunstate_t next_state;
    time_t next_at;
} sunevent_t;


class AgoTimer: public AgoApp {
private:
    std::string agocontroller;

    int sunsetoffset;
    int sunriseoffset;

    latlon_struct latlon;

    boost::asio::deadline_timer periodicTimer;
    void clocktimer(const boost::system::error_code& error) ;

    boost::asio::deadline_timer sunriseTimer;
    sunevent_t next_sunevent();
    void suntimer(const boost::system::error_code& error, sunstate_t new_state) ;

    void setupApp();
    void cleanupApp();
public:
    AGOAPP_CONSTRUCTOR_HEAD(AgoTimer)
        , periodicTimer(ioService())
        , sunriseTimer(ioService())
        {}

};

typedef boost::date_time::c_local_adjustor<pt::ptime> local_adj;
#define TIME_SIMPLE_LOCAL_STRING(pTime) \
    (pt::to_simple_string(local_adj::utc_to_local((pTime))))

#define TIME_T_SIMPLE_LOCAL_STRING(tTime) \
    TIME_SIMPLE_LOCAL_STRING(pt::from_time_t((tTime)))

void AgoTimer::clocktimer(const boost::system::error_code& error) {
    if(error && error != boost::asio::error::not_connected) {
        return;
    }

    // There are boost methods to go from utc->local, but not other way arround
    pt::ptime now = pt::second_clock::universal_time();

    /* The timer subsystem might fire prematurely, i.e. if timer is
     * scheduled for 20:41:00 it was fired 20:40:59.
     * 20:40:59 +1m -59s == 20:41:00..again..
     *
     * Make sure we are sending a nice and round timestamp.
     */
    pt::time_duration t = now.time_of_day();
    if(t.seconds() != 0) {
        if(t.seconds() > 30) {
            now+= pt::minutes(1);
        }
        now-= pt::seconds(t.seconds());
    }

    pt::ptime now_local = local_adj::utc_to_local(now);
    greg::date d = now_local.date();
    t = now_local.time_of_day();
    assert(t.seconds() == 0);
    if(!error) {
        AGO_DEBUG() << "Distributing clock: " << pt::to_simple_string(now_local);

        Variant::Map content;
        content["minute"]=t.minutes();
        content["second"]=t.seconds();
        content["hour"]=t.hours();


        content["month"] = d.month();
        content["day"]= d.day();
        content["year"]= d.year();
        content["weekday"]= d.day_of_week().as_number() == 0 ? 7 : d.day_of_week().as_number()-1;
        content["yday"]=d.day_of_year();

        agoConnection->sendMessage("event.environment.timechanged", content);
    }

    pt::ptime next = now;
    next+= pt::minutes(1);

    AGO_TRACE() << "Waiting for next periodic at "
        << TIME_SIMPLE_LOCAL_STRING(next);

    periodicTimer.expires_at(next);
    periodicTimer.async_wait(boost::bind(&AgoTimer::clocktimer, this, _1));
}


/* Find out the next sunset/sunrise */
sunevent_t AgoTimer::next_sunevent() {
    sunevent_t evt;

    // All sunrise/sunset times are in UTC!
    time_t sunrise, sunset, sunrise_tomorrow, sunset_tomorrow;
    time_t now = time(NULL);
    if (!GetSunriseSunset(sunrise, sunset,
                sunrise_tomorrow, sunset_tomorrow,
                latlon.lat, latlon.lon)) {
        AGO_ERROR() << "Failed to acquire sunrise/sunset data";
        evt.next_state = UNKNOWN;
        return evt;
    }

    AGO_DEBUG() << "Now: "
        << TIME_T_SIMPLE_LOCAL_STRING(now)
        << " Sunrise: "
        << TIME_T_SIMPLE_LOCAL_STRING(sunrise)
        << " ("<< sunriseoffset << "s offset)"
        << " Sunset: "
        << TIME_T_SIMPLE_LOCAL_STRING(sunset)
        << " ("<< sunsetoffset << "s offset)"

        << " SunriseT: "
        << TIME_T_SIMPLE_LOCAL_STRING(sunrise_tomorrow)
        << " SunsetT: "
        << TIME_T_SIMPLE_LOCAL_STRING(sunset_tomorrow);

    if (now < (sunrise + sunriseoffset)) {
        evt.next_state = SUNRISE;
        evt.next_at = sunrise + sunriseoffset;
    } else if (now > (sunset + sunsetoffset)) {
        if (now < (sunrise_tomorrow+sunriseoffset)) {
            evt.next_state = SUNRISE;
            evt.next_at = sunrise_tomorrow + sunriseoffset;
        }else{
            AGO_ERROR() << "illegal sunrise_tomorrow, has already occured ("
                << TIME_T_SIMPLE_LOCAL_STRING(sunrise_tomorrow) << "+"
                << sunriseoffset << "s, now = "
                << TIME_T_SIMPLE_LOCAL_STRING(now);

            evt.next_state = UNKNOWN;
        }
    } else {
        evt.next_state = SUNSET;
        evt.next_at = sunset - sunsetoffset;
    }
    return evt;
}

/* Act on sun changes.
 * Initially called with new_state == unknown.
 * This will update variable isDaytime, then we schedule
 * timer to kick us alive again at next change at which time we send
 * the updated value.
 */
void AgoTimer::suntimer(const boost::system::error_code& error, sunstate_t new_state) {
    if(error) {
        // Aborted?
        return;
    }

    AGO_TRACE() << "suntimer triggered with new_state=" << new_state;

    sunevent_t sun = next_sunevent();
    if(sun.next_state == UNKNOWN)
    {
        // Retry in 60s
        AGO_DEBUG() << "Retrying in 60s";
        sunriseTimer.expires_from_now(pt::seconds(60));
        sunriseTimer.async_wait(boost::bind(&AgoTimer::suntimer, this, _1, UNKNOWN));
        return;
    }

    Variant::Map empty;

    // first, update variable with current state
    // Boolean, isDaytime = true if sun is about to set
    Variant isDaytime = (sun.next_state == SUNSET);

    AGO_TRACE() << "Setting isDaytime = " << isDaytime;
    agoConnection->setGlobalVariable("isDaytime", isDaytime);

    if(new_state == SUNRISE) {
        agoConnection->sendMessage("event.environment.sunrise", empty);
    }
    else if(new_state == SUNSET) {
        agoConnection->sendMessage("event.environment.sunset", empty);
    }

    pt::ptime next_at = pt::from_time_t(sun.next_at);
    AGO_DEBUG() << "Next sun-event is "
        << ((sun.next_state == SUNSET) ? "sunset":"sunrise")
        << " at " <<
        pt::to_simple_string(local_adj::utc_to_local(next_at));

    sunriseTimer.expires_at(next_at);
    sunriseTimer.async_wait(boost::bind(&AgoTimer::suntimer, this, _1, sun.next_state));
}

void AgoTimer::setupApp() {
    agocontroller = agoConnection->getAgocontroller();

    latlon.lat=atof(getConfigOption("system", "lat", "47.07").c_str());
    latlon.lon=atof(getConfigOption("system", "lon", "15.42").c_str());
    sunriseoffset=atoi(getConfigOption("system", "sunriseoffset", "0").c_str());
    sunsetoffset=atoi(getConfigOption("system", "sunsetoffset", "0").c_str());

    AGO_DEBUG() << "agotimer configured with location "
        << latlon.lat << ", " << latlon.lon
        << " and sunrise offset " << sunriseoffset
        << ", sunset offset " << sunsetoffset;

    // Start IO service in separate thread
    clocktimer(boost::asio::error::not_connected);
    suntimer(boost::system::error_code(), UNKNOWN);
}

void AgoTimer::cleanupApp() {
    AGO_TRACE() << "Cancelling timers";
    periodicTimer.cancel();
    sunriseTimer.cancel();
}

AGOAPP_ENTRY_POINT(AgoTimer);
