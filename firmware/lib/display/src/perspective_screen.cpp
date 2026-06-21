#include "perspective_screen.h"

#include <algorithm>
#include <cmath>

#include "draw_helpers.h"
#include "image_loader.h"

namespace display {
namespace {

// Synthwave-night palette (RGB565). Dark, high-contrast, glowing — reads well on
// the cluster display and matches the neon reference art.
constexpr uint16_t kSky = 0x1009;        // very dark blue-violet (above horizon)
constexpr uint16_t kSkyHorizon = 0xF81F; // bright magenta horizon seam (glow)
constexpr uint16_t kGround = 0x0805;     // near-black violet (below horizon)
constexpr uint16_t kGrid = 0x500A;       // dim magenta perspective grid
constexpr uint16_t kRoad = 0x18C3;       // dark blue-grey road fill
constexpr uint16_t kRoadFar = 0x10A2;    // road tint near the horizon (gradient)
constexpr uint16_t kEdge = 0xF81F;       // glowing magenta road edges
constexpr uint16_t kCenter = 0x07FF;     // bright cyan centerline dashes
constexpr uint16_t kCity = 0x2009;       // building silhouette (dark violet)
constexpr uint16_t kCityTop = 0x4811;    // lit top edge of buildings (dim magenta)
constexpr uint16_t kWindow = 0xFE60;     // warm-yellow lit windows

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

// Cheap deterministic hash -> [0,1). Stable per virtual column so the skyline is
// identical frame to frame (no stored state, no rand()).
float hash01(int n) {
  uint32_t x = static_cast<uint32_t>(n) * 374761393u + 668265263u;
  x = (x ^ (x >> 13)) * 1274126177u;
  x ^= x >> 16;
  return (x & 0xFFFFFF) / static_cast<float>(0x1000000);
}

// --- Skyline panorama model ------------------------------------------------
// The skyline is a virtual 360° panorama of kSkylineCols columns. Everything is
// a pure function of the (wrapped) column index, so it's stable, wraps, and can
// be drawn either windowed+scrolled (live view) or in full (the 360 export).
constexpr int kSkylineCols = 90;   // columns spanning a full turn (4°/col)
constexpr int kSkylineColW = 12;   // px per column

// SF landmark columns: a triangular Transamerica-like tower + a tall Salesforce-
// like tower. Placed inside the densest downtown clump.
constexpr int kColPyramid = 41;
constexpr int kColTower = 44;

// Mountain range: a contiguous arc of columns rendered as jagged peaks (no
// windows) instead of buildings — "mountains in one direction".
constexpr int kMtnStart = 66;
constexpr int kMtnEnd = 80;  // inclusive

bool colIsMountain(int wrapped) {
  return wrapped >= kMtnStart && wrapped <= kMtnEnd;
}

// Clump density in [0,1] for a column: a sum of smooth bumps centered on a few
// hashed "downtown" columns, so tall buildings cluster and the gaps between
// clumps hold only the occasional lone building. Low frequency => clumpy.
float clumpDensity(int wrapped) {
  // Fixed clump centers (bearings) with per-center width/strength.
  struct Clump { int center; float width; float strength; };
  static const Clump clumps[] = {
      {8, 6.0f, 1.0f},    // a downtown
      {41, 7.0f, 1.0f},   // main downtown (holds the landmarks)
      {54, 4.0f, 0.7f},   // a smaller cluster
      {86, 5.0f, 0.8f},   // wraps near 0
  };
  float d = 0.0f;
  for (const auto& c : clumps) {
    // circular distance on the 90-column ring
    int raw = std::abs(wrapped - c.center);
    int dist = raw < kSkylineCols - raw ? raw : kSkylineCols - raw;
    float t = dist / c.width;
    d += c.strength * std::exp(-t * t);  // gaussian bump
  }
  return d > 1.0f ? 1.0f : d;
}

// Mountain ridge height (px) at a given wrapped column. Peaks are placed at
// varied (hashed) spacings; BETWEEN peaks the ridge only drops to a valley floor
// (a fraction of the peak), then rises into the next peak — so it reads as a
// continuous range, not detached triangles. Returns 0 outside the range.
int mountainHeight(int wrapped, int bandH) {
  if (!colIsMountain(wrapped)) return 0;
  // Both ends of the range must meet the ground, so the silhouette isn't cut off
  // vertically at its edges.
  if (wrapped == kMtnStart || wrapped == kMtnEnd) return 0;

  // One vertex per column, STRICTLY alternating tall/short, so adjacent peaks
  // never merge into a single big triangle — the ridge reads as a saw-tooth.
  // A low-frequency envelope (taller toward the middle) shapes the overall range
  // and tapers both ends to the ground.
  int mid = (kMtnStart + kMtnEnd) / 2;
  int span = (kMtnEnd - kMtnStart) / 2 + 1;
  float center = 1.0f - std::abs(wrapped - mid) / static_cast<float>(span);
  float envelope = 0.35f + 0.65f * center;  // overall ridge shape

  int idx = wrapped - kMtnStart;            // position along the range
  bool tall = (idx % 2) == 1;               // alternate short/TALL/short/...
  float level = tall ? 1.0f : 0.45f;        // strong high/low swing
  // A little per-column jitter so it's not a perfectly regular saw-tooth.
  level *= 0.85f + 0.30f * hash01(wrapped * 911 + 7);

  // Edge taper: 0 at the endpoints, ramping to 1 two columns in.
  int edgeDist = std::min(wrapped - kMtnStart, kMtnEnd - wrapped);
  float taper = edgeDist >= 2 ? 1.0f : edgeDist / 2.0f;

  return static_cast<int>(bandH * envelope * level * taper);
}

// Draw a single panorama column at screen x [bx, bx+colW) into the sky band.
void drawSkylineColumn(Adafruit_GFX* gfx, int wrapped, int bx, int colW,
                       int horizonY, int bandH) {
  // Mountains: a continuous ridge with partial valleys between varied peaks.
  if (colIsMountain(wrapped)) {
    int hL = mountainHeight(wrapped, bandH);
    int hR = mountainHeight(wrapped + 1, bandH);
    if (hL < 2) hL = 2;
    if (hR < 2) hR = 2;
    // Fill the slice as a quad between this column's height and the next's, so
    // adjacent columns join into a continuous ridge line.
    gfx->fillTriangle(bx, horizonY - hL, bx, horizonY, bx + colW, horizonY, kCity);
    gfx->fillTriangle(bx, horizonY - hL, bx + colW, horizonY - hR,
                      bx + colW, horizonY, kCity);
    gfx->drawLine(bx, horizonY - hL, bx + colW, horizonY - hR, kCityTop);
    return;
  }

  const float density = clumpDensity(wrapped);
  const float h01 = hash01(wrapped);
  // Outside clumps most columns are empty; only an occasional lone building.
  const bool present = density > 0.18f || h01 > 0.82f;
  if (!present) return;

  const bool pyramid = wrapped == kColPyramid;
  const bool tower = wrapped == kColTower;

  // Height scales with clump density (tall downtown, short/lone in the gaps),
  // with a strong per-column noise term so a cluster isn't a flat wall.
  const float hNoise = hash01(wrapped * 919 + 17);   // independent height jitter
  float hfrac = 0.20f + 0.60f * density + 0.45f * (hNoise - 0.5f) * density;
  hfrac += 0.10f * (h01 - 0.5f);                       // small extra wobble
  if (hfrac < 0.16f) hfrac = 0.16f;
  if (hfrac > 1.0f) hfrac = 1.0f;
  int bh = static_cast<int>(bandH * hfrac);
  if (tower) bh = bandH;
  if (pyramid) bh = static_cast<int>(bandH * 0.92f);
  if (bh > bandH) bh = bandH;

  // Variable width: most buildings span a fraction of the column, a few are
  // wider blocks. Center the building within its column slot.
  const float wHash = hash01(wrapped * 433 + 5);
  int bw;
  if (pyramid || tower) {
    bw = colW - 2;
  } else {
    bw = static_cast<int>(colW * (0.55f + 0.85f * wHash));  // ~0.55x..1.4x col
  }
  if (bw < 3) bw = 3;
  const int bx2 = bx + (colW - bw) / 2;  // centered in the slot
  const int by = horizonY - bh;

  if (pyramid) {
    gfx->fillTriangle(bx2 + bw / 2, by, bx2, horizonY, bx2 + bw, horizonY, kCity);
    gfx->drawLine(bx2 + bw / 2, by, bx2, horizonY, kCityTop);
    gfx->drawLine(bx2 + bw / 2, by, bx2 + bw, horizonY, kCityTop);
    return;
  }

  gfx->fillRect(bx2, by, bw, bh, kCity);
  gfx->drawFastHLine(bx2, by, bw, kCityTop);
  if (tower) {  // rounded-ish top: clip the corners
    gfx->drawPixel(bx2, by, kCity);
    gfx->drawPixel(bx2 + bw - 1, by, kCity);
  }

  // Scatter a few lit windows, deterministic per building.
  for (int wy = by + 2; wy < horizonY - 1; wy += 3) {
    for (int wx = bx2 + 1; wx < bx2 + bw - 1; wx += 2) {
      if (hash01(wrapped * 131 + wy * 17 + wx) > 0.62f) {
        gfx->drawPixel(wx, wy, kWindow);
      }
    }
  }
}

// Draw the background city skyline in the sky band (between topInset and the
// horizon), panned by heading so the world feels anchored as the car turns.
void drawSkyline(Adafruit_GFX* gfx, int16_t W, int16_t topInset,
                 int16_t horizonY, float headingDeg) {
  const int bandH = horizonY - topInset;
  if (bandH < 6) return;

  // ~kSkylineColW px per 4° column => kSkylineColW/4 px per degree of parallax.
  const float scroll = headingDeg * (kSkylineColW / 4.0f);

  const int firstCol = static_cast<int>((scroll - kSkylineColW) / kSkylineColW) - 1;
  const int lastCol = static_cast<int>((scroll + W + kSkylineColW) / kSkylineColW) + 1;
  for (int col = firstCol; col <= lastCol; ++col) {
    const int wrapped = ((col % kSkylineCols) + kSkylineCols) % kSkylineCols;
    const int x = static_cast<int>(col * kSkylineColW - scroll);
    if (x > W) continue;
    drawSkylineColumn(gfx, wrapped, x, kSkylineColW, horizonY, bandH);
  }
}

// Draw the synthwave ground grid below the horizon: horizontal lines receding to
// the horizon (spaced so they bunch up near it) plus radial lines converging on
// the vanishing point. Uses the same ground projection as the road so the
// perspective is consistent; the road fill is drawn on top afterward.
void drawGrid(Adafruit_GFX* gfx, int16_t W, int16_t H, int16_t centerX,
              int16_t horizonY, const PerspectiveProps& p) {
  // Radial lines first (so horizontals cross on top): evenly spaced in ground-X,
  // all converging on the vanishing point (centerX, horizonY) and fanning out to
  // the bottom of the screen. Project a near plane to get each line's bottom x.
  const float nearZ = p.cameraHeightM * p.focalPx / (H - horizonY);  // z at bottom row
  for (int gx = -9; gx <= 9; ++gx) {
    if (gx == 0) continue;  // center is the road; skip
    const float worldX = gx * 3.0f;  // meters left/right of center
    const float bx = centerX + (worldX / nearZ) * p.focalPx;  // x at bottom edge
    gfx->drawLine(centerX, horizonY, static_cast<int>(bx + 0.5f), H - 1, kGrid);
  }
  // Horizontal lines at increasing ground distances. Start a little out from the
  // horizon so they don't merge into a muddy band right at the vanishing point.
  for (float z = 6.0f; z <= 120.0f; z *= 1.6f) {
    int y = static_cast<int>(horizonY + p.cameraHeightM * p.focalPx / z + 0.5f);
    if (y > horizonY + 2 && y < H) gfx->drawFastHLine(0, y, W, kGrid);
  }
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
  // Background city skyline in the sky band, panned by heading. The top ~36px is
  // covered by the HUD strip, so start the skyline below that.
  if (props.headingDegrees >= 0.0f) {
    drawSkyline(gfx, W, /*topInset=*/36, horizonY, props.headingDegrees);
  }
  // A thin bright line right at the horizon (the synthwave "glow" seam).
  gfx->drawFastHLine(0, horizonY, W, kSkyHorizon);
  drawGrid(gfx, W, H, centerX, horizonY, props);

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
  ScreenPt prevC{0, 0, false};
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
    ScreenPt cen = project(c.forward, c.right, centerX, horizonY, props);
    if (l.valid && r.valid && prevL.valid && prevR.valid) {
      // Fill the road quad spanning this sample's edges to the previous sample's.
      // A general convex-quad fill (not a Y-slice trapezoid) so near-horizontal
      // slices still render. Corners wound around the perimeter:
      //   prevL -> l (far left) -> r (far right) -> prevR (near right) -> prevL.
      // Tint shifts toward kRoadFar with distance for a subtle gradient.
      const uint16_t fill =
          c.forward > props.maxDrawDistanceM * 0.45f ? kRoadFar : kRoad;
      const Vec2 quad[4] = {{prevL.x, prevL.y}, {l.x, l.y},
                            {r.x, r.y}, {prevR.x, prevR.y}};
      fillQuad(gfx, quad, fill);
      // Road boundary = the two LONG sides (left rail prevL->l, right rail
      // prevR->r). These always follow the road, so they never float free the
      // way a far->near cross-link could.
      gfx->drawLine((int)prevL.x, (int)prevL.y, (int)l.x, (int)l.y, kEdge);
      gfx->drawLine((int)prevR.x, (int)prevR.y, (int)r.x, (int)r.y, kEdge);
      // Cyan dashed centerline. Subdivide this segment in WORLD distance (finer
      // than the ~4 m centerline samples) so the dashes have enough resolution
      // to read as dashes; key each sub-step's on/off to its world distance +
      // the animation phase, so dashes are perspective-correct (compress toward
      // the horizon) and flow toward the camera as the phase advances.
      if (i > 0) {
        const CenterlinePoint& pc = centerline[i - 1];
        constexpr float kDashPeriodM = 3.0f;  // on+off cycle every 3 m of road
        constexpr float kStepM = 0.5f;        // sub-step resolution
        const float f0 = pc.forward, f1 = c.forward;
        const float seg = f1 - f0;
        if (seg > 0.01f) {
          int steps = static_cast<int>(seg / kStepM) + 1;
          ScreenPt prevPt{0, 0, false};
          float prevS = 0;
          for (int s = 0; s <= steps; ++s) {
            float t = static_cast<float>(s) / steps;
            float fwd = f0 + seg * t;
            float rgt = pc.right + (c.right - pc.right) * t;
            ScreenPt pt = project(fwd, rgt, centerX, horizonY, props);
            float sdist = fwd + props.centerlinePhaseM;
            if (s > 0 && pt.valid && prevPt.valid) {
              bool on =
                  (static_cast<int>(std::floor(prevS / kDashPeriodM)) & 1) == 0;
              if (on) {
                gfx->drawLine((int)prevPt.x, (int)prevPt.y, (int)pt.x, (int)pt.y,
                              kCenter);
              }
            }
            prevPt = pt;
            prevS = sdist;
          }
        }
      }
    }
    prevL = l;
    prevR = r;
    prevC = cen;
  }

