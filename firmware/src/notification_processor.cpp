#include "notification_processor.h"

#include "apple_notification.h"
#include "logger.h"
#include "utilities.h"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <esp32-hal.h>
#include <esp_task_wdt.h>

#include <map>
#include <deque>
#include <mutex>


namespace NotificationProcessor
{
    namespace
    {
        int NotificationSourceQueue_Length = 100;
        QueueHandle_t NotificationSourceQueue = nullptr;
        int DataQueue_Length = 15;
        QueueHandle_t DataQueue = nullptr;
        TaskHandle_t TaskHandle = nullptr;
        bool IsInitialized = false;
        bool IsTaskStarted = false;
        NotificationProcessor::RequestAttributeCallback AttributeCallback = nullptr;

        std::mutex OpenNotificationsMutex;
        std::map<uint32_t, AppleNotifications::NotificationSummary> OpenNotifications;
        std::deque<uint32_t> Inbox;

        constexpr double MaxNotificationAgeSeconds = 2 * 60 * 60;
        constexpr uint32_t RequestTimeoutMs = 10 * 1000;

        bool ShouldKeepNotification( const AppleNotifications::NotificationSummary& summary )
        {
            if( !summary.mDateTime.has_value() )
            {
                return false;
            }

            // check if it's less than MaxNotificationAgeSeconds
            double age_seconds = difftime( time( nullptr ), summary.mDateTime.value() );
            if( age_seconds > MaxNotificationAgeSeconds )
            {
                return false;
            }
            return true;
        }

        int RequestAttributes( uint32_t uid )
        {
            AttributeCallback( uid, AppleNotifications::AttributeId::Title );
            AttributeCallback( uid, AppleNotifications::AttributeId::Subtitle );
            AttributeCallback( uid, AppleNotifications::AttributeId::Message );
            AttributeCallback( uid, AppleNotifications::AttributeId::AppIdentifier );
            AttributeCallback( uid, AppleNotifications::AttributeId::Date );
            return 5;
        }

