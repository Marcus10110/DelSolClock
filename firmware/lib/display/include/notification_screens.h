// Notification + Navigation screens (both render an iOS-style notification).
#pragma once

#include <Adafruit_GFX.h>

namespace display {

// A notification to display. Strings are non-owning const char* (callers keep
// the backing storage alive for the Draw call).
struct Notification {
  const char* title{""};
  const char* subtitle{""};
  const char* message{""};
};

struct NotificationsProps {
  bool hasNotification{false};
  Notification notification;
  int notificationCount{0};
};
void DrawNotifications(Adafruit_GFX* gfx, const NotificationsProps& props);

struct NavigationProps {
  bool hasNotification{false};
  Notification notification;
};
void DrawNavigation(Adafruit_GFX* gfx, const NavigationProps& props);

}  // namespace display
