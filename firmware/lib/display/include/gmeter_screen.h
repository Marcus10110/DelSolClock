// G-Meter screen: live brake/lateral G dot + scrolling history graphs.
#pragma once

#include <Adafruit_GFX.h>
#include <cstddef>

namespace display {

struct GMeterProps {
  static constexpr size_t HistorySize = 103;

  double brakeLive{0};
  double lateralLive{0};
  const double* brakeHistory{nullptr};    // HistorySize elements
  const double* lateralHistory{nullptr};  // HistorySize elements
};

void DrawGMeter(Adafruit_GFX* gfx, const GMeterProps& props);

}  // namespace display
