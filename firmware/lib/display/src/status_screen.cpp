#include "status_screen.h"

#include <cmath>
#include <cstdio>

#include "draw_helpers.h"
#include "fonts.h"

namespace display {
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

const char* HeadingToDirection(float heading) {
  static const char* directions[] = {"N", "NE", "E", "SE",
                                     "S", "SW", "W", "NW"};
  if (heading < 0.0f || heading >= 360.0f) {
    return "Invalid";
  }
  int index = (static_cast<int>(heading + 22.5f) % 360) / 45;
  return directions[index];
}

}  // namespace

void DrawStatus(Adafruit_GFX* gfx, const StatusProps& props) {
  Clear(gfx);

  // Two columns + three rows, laid out inside the bezel-aware drawable region so
  // everything reflows when the bezel insets change. x1/x2 are the left edges of
  // the two columns; y1 is the baseline of the first row's title.
  const Rect screen = ScreenRect();
  const int x1 = screen.x;
  const int x2 = screen.x + screen.w / 2;
  const int y1 = screen.y + 15;
  const int y_pitch = 42;

  char buffer[128];

  if (props.gpsHasFix) {
    // Normal: GPS location / speed / heading.
    char ns = props.latitude > 0 ? 'N' : 'S';
    char ew = props.longitude > 0 ? 'E' : 'W';
    snprintf(buffer, sizeof(buffer), "%2.4f %c", std::abs(props.latitude), ns);
    DrawPair(gfx, "Latitude", buffer, x1, y1);

    snprintf(buffer, sizeof(buffer), "%3.4f %c", std::abs(props.longitude), ew);
    DrawPair(gfx, "Longitude", buffer, x2, y1);

    snprintf(buffer, sizeof(buffer), "%3.1f mph", props.speedMph);
    DrawPair(gfx, "Speed", buffer, x1, y1 + y_pitch);

    DrawPair(gfx, "Heading",
             HeadingToDirection(static_cast<float>(props.headingDegrees)), x2,
             y1 + y_pitch);
  } else {
    // No fix yet: show GPS acquisition diagnostics in place of location so you
    // can watch satellites come in outdoors. "In view" is the key field — it
    // should climb as the antenna picks up satellites; a fix needs >= 4 used.
    snprintf(buffer, sizeof(buffer), "%d", props.gpsSatsInView);
    DrawPair(gfx, "Sats View", buffer, x1, y1);

    snprintf(buffer, sizeof(buffer), "%d", props.gpsSatsUsed);
    DrawPair(gfx, "Sats Used", buffer, x2, y1);

    snprintf(buffer, sizeof(buffer), "%d", props.gpsFixQuality);
    DrawPair(gfx, "Fix Q", buffer, x1, y1 + y_pitch);

    snprintf(buffer, sizeof(buffer), "%lu", props.gpsChars);
    DrawPair(gfx, "NMEA Bytes", buffer, x2, y1 + y_pitch, true);
  }

  snprintf(buffer, sizeof(buffer), "%2.1f V", props.batteryVolts);
  DrawPair(gfx, "Battery", buffer, x1, y1 + y_pitch * 2);

  snprintf(buffer, sizeof(buffer), "% 1.1f/% 1.1f/% 1.1f", props.forwardG,
           props.lateralG, props.verticalG);
  DrawPair(gfx, "G-Force", buffer, x2, y1 + y_pitch * 2, true);
}

}  // namespace display
