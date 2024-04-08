#include "quarter_mile.h"
#include "screens.h"
#include "display.h"
#include "tft.h"
#include "motion.h"

#include <TinyGPSPlus.h>

#include <optional>

namespace QuarterMile
{
    enum class QuarterMileState
    {
        Start,
        Launch,
        InProgress,
        Summary,
    };
    namespace
    {
        QuarterMileState mState{ QuarterMileState::Start };

        struct Location
        {
            double mLatitude{ 0 };
            double mLongitude{ 0 };
        };

        std::optional<Location> mStartLocation;
        uint32_t mStartTimeMs{ 0 };
        Screens::QuarterMile::Summary gSummary;
    }

    void Reset()
    {
        mState = QuarterMileState::Start;
        mStartLocation = std::nullopt;
        mStartTimeMs = 0;
        gSummary = Screens::QuarterMile::Summary();
    }

    void Service( Display::Display* display, Tft::Tft* tft, CarIO::ButtonEvents button_events, TinyGPSPlus* gps )
    {
        // we'll probably draw the screen from here.
        if( mState == QuarterMileState::Start )
        {
            Screens::QuarterMile::Start start;
            start.Draw( display );
            tft->DrawCanvas( display );

            if( button_events.mMinuteButtonPressed )
            {
                mState = QuarterMileState::Launch;
            }
        }
        else if( mState == QuarterMileState::Launch )
        {
            Screens::QuarterMile::Launch launch;
            launch.Draw( display );
            tft->DrawCanvas( display );
            // wait for some g threshold to detect launch.
            // note, we need forward direction specifically!
            // for now, just press the minute button to simulate launch.

            if( button_events.mMinuteButtonPressed )
            {
                mState = QuarterMileState::InProgress;
                mStartTimeMs = millis();
                if( gps->location.isValid() )
                {
                    // store the start location.
                    mStartLocation = Location{ gps->location.lat(), gps->location.lng() };
                }
            }
        }
        else if( mState == QuarterMileState::InProgress )
        {
            auto elapsed_ms = millis() - mStartTimeMs;
            auto acceleration = Motion::GetState();
            double distance_miles = 0;
            if( mStartLocation.has_value() && gps->location.isValid() )
            {
                distance_miles = TinyGPSPlus::distanceBetween( mStartLocation->mLatitude, mStartLocation->mLongitude, gps->location.lat(),
                                                               gps->location.lng() ) *
                                 0.000621371;
            }
            Screens::QuarterMile::InProgress in_progress;
            in_progress.mTimeSec = elapsed_ms / 1000.0;

            in_progress.mAccelerationG = acceleration.mForward;
            in_progress.mDistanceMiles = distance_miles;
            in_progress.mSpeedMph = gps->speed.isValid() ? gps->speed.mph() : 0;

            gSummary.mMaxAccelerationG = std::max( gSummary.mMaxAccelerationG, in_progress.mAccelerationG );
            gSummary.mMaxSpeedMph = std::max( gSummary.mMaxSpeedMph, in_progress.mSpeedMph );

            if( gSummary.mZeroSixtyTimeSec == 0 && in_progress.mSpeedMph >= 60.0 )
            {
                gSummary.mZeroSixtyTimeSec = in_progress.mTimeSec;
            }

            in_progress.Draw( display );
            tft->DrawCanvas( display );

            if( distance_miles >= 0.25 )
            {
                gSummary.mQuarterMileTimeSec = in_progress.mTimeSec;
                mState = QuarterMileState::Summary;
            }

            // press M to cancel.
            if( button_events.mMinuteButtonPressed )
            {
                Reset();
            }
        }
        else if( mState == QuarterMileState::Summary )
        {
            gSummary.Draw( display );
            tft->DrawCanvas( display );

            // pressing M will start over. press H to exit.
            if( button_events.mMinuteButtonPressed )
            {
                Reset();
            }
        }
    }
}