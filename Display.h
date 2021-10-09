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
        float mSpead{ 0 };
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
    void DrawSplash();
    void DrawLightAlarm();
    void Clear();
    void DrawTime( uint8_t hours24, uint8_t minutes );
    void DrawSpeed( float speed );
    void DrawIcon( Icon icon, bool visible );
    void DrawMediaInfo( const std::string& title, const std::string& artist );
    void DrawDebugInfo( const std::string& message, bool center );
    void EnableSleep( bool sleep );
    const DisplayState& GetState();
    void DrawState( const DisplayState& state );
}