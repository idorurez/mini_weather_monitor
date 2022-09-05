// RENAME THIS TO secrets.h

#ifndef SECRETS_H
#define SECRETS_H

static char respBuf[4096];

// Use your own API key by signing up for a free developer account.
// http://www.wunderground.com/weather/api/
#define WU_API_KEY "<your key here>"

// Specify your favorite location one of these ways.
//#define WU_LOCATION "CA/HOLLYWOOD"
// Country and city
//#define WU_LOCATION "Australia/Sydney"

// US ZIP code
//#define WU_LOCATION ""
#define ZIP_LOCATION "<your zip>"

// 5 minutes between update checks. The free developer account has a limit
// on the  number of calls so don't go wild.
#define DELAY_NORMAL    (5*60*1000)
// 20 minute delay between updates after an error
#define DELAY_ERROR     (20*60*1000)
#define WEATHER_HOST "api.weather.com"



//===========================================
//WiFi connection
//===========================================
char ssid[] = ""; // WiFi Router ssid
char pass[] = ""; // WiFi Router password

// char ssid[] = """ // WiFi Router ssid
// char pass[] = ""; // WiFi Router password


#endif