// Convert a route + current position into an upcoming road centerline in the
// car-local frame (meters): +forward = ahead along the route, +right = to the
// driver's right. This bridges the navigation route to the perspective renderer
// (which is display-only and takes plain {forward,right} geometry).
#pragma once

#include <vector>

#include "route_summary.h"

namespace nav {

struct LocalPoint {
  double forward{0};  // meters ahead of the car
  double right{0};    // meters to the car's right
};

struct CenterlineParams {
  double aheadMeters{120.0};   // how far ahead to sample
  double spacingMeters{4.0};   // sample spacing along the route
};

/**
 * Sample the route ahead of `position` into the car-local frame. "Forward" is
 * the route's bearing at the nearest point to `position`. Returns points ordered
 * from the car outward. Empty if the route has < 2 points.
 */
std::vector<LocalPoint> routeCenterline(const RouteSummary& route,
                                        const LatLng& position,
                                        const CenterlineParams& params = {});

}  // namespace nav
