#pragma once
#include <cstdint>

// Persistent display tuning: the physical bezel hides a few pixels around each
// edge, so the rendered image is shifted to center it in the visible window.
// Offsets are INSETS per side (positive = push content away from that edge).
// Stored in NVS so they survive reboots; tunable live over BLE.
namespace DisplayConfig
{
    struct BezelOffsets
    {
        int8_t top{ 0 };
        int8_t bottom{ 0 };
        int8_t left{ 0 };
        int8_t right{ 0 };
    };

    // Load persisted offsets from NVS (call once at boot). Missing => zeros.
    void Begin();

    // Current offsets (cached in RAM).
    const BezelOffsets& Get();

    // Set + persist to NVS, and apply live. Values are clamped to [0, max]:
    // insets shrink the drawable area, so they're non-negative.
    void Set( const BezelOffsets& offsets );
}
