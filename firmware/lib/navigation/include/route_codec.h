// Binary RouteSummary wire format (little-endian), shared with the web app's
// webapp/src/navigation/routeCodec.ts. Coordinates are int32 = round(deg * 1e6).
// Instruction/road-name text is NOT carried in this format (the matcher doesn't
// need it; display text arrives in a later phase). type/modifier are codes from
// maneuver_codes.h.
//
// Layout:
//   u16 magic = 0x4453 ('DS')
//   u8  version = 1
//   u8  flags = 0
//   i32 baseLat, i32 baseLng           (== polyline[0])
//   u16 ptCount
//     { i32 lat, i32 lng } x ptCount
//   u16 mvCount
//     { u16 polylineIndex, u32 distanceMeters, u8 typeCode, u8 modifierCode,
//       u16 instrLen, instr bytes, u16 roadLen, road bytes } x mvCount
//   u32 totalDistanceMeters
//   u32 totalDurationSeconds
#pragma once

#include <cstddef>
#include <cstdint>

#include "route_summary.h"

namespace nav {

constexpr uint16_t kRouteMagic = 0x4453;  // 'D','S'
constexpr uint8_t kRouteVersion = 2;  // v2 adds per-maneuver instruction/roadName
constexpr double kCoordScale = 1e6;

/** Decode the binary route format into `out`. Returns false on malformed input. */
bool decodeRoute(const uint8_t* data, size_t len, RouteSummary& out);

}  // namespace nav
