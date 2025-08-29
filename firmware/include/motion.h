#pragma once

#include <Adafruit_LSM6DS3.h>


namespace Motion
{
    struct State
    {
        float mForward{ 0 }; // X
        float mLeft{ 0 };    // Y
        float mUp{ 0 };      // Z
        bool mValid{ false };
        bool mCalibrated{ false };
    };

    void Begin();
    State GetState();
    void Calibrate();

    class HistoryTracker
    {
      public:
        HistoryTracker( int16_t size, int32_t ms_per_tick );
        ~HistoryTracker();

        void PushData( double data, int32_t timestamp );

        void GetData( double* buffer, int16_t size );

      private:
        int16_t mSize;
        int32_t mMsPerTick;
        int32_t mLastUpdateTime;
        double* mBuffer{ nullptr };

        int16_t mHead{ 0 };
        int16_t mCount{ 0 };
    };
}