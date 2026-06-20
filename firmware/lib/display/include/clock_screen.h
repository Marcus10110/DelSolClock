// Clock screen: time, AM/PM, now-playing title, speed, headlight + bluetooth.
#pragma once

#include <Adafruit_GFX.h>
#include <cstdint>

namespace display {

struct ClockProps {
  uint8_t hours24{0};
  uint8_t minutes{0};
  float speedMph{0};
  bool headlight{false};
  bool bluetooth{false};
  const char* mediaTitle{""};
};

void DrawClock(Adafruit_GFX* gfx, const ClockProps& props);

}  // namespace display
