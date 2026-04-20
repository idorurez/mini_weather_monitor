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
    logEvent("portal: http.begin failed");
    return true;
  }
  int code = http.GET();
  http.end();
  // Google's canary returns exactly 204 No Content when there's no portal.
  bool behind = (code != 204);
  logEvent("portal: HTTP %d (%s)", code, behind ? "behind portal" : "clear");
  return behind;
}

// Stream a GET into the given doc using a JSON filter to discard fields we
// don't render. Returns detailed outcome so callers can log + render it.
static FetchResult fetchJsonFiltered(const char* url, JsonDocument& doc, JsonDocument& filter) {
  FetchResult r = {false, 0, ""};
  HTTPClient http;
  http.setTimeout(kHttpTimeoutMs);
  http.setReuse(false);

  if (!http.begin(url)) {
    strlcpy(r.detail, "begin failed", sizeof(r.detail));
    return r;
  }

  const char* hdrs[] = {"Content-Type"};
  http.collectHeaders(hdrs, 1);

  r.httpCode = http.GET();
  if (r.httpCode != HTTP_CODE_OK) {
    snprintf(r.detail, sizeof(r.detail), "HTTP %d", r.httpCode);
    http.end();
    return r;
  }

  String ct = http.header("Content-Type");
  if (ct.length() && ct.indexOf("json") < 0) {
    // Trim to fit; usually the content-type string is short anyway.
    snprintf(r.detail, sizeof(r.detail), "ct=%.30s", ct.c_str());
    http.end();
    return r;
  }

  DeserializationError err = deserializeJson(
      doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();

  if (err) {
    snprintf(r.detail, sizeof(r.detail), "JSON: %.25s", err.c_str());
    return r;
  }

  r.ok = true;
  strlcpy(r.detail, "ok", sizeof(r.detail));
  return r;
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
FetchResult refreshForecast() {
  String url = "https://api.weather.com/v3/wx/forecast/daily/5day?geocode=" +
               String(config.latitude) + "," + String(config.longitude) +
               "&format=json&units=e&language=en-US&apiKey=" + String(config.wu_api_key);

  StaticJsonDocument<256> filter;
  buildForecastFilter(filter);

  // Filtered doc: just the strings/numbers for daypart[0..4] + day-level temps.
  // 6KB is comfortable headroom for 5-day filtered output.
  DynamicJsonDocument doc(6144);

  uint32_t before = ESP.getFreeHeap();
  FetchResult r = fetchJsonFiltered(url.c_str(), doc, filter);
  uint32_t after = ESP.getFreeHeap();
  logEvent("forecast: %s (heap %u->%u)", r.detail, before, after);

  if (!r.ok) {
    today.valid = tonight.valid = tomorrow.valid = false;
    return r;
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

  return r;
}

FetchResult refreshLocation() {
  String url = "https://api.weather.com/v3/location/point?geocode=" +
               String(config.latitude) + "," + String(config.longitude) +
               "&language=en-US&format=json&apiKey=" + String(config.wu_api_key);

  StaticJsonDocument<128> filter;
  buildLocationFilter(filter);

  DynamicJsonDocument doc(1024);

  FetchResult r = fetchJsonFiltered(url.c_str(), doc, filter);
  logEvent("location: %s", r.detail);

  if (!r.ok) {
    location.valid = false;
    return r;
  }

  copyStr(doc["location"]["city"], location.city, sizeof(location.city));
  copyStr(doc["location"]["adminDistrictCode"], location.state, sizeof(location.state));
  copyStr(doc["location"]["neighborhood"], location.neighborhood, sizeof(location.neighborhood));
  location.valid = location.city[0] != '\0';
  if (!location.valid) {
    strlcpy(r.detail, "empty city", sizeof(r.detail));
    r.ok = false;
  }
  return r;
}
