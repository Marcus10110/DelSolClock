#include "debug_service.h"

#include "logger.h"
#include "utilities.h"
#include "display_config.h"
#include "gps_recorder.h"

#include "BLE2902.h"
#include <BLEServer.h>
#include "esp_core_dump.h"
#include "esp_partition.h"
#include "esp_err.h"

#include <atomic>
#include <vector>

#define DEBUG_SERVICE_UUID "2b0caa53-e543-4bb0-8f7f-50d1c64aa0dd"
#define DEBUG_STATUS_CHARACTERISTIC_UUID "32a18dc5-fdda-4601-b5b7-dc2920ac3f37"
#define DEBUG_DATA_CHARACTERISTIC_UUID "2969eccf-48f1-4069-a662-1ae77fe69118"
#define DEBUG_CONTROL_CHARACTERISTIC_UUID "65376e10-7797-435b-ac52-14ac0fab362c"
// Bezel offsets (read+write): 4 signed bytes [top, bottom, left, right].
#define DEBUG_BEZEL_CHARACTERISTIC_UUID "9a8b6f12-5d3e-4c7a-bf21-0e9d4c8a1b76"
// GPS recorder control (write): 1 byte command
//   0x01 = start, 0x02 = stop, 0x03 = arm download (snapshot + chunk the buffer).
#define DEBUG_GPSREC_CONTROL_CHARACTERISTIC_UUID "b4e2a1c0-9f3d-4a7e-8c52-1d6b0f93a27e"
// GPS recorder status (read): packed LE
//   u8 recording, u32 recordCount, u32 byteCount, u32 dropped, u32 chunkCount.
#define DEBUG_GPSREC_STATUS_CHARACTERISTIC_UUID "c5f3b2d1-a04e-4b8f-9d63-2e7c1a04b38f"
// GPS recorder data (read): sequential [u16 LE chunkIndex][<=510 bytes], the
// armed snapshot, auto-advancing pointer (same mechanism as the crash dump).
#define DEBUG_GPSREC_DATA_CHARACTERISTIC_UUID "d602c3e2-b15f-4c90-ae74-3f8d2b15c490"

namespace DebugService
{
    namespace
    {
        BLECharacteristic* DebugStatusCharacteristic = nullptr;
        BLECharacteristic* DebugDataCharacteristic = nullptr;
        BLECharacteristic* DebugControlCharacteristic = nullptr;
        BLECharacteristic* DebugBezelCharacteristic = nullptr;
        BLECharacteristic* GpsRecControlCharacteristic = nullptr;
        BLECharacteristic* GpsRecStatusCharacteristic = nullptr;
        BLECharacteristic* GpsRecDataCharacteristic = nullptr;

        // Pack the current bezel offsets into the characteristic value.
        void PublishBezel()
        {
            const auto& o = DisplayConfig::Get();
            uint8_t buf[ 4 ] = { ( uint8_t )o.top, ( uint8_t )o.bottom,
                                 ( uint8_t )o.left, ( uint8_t )o.right };
            DebugBezelCharacteristic->setValue( buf, sizeof( buf ) );
        }

        std::atomic<bool> ShouldAssert = false;

        bool CoreDumpIsAvailable( size_t* total_size_out = nullptr )
        {
            size_t addr = 0, size = 0;
            esp_err_t err = esp_core_dump_image_get( &addr, &size );
            if( err == ESP_OK && size > 0 )
            {
                if( total_size_out )
                    *total_size_out = size;
                return true;
            }
            return false;
        }

        constexpr size_t SliceSize = 510;
        uint8_t CoreDumpBuffer[ 512 ] = { 0 };
        const esp_partition_t* DumpPartition = nullptr;
        // this is the address, relative to the partition start, where the dump begins.
        size_t DumpStartPartitionRelative = 0;
        size_t DumpSize = 0;
        int CurrentSlice = 0;
        int SliceCount = 0;

        int GetSliceCount( size_t total_size )
        {
            return ( total_size + SliceSize - 1 ) / SliceSize;
        }

        void GetSliceAddress( size_t total_size, int slice, int* slice_start, int* slice_length )
        {
            // return the start index and slice length (max SliceSize) for the given slice index.
            if( slice_start != nullptr )
            {
                *slice_start = slice * SliceSize;
            }
            if( slice_length != nullptr )
            {
                *slice_length = std::min( SliceSize, total_size - *slice_start );
            }
        }

