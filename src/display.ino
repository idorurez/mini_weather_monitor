#define AA_FONT_14 "kozgoprobold14"
#define AA_FONT_15 "kozgoprobold15"
#define AA_FONT_26 "kozgoprobold26"
#define AA_FONT_30 "kozgoprobold30"
#define AA_FONT_32 "kozgoprobold32"
#define AA_FONT_35 "kozgoprobold35"
#define AA_FONT_40 "kozgoprobold40"
#define AA_FONT_45 "kozgoprobold45"
#define AA_FONT_50 "kozgoprobold50"
#define AA_FONT_55 "kozgoprobold55"
#define AA_FONT_60 "kozgoprobold60"
#define AA_FONT_65 "kozgoprobold65"
#define AA_FONT_70 "kozgoprobold70"
#define AA_FONT_80 "kozgoprobold80"
#define AA_FONT_90 "kozgoprobold90"
#define AA_FONT_95 "kozgoprobold95"
#define AA_FONT_100 "kozgoprobold100"
#define AA_FONT_115 "kozgoprobold115"
#define AA_FONT_125 "kozgoprobold125"
#define AA_FONT_150 "kozgoprobold150"
#define AA_FONT_175 "kozgoprobold175"
#define AA_FONT_225 "kozgoprobold225"
#define AA_FONT_250 "kozgoprobold250"
#define AA_FONT_300 "kozgoprobold300"

void printLocalTime(int x, int y){
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  if (oriented == PORTRAIT) {
    sprTime.loadFont(AA_FONT_65);
    
    sprTime.setColorDepth(8);       // just black and white mam
    sprTime.fillSprite(TFT_BLACK); // Fill the Sprite with black
    sprTime.createSprite(300, 160);  // 160 x 100 sprite
    sprTime.setTextColor(TFT_ORANGE);
    sprTime.loadFont(AA_FONT_65);
    sprTime.setCursor(5, 0);
    sprTime.println(&timeinfo, "%H:%M:%S"); // print time
    sprTime.setTextColor(TFT_WHITE);
    sprTime.setCursor(5, 58);
    sprTime.loadFont(AA_FONT_55);
    sprTime.println(&timeinfo, "%A");   // day of the week
    sprTime.setCursor(5, 115);
    sprTime.loadFont(AA_FONT_45);
    sprTime.println(&timeinfo, "%B %d"); // month and date
    sprTime.pushSprite(x, y);
    sprTime.deleteSprite();
  }
}

void displayIndoorConditions(float te, float pe, float he) {

    float tempF = ((te * 9/5) + 32)  + BME280_TEMP_ADJUST;

    if (oriented == LANDSCAPE) {

        tft.fillRect(0, 0, TFT_W/2, (TFT_H/2) - 20, TFT_BLACK);
        tft.setTextColor(TFT_WHITE);
        
        tft.setCursor(0,0);
        tft.loadFont(AA_FONT_100, SD);
        tft.print(tempF, 1);
        tft.println("°");
        
        tft.loadFont(AA_FONT_70, SD);
        tft.setCursor(0,80);
        tft.print(he, 1);
        tft.println("%");

    } else {

        tft.fillRect(0, 0, TFT_W, 175, TFT_BLACK);
        tft.setTextColor(TFT_WHITE);
        
        tft.setCursor(5,0);
        tft.loadFont(AA_FONT_115, SD);
        tft.print(tempF, 1); 
        tft.println("°");
        
        tft.loadFont(AA_FONT_90, SD);
        tft.setCursor(5,95);
        tft.print(he, 1);
        tft.println("%");
    }

    drawPressure(pe);
}

void drawPressure(float pressure) {

    float pe = pressure / 100.0;

    if (oriented == LANDSCAPE) {

        tft.setPivot(225, 0);     // pivot set on edge
        // Create the Sprite
        spr.setColorDepth(16);       // just black and white mam
        spr.createSprite(160, 28);  // 160 x 100 sprite
        spr.setPivot(0, 28);      // bottom left corner of sprite as pivot
        spr.fillSprite(TFT_BLACK); // Fill the Sprite with black

        spr.loadFont(AA_FONT_26, SD);
        spr.drawString(String(pe) + " hPA", 0, 0); // test font  
        spr.pushRotated(90);
    } else {
        spr.setTextColor(TFT_SKYBLUE, TFT_BLACK);
        tft.setPivot(280, 0);     // pivot set on edge
        // Create the Sprite
        spr.setColorDepth(16);       // just black and white mam
        spr.createSprite(180, 28);  // 160 x 100 sprite
        spr.setPivot(0, 28);      // bottom left corner of sprite as pivot
        spr.fillSprite(TFT_BLACK); // Fill the Sprite with black

        spr.loadFont(AA_FONT_30, SD);
        spr.drawString(String(pe) + " hPA", 0, 0); // test font  
        spr.pushRotated(90);
    }
}

