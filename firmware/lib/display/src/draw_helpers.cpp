#include "draw_helpers.h"

namespace display {

Rect ScreenRect() {
  return Rect{kLeftPadding, kTopPadding, kWidth - kLeftPadding - kRightPadding,
              kHeight - kTopPadding - kBottomPadding};
}

void Clear(Adafruit_GFX* gfx) {
  if (!gfx) return;
  gfx->fillScreen(kDefaultBackgroundColor);
  gfx->setCursor(0, 0);
  gfx->setTextColor(kDefaultTextColor);
  gfx->setTextWrap(true);
  gfx->setFont(nullptr);
  gfx->setTextSize(kDefaultFontSize);
}

void GetTextLocation(Adafruit_GFX* gfx, const char* text, HAlign horizontal,
                     VAlign vertical, int16_t* left, int16_t* top,
                     const Rect* within_region, Rect* text_region) {
  if (!gfx || !text || !left || !top) return;

  Rect region = within_region ? *within_region : ScreenRect();

  int16_t x1, y1;
  uint16_t w, h;
  gfx->getTextBounds(text, 0, 0, &x1, &y1, &w, &h);

  switch (horizontal) {
    case HAlign::Left:
      *left = -x1;
      break;
    case HAlign::Center:
      *left = (region.w - static_cast<int16_t>(w)) / 2 - x1;
      break;
    case HAlign::Right:
      *left = region.w - static_cast<int16_t>(w) - x1;
      break;
  }
  *left += region.x;

  switch (vertical) {
    case VAlign::Top:
      *top = -y1;
      break;
    case VAlign::Center:
      *top = (region.h - static_cast<int16_t>(h)) / 2 - y1;
      break;
    case VAlign::Bottom:
      *top = region.h - static_cast<int16_t>(h) - y1;
      break;
  }
  *top += region.y;

  if (text_region) {
    text_region->x = *left + x1;
    text_region->y = *top + y1;
    text_region->w = static_cast<int16_t>(w);
    text_region->h = static_cast<int16_t>(h);
  }
}

void WriteAligned(Adafruit_GFX* gfx, const char* text, HAlign horizontal,
                  VAlign vertical, const Rect* within_region,
                  Rect* text_region) {
  int16_t left = 0, top = 0;
  GetTextLocation(gfx, text, horizontal, vertical, &left, &top, within_region,
                  text_region);
  gfx->setCursor(left, top);
  gfx->write(text);
}

int16_t MeasureWidth(const GFXfont* font, const std::string& s) {
  if (!font) return 0;
  int total = 0;
  for (char ch : s) {
    uint8_t c = static_cast<uint8_t>(ch);
    if (c < font->first || c > font->last) continue;
    total += font->glyph[c - font->first].xAdvance;
  }
  return static_cast<int16_t>(total);
}

std::vector<std::string> WrapWords(const GFXfont* font, const std::string& text,
                                   int16_t maxWidth) {
  std::vector<std::string> lines;
  std::string line;
  size_t i = 0;
  while (i < text.size()) {
    size_t sp = text.find(' ', i);
    std::string word = text.substr(i, sp == std::string::npos ? sp : sp - i);
    std::string candidate = line.empty() ? word : line + " " + word;
    if (MeasureWidth(font, candidate) <= maxWidth || line.empty()) {
      line = candidate;
    } else {
      lines.push_back(line);
      line = word;
    }
    if (sp == std::string::npos) break;
    i = sp + 1;
  }
  if (!line.empty()) lines.push_back(line);
  return lines;
}

}  // namespace display
