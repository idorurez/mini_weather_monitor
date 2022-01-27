#define AA_FONT_SMALL "kozgoprobold15"
#define AA_FONT_MED_26 "kozgoprobold26"
#define AA_FONT_MED_70 "kozgoprobold70"
#define AA_FONT_100 "kozgoprobold100"
#define AA_FONT_300 "kozgoprobold300"

void displayIndoorConditions(sensors_event_t temp_event, sensors_event_t pressure_event, sensors_event_t humidity_event) {


    if (oriented == LANDSCAPE) {

        tft.fillRect(0, 0, TFT_W/2, (TFT_H/2) - 20, TFT_BLACK);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        
        tft.setCursor(0,0);
        tft.loadFont(AA_FONT_100, SD);
        tft.print((temp_event.temperature * 9/5) + 32, 1);
        tft.println("°");
        
        tft.loadFont(AA_FONT_MED_70, SD);
        tft.setCursor(0,80);
        tft.print(humidity_event.relative_humidity, 1);
        tft.println("%");
    } else {
        tft.fillRect(0, 0, TFT_W, TFT_H/3, TFT_BLACK);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        
        tft.setCursor(0,0);
        tft.loadFont(AA_FONT_100, SD);
        tft.print((temp_event.temperature * 9/5) + 32, 1); 
        tft.println("°");
        
        tft.loadFont(AA_FONT_MED_70, SD);
        tft.setCursor(0,80);
        tft.print(humidity_event.relative_humidity, 1);
        tft.println("%");
    }

    // drawPressure(pressure_event);
}

void drawPressure(sensors_event_t pressure_event) {

    if (oriented == LANDSCAPE) {

        tft.setPivot(225, 0);     // pivot set on edge
        // Create the Sprite
        spr.setColorDepth(1);       // just black and white mam
        spr.createSprite(160, 28);  // 160 x 100 sprite
        spr.setPivot(0, 28);      // bottom left corner of sprite as pivot
        spr.fillSprite(TFT_BLACK); // Fill the Sprite with black

        spr.loadFont(AA_FONT_MED_26, SD);
        spr.drawString(String(pressure_event.pressure) + " hPA", 0, 0); // test font  
        spr.pushRotated(90);
    } else {
        
        tft.setPivot(225, 0);     // pivot set on edge
        // Create the Sprite
        spr.setColorDepth(1);       // just black and white mam
        spr.createSprite(160, 28);  // 160 x 100 sprite
        spr.setPivot(0, 28);      // bottom left corner of sprite as pivot
        spr.fillSprite(TFT_BLACK); // Fill the Sprite with black

        spr.loadFont(AA_FONT_MED_26);
        spr.drawString(String(pressure_event.pressure) + " hPA", 0, 0); // test font  
        spr.pushRotated(90);
    }
}

void drawTodaysForecast(ForecastParsed forecast, int x, int y) {
    // clear area

    tft.fillRect(x, y, TFT_W, TFT_H/2-20, TFT_BLACK);

    int centerOffset = 170;
    
    String suffix = "_120x120";
    if (oriented == PORTRAIT) {
        suffix = "_120x120";
    }

    // String iconPath = "/" + String(forecast.iconCode) + suffix + ".jpg";
    // drawSdJpeg(iconPath.c_str(), x, y);

    // tft.loadFont(AA_FONT_SMALL, SD);
    // tft.setTextDatum(MC_DATUM);
    // tft.setCursor(x+centerOffset, y+10);
    // tft.println(forecast.dayOfWeek);
    // tft.println(String(forecast.temperatureMin) + "°/" + String(forecast.temperatureMax) + "°");
    // tft.println("UV Index: " + String(forecast.uvIndex));
    // tft.println(String(forecast.windDirectionCardinal) + " " + String(forecast.windSpeed));
    // tft.println(String(forecast.precipChance) + "% " + String(forecast.qpf) + " in");
    // tft.println(String(forecast.wxPhraseShort));
}
//     tft.drawString(forecast.dayOfWeek, x+centerOffset, y+10);
//     tft.drawString(String(forecast.temperatureMin) + "°/" + String(forecast.temperatureMax) + "°", x+centerOffset, y+30, 4);
//     tft.drawString("UV Index: " + String(forecast.uvIndex), x+centerOffset, y+50);
//     tft.drawString(String(forecast.windDirectionCardinal) + " " + String(forecast.windSpeed), x+centerOffset, y+70);
//     tft.drawString(String(forecast.precipChance) + "% " + String(forecast.qpf) + " in", x+centerOffset, y+90);
//     tft.drawString(String(forecast.wxPhraseShort), x+centerOffset, y+110);
// }

// draws a single block for forecast 
void drawForecast(ForecastParsed forecast, int x, int y) {
    
    // clear area
    tft.fillRect(x, y, x+120, TFT_H, TFT_BLACK);

    int centerOffset = 60;

    String suffix = "_120x120";
    if (oriented == PORTRAIT) {
        suffix = "_120x120";
    }

    String iconPath = "/" + String(forecast.iconCode) + suffix + ".jpg";
    drawJpeg(iconPath.c_str(), x, y);

    tft.loadFont(AA_FONT_SMALL);
    
    tft.setTextDatum(MC_DATUM);
    tft.drawString(forecast.dayOfWeek, x+centerOffset, y+128);
    tft.drawString(String(forecast.temperatureMin) + "°/" + String(forecast.temperatureMax) + "°", x+centerOffset, y+143, 4);
    tft.drawString(String(forecast.precipChance) + "% " + String(forecast.qpf) + " in", x+centerOffset, y+158);
    tft.drawString(String(forecast.wxPhraseShort), x+centerOffset, y+173);
}

void drawLocation(LocationParsed location, int x, int y) {
    tft.loadFont(AA_FONT_SMALL);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(String(location.city) + "," + String(location.state), x, y);
}

void drawAllForecast() {
  String forecastResp = getForecast(FIVEDAY); 
  String locationResp = getLocation();
  if (oriented == LANDSCAPE) {
    for (int i=1; i<5; i++) {
        drawForecast(parseExtendedForecastResp(forecastResp, i), (TFT_W/4) * (i-1), (TFT_H/2)-20);
    }
    drawTodaysForecast(parseCurrentForecastResp(forecastResp), (TFT_W-215), 0);
    drawLocation(parseLocation(locationResp), 315, (TFT_H/2)-25);
  } else {
    drawTodaysForecast(parseCurrentForecastResp(forecastResp), 25, 165);
    drawLocation(parseLocation(locationResp), 0, 125);
  }
}