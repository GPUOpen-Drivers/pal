/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2022-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include <RmtEventStreamer.h>
#include <RmtEventTracer.h>
#include <ddEventClient.h>
#include <ddEventParser.h>
#include <stdint.h>

using namespace DevDriver;

DD_STATIC_CONST uint32 kUmdRmtTokenId         = 13;
DD_STATIC_CONST uint32 kUmdRmtVersionId       = 14;

DD_STATIC_CONST uint8_t kEventClientNumRetries = 10;

// =================================================================================================
RmtEventStreamer::RmtEventStreamer(RmtEventTracer* pTracer, const LoggerUtil& logger)
    : m_hEventClient(DD_API_INVALID_HANDLE)
    , m_hEventParser(DD_API_INVALID_HANDLE)
    , m_dataStreamId(0)
    , m_providerId(0)
    , m_expectedEventDataIndex(0)
    , m_expectedEventDataIndexValid(false)
    , m_exitRequested(false)
    , m_isStreaming(false)
    , m_encounteredErrors(false)
    , m_rmtVersionScratchOffset(0)
    , m_pRmtTracer(pTracer)
    , m_logger(logger)
{
}

// =================================================================================================
RmtEventStreamer::~RmtEventStreamer()
{
    // A streamer should never be destroyed while it's still in the process of streaming event data.
    if (IsStreaming())
    {
        m_pRmtTracer->LogError("Even stream is being destroyed, but was still streaming.");
    }
}

// =================================================================================================
DD_RESULT RmtEventStreamer::Init(
    DDClientId clientId,
    DDNetConnection hConnection,
    uint32_t providerId)
{
    DDEventClientCreateInfo clientInfo = {};
    clientInfo.hConnection        = hConnection;
    clientInfo.clientId           = clientId;
    clientInfo.providerId         = providerId;
    clientInfo.dataCb.pUserdata   = this;
    clientInfo.dataCb.pfnCallback = [](void* pUserdata, const void* pData, size_t dataSize) {
        reinterpret_cast<RmtEventStreamer*>(pUserdata)->OnEventData(pData, dataSize);
    };

    DD_RESULT result = ddEventParserCreateEx(&m_hEventParser);

    if (result == DD_RESULT_SUCCESS)
    {
        result = ddEventClientCreate(&clientInfo, &m_hEventClient);

        if (result != DD_RESULT_SUCCESS)
        {
            // WA: The event server currently only supports one reader at a time. This causes issues for DX
            // applications which launch multiple clients during startup. The clients all try to connect to
            // the single global kernel client, and sometimes the later clients will fail to start RMV tracing
            // because the earlier clients haven't fully disconnected yet.
            //
            // We work around this problem here by simply "retrying" after a failed connection attempt. This gives
            // the earlier clients about an extra second to disconnect and is enough to fix the timing problem
            // in all the cases we tested.
            //
            // Some applications (like RDR2) still don't connect after one retry, so we do several.
            // 10 is likely more than is needed, but it will ensure that RMV connects.
            //
            // TODO: This code should be removed once we implement proper multi-client support in our event server
            uint8_t numRetries = kEventClientNumRetries;

            do
            {
                m_pRmtTracer->LogError(
                    "Failed to begin event stream: %s, Retrying...",
                    ddApiResultToString(result));

                --numRetries;
                result = ddEventClientCreate(&clientInfo, &m_hEventClient);
                if (result != DD_RESULT_SUCCESS)
                {
                    m_pRmtTracer->LogError("Retry failed: %s", ddApiResultToString(result));
                }
            } while ((result != DD_RESULT_SUCCESS) && (numRetries > 0));
        }
    }

    if (result != DD_RESULT_SUCCESS)
    {
        ddEventClientDestroy(m_hEventClient);
        ddEventParserDestroy(m_hEventParser);
    }

    return result;
}

// =================================================================================================
DD_RESULT RmtEventStreamer::BeginStreaming(
    DDClientId      clientId,
    DDNetConnection hConnection,
    uint32_t        dataStreamId,
    uint32_t        providerId)
{
    m_providerId = providerId;
    m_dataStreamId = dataStreamId;

    DD_RESULT result = DD_RESULT_COMMON_UNKNOWN;

    // Attempt to enable the desired provider

    result = Init(clientId, hConnection, m_providerId);
    if (result == DD_RESULT_SUCCESS)
    {
        result = ddEventClientEnableProviders(m_hEventClient, 1, &providerId);
        if (result == DD_RESULT_SUCCESS)
        {
            // Start the thread that will pull event data
            m_exitRequested = false;
            result = DevDriverToDDResult(m_eventThread.Start(EventReceiveThreadFunc, this));

            if (result == DD_RESULT_SUCCESS)
            {
                // We've successfully started the streaming process
                m_isStreaming = true;
            }
            else
            {
                // We failed to start our thread, but we did remotely enable the event provider.
                // We need to attempt to turn off the remote event provider before returning a failure.
                ddEventClientDisableProviders(m_hEventClient, 1, &providerId);
            }
        }
        else
        {
            m_pRmtTracer->LogError(
                "ddEventClientEnableProviders failed with error: %s.",
                ddApiResultToString(result));
        }
    }
    else
    {
        m_pRmtTracer->LogError(
            "[RmtEventStreamer::BeginStreaming] Init failed with error: %s.",
            ddApiResultToString(result));
    }

    return result;
}

