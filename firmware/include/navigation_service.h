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

    // The most recently received route. Only valid if HasRoute() is true.
    const nav::RouteSummary& GetRoute();
}
