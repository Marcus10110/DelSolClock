#include "ble_ota.h"
#include "version.h"
#include "logger.h"
#include "utilities.h"

#include <Arduino.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <mutex>

#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "mbedtls/sha256.h"
#include <Update.h>

#define FIRMWARE_UPDATE_SERVICE_UUID "69da0f2b-43a4-4c2a-b01d-0f11564c732b"
#define FIRMWARE_UPDATE_WRITE_CHARACTERISTIC_UUID "7efc013a-37b7-44da-8e1c-06e28256d83b"
#define FIRMWARE_UPDATE_VERSION_CHARACTERISTIC_UUID "a5c0d67a-9576-47ea-85c6-0347f8473cf3"


namespace BleOta
{
    namespace
    {
        // Protocol version reported in the version characteristic. Bump when the
        // wire format (header packet / commands / version-string layout) changes.
        constexpr int ProtocolVersion = 2;

        // First-packet command bytes (the 5-byte header is [command:1][total_size:4 LE]).
        constexpr uint8_t CmdWriteApp = 0x01;
        constexpr uint8_t CmdWriteSpiffs = 0x02;
        // 0x03 = cancel — reserved, not implemented in protocol 2.

        constexpr size_t HeaderSize = 5;

        enum class Target
        {
            None,
            App,
            Spiffs,
        };

        esp_ota_handle_t UpdateHandle = 0;

        uint32_t PacketsReceived{ 0 };
        uint32_t FirstPacketTimeMs{ 0 };
        uint32_t LastPacketTimeMs{ 0 };

        constexpr uint32_t FullOtaPacketSize = 512;

        // cross thread data
        std::mutex Mutex;
        uint32_t BytesReceived{ 0 }; // image bytes received so far (excludes the header)
        bool RebootRequired{ false };
        uint32_t RebootMinTimeMs{ 0 };

        // Per-upload state. Set from the header packet; reset when an upload
        // begins, completes, or errors. Touched only on the BLE callback thread.
        Target CurrentTarget{ Target::None };
        uint32_t DeclaredSize{ 0 };

        // Tear down any in-flight upload handle so the next one starts clean.
        // (App OTA reboots on success, but FS OTA could be retried, and a failed
        // upload must not strand a half-open handle.)
        void AbortInFlight()
        {
            if( CurrentTarget == Target::App && UpdateHandle != 0 )
            {
                esp_ota_abort( UpdateHandle );
            }
            else if( CurrentTarget == Target::Spiffs && Update.isRunning() )
            {
                Update.abort();
            }
            UpdateHandle = 0;
            CurrentTarget = Target::None;
            DeclaredSize = 0;
        }

        void ResetUploadState()
        {
            std::scoped_lock lock( Mutex );
            BytesReceived = 0;
            PacketsReceived = 0;
            FirstPacketTimeMs = 0;
            LastPacketTimeMs = 0;
        }

        // Begin a new upload from a parsed header. Returns true on success.
        bool BeginUpload( uint8_t command, uint32_t total_size )
        {
            AbortInFlight(); // clean slate, even mid-way through a prior upload.
            ResetUploadState();

            if( command == CmdWriteApp )
            {
                esp_err_t status = esp_ota_begin( esp_ota_get_next_update_partition( NULL ),
                                                  total_size > 0 ? total_size : OTA_SIZE_UNKNOWN, &UpdateHandle );
                if( status != ESP_OK )
                {
                    LOG_ERROR( "esp_ota_begin failed: %i", status );
                    return false;
                }
                CurrentTarget = Target::App;
                DeclaredSize = total_size;
                LOG_TRACE( "BeginUpload: app, %u bytes", total_size );
                return true;
            }
            if( command == CmdWriteSpiffs )
            {
                // U_SPIFFS targets the SPIFFS data partition. Update wants a size;
                // the declared total is the (full, padded) image size.
                if( !Update.begin( total_size, U_SPIFFS ) )
                {
                    LOG_ERROR( "Update.begin(U_SPIFFS) failed: %i", Update.getError() );
                    return false;
                }
                CurrentTarget = Target::Spiffs;
                DeclaredSize = total_size;
                LOG_TRACE( "BeginUpload: spiffs, %u bytes", total_size );
                return true;
            }

            LOG_ERROR( "BeginUpload: unsupported command 0x%02x", command );
            return false;
        }

