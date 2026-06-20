#include "simple_screens.h"

#include <cstdio>
#include <string>

#include "draw_helpers.h"
#include "fonts.h"
#include "image_loader.h"

namespace display {

void DrawSplash(Adafruit_GFX* gfx) {
  Clear(gfx);
  DrawBMP(gfx, "/OldSols.bmp", 0, 0, true);
}

void DrawLightsAlarm(Adafruit_GFX* gfx) {
  Clear(gfx);
  DrawBMP(gfx, "/light_large.bmp", 68, 16);  // 104x104
  gfx->setFont(&JetBrainsMono_Thin16pt7b);
  gfx->setTextSize(1);
  int16_t x, y;
  const char* str = "LIGHTS ON";
  GetTextLocation(gfx, str, HAlign::Center, VAlign::Bottom, &x, &y);
  gfx->setCursor(x, y);
  gfx->print(str);
}

void DrawDiscoverable(Adafruit_GFX* gfx, const DiscoverableProps& props) {
  Clear(gfx);
  gfx->setFont(&JetBrainsMono_Thin9pt7b);
  std::string message =
      std::string("Bluetooth Discoverable\nName: ") + props.bluetoothName;
  int16_t x, y;
  GetTextLocation(gfx, message.c_str(), HAlign::Left, VAlign::Center, &x, &y);
  gfx->setCursor(x, y);
  gfx->print(message.c_str());
}

void DrawOtaInProgress(Adafruit_GFX* gfx, const OtaInProgressProps& props) {
  char buffer[128];
  snprintf(buffer, sizeof(buffer), "%u Bytes",
           static_cast<unsigned>(props.bytesReceived));

  Clear(gfx);
  gfx->setFont(&JetBrainsMono_Thin14pt7b);
  WriteAligned(gfx, "Updating", HAlign::Center, VAlign::Top);
  gfx->setFont(&JetBrainsMono_Thin10pt7b);
  WriteAligned(gfx, buffer, HAlign::Center, VAlign::Center);
}

void DrawCalibrationMissing(Adafruit_GFX* gfx) {
  Clear(gfx);
  gfx->setFont(&JetBrainsMono_Thin7pt7b);
  WriteAligned(gfx,
               "Calibration missing\nPark on a flat surface\nThen press M to "
               "calibrate",
               HAlign::Center, VAlign::Center);
}

}  // namespace display
