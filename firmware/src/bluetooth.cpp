#include "bluetooth.h"

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLESecurity.h>
#include <BLE2902.h>
#include <BLE2904.h>

#include <time.h>
#include <sys/time.h>
#include <esp_bt_device.h>
#include <esp_gap_ble_api.h>

#include "apple_media_service.h"
#include "current_time_service.h"
#include "apple_notification_center_service.h"
#include "ble_ota.h"
#include "logger.h"

#define APPLE_SERVICE_UUID "89D3502B-0F36-433A-8EF4-C502AD55F8DC"

#define DELSOL_VEHICLE_SERVICE_UUID "8fb88487-73cf-4cce-b495-505a4b54b802"
#define DELSOL_STATUS_CHARACTERISTIC_UUID "40d527f5-3204-44a2-a4ee-d8d3c16f970e"
#define DELSOL_BATTERY_CHARACTERISTIC_UUID "5c258bb8-91fc-43bb-8944-b83d0edc9b43"


#define DELSOL_LOCATION_SERVICE_UUID "61d33c70-e3cd-4b31-90d8-a6e14162fffd"
#define DELSOL_NAVIGATION_SERVICE_UUID "77f5d2b5-efa1-4d55-b14a-cc92b72708a0"


namespace Bluetooth
{
    namespace
    {
        bool DeviceConnected = false;
        bool OldDeviceConnected = false;
        bool AuthenticationComplete = false;
        bool Ended = true;
        BLEServer* Server = nullptr;
        BLEAddress* IPhoneAddress = nullptr;
        BLEClient* Client = nullptr;
        BLESecurity Security{};
        BLECharacteristic* VehicleStatusCharacteristic = nullptr;

        RTC_DATA_ATTR bool TimeSet = false;

        class GetAddressServerCallbacks : public BLEServerCallbacks
        {
          public:
            void onConnect( BLEServer* server, esp_ble_gatts_cb_param_t* param ) override
            {
                LOG_TRACE( "onConnect CB" );
                if( IPhoneAddress )
                {
                    delete IPhoneAddress;
                    IPhoneAddress = nullptr;
                }
                IPhoneAddress = new BLEAddress( param->connect.remote_bda );

                DeviceConnected = true;
            };

            void onDisconnect( BLEServer* server ) override
            {
                LOG_TRACE( "onDisconnect CB" );
                DeviceConnected = false;
            }
        };

        class NotificationSecurityCallbacks : public BLESecurityCallbacks
        {
            uint32_t onPassKeyRequest() override
            {
                LOG_TRACE( "PassKeyRequest" );
                return 123456;
            }
            void onPassKeyNotify( uint32_t pass_key ) override
            {
                LOG_TRACE( "On passkey Notify number: %d", pass_key );
            }

            bool onSecurityRequest() override
            {
                LOG_TRACE( "On Security Request" );
                return true;
            }

            bool onConfirmPIN( unsigned int ) override
            {
                LOG_TRACE( "On Confirmed Pin Request" );
                return true;
            }

            void onAuthenticationComplete( esp_ble_auth_cmpl_t cmpl ) override
            {
                if( cmpl.success )
                {
                    LOG_DEBUG( "Authentication Successful!" );
                    uint16_t length;
                    esp_ble_gap_get_whitelist_size( &length );
                    LOG_TRACE( "size: %d", length );
                    AuthenticationComplete = true;
                }
                else
                {
                    LOG_ERROR( "Authentication failed. reason: %u, auth_mode: %u, key_present: %u, key_type: %u", cmpl.fail_reason,
                               cmpl.auth_mode, cmpl.key_present, cmpl.key_type );
                }
            }
        };


        void setServiceSolicitation( BLEAdvertisementData& advertisement_data, BLEUUID uuid )
        {
            char c_data[ 2 ];
            switch( uuid.bitSize() )
            {
            case 16:
            {
                // [Len] [0x14] [UUID16] data
                c_data[ 0 ] = 3;
                c_data[ 1 ] = ESP_BLE_AD_TYPE_SOL_SRV_UUID; // 0x14
                advertisement_data.addData( std::string( c_data, 2 ) + std::string( ( char* )&uuid.getNative()->uuid.uuid16, 2 ) );
                break;
            }

            case 128:
            {
                // [Len] [0x15] [UUID128] data
                c_data[ 0 ] = 17;
                c_data[ 1 ] = ESP_BLE_AD_TYPE_128SOL_SRV_UUID; // 0x15
                advertisement_data.addData( std::string( c_data, 2 ) + std::string( ( char* )uuid.getNative()->uuid.uuid128, 16 ) );
                break;
            }

            default:
                return;
            }
        }


