// Weather Underground / weather.com fetch + parse layer.
// Streams the response body directly into ArduinoJson with a Filter so the
// in-memory doc stays small and we don't hold a giant String in heap.

#include <HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>

static const uint16_t kHttpTimeoutMs = 10000;

// Probe a known captive-portal canary. Returns true if behind a portal
// (or the network blocked the probe), false if the internet is reachable.
bool isBehindCaptivePortal() {
  HTTPClient http;
  http.setTimeout(5000);
  http.setReuse(false);
  if (!http.begin("http://connectivitycheck.gstatic.com/generate_204")) {
    return true;
  }
  int code = http.GET();
  http.end();
  // Google's canary returns exactly 204 No Content when there's no portal.
  return code != 204;
}

// Stream a GET into the given doc using a JSON filter to discard fields we
// don't render. Returns true on success.
static bool fetchJsonFiltered(const char* url, JsonDocument& doc, JsonDocument& filter) {
  HTTPClient http;
  http.setTimeout(kHttpTimeoutMs);
  http.setReuse(false);

  if (!http.begin(url)) {
    Serial.println("http.begin failed");
    return false;
  }

  const char* hdrs[] = {"Content-Type"};
  http.collectHeaders(hdrs, 1);

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("HTTP %d for %s\n", code, url);
    http.end();
    return false;
  }

  String ct = http.header("Content-Type");
  if (ct.length() && ct.indexOf("json") < 0) {
    Serial.printf("Non-JSON content-type: %s — likely captive portal\n", ct.c_str());
    http.end();
    return false;
  }

  DeserializationError err = deserializeJson(
      doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();

  if (err) {
    Serial.printf("deserializeJson failed: %s\n", err.c_str());
    return false;
  }
  return true;
}

// Helper: copy doc[key] string into a fixed buffer; empty string on miss.
static void copyStr(JsonVariantConst v, char* dst, size_t dstSize) {
  const char* s = v.as<const char*>();
  if (s) {
    strlcpy(dst, s, dstSize);
  } else {
    dst[0] = '\0';
  }
}

// Build a filter doc that keeps only the fields we render from the 5-day endpoint.
static void buildForecastFilter(JsonDocument& filter) {
  JsonObject dp = filter["daypart"].createNestedObject();
  dp["daypartName"] = true;
  dp["iconCode"] = true;
  dp["temperature"] = true;
  dp["windDirectionCardinal"] = true;
  dp["wxPhraseShort"] = true;
  dp["precipChance"] = true;
  dp["qpf"] = true;
  dp["uvDescription"] = true;
  dp["uvIndex"] = true;
  dp["windSpeed"] = true;
  filter["dayOfWeek"] = true;
  filter["calendarDayTemperatureMax"] = true;
  filter["calendarDayTemperatureMin"] = true;
}

static void buildLocationFilter(JsonDocument& filter) {
  JsonObject loc = filter.createNestedObject("location");
  loc["city"] = true;
  loc["adminDistrictCode"] = true;
  loc["neighborhood"] = true;
}

// Populate `out` from daypart slot `dayPart` (0=today day, 1=tonight, etc).
// `valid` stays false if the slot is null (forecast has rolled past it).
static void fillFromDaypart(JsonObjectConst dp, int slot, ForecastParsed& out) {
  out.valid = false;
  out.dayPartName[0] = '\0';
  out.windDirectionCardinal[0] = '\0';
  out.wxPhraseShort[0] = '\0';
  out.uvDescription[0] = '\0';
  out.dayOfWeek[0] = '\0';

  JsonVariantConst name = dp["daypartName"][slot];
  if (name.isNull()) {
    return;
  }
  out.valid = true;
  copyStr(name, out.dayPartName, sizeof(out.dayPartName));
  copyStr(dp["windDirectionCardinal"][slot], out.windDirectionCardinal, sizeof(out.windDirectionCardinal));
  copyStr(dp["wxPhraseShort"][slot], out.wxPhraseShort, sizeof(out.wxPhraseShort));
  copyStr(dp["uvDescription"][slot], out.uvDescription, sizeof(out.uvDescription));
  out.iconCode     = dp["iconCode"][slot]     | 0;
  out.temperature  = dp["temperature"][slot]  | 0;
  out.precipChance = dp["precipChance"][slot] | 0;
  out.qpf          = dp["qpf"][slot]          | 0.0f;
  out.uvIndex      = dp["uvIndex"][slot]      | 0;
  out.windSpeed    = dp["windSpeed"][slot]    | 0.0f;
}

// Fetch the 5-day forecast and populate today/tonight/tomorrow in one shot.
// Returns true on full success.
bool refreshForecast() {
  String url = "https://api.weather.com/v3/wx/forecast/daily/5day?geocode=" +
               String(config.latitude) + "," + String(config.longitude) +
               "&format=json&units=e&language=en-US&apiKey=" + String(config.wu_api_key);

  StaticJsonDocument<256> filter;
  buildForecastFilter(filter);

  // Filtered doc: just the strings/numbers for daypart[0..4] + day-level temps.
  // 6KB is comfortable headroom for 5-day filtered output.
  DynamicJsonDocument doc(6144);

  Serial.printf("[heap] before forecast fetch: %u\n", ESP.getFreeHeap());
  bool ok = fetchJsonFiltered(url.c_str(), doc, filter);
  Serial.printf("[heap] after  forecast fetch: %u\n", ESP.getFreeHeap());
  if (!ok) {
    today.valid = tonight.valid = tomorrow.valid = false;
    return false;
  }

  JsonObjectConst dp = doc["daypart"][0];
  fillFromDaypart(dp, 0, today);
  fillFromDaypart(dp, 1, tonight);

  // Tomorrow uses the day-level calendar fields + the day-side daypart slot.
  fillFromDaypart(dp, 2, tomorrow);
  copyStr(doc["dayOfWeek"][1], tomorrow.dayOfWeek, sizeof(tomorrow.dayOfWeek));
  tomorrow.temperatureMax = doc["calendarDayTemperatureMax"][1] | 0;
  tomorrow.temperatureMin = doc["calendarDayTemperatureMin"][1] | 0;
  tomorrow.valid = tomorrow.dayOfWeek[0] != '\0';

  return true;
}

bool refreshLocation() {
  String url = "https://api.weather.com/v3/location/point?geocode=" +
               String(config.latitude) + "," + String(config.longitude) +
               "&language=en-US&format=json&apiKey=" + String(config.wu_api_key);

  StaticJsonDocument<128> filter;
  buildLocationFilter(filter);

  DynamicJsonDocument doc(1024);

  if (!fetchJsonFiltered(url.c_str(), doc, filter)) {
    location.valid = false;
    return false;
  }

  copyStr(doc["location"]["city"], location.city, sizeof(location.city));
  copyStr(doc["location"]["adminDistrictCode"], location.state, sizeof(location.state));
  copyStr(doc["location"]["neighborhood"], location.neighborhood, sizeof(location.neighborhood));
  location.valid = location.city[0] != '\0';
  return location.valid;
}
