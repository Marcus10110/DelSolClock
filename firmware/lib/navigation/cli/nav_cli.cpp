// nav_cli — desktop test driver for the navigation matcher.
//
// Usage:  nav_cli <route-file>
//   Reads the route from <route-file> (route_io text format).
//   Reads GPS fixes from stdin, one "<lat> <lng>" per line.
//   Writes one result line per fix to stdout:
//     <alongM> <offRouteM> <isOffRoute> <nextManeuverIdx> <toTurnM> <toDestM> "<instruction>" "<roadName>"
//   (isOffRoute is 0/1; the trailing quoted fields are the next maneuver's text
//   for human-reviewable output.)
//
// This binary contains no assertions or synthetic-GPS generation — the TS test
// harness drives it. It exists only to run the *real* shared matcher on desktop.
#include <fstream>
#include <iostream>
#include <string>

#include "matcher.h"
#include "route_io.h"

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "usage: nav_cli <route-file>\n";
    return 2;
  }
  std::ifstream routeFile(argv[1]);
  if (!routeFile) {
    std::cerr << "nav_cli: cannot open route file: " << argv[1] << "\n";
    return 2;
  }
  nav::RouteSummary route;
  if (!nav::readRoute(routeFile, route)) {
    std::cerr << "nav_cli: failed to parse route file\n";
    return 2;
  }

  std::cout.setf(std::ios::fixed);
  std::cout.precision(3);

  double lat, lng;
  while (std::cin >> lat >> lng) {
    nav::GpsFix fix{lat, lng};
    nav::MatchResult r = nav::match(route, fix);

    std::string instruction, roadName;
    if (r.nextManeuverIndex >= 0 &&
        r.nextManeuverIndex < static_cast<int>(route.maneuvers.size())) {
      instruction = route.maneuvers[r.nextManeuverIndex].instruction;
      roadName = route.maneuvers[r.nextManeuverIndex].roadName;
    }

    std::cout << r.distanceAlongRouteMeters << ' ' << r.offRouteDistanceMeters
              << ' ' << (r.isOffRoute ? 1 : 0) << ' ' << r.nextManeuverIndex
              << ' ' << r.distanceToNextTurnMeters << ' '
              << r.distanceToDestinationMeters << " \"" << instruction << "\" \""
              << roadName << "\"\n";
  }
  return 0;
}
