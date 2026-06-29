#include "nav_overlay.h"

#include <cmath>
#include <cstdio>

#include "draw_helpers.h"
#include "fonts.h"

namespace display {
namespace {

// HUD theme: night (light text on a black bar, glowing magenta chrome) and day
// (dark ink on a light bar) for sunlight legibility. The bars are a solid
// backing so the text/arrows always have a high-contrast field, never competing
// with the road/sky behind them.
struct Theme {
  uint16_t stripBg;    // bar backing
  uint16_t stripLine;  // thin separator
  uint16_t fg;         // primary text/arrows
  uint16_t accent;     // distance / speed accent
  uint16_t dim;        // secondary labels
  uint16_t chrome;     // HUD border
  uint16_t chromeDim;  // inner dividers
};

constexpr Theme kNightHud = {
    0x0000,  // stripBg   — black
    0x4208,  // stripLine — mid grey
    0xFFFF,  // fg        — white
    0x07FF,  // accent    — cyan
    0xC618,  // dim       — light grey
    0xF81F,  // chrome    — magenta glow
    0x6010,  // chromeDim — dim magenta
};

constexpr Theme kDayHud = {
    0xEF7D,  // stripBg   — near-white (light bar)
    0x8410,  // stripLine — mid grey
    0x0000,  // fg        — black ink
    0xC000,  // accent    — dark red (distance pops, stays dark for glare)
    0x4208,  // dim       — dark grey secondary
    0x300C,  // chrome    — dark navy border
    0x9CD3,  // chromeDim — soft grey dividers
};

const Theme* gTh = &kNightHud;

// Back-compat aliases so the existing draw code reads gTh->* via short names.
#define kStripBg (gTh->stripBg)
#define kStripLine (gTh->stripLine)
#define kFg (gTh->fg)
#define kAccent (gTh->accent)
#define kDim (gTh->dim)
#define kChrome (gTh->chrome)
#define kChromeDim (gTh->chromeDim)

constexpr int16_t kStripH = 34;  // top strip height
constexpr int16_t kBarH = 16;    // bottom bar height (a touch taller for chrome)

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
  gTh = props.daytime ? &kDayHud : &kNightHud;
  // Work inside the bezel's visible area so the HUD text never lands under the
  // hidden border. x0/x1 are the visible left/right edges; the bars hug the
  // visible top and bottom.
  const int16_t x0 = VisibleLeft();
  const int16_t x1 = VisibleRight();  // exclusive
  const int16_t vw = x1 - x0;
  const int16_t visTop = VisibleTop();
  const int16_t visBot = VisibleBottom();  // exclusive