        void WriteSliceToCharacteristic( int slice )
        {
            int slice_start = 0;
            int slice_length = 0;
            GetSliceAddress( DumpSize, slice, &slice_start, &slice_length );
            if( slice_length > 0 )
            {
                // Read the slice from the core dump buffer and write it to the characteristic.

                esp_err_t er =
                    esp_partition_read( DumpPartition, DumpStartPartitionRelative + slice_start, &CoreDumpBuffer[ 2 ], slice_length );
                if( er != ESP_OK )
                {
                    LOG_ERROR( "Failed to read core dump slice (%i, %i), error: %i", slice_start, slice_length, er );
                    return;
                }

                // write the current slice index into the first 2 bytes of the array, little endian:
                CoreDumpBuffer[ 0 ] = CurrentSlice & 0xFF;
                CoreDumpBuffer[ 1 ] = ( CurrentSlice >> 8 ) & 0xFF;
                LOG_TRACE( "write dmp slice %i, length %i", CurrentSlice, slice_length );
                DebugDataCharacteristic->setValue( CoreDumpBuffer, slice_length + 2 );
            }
        }

        bool LoadPartitionData()
        {
            // load partition details.
            // Find the coredump partition; name is usually "coredump" but we match by subtype.
            const esp_partition_t* part = esp_partition_find_first( ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_COREDUMP, NULL );
            if( !part )
            {
                LOG_ERROR( "Failed to find coredump partition" );
                return false;
            }

            size_t addr = 0, size = 0;
            esp_err_t err = esp_core_dump_image_get( &addr, &size );

            if( addr < part->address )
            {
                LOG_ERROR( "Core dump image address is before partition start" );
                return false;
            }
            size_t part_offset = addr - part->address;
            if( part_offset + size > part->size )
            {
                LOG_ERROR( "Core dump image size is larger than partition size" );
                return false;
            }

            // only assign this if everything is valid
            DumpStartPartitionRelative = part_offset;
            DumpPartition = part;
            DumpSize = size;
            LOG_INFO( "Core dump image loaded. Address: 0x%08X, Size: %d relative address: %d", addr, size, DumpStartPartitionRelative );
            return true;
        }

        class DebugDataCallbacks : public BLECharacteristicCallbacks
        {
            void onRead( BLECharacteristic* pCharacteristic, esp_ble_gatts_cb_param_t* param ) override
            {
                if( CurrentSlice < SliceCount )
                {
                    WriteSliceToCharacteristic( CurrentSlice );
                    CurrentSlice++;
                    if( CurrentSlice >= SliceCount )
                    {
                        CurrentSlice = 0;
                    }
                }
            }
        };

        DebugDataCallbacks DebugDataCB;

        class DebugControlCallbacks : public BLECharacteristicCallbacks
        {
            void onWrite( BLECharacteristic* pCharacteristic, esp_ble_gatts_cb_param_t* param ) override
            {
                std::string rxValue = pCharacteristic->getValue();
                LOG_TRACE( "Debug control characteristic received value: %s", rxValue.c_str() );

                if( rxValue == "REBOOT" )
                {
                    LOG_INFO( "Rebooting device..." );
                    esp_restart();
                    return;
                }
                else if( rxValue == "ASSERT" )
                {
                    LOG_INFO( "Asserting device now..." );
                    // crash such that we generate a nice clean stack trace to right here.
                    abort();
                }
                else if( rxValue == "ASSERT_LATER" )
                {
                    LOG_INFO( "setting flag to assert later..." );
                    // assert from the main thread instead.
                    ShouldAssert = true;
                }
                else if( rxValue == "CLEAR" )
                {
                    LOG_INFO( "Clearing crash data..." );
                    esp_core_dump_image_erase();
                    DebugStatusCharacteristic->setValue( "NOCRASH" );
                    LOG_INFO( "crash data cleared." );
                }
                else if( rxValue == "PRINT" )
                {
                    LOG_INFO( "Log INFO" );
                }
                else
                {
                    LOG_WARN( "Unknown command received: %s", rxValue.c_str() );
                }
            }
        };

        DebugControlCallbacks DebugControlCB;