        // Write a chunk of image data to the in-flight target. Returns true on success.
        bool WriteChunk( const uint8_t* data, size_t length )
        {
            if( length == 0 )
            {
                return true;
            }
            if( CurrentTarget == Target::App )
            {
                esp_err_t status = esp_ota_write( UpdateHandle, data, length );
                if( status != ESP_OK )
                {
                    LOG_ERROR( "esp_ota_write: %i", status );
                    return false;
                }
                return true;
            }
            if( CurrentTarget == Target::Spiffs )
            {
                if( Update.write( const_cast<uint8_t*>( data ), length ) != length )
                {
                    LOG_ERROR( "Update.write failed: %i", Update.getError() );
                    return false;
                }
                return true;
            }
            return false;
        }

        // Finalize the in-flight upload. Returns true on success; on app success
        // also sets the boot partition. The caller handles the reboot.
        bool FinishUpload()
        {
            if( CurrentTarget == Target::App )
            {
                esp_err_t status = esp_ota_end( UpdateHandle );
                UpdateHandle = 0;
                if( status != ESP_OK )
                {
                    LOG_ERROR( "esp_ota_end error: %i", status );
                    CurrentTarget = Target::None;
                    return false;
                }
                status = esp_ota_set_boot_partition( esp_ota_get_next_update_partition( NULL ) );
                LOG_TRACE( "esp_ota_set_boot_partition: %i", status );
                CurrentTarget = Target::None;
                return status == ESP_OK;
            }
            if( CurrentTarget == Target::Spiffs )
            {
                bool ok = Update.end( true ); // true = the declared size was fully written
                if( !ok )
                {
                    LOG_ERROR( "Update.end(U_SPIFFS) error: %i", Update.getError() );
                }
                CurrentTarget = Target::None;
                return ok;
            }
            return false;
        }

        class OtaWriteCallbacks : public BLECharacteristicCallbacks
        {
          public:
            void onWrite( BLECharacteristic* characteristic ) override
            {
                auto start_time = millis();
                std::string rxData = characteristic->getValue();
                const uint8_t* bytes = reinterpret_cast<const uint8_t*>( rxData.data() );

                bool is_first_packet;
                {
                    std::scoped_lock lock( Mutex );
                    is_first_packet = ( BytesReceived == 0 && CurrentTarget == Target::None );
                }

                // The first packet of an upload is a 5-byte header: [command][size LE].
                if( is_first_packet )
                {
                    if( rxData.length() < HeaderSize )
                    {
                        LOG_ERROR( "OTA header too short: %u bytes", ( unsigned )rxData.length() );
                        NotifyError( characteristic );
                        return;
                    }
                    uint8_t command = bytes[ 0 ];
                    uint32_t total_size = static_cast<uint32_t>( bytes[ 1 ] ) | ( static_cast<uint32_t>( bytes[ 2 ] ) << 8 ) |
                                          ( static_cast<uint32_t>( bytes[ 3 ] ) << 16 ) | ( static_cast<uint32_t>( bytes[ 4 ] ) << 24 );

                    {
                        std::scoped_lock lock( Mutex );
                        FirstPacketTimeMs = millis();
                    }

                    if( !BeginUpload( command, total_size ) )
                    {
                        NotifyError( characteristic );
                        return;
                    }
                    // Header consumed; ack and wait for the first data chunk.
                    characteristic->setValue( "continue" );
                    characteristic->notify();
                    return;
                }

                // Subsequent packets are image data.
                if( !WriteChunk( bytes, rxData.length() ) )
                {
                    NotifyError( characteristic );
                    return;
                }

                uint32_t bytes_received;
                {
                    std::scoped_lock lock( Mutex );
                    BytesReceived += rxData.length();
                    bytes_received = BytesReceived;
                }

                // End of stream: a short/zero final chunk, or the declared size reached.
                bool size_reached = ( DeclaredSize > 0 && bytes_received >= DeclaredSize );
                bool short_chunk = ( rxData.length() != FullOtaPacketSize );
                if( short_chunk || size_reached )
                {
                    LOG_TRACE( "EndOTA (%u bytes)", bytes_received );
                    if( FinishUpload() )
                    {
                        // The PC never gets this if we reboot synchronously, so we
                        // defer the reboot (below) until after this callback returns.
                        characteristic->setValue( "success" );
                        characteristic->notify();
                        LOG_TRACE( "notified success" );

                        {
                            std::scoped_lock lock( Mutex );
                            RebootMinTimeMs = millis() + 3000;
                            RebootRequired = true;
                        }
                        return;
                    }
                    LOG_ERROR( "Upload finalize error" );
                    NotifyError( characteristic );
                    return;
                }

                characteristic->setValue( "continue" );
                characteristic->notify();

                uint32_t packet_time = 0;
                if( LastPacketTimeMs > 0 )
                {
                    packet_time = millis() - LastPacketTimeMs;
                }
                LastPacketTimeMs = millis();

                auto cycle_time = millis() - start_time;
                auto total_time = millis() - FirstPacketTimeMs;
                PacketsReceived++;
                float ms_per_cycle = static_cast<float>( total_time ) / PacketsReceived;
                LOG_TRACE( "OTA %ims/%ims/%ims %1.1f %u B", cycle_time, packet_time, total_time, ms_per_cycle, bytes_received );
            }