        void HandleConnection()
        {
            if( !IPhoneAddress )
                return;

            if( Client )
            {
                LOG_WARN( "HandleConnection: Client already exists, deleteing." );
                delete Client;
                Client = nullptr;
            }

            Client = BLEDevice::createClient();

            Security.setAuthenticationMode( ESP_LE_AUTH_REQ_SC_BOND );
            Security.setCapability( ESP_IO_CAP_IO );
            Security.setRespEncryptionKey( ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK );
            LOG_TRACE( "set security details" );

            if( !Client->connect( *IPhoneAddress ) )
            {
                LOG_ERROR( "failed to connect" );
                delete Client;
                Client = nullptr;
                return;
            }
            LOG_TRACE( "connected using client!" );

            LOG_TRACE( "Waiting for authentication" );
            while( !AuthenticationComplete && Client->isConnected() )
            {
                LOG_TRACE( "." );
                delay( 100 );
            }
            LOG_TRACE( "Authentication finished" );
            delay( 100 );
            if( !Client->isConnected() )
            {
                LOG_WARN( "client disconnected during authentication." );
                delete Client;
                Client = nullptr;
                return;
            }

            if( !AppleMediaService::StartMediaService( Client ) )
            {
                Client->disconnect();
                delete Client;
                Client = nullptr;
                LOG_ERROR( "StartMediaService failed" );
                return;
            }

            CurrentTimeService::CurrentTime time;
            if( !CurrentTimeService::StartTimeService( Client, &time ) )
            {
                LOG_ERROR( "StartTimeService failed" );
                return;
            }
            timeval new_time;
            new_time.tv_sec = time.ToTimeT();
            new_time.tv_usec = static_cast<long>( time.mSecondsFraction * 1000000 );
            LOG_INFO( "Unix time stamp: %l, usec: %l", new_time.tv_sec, new_time.tv_usec );

            if( settimeofday( &new_time, nullptr ) != 0 )
            {
                LOG_ERROR( "Error setting time of day" );
            }
            else
            {
                TimeSet = true;
            }

            time.Dump();
// Apple Notification Service disabled, not used.
#if 1
            if( !AppleNotifications::StartNotificationService( Client ) )
            {
                LOG_ERROR( "failed to start the notification service" );
                return;
            }
            LOG_TRACE( "Notification service started" );
#endif
        }


    }


