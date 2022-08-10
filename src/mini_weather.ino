/*
 * Hardware:
 * - TFT LCD (SD card + touch) using ILI9341 via 8bit parallel interface: http://www.lcdwiki.com/3.2inch_16BIT_Module_ILI9341_SKU:MRB3205
 * - ESP-WROOM-32 dev Board 
 * 
 * Wiring: just follow the pin definitios below
 * NOTE: In order to make everything work you HAVE to solder the SMD resistor (actually it's a jumper) in 8bit position.
 * */
// #include <XPT2046_Touchscreen.h> //https://github.com/PaulStoffregen/XPT2046_Touchscreen
#include <TFT_eSPI.h> // https://github.com/Bodmer/TFT_eSPI
#include <SPI.h>  //https://github.com/espressif/arduino-esp32/tree/master/libraries/SPI
#include <SD.h>
#include <SoftwareSerial.h>
#include "time.h"

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <BH1750.h>
#include <JPEGDecoder.h>
#include <Wifi.h>
#include <secrets.h>
#include <TinyGPSPlus.h>
#include <SPIFFS.h>

// === backlight screen pwm
int PWM1_DutyCycle = 0;

// Buffer for data output
char dataOut[256];
volatile bool mpuInterrupt = false;     // indicates whether MPU interrupt pin has gone high

// BME280
#define SEALEVELPRESSURE_HPA (1013.25)
#define BME280_TEMP_ADJUST -2

Adafruit_BME280 bme; // software SPI
Adafruit_Sensor *bme_temp;
Adafruit_Sensor *bme_pressure;
Adafruit_Sensor *bme_humidity;

unsigned long bmeUpdateDelay = 60 * 1000; // 60 seconds for bme update
unsigned long weather5dayDelay = 30 * 60 * 1000; // 30 minutes

unsigned long bmeUpdateTime = 0;
unsigned long weather5dayUpdateTime = 0;
unsigned long currTime;
bool triggerUpdate = false;
String forecastResp, locationResp;

// BH1750 I2C
BH1750 lightMeter(0x23);

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft); // Sprite object
TFT_eSprite sprTime = TFT_eSprite(&tft);
TFT_eSprite sprForecastBlock = TFT_eSprite(&tft);
TFT_eSprite sprTodaysForecastBlock = TFT_eSprite(&tft);

sensors_event_t temp_event, pressure_event, humidity_event;

// current time stuff

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -28800;
const int   daylightOffset_sec = 3600;

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

struct LocationParsed {
  const char* neighborhood;
  const char* city;
  const char* state;
};

enum ForecastReq {
  FIVEDAY,
  CURRENT
};

enum Orientation {
  PORTRAIT = 2,
  LANDSCAPE = 3,
};

enum Orientation oriented = PORTRAIT;

String latitude = "37.4959";
String longitude = "-122.2764";

int TFT_W = 480;
int TFT_H = 320;

int tilt_pin = 2;

