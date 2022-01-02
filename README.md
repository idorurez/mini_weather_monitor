# Mini indoor weather monitor

## Hardware
* esp32 (devkit v1)
* bme280
* 4" tft spi 480x320 v1.0 LCD

## What's in this
* uses tft_spi by bodmer
* kicad pcb and schematics
* pulls info from weather underground
* uses various jpg and converted fonts stored in /data (Upload the stuff in data using Upload Filestystem Image)

## TODO
* parse and display 3 day forecast using the jpgs i crammed into eeprom

Current repo currently uses platform io to develop and manage and deploy
