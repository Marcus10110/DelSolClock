#include <Arduino.h>
#include "CarIO.h"
#include "pins.h"

namespace CarIO
{
    namespace
    {
        constexpr uint8_t TftBacklightPwmChannel = 0;
        constexpr uint8_t BuzzerPwmChannel = 1;
        bool TftPwmAttached = false;
        uint32_t BeeperStartMs = 0;
        bool BeeperActive = false;
        bool ToneActive = false;

        int PulseCount;
        int PitchHz;
        int PulseDurationMs;
        int PulsePeriodMs;
        int GroupPeriodMs;
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
        gpio_hold_dis( static_cast<gpio_num_t>( Pin::TftLit ) );
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
        ledcSetup( BuzzerPwmChannel, 440, 8 );        // channel 1, 440 hz, 8 bit resolution
        ledcAttachPin( Pin::TftLit, TftBacklightPwmChannel );
        TftPwmAttached = true;
        ledcAttachPin( Pin::Buzzer, BuzzerPwmChannel );

        ledcWrite( TftBacklightPwmChannel, 255 );
        ledcWrite( BuzzerPwmChannel, 0 );

        // set the ADC range to 0 to 800mV
        // analogSetAttenuation( ADC_0db );
        // by default: ADC_11db 2600 mV. (1V input = ADC reading of 1575
    }
    CarStatus GetStatus()
    {
        CarStatus status;
        int battery_code = analogRead( Pin::Battery );
        int illumination_code = analogRead( Pin::Illumination );
        pinMode( Pin::Illumination, INPUT ); // required to put the pin back into digital mode.
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
        if( brightness > 0 && !TftPwmAttached )
        {
            ledcAttachPin( Pin::TftLit, TftBacklightPwmChannel );
        }

        if( TftPwmAttached )
        {
            brightness = constrain( brightness, 0, 255 );
            ledcWrite( TftBacklightPwmChannel, brightness );
        }

        if( brightness == 0 && TftPwmAttached )
        {
            // let's just set that pin to 0.
            ledcDetachPin( Pin::TftLit );
            TftPwmAttached = false;
            digitalWrite( Pin::TftLit, 0 ); // drive low.
        }
    }

    void StartBeeper( int pulse_count, int pitch_hz, int pulse_duration_ms, int pulse_period_ms, int group_period_ms )
    {
        PulseCount = pulse_count;
        PitchHz = pitch_hz;
        PulseDurationMs = pulse_duration_ms;
        PulsePeriodMs = pulse_period_ms;
        GroupPeriodMs = group_period_ms;
        BeeperActive = true;
        ToneActive = false;
        ledcSetup( BuzzerPwmChannel, pitch_hz, 8 );
        ledcWrite( BuzzerPwmChannel, 0 );
        BeeperStartMs = millis();
    }
    void StopBeeper()
    {
        BeeperActive = false;
        ToneActive = false;
        ledcWrite( BuzzerPwmChannel, 0 );
    }

    void Service()
    {
        if( BeeperActive )
        {
            uint32_t elapsed_ms = millis() - BeeperStartMs;
            // determine if the tone should be on.
            elapsed_ms %= GroupPeriodMs;
            bool tone_on = false;
            if( elapsed_ms < ( PulsePeriodMs * PulseCount ) )
            {
                elapsed_ms %= PulsePeriodMs;
                if( elapsed_ms <= PulseDurationMs )
                {
                    tone_on = true;
                }
            }

            if( tone_on && !ToneActive )
            {
                ledcWrite( BuzzerPwmChannel, 128 );
                ToneActive = true;
            }
            else if( !tone_on && ToneActive )
            {
                ledcWrite( BuzzerPwmChannel, 0 );
                ToneActive = false;
            }
        }
    }
}