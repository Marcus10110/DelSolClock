#include "ble_ota.h"
#include "version.h"

#include <Arduino.h>
#include <BLEServer.h>
#include <BLE2902.h>

#include "esp_ota_ops.h"

#define FIRMWARE_UPDATE_SERVICE_UUID "69da0f2b-43a4-4c2a-b01d-0f11564c732b"
#define FIRMWARE_UPDATE_WRITE_CHARACTERISTIC_UUID "7efc013a-37b7-44da-8e1c-06e28256d83b"
#define FIRMWARE_UPDATE_VERSION_CHARACTERISTIC_UUID "a5c0d67a-9576-47ea-85c6-0347f8473cf3"


namespace BleOta
{
    namespace
    {
        esp_ota_handle_t UpdateHandle = 0;
        uint32_t BytesReceived{ 0 };
        constexpr uint32_t FullOtaPacketSize = 512;
        class OtaWriteCallbacks : public BLECharacteristicCallbacks
        {
          public:
            void onWrite( BLECharacteristic* characteristic ) override
            {
                auto start_time = millis();
                esp_err_t status;
                std::string rxData = characteristic->getValue();
                if( BytesReceived == 0 )
                {
                    Serial.println( "BeginOTA" );
                }

                if( rxData.length() > 0 )
                {
                    status = esp_ota_write( UpdateHandle, rxData.c_str(), rxData.length() );
                    Serial.printf( "esp_ota_write: %i\n", status );
                    BytesReceived += rxData.length();
                    Serial.printf( "OTA received %u bytes\n", BytesReceived );
                }
                auto write_time = millis() - start_time;
                if( rxData.length() != FullOtaPacketSize && BytesReceived > 0 )
                {
                    Serial.println( "EndOTA" );
                    status = esp_ota_end( UpdateHandle );
                    Serial.printf( "esp_ota_end: %i\n", status );
                    status = esp_ota_set_boot_partition( esp_ota_get_next_update_partition( NULL ) );
                    Serial.printf( "esp_ota_set_boot_partition: %i\n", status );
                    if( status == ESP_OK )
                    {
                        characteristic->setValue( "success" );
                        characteristic->notify();
                        Serial.println( "notified success" );
                        delay( 2000 );
                        esp_restart();
                        return;
                    }
                    else
                    {
                        BytesReceived = 0;
                        Serial.println( "Upload Error" );
                        characteristic->setValue( "error" );
                        Serial.println( "notified error" );
                        characteristic->notify();
                        return;
                    }
                }


                characteristic->setValue( "continue" );
                Serial.println( "notified continue" );
                characteristic->notify();
                auto total_time = millis() - start_time;
                Serial.printf( "OTA fw write %i, total %i\n", write_time, total_time );
            }
        };
    }
    void Begin( BLEServer* server )
    {
        auto status = esp_ota_begin( esp_ota_get_next_update_partition( NULL ), OTA_SIZE_UNKNOWN, &UpdateHandle );
        Serial.printf( "esp_ota_begin: %i\n", status );
        if( status != ESP_OK )
        {
            return;
        }
        auto service = server->createService( FIRMWARE_UPDATE_SERVICE_UUID );
        auto write_characteristic = service->createCharacteristic(
            FIRMWARE_UPDATE_WRITE_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_WRITE_NR );
        write_characteristic->setCallbacks( new OtaWriteCallbacks() );
        auto version_characteristic =
            service->createCharacteristic( FIRMWARE_UPDATE_VERSION_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ );
        write_characteristic->addDescriptor( new BLE2902() );

        version_characteristic->setValue( VERSION );
        service->start();
        Serial.printf( "Started OTA service. Current version: %s\n", VERSION );
    }

    bool IsInProgress( uint32_t* bytes_received )
    {
        if( bytes_received != nullptr )
        {
            *bytes_received = BytesReceived;
        }
        return BytesReceived > 0;
    }
}