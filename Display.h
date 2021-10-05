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
    void Begin();
    void Clear();
    void DrawTime( uint8_t hours24, uint8_t minutes );
    void DrawSpeed( float speed );
    void DrawIcon( Icon icon, bool visible );
    void DrawMediaInfo( const AppleMediaService::MediaInformation& media_info );
    void DrawDebugInfo( const std::string& message );
    void EnableSleep( bool sleep );
}