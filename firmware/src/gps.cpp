#include "gps.h"
#include "pins.h"
#include <TinyGPSPlus.h>
#include <cstdlib>
#include "driver/uart.h"
#include "logger.h"

namespace Gps
{
    namespace
    {
        constexpr uart_port_t GpsUartPort = UART_NUM_2;
        QueueHandle_t GpsUartQueue;
        TinyGPSPlus Gps;
        // Diagnostic NMEA field extractors:
        //  GGA term 6 = fix quality (0 = no fix, 1 = GPS, 2 = DGPS)
        //  GGA term 7 = satellites USED in the fix
        //  GSV term 3 = satellites IN VIEW (does the antenna see anything at all?)
        TinyGPSCustom GgaFixQuality( Gps, "GPGGA", 6 );
        TinyGPSCustom GgaSatsUsed( Gps, "GPGGA", 7 );
        TinyGPSCustom GsvSatsInView( Gps, "GPGSV", 3 );
        // u-blox (SAM-M8Q) often uses the multi-GNSS "GN" talker ID instead of
        // "GP". Watch both so we don't miss the fields.
        TinyGPSCustom GnGgaFixQuality( Gps, "GNGGA", 6 );
        TinyGPSCustom GnGgaSatsUsed( Gps, "GNGGA", 7 );
        TinyGPSCustom GnGsvSatsInView( Gps, "GNGSV", 3 );
        // constexpr char StandbyCommand[] = "$PMTK161,0*28\r\n";
        // constexpr char WakeCommand[] = "$PMTK010,002*2D\r\n";
    }
    TinyGPSPlus* GetGps()
    {
        return &Gps;
    }
    Debug GetDebug()
    {
        // Prefer the GN talker fields (u-blox multi-GNSS), fall back to GP.
        auto pick = []( TinyGPSCustom& gn, TinyGPSCustom& gp ) -> int {
            if( gn.isValid() ) return atoi( gn.value() );
            if( gp.isValid() ) return atoi( gp.value() );
            return 0;
        };
        Debug d;
        d.satsInView = pick( GnGsvSatsInView, GsvSatsInView );
        d.satsUsed = pick( GnGgaSatsUsed, GgaSatsUsed );
        d.fixQuality = pick( GnGgaFixQuality, GgaFixQuality );
        d.charsProcessed = Gps.charsProcessed();
        return d;
    }
    void Begin()
    {
        LOG_TRACE( "Gps::Begin()" );
        // Zero-initialize: uart_config_t has fields beyond the ones we set
        // (notably source_clk, and flags on newer ESP-IDF). Leaving them as
        // uninitialized stack garbage can misconfigure the UART clock so RX
        // produces no/garbled data — a latent bug that surfaced after the
        // PlatformIO/IDF toolchain change. Set source_clk explicitly.
        uart_config_t uart_config = {};
        uart_config.baud_rate = 9600;
        uart_config.data_bits = UART_DATA_8_BITS;
        uart_config.parity = UART_PARITY_DISABLE;
        uart_config.stop_bits = UART_STOP_BITS_1;
        uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
        uart_config.rx_flow_ctrl_thresh = 120;
        uart_config.source_clk = UART_SCLK_APB;

        uart_param_config( GpsUartPort, &uart_config );

        uart_set_pin( GpsUartPort,
                      Pin::GpsRx,         // TX
                      Pin::GpsTx,         // RX
                      UART_PIN_NO_CHANGE, // RTS
                      UART_PIN_NO_CHANGE  // CTS
        );

        uart_driver_install( GpsUartPort, 2048, 2048, 10, &GpsUartQueue, 0 );
    }
    void Service()
    {
        size_t count = 0;
        uint8_t data[ 128 ];
        uart_get_buffered_data_len( GpsUartPort, &count );
        if( count > 0 )
        {
            while( count > 0 )
            {
                auto read = uart_read_bytes( GpsUartPort, data, min( count, sizeof( data ) ), 100 );
                for( int i = 0; i < read; ++i )
                {
                    Gps.encode( static_cast<char>( data[ i ] ) );
                }
                count -= read;
            }
        }

        if( Gps.location.isUpdated() )
        {
            float lat = Gps.location.lat();
            float lng = Gps.location.lng();
            // LOG_TRACE( "GPS: %f, %f", lat, lng );
        }

        // Health diagnostic (~every 5s), but ONLY while there's no fix — once
        // locked it goes quiet (the on-screen Status acquisition view covers
        // field debugging). Distinguishes the failure modes:
        //  - chars not increasing      => no bytes arriving (wiring/UART/power)
        //  - chars up but checksum-fail climbing / withFix=0 => garbled (baud/overflow)
        //  - chars + sats climbing     => acquiring; just needs time / open sky
        if( !Gps.location.isValid() )
        {
            static uint32_t last_diag_ms = 0;
            static uint32_t last_chars = 0;
            uint32_t now = millis();
            if( now - last_diag_ms >= 5000 )
            {
                last_diag_ms = now;
                uint32_t chars = Gps.charsProcessed();
                // Prefer the GN talker fields, fall back to GP.
                const char* fixQ = GnGgaFixQuality.isValid() ? GnGgaFixQuality.value()
                                   : GgaFixQuality.isValid() ? GgaFixQuality.value() : "-";
                const char* satsUsed = GnGgaSatsUsed.isValid() ? GnGgaSatsUsed.value()
                                       : GgaSatsUsed.isValid() ? GgaSatsUsed.value() : "-";
                const char* satsView = GnGsvSatsInView.isValid() ? GnGsvSatsInView.value()
                                       : GsvSatsInView.isValid() ? GsvSatsInView.value() : "-";
                LOG_INFO( "gps-diag: chars=%lu (+%lu) withFix=%lu passed=%lu failed=%lu "
                          "fixQ=%s satsUsed=%s satsInView=%s locValid=%d",
                          ( unsigned long )chars, ( unsigned long )( chars - last_chars ),
                          ( unsigned long )Gps.sentencesWithFix(),
                          ( unsigned long )Gps.passedChecksum(),
                          ( unsigned long )Gps.failedChecksum(),
                          fixQ, satsUsed, satsView,
                          ( int )Gps.location.isValid() );
                last_chars = chars;
            }
        }
    }
    void Wake()
    {
        // I never managed to get low power mode working on the new SAM-M8Q.
        // auto bytes_pushed = uart_write_bytes( GpsUartPort, WakeCommand, sizeof( WakeCommand ) - 1 );
        // uart_wait_tx_done( GpsUartPort, 100 );
    }
    void Sleep()
    {
        // I never managed to get low power mode working on the new SAM-M8Q.
        // auto bytes_pushed = uart_write_bytes( GpsUartPort, StandbyCommand, sizeof( StandbyCommand ) - 1 );
        // uart_wait_tx_done( GpsUartPort, 100 );
    }
}