        class DebugBezelCallbacks : public BLECharacteristicCallbacks
        {
            void onWrite( BLECharacteristic* pCharacteristic, esp_ble_gatts_cb_param_t* param ) override
            {
                std::string v = pCharacteristic->getValue();
                if( v.size() < 4 )
                {
                    LOG_WARN( "bezel write too short (%d bytes)", ( int )v.size() );
                    return;
                }
                DisplayConfig::BezelOffsets o;
                o.top = static_cast<int8_t>( v[ 0 ] );
                o.bottom = static_cast<int8_t>( v[ 1 ] );
                o.left = static_cast<int8_t>( v[ 2 ] );
                o.right = static_cast<int8_t>( v[ 3 ] );
                DisplayConfig::Set( o );  // applies live + persists to NVS
                PublishBezel();           // reflect the clamped values back
            }
        };

        DebugBezelCallbacks DebugBezelCB;

        // --- GPS recorder download ---------------------------------------
        // The armed snapshot of the recording, sliced for sequential reads
        // exactly like the crash dump. Armed by control command 0x03.
        std::vector<uint8_t> GpsRecSnapshot;
        uint8_t GpsRecSlice[ 512 ] = { 0 };
        int GpsRecCurrentSlice = 0;
        int GpsRecSliceCount = 0;

        // Pack the current recorder status into the status characteristic.
        // Layout (LE): u8 recording, u32 recordCount, u32 byteCount,
        //              u32 dropped, u32 chunkCount.
        void PublishGpsRecStatus()
        {
            uint8_t buf[ 17 ];
            buf[ 0 ] = GpsRecorder::IsRecording() ? 1 : 0;
            auto put32 = []( uint8_t* p, uint32_t v ) {
                p[ 0 ] = v & 0xFF;
                p[ 1 ] = ( v >> 8 ) & 0xFF;
                p[ 2 ] = ( v >> 16 ) & 0xFF;
                p[ 3 ] = ( v >> 24 ) & 0xFF;
            };
            put32( &buf[ 1 ], ( uint32_t )GpsRecorder::RecordCount() );
            put32( &buf[ 5 ], ( uint32_t )GpsRecorder::ByteCount() );
            put32( &buf[ 9 ], ( uint32_t )GpsRecorder::DroppedRecords() );
            put32( &buf[ 13 ], ( uint32_t )GpsRecSliceCount );
            GpsRecStatusCharacteristic->setValue( buf, sizeof( buf ) );
        }

        void WriteGpsRecSliceToCharacteristic( int slice )
        {
            int start = slice * ( int )SliceSize;
            int len = ( int )std::min( SliceSize, GpsRecSnapshot.size() - ( size_t )start );
            if( len <= 0 ) return;
            GpsRecSlice[ 0 ] = slice & 0xFF;
            GpsRecSlice[ 1 ] = ( slice >> 8 ) & 0xFF;
            memcpy( &GpsRecSlice[ 2 ], &GpsRecSnapshot[ start ], len );
            GpsRecDataCharacteristic->setValue( GpsRecSlice, len + 2 );
        }

        // Snapshot the buffered recording (recording is stopped first so the
        // buffer is stable) and compute the chunk count.
        void ArmGpsRecDownload()
        {
            GpsRecorder::Stop();  // freeze the buffer for a consistent download
            GpsRecorder::SnapshotForDownload( GpsRecSnapshot );
            GpsRecCurrentSlice = 0;
            GpsRecSliceCount = ( int )GetSliceCount( GpsRecSnapshot.size() );
            LOG_INFO( "GpsRecorder: armed download, %u bytes, %d slices",
                      ( unsigned )GpsRecSnapshot.size(), GpsRecSliceCount );
            // Seed the first slice so an immediate read returns slice 0.
            if( GpsRecSliceCount > 0 )
            {
                WriteGpsRecSliceToCharacteristic( GpsRecCurrentSlice );
            }
        }

        class GpsRecControlCallbacks : public BLECharacteristicCallbacks
        {
            void onWrite( BLECharacteristic* pCharacteristic, esp_ble_gatts_cb_param_t* param ) override
            {
                std::string v = pCharacteristic->getValue();
                if( v.empty() ) return;
                switch( ( uint8_t )v[ 0 ] )
                {
                case 0x01:
                    LOG_INFO( "GpsRecorder: start command" );
                    GpsRecorder::Start();
                    break;
                case 0x02:
                    LOG_INFO( "GpsRecorder: stop command" );
                    GpsRecorder::Stop();
                    break;
                case 0x03:
                    LOG_INFO( "GpsRecorder: arm-download command" );
                    ArmGpsRecDownload();
                    break;
                default:
                    LOG_WARN( "GpsRecorder: unknown control byte 0x%02X", ( uint8_t )v[ 0 ] );
                    break;
                }
                PublishGpsRecStatus();
            }
        };

