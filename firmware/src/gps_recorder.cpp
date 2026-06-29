#include "gps_recorder.h"

#include <Arduino.h>  // millis()
#include <TinyGPSPlus.h>

#include <cstdlib>  // malloc/free
#include <cstring>

#include "gps.h"
#include "logger.h"

namespace GpsRecorder
{
    namespace
    {
        // Ring buffer size. At ~1 Hz a no-fix record is 12 bytes and a fix
        // record is 32 bytes, so 32 KB holds roughly 17-45 minutes depending on
        // the fix ratio. The buffer is allocated lazily on Start() and freed on
        // Stop()/download so it costs ZERO RAM at boot — important because the
        // splash-screen GFXcanvas16 needs a contiguous 64 KB malloc at startup,
        // and a permanent static reservation here was starving it (null buffer
        // -> LoadProhibited crash in DrawCanvas).
        constexpr size_t kBufferSize = 32 * 1024;

        constexpr size_t kHeaderBytes = 12;  // common header, every record
        constexpr size_t kFixBytes = 20;     // fix extension
        constexpr size_t kMaxRecordBytes = kHeaderBytes + kFixBytes;

        constexpr uint8_t kFlagHasFix = 0x01;
        constexpr uint8_t kFlagDateValid = 0x02;

        // Heap-allocated on Start(), freed when not recording / after a download
        // is armed-and-snapshotted. Null when not recording.
        uint8_t* gBuffer = nullptr;
        size_t gHead = 0;     // write offset (next byte to write)
        size_t gTail = 0;     // read offset (oldest byte)
        size_t gUsed = 0;     // bytes currently stored
        size_t gCount = 0;    // records currently stored
        size_t gDropped = 0;  // whole records evicted due to overrun

        bool gRecording = false;

        // Checksum-fail counter at the previous cycle, to compute per-record
        // deltas. Reset on Start.
        uint32_t gPrevCsumFails = 0;

        void ResetState()
        {
            gHead = gTail = gUsed = gCount = gDropped = 0;
            gPrevCsumFails = 0;
        }

        void RingWriteByte( uint8_t b )
        {
            gBuffer[ gHead ] = b;
            gHead = ( gHead + 1 ) % kBufferSize;
            ++gUsed;
        }

        // Read the record length stored at ring offset `off` (peeking its flags
        // byte). Header is always present; the fix extension is present when the
        // hasFix flag is set.
        size_t RecordLenAt( size_t off )
        {
            uint8_t flags = gBuffer[ off ];  // flags is the first byte
            return kHeaderBytes + ( ( flags & kFlagHasFix ) ? kFixBytes : 0 );
        }

        // Evict the oldest record to make room. Advances the tail by one whole
        // record and bumps the dropped counter.
        void EvictOldest()
        {
            size_t len = RecordLenAt( gTail );
            gTail = ( gTail + len ) % kBufferSize;
            gUsed -= len;
            --gCount;
            ++gDropped;
        }

        // Little-endian append helpers (operate on the ring via RingWriteByte).
        void PutU8( uint8_t v ) { RingWriteByte( v ); }
        void PutU16( uint16_t v )
        {
            RingWriteByte( v & 0xFF );
            RingWriteByte( ( v >> 8 ) & 0xFF );
        }
        void PutU32( uint32_t v )
        {
            RingWriteByte( v & 0xFF );
            RingWriteByte( ( v >> 8 ) & 0xFF );
            RingWriteByte( ( v >> 16 ) & 0xFF );
            RingWriteByte( ( v >> 24 ) & 0xFF );
        }
        void PutI32( int32_t v ) { PutU32( static_cast<uint32_t>( v ) ); }

        // Little-endian append into a std::vector (for the download snapshot).
        void VecU8( std::vector<uint8_t>& o, uint8_t v ) { o.push_back( v ); }
        void VecU16( std::vector<uint8_t>& o, uint16_t v )
        {
            o.push_back( v & 0xFF );
            o.push_back( ( v >> 8 ) & 0xFF );
        }
        void VecU32( std::vector<uint8_t>& o, uint32_t v )
        {
            o.push_back( v & 0xFF );
            o.push_back( ( v >> 8 ) & 0xFF );
            o.push_back( ( v >> 16 ) & 0xFF );
            o.push_back( ( v >> 24 ) & 0xFF );
        }
    }

    namespace
    {
        void FreeBuffer()
        {
            if( gBuffer )
            {
                free( gBuffer );
                gBuffer = nullptr;
            }
            ResetState();
        }
    }

    void Begin()
    {
        // Allocate nothing at boot — the buffer is created on Start(). This keeps
        // the heap free for the splash-screen canvas's 64 KB malloc.
        gBuffer = nullptr;
        ResetState();
        gRecording = false;
    }

    void Start()
    {
        // Free any prior buffer (e.g. a finished session not yet downloaded) and
        // allocate a fresh one. If the allocation fails, recording stays off.
        FreeBuffer();
        gBuffer = static_cast<uint8_t*>( malloc( kBufferSize ) );
        if( !gBuffer )
        {
            LOG_ERROR( "GpsRecorder: failed to allocate %u byte buffer; not recording",
                       ( unsigned )kBufferSize );
            gRecording = false;
            return;
        }
        ResetState();
        // Baseline the checksum-fail counter so the first record's delta is 0.
        TinyGPSPlus* gps = Gps::GetGps();
        gPrevCsumFails = gps ? static_cast<uint32_t>( gps->failedChecksum() ) : 0;
        gRecording = true;
        LOG_INFO( "GpsRecorder: recording started (buffer %u bytes)", ( unsigned )kBufferSize );
    }

