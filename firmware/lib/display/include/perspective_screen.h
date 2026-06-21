// F-Zero / SNES-style perspective route view (work in progress).
//
// Step 1b: render the road bent along a route centerline. The caller supplies the
// upcoming route as points in a CAR-LOCAL frame (meters): +forward = ahead (into
// the screen), +right = to the driver's right. The display lib stays decoupled
// from the navigation lib — the caller (firmware / harness) does the
// route->local-frame conversion and hands us plain geometry.
#pragma once

#include <Adafruit_GFX.h>

#include <vector>

namespace display {

// A point on the route centerline, in car-local meters.
struct CenterlinePoint {
  float forward{0};  // meters ahead of the car
  float right{0};    // meters to the car's right (left is negative)
};

struct PerspectiveProps {
  // Camera / projection tuning (world units in meters).
  float cameraHeightM{3.0f};   // eye height above the road
  float horizonFrac{0.42f};    // horizon line as a fraction of screen height
  float roadHalfWidthM{2.5f};  // half the road width in meters
  float focalPx{80.0f};        // focal length in pixels (controls FOV/steepness)
  float maxDrawDistanceM{120.0f};  // clip the road beyond this distance

  // Upcoming route centerline in car-local meters, ordered from the car forward.
  // Empty => draw a straight road (step 1a behavior).
  std::vector<CenterlinePoint> centerline;
};

void DrawPerspective(Adafruit_GFX* gfx, const PerspectiveProps& props);

}  // namespace display