// =================================================================================================
DD_RESULT RmtEventStreamer::EndStreaming(bool isClientAlive)
{
    DD_RESULT result = DD_RESULT_DD_GENERIC_UNAVAILABLE;

    if (m_isStreaming)
    {
        result = DD_RESULT_SUCCESS;

        // Shut down the streaming thread
        if (m_eventThread.IsJoinable())
        {
            m_exitRequested = true;
            m_eventThread.Join(1000);
        }

        // If we still expect the client to be alive, then we should attempt to undo our provider configuration changes
        // and read any remaining event data from the client.
        if (isClientAlive && (result == DD_RESULT_SUCCESS))
        {
            // Disable the event provider
            result = ddEventClientDisableProviders(m_hEventClient, 1, &m_providerId);

            // Pull any remaining data
            while (result == DD_RESULT_SUCCESS)
            {
                result = ddEventClientReadEventData(m_hEventClient, 0);
            }

            // NotReady indicates we've successfully drained the event pipe
            if (result == DD_RESULT_DD_GENERIC_NOT_READY)
            {
                result = DD_RESULT_SUCCESS;
            }
        }

        if (result == DD_RESULT_SUCCESS)
        {
            ddEventParserDestroy(m_hEventParser);
            ddEventClientDestroy(m_hEventClient);

            m_hEventParser = DD_API_INVALID_HANDLE;
            m_hEventClient = DD_API_INVALID_HANDLE;

            m_isStreaming = false;
        }
    }

    return result;
}

// =================================================================================================
void RmtEventStreamer::OnEventData(const void* pData, size_t dataSize)
{
    // LogInfo("Received %llu data from provider (0x%x)", dataSize, m_providerId);

    DD_RESULT result = DD_RESULT_SUCCESS;
    DDEventParserEventInfo currEventInfo = {};
    uint64_t currPayloadReceivedSize = 0;

    ddEventParserSetBuffer(m_hEventParser, pData, dataSize);

    bool stop = false;
    while (stop == false)
    {
        DD_EVENT_PARSER_STATE parserState = ddEventParserParseNext(m_hEventParser);
        switch (parserState)
        {
            case DD_EVENT_PARSER_STATE_EVENT_RECEIVED:
            {
                currEventInfo = ddEventParserGetEventInfo(m_hEventParser);
                result = EventBegin(&currEventInfo, currEventInfo.totalPayloadSize);
            } break;

            case DD_EVENT_PARSER_STATE_PAYLOAD_RECEIVED:
            {
                DDEventParserDataPayload payload = ddEventParserGetDataPayload(m_hEventParser);
                DD_ASSERT(payload.size <= SIZE_MAX);
                result = EventWritePayloadChunk(&currEventInfo, payload.pData, static_cast<size_t>(payload.size));

                currPayloadReceivedSize += payload.size;
                if (currPayloadReceivedSize >= currEventInfo.totalPayloadSize)
                {
                    result = EventEnd(&currEventInfo, result);
                }
            } break;

            case DD_EVENT_PARSER_STATE_NEED_MORE_DATA:
            {
                stop = true;
            } break;

            case DD_EVENT_PARSER_STATE_UNKNOWN:
            {
                DD_ASSERT_ALWAYS();
            } break;
        }

        if (result != DD_RESULT_SUCCESS)
        {
            break;
        }
    }

    if (result != DD_RESULT_SUCCESS)
    {
        DD_WARN_REASON("Encountered errors during event token parsing!");

        m_encounteredErrors = true;
    }
}

// =================================================================================================
DD_RESULT RmtEventStreamer::EventBegin(const DDEventParserEventInfo* pEvent, uint64_t totalPayloadSize)
{
    DD_RESULT result = DD_RESULT_SUCCESS;

    // Validate the event indexing
    if (m_expectedEventDataIndexValid)
    {
        if (m_expectedEventDataIndex == pEvent->eventIndex)
        {
            ++m_expectedEventDataIndex;
        }
        else
        {
            m_pRmtTracer->LogError(
                "Unexpected data token index in stream %u! Got %u but expected %u.",
                m_dataStreamId,
                pEvent->eventIndex,
                m_expectedEventDataIndex);

            result = DD_RESULT_PARSING_INVALID_BYTES;
        }
    }
    else
    {
        m_expectedEventDataIndex = (pEvent->eventIndex + 1);
        m_expectedEventDataIndexValid = true;
    }

    if ((result == DD_RESULT_SUCCESS)          &&
        (pEvent->providerId == RmtEventTracer::kUmdProviderId) &&
        (pEvent->eventId == kUmdRmtVersionId))
    {
        // Verify that the payload size is what we expect if this is an RMT version event
        if (totalPayloadSize != sizeof(m_rmtVersionScratch))
        {
            result = DD_RESULT_COMMON_VERSION_MISMATCH;
        }
    }

    return result;
}

