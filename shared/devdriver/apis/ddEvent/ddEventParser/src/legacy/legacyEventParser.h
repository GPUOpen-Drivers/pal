/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */

#pragma once

#include <ddPlatform.h>
#include <protocols/ddEventProtocol.h>

namespace DevDriver
{

namespace EventProtocol
{
    class EventClient;

    // Structure that contains information about the current event being handled by the parser
    struct EventReceivedInfo
    {
        EventProviderId providerId;         /// Id of the event provider that emitted this event
        uint32          eventId;            /// Id of the event within the provider
        uint32          eventIndex;         /// Index of the event within the provider's event stream
                                            /// This can be used to verify that all events were correctly
                                            /// captured in the data stream.
        uint32          padding;            /// Padding bytes
        uint64          payloadSize;        /// Size of the payload data associated with the event in bytes
        uint64          timestampFrequency; /// Frequency of the timestamp associated with this event (ticks per second)
        uint64          timestamp;          /// Timestamp recorded when this event was emitted by the provider
    };

    // Called once per event in the input data stream
    // If the user returns Result::Rejected from this callback, the implementation will avoid calling the payload data
    // callback for any of the payload data associated with this event.
    typedef Result (*EventReceived)(void* pUserdata, const EventReceivedInfo& eventInfo);

    // Called many times per event to deliver event payload data to the user
    typedef Result (*EventPayloadData)(void* pUserdata, const void* pData, size_t dataSize);

    // Structure used to configure the callbacks used by the event parser
    struct ParserCallbackInfo
    {
        EventReceived    pfnEventReceived;
        EventPayloadData pfnPayloadData;
        void*            pUserdata;
    };

    // Class that can be used to parse individual events and payloads out of an input data stream.
    // The parser is capable of parsing the data in small chunks rather than requiring the full buffer to be available
    // in contiguous memory. This allows it to be used in streaming scenarios.
    class EventParser
    {
    public:
        EventParser();
        ~EventParser();

        // Sets the parser callback
        // This will be called whenever a new event is available and whenever data from an event payload arrives
        void SetCallback(const ParserCallbackInfo& callbackInfo)
        {
            m_callback = callbackInfo;
        }

        // Parses the provided event data
        Result Parse(const void* pEventData, size_t eventDataSize);

    private:
        static size_t GetTokenSize(EventTokenType tokenType);
        void ResetEventDataBufferState();

        void WriteIntoTokenBuffer(const void* pData, size_t dataSize);
        void ProcessToken();
        Result EmitEventReceived(const EventDataToken* pDataToken);
        Result EmitPayloadData(const void* pData, size_t dataSize);

        enum class EventDataState : uint32
        {
            WaitingForHeader  = 0,
            WaitingForToken   = 1,
            WaitingForPayload = 2,
        };

        ParserCallbackInfo m_callback;
        uint8              m_eventTokenBuffer[EventProtocol::kMaxEventTokenSize];
        size_t             m_eventTokenBufferSize;
        size_t             m_eventPayloadBytesRead;
        EventDataState     m_eventDataState;
        bool               m_skipPayloadData;
        EventProviderId    m_currentProviderId;
        uint64             m_currentTimestampFrequency;
        uint64             m_currentTimestamp;
    };

}
} // DevDriver
