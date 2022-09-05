# Mini indoor weather monitor

# What is this?
After using a really crappy indoor temp and humidity monitor, I wanted to build one for myself to see if I could do better. This is the result. Is it better? I have no idea, but at least it also has a forecast that I can trust and built the way I like.

## Hardware
* esp32 (devkit v1)
* bme280
* bh1750
* tilt sensor
* 4" tft spi 480x320 v1.0 LCD

## What's in this
* uses tft_spi by bodmer
* kicad pcb and schematics
* pulls info from weather underground
* uses various jpg and converted fonts stored in /data (Upload the stuff in data using Upload Filestystem Image)

## How to build

* Current repo currently uses platform io to develop and manage and deploy
* If you want to use SPIFFS, upload the stuff in /data using platform io
* Othewrise, if you want to use the sdcard, you'll need to change a few lines of code to source from that instead and to be sure to move everything from /data onto the sdcard
