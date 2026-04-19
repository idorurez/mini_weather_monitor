#define AA_FONT_14 "kozgoprobold14"
#define AA_FONT_15 "kozgoprobold15"
#define AA_FONT_30 "kozgoprobold30"
#define AA_FONT_35 "kozgoprobold35"
#define AA_FONT_40 "kozgoprobold40"
#define AA_FONT_45 "kozgoprobold45"
#define AA_FONT_48 "kozgoprobold48"
#define AA_FONT_55 "kozgoprobold55"
#define AA_FONT_65 "kozgoprobold65"
#define AA_FONT_90 "kozgoprobold90"
#define AA_FONT_115 "kozgoprobold115"

// Time-area sprite dimensions; the auto-shrink uses width - margin as the budget.
static const int kTimeSpriteW = 270;
static const int kTimeMargin  = 10;  // 5px cursor + 5px right slack

// Try fonts largest-first; load the first one whose rendered width fits.
// Logs the measured width so you can recalibrate without guessing.
static void loadLargestFitting(TFT_eSprite& spr, const char* text, int budgetPx,
                               const char* const* fonts, int n) {
  for (int i = 0; i < n; i++) {
    spr.loadFont(fonts[i]);
    int w = spr.textWidth(text);
    Serial.printf("  fit '%s' @ %s = %dpx (budget %d)\n", text, fonts[i], w, budgetPx);
    if (w <= budgetPx || i == n - 1) {
      return;
    }
  }
}

// Banner band shown under the time area when weather can't be reached.
static const int kBannerY = 320;
static const int kBannerH = 30;

void clearNetBanner() {
  tft.fillRect(0, kBannerY, TFT_W, kBannerH, TFT_BLACK);
}

void drawNetBanner(NetState state) {
  const char* msg = "";
  uint16_t color = TFT_RED;
  switch (state) {
    case NET_WIFI_FAILED:    msg = "WiFi failed"; break;
    case NET_CAPTIVE_PORTAL: msg = "Captive portal"; color = TFT_YELLOW; break;
    case NET_FETCH_FAILED:   msg = "Weather fetch failed"; color = TFT_ORANGE; break;
    default: return;
  }
  tft.fillRect(0, kBannerY, TFT_W, kBannerH, TFT_BLACK);
  tft.loadFont(AA_FONT_15, SD);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(color, TFT_BLACK);
  tft.drawString(msg, TFT_W / 2, kBannerY + kBannerH / 2);
  tft.unloadFont();
}

void printLocalTime(int x, int y) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }

  spr_time.setColorDepth(8);
  spr_time.createSprite(270, 160);
  spr_time.fillSprite(TFT_BLACK);

  spr_time.loadFont(AA_FONT_65);
  spr_time.setTextColor(lux > DARK ? TFT_ORANGE : TFT_WHITE);
  spr_time.setCursor(5, 0);
  spr_time.println(&timeinfo, "%H:%M:%S");

  spr_time.setTextColor(TFT_MAGENTA);
  spr_time.setCursor(5, 58);
  char dayBuf[20];
  strftime(dayBuf, sizeof(dayBuf), "%A", &timeinfo);
  static const char* const dayFonts[] = {AA_FONT_55, AA_FONT_48, AA_FONT_45};
  loadLargestFitting(spr_time, dayBuf, kTimeSpriteW - kTimeMargin, dayFonts, 3);
  spr_time.println(dayBuf);

  spr_time.setTextColor(TFT_WHITE);
  spr_time.setCursor(5, 115);
  char dateBuf[32];
  strftime(dateBuf, sizeof(dateBuf), "%B %d", &timeinfo);
  static const char* const dateFonts[] = {AA_FONT_45, AA_FONT_40, AA_FONT_35, AA_FONT_30};
  loadLargestFitting(spr_time, dateBuf, kTimeSpriteW - kTimeMargin, dateFonts, 4);
  spr_time.println(dateBuf);

  spr_time.pushSprite(x, y);
  spr_time.deleteSprite();
}

