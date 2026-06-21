// Geometry helpers for route matching. Uses a local equirectangular projection
// (meters) anchored per-call, matching the web app's simplify.ts math so the
// C++ matcher and the TS test harness agree on distances at road scale.
#pragma once

#include <vector>

#include "route_summary.h"

namespace nav {

/** Great-circle-ish distance in meters between two lat/lngs (equirectangular). */
double distanceMeters(const LatLng& a, const LatLng& b);

// Local east/north offset of `p` relative to `origin`, in meters (equirect).
struct MetersEN {
  double east{0};
  double north{0};
};
MetersEN localOffsetMeters(const LatLng& p, const LatLng& origin);

struct Projection {
  LatLng point;             // projected point on the segment
  double t{0};              // 0..1 position along the segment a->b
  double distanceMeters{0}; // perpendicular distance from p to the segment
};

/** Project point p onto segment a-b. */
Projection projectToSegment(const LatLng& p, const LatLng& a, const LatLng& b);

/**
 * Cumulative distance (meters) from polyline[0] to each vertex.
 * Returns a vector of size polyline.size(); element 0 is 0.
 */
std::vector<double> cumulativeDistances(const std::vector<LatLng>& polyline);

}  // namespace nav
