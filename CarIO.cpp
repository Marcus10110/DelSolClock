#include <Arduino.h>
#include "CarIO.h"
#include "pins.h"

namespace CarIO
{
    namespace
    {
        constexpr uint8_t TftBacklightPwmChannel = 0;
        constexpr uint8_t BuzzerPwmChannel = 1;
    }
    std::string CarStatus::ToString()
    {
        char display_string[ 128 ] = { 0 };
        snprintf( display_string, sizeof( display_string ), "B: %f, Ill: %f (%i), IGN: %i, RW: %i, T: %i, R: %i, H: %i, M: %i\n",
                  mBatteryVoltage, mIlluminationVoltage, mLights, mIgnition, mRearWindow, mTrunk, mRoof, mHourButton, mMinuteButton );
        return display_string;
    }
    void Setup()
    {
        pinMode( Pin::TftLit, OUTPUT );
        pinMode( Pin::Battery, INPUT );
        pinMode( Pin::Illumination, INPUT );
        pinMode( Pin::Ignition, INPUT );
        pinMode( Pin::RearWindow, INPUT );
        pinMode( Pin::Trunk, INPUT );
        pinMode( Pin::Roof, INPUT );
        pinMode( Pin::Hour, INPUT );
        pinMode( Pin::Minute, INPUT );
        pinMode( Pin::Buzzer, OUTPUT );

        ledcSetup( TftBacklightPwmChannel, 5000, 8 ); // channel 0, 5 khz, 8 bit resolution
        ledcSetup( BuzzerPwmChannel, 440, 8 );        // channel 1, 5 khz, 8 bit resolution
        ledcAttachPin( Pin::TftLit, TftBacklightPwmChannel );
        ledcAttachPin( Pin::Buzzer, BuzzerPwmChannel );

        ledcWrite( TftBacklightPwmChannel, 255 );
        // ledcWrite( BuzzerPwmChannel, 128 );

        // set the ADC range to 0 to 800mV
        // analogSetAttenuation( ADC_0db );
        // by default: ADC_11db 2600 mV. (1V input = ADC reading of 1575
    }
    CarStatus GetStatus()
    {
        CarStatus status;
        int battery_code = analogRead( Pin::Battery );
        int illumination_code = analogRead( Pin::Illumination );
        // Volts Per Code (Oct 2 2021) 0.0202547273
        status.mBatteryVoltage = battery_code * 0.0202547273f;
        // volts per code (oct 2 2021) 0.003800545407
        status.mIlluminationVoltage = illumination_code * 0.003800545407f;
        status.mLights = digitalRead( Pin::Illumination );
        status.mIgnition = digitalRead( Pin::Ignition );
        status.mRearWindow = digitalRead( Pin::RearWindow );
        status.mTrunk = digitalRead( Pin::Trunk );
        status.mRoof = digitalRead( Pin::Roof );
        status.mHourButton = digitalRead( Pin::Hour ) ? false : true;
        status.mMinuteButton = digitalRead( Pin::Minute ) ? false : true;
        return status;
    }

    void Print( const CarStatus& car_status )
    {
        Serial.printf( "B: %f, Ill: %f (%i), IGN: %i, RW: %i, T: %i, R: %i, H: %i, M: %i\n", car_status.mBatteryVoltage,
                       car_status.mIlluminationVoltage, car_status.mLights, car_status.mIgnition, car_status.mRearWindow, car_status.mTrunk,
                       car_status.mRoof, car_status.mHourButton, car_status.mMinuteButton );
    }

    void SetTftBrightness( int brightness )
    {
        brightness = constrain( brightness, 0, 255 );
        ledcWrite( TftBacklightPwmChannel, brightness );
    }
}