#include "route_codec.h"

#include <string>

#include "maneuver_codes.h"

namespace nav {
namespace {

// Little-endian cursor reader with bounds checking. `ok` latches false on overrun.
struct Reader {
  const uint8_t* p;
  size_t remaining;
  bool ok{true};

  uint8_t u8() {
    if (remaining < 1) { ok = false; return 0; }
    uint8_t v = p[0];
    p += 1; remaining -= 1;
    return v;
  }
  uint16_t u16() {
    if (remaining < 2) { ok = false; return 0; }
    uint16_t v = static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
    p += 2; remaining -= 2;
    return v;
  }
  uint32_t u32() {
    if (remaining < 4) { ok = false; return 0; }
    uint32_t v = static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
                 (static_cast<uint32_t>(p[2]) << 16) |
                 (static_cast<uint32_t>(p[3]) << 24);
    p += 4; remaining -= 4;
    return v;
  }
  int32_t i32() { return static_cast<int32_t>(u32()); }

  // Length-prefixed (u16) UTF-8 string.
  std::string str() {
    uint16_t n = u16();
    if (!ok || remaining < n) { ok = false; return std::string(); }
    std::string s(reinterpret_cast<const char*>(p), n);
    p += n; remaining -= n;
    return s;
  }
};

}  // namespace

bool decodeRoute(const uint8_t* data, size_t len, RouteSummary& out) {
  out = RouteSummary{};
  Reader r{data, len};

  if (r.u16() != kRouteMagic) return false;
  if (r.u8() != kRouteVersion) return false;
  r.u8();  // flags (ignored)

  out.base.lat = r.i32() / kCoordScale;
  out.base.lng = r.i32() / kCoordScale;

  uint16_t ptCount = r.u16();
  if (!r.ok) return false;
  out.polyline.reserve(ptCount);
  for (uint16_t i = 0; i < ptCount; ++i) {
    LatLng p;
    p.lat = r.i32() / kCoordScale;
    p.lng = r.i32() / kCoordScale;
    if (!r.ok) return false;
    out.polyline.push_back(p);
  }

  uint16_t mvCount = r.u16();
  if (!r.ok) return false;
  out.maneuvers.reserve(mvCount);
  for (uint16_t i = 0; i < mvCount; ++i) {
    RouteManeuver m;
    m.polylineIndex = r.u16();
    m.distanceMeters = static_cast<double>(r.u32());
    uint8_t typeCode = r.u8();
    uint8_t modifierCode = r.u8();
    m.type = maneuverTypeFromCode(typeCode);
    m.modifier = maneuverModifierFromCode(modifierCode);
    m.instruction = r.str();
    m.roadName = r.str();
    if (!r.ok) return false;
    out.maneuvers.push_back(m);
  }

  out.totalDistanceMeters = static_cast<double>(r.u32());
  out.totalDurationSeconds = static_cast<double>(r.u32());

  if (!r.ok) return false;
  // The base anchor should equal polyline[0]; if no points, that's malformed.
  if (out.polyline.empty()) return false;
  return true;
}

}  // namespace nav
