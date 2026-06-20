// Shared maneuver type/modifier <-> small-int mappings for the binary route wire
// format. MUST stay in sync with webapp/src/navigation/maneuverCodes.ts (same
// order = same codes). Unknown codes map back to "unknown"/"" .
#pragma once

#include <cstdint>
#include <string>

namespace nav {

// Index = wire code. Order must match maneuverCodes.ts MANEUVER_TYPES.
inline const char* maneuverTypeFromCode(uint8_t code) {
  static const char* kTypes[] = {
      "unknown",   "turn",        "new name",       "depart",
      "arrive",    "merge",       "on ramp",        "off ramp",
      "fork",      "end of road", "continue",       "roundabout",
      "rotary",    "roundabout turn", "notification", "exit roundabout",
      "exit rotary",
  };
  constexpr uint8_t n = sizeof(kTypes) / sizeof(kTypes[0]);
  return code < n ? kTypes[code] : kTypes[0];
}

// Order must match maneuverCodes.ts MANEUVER_MODIFIERS. Code 0 = "" (none).
inline const char* maneuverModifierFromCode(uint8_t code) {
  static const char* kModifiers[] = {
      "",           "uturn",      "sharp right", "right",     "slight right",
      "straight",   "slight left", "left",       "sharp left",
  };
  constexpr uint8_t n = sizeof(kModifiers) / sizeof(kModifiers[0]);
  return code < n ? kModifiers[code] : kModifiers[0];
}

}  // namespace nav
