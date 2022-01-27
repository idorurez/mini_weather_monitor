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

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <BH1750.h>
#include <JPEGDecoder.h>
#include <Wifi.h>
#include <secrets.h>

// int TFT_W = 480;
// int TFT_H = 320;
 
int PWM1_DutyCycle = 0;

#define SEALEVELPRESSURE_HPA (1013.25)
Adafruit_BME280 bme; // software SPI
Adafruit_Sensor *bme_temp;
Adafruit_Sensor *bme_pressure;
Adafruit_Sensor *bme_humidity;

// BH1750 I2C
BH1750 lightMeter(0x23);

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft); // Sprite object

unsigned long bmeUpdateDelay = 30 * 1000; // 30 seconds for bme update
unsigned long weather5dayDelay = 30 * 60 * 1000; // 30 minutes

unsigned long bmeUpdateTime = 0;
unsigned long weather5dayUpdateTime = 0;
unsigned long currTime;

sensors_event_t temp_event, pressure_event, humidity_event;

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
  PORTRAIT,
  LANDSCAPE
};

Orientation oriented = LANDSCAPE;

void setup(void) {
  Serial.begin(115200);
  
  digitalWrite(TFT_CS, HIGH);
  digitalWrite(TFT_BL, HIGH);
  digitalWrite(SD_PIN, HIGH);
  pinMode(23, INPUT_PULLUP);

  tft.begin();
  Wire.begin();
 

  ledcAttachPin(TFT_BL, PWM1_CH);
  ledcSetup(PWM1_CH, PWM1_FREQ, PWM1_RES);
  ledcWrite(PWM1_CH, PWM1_DutyCycle);

  currTime = millis();

   //========== Initialize SPIFFS

  if (!SPIFFS.begin()) {
    Serial.println("SPIFFS initialisation failed!");
    while (1) yield(); // Stay here twiddling thumbs waiting
  }

  
 if (!SD.begin()) {
    Serial.println("Card Mount Failed");
    return;
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

  // File root = SD.open("/");
  // printDirectory(root, 0);


  // ========== Initialize display
  tft.invertDisplay(0);
  tft.setRotation(3);
  // tft.setRotation(0);

  //========== Initialize BME

  unsigned bme_status;
  bme_status = bme.begin(0x76);
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

  if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
    Serial.println(F("BH1750 Advanced begin"));
  }
  else {
    Serial.println(F("Error initialising BH1750"));
  }

  //========== Get and display initial readings for weather
  tft.fillScreen(TFT_BLACK);

  tft.setCursor(0,0);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.println("Initializing.");

  tft.print("Connecting to wifi... ");
  wifiConnect();
  tft.println("Success");

  drawAllForecast();
  sampleIndoorAtmo();

  displayIndoorConditions(temp_event, pressure_event, humidity_event);
  weather5dayUpdateTime = currTime;
}

void loop() {

  // if (oriented == LANDSCAPE) {
  //   TFT_W  = 480;
  //   TFT_H  = 320;
  // } else {
  //   TFT_W  = 320;
  //   TFT_H  = 480;
  // }

  // === check light meter stuff and set display intensity
  if (lightMeter.measurementReady()) {
    float lux = lightMeter.readLightLevel();
    PWM1_DutyCycle = (0.85 * lux) + 1;
    ledcWrite(PWM1_CH, PWM1_DutyCycle);
  }

  // === check if we are scheduled to update indoor temps
  currTime = millis();
  if (currTime - bmeUpdateTime > bmeUpdateDelay) {
    sampleIndoorAtmo();
    displayIndoorConditions(temp_event, pressure_event, humidity_event);
    bmeUpdateTime = currTime;
  }

  // === check if we are scheduled to update weather info
  if (currTime - weather5dayUpdateTime > weather5dayDelay) {
    wifiConnect();
    weather5dayUpdateTime = currTime;
    drawAllForecast();
    sampleIndoorAtmo();
    drawPressure(pressure_event);
  }

}

void sampleIndoorAtmo() {
    bme_temp->getEvent(&temp_event);
    bme_pressure->getEvent(&pressure_event);
    bme_humidity->getEvent(&humidity_event);
}

void printDirectory(File dir, int numTabs) {
  while (true) {

    File entry =  dir.openNextFile();
    if (! entry) {
      // no more files
      break;
    }
    for (uint8_t i = 0; i < numTabs; i++) {
      Serial.print('\t');
    }
    Serial.print(entry.name());
    if (entry.isDirectory()) {
      Serial.println("/");
      printDirectory(entry, numTabs + 1);
    } else {
      // files have sizes, directories do not
      Serial.print("\t\t");
      Serial.println(entry.size(), DEC);
    }
    entry.close();
  }
}