        GpsRecControlCallbacks GpsRecControlCB;

        class GpsRecDataCallbacks : public BLECharacteristicCallbacks
        {
            void onRead( BLECharacteristic* pCharacteristic, esp_ble_gatts_cb_param_t* param ) override
            {
                if( GpsRecCurrentSlice < GpsRecSliceCount )
                {
                    WriteGpsRecSliceToCharacteristic( GpsRecCurrentSlice );
                    GpsRecCurrentSlice++;
                    if( GpsRecCurrentSlice >= GpsRecSliceCount )
                    {
                        GpsRecCurrentSlice = 0;  // wrap so a re-download can restart
                    }
                }
            }
        };

        GpsRecDataCallbacks GpsRecDataCB;

    }


    bool StartDebugService( BLEServer* server )
    {
        LOG_TRACE( "creating debug service" );
        PRINT_MEMORY_USAGE();
        auto debug_service = server->createService( BLEUUID( DEBUG_SERVICE_UUID ) );
        LOG_TRACE( "creating debug status characteristic" );
        DebugStatusCharacteristic =
            debug_service->createCharacteristic( DEBUG_STATUS_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ );
        DebugStatusCharacteristic->addDescriptor( new BLE2902() );


        LOG_TRACE( "creating debug data characteristic" );
        DebugDataCharacteristic = debug_service->createCharacteristic( DEBUG_DATA_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ );
        DebugDataCharacteristic->addDescriptor( new BLE2902() );
        DebugDataCharacteristic->setCallbacks( &DebugDataCB );

        DebugControlCharacteristic =
            debug_service->createCharacteristic( DEBUG_CONTROL_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_WRITE );
        DebugControlCharacteristic->addDescriptor( new BLE2902() );
        DebugControlCharacteristic->setCallbacks( &DebugControlCB );

        LOG_TRACE( "creating debug bezel characteristic" );
        DebugBezelCharacteristic = debug_service->createCharacteristic(
            DEBUG_BEZEL_CHARACTERISTIC_UUID,
            BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE );
        DebugBezelCharacteristic->addDescriptor( new BLE2902() );
        DebugBezelCharacteristic->setCallbacks( &DebugBezelCB );
        PublishBezel();  // seed with the persisted values so a read returns them

        LOG_TRACE( "creating gps recorder characteristics" );
        GpsRecControlCharacteristic = debug_service->createCharacteristic(
            DEBUG_GPSREC_CONTROL_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_WRITE );
        GpsRecControlCharacteristic->addDescriptor( new BLE2902() );
        GpsRecControlCharacteristic->setCallbacks( &GpsRecControlCB );

        GpsRecStatusCharacteristic = debug_service->createCharacteristic(
            DEBUG_GPSREC_STATUS_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ );
        GpsRecStatusCharacteristic->addDescriptor( new BLE2902() );

        GpsRecDataCharacteristic = debug_service->createCharacteristic(
            DEBUG_GPSREC_DATA_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ );
        GpsRecDataCharacteristic->addDescriptor( new BLE2902() );
        GpsRecDataCharacteristic->setCallbacks( &GpsRecDataCB );
        PublishGpsRecStatus();  // seed an idle status

        size_t core_size = 0;
        bool has_core = CoreDumpIsAvailable( &core_size );
        if( has_core )
        {
            SliceCount = GetSliceCount( core_size );

            LOG_INFO( "Core Dump available, advertising size: %d", core_size );
            std::string status_str = "CRASH:" + std::to_string( core_size ) + ":" + std::to_string( SliceCount );
            DebugStatusCharacteristic->setValue( status_str.c_str() );

            if( !LoadPartitionData() )
            {
                LOG_ERROR( "Failed to load partition data" );
                return false;
            }

            WriteSliceToCharacteristic( CurrentSlice );
        }
        else
        {
            LOG_TRACE( "no core dump available" );
            DebugStatusCharacteristic->setValue( "NOCRASH" );
        }
        LOG_TRACE( "starting debug service" );
        debug_service->start();
        LOG_TRACE( "Debug service started" );
        PRINT_MEMORY_USAGE();
        return true;
    }

    void HandleAssertCase()
    {
        if( ShouldAssert.load() )
        {
            LOG_INFO( "Asserting device now..." );
            ShouldAssert = false;
            // crash such that we generate a nice clean stack trace to right here.
            abort();
        }
    }
}