# 1 "C:\\Users\\neuro\\AppData\\Local\\Temp\\tmpfl59swwa"
#include <Arduino.h>
# 1 "C:/Users/neuro/dev/mini_weather_monitor/src/mini_weather.ino"
# 10 "C:/Users/neuro/dev/mini_weather_monitor/src/mini_weather.ino"
#include <TFT_eSPI.h>
#include <SPI.h>
#include "SD.h"
#include "time.h"
#include <esp_task_wdt.h>

#include <Wire.h>
#include <ArduinoJson.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <BH1750.h>
#include <JPEGDecoder.h>
#include <Wifi.h>
#include <SPIFFS.h>

#define LED_BUILTIN 2


#define WDT_TIMEOUT 60


int PWM1_DutyCycle = 0;


char dataOut[256];
volatile bool mpuInterrupt = false;


#define SEALEVELPRESSURE_HPA (1013.25)
#define BME280_TEMP_ADJUST -2

Adafruit_BME280 bme;
Adafruit_Sensor *bme_temp;
Adafruit_Sensor *bme_pressure;
Adafruit_Sensor *bme_humidity;

unsigned long bmeUpdateDelay = 60 * 1000;
unsigned long weatherUpdateDelay = 30 * 60 * 1000;

unsigned long bmeUpdateTime = 0;
unsigned long weatherUpdateTime = 0;
unsigned long currTime;
bool triggerUpdate = false;
String forecastResp, locationResp;


BH1750 lightMeter(0x23);

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr_pressure = TFT_eSprite(&tft);
TFT_eSprite spr_time = TFT_eSprite(&tft);
TFT_eSprite spr_location = TFT_eSprite(&tft);

sensors_event_t temp_event, pressure_event, humidity_event;



const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -28800;
const int daylightOffset_sec = 3600;

struct WeatherConfig {
  char wu_api_key[64];
  char zip[10];
  char station_id[32];
  char hostname[64];
  char ssid[64];
  char pass[64];
  char latitude[16];
  char longitude[16];
};

WeatherConfig config;

struct ForecastParsed {
  const char* dayOfWeek;
  int temperatureMax;
  int temperatureMin;
  int precipChance;
  float qpf;
  float windSpeed;
  const char* windDirectionCardinal;
  const char* wxPhraseShort;
  int iconCode;
  const char* uvDescription;
  int uvIndex;
};

ForecastParsed parsed;

struct Location {
  const char* neighborhood;
  const char* city;
  const char* state;
};

Location location;

enum ForecastReq {
  FIVEDAY,
  CURRENT
};

enum Orientation {
  PORTRAIT = 2,
  LANDSCAPE = 3,
};

enum Orientation oriented = PORTRAIT;

