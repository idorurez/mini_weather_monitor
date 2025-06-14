#define AA_FONT_10 "kozgoprobold10"
#define AA_FONT_12 "kozgoprobold12"
#define AA_FONT_14 "kozgoprobold14"
#define AA_FONT_15 "kozgoprobold15"
#define AA_FONT_26 "kozgoprobold26"
#define AA_FONT_30 "kozgoprobold30"
#define AA_FONT_32 "kozgoprobold32"
#define AA_FONT_35 "kozgoprobold35"
#define AA_FONT_40 "kozgoprobold40"
#define AA_FONT_48 "kozgoprobold48"
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

    if(!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain time");
        return;
    }

    spr_time.loadFont(AA_FONT_65);

    spr_time.setColorDepth(8);       // just black and white mam
    spr_time.createSprite(270, 160);  // 160 x 100 sprite
    spr_time.fillSprite(TFT_BLACK); // Fill the Sprite with black
    spr_time.setTextColor(TFT_ORANGE);
    spr_time.loadFont(AA_FONT_65);
    spr_time.setCursor(5, 0);
    spr_time.println(&timeinfo, "%H:%M:%S"); // print time
    spr_time.setTextColor(TFT_WHITE);
    spr_time.setCursor(5, 58);
    spr_time.loadFont(AA_FONT_55);
    

    char buffer[20];
    strftime(buffer, sizeof(buffer), "%A", &timeinfo);
    String dayOfWeek = String(buffer);
    int16_t w = tft.textWidth(dayOfWeek);
    if (w >= 485) { // Wednesday is the culprit
        spr_time.loadFont(AA_FONT_48);
    }
    spr_time.println(&timeinfo, "%A");   // day of the week
    spr_time.setCursor(5, 115);
    spr_time.loadFont(AA_FONT_45);
    spr_time.println(&timeinfo, "%B %d"); // month and date
    spr_time.pushSprite(x, y);
    spr_time.deleteSprite();
}

void displayIndoorConditions(float te, float pe, float he) {

    float tempF = ((te * 9/5) + 32)  + BME280_TEMP_ADJUST;

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
    drawPressure(pe);
}

void drawPressure(float pressure) {

    float pe = pressure / 100.0;
    spr_pressure.setColorDepth(16);

    tft.setPivot(280, 0);     // pivot set on edge

    // Create the Sprite
    spr_pressure.createSprite(180, 24);
    spr_pressure.setPivot(0, 28);      
    spr_pressure.fillSprite(TFT_BLACK);
    spr_pressure.setTextColor(TFT_SKYBLUE, TFT_BLACK);
    spr_pressure.loadFont(AA_FONT_30, SD);
    spr_pressure.drawString(String(pe) + " hPA", 0, 0); // test font  
    spr_pressure.pushRotated(90);
    spr_pressure.unloadFont();
    spr_pressure.deleteSprite();
}

void drawForecast(ForecastParsed forecast, int x, int y, bool stats_only=false) {
    int offset = 0;
    tft.setTextWrap(false, false);
    tft.fillRect(x, y, TFT_W, TFT_H, TFT_BLACK);

    int textOffset = 0;
    if (!stats_only) {
        String suffix = "_120x120";
        String iconPath = "/" + String(forecast.iconCode) + suffix + ".jpg";
        drawSdJpeg(iconPath.c_str(), x, y);
        textOffset = 170;
    }
    tft.loadFont(AA_FONT_15, SD);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(forecast.dayOfWeek, x+textOffset, y+10);
    tft.drawString(String(forecast.temperatureMin) + "°/" + String(forecast.temperatureMax) + "°", x+textOffset, y+30);
    tft.drawString("UV Index: " + String(forecast.uvIndex), x+textOffset, y+50);
    tft.drawString(String(forecast.windDirectionCardinal) + " " + String(forecast.windSpeed), x+textOffset, y+70);
    tft.drawString(String(forecast.precipChance) + "% " + String(forecast.qpf) + " in", x+textOffset, y+90);
    tft.drawString(String(forecast.wxPhraseShort), x+textOffset, y+110);
    tft.unloadFont();
    
}

void drawLocation() {

    tft.setPivot(280, 180);
    // Create the Sprite
    spr_location.createSprite(120, 28);
    spr_location.setColorDepth(16);
    spr_location.setPivot(0, 28);
    spr_location.fillSprite(TFT_BLACK);
    spr_location.setTextColor(TFT_WHITE);
    spr_location.loadFont(AA_FONT_14, SD);
    location = parseLocation(locationResp);
    spr_location.drawString(String(location.city) + ", " + String(location.state), 0, 0);
    spr_location.pushRotated(90);
    spr_location.unloadFont();
    spr_location.deleteSprite();
}

void drawForecast() {
    today = parseCurrentForecastResp(forecastResp, 0);
    tomorrow = parseExtendedForecastResp(forecastResp, 1);
    drawForecast(today, 0, 350);  
    drawForecast(tomorrow, 260, 350, true);
}