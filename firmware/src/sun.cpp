#include "sun.h"

#include <cmath>

namespace Sun
{
    namespace
    {
        constexpr double kPi = 3.14159265358979323846;
        constexpr double kDeg = kPi / 180.0;

        // Day of year (1..366) for a UTC calendar date.
        int DayOfYear( int y, int m, int d )
        {
            static const int cum[] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
            int doy = cum[ ( m - 1 ) % 12 ] + d;
            bool leap = ( y % 4 == 0 && y % 100 != 0 ) || ( y % 400 == 0 );
            if( leap && m > 2 ) ++doy;
            return doy;
        }
    }

    bool IsDaytime( double latDeg, double lngDeg, int utcYear, int utcMonth,
                    int utcDay, int utcHour, int utcMinute )
    {
        // Fractional day-of-year angle (gamma), radians.
        int doy = DayOfYear( utcYear, utcMonth, utcDay );
        double hourUtc = utcHour + utcMinute / 60.0;
        double gamma = 2.0 * kPi / 365.0 * ( doy - 1 + ( hourUtc - 12.0 ) / 24.0 );

        // Equation of time (minutes) and solar declination (radians) — NOAA.
        double eqTime = 229.18 * ( 0.000075 + 0.001868 * std::cos( gamma ) -
                                   0.032077 * std::sin( gamma ) -
                                   0.014615 * std::cos( 2 * gamma ) -
                                   0.040849 * std::sin( 2 * gamma ) );
        double decl = 0.006918 - 0.399912 * std::cos( gamma ) +
                      0.070257 * std::sin( gamma ) - 0.006758 * std::cos( 2 * gamma ) +
                      0.000907 * std::sin( 2 * gamma ) - 0.002697 * std::cos( 3 * gamma ) +
                      0.00148 * std::sin( 3 * gamma );

        // True solar time (minutes) -> hour angle (degrees).
        double timeOffset = eqTime + 4.0 * lngDeg;  // minutes (lng east positive)
        double tst = hourUtc * 60.0 + timeOffset;   // true solar time, minutes
        double ha = ( tst / 4.0 ) - 180.0;          // hour angle, degrees

        // Solar zenith -> altitude.
        double latR = latDeg * kDeg;
        double cosZen = std::sin( latR ) * std::sin( decl ) +
                        std::cos( latR ) * std::cos( decl ) * std::cos( ha * kDeg );
        if( cosZen > 1.0 ) cosZen = 1.0;
        if( cosZen < -1.0 ) cosZen = -1.0;
        double altitudeDeg = 90.0 - std::acos( cosZen ) / kDeg;

        // Daytime once the sun is above the standard sunrise/sunset altitude.
        return altitudeDeg > -0.833;
    }
}