int TFT_W = 320;
int TFT_H = 480;
void setup(void);
void loop();
void GetSetConfig();
void BlinkLED(int num);
void drawSdJpeg(const char *filename, int xpos, int ypos);
void drawJpeg(const char *filename, int xpos, int ypos);
void jpegRender(int xpos, int ypos);
void jpegInfo();
void showTime(uint32_t msTime);
void printLocalTime(int x, int y);
void displayIndoorConditions(float te, float pe, float he);
void drawPressure(float pressure);
void drawTodaysForecast(ForecastParsed forecast, int x, int y);
void drawLocation();
void drawForecast();
String httpGETRequest(const char url[]);
String getForecast(ForecastReq req);
String getLocation();
Location parseLocation(String json);
ForecastParsed parseCurrentForecastResp(String json);
ForecastParsed parseExtendedForecastResp(String json, int forecastDay);
void wifiConnect(void);
void WiFiEvent(WiFiEvent_t event);
#line 123 "C:/Users/neuro/dev/mini_weather_monitor/src/mini_weather.ino"
void setup(void) {



  Serial.begin(115200);
  BlinkLED(1);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  digitalWrite(TFT_CS, HIGH);
  digitalWrite(TFT_BL, HIGH);
  digitalWrite(SD_PIN, HIGH);

  tft.init();
  tft.fillScreen(TFT_BLACK);

  if (!SD.begin(SD_PIN)) {
    Serial.println("Card Mount Failed");
    BlinkLED(2);
    }

  Wire.end();
  Wire.begin();
  Wire.setClock(100000);

  ledcAttachPin(TFT_BL, PWM1_CH);
  ledcSetup(PWM1_CH, PWM1_FREQ, PWM1_RES);
  ledcWrite(PWM1_CH, PWM1_DutyCycle);

  currTime = millis();

  uint8_t cardType = SD.cardType();

  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");

  }

  Serial.print("SD Card Type: ");
  if (cardType == CARD_MMC) {
    Serial.println("MMC");
  } else if (cardType == CARD_SD) {
    Serial.println("SDSC");
  } else if (cardType == CARD_SDHC) {
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);
  Serial.println("initialisation done.");
  GetSetConfig();

  if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
    Serial.println(F("BH1750 working in CONTINUOUS HIGH RES MODE"));
  } else {
    Serial.println(F("Error initialising BH1750"));
  }

  unsigned bme_status;
  bme_status = bme.begin(0x76);



  bme.setSampling(Adafruit_BME280::MODE_FORCED,
    Adafruit_BME280::SAMPLING_X1,
    Adafruit_BME280::SAMPLING_X1,
    Adafruit_BME280::SAMPLING_X1,
    Adafruit_BME280::FILTER_OFF );

  bme_temp = bme.getTemperatureSensor();
  bme_pressure = bme.getPressureSensor();
  bme_humidity = bme.getHumiditySensor();

  if (!bme_status) {
    Serial.println("Could not find a valid BME280 sensor, check wiring, address, sensor ID!");
    Serial.print("SensorID was: 0x"); Serial.println(bme.sensorID(),16);
    Serial.print("        ID of 0xFF probably means a bad address, a BMP 180 or BMP 085\n");
    Serial.print("   ID of 0x56-0x58 represents a BMP 280,\n");
    Serial.print("        ID of 0x60 represents a BME 280.\n");
    Serial.print("        ID of 0x61 represents a BME 680.\n");
  } else {
    bme_temp->printSensorDetails();
    bme_pressure->printSensorDetails();
    bme_humidity->printSensorDetails();
  }

  bme.takeForcedMeasurement();

  tft.begin();
  tft.invertDisplay(0);
  tft.setRotation(oriented);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE);
  tft.fillScreen(TFT_BLACK);
  tft.println("Loading...");
  BlinkLED(3);
  wifiConnect();
  forecastResp = getForecast(FIVEDAY);
  locationResp = getLocation();

  drawForecast();
  displayIndoorConditions(bme.readTemperature(), bme.readPressure(), bme.readHumidity());
  drawLocation();
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  BlinkLED(4);

}

void loop() {


  bme.takeForcedMeasurement();


  if (lightMeter.measurementReady()) {
    float lux = lightMeter.readLightLevel();
    PWM1_DutyCycle = (0.85 * lux) + 1;
    PWM1_DutyCycle = constrain(PWM1_DutyCycle, 0.001, 90);
    ledcWrite(PWM1_CH, PWM1_DutyCycle);
  }


  currTime = millis();

  if ((currTime - bmeUpdateTime) > bmeUpdateDelay) {
    displayIndoorConditions(bme.readTemperature(), bme.readPressure(), bme.readHumidity());
    bmeUpdateTime = currTime;
  }

  if ((currTime - weatherUpdateTime) > weatherUpdateDelay) {
    wifiConnect();
    forecastResp = getForecast(FIVEDAY);
    drawForecast();
    weatherUpdateTime = currTime;
    drawLocation();
  }

  printLocalTime(0, 175);
  drawLocation();
}

void GetSetConfig() {
  File configFile = SD.open("/weather.cfg", FILE_READ);
  if (configFile) {
    const size_t capacity = 4096;
    DynamicJsonDocument doc(capacity);
    DeserializationError error = deserializeJson(doc, configFile);
    if (error) {
      Serial.print(F("Failed to read file, using default configuration. Error: "));
      Serial.println(error.f_str());
    } else {
      strlcpy(config.wu_api_key, doc["wu_api_key"] | "", sizeof(config.wu_api_key));
      strlcpy(config.zip, doc["zip"] | "", sizeof(config.zip));
      strlcpy(config.station_id, doc["station_id"] | "", sizeof(config.station_id));
      strlcpy(config.hostname, doc["WIFI"]["hostname"] | "", sizeof(config.hostname));
      strlcpy(config.ssid, doc["WIFI"]["ssid"] | "", sizeof(config.ssid));
      strlcpy(config.pass, doc["WIFI"]["pass"] | "", sizeof(config.pass));
      strlcpy(config.latitude, doc["LOCATION"]["latitude"] | "", sizeof(config.latitude));
      strlcpy(config.longitude, doc["LOCATION"]["longitude"] | "", sizeof(config.longitude));
     }
     configFile.close();
  } else {
    Serial.println(F("Failed to open config file"));
  }
}