    void Begin( const std::string& device_name )
    {
        LOG_TRACE( "Bluetooth::Begin()" );
        Ended = false;
        BLEDevice::init( device_name );
        Server = BLEDevice::createServer();
        Server->setCallbacks( new GetAddressServerCallbacks() );

        const uint8_t* address = esp_bt_dev_get_address();
        if( address )
        {
            LOG_DEBUG( "public device address:  %02X %02X %02X %02X %02X %02X %02X %02X", address[ 0 ], address[ 1 ], address[ 2 ],
                       address[ 3 ], address[ 4 ], address[ 5 ] );
        }
        else
        {
            LOG_ERROR( "public device address null" );
        }

        esp_bd_addr_t local_address;
        uint8_t address_type;
        if( esp_ble_gap_get_local_used_addr( local_address, &address_type ) == ESP_OK )
        {
            LOG_DEBUG( "local use address:  %02X %02X %02X %02X %02X %02X %02X %02X. Type: %u", local_address[ 0 ], local_address[ 1 ],
                       local_address[ 2 ], local_address[ 3 ], local_address[ 4 ], local_address[ 5 ], address_type );
        }
        else
        {
            LOG_ERROR( "failed to get local address" );
        }

        // Add the OTA service.
        BleOta::Begin( Server );

        auto vechicle_service = Server->createService( DELSOL_VEHICLE_SERVICE_UUID );
        VehicleStatusCharacteristic = vechicle_service->createCharacteristic(
            DELSOL_STATUS_CHARACTERISTIC_UUID,
            BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY );
        VehicleStatusCharacteristic->addDescriptor( new BLE2902() );
        VehicleStatusCharacteristic->setValue( "" );
        vechicle_service->start();


        BLEDevice::setEncryptionLevel( ESP_BLE_SEC_ENCRYPT );
        BLEDevice::setSecurityCallbacks( new NotificationSecurityCallbacks() );


        BLEAdvertising* advertising = Server->getAdvertising();
        advertising->setAppearance( 0x03C1 );
        advertising->setScanResponse( true );

        BLEAdvertisementData oAdvertisementData = BLEAdvertisementData();
        BLEAdvertisementData scan_response_data;
        scan_response_data.setCompleteServices( BLEUUID( DELSOL_VEHICLE_SERVICE_UUID ) );
        oAdvertisementData.setFlags( 0x01 );
        // advertise the vehicle service.
        // oAdvertisementData.setCompleteServices( BLEUUID( DELSOL_VEHICLE_SERVICE_UUID ) );
        // Apple Media Service
        setServiceSolicitation( oAdvertisementData, BLEUUID( APPLE_SERVICE_UUID ) );
        // Apple Notification Service ANCS
        // setServiceSolicitation( oAdvertisementData, BLEUUID( "7905F431-B5CE-4E99-A40F-4B1E122D00D0" ) );
        auto before = oAdvertisementData.getPayload().size();
        oAdvertisementData.setCompleteServices( BLEUUID( DELSOL_VEHICLE_SERVICE_UUID ) );
        auto after = oAdvertisementData.getPayload().size();
        LOG_TRACE( "advertising size before %i, after %i", before, after );
        advertising->setAdvertisementData( oAdvertisementData );
        advertising->setScanResponseData( scan_response_data );

        // TODO: Figure out what this does, and why we set it twice.
        Security.setAuthenticationMode( ESP_LE_AUTH_REQ_SC_BOND );
        Security.setCapability( ESP_IO_CAP_OUT ); // This value changes between server and client. Is it needed?
        Security.setInitEncryptionKey( ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK ); // not sure how this is used either.
        advertising->start();
    }
    void End()
    {
        Ended = true;
        if( Client )
        {
            Client->disconnect();
            delete Client;
            Client = nullptr;
        }
        DeviceConnected = false;
        OldDeviceConnected = false;
        VehicleStatusCharacteristic = nullptr;
        if( Server )
        {
            auto advertising = Server->getAdvertising();
            if( advertising )
            {
                advertising->stop();
            }
            delete Server;
            Server = nullptr;
        }

        if( IPhoneAddress )
        {
            delete IPhoneAddress;
            IPhoneAddress = nullptr;
        }

        BLEDevice::deinit( true );
    }

    void Service()
    {
        if( Ended )
        {
            return;
        }
        if( !DeviceConnected && OldDeviceConnected )
        {
            // device disconnected.
            delay( 500 ); // give the bluetooth stack the chance to get things ready
            if( Client )
            {
                Client->disconnect();
                delete Client;
                Client = nullptr;
            }
            AuthenticationComplete = false;
            Server->startAdvertising(); // restart advertising
            LOG_DEBUG( "disconnected, restart advertising" );
            OldDeviceConnected = DeviceConnected;
        }
        // connecting
        else if( DeviceConnected && !OldDeviceConnected )
        {
            // do stuff here on connecting
            OldDeviceConnected = DeviceConnected;
            LOG_DEBUG( "connected!" );
            HandleConnection();
            LOG_TRACE( "HandleConnection returned" );
        }
    }

    bool IsConnected()
    {
        return DeviceConnected && OldDeviceConnected;
    }

    bool IsTimeSet()
    {
        return TimeSet;
    }

    void SetVehicleStatus( const std::string& status )
    {
        if( VehicleStatusCharacteristic )
        {
            VehicleStatusCharacteristic->setValue( status );
            VehicleStatusCharacteristic->notify();
            LOG_TRACE( "set characteristic notify." );
        }
    }
}