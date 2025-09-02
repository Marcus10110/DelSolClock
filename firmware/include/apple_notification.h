#pragma once

#include <cstdint>
#include <string>
#include <optional>

namespace AppleNotifications
{
    struct NotificationSummary;

    namespace Detail
    {
        uint32_t Int32FromBytes( uint8_t* bytes );
        time_t ParseDateTime( const std::string& date_string );
        bool IsNavigationNotification( const NotificationSummary& notification );
    }
    struct Notification
    {
        enum class EventId : uint8_t
        {
            NotificationAdded = 0,
            NotificationModified = 1,
            NotificationRemoved = 2
        };
        enum class EventFlags : uint8_t
        {
            Sent = 1 << 0,
            Important = 1 << 1,
            PreExisting = 1 << 2,
            PositiveAction = 1 << 3,
            NegativeAction = 1 << 4
        };
        enum class CategoryId : uint8_t
        {
            Other = 0,
            IncomingCall = 1,
            MissedCall = 2,
            VoiceMail = 3,
            Social = 4,
            Schedule = 5,
            Email = 6,
            News = 7,
            HealthAndFitness = 8,
            BusinessAndFinance = 9,
            Location = 10,
            Entertainment = 11
        };

        EventId mEventId;
        uint8_t mEventFlags;
        CategoryId mCategoryId;
        uint8_t mCategoryCount;
        uint32_t mNotificationUID;

        static Notification Parse( uint8_t* data, int length );
        void Dump() const;
    };


    enum class CommandId : uint8_t
    {
        GetNotificationAttributes = 0,
        GetAppAttributes = 1,
        PerformNotificationAction = 2
    };

    enum class AttributeId : uint8_t
    {
        AppIdentifier = 0,
        Title = 1,    // (Needs to be followed by a 2-bytes max length parameter)
        Subtitle = 2, // (Needs to be followed by a 2-bytes max length parameter)
        Message = 3,  // (Needs to be followed by a 2-bytes max length parameter)
        MessageSize = 4,
        Date = 5,
        PositiveActionLabel = 6,
        NegativeActionLabel = 7
    };

    struct NotificationAttribute
    {
        CommandId mCommandId;
        uint32_t mNotificationUID;
        AttributeId mAttributeId;
        // Note, this only supports 1 attribute at a time.
        std::string mValue;

        static NotificationAttribute Parse( uint8_t* data, int length );
        void Dump();
    };

    struct NotificationSummary
    {
        uint32_t mNotificationUID;
        uint32_t mReceivedTime{ 0 };
        uint32_t mDeleteAfterTime{ 0 };
        std::optional<std::string> mAppIdentifier;
        std::optional<std::string> mTitle;
        std::optional<std::string> mSubtitle;
        std::optional<std::string> mMessage;
        std::optional<std::string> mDateString;
        std::optional<time_t> mDateTime;

        void SetAttribute( AttributeId id, const std::string& value );
        void Dump();
        std::optional<AttributeId> NextAttributeToRequest() const;
        bool IsComplete() const;
    };
}