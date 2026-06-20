// Shared demo screen registry: a list of named screens with sample data,
// rendered by BOTH the UiDesigner runner and the firmware Demo::Demo().
//
// Guarded by DEMO_ENABLED so the demo data + this registry link into the
// firmware ONLY in demo builds (UiDesigner always defines DEMO_ENABLED). The
// real Draw* functions always link; only the sample props here are conditional.
#pragma once

#include <Adafruit_GFX.h>

#include <functional>
#include <string>
#include <vector>

namespace demo {

struct DemoScreen {
  std::string name;
  std::function<void(Adafruit_GFX*)> draw;
};

// All demo screens, in display order. Empty if DEMO_ENABLED is not defined.
const std::vector<DemoScreen>& GetDemoScreens();

}  // namespace demo