void BlinkLED(int num)
{
  int x;

  if (!num)
  {
    num = 1;
  }
  for (x = 0; x < num; x++)
  {

    digitalWrite(LED_BUILTIN, HIGH);
    delay(200);

    digitalWrite(LED_BUILTIN, LOW);
    delay(100);
  }
  delay(1000);
}
# 1 "C:/Users/neuro/dev/mini_weather_monitor/src/JPEG_functions.ino"




void drawSdJpeg(const char *filename, int xpos, int ypos) {


  File jpegFile = SD.open( filename, FILE_READ);

  if ( !jpegFile ) {
    Serial.print("ERROR: File \""); Serial.print(filename); Serial.println ("\" not found!");
    return;
  }

  Serial.println("===========================");
  Serial.print("Drawing file: "); Serial.println(filename);
  Serial.println("===========================");


  bool decoded = JpegDec.decodeSdFile(jpegFile);


  if (decoded) {

    jpegInfo();

    jpegRender(xpos, ypos);
  }
  else {
    Serial.println("Jpeg file format not supported!");
  }
}




void drawJpeg(const char *filename, int xpos, int ypos) {

  Serial.println("===========================");
  Serial.print("Drawing file: "); Serial.println(filename);
  Serial.println("===========================");


  fs::File jpegFile = SPIFFS.open( filename, "r");


  if ( !jpegFile ) {
    Serial.print("ERROR: File \""); Serial.print(filename); Serial.println ("\" not found!");
    return;
  }




  boolean decoded = JpegDec.decodeFsFile(filename);

  if (decoded) {

    jpegInfo();


    jpegRender(xpos, ypos);
  }
  else {
    Serial.println("Jpeg file format not supported!");
  }
}
# 76 "C:/Users/neuro/dev/mini_weather_monitor/src/JPEG_functions.ino"
void jpegRender(int xpos, int ypos) {



  uint16_t *pImg;
  uint16_t mcu_w = JpegDec.MCUWidth;
  uint16_t mcu_h = JpegDec.MCUHeight;
  uint32_t max_x = JpegDec.width;
  uint32_t max_y = JpegDec.height;

  bool swapBytes = tft.getSwapBytes();
  tft.setSwapBytes(true);




  uint32_t min_w = jpg_min(mcu_w, max_x % mcu_w);
  uint32_t min_h = jpg_min(mcu_h, max_y % mcu_h);


  uint32_t win_w = mcu_w;
  uint32_t win_h = mcu_h;


  uint32_t drawTime = millis();



  max_x += xpos;
  max_y += ypos;


  while (JpegDec.read()) {
    pImg = JpegDec.pImage ;


    int mcu_x = JpegDec.MCUx * mcu_w + xpos;
    int mcu_y = JpegDec.MCUy * mcu_h + ypos;


    if (mcu_x + mcu_w <= max_x) win_w = mcu_w;
    else win_w = min_w;


    if (mcu_y + mcu_h <= max_y) win_h = mcu_h;
    else win_h = min_h;


    if (win_w != mcu_w)
    {
      uint16_t *cImg;
      int p = 0;
      cImg = pImg + win_w;
      for (int h = 1; h < win_h; h++)
      {
        p += mcu_w;
        for (int w = 0; w < win_w; w++)
        {
          *cImg = *(pImg + w + p);
          cImg++;
        }
      }
    }


    uint32_t mcu_pixels = win_w * win_h;


    if (( mcu_x + win_w ) <= tft.width() && ( mcu_y + win_h ) <= tft.height())
      tft.pushImage(mcu_x, mcu_y, win_w, win_h, pImg);
    else if ( (mcu_y + win_h) >= tft.height())
      JpegDec.abort();
  }

  tft.setSwapBytes(swapBytes);

  showTime(millis() - drawTime);
}





