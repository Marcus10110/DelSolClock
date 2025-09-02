#include "utilities.h"
#include "logger.h"

namespace Utilities
{
    void PrintMemory()
    {
        LOG_TRACE( "heap: %u, internal: %u, largest internal: %u, psram: %u", ( unsigned )ESP.getFreeHeap(),
                   ( unsigned )heap_caps_get_free_size( MALLOC_CAP_INTERNAL ),
                   ( unsigned )heap_caps_get_largest_free_block( MALLOC_CAP_INTERNAL ), ( unsigned )ESP.getFreePsram() );
    }
}