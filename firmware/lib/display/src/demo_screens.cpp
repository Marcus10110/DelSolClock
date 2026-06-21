#include "demo_screens.h"

#ifdef DEMO_ENABLED

#include <vector>

#include "clock_screen.h"
#include "gmeter_screen.h"
#include "notification_screens.h"
#include "perspective_screen.h"
#include "quarter_mile_screens.h"
#include "simple_screens.h"
#include "status_screen.h"

namespace demo {
namespace {

const std::vector<DemoScreen> g_demo_screens = {
    {"Splash", [](Adafruit_GFX* g) { display::DrawSplash(g); }},

    {"Clock",
     [](Adafruit_GFX* g) {
       display::ClockProps p;
       p.hours24 = 23;
       p.minutes = 59;
       p.speedMph = 42.1f;
       p.headlight = true;
       p.bluetooth = true;
       p.mediaTitle = "She Swam Higher";
       display::DrawClock(g, p);
     }},

    {"LightsAlarm", [](Adafruit_GFX* g) { display::DrawLightsAlarm(g); }},

    {"Discoverable",
     [](Adafruit_GFX* g) {
       display::DiscoverableProps p;
       p.bluetoothName = "Del Sol";
       display::DrawDiscoverable(g, p);
     }},

    {"Status",
     [](Adafruit_GFX* g) {
       display::StatusProps p;
       p.batteryVolts = 12.5;
       p.latitude = 37.7749;
       p.longitude = -122.4194;
       p.speedMph = 55.0;
       p.headingDegrees = 180.0;
       p.forwardG = 0.5;
       p.lateralG = 0.1;
       p.verticalG = 1.01;
       display::DrawStatus(g, p);
     }},

    {"OtaInProgress",
     [](Adafruit_GFX* g) {
       display::OtaInProgressProps p;
       p.bytesReceived = 123456;
       display::DrawOtaInProgress(g, p);
     }},

    {"CalibrationMissing",
     [](Adafruit_GFX* g) { display::DrawCalibrationMissing(g); }},

    {"Notifications",
     [](Adafruit_GFX* g) {
       display::NotificationsProps p;
       p.hasNotification = true;
       p.notification.title = "New Message";
       p.notification.subtitle = "From John Doe";
       p.notification.message = "Hey, are you coming to the party?";
       p.notificationCount = 3;
       display::DrawNotifications(g, p);
     }},

    {"Notifications-None",
     [](Adafruit_GFX* g) {
       display::NotificationsProps p;
       p.hasNotification = false;
       display::DrawNotifications(g, p);
     }},

    {"Navigation-None",
     [](Adafruit_GFX* g) {
       display::NavigationProps p;
       p.hasNotification = false;
       display::DrawNavigation(g, p);
     }},

    {"Navigation-Left",
     [](Adafruit_GFX* g) {
       display::NavigationProps p;
       p.hasNotification = true;
       p.notification.message = "Turn left onto Eucalyptus Dr";
       display::DrawNavigation(g, p);
     }},

    {"Navigation-Right",
     [](Adafruit_GFX* g) {
       display::NavigationProps p;
       p.hasNotification = true;
       p.notification.message = "Turn right onto Eucalyptus Dr";
       display::DrawNavigation(g, p);
     }},

    {"Navigation-Complex",
     [](Adafruit_GFX* g) {
       display::NavigationProps p;
       p.hasNotification = true;
       p.notification.message =
           "Keep right, follow signs for Golden Gate National Cemetery/VA "
           "Clinic and merge onto Sneath Ln";
       display::DrawNavigation(g, p);
     }},

    {"NavRoute-Turn",
     [](Adafruit_GFX* g) {
       display::NavRouteProps p;
       p.hasRoute = true;
       p.hasFix = true;
       p.distanceToTurnMeters = 480;  // ~0.3 mi
       p.instruction = "Turn left onto Eucalyptus Dr";
       display::DrawNavRoute(g, p);
     }},

    {"NavRoute-Close",
     [](Adafruit_GFX* g) {
       display::NavRouteProps p;
       p.hasRoute = true;
       p.hasFix = true;
       p.distanceToTurnMeters = 90;  // feet
       p.instruction = "Turn right onto Sneath Ln";
       display::DrawNavRoute(g, p);
     }},

    {"NavRoute-OffRoute",
     [](Adafruit_GFX* g) {
       display::NavRouteProps p;
       p.hasRoute = true;
       p.hasFix = true;
       p.isOffRoute = true;
       p.offRouteDistanceMeters = 175.4;
       display::DrawNavRoute(g, p);
     }},

    {"NavRoute-NoRoute",
     [](Adafruit_GFX* g) {
       display::NavRouteProps p;
       p.hasRoute = false;
       display::DrawNavRoute(g, p);
     }},

    {"QuarterMile-Start",
     [](Adafruit_GFX* g) { display::quarter_mile::DrawStart(g); }},

    {"QuarterMile-Launch",
     [](Adafruit_GFX* g) {
       display::quarter_mile::LaunchProps p;
       p.accelerationG = 1.2;
       display::quarter_mile::DrawLaunch(g, p);
     }},

    {"QuarterMile-InProgress",
     [](Adafruit_GFX* g) {
       display::quarter_mile::InProgressProps p;
       p.timeSec = 5.0;
       p.speedMph = 30.0;
       p.accelerationG = 1.5;
       p.distanceMiles = 0.1;
       display::quarter_mile::DrawInProgress(g, p);
     }},

    {"QuarterMile-Summary",
     [](Adafruit_GFX* g) {
       display::quarter_mile::SummaryProps p;
       p.maxAccelerationG = 0.9;
       p.maxSpeedMph = 121.1;
       p.zeroSixtyTimeSec = 9.9;
       p.quarterMileTimeSec = 87.1;
       display::quarter_mile::DrawSummary(g, p);
     }},

    {"Perspective-Straight",
     [](Adafruit_GFX* g) {
       display::PerspectiveProps p;  // no centerline => straight road
       display::DrawPerspective(g, p);
     }},

    {"Perspective-CurveRight",
     [](Adafruit_GFX* g) {
       display::PerspectiveProps p;
       // Synthetic centerline curving to the right ahead of the car.
       for (float d = 1.0f; d <= 120.0f; d += 4.0f) {
         float right = 0.0008f * d * d;  // gentle rightward bend
         p.centerline.push_back({d, right});
       }
       display::DrawPerspective(g, p);
     }},

    {"Perspective-CurveLeft",
     [](Adafruit_GFX* g) {
       display::PerspectiveProps p;
       for (float d = 1.0f; d <= 120.0f; d += 4.0f) {
         float right = -0.0008f * d * d;
         p.centerline.push_back({d, right});
       }
       display::DrawPerspective(g, p);
     }},
};

}  // namespace

const std::vector<DemoScreen>& GetDemoScreens() { return g_demo_screens; }

}  // namespace demo

#else  // !DEMO_ENABLED

namespace demo {

const std::vector<DemoScreen>& GetDemoScreens() {
  static const std::vector<DemoScreen> empty;
  return empty;
}

}  // namespace demo

#endif  // DEMO_ENABLED
