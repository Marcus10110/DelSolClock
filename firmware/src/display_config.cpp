#include "display_config.h"

#include <Preferences.h>

#include "draw_helpers.h"
#include "logger.h"

namespace DisplayConfig
{
    namespace
    {
        Preferences Prefs;
        BezelOffsets Current;
        constexpr int8_t kMaxOffset = 40;  // px the bezel could plausibly hide

        int8_t Clamp( int v )
        {
            if( v > kMaxOffset ) return kMaxOffset;
            if( v < 0 ) return 0;  // insets are non-negative (can't expand the panel)
            return static_cast<int8_t>( v );
        }

        // Push the current insets into the display lib so all layout reflows.
        void Apply()
        {
            display::SetBezelInsets( Current.top, Current.bottom, Current.left,
                                     Current.right );
        }
    }

    void Begin()
    {
        Prefs.begin( "display", false );  // namespace "display", read-write
        Current.top = Clamp( Prefs.getChar( "bz_top", 0 ) );
        Current.bottom = Clamp( Prefs.getChar( "bz_bot", 0 ) );
        Current.left = Clamp( Prefs.getChar( "bz_left", 0 ) );
        Current.right = Clamp( Prefs.getChar( "bz_right", 0 ) );
        Apply();
        LOG_INFO( "display: bezel insets t=%d b=%d l=%d r=%d", Current.top,
                  Current.bottom, Current.left, Current.right );
    }

    const BezelOffsets& Get() { return Current; }

    void Set( const BezelOffsets& offsets )
    {
        Current.top = Clamp( offsets.top );
        Current.bottom = Clamp( offsets.bottom );
        Current.left = Clamp( offsets.left );
        Current.right = Clamp( offsets.right );
        Apply();  // live
        Prefs.putChar( "bz_top", Current.top );
        Prefs.putChar( "bz_bot", Current.bottom );
        Prefs.putChar( "bz_left", Current.left );
        Prefs.putChar( "bz_right", Current.right );
        LOG_INFO( "display: bezel insets set t=%d b=%d l=%d r=%d", Current.top,
                  Current.bottom, Current.left, Current.right );
    }
}
