// Navigation data overlay drawn ON TOP of the perspective road
// (perspective_screen.h). Readability-first for a 1.14" instrument-cluster
// display read at arm's length: a high-contrast top strip with a big direction
// arrow + distance-to-turn + the street being turned onto, and a thin bottom bar
// with ETA / remaining distance / speed.
//
// Like the perspective renderer, this is DECOUPLED from the navigation lib: the
// caller fills plain fields (direction enum, pre-formatted strings) so the
// display lib has no dependency on nav::RouteSummary.
#pragma once

#include <Adafruit_GFX.h>

#include <string>

namespace display {

// The maneuver direction, mapped from Mapbox type+modifier by the caller. Drives
// which arrow glyph is drawn. Keep this small + display-oriented.
enum class TurnDir {
  Straight,
  SlightLeft,
  Left,
  SharpLeft,
  SlightRight,
  Right,
  SharpRight,
  UTurn,
  Arrive,  // destination — show a flag/marker instead of a turn arrow
  None,    // nothing upcoming (hide the strip)
};

struct NavOverlayProps {
  // --- Top strip (the hero: what to do + when) ---
  TurnDir dir{TurnDir::None};
  std::string distance;  // distance to the maneuver, pre-formatted e.g. "500 ft"
  std::string street;    // street being turned onto, e.g. "Eucalyptus Dr" ("" ok)

  // --- Bottom bar (secondary; each empty string is omitted) ---
  std::string eta;        // arrival clock time, e.g. "5:42"
  std::string remaining;  // distance to destination, e.g. "12.4 mi"
  std::string speed;      // current speed, e.g. "34"  (units drawn as a label)

  bool showBottomBar{true};

  // Daytime theme (dark ink on light bars) for sunlight legibility; false => the
  // night theme (light text on black bars with neon chrome).
  bool daytime{false};
};

// Draw the overlay over whatever is already on `gfx` (does NOT clear). Call after
// DrawPerspective.
void DrawNavOverlay(Adafruit_GFX* gfx, const NavOverlayProps& props);

// ---- Formatting helpers (shared by the firmware + the UiDesigner harness) ---
// These keep the display lib free of nav-lib types: callers pass plain values.

// Map a Mapbox maneuver type + modifier (e.g. "turn"/"left", "arrive") to a
// TurnDir. Unknown/continue/merge => Straight.
TurnDir TurnDirFromManeuver(const std::string& type, const std::string& modifier);

// Format a distance in meters as imperial text: feet in tidy steps under
// ~1000 ft, miles with one decimal above (e.g. "75 ft", "1.2 mi"). Metric
// toggle can come later.
std::string FormatDistanceImperial(double meters);

// Format an ETA clock time from the current wall-clock h:m plus minutes to add,
// 12-hour style without AM/PM (e.g. 17:05 + 37 => "5:42"). minutesToAdd < 0 or
// non-finite => "".
std::string FormatEta(int nowHour24, int nowMinute, double minutesToAdd);

}  // namespace display
