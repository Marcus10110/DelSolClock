#pragma once
#include <string>
#include <cstdint>

#include "display.h"

namespace Screens
{
    class Screen
    {
      public:
        virtual void Draw( Display::Display* display ) = 0;
    };


    class Splash : public Screen
    {
      public:
        void Draw( Display::Display* display ) override;
    };

    class Clock : public Screen
    {
      public:
        uint8_t mHours24{ 0 };
        uint8_t mMinutes{ 0 };
        float mSpeed{ 0 };
        bool mHeadlight{ false };
        bool mBluetooth{ false };
        std::string mMediaTitle{ "" };
        std::string mMediaArtist{ "" };

        void Draw( Display::Display* display ) override;
    };

    class LightsAlarm : public Screen
    {
      public:
        void Draw( Display::Display* display ) override;
    };

    class Discoverable : public Screen
    {
      public:
        std::string mBluetoothName{ "" };
        void Draw( Display::Display* display ) override;
    };

    class Status : public Screen
    {
      public:
        double mLatitude;
        double mLongitude;
        double mSpeedMph;
        double mHeadingDegrees;
        double mBatteryVolts;

        void Draw( Display::Display* display ) override;
    };

    class OtaInProgress : public Screen
    {
      public:
        void Draw( Display::Display* display ) override;
        uint32_t mBytesReceived;
    };

    namespace QuarterMile
    {
        class Start : public Screen
        {
          public:
            void Draw( Display::Display* display ) override;
        };
        class Launch : public Screen
        {
          public:
            double mAccelerationG;
            void Draw( Display::Display* display ) override;
        };
        class InProgress : public Screen
        {
          public:
            double mTimeSec;
            double mAccelerationG;
            double mDistanceMiles;
            double mSpeedMph;
            void Draw( Display::Display* display ) override;
        };
        class Summary : public Screen
        {
          public:
            double mQuarterMileTimeSec;
            double mZeroSixtyTimeSec;
            double mMaxAccelerationG;
            double mMaxSpeedMph;
            void Draw( Display::Display* display ) override;
        };
    }


    class FontTest : public Screen
    {
      public:
        void Draw( Display::Display* display ) override;
    };

}