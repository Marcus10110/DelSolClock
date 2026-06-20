#include "notification_screens.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "draw_helpers.h"
#include "fonts.h"
#include "image_loader.h"

namespace display {
namespace {

std::string Trim(const std::string& str) {
  size_t first = str.find_first_not_of(' ');
  if (first == std::string::npos) return "";
  size_t last = str.find_last_not_of(' ');
  return str.substr(first, (last - first + 1));
}

}  // namespace

void DrawNotifications(Adafruit_GFX* gfx, const NotificationsProps& props) {
  Clear(gfx);
  if (!props.hasNotification) {
    gfx->setFont(&JetBrainsMono_Thin14pt7b);
    WriteAligned(gfx, "Notifications", HAlign::Center, VAlign::Top);
    gfx->setFont(&JetBrainsMono_Thin10pt7b);
    WriteAligned(gfx, "No Notifications", HAlign::Center, VAlign::Center);
    return;
  }

  gfx->setFont(&JetBrainsMono_Thin14pt7b);
  WriteAligned(gfx, "Notifications", HAlign::Center, VAlign::Top);

  std::string summary = std::string(props.notification.title) + "\n" +
                        props.notification.subtitle + "\n" +
                        props.notification.message;

  gfx->setFont(&JetBrainsMono_Thin7pt7b);
  WriteAligned(gfx, summary.c_str(), HAlign::Center, VAlign::Center);

  // write count of notifications in bottom right
  gfx->setFont(&JetBrainsMono_Thin7pt7b);
  char buffer[8];
  snprintf(buffer, sizeof(buffer), "%d", props.notificationCount);
  WriteAligned(gfx, buffer, HAlign::Right, VAlign::Bottom);
}

void DrawNavigation(Adafruit_GFX* gfx, const NavigationProps& props) {
  Clear(gfx);

  gfx->setFont(&JetBrainsMono_Thin14pt7b);
  WriteAligned(gfx, "Navigation", HAlign::Center, VAlign::Top);

  if (!props.hasNotification) {
    gfx->setFont(&JetBrainsMono_Thin10pt7b);
    WriteAligned(gfx, "No Notifications", HAlign::Center, VAlign::Center);
    return;
  }

  std::string message = props.notification.message;
  const char* left_prefix = "Turn left onto";
  const char* right_prefix = "Turn right onto";
  bool is_left = message.rfind(left_prefix, 0) == 0;
  bool is_right = message.rfind(right_prefix, 0) == 0;

  if (!is_left && !is_right) {
    // display the contents in the center.
    if (message.length() <= 60) {
      gfx->setFont(&JetBrainsMono_Thin10pt7b);
    } else if (message.length() <= 80) {
      gfx->setFont(&JetBrainsMono_Thin9pt7b);
    } else {
      gfx->setFont(&JetBrainsMono_Thin8pt7b);
    }
    WriteAligned(gfx, message.c_str(), HAlign::Center, VAlign::Center);
    return;
  }

  // draw an arrow on the right side of the screen.
  const int bmp_size = 80;  // 80x80 pixels
  Rect screen_rect = ScreenRect();
  int top_height = 20;
  screen_rect.y += top_height;
  screen_rect.h -= top_height;
  if (is_left) {
    DrawBMP(gfx, "/left.bmp", screen_rect.x + screen_rect.w - bmp_size,
            screen_rect.y + (screen_rect.h - bmp_size) / 2);
  } else if (is_right) {
    DrawBMP(gfx, "/right.bmp", screen_rect.x + screen_rect.w - bmp_size,
            screen_rect.y + (screen_rect.h - bmp_size) / 2);
  }
  // write the text on the left side of the screen.
  std::string line1 = is_left ? left_prefix : right_prefix;
  std::string line2 = message.substr(is_left ? strlen(left_prefix)
                                             : strlen(right_prefix));
  line2 = Trim(line2);
  // line 1 in 8pt, line 2 in 10pt.
  gfx->setFont(&JetBrainsMono_Thin8pt7b);
  gfx->setCursor(0, top_height + 26);
  gfx->write(line1.c_str());
  gfx->write("\n");

  gfx->setFont(&JetBrainsMono_Thin10pt7b);
  gfx->write(line2.c_str());
}

namespace {

// Format a distance for display: feet under ~0.2mi, otherwise miles.
void formatDistance(double meters, char* out, size_t n) {
  const double feet = meters * 3.28084;
  if (feet < 1000.0) {
    // round to nearest 10 ft for stability
    int ft = static_cast<int>((feet + 5) / 10) * 10;
    snprintf(out, n, "%d ft", ft);
  } else {
    const double miles = meters / 1609.344;
    snprintf(out, n, "%.1f mi", miles);
  }
}

// Width of `s` in pixels for the given custom GFX font, summing per-glyph
// xAdvance. (Adafruit getTextBounds can under-report custom-font width; this is
// reliable and matches how drawChar actually advances the cursor.)
int16_t measureWidth(const GFXfont* font, const std::string& s) {
  int total = 0;
  for (char ch : s) {
    uint8_t c = static_cast<uint8_t>(ch);
    if (c < font->first || c > font->last) continue;
    total += font->glyph[c - font->first].xAdvance;
  }
  return static_cast<int16_t>(total);
}

// Greedily word-wrap `text` to lines no wider than `maxWidth` in the current
// font. A single word wider than maxWidth is left on its own line (GFX would
// otherwise break mid-word).
std::vector<std::string> wrapWords(const GFXfont* font, const std::string& text,
                                   int16_t maxWidth) {
  std::vector<std::string> lines;
  std::string line;
  size_t i = 0;
  while (i < text.size()) {
    size_t sp = text.find(' ', i);
    std::string word = text.substr(i, sp == std::string::npos ? sp : sp - i);
    std::string candidate = line.empty() ? word : line + " " + word;
    if (measureWidth(font, candidate) <= maxWidth || line.empty()) {
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

}  // namespace

void DrawNavRoute(Adafruit_GFX* gfx, const NavRouteProps& props) {
  Clear(gfx);
  const Rect screen = ScreenRect();

  if (!props.hasRoute) {
    gfx->setFont(&JetBrainsMono_Thin12pt7b);
    WriteAligned(gfx, "No Route", HAlign::Center, VAlign::Center);
    return;
  }
  if (!props.hasFix) {
    gfx->setFont(&JetBrainsMono_Thin12pt7b);
    WriteAligned(gfx, "Waiting for GPS", HAlign::Center, VAlign::Center);
    return;
  }
  if (props.isOffRoute) {
    gfx->setFont(&JetBrainsMono_Thin16pt7b);
    WriteAligned(gfx, "OFF ROUTE", HAlign::Center, VAlign::Top);
    char buf[32];
    formatDistance(props.offRouteDistanceMeters, buf, sizeof(buf));
    gfx->setFont(&JetBrainsMono_Thin12pt7b);
    WriteAligned(gfx, buf, HAlign::Center, VAlign::Center);
    return;
  }

  // Distance-to-turn, large, near the top.
  char dist[32];
  formatDistance(props.distanceToTurnMeters, dist, sizeof(dist));
  gfx->setFont(&JetBrainsMono_Thin16pt7b);
  Rect distRegion = screen;
  distRegion.h = 34;
  WriteAligned(gfx, dist, HAlign::Center, VAlign::Top, &distRegion);

  // Instruction below, full width. Pick the largest font whose word-wrapped
  // lines fit the remaining height, then draw them centered.
  const char* instr =
      props.instruction && props.instruction[0] ? props.instruction : "Continue";
  Rect instrRegion = screen;
  instrRegion.y += distRegion.h + 6;
  instrRegion.h -= distRegion.h + 6;

  const GFXfont* fonts[] = {
      &JetBrainsMono_Thin16pt7b, &JetBrainsMono_Thin14pt7b,
      &JetBrainsMono_Thin12pt7b, &JetBrainsMono_Thin10pt7b,
      &JetBrainsMono_Thin9pt7b,  &JetBrainsMono_Thin8pt7b,
      &JetBrainsMono_Thin7pt7b,
  };
  const GFXfont* chosen = fonts[6];
  std::vector<std::string> lines;
  for (const GFXfont* f : fonts) {
    auto candidate = wrapWords(f, instr, instrRegion.w);
    int16_t lineH = f->yAdvance;
    bool fitsHeight =
        static_cast<int16_t>(candidate.size()) * lineH <= instrRegion.h;
    lines = candidate;  // keep last evaluated as fallback (smallest font)
    chosen = f;
    // wrapWords already guarantees each line fits the width (except a single
    // over-long word, unavoidable); just check the height here.
    if (fitsHeight) break;
  }

  gfx->setFont(chosen);
  gfx->setTextWrap(false);
  int16_t lineH = chosen->yAdvance;
  int16_t totalH = static_cast<int16_t>(lines.size()) * lineH;
  int16_t y = instrRegion.y + (instrRegion.h - totalH) / 2;
  for (const auto& ln : lines) {
    Rect lineRegion{instrRegion.x, y, instrRegion.w, lineH};
    WriteAligned(gfx, ln.c_str(), HAlign::Center, VAlign::Top, &lineRegion);
    y += lineH;
  }
}

}  // namespace display
