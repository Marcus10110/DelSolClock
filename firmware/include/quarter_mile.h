#pragma once
#include "car_io.h"

class TinyGPSPlus;

namespace Display
{
    class Display;
}

namespace Tft
{
    class Tft;
}


namespace QuarterMile
{
    void Reset();
    void Service( Display::Display* display, Tft::Tft* tft, CarIO::ButtonEvents button_events, TinyGPSPlus* gps );
}