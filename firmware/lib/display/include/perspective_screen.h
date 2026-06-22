// F-Zero / SNES-style perspective route view (work in progress).
//
// Step 1b: render the road bent along a route centerline. The caller supplies the
// upcoming route as points in a CAR-LOCAL frame (meters): +forward = ahead (into
// the screen), +right = to the driver's right. The display lib stays decoupled
// from the navigation lib — the caller (firmware / harness) does the
// route->local-frame conversion and hands us plain geometry.
#pragma once

#include <Adafruit_GFX.h>

#include <string>
#include <vector>

namespace display {

// A point on the route centerline, in car-local meters.
struct CenterlinePoint {
  float forward{0};  // meters ahead of the car
  float right{0};    // meters to the car's right (left is negative)
};

struct PerspectiveProps {
  // Daytime palette: lightened "daytime synthwave" (bright sky, dark ink) for
  // sunlight legibility. false => the dark glowing night palette.
  bool daytime{false};

  // Camera / projection tuning (world units in meters).
  float cameraHeightM{3.0f};   // eye height above the road
  float horizonFrac{0.42f};    // horizon line as a fraction of screen height
  float roadHalfWidthM{2.5f};  // half the road width in meters
  float focalPx{80.0f};        // focal length in pixels (controls FOV/steepness)
  float maxDrawDistanceM{120.0f};  // clip the road beyond this distance

  // Current compass heading in degrees (0 = N, 90 = E). Pans the background city
  // skyline so the world feels anchored as the car turns. Negative => no skyline
  // (the caller can disable it when heading is unknown).
  float headingDegrees{-1.0f};

  // Centerline dash animation: a phase offset in meters that the caller advances
  // over time (faster = dashes flow toward you faster). The dash pattern is keyed
  // to world distance + this phase, so it scrolls smoothly and stays perspective-
  // correct. 0 => static dashes (no animation).
  float centerlinePhaseM{0.0f};

  // Upcoming route centerline in car-local meters, ordered from the car forward.
  // Empty => draw a straight road (step 1a behavior).
  std::vector<CenterlinePoint> centerline;

  // Car sprite drawn at the bottom-center (rear view), over the road. Empty =>
  // none. The sprite's magenta key color is treated as transparent.
  std::string carSpritePath;
};

void DrawPerspective(Adafruit_GFX* gfx, const PerspectiveProps& props);

// Render the full 360° skyline panorama into `gfx` for review (UiDesigner). The
// panorama is PanoramaWidth() px wide; `gfx` should be that wide and tall enough
// for the sky band. `horizonY` is where buildings rest (their bases). Buildings
// are drawn from a top inset of 0 down to horizonY.
int PanoramaWidth();
void DrawSkylinePanorama(Adafruit_GFX* gfx, int16_t horizonY);

}  // namespace display
