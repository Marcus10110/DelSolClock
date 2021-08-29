#pragma once
#include <Arduino.h>

namespace AppleMediaService
{
    struct MediaInformation;
}
namespace Display
{
    void Begin();
    void Clear();
    void DrawTime( uint8_t hours24, uint8_t minutes );
    void DrawSpeed( float speed );
    void DrawMediaInfo( const AppleMediaService::MediaInformation& media_info );
}