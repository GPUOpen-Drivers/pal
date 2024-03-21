/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#if PAL_BUILD_RDF

#include "palTraceSession.h"
#include "palHashMapImpl.h"
#include "core/imported/rdf/rdf/inc/amdrdf.h"
#include "uberTraceService.h"
#include "util/ddStructuredReader.h"
#include "util/ddJsonWriter.h"

using namespace Pal;
using DevDriver::StructuredValue;

namespace GpuUtil
{
static_assert(TextIdentifierSize == RDF_IDENTIFIER_SIZE,
              "The text identifer size of the trace chunk must match that of the rdf chunk!");

// =====================================================================================================================
// Translates a rdfResult to a Pal::Result
static Result RdfResultToPalResult(
    int rdfResult)
{
    Result result;

    switch (rdfResult)
    {
    case rdfResult::rdfResultOk:
        result = Result::Success;
        break;
    case rdfResult::rdfResultInvalidArgument:
        result = Result::ErrorInvalidValue;
        break;
    case rdfResult::rdfResultError:
    default:
        // The default case is being included here, since more error codes may be added to rdf in the future.
        result = Result::ErrorUnknown;
        break;
    }

    return result;
}

// =====================================================================================================================
TraceSession::TraceSession(
    IPlatform* pPlatform)
    :
    m_pPlatform(pPlatform),
    m_pReader(nullptr),
    m_registerTraceSourceLock(),
    m_registerTraceControllerLock(),
    m_chunkAppendLock(),
    m_registeredTraceSources(64, pPlatform),
    m_traceSourcesConfigs(64, pPlatform),
    m_registeredTraceControllers(64, pPlatform),
    m_pActiveController(nullptr),
    m_sessionState(TraceSessionState::Ready),
    m_pChunkFileWriter(nullptr),
    m_pCurrentStream(nullptr),
    m_currentChunkIndex(0),
    m_tracingEnabled(false),
    m_pConfigData(nullptr),
    m_configDataSize(0)
{
}

// =====================================================================================================================
TraceSession::~TraceSession()
{
    PAL_ASSERT(TraceSessionState::Ready == GetTraceSessionState());

    // Free config memory of each TraceSource
    if (m_traceSourcesConfigs.GetNumEntries() > 0)
    {
        for (auto iter = m_traceSourcesConfigs.Begin(); iter.Get() != nullptr; iter.Next())
        {
            if (iter.Get()->value != nullptr)
            {
                PAL_DELETE(iter.Get()->value, m_pPlatform);
            }
        }
    }

    if (m_pReader != nullptr)
    {
        DevDriver::IStructuredReader::Destroy(&m_pReader);
    }

    if (m_pConfigData != nullptr)
    {
        PAL_SAFE_FREE(m_pConfigData, m_pPlatform);
    }
}

// =====================================================================================================================
Result TraceSession::Init()
{
    Result result = m_registeredTraceSources.Init();

    if (result == Result::Success)
    {
        result = m_traceSourcesConfigs.Init();
    }

    if (result == Result::Success)
    {
        result = m_registeredTraceControllers.Init();
    }

    return result;
}

// =====================================================================================================================
Result TraceSession::RegisterController(
    ITraceController* pController)
{
    Result result = (pController != nullptr) ? Result::Success : Result::ErrorInvalidPointer;

    if (result == Result::Success)
    {
        if (m_sessionState == TraceSessionState::Ready)
        {
            Util::RWLockAuto<Util::RWLock::ReadWrite> traceControllerLock(&m_registerTraceControllerLock);

            bool existed = false;
            ITraceController** ppMapEntry = nullptr;
            result = m_registeredTraceControllers.FindAllocate(pController->GetName(), &existed, &ppMapEntry);

            if (result == Result::Success)
            {
                if (existed)
                {
                    result = Result::AlreadyExists;
                }
                else
                {
                    (*ppMapEntry) = pController;
                }
            }
        }
        else
        {
            result = Result::ErrorUnavailable;
        }
    }

    return result;
}

// =====================================================================================================================
Result TraceSession::UnregisterController(
    ITraceController* pController)
{
    Result result = Result::Success;

    if (m_sessionState == TraceSessionState::Ready)
    {
        Util::RWLockAuto<Util::RWLock::ReadWrite> traceControllerLock(&m_registerTraceControllerLock);
        result = m_registeredTraceControllers.Erase(pController->GetName()) ? Result::Success : Result::NotFound;
    }
    else
    {
        result = Result::ErrorUnavailable;
    }

    return result;
}

// =====================================================================================================================
Result TraceSession::RegisterSource(
    ITraceSource* pSource)
{
    Result result = (pSource != nullptr) ? Result::Success : Result::ErrorInvalidPointer;

    if (result == Result::Success)
    {
        if (m_sessionState == TraceSessionState::Ready)
        {
            Util::RWLockAuto<Util::RWLock::ReadWrite> traceSourceLock(&m_registerTraceSourceLock);

            bool existed = false;
            ITraceSource** ppMapEntry;
            result = m_registeredTraceSources.FindAllocate(pSource->GetName(), &existed, &ppMapEntry);

            if(result == Result::Success)
            {
                if (existed)
                {
                    result = Result::AlreadyExists;
                }
                else
                {
                    *ppMapEntry = pSource;

                    // Update source configs if available
                    StructuredValue** ppSourceConfig = m_traceSourcesConfigs.FindKey(pSource->GetName());

                    if ((ppSourceConfig != nullptr) && (*ppSourceConfig != nullptr))
                    {
                        pSource->OnConfigUpdated(*ppSourceConfig);
                    }
                }
            }
        }
        else
        {
            result = Result::ErrorUnavailable;
        }
    }

    return result;
}

// =====================================================================================================================
Result TraceSession::UnregisterSource(
    ITraceSource* pSource)
{
    Result result = Result::Success;

    if (m_sessionState == TraceSessionState::Ready)
    {
        Util::RWLockAuto<Util::RWLock::ReadWrite> traceSourceLock(&m_registerTraceSourceLock);
        result = m_registeredTraceSources.Erase(pSource->GetName()) ? Result::Success : Result::NotFound;
    }
    else
    {
        result = Result::ErrorUnavailable;
    }

    return result;
}

// =====================================================================================================================
Result TraceSession::RequestTrace()
{
    Result result = Result::Success;

    if (m_sessionState == TraceSessionState::Ready)
    {
        m_sessionState = TraceSessionState::Requested;
    }
    else
    {
        result = Result::ErrorUnavailable;
    }

    return result;
}

// =====================================================================================================================
Result TraceSession::UpdateTraceConfig(
    const void* pData,
    size_t      dataSize)
{
    Result result = Result::ErrorUnknown;

    if (pData == nullptr)
    {
        result = Result::ErrorInvalidPointer;
    }
    else if (m_sessionState != TraceSessionState::Ready)
    {
        result = Result::ErrorUnavailable;
    }
    else
    {
        // Parse the JSON string into a structured reader
        DevDriver::Result devDriverResult
            = DevDriver::IStructuredReader::CreateFromJson(pData,
                                                           dataSize,
                                                           DevDriver::Platform::GenericAllocCb,
                                                           &m_pReader);

        result = (devDriverResult == DevDriver::Result::Success) ? Result::Success : Result::ErrorUnknown;

        // Cache the raw JSON config for later use
        if (result == Result::Success)
        {
            if (m_pConfigData != nullptr)
            {
                PAL_SAFE_FREE(m_pConfigData, m_pPlatform);
            }

            m_pConfigData = PAL_MALLOC(dataSize, m_pPlatform, Util::AllocInternal);

            if (m_pConfigData != nullptr)
            {
                m_configDataSize = dataSize;
                memcpy(m_pConfigData, pData, dataSize);
            }
            else
            {
                m_configDataSize = 0;
                result = Result::ErrorOutOfMemory;
            }
        }

        // Update configs of TraceControllers and TraceSources
        if (result == Result::Success)
        {
            const StructuredValue root = m_pReader->GetRoot();

            // Update configs of registered TraceControllers
            const StructuredValue traceControllers    = root["controllers"];
            const size_t          numTraceControllers = traceControllers.GetArrayLength();

            if (traceControllers.IsNull() == false)
            {
                for (uint32 idx = 0; idx < numTraceControllers; ++idx)
                {
                    // Grab the config for each controller
                    const StructuredValue traceController       = traceControllers[idx];
                    const char*           pName                 = traceController["name"].GetStringPtr();
                    StructuredValue       traceControllerConfig = traceController["config"];

                    if ((pName != nullptr) && (traceControllerConfig.IsNull() == false))
                    {
                        Util::RWLockAuto<Util::RWLock::ReadOnly> traceControllerLock(&m_registerTraceControllerLock);

                        ITraceController** ppController = m_registeredTraceControllers.FindKey(pName);
                        if (ppController != nullptr)
                        {
                            (*ppController)->OnConfigUpdated(&traceControllerConfig);
                        }
                    }
                }
            }

            // Configs of TraceSources will be updated later during registration. This is because clients might register
            // sources during DevDriver's LateDeviceInit and require that configs be updated at that time. In the future,
            // we might want to move LateDeviceInit from client's responsibility to PAL's.
            const StructuredValue traceSources    = root["sources"];
            const size_t          numTraceSources = traceSources.GetArrayLength();

            if (traceSources.IsNull() == false)
            {
                Util::RWLockAuto<Util::RWLock::ReadWrite> traceSourceLock(&m_registerTraceSourceLock);

                for (uint32 idx = 0; idx < numTraceSources; ++idx)
                {
                    // Grab the config for each source
                    const StructuredValue traceSource       = traceSources[idx];
                    const char*           pName             = traceSource["name"].GetStringPtr();
                    StructuredValue       traceSourceConfig = traceSource["config"];

                    if ((pName != nullptr) && (traceSourceConfig.IsNull() == false))
                    {
                        // Update source configs if available
                        ITraceSource** ppSource = m_registeredTraceSources.FindKey(pName);
                        if (ppSource != nullptr)
                        {
                            if (*ppSource != nullptr)
                            {
                                (*ppSource)->OnConfigUpdated(&traceSourceConfig);
                            }
                        }

                        // Store configs of TraceSources
                        bool              existed    = false;
                        StructuredValue** ppMapEntry = nullptr;
                        result = m_traceSourcesConfigs.FindAllocate(pName, &existed, &ppMapEntry);

                        if (result == Result::Success)
                        {
                            // Don't want to re-allocate memory if the entry already exists
                            if (existed == false)
                            {
                                // Ensure deallocations when TraceSession is destroyed
                                (*ppMapEntry) = PAL_NEW(StructuredValue, m_pPlatform, Util::AllocInternalTemp);
                            }

                            if ((*ppMapEntry) != nullptr)
                            {
                                (**ppMapEntry) = traceSourceConfig;
                            }
                        }
                    }
                }
            }
        }
        else
        {
            result  = Result::ErrorInvalidValue; // Invalid JSON parameter
        }
    }

    return result;
}

// =====================================================================================================================
Result TraceSession::AcceptTrace(
    ITraceController* pController,
    uint64            supportedGpuMask)
{
    PAL_ASSERT(TraceSessionState::Requested == GetTraceSessionState());

    Result result = Result::ErrorUnknown;

    if (pController == nullptr)
    {
        result = Result::ErrorInvalidPointer;
    }
    else
    {
        if (m_sessionState == TraceSessionState::Requested)
        {
            m_pActiveController = pController;

            // Create the stream to be used in the chunkFileWriter. This will also be used to retrieve
            // the final list of appended chunks in CollectTrace().
            result = RdfResultToPalResult(rdfStreamCreateMemoryStream(&m_pCurrentStream));

            // Create the chunkFileWriter to setup the chunk data structures and buffers to collect the incoming chunks
            if (result == Result::Success)
            {
                result = RdfResultToPalResult(rdfChunkFileWriterCreate(m_pCurrentStream, &m_pChunkFileWriter));
            }

            if (result == Result::Success)
            {
                Util::RWLockAuto<Util::RWLock::ReadOnly> traceSourceLock(&m_registerTraceSourceLock);

                // Grab the "sources" field from the trace config
                const StructuredValue traceSources = m_pReader->GetRoot()["sources"];

                // Notify all requested trace sources that the trace has been accepted
                for (uint32 i = 0; i < traceSources.GetArrayLength(); i++)
                {
                    const char* pName = traceSources[i]["name"].GetStringPtr();

                    if (pName != nullptr)
                    {
                        // Search for and notify the TraceSource object named in the trace configuration
                        ITraceSource** ppSource = m_registeredTraceSources.FindKey(pName);

                        if ((ppSource != nullptr) && (*ppSource != nullptr))
                        {
                            (*ppSource)->OnTraceAccepted();
                        }
                    }
                }
            }
        }
        else
        {
            result = Result::ErrorUnavailable;
        }
    }

    return result;
}

// =====================================================================================================================
Result TraceSession::BeginTrace()
{
    PAL_ASSERT(TraceSessionState::Preparing == GetTraceSessionState());

    // GPU Index has been hardcoded for now
    uint32      gpuIndex     = 0;
    ICmdBuffer* pBeginCmdBuf = nullptr;

    // Notify the active controller of any required GPU work
    Result result = m_pActiveController->OnBeginGpuWork(gpuIndex, &pBeginCmdBuf);

    if (result == Result::Success)
    {
        Util::RWLockAuto<Util::RWLock::ReadOnly> traceSourceLock(&m_registerTraceSourceLock);

        // Grab the "sources" field from the trace config
        const StructuredValue traceSources = m_pReader->GetRoot()["sources"];

        // Notify all requested trace sources that the trace has begun
        for (uint32 i = 0; i < traceSources.GetArrayLength(); i++)
        {
            const char* pName = traceSources[i]["name"].GetStringPtr();

            if (pName != nullptr)
            {
                // Search for and notify the TraceSource object named in the trace configuration
                ITraceSource** ppSource = m_registeredTraceSources.FindKey(pName);

                if ((ppSource != nullptr) && (*ppSource != nullptr))
                {
                    (*ppSource)->OnTraceBegin(gpuIndex, pBeginCmdBuf);
                }
            }
        }
    }

    return result;
}

// =====================================================================================================================
Result TraceSession::EndTrace()
{
    PAL_ASSERT(TraceSessionState::Running == GetTraceSessionState());

    // GPU Index has been hardcoded for now
    uint32 gpuIndex = 0;
    ICmdBuffer* pCmdBuf = nullptr;

    // Notify the active controller of any required GPU work
    Result result = m_pActiveController->OnEndGpuWork(gpuIndex, &pCmdBuf);

    if (result == Result::Success)
    {
        Util::RWLockAuto<Util::RWLock::ReadOnly> traceSourceLock(&m_registerTraceSourceLock);

        // Grab the "sources" field from the trace config
        const StructuredValue traceSources = m_pReader->GetRoot()["sources"];

        // Notify all requested trace sources that the trace has ended
        for (uint32 i = 0; i < traceSources.GetArrayLength(); i++)
        {
            const char* pName = traceSources[i]["name"].GetStringPtr();

            if (pName != nullptr)
            {
                // Search for and notify the TraceSource object named in the trace configuration
                ITraceSource** ppSource = m_registeredTraceSources.FindKey(pName);

                if ((ppSource != nullptr) && (*ppSource != nullptr))
                {
                    (*ppSource)->OnTraceEnd(gpuIndex, pCmdBuf);
                }
            }
        }
    }

    return result;
}

// =====================================================================================================================
Result TraceSession::CollectTrace(
    void*   pData,
    size_t* pDataSize)
{
    Result result = Result::ErrorUnknown;

    switch (m_sessionState)
    {
    case TraceSessionState::Completed:
    {
        if (pDataSize == nullptr)
        {
            result = Result::ErrorInvalidPointer;
        }
        else
        {
            int64 streamSize = static_cast<int64>(*pDataSize);

            // Check if the ChunkWriter hasn't already been closed ie. don't destroy it twice.
            if (m_pChunkFileWriter != nullptr)
            {
                // Destroying (i.e. closing) the ChunkWriter ensures that all data, both compressed and uncompressed,
                // is written to the data stream. This step also completes the RDF file by adding the final parts
                // (index entries) of the file. Trace data and data-sizes will be correctly output only after this step.
                result = RdfResultToPalResult(rdfChunkFileWriterDestroy(&m_pChunkFileWriter));

                // We need to move the RDF offset manually to the beginning of the data stream
                if (result == Result::Success)
                {
                    result = RdfResultToPalResult(rdfStreamSeek(m_pCurrentStream, 0));
                }

                if (result == Result::Success)
                {
                    result = RdfResultToPalResult(rdfStreamGetSize(m_pCurrentStream, &streamSize));
                }
            }

            // Read the trace data if pData is valid, else simply output the data-size.
            if (pData != nullptr)
            {
                if (*pDataSize < streamSize)
                {
                    result = Result::ErrorInvalidMemorySize;
                }
                else
                {
                    // Read all trace data in the current stream in RDF format
                    int64 bytesRead = 0;
                    result = RdfResultToPalResult(rdfStreamRead(m_pCurrentStream, streamSize, pData, &bytesRead));

                    if (result == Result::Success)
                    {
                        result = RdfResultToPalResult(rdfStreamClose(&m_pCurrentStream));
                    }

                    if (result == Result::Success)
                    {
                        m_sessionState = TraceSessionState::Ready;
                    }
                }
            }
            else
            {
                (*pDataSize) = streamSize;
            }
        }
        break;
    }
    case TraceSessionState::Requested:
    case TraceSessionState::Preparing:
    case TraceSessionState::Running:
    case TraceSessionState::Waiting:
    {
        // If the trace is currently in progress, return "NotReady" to inform callees
        // that the trace data is not ready to be collected
        result = Result::NotReady;
        break;
    }
    case TraceSessionState::Ready:
    {
        // If the session state is idle, trace results are "unavailable"
        result = Result::ErrorUnavailable;
        break;
    }
    }

    return result;
}

// =====================================================================================================================
// Writes/appends the data chunks in one long data block ie. the current data stream
Result TraceSession::WriteDataChunk(
    ITraceSource*         pSource,
    const TraceChunkInfo& info)
{
    // Populate rdfChunkCreateInfo parameters from TraceChunkInfo struct
    rdfChunkCreateInfo currentChunkInfo = {
        .headerSize  = info.headerSize,
        .pHeader     = info.pHeader,
        .compression = info.enableCompression ? rdfCompression::rdfCompressionZstd :
                                                rdfCompression::rdfCompressionNone,
        .version     = info.version
    };
    memcpy(currentChunkInfo.identifier, info.id, TextIdentifierSize);

    Util::RWLockAuto<Util::RWLock::ReadWrite> chunkAppendLock(&m_chunkAppendLock);

    // Append the incoming chunk to the data stream
    int result = rdfChunkFileWriterWriteChunk(m_pChunkFileWriter,
                                              &currentChunkInfo,
                                              info.dataSize,
                                              info.pData,
                                              &m_currentChunkIndex);

    return RdfResultToPalResult(result);
}

// =====================================================================================================================
void TraceSession::FinishTrace()
{
    Util::RWLockAuto<Util::RWLock::ReadOnly> traceSourceLock(&m_registerTraceSourceLock);

    // Notify all requested trace sources that the trace has finished
    const StructuredValue traceSources = m_pReader->GetRoot()["sources"];
    for (uint32 i = 0; i < traceSources.GetArrayLength(); i++)
    {
        const char* pName = traceSources[i]["name"].GetStringPtr();
        if (pName != nullptr)
        {
            GpuUtil::ITraceSource** ppSource = m_registeredTraceSources.FindKey(pName);

            if ((ppSource != nullptr) && (*ppSource != nullptr))
            {
                (*ppSource)->OnTraceFinished(); // Writes data into TraceSession
            }
        }
    }
}

// =====================================================================================================================
Result TraceSession::ReportError(
    const char        chunkId[TextIdentifierSize],
    const void*       pPayload,
    uint64            payloadSize,
    TraceErrorPayload payloadType,
    Result            errorResult)
{
    TraceErrorHeader errTraceHeader = {
        .chunkIndex  = static_cast<uint32>(m_currentChunkIndex),
        .resultCode  = errorResult,
        .payloadType = payloadType
    };
    memcpy(errTraceHeader.chunkId, chunkId, TextIdentifierSize);

    TraceChunkInfo info = {
        .version           = ErrorTraceChunkVersion,
        .pHeader           = &errTraceHeader,
        .headerSize        = sizeof(TraceErrorHeader),
        .pData             = pPayload,
        .dataSize          = static_cast<int64>(payloadSize),
        .enableCompression = false
    };
    memcpy(info.id, ErrorChunkTextIdentifier, TextIdentifierSize);

    Result result = WriteDataChunk(nullptr, info);

    return result;
}

} // namespace GpuUtil

#endif