void jpegInfo() {


  Serial.println("JPEG image info");
  Serial.println("===============");
  Serial.print("Width      :");
  Serial.println(JpegDec.width);
  Serial.print("Height     :");
  Serial.println(JpegDec.height);
  Serial.print("Components :");
  Serial.println(JpegDec.comps);
  Serial.print("MCU / row  :");
  Serial.println(JpegDec.MCUSPerRow);
  Serial.print("MCU / col  :");
  Serial.println(JpegDec.MCUSPerCol);
  Serial.print("Scan type  :");
  Serial.println(JpegDec.scanType);
  Serial.print("MCU width  :");
  Serial.println(JpegDec.MCUWidth);
  Serial.print("MCU height :");
  Serial.println(JpegDec.MCUHeight);
  Serial.println("===============");
  Serial.println("");
}
# 192 "C:/Users/neuro/dev/mini_weather_monitor/src/JPEG_functions.ino"
void showTime(uint32_t msTime) {







  Serial.print(F(" JPEG drawn in "));
  Serial.print(msTime);
  Serial.println(F(" ms "));
}
# 1 "C:/Users/neuro/dev/mini_weather_monitor/src/display.ino"
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

    if(!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain time");
        return;
    }

    spr_time.loadFont(AA_FONT_65);

    spr_time.setColorDepth(8);
    spr_time.createSprite(270, 160);
    spr_time.fillSprite(TFT_BLACK);
    spr_time.setTextColor(TFT_ORANGE);
    spr_time.loadFont(AA_FONT_65);
    spr_time.setCursor(5, 0);
    spr_time.println(&timeinfo, "%H:%M:%S");
    spr_time.setTextColor(TFT_WHITE);
    spr_time.setCursor(5, 58);
    spr_time.loadFont(AA_FONT_55);
    spr_time.println(&timeinfo, "%A");
    spr_time.setCursor(5, 115);
    spr_time.loadFont(AA_FONT_45);
    spr_time.println(&timeinfo, "%B %d");
    spr_time.pushSprite(x, y);
    spr_time.deleteSprite();
}

void displayIndoorConditions(float te, float pe, float he) {

    float tempF = ((te * 9/5) + 32) + BME280_TEMP_ADJUST;

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

    tft.setPivot(280, 0);


    spr_pressure.createSprite(180, 28);
    spr_pressure.setPivot(0, 28);
    spr_pressure.fillSprite(TFT_BLACK);
    spr_pressure.setTextColor(TFT_SKYBLUE, TFT_BLACK);
    spr_pressure.loadFont(AA_FONT_30, SD);
    spr_pressure.drawString(String(pe) + " hPA", 0, 0);
    spr_pressure.pushRotated(90);
    spr_pressure.unloadFont();
    spr_pressure.deleteSprite();
}

void drawTodaysForecast(ForecastParsed forecast, int x, int y) {

    int offset = 0;
    tft.fillRect(x, y, TFT_W, TFT_H, TFT_BLACK);

    String suffix = "_120x120";
    int textOffset = 170;

    String iconPath = "/" + String(forecast.iconCode) + suffix + ".jpg";
    drawSdJpeg(iconPath.c_str(), x, y);

    tft.loadFont(AA_FONT_15, SD);
    tft.setTextDatum(MC_DATUM);
    if (tft.textWidth(String(forecast.dayOfWeek)) > TFT_H) {
        tft.loadFont(AA_FONT_14, SD);
        tft.drawString(forecast.dayOfWeek, x+textOffset, y+10);
        tft.loadFont(AA_FONT_15, SD);
    }

    tft.drawString(String(forecast.temperatureMin) + "°/" + String(forecast.temperatureMax) + "°", x+textOffset, y+30);
    tft.drawString("UV Index: " + String(forecast.uvIndex), x+textOffset, y+50);
    tft.drawString(String(forecast.windDirectionCardinal) + " " + String(forecast.windSpeed), x+textOffset, y+70);
    tft.drawString(String(forecast.precipChance) + "% " + String(forecast.qpf) + " in", x+textOffset, y+90);
    tft.drawString(String(forecast.wxPhraseShort), x+textOffset, y+110);

}

void drawLocation() {

    tft.setPivot(280, 340);

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
    parsed = parseCurrentForecastResp(forecastResp);
    drawTodaysForecast(parsed, 50, 350);
}
# 1 "C:/Users/neuro/dev/mini_weather_monitor/src/forecast.ino"



#include <HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>




const int kNetworkTimeout = 30*1000;

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



String getForecast(ForecastReq req) {
  String reqUrl;
  switch (req) {
    case FIVEDAY:
      reqUrl = "https://api.weather.com/v3/wx/forecast/daily/5day?geocode=" + String(config.latitude) + "," + String(config.longitude) + "&format=json&units=e&language=en-US&apiKey=" + String(config.wu_api_key);
      break;
    default:
      reqUrl = "https://api.weather.com/v2/pws/dailysummary/7day?stationId=" + String(config.station_id) + "&format=json&units=e&apiKey=" + String(config.wu_api_key);
      break;
  }

  const char *requestUrl = reqUrl.c_str();
  return httpGETRequest(requestUrl);
}

