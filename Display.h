#pragma once
#include <Arduino.h>

namespace AppleMediaService
{
    struct MediaInformation;
}
namespace Display
{
    enum class Icon
    {
        Headlight,
        Bluetooth,
        Shuffle,
        Repeat
    };

    struct DisplayState
    {
        uint8_t mHours24{ 0 };
        uint8_t mMinutes{ 0 };
        float mSpeed{ 0 };
        bool mIconHeadlight{ false };
        bool mIconBluetooth{ false };
        bool mIconShuffle{ false };
        bool mIconRepeat{ false };
        std::string mMediaTitle{ "" };
        std::string mMediaArtist{ "" };
        bool operator==( const DisplayState& rhs ) const;
        bool operator!=( const DisplayState& rhs ) const;
        void Dump() const;
    };

    void Begin();
    void WriteDisplay(); // due to the canvas buffering to prevent flicker, this function must be called to actually update the display.
    void DrawSplash();
    void DrawLightAlarm();
    void Clear(); // clears the buffer, but doe not actually clear the display. follow with WriteDisplay to blank the screen.
    void DrawTime( uint8_t hours24, uint8_t minutes );
    void DrawSpeed( float speed );
    void DrawIcon( Icon icon, bool visible );
    void DrawMediaInfo( const std::string& title, const std::string& artist );
    void DrawDebugInfo( const std::string& message, bool center, bool fine_print );
    void EnableSleep( bool sleep );
    const DisplayState& GetState();
    void DrawState( const DisplayState& state );
}