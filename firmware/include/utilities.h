#pragma once

namespace Utilities
{
    void PrintMemory();
}

// Define macro that prints memory usage directly


#define PRINT_MEMORY_USAGE()                                                                                                               \
    LOG_TRACE( "heap: %u, internal: %u", ( unsigned )ESP.getFreeHeap(), ( unsigned )heap_caps_get_free_size( MALLOC_CAP_INTERNAL ) )

// #define PRINT_MEMORY_USAGE()                                                                                                               \
//     LOG_TRACE( "heap: %u, internal: %u, largest internal: %u, psram: %u", ( unsigned )ESP.getFreeHeap(),                                   \
//                ( unsigned )heap_caps_get_free_size( MALLOC_CAP_INTERNAL ),                                                                 \
//                ( unsigned )heap_caps_get_largest_free_block( MALLOC_CAP_INTERNAL ), ( unsigned )ESP.getFreePsram() )

#define PRINT_MEMORY_USAGE_MSG( description )                                                                                              \
    LOG_TRACE( "%s heap: %u, internal: %u", description, ( unsigned )ESP.getFreeHeap(),                                                    \
               ( unsigned )heap_caps_get_free_size( MALLOC_CAP_INTERNAL ) )

// #define PRINT_MEMORY_USAGE_MSG( description )                                                                                              \
//     LOG_TRACE( "%s heap: %u, internal: %u, largest internal: %u, psram: %u", description, ( unsigned )ESP.getFreeHeap(),                   \
//                ( unsigned )heap_caps_get_free_size( MALLOC_CAP_INTERNAL ),                                                                 \
//                ( unsigned )heap_caps_get_largest_free_block( MALLOC_CAP_INTERNAL ), ( unsigned )ESP.getFreePsram() )