        void Task( void* )
        {
            LOG_INFO( "NotificationProcessor::Task started" );

            while( !NotificationSourceQueue || !DataQueue )
            {
                LOG_ERROR( "Queues not initialized yet..." );
                vTaskDelay( pdMS_TO_TICKS( 100 ) );
            }

            // bool in_flight = false;
            int in_flight_count = 0;
            uint32_t request_start_ms = 0;
            uint32_t in_flight_uuid = 0;
            uint32_t latest_new_notification_ms = 0;

            while( true )
            {
                esp_task_wdt_reset();
                static uint32_t water_stamp = millis();
                if( millis() > water_stamp + 5000 )
                {
                    water_stamp = millis();
                    UBaseType_t water = uxTaskGetStackHighWaterMark( nullptr );
                    LOG_TRACE( "NotificationProcessor::Task stack high water mark: %u bytes", water * sizeof( StackType_t ) );
                }


                static AppleNotifications::Notification notification;
                bool any_notifications = false;
                while( xQueueReceive( NotificationSourceQueue, &notification, 0 ) == pdTRUE )
                {
                    esp_task_wdt_reset();
                    any_notifications = true;
                    switch( notification.mEventId )
                    {
                    case AppleNotifications::Notification::EventId::NotificationAdded:
                    {
                        LOG_TRACE( "task added notification %u", notification.mNotificationUID );
                        PRINT_MEMORY_USAGE();
                        std::scoped_lock lock( OpenNotificationsMutex );
                        Inbox.push_back( notification.mNotificationUID );
                    }
                    break;
                    case AppleNotifications::Notification::EventId::NotificationModified:
                    {
                        LOG_TRACE( "task modified notification %u", notification.mNotificationUID );
                        std::scoped_lock lock( OpenNotificationsMutex );
                        if( OpenNotifications.count( notification.mNotificationUID ) )
                        {
                            OpenNotifications.erase( notification.mNotificationUID );
                            Inbox.push_back( notification.mNotificationUID );
                        }
                    }
                    break;
                    case AppleNotifications::Notification::EventId::NotificationRemoved:
                    {
                        LOG_TRACE( "task removed notification %u", notification.mNotificationUID );
                        // TODO: instead mark for deletion later.
                        std::scoped_lock lock( OpenNotificationsMutex );
                        if( OpenNotifications.count( notification.mNotificationUID ) )
                        {
                            OpenNotifications[ notification.mNotificationUID ].mDeleteAfterTime = millis() + 5000;
                        }
                    }
                    break;
                    }
                }
                if( any_notifications )
                {
                    latest_new_notification_ms = millis();
                    LOG_TRACE( "Finished processing notifications in loop." );
                }

                // process all data queue items.
                static RawAttribute raw_attribute;
                static AppleNotifications::NotificationAttribute parsed_attribute;
                bool any_attributes = false;
                while( xQueueReceive( DataQueue, &raw_attribute, 0 ) == pdTRUE )
                {
                    esp_task_wdt_reset();
                    any_attributes = true;

                    in_flight_count--;

                    parsed_attribute = AppleNotifications::NotificationAttribute::Parse( raw_attribute.mData, raw_attribute.mLength );
                    LOG_DEBUG( "task processed attribute %i %u", parsed_attribute.mAttributeId, parsed_attribute.mNotificationUID );
                    {
                        std::scoped_lock lock( OpenNotificationsMutex );
                        auto& summary = OpenNotifications[ parsed_attribute.mNotificationUID ];
                        summary.SetAttribute( parsed_attribute.mAttributeId, parsed_attribute.mValue );
                        if( summary.IsComplete() )
                        {
                            // delete it if we don't want to keep it!

                            LOG_TRACE( "task completed notification %u", parsed_attribute.mNotificationUID );

                            if( !ShouldKeepNotification( summary ) )
                            {
                                LOG_TRACE( "task deleting notification %u due to age", parsed_attribute.mNotificationUID );
                                OpenNotifications.erase( parsed_attribute.mNotificationUID );
                            }
                            else
                            {
                                summary.Dump();
                                summary.mReceivedTime = millis();
                            }
                        }
                    }
                }
                if( any_attributes )
                {
                    LOG_TRACE( "Finished processing attributes in loop." );

                    if( in_flight_count == 0 )
                    {
                        // delay 0.1 seconds
                        LOG_TRACE( "delaying for 0.1 seconds after complete notification load." );
                        vTaskDelay( pdMS_TO_TICKS( 100 ) );
                    }
                }

                if( in_flight_count > 0 && ( millis() - request_start_ms > RequestTimeoutMs ) )
                {
                    in_flight_count = 0;
                    if( OpenNotifications.count( in_flight_uuid ) > 0 )
                    {
                        LOG_WARN( "canceled timed-out in-flight notification %u", in_flight_uuid );
                        OpenNotifications.erase( in_flight_uuid );
                    }
                }

                if( in_flight_count == 0 )
                {
                    if( millis() > ( latest_new_notification_ms + 500 ) && Inbox.size() > 0 && AttributeCallback != nullptr )
                    { // put the next one in flight?
                        // find the first OpenNotifications which has a non-empty NextAttributeToRequest.
                        uint32_t uid;
                        {
                            std::scoped_lock lock( OpenNotificationsMutex );
                            uid = Inbox.front();
                            Inbox.pop_front();
                        }
                        in_flight_uuid = uid;
                        in_flight_count += RequestAttributes( uid );
                        request_start_ms = millis();
                        LOG_TRACE( "requested %i attributes for notification %u", in_flight_count, uid );
                    }
                }
                else
                {
                    // something is in flight and we're waiting for it.
                    vTaskDelay( pdMS_TO_TICKS( 1 ) );
                    continue;
                }

                if( in_flight_count == 0 )
                {
                    // nothing is in flight, we're caught up.

                    // prune notifications!
                    {
                        std::scoped_lock lock( OpenNotificationsMutex );
                        for( auto it = OpenNotifications.begin(); it != OpenNotifications.end(); )
                        {
                            if( it->second.mDeleteAfterTime != 0 && millis() > it->second.mDeleteAfterTime )
                            {
                                LOG_TRACE( "deleted later %u", it->first );
                                it = OpenNotifications.erase( it );
                            }
                            else if( !ShouldKeepNotification( it->second ) )
                            {
                                LOG_TRACE( "deleted due to age %u", it->first );
                                it = OpenNotifications.erase( it );
                            }
                            else
                            {
                                ++it;
                            }
                        }
                    }


                    vTaskDelay( pdMS_TO_TICKS( 10 ) );
                }
            }
        }
    }

