# Yokai — Mini Weather Monitor

After using a really crappy indoor temp and humidity monitor, I wanted to build one for myself to see if I could do better. This is the result. Is it better? I have no idea, but at least it also has a forecast I can trust, built the way I like.

## What it does

* Indoor temperature (°F), humidity, and pressure from a BME280, refreshed every 60 seconds
* Clock with day and date, synced over NTP, with auto-shrinking type so long day names still fit
* Weather Underground forecast for today, tonight, and tomorrow: hi/lo, UV index, wind, precip chance, QPF, short phrase, and icon
* Location line (city, state) from WU
* Backlight that follows the room: a BH1750 ambient light sensor drives PWM with smooth ramping, no stepped jumps
* Boot log rendered on the TFT itself, so you can see what's failing without a serial cable
* On-screen banners for wifi / fetch failures, plus a timestamped log on the SD card (`/weather.log`) for postmortems
* Hardware watchdog covering the long blocking bits (wifi associate, HTTP fetch)

## Hardware

* ESP32 devkit (ESP-WROOM-32)
* 4" 480x320 TFT, ST7796 over SPI, run in portrait (320x480)
* BME280 (temp / humidity / pressure) and BH1750 (ambient light) on I2C
* Micro SD card on its own VSPI bus
* Custom PCB and schematic in `pcb/` (KiCad), 3D-printed stand/case models in `model/` (SolidWorks)

Pin map lives in `src/User_Setup.h`. TFT on MOSI 13 / SCLK 14 / CS 15 / DC 26 / RST 32, backlight on GPIO 4 (PWM). SD on VSPI: MOSI 23 / MISO 19 / SCLK 18 / CS 5.

## SD card contents

Everything in `sdcard/` goes on the card root:

* `kozgoprobold*.vlw` — Kozuka Gothic Bold smooth fonts, the UI typeface
* `*_120x120.jpg` — weather icons, named by Weather Underground icon code
* `weather.cfg` — your secrets and location (below). Not in the repo on purpose.

## Configuration

`/weather.cfg` on the SD card, JSON:

```json
{
  "wu_api_key": "your weather underground key",
  "zip": "94070",
  "station_id": "",
  "WIFI": { "hostname": "yokai", "ssid": "your-ssid", "pass": "your-password" },
  "LOCATION": { "latitude": "37.50", "longitude": "-122.34" }
}
```

## Build and flash

PlatformIO project (`platformio.ini` targets `esp32dev`, Arduino framework). Build and upload from VS Code with PlatformIO, or:

```
pio run -t upload
```

Serial monitor at 115200 baud.

## Layout

* `src/` — firmware
* `pcb/` — KiCad schematic and PCB
* `model/` — SolidWorks case/stand models plus a gcode
* `sdcard/` — fonts, weather icons, and config that live on the SD card
