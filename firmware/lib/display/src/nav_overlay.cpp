#include "nav_overlay.h"

#include <cmath>

#include "draw_helpers.h"
#include "fonts.h"

namespace display {
namespace {

// High-contrast palette. The strips are near-black so light text/arrows pop
// against both the road and the sky.
constexpr uint16_t kStripBg = 0x0000;     // solid black backing
constexpr uint16_t kStripLine = 0x4208;   // thin separator line (mid grey)
constexpr uint16_t kFg = 0xFFFF;          // white text/arrows (max contrast)
constexpr uint16_t kAccent = 0xFD20;      // amber accent for distance (warm, pops)
constexpr uint16_t kDim = 0xC618;         // light grey for secondary labels

constexpr int16_t kStripH = 34;  // top strip height
constexpr int16_t kBarH = 14;    // bottom bar height

// ---- Direction arrow -------------------------------------------------------
// Drawn as a chunky filled arrow inside a box centred at (cx, cy) with the given
// half-size, rotated to the maneuver direction. Filled shapes keep it crisp and
// high-contrast at arm's length (no thin strokes).

// Rotate (x,y) about origin by angleDeg (screen coords: +y down), return ints.
void rot(float x, float y, float angleDeg, float cx, float cy, int16_t& ox,
         int16_t& oy) {
  const float a = angleDeg * 3.14159265f / 180.0f;
  const float c = std::cos(a), s = std::sin(a);
  ox = static_cast<int16_t>(cx + (x * c - y * s) + 0.5f);
  oy = static_cast<int16_t>(cy + (x * s + y * c) + 0.5f);
}

// Draw a thick arrow pointing "up" (toward the horizon) then rotated by angle.
// angle: 0 = straight ahead, negative = left, positive = right.
void drawArrow(Adafruit_GFX* gfx, int16_t cx, int16_t cy, float r, float angle,
               uint16_t color) {
  // Arrowhead (triangle) + shaft (quad), defined pointing up, then rotated.
  const float headW = r * 0.95f;     // half-width of arrowhead
  const float headY = -r;            // tip
  const float headBase = -r * 0.15f; // where head meets shaft
  const float shaftW = r * 0.34f;    // half-width of shaft
  const float shaftBot = r * 0.85f;  // bottom of shaft

  int16_t ax, ay, bx, by, ccx, ccy;
  // Arrowhead triangle: tip, left base, right base.
  rot(0, headY, angle, cx, cy, ax, ay);
  rot(-headW, headBase, angle, cx, cy, bx, by);
  rot(headW, headBase, angle, cx, cy, ccx, ccy);
  gfx->fillTriangle(ax, ay, bx, by, ccx, ccy, color);

  // Shaft rectangle as two triangles (rotated quad).
  int16_t s0x, s0y, s1x, s1y, s2x, s2y, s3x, s3y;
  rot(-shaftW, headBase, angle, cx, cy, s0x, s0y);
  rot(shaftW, headBase, angle, cx, cy, s1x, s1y);
  rot(shaftW, shaftBot, angle, cx, cy, s2x, s2y);
  rot(-shaftW, shaftBot, angle, cx, cy, s3x, s3y);
  gfx->fillTriangle(s0x, s0y, s1x, s1y, s2x, s2y, color);
  gfx->fillTriangle(s0x, s0y, s2x, s2y, s3x, s3y, color);
}

// Draw a small destination "flag" marker for the Arrive case.
void drawFlag(Adafruit_GFX* gfx, int16_t cx, int16_t cy, float r,
              uint16_t color) {
  int16_t poleX = static_cast<int16_t>(cx - r * 0.5f);
  gfx->fillRect(poleX, static_cast<int16_t>(cy - r),
                2, static_cast<int16_t>(r * 2), color);
  gfx->fillTriangle(poleX + 2, static_cast<int16_t>(cy - r),
                    poleX + 2, static_cast<int16_t>(cy),
                    static_cast<int16_t>(cx + r * 0.7f),
                    static_cast<int16_t>(cy - r * 0.5f), color);
}

// Map a TurnDir to a rotation angle (degrees) for drawArrow. Returns false for
// non-arrow cases (handled separately).
bool turnAngle(TurnDir d, float& angle) {
  switch (d) {
    case TurnDir::Straight:    angle = 0;    return true;
    case TurnDir::SlightLeft:  angle = -35;  return true;
    case TurnDir::Left:        angle = -90;  return true;
    case TurnDir::SharpLeft:   angle = -135; return true;
    case TurnDir::SlightRight: angle = 35;   return true;
    case TurnDir::Right:       angle = 90;   return true;
    case TurnDir::SharpRight:  angle = 135;  return true;
    case TurnDir::UTurn:       angle = 180;  return true;
    default:                   return false;
  }
}

}  // namespace

void DrawNavOverlay(Adafruit_GFX* gfx, const NavOverlayProps& props) {
  const int16_t W = gfx->width();

  // ----- Top strip: arrow + distance + street -----
  if (props.dir != TurnDir::None) {
    gfx->fillRect(0, 0, W, kStripH, kStripBg);
    gfx->drawFastHLine(0, kStripH, W, kStripLine);

    // Arrow box on the left.
    const int16_t arrowCx = 18;
    const int16_t arrowCy = kStripH / 2;
    const float arrowR = 12.0f;
    float angle;
    if (props.dir == TurnDir::Arrive) {
      drawFlag(gfx, arrowCx, arrowCy, arrowR, kFg);
    } else if (turnAngle(props.dir, angle)) {
      drawArrow(gfx, arrowCx, arrowCy, arrowR, angle, kFg);
    }

    gfx->setTextSize(1);  // custom fonts ignore this, but reset for safety
    const int16_t textX = 40;
    const int16_t maxW = W - textX - 4;

    // Distance: the most important element — large amber, top of the strip.
    // (10pt JetBrainsMono: cap height ~14px; baseline placed near the top.)
    gfx->setFont(&JetBrainsMono_Thin10pt7b);
    gfx->setTextColor(kAccent);
    gfx->setCursor(textX, 16);
    gfx->print(props.distance.c_str());

    // Street name (smaller, white) below the distance, truncated to fit.
    if (!props.street.empty()) {
      gfx->setFont(&JetBrainsMono_Thin7pt7b);
      gfx->setTextColor(kFg);
      gfx->setCursor(textX, kStripH - 4);
      std::string s = props.street;
      while (!s.empty() &&
             MeasureWidth(&JetBrainsMono_Thin7pt7b, s) > maxW) {
        s.pop_back();
      }
      gfx->print(s.c_str());
    }
  }

  // ----- Bottom bar: ETA / remaining / speed -----
  if (props.showBottomBar) {
    const int16_t barY = gfx->height() - kBarH;
    gfx->fillRect(0, barY, W, kBarH, kStripBg);
    gfx->drawFastHLine(0, barY, W, kStripLine);

    gfx->setFont(&JetBrainsMono_Thin7pt7b);
    gfx->setTextSize(1);
    const int16_t textY = gfx->height() - 4;

    // Left: ETA. Center: remaining. Right: speed (+ "mph").
    if (!props.eta.empty()) {
      gfx->setTextColor(kFg);
      gfx->setCursor(4, textY);
      gfx->print(props.eta.c_str());
    }
    if (!props.remaining.empty()) {
      int16_t w = MeasureWidth(&JetBrainsMono_Thin7pt7b, props.remaining);
      gfx->setTextColor(kDim);
      gfx->setCursor((W - w) / 2, textY);
      gfx->print(props.remaining.c_str());
    }
    if (!props.speed.empty()) {
      std::string sp = props.speed + " mph";
      int16_t w = MeasureWidth(&JetBrainsMono_Thin7pt7b, sp);
      gfx->setTextColor(kFg);
      gfx->setCursor(W - w - 4, textY);
      gfx->print(sp.c_str());
    }
  }
}

}  // namespace display