void displayIndoorConditions(float te, float pe, float he) {
  float tempF = ((te * 9 / 5) + 32) + BME280_TEMP_ADJUST;

  tft.fillRect(0, 0, TFT_W, 175, TFT_BLACK);
  tft.setTextColor(TFT_WHITE);

  tft.setCursor(5, 0);
  tft.loadFont(AA_FONT_115, SD);
  tft.print(tempF, 1);
  tft.println("°");

  tft.loadFont(AA_FONT_90, SD);
  tft.setCursor(5, 95);
  tft.print(he, 1);
  tft.println("%");
  drawPressure(pe);
}

void drawPressure(float pressure) {
  float pe = pressure / 100.0;

  spr_pressure.setColorDepth(16);
  spr_pressure.createSprite(180, 24);
  tft.setPivot(280, 0);
  spr_pressure.setPivot(0, 28);
  spr_pressure.fillSprite(TFT_BLACK);
  spr_pressure.setTextColor(TFT_SKYBLUE, TFT_BLACK);
  spr_pressure.loadFont(AA_FONT_30, SD);
  spr_pressure.drawString(String(pe) + " hPA", 0, 0);
  spr_pressure.pushRotated(90);
  spr_pressure.unloadFont();
  spr_pressure.deleteSprite();
}

// Render one forecast slot. Pulls from the day daypart if valid, otherwise tonight.
static void drawForecastTodaySummary(const ForecastParsed& day, const ForecastParsed& night,
                                     int x, int y) {
  tft.setTextWrap(false, false);
  tft.fillRect(x, y, TFT_W, TFT_H - y, TFT_BLACK);

  const ForecastParsed& src = day.valid ? day : night;
  if (!src.valid) {
    return;
  }

  String iconPath = "/" + String(src.iconCode) + "_120x120.jpg";
  drawSdJpeg(iconPath.c_str(), x, y);
  int textOffset = 170;

  tft.loadFont(AA_FONT_15, SD);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(src.dayPartName, x + textOffset, y + 10);
  if (day.valid) {
    tft.drawString(String(day.temperature) + "°/" + String(night.temperature) + "°",
                   x + textOffset, y + 30);
  } else {
    tft.drawString(String(src.temperature) + "°", x + textOffset, y + 30);
  }
  tft.drawString("UV Index: " + String(src.uvIndex), x + textOffset, y + 50);
  tft.drawString(String(src.windDirectionCardinal) + " " + String(src.windSpeed),
                 x + textOffset, y + 70);
  tft.drawString(String(src.precipChance) + "% " + String(src.qpf) + " in",
                 x + textOffset, y + 90);
  tft.drawString(src.wxPhraseShort, x + textOffset, y + 110);
  tft.unloadFont();
}

static void drawForecastTomorrow(const ForecastParsed& f, int x, int y) {
  tft.setTextWrap(false, false);
  tft.fillRect(x, y, TFT_W - x, TFT_H - y, TFT_BLACK);
  if (!f.valid) {
    return;
  }

  tft.loadFont(AA_FONT_15, SD);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(f.dayOfWeek, x + 30, y + 10);
  tft.drawString(String(f.temperatureMin) + "°/" + String(f.temperatureMax) + "°",
                 x + 30, y + 30);
  tft.drawString("UV: " + String(f.uvIndex), x + 30, y + 50);
  tft.drawString(String(f.windDirectionCardinal) + " " + String(f.windSpeed),
                 x + 30, y + 70);
  tft.drawString(String(f.precipChance) + "% " + String(f.qpf) + " in",
                 x + 30, y + 90);
  tft.drawString(f.wxPhraseShort, x + 30, y + 110);
  tft.unloadFont();
}

void drawLocation() {
  if (!location.valid) {
    return;
  }
  spr_location.setColorDepth(16);
  spr_location.createSprite(120, 28);
  tft.setPivot(280, 180);
  spr_location.setPivot(0, 28);
  spr_location.fillSprite(TFT_BLACK);
  spr_location.setTextColor(TFT_WHITE);
  spr_location.loadFont(AA_FONT_14, SD);
  spr_location.drawString(String(location.city) + ", " + String(location.state), 0, 0);
  spr_location.pushRotated(90);
  spr_location.unloadFont();
  spr_location.deleteSprite();
}

void drawForecast() {
  drawForecastTodaySummary(today, tonight, 0, 350);
  drawForecastTomorrow(tomorrow, 260, 350);
}