    void Init()
    {
        if( !IsInitialized )
        {
            LOG_TRACE( "NotificationProcessor::Init" );
            PRINT_MEMORY_USAGE();
            // one time stuff.
            IsInitialized = true;
            NotificationSourceQueue = xQueueCreate( NotificationSourceQueue_Length, sizeof( AppleNotifications::Notification ) );
            PRINT_MEMORY_USAGE();
            DataQueue = xQueueCreate( DataQueue_Length, sizeof( RawAttribute ) );
            LOG_TRACE( "NotificationProcessor::Init done" );
            PRINT_MEMORY_USAGE();

            // Subscribe your BLE notifications to on_ns_notify / on_ds_notify first…
        }
    }

    void Begin( RequestAttributeCallback attribute_request_callback )

    {
        // init can be called more than once, in which this will be updated.
        AttributeCallback = attribute_request_callback;

        if( !IsTaskStarted )
        {
            if( !IsInitialized )
            {
                LOG_WARN( "Begin called before Init()" );
                Init();
            }
            LOG_TRACE( "NotificationProcessor::Begin" );
            PRINT_MEMORY_USAGE();
            IsTaskStarted = true;
            // Create worker on APP core; priority same or -1 vs loopTask:
            UBaseType_t loop_priority = uxTaskPriorityGet( xTaskGetCurrentTaskHandle() );
            UBaseType_t task_priority = ( loop_priority > tskIDLE_PRIORITY ) ? loop_priority - 1 : loop_priority;
            LOG_TRACE( "current thread priority: %d, new priority: %d", loop_priority, task_priority );
            xTaskCreatePinnedToCore( Task, "notif_proc", 4096, nullptr, task_priority, &TaskHandle,
                                     ARDUINO_RUNNING_CORE // usually 1, the app core
            );
            esp_task_wdt_add( TaskHandle );
            LOG_TRACE( "NotificationProcessor::Begin done" );
            PRINT_MEMORY_USAGE();
        }
    }

    void HandleNotification( const AppleNotifications::Notification& notification )
    {
        if( !NotificationSourceQueue )
        {
            LOG_ERROR( "Notification source queue not initialized" );
            return;
        }
        auto result = xQueueSend( NotificationSourceQueue, &notification, 0 );
        if( result != pdTRUE )
        {
            LOG_ERROR( "Failed to send notification to queue" );
        }
    }

    void HandleAttribute( const RawAttribute& attribute )
    {
        if( !DataQueue )
        {
            LOG_ERROR( "Data queue not initialized" );
            return;
        }
        auto result = xQueueSend( DataQueue, &attribute, 0 );
        if( result != pdTRUE )
        {
            LOG_ERROR( "Failed to send attribute to queue" );
        }
    }

    bool GetLatestNotification( AppleNotifications::NotificationSummary& notification )
    {
        std::scoped_lock lock( OpenNotificationsMutex );
        if( OpenNotifications.empty() )
        {
            return false;
        }
        auto* latest = &( OpenNotifications.begin()->second );
        for( auto& n : OpenNotifications )
        {
            if( n.second.mDateTime > latest->mDateTime )
            {
                latest = &( n.second );
            }
        }
        notification = *latest;
        return true;
    }
    bool GetLatestNavigationNotification( AppleNotifications::NotificationSummary& notification )
    {
        std::scoped_lock lock( OpenNotificationsMutex );
        if( OpenNotifications.empty() )
        {
            return false;
        }
        AppleNotifications::NotificationSummary* latest = nullptr;

        for( auto& n : OpenNotifications )
        {
            if( !AppleNotifications::Detail::IsNavigationNotification( n.second ) )
            {
                continue;
            }

            if( latest == nullptr || n.second.mReceivedTime > latest->mReceivedTime )
            {
                latest = &n.second;
            }
        }
        if( latest == nullptr )
        {
            return false;
        }
        notification = *latest;
        return true;
    }
}