// =================================================================================================
DD_RESULT RmtEventStreamer::EventWritePayloadChunk(
    const DDEventParserEventInfo* pEvent,
    const void* pData,
    size_t dataSize)
{
    DD_RESULT result = DD_RESULT_COMMON_INVALID_PARAMETER;

    if (pEvent->providerId == RmtEventTracer::kUmdProviderId)
    {
        if (pEvent->eventId == kUmdRmtVersionId)
        {
            // Write RMT version event data into our scratch buffer and trigger an error if it doesn't fit
            if ((m_rmtVersionScratchOffset + dataSize) <= sizeof(m_rmtVersionScratch))
            {
                memcpy(VoidPtrInc(m_rmtVersionScratch, m_rmtVersionScratchOffset), pData, dataSize);
                m_rmtVersionScratchOffset += dataSize;

                result = DD_RESULT_SUCCESS;
            }
            else
            {
                result = DD_RESULT_COMMON_VERSION_MISMATCH;
            }
        }
        else if (pEvent->eventId == kUmdRmtTokenId)
        {
            // Write RMT token event data directly into the data stream
            result = m_pRmtTracer->WriteDataStream(
                m_dataStreamId,
                pData,
                dataSize);
        }
        else
        {
            // We don't expect any events other than RmtVersion and RmtToken,
            // if we see a different ID then we likely are talking to a driver
            // with a different provider version than we can handle.
            result = DD_RESULT_COMMON_VERSION_MISMATCH;
        }
    }
    else
    {
        // Normal event data can simply be written directly into the data stream
        result = m_pRmtTracer->WriteDataStream(
            m_dataStreamId,
            pData,
            dataSize);
    }

    return result;
}

// =================================================================================================
DD_RESULT RmtEventStreamer::EventEnd(const DDEventParserEventInfo* pEvent, DD_RESULT finalResult)
{
    if ((finalResult == DD_RESULT_SUCCESS)     &&
        (pEvent->providerId == RmtEventTracer::kUmdProviderId) &&
        (pEvent->eventId == kUmdRmtVersionId))
    {
        // Once we finish collecting all of the data for an RMTVersion event, we can safely parse it and take
        // action based off of its contents.
        if (m_rmtVersionScratchOffset == sizeof(m_rmtVersionScratch))
        {
            // Copy the event data onto the stack to ensure proper alignment
            RmtVersionEvent versionEvent = {};
            memcpy(&versionEvent, m_rmtVersionScratch, sizeof(versionEvent));

            // Reset the scratch offset
            m_rmtVersionScratchOffset = 0;

            // Write the version information from the event into the data context
            finalResult = m_pRmtTracer->WriteRmtVersion(
                m_dataStreamId,
                versionEvent.majorVersion,
                versionEvent.minorVersion);
        }
        else
        {
            // We didn't gather the right amount of data for some reason. Assume a version mismatch.
            finalResult = DD_RESULT_PARSING_INVALID_BYTES;
        }
    }

    return finalResult;
}

// =================================================================================================
void RmtEventStreamer::LogError(const char* pFmt, ...)
{
    va_list args;
    va_start(args, pFmt);
    m_logger.Vprintf(DD_MAKE_LOG_EVENT(DD_LOG_LEVEL_ERROR, "RmtEventStreamer"), pFmt, args);
    va_end(args);
}

// =================================================================================================
void RmtEventStreamer::LogInfo(const char* pFmt, ...)
{
    va_list args;
    va_start(args, pFmt);
    m_logger.Vprintf(DD_MAKE_LOG_EVENT(DD_LOG_LEVEL_INFO, "RmtEventStreamer"), pFmt, args);
    va_end(args);
}

// =================================================================================================
void RmtEventStreamer::EventReceiveThreadFunc(void* pUserdata)
{
    RmtEventStreamer* pStreamer = static_cast<RmtEventStreamer*>(pUserdata);

    while ((pStreamer->m_exitRequested == false) && (pStreamer->m_encounteredErrors == false))
    {
        // Attempt to read some event data
        const DD_RESULT result = ddEventClientReadEventData(pStreamer->m_hEventClient, 100);

        if ((result == DD_RESULT_SUCCESS) || (result == DD_RESULT_DD_GENERIC_NOT_READY))
        {
            // We've either read event data successfully, or we've timed out.
            // Both of the situations are expected and we don't need to do anything special here.
        }
        else if (result == DD_RESULT_DD_GENERIC_END_OF_STREAM)
        {
            // The client disconnected. Break out of the read loop since we won't be receiving any more messages.
            break;
        }
        else
        {
            // We've encountered some sort of error so we should exit the loop to avoid further issues.
            pStreamer->LogError(
                "Encountered error while streaming event data! (%s)",
                ddApiResultToString(result));

            pStreamer->m_encounteredErrors = true;
            break;
        }
    }
}