String getLocation() {
    String reqUrl = "https://api.weather.com/v3/location/point?geocode=" + String(config.latitude) + "," + String(config.longitude) + "&language=en-US&format=json&apiKey=" + String(config.wu_api_key);
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

  const size_t capacity = 12288;
  DynamicJsonDocument doc(capacity);

  DeserializationError error = deserializeJson(doc, json);
  ForecastParsed parsedInfo;

  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
  } else {



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

  const size_t capacity = 14000;

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
# 1 "C:/Users/neuro/dev/mini_weather_monitor/src/wifi.ino"
void wifiConnect(void)
{
  unsigned long timerDelay = 20000;
  unsigned long lastTime = millis();
  int retries = 0;
  const int maxRetries = 10;
  WiFi.setHostname(config.hostname);
  WiFi.onEvent(WiFiEvent);
  WiFi.mode(WIFI_STA);
  WiFi.begin(config.ssid, config.pass);
  Serial.println("Connecting to " + String(config.ssid));
  Serial.println("Began wifi connection attempt");

  while (WiFi.status() != WL_CONNECTED && retries < maxRetries) {
    if ((millis() - lastTime) > timerDelay) {
      retries++;
      Serial.println("Retrying connection... Attempt " + String(retries));
      WiFi.begin(config.ssid, config.pass);
      lastTime = millis();
    }
    Serial.print('.');
    delay(1000);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Connected");
  } else {
    Serial.println("Failed to connect after " + String(maxRetries) + " attempts");
    ESP.restart();
  }
}


void WiFiEvent(WiFiEvent_t event) {

  switch (event) {
    case SYSTEM_EVENT_WIFI_READY:
      Serial.println("WiFi interface ready");
      break;
    case SYSTEM_EVENT_SCAN_DONE:
      Serial.println("Completed scan for access points");
      break;
    case SYSTEM_EVENT_STA_START:
      Serial.println("WiFi client started");
      break;
    case SYSTEM_EVENT_STA_STOP:
      Serial.println("WiFi clients stopped");
      break;
    case SYSTEM_EVENT_STA_CONNECTED:
      Serial.println("Connected to access point");
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      Serial.println("Disconnected from WiFi access point");
      break;
    case SYSTEM_EVENT_STA_AUTHMODE_CHANGE:
      Serial.println("Authentication mode of access point has changed");
      break;
    case SYSTEM_EVENT_STA_GOT_IP:
      Serial.print("Obtained IP address: ");
      Serial.println(WiFi.localIP());
      break;
    case SYSTEM_EVENT_STA_LOST_IP:
      Serial.println("Lost IP address and IP address is reset to 0");
      break;
    case SYSTEM_EVENT_STA_WPS_ER_SUCCESS:
      Serial.println("WiFi Protected Setup (WPS): succeeded in enrollee mode");
      break;
    case SYSTEM_EVENT_STA_WPS_ER_FAILED:
      Serial.println("WiFi Protected Setup (WPS): failed in enrollee mode");
      break;
    case SYSTEM_EVENT_STA_WPS_ER_TIMEOUT:
      Serial.println("WiFi Protected Setup (WPS): timeout in enrollee mode");
      break;
    case SYSTEM_EVENT_STA_WPS_ER_PIN:
      Serial.println("WiFi Protected Setup (WPS): pin code in enrollee mode");
      break;
    case SYSTEM_EVENT_AP_START:
      Serial.println("WiFi access point started");
      break;
    case SYSTEM_EVENT_AP_STOP:
      Serial.println("WiFi access point stopped");
      break;
    case SYSTEM_EVENT_AP_STACONNECTED:
      Serial.println("Client connected");
      break;
    case SYSTEM_EVENT_AP_STADISCONNECTED:
      Serial.println("Client disconnected");
      break;
    case SYSTEM_EVENT_AP_STAIPASSIGNED:
      Serial.println("Assigned IP address to client");
      break;
    case SYSTEM_EVENT_AP_PROBEREQRECVED:
      Serial.println("Received probe request");
      break;
    case SYSTEM_EVENT_GOT_IP6:
      Serial.println("IPv6 is preferred");
      break;
    case SYSTEM_EVENT_ETH_START:
      Serial.println("Ethernet started");
      break;
    case SYSTEM_EVENT_ETH_STOP:
      Serial.println("Ethernet stopped");
      break;
    case SYSTEM_EVENT_ETH_CONNECTED:
      Serial.println("Ethernet connected");
      break;
    case SYSTEM_EVENT_ETH_DISCONNECTED:
      Serial.println("Ethernet disconnected");
      break;
    case SYSTEM_EVENT_ETH_GOT_IP:
      Serial.println("Obtained IP address");
      break;
    default:
      Serial.println("Unknown: " + event);
    break;
}}