          private:
            // Notify the host of an error and reset to a clean idle state so a
            // retry (without rebooting) starts fresh.
            void NotifyError( BLECharacteristic* characteristic )
            {
                AbortInFlight();
                ResetUploadState();
                characteristic->setValue( "error" );
                characteristic->notify();
                LOG_TRACE( "notified error" );
            }
        };
    }
    void Begin( BLEServer* server )
    {
        LOG_TRACE( "Starting OTA service" );
        PRINT_MEMORY_USAGE();
        // Note: the OTA target (app vs. SPIFFS) is now selected per-upload from the
        // first packet's header, so we no longer call esp_ota_begin() here.
        auto service = server->createService( FIRMWARE_UPDATE_SERVICE_UUID );
        auto write_characteristic = service->createCharacteristic(
            FIRMWARE_UPDATE_WRITE_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_WRITE_NR );
        write_characteristic->setCallbacks( new OtaWriteCallbacks() );
        auto version_characteristic =
            service->createCharacteristic( FIRMWARE_UPDATE_VERSION_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ );
        write_characteristic->addDescriptor( new BLE2902() );

        // Version string: "proto=<n>;app=<version>;fs=<spiffs-hash>". A reader that
        // sees no "proto" field is talking to old (protocol 1) firmware.
        uint32_t hash_ms = 0;
        std::string fs_hash = HashSpiffsPartition( &hash_ms );
        std::string version_value =
            "proto=" + std::to_string( ProtocolVersion ) + ";app=" + std::string( VERSION ) + ";fs=" + fs_hash;
        version_characteristic->setValue( version_value );
        service->start();
        LOG_TRACE( "Started OTA service. Version: %s (fs hash in %u ms)", version_value.c_str(), hash_ms );
        PRINT_MEMORY_USAGE();
    }

    bool IsInProgress( uint32_t* bytes_received )
    {
        std::scoped_lock lock( Mutex );
        if( bytes_received != nullptr )
        {
            *bytes_received = BytesReceived;
        }
        return BytesReceived > 0;
    }

    bool IsRebootRequired()
    {
        std::scoped_lock lock( Mutex );
        if( RebootRequired && millis() >= RebootMinTimeMs )
        {
            return true;
        }
        return false;
    }

    std::string HashSpiffsPartition( uint32_t* out_elapsed_ms )
    {
        auto start_time = millis();

        const esp_partition_t* part =
            esp_partition_find_first( ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, nullptr );
        if( part == nullptr )
        {
            LOG_ERROR( "HashSpiffsPartition: no SPIFFS partition found" );
            if( out_elapsed_ms )
            {
                *out_elapsed_ms = millis() - start_time;
            }
            return "";
        }

        // Read the raw partition in chunks and feed each into SHA-256. Hashing the
        // full partition size (not just used blocks) keeps the result equal to
        // sha256(spiffs.bin), which is also the full padded image.
        constexpr size_t ChunkSize = 4096;
        static uint8_t buffer[ ChunkSize ]; // static: avoid a 4 KB stack allocation.

        mbedtls_sha256_context ctx;
        mbedtls_sha256_init( &ctx );
        mbedtls_sha256_starts( &ctx, 0 ); // 0 = SHA-256 (not SHA-224)

        bool ok = true;
        for( size_t offset = 0; offset < part->size; offset += ChunkSize )
        {
            size_t to_read = part->size - offset;
            if( to_read > ChunkSize )
            {
                to_read = ChunkSize;
            }
            esp_err_t err = esp_partition_read( part, offset, buffer, to_read );
            if( err != ESP_OK )
            {
                LOG_ERROR( "HashSpiffsPartition: esp_partition_read failed at %u: %i", ( unsigned )offset, err );
                ok = false;
                break;
            }
            mbedtls_sha256_update( &ctx, buffer, to_read );
        }

        uint8_t digest[ 32 ];
        mbedtls_sha256_finish( &ctx, digest );
        mbedtls_sha256_free( &ctx );

        std::string hex;
        if( ok )
        {
            static const char* kHex = "0123456789abcdef";
            hex.reserve( 64 );
            for( int i = 0; i < 32; ++i )
            {
                hex.push_back( kHex[ digest[ i ] >> 4 ] );
                hex.push_back( kHex[ digest[ i ] & 0x0F ] );
            }
        }

        if( out_elapsed_ms )
        {
            *out_elapsed_ms = millis() - start_time;
        }
        return hex;
    }
}