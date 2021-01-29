/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2020-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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
/**
***********************************************************************************************************************
* @file  ddEventParser.cpp
* @brief Implementation for EventParser
***********************************************************************************************************************
*/

#include <protocols/ddEventParser.h>
#include <util/ddByteReader.h>

namespace DevDriver
{
namespace EventProtocol
{

// =====================================================================================================================
EventParser::EventParser()
    : m_eventTokenBufferSize(0)
    , m_eventPayloadBytesRead(0)
    , m_eventDataState(EventDataState::WaitingForHeader)
    , m_skipPayloadData(false)
    , m_currentProviderId(0)
    , m_currentTimestampFrequency(0)
    , m_currentTimestamp(0)
{
}

// =====================================================================================================================
EventParser::~EventParser()
{
}

// =====================================================================================================================
size_t EventParser::GetTokenSize(EventTokenType tokenType)
{
    switch (tokenType)
    {
    case EventTokenType::Provider:
        return sizeof(EventProviderToken);
    case EventTokenType::Data:
        return sizeof(EventDataToken);
    case EventTokenType::Timestamp:
        return sizeof(EventTimestampToken);
    case EventTokenType::TimeDelta:
        return sizeof(EventTimeDeltaToken);
    case EventTokenType::Count:
        break;
    }
    DD_ASSERT_REASON("Invalid token type!");
    return 0;
}

// =====================================================================================================================
Result EventParser::Parse(const void* pEventData, size_t eventDataSize)
{
    Result result = Result::Success;

    ByteReader reader(pEventData, eventDataSize);
    while (reader.HasBytes() && (result == Result::Success))
    {
        switch (m_eventDataState)
        {
        case EventDataState::WaitingForHeader:
        {
            // We should only be looking for a token header when we have an empty buffer
            DD_ASSERT(m_eventTokenBufferSize == 0);

            const EventTokenHeader* pTokenHeader = nullptr;
            result = reader.Get(&pTokenHeader);
            if (result == Result::Success)
            {
                WriteIntoTokenBuffer(pTokenHeader, sizeof(EventTokenHeader));

                m_eventDataState = EventDataState::WaitingForToken;
            }
            break;
        }
        case EventDataState::WaitingForToken:
        {
            ByteReader bufferReader(m_eventTokenBuffer, m_eventTokenBufferSize);

            const EventTokenHeader* pTokenHeader = nullptr;
            result = bufferReader.Get(&pTokenHeader);
            if (result == Result::Success)
            {
                const size_t tokenSize = GetTokenSize(static_cast<EventTokenType>(pTokenHeader->id));

                const size_t bytesCopied = bufferReader.Remaining();
                const size_t copySize = Platform::Min(reader.Remaining(), (tokenSize - bytesCopied));

                const void* pBytes = nullptr;
                result = reader.GetBytes(&pBytes, copySize);
                if (result == Result::Success)
                {
                    WriteIntoTokenBuffer(pBytes, copySize);

                    if (m_eventTokenBufferSize == (tokenSize + sizeof(EventTokenHeader)))
                    {
                        if ((pTokenHeader->id == static_cast<uint8>(EventTokenType::Data)) ||
                            (pTokenHeader->id == static_cast<uint8>(EventTokenType::TimeDelta)))
                        {
                            if (pTokenHeader->id == static_cast<uint8>(EventTokenType::Data))
                            {
                                const EventDataToken* pDataToken =
                                    reinterpret_cast<const EventDataToken*>(m_eventTokenBuffer + sizeof(EventTokenHeader));

                                result = EmitEventReceived(pDataToken);
                            }

                            m_eventDataState = EventDataState::WaitingForPayload;
                        }
                        else
                        {
                            ProcessToken();
                        }
                    }
                }
            }

            break;
        }
        case EventDataState::WaitingForPayload:
        {
            ByteReader bufferReader(m_eventTokenBuffer, m_eventTokenBufferSize);

            const EventTokenHeader* pTokenHeader = nullptr;
            result = bufferReader.Get(&pTokenHeader);
            if (result == Result::Success)
            {
                if (pTokenHeader->id == static_cast<uint8>(EventTokenType::TimeDelta))
                {
                    const EventTimeDeltaToken* pToken = nullptr;
                    result = bufferReader.Get(&pToken);
                    if (result == Result::Success)
                    {
                        const size_t bytesCopied = bufferReader.Remaining();
                        const size_t copySize = Platform::Min(reader.Remaining(), (pToken->numBytes - bytesCopied));

                        const void* pBytes = nullptr;
                        result = reader.GetBytes(&pBytes, copySize);
                        if (result == Result::Success)
                        {
                            WriteIntoTokenBuffer(pBytes, copySize);

                            const size_t finalDataSize = (sizeof(EventTokenHeader) + sizeof(EventTimeDeltaToken) + pToken->numBytes);
                            if (m_eventTokenBufferSize == finalDataSize)
                            {
                                ProcessToken();
                            }
                        }
                    }
                }
                else if (pTokenHeader->id == static_cast<uint8>(EventTokenType::Data))
                {
                    const EventDataToken* pToken = nullptr;
                    result = bufferReader.Get(&pToken);
                    if (result == Result::Success)
                    {
                        // Make sure the token size actually fits in a pointer before casting it
                        if (pToken->size <= SIZE_MAX)
                        {
                            const size_t payloadSize = static_cast<size_t>(pToken->size);

                            const size_t copySize = Platform::Min(reader.Remaining(), (payloadSize - m_eventPayloadBytesRead));

                            const void* pBytes = nullptr;
                            result = reader.GetBytes(&pBytes, copySize);
                            if (result == Result::Success)
                            {
                                result = EmitPayloadData(pBytes, copySize);
                            }

                            if (result == Result::Success)
                            {
                                if (m_eventPayloadBytesRead == payloadSize)
                                {
                                    ResetEventDataBufferState();
                                }
                            }
                        }
                        else
                        {
                            DD_ASSERT_REASON("Packet too large for 32bit client implementation!");
                            result = Result::Aborted;
                        }
                    }
                }
                else
                {
                    DD_ASSERT_REASON("Invalid token type!");
                    result = Result::Aborted;
                }
            }

            break;
        }
        }
    }

    return result;
}

// =====================================================================================================================
void EventParser::ResetEventDataBufferState()
{
    m_eventTokenBufferSize  = 0;
    m_eventPayloadBytesRead = 0;
    m_eventDataState        = EventDataState::WaitingForHeader;
}

// =====================================================================================================================
void EventParser::WriteIntoTokenBuffer(const void* pData, size_t dataSize)
{
    DD_ASSERT(m_eventTokenBufferSize + dataSize <= sizeof(m_eventTokenBuffer));

    memcpy(m_eventTokenBuffer + m_eventTokenBufferSize, pData, dataSize);
    m_eventTokenBufferSize += dataSize;
}

// =====================================================================================================================
void EventParser::ProcessToken()
{
    const EventTokenHeader* pTokenHeader = reinterpret_cast<const EventTokenHeader*>(m_eventTokenBuffer);
    const void* pTokenData = (m_eventTokenBuffer + sizeof(EventTokenHeader));
    switch (static_cast<EventTokenType>(pTokenHeader->id))
    {
    case EventTokenType::Provider:
    {
        const auto* pToken = reinterpret_cast<const EventProviderToken*>(pTokenData);

        m_currentProviderId         = pToken->id;
        m_currentTimestampFrequency = pToken->frequency;
        m_currentTimestamp          = pToken->timestamp;

        break;
    }
    case EventTokenType::Data:
    {
        // Data tokens must be handled separately
        DD_ASSERT_REASON("Data tokens should never be processed here!");

        break;
    }
    case EventTokenType::Timestamp:
    {
        const auto* pToken = reinterpret_cast<const EventTimestampToken*>(pTokenData);

        m_currentTimestampFrequency = pToken->frequency;
        m_currentTimestamp          = pToken->timestamp;

        break;
    }
    case EventTokenType::TimeDelta:
    {
        const auto* pToken = reinterpret_cast<const EventTimeDeltaToken*>(pTokenData);

        // Clamp the max number of bytes to 6 since that's the max that the spec allows
        const size_t numBytes = Platform::Min(static_cast<uint32>(pToken->numBytes), 6u);

        const void* pTimeDeltaMemory = (m_eventTokenBuffer       +
                                        sizeof(EventTokenHeader) +
                                        sizeof(EventTimeDeltaToken));

        // Extract the time delta from the token data
        uint64 currentTimeDelta = 0;
        memcpy(&currentTimeDelta, pTimeDeltaMemory, numBytes);

        // Add the time delta to our current timestamp
        m_currentTimestamp += currentTimeDelta;

        break;
    }
    case EventTokenType::Count:
    {
        DD_ASSERT_REASON("Invalid token type");

        break;
    }
    }

    ResetEventDataBufferState();
}

// =====================================================================================================================
Result EventParser::EmitEventReceived(const EventDataToken* pDataToken)
{
    Result result = Result::Success;

    if (m_callback.pfnEventReceived != nullptr)
    {
        EventReceivedInfo info = {};

        info.providerId = m_currentProviderId;
        info.eventId = pDataToken->id;
        info.eventIndex = pDataToken->index;
        info.payloadSize = pDataToken->size;
        info.timestampFrequency = m_currentTimestampFrequency;
        info.timestamp = m_currentTimestamp;

        result = m_callback.pfnEventReceived(m_callback.pUserdata, info);

        // If the user returns Rejected from the event callback, then we shouldn't send them
        // the payload data for the current event.
        if (result == Result::Rejected)
        {
            // Change the result to success since this is a supported situation.
            result = Result::Success;

            m_skipPayloadData = true;
        }
        else
        {
            m_skipPayloadData = false;
        }
    }

    return result;
}

// =====================================================================================================================
Result EventParser::EmitPayloadData(const void* pData, size_t dataSize)
{
    Result result = Result::Success;

    if ((m_skipPayloadData == false) && (m_callback.pfnPayloadData != nullptr))
    {
        result = m_callback.pfnPayloadData(m_callback.pUserdata, pData, dataSize);
    }

    if (result == Result::Success)
    {
        m_eventPayloadBytesRead += dataSize;
    }

    return result;
}

} // EventProtocol
} // DevDriver
