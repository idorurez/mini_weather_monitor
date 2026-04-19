/*
 * Hardware:
 * - ESP-WROOM-32 devkit
 * - 4" 480x320 TFT (ST7796) on HSPI; pins in src/User_Setup.h
 * - SD card on VSPI (separate bus from TFT)
 * - BME280 + BH1750 on I2C
 * Pin map in src/User_Setup.h. Runtime config (api keys, ssid, geo) on SD as /weather.cfg.
 */
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

#define LED_BUILTIN   2  //Diagnostics using built-in LED

// timeout after 60 seconds
#define WDT_TIMEOUT 60

// === backlight screen pwm
// Target = what the light sensor wants right now.
// Current = where the backlight actually is; glides toward target via pwmStep().
int   pwmTarget  = 0;
float pwmCurrent = 0;
unsigned long pwmStepTime = 0;
const unsigned long pwmStepDelay = 20;  // ms between ramp steps
const float pwmSmoothing = 0.12;        // 0..1; higher = faster fade

// BME280
#define SEALEVELPRESSURE_HPA (1013.25)
#define BME280_TEMP_ADJUST -2

Adafruit_BME280 bme; // I2C

const unsigned long bmeUpdateDelay     = 60UL * 1000;        // 60s
const unsigned long weatherUpdateDelay = 30UL * 60 * 1000;   // 30min
const unsigned long timeUpdateDelay    = 1000;               // 1s
const unsigned long heapLogDelay       = 60UL * 1000;        // 60s

unsigned long bmeUpdateTime = 0;
unsigned long weatherUpdateTime = 0;
unsigned long timeUpdateTime = 0;
unsigned long heapLogTime = 0;
unsigned long currTime;

// BH1750 I2C
BH1750 lightMeter(0x23);
float lux;
#define DARK 5

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr_pressure = TFT_eSprite(&tft);
TFT_eSprite spr_time = TFT_eSprite(&tft);
TFT_eSprite spr_location = TFT_eSprite(&tft);

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -28800;
const int   daylightOffset_sec = 3600;

struct WeatherConfig  {
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

// Owned char buffers — values survive the JsonDocument that produced them.
struct ForecastParsed {
  bool  valid;
  char  dayOfWeek[16];
  char  dayPartName[16];
  int   temperature;
  int   temperatureMax;
  int   temperatureMin;
  int   precipChance;
  float qpf;
  float windSpeed;
  char  windDirectionCardinal[8];
  char  wxPhraseShort[40];
  int   iconCode;
  char  uvDescription[16];
  int   uvIndex;
};

ForecastParsed today, tonight, tomorrow;

struct Location {
  bool valid;
  char neighborhood[48];
  char city[48];
  char state[8];
};

Location location;

// Network/screen state surfaced from wifi + fetch layers.
enum NetState {
  NET_OK,
  NET_WIFI_FAILED,
  NET_CAPTIVE_PORTAL,
  NET_FETCH_FAILED
};

NetState netState = NET_OK;

enum ForecastReq {
  FIVEDAY,
  CURRENT
};

enum Orientation {
  PORTRAIT = 2,
  LANDSCAPE = 3,
};

const Orientation oriented = PORTRAIT;
const int TFT_W = 320;
const int TFT_H = 480;

// Refresh weather + location, updating netState and screen banners as needed.
static void doWeatherRefresh() {
  if (!wifiConnect()) {
    netState = NET_WIFI_FAILED;
    drawNetBanner(netState);
    return;
  }
  if (isBehindCaptivePortal()) {
    netState = NET_CAPTIVE_PORTAL;
    drawNetBanner(netState);
    return;
  }
  bool fok = refreshForecast();
  bool lok = refreshLocation();
  if (fok && lok) {
    netState = NET_OK;
    clearNetBanner();
    drawForecast();
    drawLocation();
  } else {
    netState = NET_FETCH_FAILED;
    drawNetBanner(netState);
  }
}

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
  ledcWrite(PWM1_CH, (int)pwmCurrent);

  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
  }
  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);

  GetSetConfig();

  if (!lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
    Serial.println(F("BH1750 init failed"));
  }

  if (!bme.begin(0x76)) {
    Serial.printf("BME280 init failed (sensorID=0x%x)\n", bme.sensorID());
  }
  bme.setSampling(Adafruit_BME280::MODE_FORCED,
                  Adafruit_BME280::SAMPLING_X1,
                  Adafruit_BME280::SAMPLING_X1,
                  Adafruit_BME280::SAMPLING_X1,
                  Adafruit_BME280::FILTER_OFF);
  bme.takeForcedMeasurement();

  tft.begin();
  tft.invertDisplay(0);
  tft.setRotation(oriented);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE);
  tft.fillScreen(TFT_BLACK);
  tft.println("Loading...");
  BlinkLED(3);

  // Watchdog covers the long blocking sections (wifi associate, http fetch).
  esp_task_wdt_init(WDT_TIMEOUT, true);
  esp_task_wdt_add(NULL);

  doWeatherRefresh();
  weatherUpdateTime = millis();

  displayIndoorConditions(bme.readTemperature(), bme.readPressure(), bme.readHumidity());
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  currTime = millis();
  bmeUpdateTime = currTime;
  timeUpdateTime = currTime;
  heapLogTime = currTime;

  BlinkLED(4);
}

