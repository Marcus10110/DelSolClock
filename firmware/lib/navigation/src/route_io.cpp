#include "route_io.h"

#include <string>

namespace nav {
namespace {

// Read a double-quoted string from the stream (skips leading whitespace, then
// expects an opening '"', reads to the next '"'). Returns false on malformed.
bool readQuoted(std::istream& in, std::string& out) {
  char c;
  // skip whitespace
  do {
    if (!in.get(c)) return false;
  } while (c == ' ' || c == '\t' || c == '\r' || c == '\n');
  if (c != '"') return false;
  out.clear();
  while (in.get(c)) {
    if (c == '"') return true;
    out.push_back(c);
  }
  return false;  // unterminated
}

void writeQuoted(std::ostream& out, const std::string& s) {
  out << '"';
  for (char c : s) {
    if (c != '"') out << c;  // drop stray quotes; Mapbox text has none
  }
  out << '"';
}

// Expect a literal keyword token.
bool expectKeyword(std::istream& in, const char* kw) {
  std::string tok;
  if (!(in >> tok)) return false;
  return tok == kw;
}

}  // namespace

bool readRoute(std::istream& in, RouteSummary& out) {
  out = RouteSummary{};

  if (!expectKeyword(in, "base")) return false;
  if (!(in >> out.base.lat >> out.base.lng)) return false;

  if (!expectKeyword(in, "polyline")) return false;
  size_t n = 0;
  if (!(in >> n)) return false;
  out.polyline.resize(n);
  for (size_t i = 0; i < n; ++i) {
    if (!(in >> out.polyline[i].lat >> out.polyline[i].lng)) return false;
  }

  if (!expectKeyword(in, "maneuvers")) return false;
  size_t m = 0;
  if (!(in >> m)) return false;
  out.maneuvers.resize(m);
  for (size_t i = 0; i < m; ++i) {
    RouteManeuver& mv = out.maneuvers[i];
    if (!(in >> mv.polylineIndex >> mv.distanceMeters >> mv.type >> mv.modifier)) {
      return false;
    }
    if (mv.modifier == "-") mv.modifier.clear();
    if (!readQuoted(in, mv.instruction)) return false;
    if (!readQuoted(in, mv.roadName)) return false;
  }

  if (!expectKeyword(in, "totals")) return false;
  if (!(in >> out.totalDistanceMeters >> out.totalDurationSeconds)) return false;

  return true;
}

void writeRoute(std::ostream& out, const RouteSummary& route) {
  out << "base " << route.base.lat << ' ' << route.base.lng << '\n';
  out << "polyline " << route.polyline.size() << '\n';
  for (const auto& p : route.polyline) {
    out << p.lat << ' ' << p.lng << '\n';
  }
  out << "maneuvers " << route.maneuvers.size() << '\n';
  for (const auto& mv : route.maneuvers) {
    out << mv.polylineIndex << ' ' << mv.distanceMeters << ' ' << mv.type << ' '
        << (mv.modifier.empty() ? "-" : mv.modifier) << ' ';
    writeQuoted(out, mv.instruction);
    out << ' ';
    writeQuoted(out, mv.roadName);
    out << '\n';
  }
  out << "totals " << route.totalDistanceMeters << ' '
      << route.totalDurationSeconds << '\n';
}

}  // namespace nav
