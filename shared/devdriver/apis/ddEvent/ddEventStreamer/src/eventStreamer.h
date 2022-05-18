/* Copyright (c) 2022 Advanced Micro Devices, Inc. All rights reserved. */

#pragma once

#include <ddEventStreamer.h>

#include <ddCommon.h>

#include <ddEventClient.h>
#include <ddEventParser.h>

namespace Event
{

class EventStreamer
{
public:
    EventStreamer();
    ~EventStreamer();

    void SetEventCallback(
        const DDEventStreamCallback& eventCallback);

    DD_RESULT BeginStreaming(
        const DDEventStreamerCreateInfo& createInfo);

    DD_RESULT EndStreaming();

    bool IsStreaming()          const { return m_isStreaming; }
    bool HasEncounteredErrors() const { return m_encounteredErrors; }

private:
    DD_RESULT ParserOnBegin(
        const DDEventParserEventInfo& eventInfo,
        size_t                        totalPayloadSize);

    DD_RESULT ParserOnWritePayloadChunk(
        const DDEventParserEventInfo& eventInfo,
        const void* pData,
        size_t dataSize);

    DD_RESULT ParserOnEnd(
        const DDEventParserEventInfo& eventInfo,
        DD_RESULT finalResult);

    void OnEventData(const void* pData, size_t dataSize);

    void EventReceiveThreadFunc();

    DDEventStreamerCreateInfo   m_hStreamerContext;   /// Metadata for streaming initialization
    DDEventClient               m_hEventClient;       /// Client for communicating with the event server
    DDEventParser               m_hEventParser;       /// Parser for parsing event data
    DevDriver::Platform::Thread m_eventThread;        /// Thread spawned to receive events
    bool                        m_exitRequested;      /// Flag to indicate that the event thread should exit
    bool                        m_isStreaming;        /// True if the streamer is currently streaming
    bool                        m_encounteredErrors;  /// Set to true if an error was encountered during streaming
    bool                        m_hasValidCallback;   /// Flag to indicate if the callback is valid
    DevDriver::Vector<uint8_t>  m_eventPayloadBuffer; /// Buffer for storing event data
    DDEventStreamerCallback     m_eventCb;            /// Event callback handle
};

}; // namespace Event
