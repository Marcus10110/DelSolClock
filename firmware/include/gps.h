#pragma once

class TinyGPSPlus;

namespace Gps
{
    TinyGPSPlus* GetGps();
    void Begin();
    void Service();
    void Wake();
    void Sleep();
}