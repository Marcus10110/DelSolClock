// C++ mirror of the web app's RouteSummary (webapp/src/navigation/types.ts).
// The on-device matcher consumes this. Populated on desktop from a text file
// (route_io) and, in a later phase, from a BLE download on the firmware.
#pragma once

#include <string>
#include <vector>

namespace nav {

struct LatLng {
  double lat{0};
  double lng{0};
};

struct RouteManeuver {
  int polylineIndex{0};      // index into RouteSummary::polyline
  double distanceMeters{0};  // length of the step beginning at this maneuver
  std::string type;          // Mapbox maneuver type, e.g. "turn", "arrive"
  std::string modifier;      // e.g. "left", "slight right"; "" if none
  std::string instruction;   // human-readable, e.g. "Turn left onto Eucalyptus Dr"
  std::string roadName;      // e.g. "Eucalyptus Dr"; "" if unknown
};

struct RouteSummary {
  LatLng base;                          // anchor (== polyline[0])
  std::vector<LatLng> polyline;         // ordered, start -> destination
  std::vector<RouteManeuver> maneuvers; // ordered; each references a polyline index
  double totalDistanceMeters{0};
  double totalDurationSeconds{0};
};

}  // namespace nav
