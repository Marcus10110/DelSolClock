#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLESecurity.h>
#include <BLE2902.h>
#include <BLE2904.h>

#include "AppleMediaService.h"
#include "CurrentTimeService.h"
#include "Display.h"

#include <time.h>
#include <sys/time.h>


#define APPLE_SERVICE_UUID "89D3502B-0F36-433A-8EF4-C502AD55F8DC"

#define DELSOL_VEHICLE_SERVICE_UUID "8fb88487-73cf-4cce-b495-505a4b54b802"
#define DELSOL_STATUS_CHARACTERISTIC_UUID "40d527f5-3204-44a2-a4ee-d8d3c16f970e"
#define DELSOL_BETTERY_CHARACTERISTIC_UUID "40d527f5-3204-44a2-a4ee-d8d3c16f970e"
#define DELSOL_LOCATION_SERVICE_UUID "61d33c70-e3cd-4b31-90d8-a6e14162fffd"
#define DELSOL_NAVIGATION_SERVICE_UUID "77f5d2b5-efa1-4d55-b14a-cc92b72708a0"


void PrintBytes( const std::string& byte_str )
{
    Serial.printf( "[%i]: ", byte_str.size() );
    for( uint8_t c : byte_str )
    {
        if( c <= 0x0f )
        {
            Serial.print( "0" );
        }
        Serial.print( c, 16 );
    }
    Serial.println( "" );
}


bool deviceConnected = false;
bool oldDeviceConnected = false;
bool AuthenticationComplete = false;
bool TrackNameDirty = false;
BLEServer* pServer = nullptr;
BLEAddress* iphone_address = nullptr;
BLEClient* client = nullptr;
BLESecurity Security{};

class MyServerCallbacks : public BLEServerCallbacks
{
  public:
    void onConnect( BLEServer* pServer, esp_ble_gatts_cb_param_t* param ) override
    {
        Serial.println( "onConnect CB" );
        if( iphone_address )
        {
            delete iphone_address;
            iphone_address = nullptr;
        }
        iphone_address = new BLEAddress( param->connect.remote_bda );

        deviceConnected = true;
    };

    void onDisconnect( BLEServer* pServer ) override
    {
        Serial.println( "onDisconnect CB" );
        deviceConnected = false;
    }
};

class NotificationSecurityCallbacks : public BLESecurityCallbacks
{
    uint32_t onPassKeyRequest() override
    {
        Serial.println( "PassKeyRequest" );
        return 123456;
    }
    void onPassKeyNotify( uint32_t pass_key ) override
    {
        Serial.printf( "On passkey Notify number:%d\n", pass_key );
    }

    bool onSecurityRequest() override
    {
        Serial.println( "On Security Request" );
        return true;
    }

    bool onConfirmPIN( unsigned int ) override
    {
        Serial.println( "On Confirmed Pin Request" );
        return true;
    }

    void onAuthenticationComplete( esp_ble_auth_cmpl_t cmpl ) override
    {
        Serial.println( "Authentication Complete!" );
        if( cmpl.success )
        {
            uint16_t length;
            esp_ble_gap_get_whitelist_size( &length );
            Serial.printf( "size: %d\n", length );
        }
        AuthenticationComplete = true;
    }
};


void setup()
{
    Serial.begin( 115200 );
    Serial.println( "Del Sol Clock Booting" );

    Display::Begin();

    BLEDevice::init( "Del Sol" );
    pServer = BLEDevice::createServer();
    pServer->setCallbacks( new MyServerCallbacks() );

    auto vechicle_service = pServer->createService( DELSOL_VEHICLE_SERVICE_UUID );
    auto vehicle_status_characteristic = vechicle_service->createCharacteristic(
        DELSOL_STATUS_CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY );

    int vehicle_status = 0xAA;
    vehicle_status_characteristic->setValue( vehicle_status );
    vechicle_service->start();


    BLEDevice::setEncryptionLevel( ESP_BLE_SEC_ENCRYPT );
    BLEDevice::setSecurityCallbacks( new NotificationSecurityCallbacks() );


    BLEAdvertising* pAdvertising = pServer->getAdvertising();
    pAdvertising->setAppearance( 0x03C1 );
    pAdvertising->setScanResponse( true );
    BLEAdvertisementData oAdvertisementData = BLEAdvertisementData();
    oAdvertisementData.setFlags( 0x01 );
    setServiceSolicitation( oAdvertisementData, BLEUUID( APPLE_SERVICE_UUID ) );
    pAdvertising->setAdvertisementData( oAdvertisementData );


    Security.setAuthenticationMode( ESP_LE_AUTH_REQ_SC_BOND );
    Security.setCapability( ESP_IO_CAP_OUT ); // This value changes between server and client. Is it needed?
    Security.setInitEncryptionKey( ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK ); // not sure how this is used either.
    pAdvertising->start();
}

BLERemoteCharacteristic* entity_update = nullptr;

