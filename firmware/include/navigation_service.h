#pragma once
#include <cstdint>

#include "route_summary.h"

class BLEServer;

namespace NavigationService
{
    // Create the Navigation BLE service (route download). Mirrors BleOta::Begin.
    void Begin( BLEServer* server );

    // True once a complete route has been received + decoded.
    bool HasRoute();

    // One-shot edge: returns true exactly once after each newly downloaded route,
    // then resets. The main loop polls this to auto-switch to the nav screen.
    bool ConsumeNewRoute();

    // The most recently received route. Only valid if HasRoute() is true.
    const nav::RouteSummary& GetRoute();
}