    void Stop()
    {
        // Keep the buffer + data so it can still be downloaded; just halt capture.
        gRecording = false;
        LOG_INFO( "GpsRecorder: recording stopped (%u records, %u bytes, %u dropped)",
                  ( unsigned )gCount, ( unsigned )gUsed, ( unsigned )gDropped );
    }

    bool IsRecording() { return gRecording; }

    void OnGpsCycle()
    {
        if( !gRecording || !gBuffer ) return;
        TinyGPSPlus* gps = Gps::GetGps();
        if( !gps ) return;

        Gps::Debug dbg = Gps::GetDebug();

        const bool hasFix =
            gps->location.isValid() && gps->location.age() < 5000;
        const bool dateValid = gps->date.isValid();

        // Checksum-fail delta since the previous record (saturate to u16).
        uint32_t csumNow = static_cast<uint32_t>( gps->failedChecksum() );
        uint32_t csumDelta = csumNow - gPrevCsumFails;
        gPrevCsumFails = csumNow;
        if( csumDelta > 0xFFFF ) csumDelta = 0xFFFF;

        uint16_t hdop = 0xFFFF;
        if( gps->hdop.isValid() )
        {
            double h = gps->hdop.hdop() * 100.0;
            if( h < 0 ) h = 0;
            if( h > 0xFFFE ) h = 0xFFFE;
            hdop = static_cast<uint16_t>( h );
        }

        uint8_t flags = 0;
        if( hasFix ) flags |= kFlagHasFix;
        if( dateValid ) flags |= kFlagDateValid;

        const size_t recLen = kHeaderBytes + ( hasFix ? kFixBytes : 0 );

        // Make room: evict whole oldest records until this one fits. (recLen is
        // always <= kMaxRecordBytes << kBufferSize, so this terminates.)
        while( kBufferSize - gUsed < recLen && gCount > 0 )
        {
            EvictOldest();
        }
        if( kBufferSize - gUsed < recLen )
        {
            // Should never happen given the size constants, but guard anyway.
            return;
        }

        // --- common header (12 bytes) ---
        PutU8( flags );
        PutU8( static_cast<uint8_t>( dbg.satsUsed > 255 ? 255 : dbg.satsUsed ) );
        PutU8( static_cast<uint8_t>( dbg.satsInView > 255 ? 255 : dbg.satsInView ) );
        PutU8( static_cast<uint8_t>( dbg.fixQuality > 255 ? 255 : dbg.fixQuality ) );
        PutU32( millis() );
        PutU16( hdop );
        PutU16( static_cast<uint16_t>( csumDelta ) );

        // --- fix extension (20 bytes) ---
        if( hasFix )
        {
            PutI32( static_cast<int32_t>( gps->location.lat() * 1e7 ) );
            PutI32( static_cast<int32_t>( gps->location.lng() * 1e7 ) );
            double altM = gps->altitude.isValid() ? gps->altitude.meters() : 0.0;
            PutI32( static_cast<int32_t>( altM * 100.0 ) );
            double spd = gps->speed.isValid() ? gps->speed.mps() * 100.0 : 0.0;
            if( spd < 0 ) spd = 0;
            if( spd > 0xFFFF ) spd = 0xFFFF;
            PutU16( static_cast<uint16_t>( spd ) );
            double crs = gps->course.isValid() ? gps->course.deg() * 100.0 : 0.0;
            if( crs < 0 ) crs = 0;
            if( crs > 0xFFFF ) crs = 0xFFFF;
            PutU16( static_cast<uint16_t>( crs ) );
            PutU32( gps->time.isValid() ? gps->time.value() : 0 );
        }

        ++gCount;
    }

    size_t RecordCount() { return gCount; }
    size_t ByteCount() { return gUsed; }
    size_t DroppedRecords() { return gDropped; }

    size_t SnapshotForDownload( std::vector<uint8_t>& out )
    {
        out.clear();
        out.reserve( gUsed + 24 );

        // Self-describing download header (20 bytes):
        //   magic "DSGPS\0" (6) | u8 formatVersion | u8 reserved
        //   u32 recordCount | u32 totalRecordBytes | u32 droppedRecords
        const char magic[ 6 ] = { 'D', 'S', 'G', 'P', 'S', '\0' };
        for( char c : magic ) out.push_back( static_cast<uint8_t>( c ) );
        VecU8( out, kFormatVersion );
        VecU8( out, 0 );  // reserved
        VecU32( out, static_cast<uint32_t>( gCount ) );
        VecU32( out, static_cast<uint32_t>( gUsed ) );
        VecU32( out, static_cast<uint32_t>( gDropped ) );

        // Records in chronological order, from tail to head. (gBuffer can be
        // null if a download is armed before anything was ever recorded.)
        if( gBuffer )
        {
            size_t off = gTail;
            for( size_t remaining = gUsed; remaining > 0; --remaining )
            {
                out.push_back( gBuffer[ off ] );
                off = ( off + 1 ) % kBufferSize;
            }
        }
        return out.size();
    }
}