void setup(void) {
  Serial.begin(115200);
  
  pinMode(tilt_pin, INPUT);

  digitalWrite(TFT_CS, HIGH);
  digitalWrite(TFT_BL, HIGH);
  digitalWrite(SD_PIN, HIGH);  

  tft.begin();
  Wire.begin(); // sda, scl, clock speed 

  // ==== LCD dimming
  ledcAttachPin(TFT_BL, PWM1_CH);
  ledcSetup(PWM1_CH, PWM1_FREQ, PWM1_RES);
  ledcWrite(PWM1_CH, PWM1_DutyCycle);

  currTime = millis();

   //========== Initialize SPIFFS and SDCARD

  // if (!SPIFFS.begin()) {
  //   Serial.println("SPIFFS initialisation failed!");
  //   while (1) yield(); // Stay here twiddling thumbs waiting
  // }

 if (!SD.begin()) {
    Serial.println("Card Mount Failed");
    ESP.restart();
  }
  uint8_t cardType = SD.cardType();

  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    // return;
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

  File root = SD.open("/");
  // printDirectory(root, 0);


  // ========== Initialize display and set orientation
  tft.invertDisplay(0);
  setOrientation();

  //========== Initialize BME

  unsigned bme_status;
  bme_status = bme.begin(0x76);

  if (!bme_status) {
    Serial.println("Could not find a valid BME280 sensor, check wiring, address, sensor ID!");
    Serial.print("SensorID was: 0x"); Serial.println(bme.sensorID(),16);
    Serial.print("        ID of 0xFF probably means a bad address, a BMP 180 or BMP 085\n");
    Serial.print("   ID of 0x56-0x58 represents a BMP 280,\n");
    Serial.print("        ID of 0x60 represents a BME 280.\n");
    Serial.print("        ID of 0x61 represents a BME 680.\n");
    ESP.restart();
  } else {
    bme_temp->printSensorDetails();
    bme_pressure->printSensorDetails();
    bme_humidity->printSensorDetails();
  }

  bme.setSampling(Adafruit_BME280::MODE_FORCED,
    Adafruit_BME280::SAMPLING_X1, // temperature
    Adafruit_BME280::SAMPLING_X1, // pressure
    Adafruit_BME280::SAMPLING_X1, // humidity
    Adafruit_BME280::FILTER_OFF );
  bme_temp = bme.getTemperatureSensor();
  bme_pressure = bme.getPressureSensor();
  bme_humidity = bme.getHumiditySensor();

  bme.takeForcedMeasurement();

  if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
    Serial.println(F("BH1750 Advanced begin"));
  }
  else {
    Serial.println(F("Error initialising BH1750"));
    ESP.restart();
  }

  //========== Get and display initial readings for weather
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  wifiConnect();
  forecastResp = getForecast(FIVEDAY); 
  locationResp = getLocation();

  drawAllForecast();
  // sampleIndoorAtmo();
  displayIndoorConditions(bme.readTemperature(), bme.readPressure(), bme.readHumidity());
  weather5dayUpdateTime = currTime;

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

void loop() { 
  bme.takeForcedMeasurement();
  setOrientation();

  // === check light meter stuff and set display intensity
  if (lightMeter.measurementReady()) {
    float lux = lightMeter.readLightLevel();
    PWM1_DutyCycle = (0.85 * lux) + 1;
    PWM1_DutyCycle = constrain(PWM1_DutyCycle, 0.001, 90);
    ledcWrite(PWM1_CH, PWM1_DutyCycle);
  }

  // === check if we are scheduled to update indoor temps
  currTime = millis();

  if (triggerUpdate) {
    // clear the screen if we've triggered an update based on orientation
     tft.fillRect(0, 0, TFT_W, TFT_H, TFT_BLACK);
  }
  // Serial.printf("currTime is %d\n", currTime);
  if (((currTime - bmeUpdateTime) > bmeUpdateDelay) || triggerUpdate ) {
    // Serial.printf("UPDATING AT %d\n", currTime);
    // sampleIndoorAtmo();
    displayIndoorConditions(bme.readTemperature(), bme.readPressure(), bme.readHumidity());
    bmeUpdateTime = currTime;
  }

  // === check if we are scheduled to update weather info
  if ((currTime - weather5dayUpdateTime > weather5dayDelay) || triggerUpdate) {
    
    if (!triggerUpdate) {
      // only connect to wifi if we're not flippin' around
      wifiConnect();
      forecastResp = getForecast(FIVEDAY); 
      locationResp = getLocation();
    }

    weather5dayUpdateTime = currTime;
    drawAllForecast();
    drawPressure(bme.readPressure());
  }
  
  printLocalTime(0, 175);

  if (triggerUpdate) {
    triggerUpdate = false;
  }
}

void setOrientation() {
  int curr_state = digitalRead(tilt_pin);
  if (curr_state != LOW) {
    if (oriented == LANDSCAPE) {
      triggerUpdate = true;
    }
    // Serial.println("orientation is PORTRAIT");
    oriented = PORTRAIT;
    
    TFT_W = 320;
    TFT_H = 480;
  } else {
    if (oriented == PORTRAIT) {
      triggerUpdate = true;
    }
    // Serial.println("orientation is LANDSCAPE");
    oriented = LANDSCAPE;
     TFT_W = 480;
    TFT_H = 320;
  }
  tft.setRotation(oriented);
}