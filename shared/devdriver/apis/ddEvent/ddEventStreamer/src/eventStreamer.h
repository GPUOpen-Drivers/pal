/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2022 Advanced Micro Devices, Inc. All Rights Reserved.
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *
 **********************************************************************************************************************/

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
