#pragma once
#include "car_io.h"

#include <Adafruit_GFX.h>

class TinyGPSPlus;

namespace Tft
{
    class Tft;
}


namespace QuarterMile
{
    void Reset();
    void Service( GFXcanvas16* display, Tft::Tft* tft, CarIO::ButtonEvents button_events, TinyGPSPlus* gps );
}