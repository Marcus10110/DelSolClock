#include "centerline.h"

#include <cmath>
#include <limits>

#include "geo.h"

namespace nav {
namespace {

// Interpolate a LatLng a fraction t along segment a->b.
LatLng lerp(const LatLng& a, const LatLng& b, double t) {
  return {a.lat + (b.lat - a.lat) * t, a.lng + (b.lng - a.lng) * t};
}

}  // namespace

std::vector<LocalPoint> routeCenterline(const RouteSummary& route,
                                        const LatLng& position,
                                        const CenterlineParams& params) {
  std::vector<LocalPoint> out;
  const auto& poly = route.polyline;
  if (poly.size() < 2) return out;

  // 1. Nearest segment to the position.
  double bestPerp = std::numeric_limits<double>::infinity();
  size_t bestSeg = 0;
  double bestT = 0;
  for (size_t i = 0; i + 1 < poly.size(); ++i) {
    Projection pr = projectToSegment(position, poly[i], poly[i + 1]);
    if (pr.distanceMeters < bestPerp) {
      bestPerp = pr.distanceMeters;
      bestSeg = i;
      bestT = pr.t;
    }
  }

  // 2. Local frame: origin at the projected position, forward = bearing of the
  //    nearest segment, right = forward rotated -90° (east/north plane).
  const LatLng origin = lerp(poly[bestSeg], poly[bestSeg + 1], bestT);
  MetersEN segVec = localOffsetMeters(poly[bestSeg + 1], poly[bestSeg]);
  double flen = std::hypot(segVec.east, segVec.north);
  if (flen < 1e-6) return out;
  const double fE = segVec.east / flen;   // forward unit (east comp)
  const double fN = segVec.north / flen;  // forward unit (north comp)
  const double rE = fN;                   // right = forward rotated -90°
  const double rN = -fE;

  auto toLocal = [&](const LatLng& p) -> LocalPoint {
    MetersEN o = localOffsetMeters(p, origin);
    return {o.east * fE + o.north * fN, o.east * rE + o.north * rN};
  };

  // 3. Walk forward from the projection point, sampling at fixed spacing.
  out.push_back(toLocal(origin));  // the car position itself (~0,0)
  double sampled = params.spacingMeters;
  double walked = 0;               // arc length covered from origin
  LatLng prev = origin;
  for (size_t i = bestSeg + 1; i < poly.size(); ++i) {
    const LatLng cur = poly[i];
    double segLen = distanceMeters(prev, cur);
    while (segLen > 0 && sampled <= walked + segLen) {
      double t = (sampled - walked) / segLen;
      LocalPoint lp = toLocal(lerp(prev, cur, t));
      if (lp.forward > 0) out.push_back(lp);
      sampled += params.spacingMeters;
      if (sampled > params.aheadMeters) return out;
    }
    walked += segLen;
    prev = cur;
    if (walked >= params.aheadMeters) break;
  }
  return out;
}

}  // namespace nav
