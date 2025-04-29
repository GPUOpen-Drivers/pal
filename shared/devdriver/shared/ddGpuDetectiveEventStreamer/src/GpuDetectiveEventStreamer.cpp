/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2022-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "GpuDetectiveEventStreamer.h"
#include <ddEventParser.h>
#include <ddEventClient.h>
#include <util/ddEventTimer.h>
#include <dd_event/gpu_detective/kernel_crash_analysis.h>
#include <dd_event/gpu_detective/umd_crash_analysis.h>

namespace
{

constexpr char kDevDriverEventChunkId[RDF_IDENTIFIER_SIZE] = "DDEvent";

constexpr uint8_t KrnlCrashMarkerId = static_cast<uint8_t>(KernelCrashAnalysisEvents::EventId::PageFault);
constexpr uint8_t UmdCrashMarkerId  = static_cast<uint8_t>(UmdCrashAnalysisEvents::EventId::CrashDebugMarkerValue);

DD_STATIC_CONST uint8_t kGDSEventClientNumRetries = 10;

} // anonymous namespace

DD_RESULT RdfResultToDDResult(int rResult)
{
    DD_RESULT result;

    switch (rResult)
    {
        case rdfResult::rdfResultOk:
            result = DD_RESULT_SUCCESS;
            break;
        case rdfResult::rdfResultInvalidArgument:
            result = DD_RESULT_COMMON_INVALID_PARAMETER;
            break;
        case rdfResult::rdfResultError:
        default:
            // The default case is being included here, since more error codes may be added to rdf in the future.
            result = DD_RESULT_UNKNOWN;
            break;
    }

    return result;
}

// =================================================================================================
GPUDetectiveStreamer::GPUDetectiveStreamer(const LoggerUtil& logger)
    : m_hEventParser(DD_API_INVALID_HANDLE)
    , m_hEventClient(DD_API_INVALID_HANDLE)
    , m_isStreaming(false)
    , m_exitRequested(false)
    , m_errorOccurred(false)
    , m_logger(logger)
    , m_pStreamFile(nullptr)
    , m_totalDataSize(0)
    , m_foundFirstEvent(false)
    , m_crashEventOccured(false)
{

}

// =================================================================================================
GPUDetectiveStreamer::~GPUDetectiveStreamer()
{
    EndStreaming(false);
}

// =================================================================================================
DD_RESULT GPUDetectiveStreamer::Init(
    DDClientId      clientId,
    DDNetConnection hConnection,
    uint32_t        providerId)
{
    DDEventClientCreateInfo clientInfo = {};
    clientInfo.hConnection             = hConnection;
    clientInfo.clientId                = clientId;
    clientInfo.providerId              = providerId;
    clientInfo.dataCb.pUserdata        = this;
    clientInfo.dataCb.pfnCallback      = [](void* pUserdata, const void* pData, size_t dataSize) {
        reinterpret_cast<GPUDetectiveStreamer*>(pUserdata)->OnEventData(pData, dataSize);
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
            uint8_t numRetries = kGDSEventClientNumRetries;

            do
            {
                LogError(
                    "Failed to begin event stream: %s, Retrying...",
                    ddApiResultToString(result));

                --numRetries;
                result = ddEventClientCreate(&clientInfo, &m_hEventClient);
                LogErrorOnFailure(result != DD_RESULT_SUCCESS, "Retry failed: %s", ddApiResultToString(result));

            } while ((result != DD_RESULT_SUCCESS) && (numRetries > 0));
        }
    }

    if (m_pStreamFile != nullptr)
    {
        fclose(m_pStreamFile);
        m_pStreamFile   = nullptr;
        m_totalDataSize = 0;
    }

    if (result != DD_RESULT_SUCCESS)
    {
        ddEventClientDestroy(m_hEventClient);
        ddEventParserDestroy(m_hEventParser);
    }

    return result;
}

