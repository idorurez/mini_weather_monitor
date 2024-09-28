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
#include "SPI.h"  //https://github.com/espressif/arduino-esp32/tree/master/libraries/SPI
#include "SD.h"
#include "time.h"
#include <esp_task_wdt.h>

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <BH1750.h>
#include <JPEGDecoder.h>
#include <Wifi.h>
#include <secrets.h>
#include <SPIFFS.h>

// timeout after 60 seconds
#define WDT_TIMEOUT 60

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
unsigned long weatherUpdateDelay = 30 * 60 * 1000; // 30 minutes

unsigned long bmeUpdateTime = 0;
unsigned long weatherUpdateTime = 0;
unsigned long currTime;
bool triggerUpdate = false;
String forecastResp, locationResp;

// BH1750 I2C
BH1750 lightMeter(0x23);

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr_pressure = TFT_eSprite(&tft); // Sprite object
TFT_eSprite spr_time = TFT_eSprite(&tft);
// TFT_eSprite sprForecastBlock = TFT_eSprite(&tft);
// TFT_eSprite sprTodaysForecastBlock = TFT_eSprite(&tft);
TFT_eSprite spr_location = TFT_eSprite(&tft);

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

int TFT_W = 320;
int TFT_H = 480;

int tilt_pin = 2;

void setup(void) {
  // esp_task_wdt_init(WDT_TIMEOUT, true); //enable panic so ESP32 restarts
  // esp_task_wdt_add(NULL); //add current thread to WDT watch

  Serial.begin(115200);
  
  // pinMode(tilt_pin, INPUT);

  digitalWrite(TFT_CS, HIGH);
  digitalWrite(TFT_BL, HIGH);
  digitalWrite(SD_PIN, HIGH);  

  Wire.begin(); // sda, scl, clock speed 

  // ==== LCD dimming
  ledcAttachPin(TFT_BL, PWM1_CH);
  ledcSetup(PWM1_CH, PWM1_FREQ, PWM1_RES);
  ledcWrite(PWM1_CH, PWM1_DutyCycle);

  currTime = millis();

   //========== Initialize SPIFFS and SDCARD

 if (!SD.begin(SD_PIN)) {
    Serial.println("Card Mount Failed");
    ESP.restart();
  }
  uint8_t cardType = SD.cardType();

  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return;
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

//   File root = SD.open("/");
  // printDirectory(root, 0);


  // ========== Initialize display and set orientation

  //========== Initialize BME

  unsigned bme_status;
  bme_status = bme.begin(0x76);

  bme.setSampling(Adafruit_BME280::MODE_FORCED,
    Adafruit_BME280::SAMPLING_X1, // temperature
    Adafruit_BME280::SAMPLING_X1, // pressure
    Adafruit_BME280::SAMPLING_X1, // humidity
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
    ESP.restart();
  } else {
    bme_temp->printSensorDetails();
    bme_pressure->printSensorDetails();
    bme_humidity->printSensorDetails();
  }

  bme.takeForcedMeasurement();

  if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
    Serial.println(F("BH1750 Advanced begin"));
  }
  else {
    Serial.println(F("Error initialising BH1750"));
    ESP.restart();
  }

  tft.begin();
  tft.init();
  tft.invertDisplay(0);
  tft.setRotation(oriented);

  //========== Get and display initial readings for weather
  tft.fillScreen(TFT_BLACK);

  wifiConnect();
  forecastResp = getForecast(FIVEDAY);
  locationResp = getLocation();

  drawForecast();
  displayIndoorConditions(bme.readTemperature(), bme.readPressure(), bme.readHumidity());
  drawLocation(parseLocation(locationResp), 0, 125);

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

void loop() { 

  // esp_task_wdt_reset();
  bme.takeForcedMeasurement();

  // === check light meter stuff and set display intensity
  if (lightMeter.measurementReady()) {
    float lux = lightMeter.readLightLevel();
    PWM1_DutyCycle = (0.85 * lux) + 1;
    PWM1_DutyCycle = constrain(PWM1_DutyCycle, 0.001, 90);
    ledcWrite(PWM1_CH, PWM1_DutyCycle);
  }

  // === check if we are scheduled to update indoor temps
  currTime = millis();

  // Serial.printf("currTime is %d\n", currTime);
  if ((currTime - bmeUpdateTime) > bmeUpdateDelay) {
    displayIndoorConditions(bme.readTemperature(), bme.readPressure(), bme.readHumidity());
    bmeUpdateTime = currTime;
  }

  if ((currTime - weatherUpdateTime) > weatherUpdateDelay) {
    
    wifiConnect();
    forecastResp = getForecast(FIVEDAY);
    locationResp = getLocation();

    drawForecast();
    drawLocation(parseLocation(locationResp), 0, 125);
    weatherUpdateTime = currTime;
  }

  printLocalTime(0, 175);

}