void loop()
{
    // put your main code here, to run repeatedly:
    // disconnecting
    if( !deviceConnected && oldDeviceConnected )
    {
        delay( 500 ); // give the bluetooth stack the chance to get things ready
        if( client )
        {
            client->disconnect();
            delete client;
            client = nullptr;
        }
        AuthenticationComplete = false;
        pServer->startAdvertising(); // restart advertising
        Serial.println( "disconnected, restart advertising" );
        oldDeviceConnected = deviceConnected;
    }
    // connecting
    else if( deviceConnected && !oldDeviceConnected )
    {
        // do stuff here on connecting
        oldDeviceConnected = deviceConnected;
        Serial.println( "connected!" );
        HandleConnection();
        Serial.println( "HandleConnection returned" );
    }
    else if( deviceConnected )
    {
        delay( 2000 );
        Serial.print( "." );
        if( entity_update )
        {
            // auto val = entity_update->readValue();
            // Serial.println( "reading value..." );
            // Serial.println( val.c_str() );
        }
        if( TrackNameDirty )
        {
            TrackNameDirty = false;
            const auto& media_info = AppleMediaService::GetMediaInformation();
            if( !media_info.mTitle.empty() )
            {
                media_info.dump();
            }
        }
        UpdateDisplay();
    }
    else
    {
        delay( 100 );
    }
}

void UpdateDisplay()
{
    Display::Clear();
    tm time;
    if( getLocalTime( &time, 100 ) )
    {
        Display::DrawTime( time.tm_hour, time.tm_min );
    }
    const auto& media_info = AppleMediaService::GetMediaInformation();
    if( !media_info.mTitle.empty() && media_info.mPlaybackState == AppleMediaService::MediaInformation::PlaybackState::Playing )
    {
        Display::DrawMediaInfo( media_info );
    }
    else
    {
        Serial.println( "failed to get time" );
    }
    Display::DrawSpeed( 42.3 );
}

void setServiceSolicitation( BLEAdvertisementData& advertisementData, BLEUUID uuid )
{
    char cdata[ 2 ];
    switch( uuid.bitSize() )
    {
    case 16:
    {
        // [Len] [0x14] [UUID16] data
        cdata[ 0 ] = 3;
        cdata[ 1 ] = ESP_BLE_AD_TYPE_SOL_SRV_UUID; // 0x14
        advertisementData.addData( std::string( cdata, 2 ) + std::string( ( char* )&uuid.getNative()->uuid.uuid16, 2 ) );
        break;
    }

    case 128:
    {
        // [Len] [0x15] [UUID128] data
        cdata[ 0 ] = 17;
        cdata[ 1 ] = ESP_BLE_AD_TYPE_128SOL_SRV_UUID; // 0x15
        advertisementData.addData( std::string( cdata, 2 ) + std::string( ( char* )uuid.getNative()->uuid.uuid128, 16 ) );
        break;
    }

    default:
        return;
    }
}


void HandleConnection()
{
    if( !iphone_address )
        return;

    client = BLEDevice::createClient();

    Security.setAuthenticationMode( ESP_LE_AUTH_REQ_SC_BOND );
    Security.setCapability( ESP_IO_CAP_IO );
    Security.setRespEncryptionKey( ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK );
    Serial.println( "set security details" );

    if( !client->connect( *iphone_address ) )
    {
        Serial.println( "failed to connect" );
        delete client;
        client = nullptr;
        return;
    }
    Serial.println( "connected using client!" );

    Serial.print( "Waiting for authentication" );
    while( !AuthenticationComplete )
    {
        Serial.print( "." );
        delay( 100 );
    }
    Serial.println( "Authentication complete" );
    delay( 100 );

    AppleMediaService::RegisterForNotifications( []() { TrackNameDirty = true; }, AppleMediaService::NotificationLevel::TrackTitleOnly );

    if( !AppleMediaService::StartMediaService( client ) )
    {
        client->disconnect();
        delete client;
        client = nullptr;
        Serial.println( "StartMediaService failed" );
        return;
    }

    CurrentTimeService::CurrentTime time;
    if( !CurrentTimeService::StartTimeService( client, &time ) )
    {
        Serial.println( "StartTimeService failed" );
        return;
    }
    timeval new_time;
    new_time.tv_sec = time.ToTimeT();
    new_time.tv_usec = static_cast<long>( time.mSecondsFraction * 1000000 );
    Serial.print( "Unix time stamp: " );
    Serial.print( new_time.tv_sec );
    Serial.print( ", usec: " );
    Serial.println( new_time.tv_usec );
    if( settimeofday( &new_time, nullptr ) != 0 )
    {
        Serial.println( "Error setting time of day" );
    }

    time.Dump();
}

/*
Application Notes:

Let's display the connection status on screen. (connected / not connected)
Let's display the current song information if available. (playing)
Let's display the current time, and update the current time at the beginning of every connection.

Let's see if notifications include driving directions

Let's make available a few flags - window, roof and trunk, interior lights voltage?, and ignition. battery voltage too.
Let's expose the current GPS location

Let's only attempt to connect when the ignition is on. let's disconnect when it's off, and go into sleep mode (turn off screen too)

State machine:

Is ignition on?
- start or continue BT operation

Is ignition off?
- clean up or stop advertising BT.
*/