  // Car sprite at bottom-center (rear view), over the road. Magenta is the
  // transparent key. Sized to the placeholder asset; positioned so it rests just
  // above where the bottom HUD bar sits.
  if (!props.carSpritePath.empty()) {
    constexpr int kCarW = 38, kCarH = 27;     // placeholder sprite size
    constexpr uint16_t kCarKey = 0xF01D;      // magenta (244,3,237) in RGB565
    const int16_t spriteX = centerX - kCarW / 2;
    const int16_t spriteY = H - kCarH - 18;   // clearance for the bottom HUD bar
    DrawBMP(gfx, props.carSpritePath.c_str(), spriteX, spriteY,
            /*delete_after_draw=*/false, /*monochrome_color=*/0xFFFF,
            /*transparent_color=*/kCarKey);
  }
}

int PanoramaWidth() { return kSkylineCols * kSkylineColW; }

void DrawSkylinePanorama(Adafruit_GFX* gfx, int16_t horizonY) {
  const int16_t W = gfx->width();
  const int16_t H = gfx->height();
  const int bandH = horizonY;  // buildings rest at horizonY, rise to the top
  gfx->fillRect(0, 0, W, H, kSky);
  gfx->fillRect(0, horizonY, W, H - horizonY, kGround);
  for (int col = 0; col < kSkylineCols; ++col) {
    drawSkylineColumn(gfx, col, col * kSkylineColW, kSkylineColW, horizonY,
                      bandH);
  }
  gfx->drawFastHLine(0, horizonY, W, kSkyHorizon);
}

}  // namespace display
