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

  // GPS acquisition diagnostics. When gpsHasFix is false the screen shows these
  // instead of location/speed/heading, so you can watch acquisition outdoors
  // (no serial needed). Populated from the GPS receiver / NMEA fields.
  bool gpsHasFix{true};      // valid location fix?
  int gpsSatsInView{0};      // satellites the antenna sees (GSV)
  int gpsSatsUsed{0};        // satellites used in the fix (GGA)
  int gpsFixQuality{0};      // GGA fix quality (0 = none, 1 = GPS, 2 = DGPS)
  unsigned long gpsChars{0}; // NMEA chars processed (rising => data flowing)
};

void DrawStatus(Adafruit_GFX* gfx, const StatusProps& props);

}  // namespace display
