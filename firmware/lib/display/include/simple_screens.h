// Simple screens with no or minimal props: splash, lights alarm, discoverable,
// OTA-in-progress, calibration-missing.
#pragma once

#include <Adafruit_GFX.h>
#include <cstdint>

namespace display {

void DrawSplash(Adafruit_GFX* gfx);

void DrawLightsAlarm(Adafruit_GFX* gfx);

struct DiscoverableProps {
  const char* bluetoothName{""};
};
void DrawDiscoverable(Adafruit_GFX* gfx, const DiscoverableProps& props);

struct OtaInProgressProps {
  uint32_t bytesReceived{0};
};
void DrawOtaInProgress(Adafruit_GFX* gfx, const OtaInProgressProps& props);

void DrawCalibrationMissing(Adafruit_GFX* gfx);

}  // namespace display
