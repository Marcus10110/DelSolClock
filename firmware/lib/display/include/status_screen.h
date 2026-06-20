// Status screen: GPS location/speed/heading, battery, and G-forces.
#pragma once

#include <Adafruit_GFX.h>

namespace display {

struct StatusProps {
  double latitude{0};
  double longitude{0};
  double speedMph{0};
  double headingDegrees{0};
  double batteryVolts{0};
  double forwardG{0};
  double lateralG{0};
  double verticalG{0};
};

void DrawStatus(Adafruit_GFX* gfx, const StatusProps& props);

}  // namespace display
