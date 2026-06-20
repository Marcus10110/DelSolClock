#include "quarter_mile_screens.h"

#include <cstdio>

#include "draw_helpers.h"
#include "fonts.h"

namespace display {
namespace quarter_mile {
namespace {

void DrawPair(Adafruit_GFX* gfx, const char* title, const char* value, int x,
              int y, bool tiny_value = false) {
  const int line_height = 20;
  gfx->setFont(&JetBrainsMono_Thin7pt7b);
  gfx->setCursor(x, y);
  gfx->write(title);
  if (!tiny_value) {
    gfx->setFont(&JetBrainsMono_Thin10pt7b);
  }
  gfx->setCursor(x, y + line_height);
  gfx->write(value);
}

void DrawProgressBar(Adafruit_GFX* gfx, uint16_t /*y*/, double percentage) {
  uint16_t h = 7;
  uint16_t w = 220;
  uint16_t bar_w = static_cast<uint16_t>(percentage * 220);
  gfx->fillRect(10, 120, w, h, 0xFFFF);
  gfx->fillRect(10, 120, bar_w, h, 0xF800);
  gfx->fillRect(bar_w + 10, 120 - 4, 2, h + 8, 0xF800);
}

}  // namespace

void DrawStart(Adafruit_GFX* gfx) {
  Clear(gfx);
  gfx->setFont(&JetBrainsMono_Thin14pt7b);
  WriteAligned(gfx, "Quarter Mile", HAlign::Center, VAlign::Top);
  gfx->setFont(&JetBrainsMono_Thin10pt7b);
  WriteAligned(gfx, "Press M to Start", HAlign::Center, VAlign::Center);
}

void DrawLaunch(Adafruit_GFX* gfx, const LaunchProps& props) {
  Clear(gfx);
  Rect top_region;
  gfx->setFont(&JetBrainsMono_Thin14pt7b);
  WriteAligned(gfx, "Quarter Mile", HAlign::Center, VAlign::Top, nullptr,
               &top_region);

  gfx->setFont(&JetBrainsMono_Thin8pt7b);
  Rect second_region;
  second_region.x = 0;
  second_region.y = top_region.h + top_region.y + 10;
  second_region.w = 240;
  second_region.h = 135 - second_region.y;
  WriteAligned(gfx, "Waiting for Launch", HAlign::Center, VAlign::Top,
               &second_region);
  char buffer[128];
  snprintf(buffer, sizeof(buffer), "%.1f g", props.accelerationG);
  WriteAligned(gfx, buffer, HAlign::Center, VAlign::Center);
}

void DrawInProgress(Adafruit_GFX* gfx, const InProgressProps& props) {
  Clear(gfx);
  gfx->setFont(&JetBrainsMono_Thin14pt7b);
  WriteAligned(gfx, "Quarter Mile", HAlign::Center, VAlign::Top);

  const int x1 = 10;
  const int x2 = 120;
  const int y1 = 40;
  const int y_pitch = 46;
  char buffer[128];
  snprintf(buffer, sizeof(buffer), "%.1f s", props.timeSec);
  DrawPair(gfx, "Time", buffer, x1, y1);

  snprintf(buffer, sizeof(buffer), "%.2f mi", props.distanceMiles);
  DrawPair(gfx, "Distance", buffer, x2, y1);

  snprintf(buffer, sizeof(buffer), "%.1f g", props.accelerationG);
  DrawPair(gfx, "Accel", buffer, x1, y1 + y_pitch);

  snprintf(buffer, sizeof(buffer), "%.1f mph", props.speedMph);
  DrawPair(gfx, "Speed", buffer, x2, y1 + y_pitch);

  double distance = props.distanceMiles;
  if (distance < 0) distance = 0;
  if (distance > 0.25) distance = 0.25;
  DrawProgressBar(gfx, 120, distance / 0.25);
}

void DrawSummary(Adafruit_GFX* gfx, const SummaryProps& props) {
  Clear(gfx);
  gfx->setFont(&JetBrainsMono_Thin14pt7b);
  WriteAligned(gfx, "Quarter Mile", HAlign::Center, VAlign::Top);

  const int x1 = 10;
  const int x2 = 130;
  const int y1 = 45;
  const int y_pitch = 55;

  char buffer[128];
  snprintf(buffer, sizeof(buffer), "%.1f s", props.quarterMileTimeSec);
  DrawPair(gfx, "1/4 mi", buffer, x1, y1);

  snprintf(buffer, sizeof(buffer), "%.1f s", props.zeroSixtyTimeSec);
  DrawPair(gfx, "0-60", buffer, x2, y1);

  snprintf(buffer, sizeof(buffer), "%.1f mph", props.maxSpeedMph);
  DrawPair(gfx, "Max Speed", buffer, x1, y1 + y_pitch);

  snprintf(buffer, sizeof(buffer), "%.1f g", props.maxAccelerationG);
  DrawPair(gfx, "Max Accel", buffer, x2, y1 + y_pitch);
}

}  // namespace quarter_mile
}  // namespace display