void drawTodaysForecast(ForecastParsed forecast, int x, int y) {

    int offset = 0;
    if (oriented == LANDSCAPE) {

        tft.fillRect(x, y, TFT_W, TFT_H/2-20, TFT_BLACK);

        String suffix = "_120x120";
        int textOffset = 165; //165;

        String iconPath = "/" + String(forecast.iconCode) + suffix + ".jpg";
        drawSdJpeg(iconPath.c_str(), x, y);

        tft.loadFont(AA_FONT_15, SD);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(forecast.dayOfWeek, x+textOffset, y+10);
        tft.drawString(String(forecast.temperatureMin) + "°/" + String(forecast.temperatureMax) + "°", x+textOffset, y+30);
        tft.drawString("UV Index: " + String(forecast.uvIndex), x+textOffset, y+50);
        tft.drawString(String(forecast.windDirectionCardinal) + " " + String(forecast.windSpeed), x+textOffset, y+70);
        tft.drawString(String(forecast.precipChance) + "% " + String(forecast.qpf) + " in", x+textOffset, y+90);
        tft.drawString(String(forecast.wxPhraseShort), x+textOffset, y+110);

    } else {
        
        tft.fillRect(x, y, TFT_W, TFT_H, TFT_BLACK);

        String suffix = "_120x120";
        int textOffset = 170;

        String iconPath = "/" + String(forecast.iconCode) + suffix + ".jpg";
        drawSdJpeg(iconPath.c_str(), x, y);

        tft.loadFont(AA_FONT_15, SD);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(forecast.dayOfWeek, x+textOffset, y+10);
        tft.drawString(String(forecast.temperatureMin) + "°/" + String(forecast.temperatureMax) + "°", x+textOffset, y+30);
        tft.drawString("UV Index: " + String(forecast.uvIndex), x+textOffset, y+50);
        tft.drawString(String(forecast.windDirectionCardinal) + " " + String(forecast.windSpeed), x+textOffset, y+70);
        tft.drawString(String(forecast.precipChance) + "% " + String(forecast.qpf) + " in", x+textOffset, y+90);
        tft.drawString(String(forecast.wxPhraseShort), x+textOffset, y+110);
    }
    // sprTodaysForecastBlock.pushSprite(x+offset, y);
}

// draws a single block for forecast 
void drawForecast(ForecastParsed forecast, int x, int y) {
    
    // clear area
    tft.fillRect(x, y, x+120, TFT_H, TFT_BLACK);

    int centerOffset = 60;

    String suffix = "_120x120";

    String iconPath = "/" + String(forecast.iconCode) + suffix + ".jpg";
    drawJpeg(iconPath.c_str(), x, y);

    tft.loadFont(AA_FONT_14, SD);
    tft.drawString(forecast.dayOfWeek, x+centerOffset, y+128);
    tft.drawString(String(forecast.temperatureMin) + "°/" + String(forecast.temperatureMax) + "°", x+centerOffset, y+143, 4);
    tft.drawString(String(forecast.precipChance) + "% " + String(forecast.qpf) + " in", x+centerOffset, y+158);
    tft.drawString(String(forecast.wxPhraseShort), x+centerOffset, y+173);
}

void drawLocation(LocationParsed location, int x, int y) {
    if (oriented == LANDSCAPE) {
    
        x = 315;
        y = (TFT_H / 2) - 25;
        
        tft.loadFont(AA_FONT_14);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(String(location.city) + "," + String(location.state), x, y);
    } else {
        tft.setPivot(TFT_W - 50, 350);     // pivot set on edge
        // Create the Sprite
        sprLocation.setColorDepth(1);       // just black and white mam
        sprLocation.createSprite(180, 28);  // 160 x 100 sprite
        sprLocation.setPivot(0, 28);      // bottom left corner of sprite as pivot
        sprLocation.fillSprite(TFT_BLACK); // Fill the Sprite with black
        sprLocation.loadFont(AA_FONT_14);
        sprLocation.drawString(String(location.city) + ", " + String(location.state), 0, 0);
        sprLocation.pushRotated(90);
    }
    sprLocation.unloadFont();
}

void drawAllForecast() {

    parsed = parseCurrentForecastResp(forecastResp);

    if (oriented == LANDSCAPE) {
        
        drawTodaysForecast(parsed, (TFT_W-215), 0);
        for (int i=1; i<5; i++) {
            drawForecast(parseExtendedForecastResp(forecastResp, i), (TFT_W/4) * (i-1), (TFT_H/2)-22);
        }
    } else {
        drawTodaysForecast(parsed, 50, 350);
        drawLocation(parseLocation(locationResp), 0, 125);
    }

    // drawLocation(parseLocation(locationResp), 315, (TFT_H/2)-25);
}