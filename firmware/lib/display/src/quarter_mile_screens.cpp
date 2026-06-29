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

void DrawProgressBar(Adafruit_GFX* gfx, uint16_t y, double percentage) {
  const Rect screen = ScreenRect();
  const int16_t h = 7;
  const int16_t x = screen.x;
  const int16_t w = screen.w;
  const int16_t bar_w = static_cast<int16_t>(percentage * w);
  gfx->fillRect(x, y, w, h, 0xFFFF);
  gfx->fillRect(x, y, bar_w, h, 0xF800);
  gfx->fillRect(x + bar_w, y - 4, 2, h + 8, 0xF800);
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
  const Rect screen = ScreenRect();
  Rect second_region;
  second_region.x = screen.x;
  second_region.y = top_region.h + top_region.y + 10;
  second_region.w = screen.w;
  second_region.h = screen.y + screen.h - second_region.y;
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

  const Rect screen = ScreenRect();
  const int x1 = screen.x;
  const int x2 = screen.x + screen.w / 2;
  const int y1 = screen.y + 40;
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
  // Bar sits near the bottom of the visible area.
  DrawProgressBar(gfx, VisibleBottom() - 16, distance / 0.25);
}

void DrawSummary(Adafruit_GFX* gfx, const SummaryProps& props) {
  Clear(gfx);
  gfx->setFont(&JetBrainsMono_Thin14pt7b);
  WriteAligned(gfx, "Quarter Mile", HAlign::Center, VAlign::Top);

  const Rect screen = ScreenRect();
  const int x1 = screen.x;
  const int x2 = screen.x + screen.w / 2;
  const int y1 = screen.y + 45;
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
