#include "geo.h"

#include <cmath>

namespace nav {
namespace {
constexpr double kEarthRadiusM = 6371008.8;
constexpr double kDegToRad = 3.14159265358979323846 / 180.0;

// Local equirectangular offset of p relative to origin, in meters.
struct XY {
  double x;
  double y;
};
XY toLocal(const LatLng& p, const LatLng& origin) {
  const double latRad = origin.lat * kDegToRad;
  const double x = (p.lng - origin.lng) * kDegToRad * std::cos(latRad) * kEarthRadiusM;
  const double y = (p.lat - origin.lat) * kDegToRad * kEarthRadiusM;
  return {x, y};
}
}  // namespace

double distanceMeters(const LatLng& a, const LatLng& b) {
  const XY d = toLocal(b, a);
  return std::hypot(d.x, d.y);
}

Projection projectToSegment(const LatLng& p, const LatLng& a, const LatLng& b) {
  // Work in meters local to `a`.
  const XY pa = toLocal(p, a);
  const XY ba = toLocal(b, a);
  const double segLenSq = ba.x * ba.x + ba.y * ba.y;

  double t = 0.0;
  if (segLenSq > 0.0) {
    t = (pa.x * ba.x + pa.y * ba.y) / segLenSq;
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
  }

  const double projX = ba.x * t;
  const double projY = ba.y * t;
  const double perp = std::hypot(pa.x - projX, pa.y - projY);

  // Convert the projected local offset back to lat/lng.
  const double latRad = a.lat * kDegToRad;
  LatLng projected;
  projected.lat = a.lat + (projY / kEarthRadiusM) / kDegToRad;
  projected.lng = a.lng + (projX / (kEarthRadiusM * std::cos(latRad))) / kDegToRad;

  return {projected, t, perp};
}

std::vector<double> cumulativeDistances(const std::vector<LatLng>& polyline) {
  std::vector<double> cum(polyline.size(), 0.0);
  for (size_t i = 1; i < polyline.size(); ++i) {
    cum[i] = cum[i - 1] + distanceMeters(polyline[i - 1], polyline[i]);
  }
  return cum;
}

}  // namespace nav
