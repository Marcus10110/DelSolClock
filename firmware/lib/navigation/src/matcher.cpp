#include "matcher.h"

#include <limits>

#include "geo.h"

namespace nav {

MatchResult match(const RouteSummary& route, const GpsFix& fix) {
  MatchResult result;
  const auto& poly = route.polyline;
  if (poly.empty()) {
    return result;  // nothing to match against
  }
  const LatLng p{fix.lat, fix.lng};

  if (poly.size() == 1) {
    result.projected = poly[0];
    result.offRouteDistanceMeters = distanceMeters(p, poly[0]);
    result.isOffRoute = result.offRouteDistanceMeters > kOffRouteThresholdMeters;
    result.nextManeuverIndex = route.maneuvers.empty() ? -1 : 0;
    return result;
  }

  const std::vector<double> cum = cumulativeDistances(poly);

  // 1. Global nearest-segment projection.
  double bestPerp = std::numeric_limits<double>::infinity();
  size_t bestSeg = 0;     // segment poly[i]..poly[i+1]
  Projection bestProj;
  for (size_t i = 0; i + 1 < poly.size(); ++i) {
    Projection proj = projectToSegment(p, poly[i], poly[i + 1]);
    if (proj.distanceMeters < bestPerp) {
      bestPerp = proj.distanceMeters;
      bestSeg = i;
      bestProj = proj;
    }
  }

  result.projected = bestProj.point;
  result.offRouteDistanceMeters = bestPerp;
  result.isOffRoute = bestPerp > kOffRouteThresholdMeters;

  // 2. Distance along the route to the projection: cumulative distance to the
  //    segment's start vertex, plus the partial distance into the segment.
  const double segLen = cum[bestSeg + 1] - cum[bestSeg];
  const double alongInSeg = bestProj.t * segLen;
  result.distanceAlongRouteMeters = cum[bestSeg] + alongInSeg;

  // 3. Distance to destination = remaining arc length.
  const double total = cum.back();
  result.distanceToDestinationMeters = total - result.distanceAlongRouteMeters;
  if (result.distanceToDestinationMeters < 0) result.distanceToDestinationMeters = 0;

  // 4. Next maneuver = first maneuver whose polyline position is ahead of us.
  //    (A maneuver's along-route distance is the cumulative distance at its
  //    polylineIndex.) The projection sits at/after bestSeg, so a maneuver is
  //    "ahead" if its index > bestSeg, or it's on bestSeg+ with more arc length.
  result.nextManeuverIndex = -1;
  for (size_t m = 0; m < route.maneuvers.size(); ++m) {
    int idx = route.maneuvers[m].polylineIndex;
    if (idx < 0 || idx >= static_cast<int>(cum.size())) continue;
    const double maneuverAlong = cum[idx];
    if (maneuverAlong > result.distanceAlongRouteMeters + 1e-6) {
      result.nextManeuverIndex = static_cast<int>(m);
      result.distanceToNextTurnMeters =
          maneuverAlong - result.distanceAlongRouteMeters;
      break;
    }
  }
  // Past the last turn: aim at the destination (typically the "arrive" maneuver).
  if (result.nextManeuverIndex == -1 && !route.maneuvers.empty()) {
    result.nextManeuverIndex = static_cast<int>(route.maneuvers.size()) - 1;
    result.distanceToNextTurnMeters = result.distanceToDestinationMeters;
  }

  return result;
}

}  // namespace nav
