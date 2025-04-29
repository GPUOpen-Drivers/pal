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

#pragma once

#include <ddEventParser.h>
#include <legacy/legacyEventParser.h>

namespace Event
{

class EventParser
{
private:
    enum ParserInternalState
    {
        ParsingHeader,
        ParsingToken,
        ParsingPayload,
    };

public:
    EventParser(const DDEventWriter& writer);
    EventParser();

    DD_RESULT Parse(
        const void* pData,
        size_t      dataSize);

    /// Parse through a data buffer, return the state of the parser. Callers
    /// are expected to call this function repeatedly and make action based on
    /// the return parser state.
    ///
    /// DD_EVENT_PARSER_STATE_EVENT_RECEIVED:
    ///     Caller needs to call `GetEventInfo()`.
    ///
    /// DD_EVENT_PARSER_STATE_PAYLOAD_RECEIVED:
    ///     Caller needs to call `GetPayload()`.
    ///
    /// DD_EVENT_PARSER_STATE_NEED_MORE_DATA:
    ///     Caller needs to obtain a new buffer and call `SetParsingBuffer()`
    DD_EVENT_PARSER_STATE Parse();

    void SetParsingBuffer(const void* pBuffer, size_t size);
    DDEventParserEventInfo GetEventInfo();
    DDEventParserDataPayload GetPayload();

private:
    DevDriver::Result EventReceived(const DevDriver::EventProtocol::EventReceivedInfo& eventInfo);
    DevDriver::Result PayloadData(const void* pData, size_t dataSize);

    // Write enough bytes (up to `tokenItemSize`) from `m_pBuffer` to
    // `m_tokenBuffer`.
    bool CopyToTokenBuffer(size_t tokenItemSize);

    void ResetTokenBuffer();

    // Deprecated.
    DevDriver::EventProtocol::EventParser m_parser;

    DDEventWriter          m_writer;
    DDEventParserEventInfo m_eventInfo;
    bool                   m_isReadingPayload;
    uint64_t               m_payloadBytesRemaining;

    // Buffer to be parsed.
    const uint8_t* m_pBuffer = nullptr;
    // The size of `m_pBuffer`.
    size_t m_bufferSize = 0;
    // The parsing starting index into `m_pBuffer`.
    size_t m_cursor = 0;

    // For every new event to be parsed, `m_tokenDataBuffer` serves as a
    // temporay buffer to store, in order, `EventTokenHeader` and differnt
    // types of `Event*Token`s, and `EventTimeDeltaToken`'s payload.
    uint8_t m_tokenBuffer[DevDriver::EventProtocol::kMaxEventTokenSize];
    // The amount of data in bytes that have copied into `m_tokenBuffer`.
    size_t m_tokenDataSize = 0;
    // Starting byte of either `EventTokenHeader` or other `Event*Token`s, or
    // TimeDelta payload, in `m_tokenBuffer`.
    size_t m_tokenItemStart = 0;

    ParserInternalState m_parsingState = ParserInternalState::ParsingHeader;

    DevDriver::EventProtocol::EventProviderId m_currProviderId = 0;
    uint64_t m_currTimestampFrequency = 0;
    uint64_t m_currTimestamp = 0;

    // Currently parsed data payload. Intended for the caller of `Parse()` to
    // use to copy data away.
    DDEventParserDataPayload m_currDataPayload = {};
    // Number of bytes the caller of `Parse()` has already read for a
    // `EventDataToken`.
    size_t m_dataPayloadReadInBytes = 0;
};

} // namespace Event
