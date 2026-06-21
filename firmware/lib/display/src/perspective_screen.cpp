#include "perspective_screen.h"

#include <cmath>

#include "draw_helpers.h"

namespace display {
namespace {

// RGB565 colors (step 1b placeholder palette).
constexpr uint16_t kSky = 0x2104;     // very dark blue-grey
constexpr uint16_t kGround = 0x0841;  // near-black ground
constexpr uint16_t kRoad = 0x4208;    // mid grey road
constexpr uint16_t kEdge = 0xFFFF;    // white road edges

struct ScreenPt {
  float x;
  float y;
  bool valid;  // false if behind/at the camera or beyond horizon
};

// Project a car-local point (forward z meters, right x meters) to screen pixels.
ScreenPt project(float forward, float right, int16_t centerX, int16_t horizonY,
                 const PerspectiveProps& p) {
  if (forward < 0.5f) return {0, 0, false};  // at/behind the camera
  const float y = horizonY + p.cameraHeightM * p.focalPx / forward;
  const float x = centerX + (right / forward) * p.focalPx;
  return {x, y, true};
}

struct Vec2 { float x; float y; };

// Fill an arbitrary convex quad (corners in order) by scanline. Works for any
// orientation — including near-horizontal road slices where the two far/near
// edges share almost the same Y (a Y-slice trapezoid fill would collapse those
// to nothing). For each scanline we intersect all four edges and span between
// the min/max crossings.
void fillQuad(Adafruit_GFX* gfx, const Vec2 q[4], uint16_t fill) {
  float minYf = q[0].y, maxYf = q[0].y;
  for (int i = 1; i < 4; ++i) {
    if (q[i].y < minYf) minYf = q[i].y;
    if (q[i].y > maxYf) maxYf = q[i].y;
  }
  // Include every row the quad touches: floor(min)..ceil(max). Rounding to the
  // nearest row (as before) dropped the top and bottom partial rows, leaving a
  // thin gap above the fill and a gap between the nearest slice and the bottom.
  int y0 = static_cast<int>(std::floor(minYf));
  int y1 = static_cast<int>(std::ceil(maxYf));
  if (y0 < 0) y0 = 0;
  if (y1 > gfx->height() - 1) y1 = gfx->height() - 1;
  const int maxX = gfx->width() - 1;
  for (int y = y0; y <= y1; ++y) {
    const float yc = static_cast<float>(y) + 0.5f;
    float xMin = 1e9f, xMax = -1e9f;
    bool hit = false;
    for (int i = 0; i < 4; ++i) {
      const Vec2& a = q[i];
      const Vec2& b = q[(i + 1) % 4];
      const float lo = a.y < b.y ? a.y : b.y;
      const float hi = a.y < b.y ? b.y : a.y;
      if (yc < lo || yc > hi || lo == hi) continue;  // scanline misses edge
      const float t = (yc - a.y) / (b.y - a.y);
      const float x = a.x + (b.x - a.x) * t;
      if (x < xMin) xMin = x;
      if (x > xMax) xMax = x;
      hit = true;
    }
    if (!hit) continue;
    int il = static_cast<int>(xMin + 0.5f);
    int ir = static_cast<int>(xMax + 0.5f);
    if (il < 0) il = 0;
    if (ir > maxX) ir = maxX;
    if (ir >= il) gfx->drawFastHLine(il, y, ir - il + 1, fill);
  }
}

// Step 1a fallback: a straight road centerline if none supplied.
std::vector<CenterlinePoint> straightCenterline(const PerspectiveProps& p) {
  std::vector<CenterlinePoint> c;
  for (float d = 1.0f; d <= p.maxDrawDistanceM; d *= 1.25f) {
    c.push_back({d, 0.0f});
  }
  return c;
}

// The smallest forward distance the projection still draws (must stay above the
// project() cull). At this distance the road reaches well below the bottom edge,
// so the near end of the ribbon meets the screen bottom.
constexpr float kNearForwardM = 0.5f;

// Prepend a point extrapolated back toward the camera so the road's near edge
// reaches the bottom of the screen instead of floating up at the first drawable
// sample (~spacing meters out). The route centerline typically starts with the
// car position itself at forward~0, which the projection culls; we find the first
// two DRAWABLE points (forward >= kNearForwardM), drop everything nearer than
// that, and extrapolate their line back to kNearForwardM. Returned unchanged if
// it can't (fewer than two drawable points, or not increasing in forward).
std::vector<CenterlinePoint> withNearEdge(
    const std::vector<CenterlinePoint>& in) {
  // First point at/after the near plane.
  size_t a = 0;
  while (a < in.size() && in[a].forward < kNearForwardM) ++a;
  if (a + 1 >= in.size()) return in;  // need a second drawable point
  const CenterlinePoint& pA = in[a];
  const CenterlinePoint& pB = in[a + 1];
  const float df = pB.forward - pA.forward;
  if (df <= 0.0f) return in;  // not increasing forward; leave as-is
  // If pA already sits at the near plane, nothing to extrapolate.
  if (pA.forward <= kNearForwardM + 1e-3f) return in;
  // Parameterize the pA->pB line by forward; solve for right at kNearForwardM.
  const float t = (kNearForwardM - pA.forward) / df;  // < 0 (extrapolate back)
  CenterlinePoint near{kNearForwardM, pA.right + (pB.right - pA.right) * t};
  std::vector<CenterlinePoint> out;
  out.reserve(in.size() - a + 1);
  out.push_back(near);
  out.insert(out.end(), in.begin() + a, in.end());  // drop the culled points
  return out;
}

}  // namespace

void DrawPerspective(Adafruit_GFX* gfx, const PerspectiveProps& props) {
  Clear(gfx);
  const int16_t W = gfx->width();
  const int16_t H = gfx->height();
  const int16_t centerX = W / 2;
  const int16_t horizonY = static_cast<int16_t>(H * props.horizonFrac);

  gfx->fillRect(0, 0, W, horizonY, kSky);
  gfx->fillRect(0, horizonY, W, H - horizonY, kGround);

  const std::vector<CenterlinePoint> centerline = withNearEdge(
      props.centerline.empty() ? straightCenterline(props) : props.centerline);
  const float hw = props.roadHalfWidthM;

  // Project each centerline sample's left + right edge, then fill the road as a
  // strip of trapezoids between consecutive (valid) samples. The width is offset
  // PERPENDICULAR to the local road direction (the tangent to neighboring
  // points) rather than always along +right. This gives a road that crosses the
  // view (e.g. heading left/right just ahead) real depth-extent — without it,
  // such a road collapses to a single horizontal line because its width axis
  // would point straight into the screen.
  ScreenPt prevL{0, 0, false}, prevR{0, 0, false};
  for (size_t i = 0; i < centerline.size(); ++i) {
    const CenterlinePoint& c = centerline[i];
    if (c.forward > props.maxDrawDistanceM) break;

    // Local road direction (tangent) in (forward, right) space, from the
    // neighboring points. Endpoints use the one-sided neighbor.
    const CenterlinePoint& nxt = centerline[i + 1 < centerline.size() ? i + 1 : i];
    const CenterlinePoint& prv = centerline[i > 0 ? i - 1 : i];
    float tf = nxt.forward - prv.forward;  // tangent (forward comp)
    float tr = nxt.right - prv.right;      // tangent (right comp)
    float tlen = std::sqrt(tf * tf + tr * tr);
    if (tlen < 1e-4f) { tf = 1.0f; tr = 0.0f; tlen = 1.0f; }  // degenerate
    tf /= tlen;
    tr /= tlen;
    // Perpendicular to the driver's right of travel: rotate tangent -90°.
    float pf = tr;   // perpendicular (forward comp)
    float pr = -tf;  // perpendicular (right comp)
    // The near edge (i==0) exists only to extend the road to the bottom of the
    // screen; offset it purely along +right so its base is HORIZONTAL (a tilted
    // perpendicular here makes the road's base look notched/slanted).
    if (i == 0) { pf = 0.0f; pr = -1.0f; }

    ScreenPt l = project(c.forward - pf * hw, c.right - pr * hw, centerX, horizonY, props);
    ScreenPt r = project(c.forward + pf * hw, c.right + pr * hw, centerX, horizonY, props);
    if (l.valid && r.valid && prevL.valid && prevR.valid) {
      // Fill the road quad spanning this sample's edges to the previous sample's.
      // A general convex-quad fill (not a Y-slice trapezoid) so near-horizontal
      // slices still render. Corners wound around the perimeter:
      //   prevL -> l (far left) -> r (far right) -> prevR (near right) -> prevL.
      const Vec2 quad[4] = {{prevL.x, prevL.y}, {l.x, l.y},
                            {r.x, r.y}, {prevR.x, prevR.y}};
      fillQuad(gfx, quad, kRoad);
      // Road boundary = the two LONG sides (left rail prevL->l, right rail
      // prevR->r). These always follow the road, so they never float free the
      // way a far->near cross-link could.
      gfx->drawLine((int)prevL.x, (int)prevL.y, (int)l.x, (int)l.y, kEdge);
      gfx->drawLine((int)prevR.x, (int)prevR.y, (int)r.x, (int)r.y, kEdge);
    }
    prevL = l;
    prevR = r;
  }
}

}  // namespace display
