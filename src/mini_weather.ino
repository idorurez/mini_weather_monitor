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
#include <I2Cdev.h>
#include <MPU6050_6Axis_MotionApps20.h>

// === backlight screen pwm
int PWM1_DutyCycle = 0;

// GY-521
// http://www.geekmomprojects.com/mpu-6050-redux-dmp-data-fusion-vs-complementary-filter/
#define ESP32 1
const int MPU_ADDR = 0x68; // MPU6050 I2C address
MPU6050 mpu;
bool dmpReady = false;  // set true if DMP init was successful
// uint8_t mpuIntStatus;   // holds actual interrupt status byte from MPU
uint8_t devStatus;      // return status after each device operation (0 = success, !0 = error)
uint16_t packetSize;    // expected DMP packet size (default is 42 bytes)
uint16_t fifoCount;     // count of all bytes currently in FIFO
uint8_t fifoBuffer[64]; // FIFO storage buffer


// Orientation/motion variables
Quaternion q;
VectorFloat gravity;
float euler[3];
float ypr[3];
const float RADIANS_TO_DEGREES = 57.2958; //180/3.14159
// Use the following global variables and access functions to help store the overall
// rotation angle of the sensor
unsigned long last_read_time;
float         last_x_angle;  // These are the filtered angles
float         last_y_angle;
float         last_z_angle;  
float         last_gyro_x_angle;  // Store the gyro angles to compare drift
float         last_gyro_y_angle;
float         last_gyro_z_angle;

//  Use the following global variables 
//  to calibrate the gyroscope sensor and accelerometer readings
float    base_x_gyro = 0;
float    base_y_gyro = 0;
float    base_z_gyro = 0;
float    base_x_accel = 0;
float    base_y_accel = 0;
float    base_z_accel = 0;


// This global variable tells us how to scale gyroscope data
float    GYRO_FACTOR;

// This global varible tells how to scale acclerometer data
float    ACCEL_FACTOR;

// Variables to store the values from the sensor readings
int16_t ax, ay, az;
int16_t gx, gy, gz;

// Buffer for data output
char dataOut[256];
volatile bool mpuInterrupt = false;     // indicates whether MPU interrupt pin has gone high

// BME280
#define SEALEVELPRESSURE_HPA (1013.25)
Adafruit_BME280 bme; // software SPI
Adafruit_Sensor *bme_temp;
Adafruit_Sensor *bme_pressure;
Adafruit_Sensor *bme_humidity;

unsigned long bmeUpdateDelay = 30 * 1000; // 30 seconds for bme update
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

Orientation oriented = PORTRAIT;

String latitude = "37.4959";
String longitude = "-122.2764";

int TFT_W = 480;
int TFT_H = 320;

