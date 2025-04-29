/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include <eventParser.h>
#include <ddCommon.h>
#include <util/ddEventTimer.h>

using namespace DevDriver;
using namespace EventProtocol;

namespace Event
{

// =======================================================================================
EventParser::EventParser(const DDEventWriter& writer)
    : m_writer(writer)
    , m_isReadingPayload(false)
    , m_payloadBytesRemaining(0)
{
    EventProtocol::ParserCallbackInfo callbackInfo = {};
    callbackInfo.pUserdata = this;
    callbackInfo.pfnEventReceived = [](void* pUserdata, const EventProtocol::EventReceivedInfo& eventInfo) -> Result {
        auto* pThis = reinterpret_cast<EventParser*>(pUserdata);
        return pThis->EventReceived(eventInfo);
    };
    callbackInfo.pfnPayloadData = [](void* pUserdata, const void* pData, size_t dataSize) -> Result {
        auto* pThis = reinterpret_cast<EventParser*>(pUserdata);
        return pThis->PayloadData(pData, dataSize);
    };
    m_parser.SetCallback(callbackInfo);
}

// =======================================================================================
EventParser::EventParser()
{
}

// =======================================================================================
DD_RESULT EventParser::Parse(
    const void* pData,
    size_t      dataSize)
{
    const Result result = m_parser.Parse(pData, dataSize);

    return DevDriverToDDResult(result);
}

// =======================================================================================
Result EventParser::EventReceived(
    const EventProtocol::EventReceivedInfo& eventInfo)
{
    Result result = Result::Error;

    if (m_isReadingPayload == false)
    {
        m_eventInfo.timestampFrequency = eventInfo.timestampFrequency;
        m_eventInfo.timestamp = eventInfo.timestamp;
        m_eventInfo.providerId = eventInfo.providerId;
        m_eventInfo.eventId = eventInfo.eventId;
        m_eventInfo.eventIndex = eventInfo.eventIndex;

        if (m_writer.pfnBegin(m_writer.pUserdata, &m_eventInfo, eventInfo.payloadSize) == DD_RESULT_SUCCESS)
        {
            if (eventInfo.payloadSize > 0)
            {
                m_isReadingPayload = true;
                m_payloadBytesRemaining = eventInfo.payloadSize;

                result = Result::Success;
            }
            else
            {
                if (m_writer.pfnEnd(m_writer.pUserdata, &m_eventInfo, DD_RESULT_SUCCESS) == DD_RESULT_SUCCESS)
                {
                    result = Result::Success;
                }
            }
        }
    }

    return result;
}

// =======================================================================================
Result EventParser::PayloadData(
    const void* pData,
    size_t      dataSize)
{
    Result result = Result::Error;

    if (m_isReadingPayload && (m_payloadBytesRemaining >= dataSize))
    {
        const DD_RESULT writeResult = m_writer.pfnWritePayloadChunk(m_writer.pUserdata, &m_eventInfo, pData, dataSize);
        if (writeResult == DD_RESULT_SUCCESS)
        {
            m_payloadBytesRemaining -= dataSize;

            if (m_payloadBytesRemaining == 0)
            {
                // If the end of the payload has been reached, trigger the end callback
                m_isReadingPayload = false;

                if (m_writer.pfnEnd(m_writer.pUserdata, &m_eventInfo, DD_RESULT_SUCCESS) == DD_RESULT_SUCCESS)
                {
                    result = Result::Success;
                }
            }
            else
            {
                // We've processed this data successfully but there's more payload data remaining
                result = Result::Success;
            }
        }
        else
        {
            m_writer.pfnEnd(m_writer.pUserdata, &m_eventInfo, writeResult);
        }
    }

    return result;
}

// =======================================================================================
bool EventParser::CopyToTokenBuffer(size_t tokenItemSize)
{
    bool needMoreData = false;

    DD_ASSERT(m_tokenItemStart <= m_tokenDataSize);
    size_t tokenItemPartialSize = m_tokenDataSize - m_tokenItemStart;

    DD_ASSERT(tokenItemSize >= tokenItemPartialSize);
    size_t remainingTokenItemSize = (tokenItemSize - tokenItemPartialSize);

    DD_ASSERT(m_bufferSize >= m_cursor);
    size_t remainingBufferSize = (m_bufferSize - m_cursor);

    size_t sizeToCopy = 0;
    if (remainingTokenItemSize <= remainingBufferSize)
    {
        sizeToCopy = remainingTokenItemSize;
        needMoreData = false;
    }
    else
    {
        sizeToCopy = remainingBufferSize;
        needMoreData = true;
    }

    if (sizeToCopy > 0)
    {
        memcpy(
            (m_tokenBuffer + m_tokenDataSize),
            (m_pBuffer + m_cursor),
            sizeToCopy);

        m_tokenDataSize += sizeToCopy;
        m_cursor += sizeToCopy;
    }

    if (needMoreData == false)
    {
        // We now have a complete token item, move `m_tokenItemStart` to
        // `m_tokenDataSize` to prepare for the next token item.
        m_tokenItemStart = m_tokenDataSize;
    }

    return needMoreData;
}

// =======================================================================================
void EventParser::ResetTokenBuffer()
{
    m_tokenItemStart = 0;
    m_tokenDataSize = 0;
}

// =======================================================================================
DD_EVENT_PARSER_STATE EventParser::Parse()
{
    // Caller of `Parse()` would take action based on the returned `parserState`.
    DD_EVENT_PARSER_STATE parserState = DD_EVENT_PARSER_STATE_UNKNOWN;

    // Whether to yield to caller to give it a chance to respond.
    bool yield = false;

    while (!yield)
    {
        switch (m_parsingState)
        {
        case ParserInternalState::ParsingHeader:
        {
            // `m_tokenBuffer` should be empty before parsing header.
            DD_ASSERT(m_tokenDataSize == 0);
            DD_ASSERT(m_tokenItemStart == 0);

            if (CopyToTokenBuffer(sizeof(EventTokenHeader)))
            {
                // Keep the same internal parsing state. Expect caller to set a
                // new data buffer before continue parsing.
                parserState = DD_EVENT_PARSER_STATE_NEED_MORE_DATA;
                yield = true;
            }
            else
            {
                // We have enough data to parse `EventTokenHeader`, advance
                // the internal parsing state.
                m_parsingState = ParserInternalState::ParsingToken;
            }
        } break;

        case ParserInternalState::ParsingToken:
        {
            const EventTokenHeader* tokenHeader = (const EventTokenHeader*)(m_tokenBuffer);
            EventTokenType tokenType = static_cast<EventTokenType>(tokenHeader->id);

            switch (tokenType)
            {
            case EventTokenType::Provider:
            {
                if (CopyToTokenBuffer(sizeof(EventProviderToken)))
                {
                    // Keep the same internal parsing state. Expect caller to set a
                    // new data buffer before continue parsing.
                    parserState = DD_EVENT_PARSER_STATE_NEED_MORE_DATA;
                    yield = true;
                }
                else
                {
                    // We have enough data to parse `EventProviderToken`, advance
                    // the internal parsing state, reset m_tokeBuffer and keep parsing.

                    const EventProviderToken* pToken =
                        (const EventProviderToken*)(m_tokenBuffer + sizeof(EventTokenHeader));

                    m_currProviderId = pToken->id;
                    m_currTimestampFrequency = pToken->frequency;
                    m_currTimestamp = pToken->timestamp;

                    m_parsingState = ParserInternalState::ParsingHeader;

                    ResetTokenBuffer();
                }
            } break;

            case EventTokenType::Timestamp:
            {
                if (CopyToTokenBuffer(sizeof(EventTimestampToken)))
                {
                    // Keep the same internal parsing state. Expect caller to set a
                    // new data buffer before continue parsing.
                    parserState = DD_EVENT_PARSER_STATE_NEED_MORE_DATA;
                    yield = true;
                }
                else
                {
                    // We have enough data to parse `EventTimestampToken`, advance
                    // the internal parsing state, reset m_tokeBuffer and keep parsing.

                    const EventTimestampToken* pToken =
                        (const EventTimestampToken*)(m_tokenBuffer + sizeof(EventTokenHeader));

                    m_currTimestampFrequency = pToken->frequency;
                    m_currTimestamp = pToken->timestamp;

                    m_parsingState = ParserInternalState::ParsingHeader;

                    ResetTokenBuffer();
                }
            } break;

            case EventTokenType::TimeDelta:
            {
                if (CopyToTokenBuffer(sizeof(EventTimeDeltaToken)))
                {
                    // Keep the same internal parsing state. Expect caller to set a
                    // new data buffer before continue parsing.
                    parserState = DD_EVENT_PARSER_STATE_NEED_MORE_DATA;
                    yield       = true;
                }
                else
                {
                    // We have enough data to parse `EventTimeDeltaToken`, advance
                    // the internal parsing state to parse the delta payload.
                    m_parsingState = ParserInternalState::ParsingPayload;
                }
            } break;

            case EventTokenType::Data:
            {
                if (CopyToTokenBuffer(sizeof(EventDataToken)))
                {
                    // Keep the same internal parsing state. Expect caller to set a
                    // new data buffer before continue parsing.
                    parserState = DD_EVENT_PARSER_STATE_NEED_MORE_DATA;
                }
                else
                {
                    // We have enough data to parse `EventDataToken`, advance
                    // the internal parsing state.
                    m_parsingState = ParserInternalState::ParsingPayload;
                    parserState = DD_EVENT_PARSER_STATE_EVENT_RECEIVED;
                }

                // Give caller a chance to respond to both parser states set
                // above.
                yield = true;
            } break;

            default:
            {
                DD_ASSERT_REASON("Invalid token type");
                yield = true;
            } break;
            }
        } break;

        case ParserInternalState::ParsingPayload:
        {
            const EventTokenHeader* tokenHeader = (const EventTokenHeader*)(m_tokenBuffer);
            EventTokenType tokenType = static_cast<EventTokenType>(tokenHeader->id);
            switch (tokenType)
            {
            case EventTokenType::Data:
            {
                const EventDataToken* pDataToken =
                    (const EventDataToken*)(m_tokenBuffer + sizeof(EventTokenHeader));

                if (pDataToken->size <= SIZE_MAX)
                {
                    const size_t totalPayloadSize = static_cast<size_t>(pDataToken->size);

                    // Data payload reading hasn't completed.
                    if (m_dataPayloadReadInBytes < totalPayloadSize)
                    {
                        // There is still data left in the current parsing buffer.
                        if (m_cursor < m_bufferSize)
                        {
                            m_currDataPayload.pData = (m_pBuffer + m_cursor);

                            size_t remainingBufferSize = m_bufferSize - m_cursor;
                            size_t remainingPayloadSize = totalPayloadSize - m_dataPayloadReadInBytes;

                            // Set `m_currDataPayload` object.
                            if (remainingPayloadSize <= remainingBufferSize)
                            {
                                m_currDataPayload.size = remainingPayloadSize;
                                m_cursor += remainingPayloadSize;
                                m_dataPayloadReadInBytes += remainingPayloadSize;
                            }
                            else
                            {
                                m_currDataPayload.size = remainingBufferSize;
                                m_cursor += remainingBufferSize;
                                m_dataPayloadReadInBytes += remainingBufferSize;
                            }
                            // Set the parser state to notify caller to retrieve
                            // `m_currDataPayload` and copy the data away.
                            parserState = DD_EVENT_PARSER_STATE_PAYLOAD_RECEIVED;
                            yield = true;
                        }
                        else
                        {
                            // No data left in the current parsing buffer, need a
                            // new buffer. Keep the internal parsing state. Expect
                            // caller to set a new buffer.
                            parserState = DD_EVENT_PARSER_STATE_NEED_MORE_DATA;
                            yield = true;
                        }
                    }
                    // All data payload has been read. Reset the parser to parse the
                    // next event.
                    else
                    {
                        m_dataPayloadReadInBytes = 0;
                        m_parsingState = ParserInternalState::ParsingHeader;
                        ResetTokenBuffer();

                        // Continue parsing the current buffer for another event token.
                    }
                }
                else
                {
                    DD_ASSERT_REASON("Packet too large for 32bit client implementation!");
                }
            } break;

            case EventTokenType::TimeDelta:
            {
                const EventTimeDeltaToken* pToken =
                    (const EventTimeDeltaToken*)(m_tokenBuffer + sizeof(EventTokenHeader));

                // Clamp the max number of bytes to 6 since that's the max
                // that the spec allows
                const size_t deltaBytes =
                    Platform::Min(static_cast<uint32>(pToken->numBytes), 6u);

                if (CopyToTokenBuffer(deltaBytes))
                {
                    parserState = DD_EVENT_PARSER_STATE_NEED_MORE_DATA;
                    yield = true;
                }
                else
                {
                    // Extract the time delta from the token data
                    uint64 currentTimeDelta = 0;
                    memcpy(
                        &currentTimeDelta,
                        (m_tokenBuffer + sizeof(EventTokenHeader) + sizeof(EventTimeDeltaToken)),
                        deltaBytes);

                    // Add the time delta to our current timestamp
                    m_currTimestamp += currentTimeDelta;

                    // Done parsing `EventTimeDeltaToken`, advance the internal
                    // parsing state to parse the next token header.
                    m_parsingState = ParserInternalState::ParsingHeader;

                    ResetTokenBuffer();
                }
            } break;

            default:
            {
                DD_ASSERT_REASON("Only Data and TimeDelta tokens have payload.");
            } break;
            }
        } break;
        default:
        {
            DD_ASSERT_REASON("Invalid internal parsing state.");
        } break;
        }
    }

    DD_ASSERT(parserState != DD_EVENT_PARSER_STATE_UNKNOWN);
    return parserState;
}

// =======================================================================================
void EventParser::SetParsingBuffer(const void* pBuffer, size_t size)
{
    m_pBuffer = (const uint8_t*)pBuffer;
    m_bufferSize = size;
    m_cursor = 0;
}

// =======================================================================================
DDEventParserEventInfo EventParser::GetEventInfo()
{
    const EventDataToken* pDataToken =
        (const EventDataToken*)(m_tokenBuffer + sizeof(EventTokenHeader));

    DDEventParserEventInfo info = {};
    info.providerId = m_currProviderId;
    info.eventId = pDataToken->id;
    info.eventIndex = pDataToken->index;
    info.totalPayloadSize = pDataToken->size;
    info.timestampFrequency = m_currTimestampFrequency;

    const EventTokenHeader* pHeader = (const EventTokenHeader*)(m_tokenBuffer);
    info.timestamp = m_currTimestamp + pHeader->delta * DevDriver::kEventTimeUnit;

    return info;
}

// =======================================================================================
DDEventParserDataPayload EventParser::GetPayload()
{
    return m_currDataPayload;
}

} // namespace Event