  // ----- Top strip: arrow + distance + street -----
  if (props.dir != TurnDir::None) {
    const int16_t stripTop = visTop;
    const int16_t stripBot = visTop + kStripH;  // y of the bottom border
    gfx->fillRect(x0, stripTop, vw, kStripH, kStripBg);
    // HUD chrome: a glowing magenta underline with brighter corner ticks, and a
    // thin divider separating the arrow box from the text.
    gfx->drawFastHLine(x0, stripBot, vw, kChromeDim);
    gfx->drawFastHLine(x0, stripBot + 1, vw, kChrome);
    gfx->drawFastVLine(x0, stripTop, kStripH, kChrome);
    gfx->drawFastVLine(x1 - 1, stripTop, kStripH, kChrome);
    gfx->drawFastVLine(x0 + 34, stripTop + 3, kStripH - 6, kChromeDim);

    // Arrow box on the left.
    const int16_t arrowCx = x0 + 18;
    const int16_t arrowCy = stripTop + kStripH / 2;
    const float arrowR = 12.0f;
    float angle;
    if (props.dir == TurnDir::Arrive) {
      drawFlag(gfx, arrowCx, arrowCy, arrowR, kFg);
    } else if (turnAngle(props.dir, angle)) {
      drawArrow(gfx, arrowCx, arrowCy, arrowR, angle, kFg);
    }

    gfx->setTextSize(1);  // custom fonts ignore this, but reset for safety
    const int16_t textX = x0 + 40;
    const int16_t maxW = x1 - textX - 4;

    // Distance: the most important element — large amber, top of the strip.
    // (10pt JetBrainsMono: cap height ~14px; baseline placed near the top.)
    gfx->setFont(&JetBrainsMono_Thin10pt7b);
    gfx->setTextColor(kAccent);
    gfx->setCursor(textX, stripTop + 16);
    gfx->print(props.distance.c_str());

    // Street name (smaller, white) below the distance, truncated to fit.
    if (!props.street.empty()) {
      gfx->setFont(&JetBrainsMono_Thin7pt7b);
      gfx->setTextColor(kFg);
      gfx->setCursor(textX, stripTop + kStripH - 4);
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
    const int16_t barY = visBot - kBarH;
    gfx->fillRect(x0, barY, vw, kBarH, kStripBg);
    // HUD chrome: glowing magenta top border + corner ticks + field dividers.
    gfx->drawFastHLine(x0, barY, vw, kChrome);
    gfx->drawFastHLine(x0, barY + 1, vw, kChromeDim);
    gfx->drawFastVLine(x0, barY, kBarH, kChrome);
    gfx->drawFastVLine(x1 - 1, barY, kBarH, kChrome);
    // Two dividers splitting the bar into ETA | remaining | speed thirds.
    const int16_t div1 = x0 + vw / 3;
    const int16_t div2 = x0 + (vw * 2) / 3;
    gfx->drawFastVLine(div1, barY + 3, kBarH - 4, kChromeDim);
    gfx->drawFastVLine(div2, barY + 3, kBarH - 4, kChromeDim);

    gfx->setFont(&JetBrainsMono_Thin7pt7b);
    gfx->setTextSize(1);
    const int16_t textY = visBot - 4;

    // Left: ETA. Center: remaining. Right: speed (boxed, like a HUD readout).
    if (!props.eta.empty()) {
      gfx->setTextColor(kFg);
      gfx->setCursor(x0 + 5, textY);
      gfx->print(props.eta.c_str());
    }
    if (!props.remaining.empty()) {
      int16_t w = MeasureWidth(&JetBrainsMono_Thin7pt7b, props.remaining);
      gfx->setTextColor(kDim);
      gfx->setCursor(x0 + (vw - w) / 2, textY);
      gfx->print(props.remaining.c_str());
    }
    if (!props.speed.empty()) {
      // Speed sits in the right third; draw it cyan and boxed for a HUD readout.
      int16_t w = MeasureWidth(&JetBrainsMono_Thin7pt7b, props.speed);
      int16_t sx = x1 - w - 8;
      gfx->setTextColor(kAccent);
      gfx->setCursor(sx, textY);
      gfx->print(props.speed.c_str());
      gfx->drawRect(sx - 3, barY + 2, w + 6, kBarH - 4, kChromeDim);
    }
  }
}

TurnDir TurnDirFromManeuver(const std::string& type,
                            const std::string& modifier) {
  if (type == "arrive") return TurnDir::Arrive;
  if (modifier == "uturn") return TurnDir::UTurn;
  if (modifier == "sharp left") return TurnDir::SharpLeft;
  if (modifier == "left") return TurnDir::Left;
  if (modifier == "slight left") return TurnDir::SlightLeft;
  if (modifier == "sharp right") return TurnDir::SharpRight;
  if (modifier == "right") return TurnDir::Right;
  if (modifier == "slight right") return TurnDir::SlightRight;
  return TurnDir::Straight;  // continue / straight / merge / depart
}

std::string FormatDistanceImperial(double meters) {
  const double feet = meters * 3.28084;
  char buf[24];
  if (feet < 1000.0) {
    int ft = static_cast<int>(feet);
    int step = ft < 200 ? 25 : 50;  // tidy increments
    ft = (ft / step) * step;
    if (ft < step) ft = step;  // never show 0 ft while still approaching
    std::snprintf(buf, sizeof(buf), "%d ft", ft);
  } else {
    double mi = meters / 1609.344;
    std::snprintf(buf, sizeof(buf), "%.1f mi", mi);
  }
  return buf;
}

std::string FormatEta(int nowHour24, int nowMinute, double minutesToAdd) {
  if (!(minutesToAdd >= 0.0) || !std::isfinite(minutesToAdd)) return "";
  int total = nowHour24 * 60 + nowMinute + static_cast<int>(minutesToAdd + 0.5);
  total %= 24 * 60;
  if (total < 0) total += 24 * 60;
  int h = total / 60;
  int m = total % 60;
  int h12 = h % 12;
  if (h12 == 0) h12 = 12;
  char buf[12];
  std::snprintf(buf, sizeof(buf), "%d:%02d", h12, m);
  return buf;
}

}  // namespace display
