#include "demo.h"
#include "demo_screens.h"
#include "tft.h"
#include "logger.h"
#include "utilities.h"

#include <Arduino.h>

namespace Demo
{
    // Render the shared demo screens (same list UiDesigner uses) in sequence.
    // Requires the firmware to be built with DEMO_ENABLED; otherwise the shared
    // registry is empty and this draws nothing.
    void Demo( GFXcanvas16* display, Tft::Tft* tft )
    {
        for( const auto& screen : demo::GetDemoScreens() )
        {
            screen.draw( display );
            tft->DrawCanvas( display );
            PRINT_MEMORY_USAGE();
            delay( 3000 );
        }
    }
}
