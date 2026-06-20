#include "clock_screen.h"

#include <cstdio>

#include "draw_helpers.h"
#include "fonts.h"
#include "image_loader.h"

namespace display {

void DrawClock(Adafruit_GFX* gfx, const ClockProps& props) {
  Clear(gfx);

  bool isAfternoon = props.hours24 >= 12;
  uint8_t hours12 = 0;
  if (props.hours24 == 0) {
    hours12 = 12;
  } else if (props.hours24 > 12) {
    hours12 = props.hours24 - 12;
  } else {
    hours12 = props.hours24;
  }

  char display_string[128] = {0};
  snprintf(display_string, sizeof(display_string), "%i:%02i", hours12,
           props.minutes);
  gfx->setFont(&digital_7__mono_40pt7b);
  gfx->setTextSize(1);
  gfx->setTextWrap(false);  // disable wrap just for this function.
  int16_t x, y;
  Rect time_region;
  GetTextLocation(gfx, display_string, HAlign::Center, VAlign::Center, &x, &y,
                  nullptr, &time_region);
  gfx->setCursor(x, y);
  gfx->print(display_string);

  // Draw AM / PM indicator
  gfx->setFont(&JetBrainsMono_Thin8pt7b);
  gfx->setTextWrap(false);
  Rect afternoon_region;
  afternoon_region.x = time_region.x + time_region.w + 8;
  afternoon_region.y = time_region.y;
  afternoon_region.h = time_region.h;
  const Rect screen_rect = ScreenRect();
  afternoon_region.w =
      screen_rect.x + screen_rect.w - afternoon_region.x - afternoon_region.w;
  GetTextLocation(gfx, isAfternoon ? "PM" : "AM", HAlign::Left, VAlign::Bottom,
                  &x, &y, &afternoon_region);
  gfx->setCursor(x, y);
  gfx->print(isAfternoon ? "PM" : "AM");
  gfx->setTextWrap(true);  // turn wrap back on

  // Music info
  snprintf(display_string, sizeof(display_string), "%s", props.mediaTitle);
  gfx->setFont(&JetBrainsMono_Thin14pt7b);
  gfx->setTextWrap(false);
  GetTextLocation(gfx, display_string, HAlign::Left, VAlign::Bottom, &x, &y);
  gfx->setCursor(x, y);
  gfx->print(display_string);
  gfx->setTextWrap(true);

  // speed
  if (props.speedMph > 10) {
    snprintf(display_string, sizeof(display_string), "%.0f",
             static_cast<double>(props.speedMph));
    gfx->setFont(&JetBrainsMono_Thin12pt7b);
    WriteAligned(gfx, display_string, HAlign::Left, VAlign::Top);
    // add MPH
    gfx->setFont(&JetBrainsMono_Thin8pt7b);
    gfx->print("MPH");
  }

  // headlight
  if (props.headlight) {
    uint16_t color = 0x007F;  // green ~3, blue full.
    DrawBMP(gfx, "/light_small.bmp", screen_rect.x + screen_rect.w - 32, 0,
            false, color);
  }

  // bluetooth
  int16_t top = (screen_rect.h - 24 * 3) / 2 + screen_rect.y;
  if (props.bluetooth) {
    DrawBMP(gfx, "/bluetooth.bmp", 0, top);
  } else {
    DrawBMP(gfx, "/bluetooth.bmp", 0, top, false, 0xF800);
  }
}

}  // namespace display
