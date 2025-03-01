//                            USER DEFINED SETTINGS
//   Set driver type, fonts to be loaded, pins used and SPI control method etc
//
//   See the User_Setup_Select.h file if you wish to be able to define multiple
//   setups and then easily select which setup file is used by the compiler.
//
//   If this file is edited correctly then all the library example sketches should
//   run without the need to make any more changes for a particular hardware setup!
//   Note that some sketches are designed for a particular TFT pixel width/height

// Only define one driver, the other ones must be commented out


// #define ILI9486_DRIVER
#define ST7796_DRIVER

#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15  // Chip select control pin (library pulls permanently low

#define TFT_DC   26  // (RS) Data Command control pin - must use a pin in the range 0-31
#define TFT_RST  32  // Reset pin, toggles on startup
#define TFT_BL   4  // LED back-light

#define SD_PIN      5
#define SD_MISO     19
#define SD_MOSI     23
#define SD_SCLK     18
// SPI.begin(18, 19, 23, 5)
// SPI.setDataMode(SPI_MODE0);
#define USE_HSPI_PORT

#define PWM1_CH    0
#define PWM1_RES   8
#define PWM1_FREQ 70588

#define SMOOTH_FONT

#define SPI_FREQUENCY  27000000
#define SPI_READ_FREQUENCY  20000000
// #define SPI_TOUCH_FREQUENCY  2500000
 
// #define TFT_INVERSION_OFF