void setup(void) {
  Serial.begin(115200);
  
  digitalWrite(TFT_CS, HIGH);
  digitalWrite(TFT_BL, HIGH);
  digitalWrite(SD_PIN, HIGH);  

  tft.begin();
  Wire.begin(); // sda, scl, clock speed 

  // === init MPU6050
  initMPU6050();

  // ==== LCD dimming
  ledcAttachPin(TFT_BL, PWM1_CH);
  ledcSetup(PWM1_CH, PWM1_FREQ, PWM1_RES);
  ledcWrite(PWM1_CH, PWM1_DutyCycle);

  currTime = millis();

   //========== Initialize SPIFFS and SDCARD

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


  // ========== Initialize display and set orientation
  tft.invertDisplay(0);
  setOrientation();

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
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  wifiConnect();
  forecastResp = getForecast(FIVEDAY); 
  locationResp = getLocation();

  drawAllForecast();
  sampleIndoorAtmo();
  displayIndoorConditions(temp_event, pressure_event, humidity_event);
  weather5dayUpdateTime = currTime;

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

void loop() {
  sampleMPU6050();
  setOrientation();

  tft.setTextWrap(true, true);

  // === check light meter stuff and set display intensity
  if (lightMeter.measurementReady()) {
    float lux = lightMeter.readLightLevel();
    PWM1_DutyCycle = (0.85 * lux) + 1;
    ledcWrite(PWM1_CH, PWM1_DutyCycle);
  }

  // === check if we are scheduled to update indoor temps
  currTime = millis();
  if ((currTime - bmeUpdateTime > bmeUpdateDelay) || triggerUpdate ) {
    sampleIndoorAtmo();
    displayIndoorConditions(temp_event, pressure_event, humidity_event);
    bmeUpdateTime = currTime;
  }

  // === check if we are scheduled to update weather info
  if ((currTime - weather5dayUpdateTime > weather5dayDelay) || triggerUpdate) {
    
    if (!triggerUpdate) {
      Serial.println("triggered update wifi connect");
      wifiConnect();
      forecastResp = getForecast(FIVEDAY); 
      locationResp = getLocation();
    }

    weather5dayUpdateTime = currTime;
    drawAllForecast();
    sampleIndoorAtmo();
    drawPressure(pressure_event);
  }
  
  printLocalTime(0, 175);

  if (triggerUpdate) {
    triggerUpdate = false;
  }
}

void sampleIndoorAtmo() {
    bme_temp->getEvent(&temp_event);
    bme_pressure->getEvent(&pressure_event);
    bme_humidity->getEvent(&humidity_event);
}

void setOrientation() {
     // Serial.print(ypr[2]*RADIANS_TO_DEGREES, 2);
    //  180/0 : 90 : 90 :  portrait
    //  90 : -180/180 : -180/180 - landscape

  float at = 15; // angle tolerance
  float yaw = ypr[2]*RADIANS_TO_DEGREES;
  float pitch = -ypr[1]*RADIANS_TO_DEGREES;
  float roll = ypr[0] *RADIANS_TO_DEGREES;

  Serial.print(yaw);
  Serial.print(":");
  Serial.print(pitch);
  Serial.print(":");
  Serial.print(roll);

  // potrait
  if ((yaw >= 0) && (90-at <= pitch) && (pitch <= 90+at) && (90-at <= roll) && (roll<= 90+at))
  {
    Serial.println(" -- PORTRAIT");
    if (oriented == LANDSCAPE) {
      tft.fillScreen(TFT_BLACK);
      triggerUpdate = true;
    }
    oriented = PORTRAIT;
    TFT_W  = 320;
    TFT_H  = 480;
    tft.setRotation(0); 

  // landscape
  } else {
    Serial.println(" -- LANDSCAPE");
    if (oriented == PORTRAIT) {
      tft.fillScreen(TFT_BLACK);
      triggerUpdate = true;
    }
    oriented = LANDSCAPE;
    TFT_W  = 480;
    TFT_H  = 320;
    tft.setRotation(3);
  }
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

inline unsigned long get_last_time() {return last_read_time;}
inline float get_last_x_angle() {return last_x_angle;}
inline float get_last_y_angle() {return last_y_angle;}
inline float get_last_z_angle() {return last_z_angle;}
inline float get_last_gyro_x_angle() {return last_gyro_x_angle;}
inline float get_last_gyro_y_angle() {return last_gyro_y_angle;}
inline float get_last_gyro_z_angle() {return last_gyro_z_angle;}

void set_last_read_angle_data(unsigned long time, float x, float y, float z, float x_gyro, float y_gyro, float z_gyro) {
  last_read_time = time;
  last_x_angle = x;
  last_y_angle = y;
  last_z_angle = z;
  last_gyro_x_angle = x_gyro;
  last_gyro_y_angle = y_gyro;
  last_gyro_z_angle = z_gyro;
}

// ================================================================
// ===               INTERRUPT DETECTION ROUTINE                ===
// ================================================================

void dmpDataReady() {
    mpuInterrupt = true;
}

// ================================================================
// ===                CALIBRATION_ROUTINE                       ===
// ================================================================
// Simple calibration - just average first few readings to subtract
// from the later data
void calibrate_sensors() {
  int       num_readings = 10;

  // Discard the first reading (don't know if this is needed or
  // not, however, it won't hurt.)
  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
  
  // Read and average the raw values
  for (int i = 0; i < num_readings; i++) {
    mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
    base_x_gyro += gx;
    base_y_gyro += gy;
    base_z_gyro += gz;
    base_x_accel += ax;
    base_y_accel += ay;
    base_y_accel += az;
  }
  
  base_x_gyro /= num_readings;
  base_y_gyro /= num_readings;
  base_z_gyro /= num_readings;
  base_x_accel /= num_readings;
  base_y_accel /= num_readings;
  base_z_accel /= num_readings;
}

void initMPU6050() {
  
    // initialize device
    Serial.println(F("Initializing MPU6050"));
    mpu.initialize();

    // verify connection
    Serial.println(F("Testing device connections..."));
    Serial.println(mpu.testConnection() ? F("MPU6050 connection successful") : F("MPU6050 connection failed"));

    // load and configure the DMP
    Serial.println(F("Initializing DMP..."));
    devStatus = mpu.dmpInitialize();
    
    // make sure it worked (returns 0 if so)
    if (devStatus == 0) {
        // turn on the DMP, now that it's ready
        Serial.println(F("Enabling DMP..."));
        mpu.setDMPEnabled(true);

        // // enable Arduino interrupt detection
        // Serial.println(F("Enabling interrupt detection (Arduino external interrupt 33)..."));
        // attachInterrupt(33, dmpDataReady, RISING);
        // mpuIntStatus = mpu.getIntStatus();

        // set our DMP Ready flag so the main loop() function knows it's okay to use it
        // Serial.println(F("DMP ready! Waiting for first interrupt..."));
        dmpReady = true;

        // Set the full scale range of the gyro
        uint8_t FS_SEL = 0;
        //mpu.setFullScaleGyroRange(FS_SEL);

        // get default full scale value of gyro - may have changed from default
        // function call returns values between 0 and 3
        uint8_t READ_FS_SEL = mpu.getFullScaleGyroRange();
        Serial.print("FS_SEL = ");
        Serial.println(READ_FS_SEL);
        GYRO_FACTOR = 131.0/(FS_SEL + 1);

        // get default full scale value of accelerometer - may not be default value.  
        // Accelerometer scale factor doesn't reall matter as it divides out
        uint8_t READ_AFS_SEL = mpu.getFullScaleAccelRange();
        Serial.print("AFS_SEL = ");
        Serial.println(READ_AFS_SEL);
        //ACCEL_FACTOR = 16384.0/(AFS_SEL + 1);
        
        // Set the full scale range of the accelerometer
        //uint8_t AFS_SEL = 0;
        //mpu.setFullScaleAccelRange(AFS_SEL);

        // get expected DMP packet size for later comparison
        packetSize = mpu.dmpGetFIFOPacketSize();
    } else {
        // ERROR!
        // 1 = initial memory load failed
        // 2 = DMP configuration updates failed
        // (if it's going to break, usually the code will be 1)
        Serial.print(F("DMP Initialization failed (code "));
        Serial.print(devStatus);
        Serial.println(F(")"));
    }

    // configure LED for output
    // pinMode(LED_PIN, OUTPUT);
    
    // get calibration values for sensors
    calibrate_sensors();
    set_last_read_angle_data(millis(), 0, 0, 0, 0, 0, 0);
}

void sampleMPU6050() {

 
  // if programming failed, don't try to do anything
  if (!dmpReady) return;
  
  unsigned long t_now = millis();
  // wait for MPU interrupt or extra packet(s) available
  // while (!mpuInterrupt && fifoCount < packetSize) {
  while (fifoCount < packetSize) {
      fifoCount = mpu.getFIFOCount();
      // Serial.println("fifoCount < packetSize");
      // Serial.println("fifoCount " + String(fifoCount) + " packetSize: " + String(packetSize));
      // Keep calculating the values of the complementary filter angles for comparison with DMP here
      // Read the raw accel/gyro values from the MPU-6050
      mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
            
      // Get time of last raw data read
      unsigned long t_now = millis();
        
      // Remove offsets and scale gyro data  
      float gyro_x = (gx - base_x_gyro)/GYRO_FACTOR;
      float gyro_y = (gy - base_y_gyro)/GYRO_FACTOR;
      float gyro_z = (gz - base_z_gyro)/GYRO_FACTOR;
      float accel_x = ax; // - base_x_accel;
      float accel_y = ay; // - base_y_accel;
      float accel_z = az; // - base_z_accel;
      
      float accel_angle_y = atan(-1*accel_x/sqrt(pow(accel_y,2) + pow(accel_z,2)))*RADIANS_TO_DEGREES;
      float accel_angle_x = atan(accel_y/sqrt(pow(accel_x,2) + pow(accel_z,2)))*RADIANS_TO_DEGREES;
      float accel_angle_z = 0;

      // Compute the (filtered) gyro angles
      float dt =(t_now - get_last_time())/1000.0;
      float gyro_angle_x = gyro_x*dt + get_last_x_angle();
      float gyro_angle_y = gyro_y*dt + get_last_y_angle();
      float gyro_angle_z = gyro_z*dt + get_last_z_angle();
      
      // Compute the drifting gyro angles
      float unfiltered_gyro_angle_x = gyro_x*dt + get_last_gyro_x_angle();
      float unfiltered_gyro_angle_y = gyro_y*dt + get_last_gyro_y_angle();
      float unfiltered_gyro_angle_z = gyro_z*dt + get_last_gyro_z_angle();     
      
      // Apply the complementary filter to figure out the change in angle - choice of alpha is
      // estimated now.  Alpha depends on the sampling rate...
      const float alpha = 0.96;
      float angle_x = alpha*gyro_angle_x + (1.0 - alpha)*accel_angle_x;
      float angle_y = alpha*gyro_angle_y + (1.0 - alpha)*accel_angle_y;
      float angle_z = gyro_angle_z;  //Accelerometer doesn't give z-angle
      
      // Update the saved data with the latest values
      // Serial.println("Updating last read angle data");
      set_last_read_angle_data(t_now, angle_x, angle_y, angle_z, unfiltered_gyro_angle_x, unfiltered_gyro_angle_y, unfiltered_gyro_angle_z);       
  }
  // get current FIFO count
  fifoCount = mpu.getFIFOCount();
  // check for overflow
  if (fifoCount == 1024) {
      // reset so we can continue cleanly
      // Serial.println("fifo overflow");
      mpu.resetFIFO();
    }  else {
    // wait for correct available data length, should be a VERY short wait
    // Serial.println("fifo count is " + String(fifoCount));
    while (fifoCount < packetSize) fifoCount = mpu.getFIFOCount();
    // read a packet from FIFO
    mpu.getFIFOBytes(fifoBuffer, packetSize);
    
    // track FIFO count here in case there is > 1 packet available
    // (this lets us immediately read more without waiting for an interrupt)
    fifoCount -= packetSize;
    
    // Obtain Euler angles from buffer
    mpu.dmpGetQuaternion(&q, fifoBuffer);
    mpu.dmpGetEuler(euler, &q);
    
    // Obtain YPR angles from buffer
    mpu.dmpGetQuaternion(&q, fifoBuffer);
    mpu.dmpGetGravity(&gravity, &q);
    mpu.dmpGetYawPitchRoll(ypr, &q, &gravity);

    // Output complementary data and DMP data to the serial port.  The signs on the data needed to be
    // fudged to get the angle direction correct.
    // Serial.print("CMP:");
    // Serial.print(get_last_x_angle(), 2);
    // Serial.print(":");
    // Serial.print(get_last_y_angle(), 2);
    // Serial.print(":");
    // Serial.println(-get_last_z_angle(), 2);
    // Serial.print("DMP:");
    // Serial.print(ypr[2]*RADIANS_TO_DEGREES, 2);
    // Serial.print(":");
    // Serial.print(-ypr[1]*RADIANS_TO_DEGREES, 2);
    // Serial.print(":");
    // Serial.println(ypr[0]*RADIANS_TO_DEGREES, 2);
        // // blink LED to indicate activity
        // blinkState = !blinkState;
        // digitalWrite(LED_PIN, blinkState);
  }
}