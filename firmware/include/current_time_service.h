#pragma once
#include <functional>

class BLEClient;

namespace CurrentTimeService
{
    struct CurrentTime
    {
        enum class DayOfWeek : uint8_t
        {
            Unknown = 0,
            Monday = 1,
            Tuesday = 2,
            Wednesday = 3,
            Thursday = 4,
            Friday = 5,
            Saturday = 6,
            Sunday = 7,
        };
        // Date Time
        uint16_t mYear{ 0 };
        uint8_t mMonth{ 0 };
        uint8_t mDay{ 0 };
        uint8_t mHours{ 0 };
        uint8_t mMinutes{ 0 };
        uint8_t mSeconds{ 0 };
        // Day Date Time
        DayOfWeek mDayOfWeek{ DayOfWeek::Unknown };
        // Exact Time 256
        float mSecondsFraction{ 0 };

        void Dump() const;
        time_t ToTimeT() const;
    };

    // Note: if the client was deleted, but StopTimeService was not called, this will probably crash.
    bool GetCurrentTime( CurrentTime* current_time );
    bool StartTimeService( BLEClient* client, CurrentTime* current_time = nullptr );
    // Call this before deleteing BLEClient.
    void StopTimeService();
}