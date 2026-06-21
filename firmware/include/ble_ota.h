#pragma once
#include <cstdint>
#include <string>

class BLEServer;

namespace BleOta
{
    void Begin( BLEServer* server );
    bool IsInProgress( uint32_t* bytes_received );
    bool IsRebootRequired();

    // Compute the SHA-256 of the raw SPIFFS partition (the full padded image, so
    // the result matches sha256(spiffs.bin) from a release build). Reads the raw
    // flash bytes via esp_partition_read — NOT the mounted filesystem.
    // Returns the hash as a lowercase hex string, or "" on error. If
    // out_elapsed_ms is non-null, the wall-clock compute time is written to it.
    std::string HashSpiffsPartition( uint32_t* out_elapsed_ms = nullptr );
}