// =================================================================================================
DD_RESULT GPUDetectiveStreamer::BeginStreaming(
    DDClientId      clientId,
    DDNetConnection hConnection,
    uint32_t        providerId)
{
    m_providerId = providerId;

    DD_RESULT result = Init(clientId, hConnection, m_providerId);

    if (result == DD_RESULT_SUCCESS)
    {
        result = ddEventClientEnableProviders(m_hEventClient, 1, &m_providerId);
        LogErrorOnFailure(
            result == DD_RESULT_SUCCESS,
            "Failed to enable event provider, clientId: %u, providerId: %u", clientId, m_providerId);
    }

    if (result == DD_RESULT_SUCCESS)
    {
        // Start event pulling thread.
        m_exitRequested = false;
        result = DevDriverToDDResult(m_eventThread.Start(EventPullingThreadFn, this));
        LogErrorOnFailure(
            result == DD_RESULT_SUCCESS,
            "Failed to start event pull thread, clientId: %u, providerId: %u", clientId, m_providerId);
    }

    if (result == DD_RESULT_SUCCESS)
    {
        m_pStreamFile = tmpfile();
        if (m_pStreamFile == nullptr)
        {
            result = DD_RESULT_DD_GENERIC_FILE_IO_ERROR;
        }
        LogErrorOnFailure(
            result == DD_RESULT_SUCCESS,
            "Fail to open temp file to stream Execution Marker events.");
    }

    if (result == DD_RESULT_SUCCESS)
    {
        m_isStreaming     = true;
        m_foundFirstEvent = false;
    }

    return result;
}

