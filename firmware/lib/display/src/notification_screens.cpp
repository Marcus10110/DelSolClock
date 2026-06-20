#include "notification_screens.h"

#include <cstdio>
#include <cstring>
#include <string>

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

}  // namespace display
