// Shared drawing helpers built on top of the real Adafruit GFX API.
//
// Free functions taking an Adafruit_GFX* (a GFXcanvas16 underneath on both
// firmware and desktop). These cover the conveniences the old Display wrapper
// provided beyond raw GFX: clear-to-defaults, padding-aware screen rect, and
// aligned text. Screen draw functions and both runners use the same code here.
#pragma once

#include <Adafruit_GFX.h>
#include <cstdint>
#include <string>
#include <vector>

namespace display {

// Display geometry + default style (ported from the old Display::Display).
constexpr int16_t kWidth = 240;
constexpr int16_t kHeight = 136;
constexpr int16_t kTopPadding = 5;
constexpr int16_t kBottomPadding = 10;
constexpr int16_t kLeftPadding = 0;
constexpr int16_t kRightPadding = 0;
constexpr uint16_t kDefaultTextColor = 0xFFFF;
constexpr uint16_t kDefaultBackgroundColor = 0x0000;
constexpr uint8_t kDefaultFontSize = 1;

enum class HAlign { Left, Center, Right };
enum class VAlign { Top, Center, Bottom };

struct Rect {
  int16_t x{0};
  int16_t y{0};
  int16_t w{0};
  int16_t h{0};
};

// The drawable region inside the padding (the old Display::ScreenRect()).
Rect ScreenRect();

// Clear to background and restore default cursor/color/wrap/font/size.
void Clear(Adafruit_GFX* gfx);

// Compute aligned text position within a region (defaults to ScreenRect()).
// Mirrors the old Display::GetTextLocation exactly. left/top are outputs;
// text_region (optional) receives the resulting bounding box.
void GetTextLocation(Adafruit_GFX* gfx, const char* text, HAlign horizontal,
                     VAlign vertical, int16_t* left, int16_t* top,
                     const Rect* within_region = nullptr,
                     Rect* text_region = nullptr);

// Position the cursor per GetTextLocation and write the text.
void WriteAligned(Adafruit_GFX* gfx, const char* text, HAlign horizontal,
                  VAlign vertical, const Rect* within_region = nullptr,
                  Rect* text_region = nullptr);

// Width of `s` in pixels for the given custom GFX font, summing per-glyph
// xAdvance. Prefer this over Adafruit getTextBounds for layout/fitting — that
// can under-report custom-font width; this matches how drawChar advances.
int16_t MeasureWidth(const GFXfont* font, const std::string& s);

// Greedily word-wrap `text` into lines no wider than `maxWidth` (pixels) in the
// given font. A single word wider than maxWidth is left on its own line (GFX
// would otherwise break mid-word).
std::vector<std::string> WrapWords(const GFXfont* font, const std::string& text,
                                   int16_t maxWidth);

}  // namespace display
