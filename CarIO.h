#pragma once
#include <string>

namespace CarIO
{
    struct CarStatus
    {
        bool mRearWindow;
        bool mTrunk;
        bool mRoof;
        bool mIgnition;
        bool mLights;
        float mBatteryVoltage;
        float mIlluminationVoltage;
        bool mHourButton;
        bool mMinuteButton;
        std::string ToString();
    };

    void Setup();
    CarStatus GetStatus();
    void Print( const CarStatus& car_status );

    void StartBeeper( int pulse_count, int pitch_hz, int pulse_duration_ms, int pulse_period_ms, int group_period_ms );
    void StopBeeper();
    void Service();

}