void loop() {
  esp_task_wdt_reset();
  bme.takeForcedMeasurement();

  if (lightMeter.measurementReady()) {
    lux = lightMeter.readLightLevel();
    int target = (int)(0.85 * lux) + 1;
    pwmTarget = constrain(target, 1, 90);
  }

  currTime = millis();

  // Glide pwmCurrent toward pwmTarget so brightness changes are smooth, not stepped.
  if ((currTime - pwmStepTime) > pwmStepDelay) {
    pwmStepTime = currTime;
    float delta = (float)pwmTarget - pwmCurrent;
    if (fabsf(delta) < 0.5f) {
      pwmCurrent = pwmTarget;
    } else {
      pwmCurrent += delta * pwmSmoothing;
    }
    ledcWrite(PWM1_CH, (int)(pwmCurrent + 0.5f));
  }

  if ((currTime - bmeUpdateTime) > bmeUpdateDelay) {
    displayIndoorConditions(bme.readTemperature(), bme.readPressure(), bme.readHumidity());
    bmeUpdateTime = currTime;
  }

  if ((currTime - weatherUpdateTime) > weatherUpdateDelay) {
    doWeatherRefresh();
    weatherUpdateTime = currTime;
  }

  if ((currTime - timeUpdateTime) > timeUpdateDelay) {
    printLocalTime(0, 175);
    timeUpdateTime = currTime;
  }

  if ((currTime - heapLogTime) > heapLogDelay) {
    Serial.printf("[heap] free=%u min=%u\n", ESP.getFreeHeap(), ESP.getMinFreeHeap());
    heapLogTime = currTime;
  }
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
      strlcpy(config.zip,        doc["zip"]        | "", sizeof(config.zip));
      strlcpy(config.station_id, doc["station_id"] | "", sizeof(config.station_id));
      strlcpy(config.hostname,   doc["WIFI"]["hostname"] | "", sizeof(config.hostname));
      strlcpy(config.ssid,       doc["WIFI"]["ssid"]     | "", sizeof(config.ssid));
      strlcpy(config.pass,       doc["WIFI"]["pass"]     | "", sizeof(config.pass));
      strlcpy(config.latitude,   doc["LOCATION"]["latitude"] | "", sizeof(config.latitude));
      strlcpy(config.longitude,  doc["LOCATION"]["longitude"] | "", sizeof(config.longitude));
     }
     configFile.close();
  } else {
    Serial.println(F("Failed to open config file"));
  }
}

void BlinkLED(int num)
{
  int x;
  //if reason code =0, then set num =1 (just so I can see something)
  if (!num)
  {
    num = 1;
  }
  for (x = 0; x < num; x++)
  {
    //LED ON
    digitalWrite(LED_BUILTIN, HIGH);
    delay(200);
    //LED OFF
    digitalWrite(LED_BUILTIN, LOW);
    delay(100);
  }
  delay(1000);
}