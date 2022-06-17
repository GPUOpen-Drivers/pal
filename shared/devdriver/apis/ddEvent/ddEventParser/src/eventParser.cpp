/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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

using namespace DevDriver;

namespace Event
{

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT EventParser::Parse(
    const void* pData,
    size_t      dataSize)
{
    const Result result = m_parser.Parse(pData, dataSize);

    return DevDriverToDDResult(result);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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

} // namespace Event
