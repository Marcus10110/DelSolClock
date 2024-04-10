#include <Arduino.h>
#include "car_io.h"
#include "pins.h"
#include "logger.h"

#include <optional>

namespace CarIO
{
    namespace
    {
        constexpr uint8_t BuzzerPwmChannel = 1;
        uint32_t BeeperStartMs = 0;
        bool BeeperActive = false;
        bool ToneActive = false;

        int PulseCount;
        int PitchHz;
        int PulseDurationMs;
        int PulsePeriodMs;
        int GroupPeriodMs;

        std::optional<uint32_t> mMinuteDownMs{ std::nullopt };
        std::optional<uint32_t> mHourDownMs{ std::nullopt };
    }
    std::string CarStatus::ToString()
    {
        char display_string[ 128 ] = { 0 };
        snprintf( display_string, sizeof( display_string ), "B: %f, Ill: %f (%i), IGN: %i, RW: %i, T: %i, R: %i, H: %i, M: %i",
                  mBatteryVoltage, mIlluminationVoltage, mLights, mIgnition, mRearWindow, mTrunk, mRoof, mHourButton, mMinuteButton );
        return display_string;
    }
    void Setup()
    {
        LOG_TRACE( "CarIO::Setup()" );
        gpio_hold_dis( static_cast<gpio_num_t>( Pin::TftPower ) );
        pinMode( Pin::TftPower, OUTPUT );
        pinMode( Pin::Battery, INPUT );
        pinMode( Pin::Illumination, INPUT );
        pinMode( Pin::Ignition, INPUT );
        pinMode( Pin::RearWindow, INPUT );
        pinMode( Pin::Trunk, INPUT );
        pinMode( Pin::Roof, INPUT );
        pinMode( Pin::Hour, INPUT );
        pinMode( Pin::Minute, INPUT );
        pinMode( Pin::Buzzer, OUTPUT );

        ledcSetup( BuzzerPwmChannel, 440, 8 ); // channel 1, 440 hz, 8 bit resolution
        ledcAttachPin( Pin::Buzzer, BuzzerPwmChannel );
        ledcWrite( BuzzerPwmChannel, 0 );

        // set the ADC range to 0 to 800mV
        // analogSetAttenuation( ADC_0db );
        // by default: ADC_11db 2600 mV. (1V input = ADC reading of 1575
    }
    CarStatus GetStatus()
    {
        CarStatus status;
        uint16_t battery_code = analogRead( Pin::Battery );
        uint16_t illumination_code = analogRead( Pin::Illumination );
        {
            constexpr float a = 3.56743E-06;
            constexpr float b = -0.007518604;
            constexpr float c = 8.781351365;
            float x = static_cast<float>( battery_code );
            status.mBatteryVoltage = ( a * x * x ) + ( b * x ) + c;
        }
        pinMode( Pin::Illumination, INPUT ); // required to put the pin back into digital mode.

        // Volts Per Code (Oct 2 2021) 0.0202547273
        // status.mBatteryVoltage = battery_code * 0.0202547273f;
        // volts per code (oct 2 2021) 0.003800545407
        status.mIlluminationVoltage = illumination_code * 0.003800545407f;
        status.mLights = digitalRead( Pin::Illumination );
        status.mIgnition = digitalRead( Pin::Ignition );
        status.mRearWindow = digitalRead( Pin::RearWindow );
        status.mTrunk = digitalRead( Pin::Trunk );
        status.mRoof = digitalRead( Pin::Roof );
        status.mHourButton = digitalRead( Pin::Hour ) ? false : true;
        status.mMinuteButton = digitalRead( Pin::Minute ) ? false : true;

        // debug for calibration
        // LOG_TRACE( "%u", battery_code );
        // LOG_TRACE( "battery code: %u [%f], lights code: %u [%f]", battery_code, status.mBatteryVoltage, illumination_code,
        //               status.mIlluminationVoltage );

        return status;
    }

    void Print( const CarStatus& car_status )
    {
        LOG_INFO( "B: %f, Ill: %f (%i), IGN: %i, RW: %i, T: %i, R: %i, H: %i, M: %i", car_status.mBatteryVoltage,
                  car_status.mIlluminationVoltage, car_status.mLights, car_status.mIgnition, car_status.mRearWindow, car_status.mTrunk,
                  car_status.mRoof, car_status.mHourButton, car_status.mMinuteButton );
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

    // check to see if there is a button press event and return it.
    ButtonEvents GetButtonEvents()
    {
        // if the previous state was low, the low duration was > 100ms, and the new state is high, we have an event.
        auto now = millis();
        auto state = GetStatus();
        ButtonEvents events;
        constexpr uint32_t ButtonDownMsMinimum = 100;

        if( !mMinuteDownMs.has_value() && state.mMinuteButton )
        {
            mMinuteDownMs = now;
        }
        else if( mMinuteDownMs.has_value() && !state.mMinuteButton )
        {
            if( now - mMinuteDownMs.value() > ButtonDownMsMinimum )
            {
                events.mMinuteButtonPressed = true;
            }
            mMinuteDownMs = std::nullopt;
        }
        if( !mHourDownMs.has_value() && state.mHourButton )
        {
            mHourDownMs = now;
        }
        else if( mHourDownMs.has_value() && !state.mHourButton )
        {
            if( now - mHourDownMs.value() > ButtonDownMsMinimum )
            {
                events.mHourButtonPressed = true;
            }
            mHourDownMs = std::nullopt;
        }

        return events;
    }
}