// =================================================================================================
DD_RESULT GPUDetectiveStreamer::EndStreaming(bool isClientAlive)
{
    DD_RESULT result = DD_RESULT_SUCCESS;

    if (m_isStreaming)
    {
        // Shut down the streaming thread
        if (m_eventThread.IsJoinable())
        {
            m_exitRequested = true;
            m_eventThread.Join(1000);
        }

        if (isClientAlive)
        {
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

        ddEventParserDestroy(m_hEventParser);
        ddEventClientDestroy(m_hEventClient);

        m_isStreaming = false;
        m_foundFirstEvent = false;
    }
    else
    {
        m_foundFirstEvent = false;
    }

    m_streamMutex.Lock();

    if (m_pStreamFile && (ferror(m_pStreamFile) == 0))
    {
        m_totalDataSize = ftell(m_pStreamFile);
    }
    else
    {
        m_totalDataSize = 0;
    }
    m_streamMutex.Unlock();

    return result;
}

// =================================================================================================
DD_RESULT GPUDetectiveStreamer::TransferDataStream(
    const DDIOHeartbeat& ioHeartbeat,
    rdfChunkFileWriter*  pRdfChunkWriter,
    bool                 useCompression)
{
    DD_ASSERT(pRdfChunkWriter != nullptr);
    DD_ASSERT(m_pStreamFile != nullptr);

    int currentChunkIndex = 0;

    m_streamMutex.Lock();

    DD_RESULT result = DD_RESULT_SUCCESS;

    size_t kTransferBufferSize  = 4 * 1024 * 1024;
    uint8_t* pTransferBuffer    = (uint8_t*)malloc(kTransferBufferSize);

    if (pTransferBuffer != nullptr)
    {
        if (m_totalDataSize > 0)
        {
            const uint32_t currPosition = ftell(m_pStreamFile);
            rewind(m_pStreamFile);
            size_t bytesRemaining       = (size_t)m_totalDataSize;

            rdfChunkCreateInfo chunkInfo = {};
            memcpy(chunkInfo.identifier, kDevDriverEventChunkId, RDF_IDENTIFIER_SIZE);
            chunkInfo.headerSize  = sizeof(DDEventProviderHeader);
            chunkInfo.pHeader     = &m_rdfChunkHeader;
            chunkInfo.compression = useCompression ? rdfCompressionZstd : rdfCompressionNone;
            chunkInfo.version     = 1;

            result = RdfResultToDDResult(rdfChunkFileWriterBeginChunk(pRdfChunkWriter, &chunkInfo));

            while ((result == DD_RESULT_SUCCESS) && (bytesRemaining > 0))
            {
                const size_t transferSize = kTransferBufferSize < bytesRemaining ? kTransferBufferSize : bytesRemaining;
                const size_t bytesRead    = fread(pTransferBuffer, 1, transferSize, m_pStreamFile);
                result = (bytesRead == transferSize) ? DD_RESULT_SUCCESS : DD_RESULT_DD_GENERIC_FILE_IO_ERROR;

                if (result == DD_RESULT_SUCCESS)
                {
                    result = RdfResultToDDResult(rdfChunkFileWriterAppendToChunk(
                        pRdfChunkWriter,
                        transferSize,
                        pTransferBuffer));
                }

                ioHeartbeat.pfnWriteHeartbeat(ioHeartbeat.pUserdata, result, DD_IO_STATUS_WRITE, bytesRead);

                bytesRemaining -= bytesRead;
            }

            if (result == DD_RESULT_SUCCESS)
            {
                result = RdfResultToDDResult(rdfChunkFileWriterEndChunk(pRdfChunkWriter, &currentChunkIndex));
            }

            // Now restore the file position we left off at
            fseek(m_pStreamFile, currPosition, SEEK_SET);

            free(pTransferBuffer);
        }
        else
        {
            LogInfo("Received no data from event provider: %u", m_providerId);
            result = DD_RESULT_SUCCESS;
        }
    }
    else
    {
        result = DD_RESULT_DD_GENERIC_INSUFFICIENT_MEMORY;
    }

    m_streamMutex.Unlock();

    return result;
}

// =================================================================================================
void GPUDetectiveStreamer::OnEventData(const void* pData, size_t dataSize)
{
    DD_RESULT              result                  = DD_RESULT_SUCCESS;
    DDEventParserEventInfo currEventInfo           = {};

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

                // Save the first event timestamp info inorder to populate the chunk header
                if (m_foundFirstEvent == false)
                {
                    DDEventMetaVersion metaVersion = { 0, 1 };
                    result = EventWritePayloadChunk(&currEventInfo, &metaVersion, sizeof(metaVersion));

                    if (result == DD_RESULT_SUCCESS)
                    {
                        m_rdfChunkHeader                        = {};
                        m_rdfChunkHeader.versionMajor           = 0;
                        m_rdfChunkHeader.versionMinor           = 1;
                        m_rdfChunkHeader.providerId             = currEventInfo.providerId;
                        m_rdfChunkHeader.timeUnit               = DevDriver::kEventTimeUnit;
                        m_rdfChunkHeader.baseTimestamp          = currEventInfo.timestamp;
                        m_rdfChunkHeader.baseTimestampFrequency = currEventInfo.timestampFrequency;
                        DD_ASSERT(currEventInfo.providerId == m_providerId);

                        result = EventWritePayloadChunk(&currEventInfo, &m_rdfChunkHeader, sizeof(m_rdfChunkHeader));
                    }
                    m_foundFirstEvent = true;
                }

                DD_ASSERT(currEventInfo.eventId <= UINT8_MAX);
                DD_ASSERT(currEventInfo.totalPayloadSize <= UINT16_MAX);

                switch (currEventInfo.providerId)
                {
                    case KernelCrashAnalysisEvents::ProviderId:
                        if (currEventInfo.eventId == KrnlCrashMarkerId)
                        {
                            m_crashEventOccured = true;
                        }
                        break;
                    case UmdCrashAnalysisEvents::ProviderId:
                        if (currEventInfo.eventId == UmdCrashMarkerId)
                        {
                            m_crashEventOccured = true;
                        }
                        break;
                }

                DDEventHeader header = {
                    header.eventId = static_cast<uint8_t>(currEventInfo.eventId),
                    header.smallDelta = 0,
                    header.eventSize = static_cast<uint16_t>(currEventInfo.totalPayloadSize)
                };

                uint64_t timestampDelta = (currEventInfo.timestamp - m_rdfChunkHeader.baseTimestamp) / DevDriver::kEventTimeUnit;
                if (timestampDelta <= UINT8_MAX)
                {
                    header.smallDelta = (uint8_t)timestampDelta;
                }
                else
                {
                    // First write a DDTimestampLargeDelta event if timestamp
                    // delta doesn't fit smallDelta.
                    {
                        DDEventHeader largeDeltaHeader = {
                            largeDeltaHeader.eventId = DDCommonEventId::TimestampLargeDelta,
                            largeDeltaHeader.smallDelta = 0,
                            largeDeltaHeader.eventSize =
                                static_cast<uint16_t>(sizeof(DDCommonEvents::TimestampLargeDelta))
                        };

                        result = EventWritePayloadChunk(&currEventInfo, &largeDeltaHeader, sizeof(header));

                        if (result == DD_RESULT_SUCCESS)
                        {
                            DDCommonEvents::TimestampLargeDelta largeDelta = { timestampDelta };
                            result = EventWritePayloadChunk(&currEventInfo, &largeDelta, sizeof(largeDelta));
                        }
                    }

                    header.smallDelta = 0;
                }

                result = EventWritePayloadChunk(&currEventInfo, &header, sizeof(DDEventHeader));

                break;
            }
            case DD_EVENT_PARSER_STATE_PAYLOAD_RECEIVED:
            {
                DDEventParserDataPayload payload = ddEventParserGetDataPayload(m_hEventParser);
                DD_ASSERT(payload.size <= SIZE_MAX);

                result = EventWritePayloadChunk(&currEventInfo, payload.pData, static_cast<size_t>(payload.size));

                break;
            }
            case DD_EVENT_PARSER_STATE_NEED_MORE_DATA:
            {
                stop = true;
                break;
            }
            case DD_EVENT_PARSER_STATE_UNKNOWN:
            {
                DD_ASSERT_ALWAYS();
                break;
            }
        }

        if (result != DD_RESULT_SUCCESS)
        {
            break;
        }
    }

    if (result != DD_RESULT_SUCCESS)
    {
        DD_WARN_REASON("Encountered errors during event token parsing!");

        m_errorOccurred = true;
    }
}

