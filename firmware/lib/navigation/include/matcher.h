// Stateless route matcher: given a RouteSummary and a single GPS fix, compute
// where we are along the route, the next maneuver, distances, and off-route
// status. v1 is intentionally stateless (no memory of prior fixes) — forward-
// window, off-route hysteresis, arrival latch, and heading hints are deferred.
#pragma once

#include "route_summary.h"

namespace nav {

// Off-route when perpendicular distance to the route exceeds this.
constexpr double kOffRouteThresholdMeters = 50.0;

struct GpsFix {
  double lat{0};
  double lng{0};
  // course/speed reserved for a future stateful matcher.
};

struct MatchResult {
  LatLng projected;                    // nearest point on the route
  double distanceAlongRouteMeters{0};  // arc length from start to projection
  double offRouteDistanceMeters{0};    // perpendicular distance to route (always)
  bool isOffRoute{false};              // offRouteDistanceMeters > threshold
  int nextManeuverIndex{-1};           // index into RouteSummary::maneuvers, -1 if none
  double distanceToNextTurnMeters{0};
  double distanceToDestinationMeters{0};
};

/** Match a single GPS fix against the route. Stateless / pure. */
MatchResult match(const RouteSummary& route, const GpsFix& fix);

}  // namespace nav
