#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

// On-device GPS fix recorder. Captures one compact binary record per NMEA cycle
// (~1 Hz, fix or not) into a fixed-size RAM ring buffer so a road test can be
// downloaded over BLE and analyzed off-device for drift, signal outages, and
// parse failures.
//
// Record format (little-endian), see gps_recorder.cpp for the writer:
//   Common header (12 bytes), every record:
//     u8  flags          bit0 = hasFix, bit1 = dateValid
//     u8  satsUsed
//     u8  satsInView
//     u8  fixQuality     0 = none, 1 = GPS, 2 = DGPS
//     u32 millis         device uptime ms at the cycle (monotonic timestamp)
//     u16 hdop           HDOP * 100, 0xFFFF = invalid
//     u16 csumFailsDelta checksum-failed sentences since the previous record
//   Fix extension (+20 bytes), only when hasFix:
//     i32 lat_e7         latitude  degrees * 1e7
//     i32 lng_e7         longitude degrees * 1e7
//     i32 alt_cm         altitude, cm
//     u16 speed_cms      speed, cm/s
//     u16 course_cd      course, degrees * 100
//     u32 gpsTimeOfDay   TinyGPS time.value() = hhmmsscc UTC
//
// Download stream = a self-describing header followed by the records in
// chronological order (see SnapshotForDownload). The ring keeps the NEWEST data,
// dropping whole oldest records when full (never splits a record).

namespace GpsRecorder
{
    // Format version embedded in the download header. Bump on layout changes.
    constexpr uint8_t kFormatVersion = 1;

    void Begin();

    // Start clears the buffer and begins capturing. Stop halts capture (the
    // buffer is retained so it can still be downloaded).
    void Start();
    void Stop();
    bool IsRecording();

    // Call once per NMEA cycle (when Gps reports the time field updated). Pulls
    // the current fields from the Gps module and appends one record. No-op when
    // not recording.
    void OnGpsCycle();

    size_t RecordCount();
    size_t ByteCount();      // bytes of record data currently buffered
    size_t DroppedRecords(); // whole records discarded due to overrun

    // Linearize the header + buffered records into `out` (chronological order),
    // ready to chunk over BLE. Returns the total byte count written. Pauses
    // capture for a consistent snapshot is the caller's responsibility (the BLE
    // layer stops recording before arming a download).
    size_t SnapshotForDownload( std::vector<uint8_t>& out );
}
