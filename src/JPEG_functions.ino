// Draw a JPEG on the TFT pulled from SD card. xpos, ypos is top-left corner.
void drawSdJpeg(const char *filename, int xpos, int ypos) {
  File jpegFile = SD.open(filename, FILE_READ);
  if (!jpegFile) {
    Serial.printf("ERROR: file \"%s\" not found\n", filename);
    return;
  }
  if (JpegDec.decodeSdFile(jpegFile)) {
    jpegRender(xpos, ypos);
  } else {
    Serial.println("Jpeg format not supported");
  }
}



// Decode JpegDec MCUs into the TFT, cropping images that overhang the screen.
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

  max_x += xpos;
  max_y += ypos;

  while (JpegDec.read()) {
    pImg = JpegDec.pImage;
    int mcu_x = JpegDec.MCUx * mcu_w + xpos;
    int mcu_y = JpegDec.MCUy * mcu_h + ypos;

    win_w = (mcu_x + mcu_w <= max_x) ? mcu_w : min_w;
    win_h = (mcu_y + mcu_h <= max_y) ? mcu_h : min_h;

    // Pack partial MCU rows into a contiguous block before pushing.
    if (win_w != mcu_w) {
      uint16_t *cImg = pImg + win_w;
      int p = 0;
      for (int h = 1; h < win_h; h++) {
        p += mcu_w;
        for (int w = 0; w < win_w; w++) {
          *cImg++ = *(pImg + w + p);
        }
      }
    }

    if ((mcu_x + win_w) <= tft.width() && (mcu_y + win_h) <= tft.height()) {
      tft.pushImage(mcu_x, mcu_y, win_w, win_h, pImg);
    } else if ((mcu_y + win_h) >= tft.height()) {
      JpegDec.abort();
    }
  }

  tft.setSwapBytes(swapBytes);
}
