
// https://api.weather.com/v2/pws/observations/current?stationId=KMAHANOV10&format=json&units=e&apiKey=yourApiKey

#include <HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>

// const char host_weather[] = "api.weather.com";

// Number of milliseconds to wait without receiving any data before we give up
const int kNetworkTimeout = 30*1000;
// Number of milliseconds to wait if no data is available before trying again
const int kNetworkDelay = 1000;

String httpGETRequest(const char url[]) {

  uint32_t timeout = millis();
  String payload = "{}"; 

  HTTPClient http;

  int resp = 0;
  http.begin(url);
  resp = http.GET();

  Serial.println(url);
  if (resp > 0) {
    Serial.print("Response: ");
    Serial.println(resp);
    payload = http.getString();
    Serial.println(payload);
  } else {
    Serial.println("Error: " + resp);
  }
  http.end();  
  return payload;
}

// mines = KCASANCA12
// closest = KCASANCA59
String getForecast(ForecastReq req) {
  String reqUrl;
  switch (req) {
    case FIVEDAY:
      reqUrl =  "https://api.weather.com/v3/wx/forecast/daily/5day?geocode=" + String(config.latitude) + "," + String(config.longitude) + "&format=json&units=e&language=en-US&apiKey=" + String(config.wu_api_key);
      break;
    default:
      reqUrl =  "https://api.weather.com/v2/pws/dailysummary/7day?stationId=" + String(config.station_id) + "&format=json&units=e&apiKey=" + String(config.wu_api_key);
      break;
  }

  const char *requestUrl = reqUrl.c_str();
  return httpGETRequest(requestUrl);
}

String getLocation() {
    String reqUrl =  "https://api.weather.com/v3/location/point?geocode=" + String(config.latitude) + "," + String(config.longitude) + "&language=en-US&format=json&apiKey=" + String(config.wu_api_key);
    const char *requestUrl = reqUrl.c_str();
    return httpGETRequest(requestUrl);
}

Location parseLocation(String json) {
    const size_t capacity = 12288;
    DynamicJsonDocument doc(capacity);
    DeserializationError error = deserializeJson(doc, json);
    Location location;
    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
    } else {
      location.city = doc["location"]["city"];
      location.state = doc["location"]["adminDistrictCode"];
      location.neighborhood = doc["location"]["neighborhood"];
    }
    return location;
}

ForecastParsed parseCurrentForecastResp(String json) {
  // Use arduinojson.org/v6/assistant to compute the capacity.
  const size_t capacity = 12288; // 12288;
  DynamicJsonDocument doc(capacity);
  // Parse JSON object
  DeserializationError error = deserializeJson(doc, json);
  ForecastParsed parsedInfo;

  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
  } else {

    // assume non-null

    int index = 0;
    String checkNull = doc["daypart"][0]["daypartName"][index];
    if (checkNull == "null") {
      index = 1;
    }

    parsedInfo.dayOfWeek = doc["daypart"][0]["daypartName"][index];
    parsedInfo.iconCode = doc["daypart"][0]["iconCode"][index];

    parsedInfo.temperatureMax = doc["daypart"][0]["temperature"][0];
    parsedInfo.temperatureMin = doc["daypart"][0]["temperature"][1];

    parsedInfo.windDirectionCardinal = doc["daypart"][0]["windDirectionCardinal"][index];
    parsedInfo.wxPhraseShort = doc["daypart"][0]["wxPhraseShort"][index];
    parsedInfo.precipChance = doc["daypart"][0]["precipChance"][index];
    parsedInfo.qpf = doc["daypart"][0]["qpf"][index];
    parsedInfo.uvDescription = doc["daypart"][0]["uvDescription"][index];
    parsedInfo.uvIndex = doc["daypart"][0]["uvIndex"][index];
    parsedInfo.windSpeed = doc["daypart"][0]["windSpeed"][index];
  }
  return parsedInfo;
}

ForecastParsed parseExtendedForecastResp(String json, int forecastDay) {
  // Use arduinojson.org/v6/assistant to compute the capacity.
  const size_t capacity = 14000; // 12288;

  DynamicJsonDocument doc(capacity);
  DeserializationError error = deserializeJson(doc, json);

  ForecastParsed parsedInfo;

  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
  } else {
    parsedInfo.dayOfWeek = doc["dayOfWeek"][forecastDay];
    parsedInfo.temperatureMax = doc["calendarDayTemperatureMax"][forecastDay];
    parsedInfo.temperatureMin = doc["calendarDayTemperatureMin"][forecastDay];
    parsedInfo.windDirectionCardinal = doc["daypart"][0]["windDirectionCardinal"][forecastDay*2];
    parsedInfo.wxPhraseShort = doc["daypart"][0]["wxPhraseShort"][forecastDay*2];
    parsedInfo.precipChance = doc["daypart"][0]["precipChance"][forecastDay*2];
    parsedInfo.qpf = doc["daypart"][0]["qpf"][forecastDay*2];
    parsedInfo.iconCode = doc["daypart"][0]["iconCode"][forecastDay*2];    


  }
  return parsedInfo;
}