#pragma once

namespace Sun
{
    // Is the sun above the horizon at the given location and UTC time?
    // lat/lng in degrees; UTC date/time. Used to auto-switch the nav view's
    // day/night palette. A simplified NOAA solar-position calc — accurate to a
    // few minutes around sunrise/sunset, which is plenty for a palette switch.
    bool IsDaytime( double latDeg, double lngDeg, int utcYear, int utcMonth,
                    int utcDay, int utcHour, int utcMinute );
}
