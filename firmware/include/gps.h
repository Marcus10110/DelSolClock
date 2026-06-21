#pragma once

class TinyGPSPlus;

namespace Gps
{
    // Acquisition diagnostics, parsed from NMEA. Useful before a fix exists (the
    // Status screen surfaces these so acquisition can be watched outdoors).
    struct Debug
    {
        int satsInView{ 0 };          // GSV: satellites the antenna sees
        int satsUsed{ 0 };            // GGA: satellites used in the fix
        int fixQuality{ 0 };          // GGA: 0 = none, 1 = GPS, 2 = DGPS
        unsigned long charsProcessed{ 0 };  // rising => NMEA data flowing
    };

    TinyGPSPlus* GetGps();
    Debug GetDebug();
    void Begin();
    void Service();
    void Wake();
    void Sleep();
}