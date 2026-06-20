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

// Matcher-driven turn-by-turn navigation screen (Phase 4). Renders the next
// instruction full-width with a large font + distance-to-turn. Independent of
// the ANCS notification stack — fed by the on-device route matcher.
struct NavRouteProps {
  bool hasRoute{false};       // a route has been loaded
  bool hasFix{false};         // GPS has a valid, fresh fix
  bool isOffRoute{false};
  const char* instruction{""};  // next maneuver instruction text
  double distanceToTurnMeters{0};
  double offRouteDistanceMeters{0};
};
void DrawNavRoute(Adafruit_GFX* gfx, const NavRouteProps& props);

}  // namespace display
