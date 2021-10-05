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

    void SetTftBrightness( int brightness ); // 0 to 255;

}