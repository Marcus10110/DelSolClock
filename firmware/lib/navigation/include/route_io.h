// Read/write the simple text route format used by the desktop nav_cli + the TS
// test harness. Stream-friendly (operator>>), with double-quoted instruction /
// road-name fields so the data is human-reviewable.
//
// Format:
//   base <lat> <lng>
//   polyline <N>
//   <lat> <lng>           (x N)
//   maneuvers <M>
//   <idx> <distM> <type> <modifier> "<instruction>" "<roadName>"   (x M)
//   totals <distM> <durS>
#pragma once

#include <istream>
#include <ostream>

#include "route_summary.h"

namespace nav {

/** Parse a RouteSummary from the text format. Returns false on malformed input. */
bool readRoute(std::istream& in, RouteSummary& out);

/** Write a RouteSummary in the text format. */
void writeRoute(std::ostream& out, const RouteSummary& route);

}  // namespace nav
