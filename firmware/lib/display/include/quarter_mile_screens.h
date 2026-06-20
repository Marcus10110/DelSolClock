// Quarter-mile drag timer: a group of related screens (start, launch,
// in-progress, summary).
#pragma once

#include <Adafruit_GFX.h>

namespace display {
namespace quarter_mile {

void DrawStart(Adafruit_GFX* gfx);

struct LaunchProps {
  double accelerationG{0};
};
void DrawLaunch(Adafruit_GFX* gfx, const LaunchProps& props);

struct InProgressProps {
  double timeSec{0};
  double accelerationG{0};
  double distanceMiles{0};
  double speedMph{0};
};
void DrawInProgress(Adafruit_GFX* gfx, const InProgressProps& props);

struct SummaryProps {
  double quarterMileTimeSec{0};
  double zeroSixtyTimeSec{0};
  double maxAccelerationG{0};
  double maxSpeedMph{0};
};
void DrawSummary(Adafruit_GFX* gfx, const SummaryProps& props);

}  // namespace quarter_mile
}  // namespace display