// =================================================================================================
void GPUDetectiveStreamer::EventPullingThreadFn(void* pUserdata)
{
    GPUDetectiveStreamer* pStreamer = static_cast<GPUDetectiveStreamer*>(pUserdata);

    while ((pStreamer->m_exitRequested == false) && (pStreamer->m_errorOccurred == false))
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
            pStreamer->LogErrorOnFailure(
                true,
                "Encountered error while streaming event data! (%s)",
                ddApiResultToString(result));

            pStreamer->m_errorOccurred = true;
            break;
        }
    }
}

// =================================================================================================
DD_RESULT GPUDetectiveStreamer::EventWritePayloadChunk(
    const DDEventParserEventInfo* pEvent,
    const void*                   pData,
    size_t                        dataSize)
{
    DD_RESULT result = DD_RESULT_SUCCESS;
    DD_ASSERT(m_pStreamFile != nullptr);
    DD_UNUSED(pEvent);

    m_streamMutex.Lock();
    const size_t bytesWritten = fwrite(pData, 1, dataSize, m_pStreamFile);
    m_streamMutex.Unlock();

    result = (bytesWritten == dataSize) ? DD_RESULT_SUCCESS : DD_RESULT_DD_GENERIC_FILE_IO_ERROR;

    return result;
}

// =================================================================================================
void GPUDetectiveStreamer::LogErrorOnFailure(bool condition, const char* pFmt, ...)
{
    if (!condition)
    {
        va_list args;
        va_start(args, pFmt);
        m_logger.Vprintf(DD_MAKE_LOG_EVENT(DD_LOG_LEVEL_ERROR, "GPUDetectiveStreamer"), pFmt, args);
        va_end(args);
    }
}

// =================================================================================================
void GPUDetectiveStreamer::LogInfo(const char* pFmt, ...)
{
    va_list args;
    va_start(args, pFmt);
    m_logger.Vprintf(DD_MAKE_LOG_EVENT(DD_LOG_LEVEL_INFO, "GPUDetectiveStreamer"), pFmt, args);
    va_end(args);
}

// =================================================================================================
void GPUDetectiveStreamer::LogError(const char* pFmt, ...)
{
    va_list args;
    va_start(args, pFmt);
    m_logger.Vprintf(DD_MAKE_LOG_EVENT(DD_LOG_LEVEL_ERROR, "GPUDetectiveStreamer"), pFmt, args);
    va_end(args);
}

// =================================================================================================
bool GPUDetectiveStreamer::HasCrashOccured()
{
    return m_crashEventOccured;
}

// =================================================================================================
void GPUDetectiveStreamer::ResetCrashBoolean()
{
    m_crashEventOccured = false;
}
