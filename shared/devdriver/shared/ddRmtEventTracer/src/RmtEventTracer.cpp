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

#include <RmtEventTracer.h>
#include <RmtEventStreamer.h>
#include <ddCommon.h>
#include <ddPlatform.h>
#include <string>

using namespace DevDriver;

namespace
{

RmtMemoryType RmtMemoryTypeFromString(const char* pString)
{
    // TODO: Replace this with a hash based lookup instead

    RmtMemoryType memoryType = RMT_MEMORY_TYPE_UNKNOWN;

    if (pString != nullptr)
    {
        if (Platform::Strcmpi(pString, "DDR2") == 0)
        {
            memoryType = RMT_MEMORY_TYPE_DDR2;
        }
        else if (Platform::Strcmpi(pString, "DDR3") == 0)
        {
            memoryType = RMT_MEMORY_TYPE_DDR3;
        }
        else if (Platform::Strcmpi(pString, "DDR4") == 0)
        {
            memoryType = RMT_MEMORY_TYPE_DDR4;
        }
        else if (Platform::Strcmpi(pString, "GDDR5") == 0)
        {
            memoryType = RMT_MEMORY_TYPE_GDDR5;
        }
        else if (Platform::Strcmpi(pString, "GDDR6") == 0)
        {
            memoryType = RMT_MEMORY_TYPE_GDDR6;
        }
        else if (Platform::Strcmpi(pString, "HBM") == 0)
        {
            memoryType = RMT_MEMORY_TYPE_HBM;
        }
        else if (Platform::Strcmpi(pString, "HBM2") == 0)
        {
            memoryType = RMT_MEMORY_TYPE_HBM2;
        }
        else if (Platform::Strcmpi(pString, "HBM3") == 0)
        {
            memoryType = RMT_MEMORY_TYPE_HBM3;
        }
        else if (Platform::Strcmpi(pString, "LPDDR4") == 0)
        {
            memoryType = RMT_MEMORY_TYPE_LPDDR4;
        }
        else if (Platform::Strcmpi(pString, "LPDDR5") == 0)
        {
            memoryType = RMT_MEMORY_TYPE_LPDDR5;
        }
        else if (Platform::Strcmpi(pString, "DDR5") == 0)
        {
            memoryType = RMT_MEMORY_TYPE_DDR5;
        }
    }

    return memoryType;
}

// =================================================================================================
// Utility to help transfer data from a file into the transfer callback. This
// function will move exactly `bufferSize` bytes per call, or fail. If more
// bytes are needed this should be called in a loop.
//
// Returns Result::FileIoError if something goes wrong
DD_RESULT TransferFileData(
    void*               pBuffer,
    size_t              bufferSize,
    FILE*               pSourceFile,
    const DDByteWriter* pWriter)
{
    DD_ASSERT(pBuffer != nullptr);
    DD_ASSERT(bufferSize > 0);
    DD_ASSERT(pSourceFile != nullptr);
    DD_ASSERT(pWriter != nullptr);

    const size_t bytesRead = fread(pBuffer, 1, bufferSize, pSourceFile);
    DD_RESULT result =
        (bytesRead == bufferSize) ? DD_RESULT_SUCCESS : DD_RESULT_DD_GENERIC_FILE_IO_ERROR;

    if (result == DD_RESULT_SUCCESS)
    {
        result = pWriter->pfnWriteBytes(pWriter->pUserdata, pBuffer, bufferSize);
    }

    return result;
}

// =================================================================================================
// Utility to reset an RmtWriter object to a ready-to-write state
void ResetRmtWriter(
    RmtWriter* pRmtWriter)
{
    DD_ASSERT(pRmtWriter != nullptr);

    pRmtWriter->Reset();
    pRmtWriter->Init();
}

// =================================================================================================
// Helper function to check if the target platform is Linux.
bool IsTargetSystemLinux(const system_info_utils::SystemInfo& systemInfo)
{
    bool isLinux = false;
    const std::string& name = systemInfo.os.name;
    const std::string& desc = systemInfo.os.desc;
    if ((name == Platform::OsInfo::kOsTypeLinux) ||
        (desc.find(Platform::OsInfo::kOsTypeLinux) != std::string::npos))
    {
        isLinux = true;
    }
    return isLinux;
}

} // unamed namespace

// =================================================================================================
RmtEventTracer::RmtEventTracer(
    const LoggerUtil&       logger,
    const DDAllocCallbacks& pApiAlloc)
    : m_apiAlloc(pApiAlloc)
    , m_ddAlloc({static_cast<void*>(&m_apiAlloc), ddApiAlloc, ddApiFree})
    , m_traceState(TraceState::NotStarted)
    , m_dataStreams(m_ddAlloc)
    , m_rmtWriter(m_ddAlloc)
    , m_logger(logger)
{

    m_pKmdStreamer = DD_NEW(RmtEventStreamer, m_ddAlloc)(this, logger);
    m_pUmdStreamer = DD_NEW(RmtEventStreamer, m_ddAlloc)(this, logger);
    m_pRouterStreamer = DD_NEW(RmtEventStreamer, m_ddAlloc)(this, logger);
}

RmtEventTracer::~RmtEventTracer()
{
    DD_DELETE(m_pKmdStreamer, m_ddAlloc);
    DD_DELETE(m_pUmdStreamer, m_ddAlloc);
    DD_DELETE(m_pRouterStreamer, m_ddAlloc);
}

// =================================================================================================
DD_RESULT RmtEventTracer::BeginTrace(
    ProcessId processId,
    DDNetConnection hConnection,
    DDClientId gfxKernelId,
    DDClientId amdLogId,
    DDClientId umdId,
    DDClientId routerId,
    const Vector<uint8_t>& systemInfoBuffer)
{
    DD_RESULT result = DD_RESULT_SUCCESS;

    system_info_utils::SystemInfo systemInfo{};
    std::string jsonStr(reinterpret_cast<const char*>(systemInfoBuffer.Data()), systemInfoBuffer.Size());
    system_info_utils::SystemInfoReader::Parse(jsonStr, systemInfo);

    bool isTargetLinux = IsTargetSystemLinux(systemInfo);

    // Start a new memory trace in our attached data context
    if (result == DD_RESULT_SUCCESS)
    {
        BeginTraceInternal(systemInfo);
    }

    if (isTargetLinux)
    {
        const uint32_t drmMajorVersion = systemInfo.os.config.drm_major_version;
        const uint32_t drmMinorVersion = systemInfo.os.config.drm_minor_version;
        if (drmMajorVersion < 3 || ((drmMajorVersion == 3) && (drmMinorVersion < 45)))
        {
            result = DD_RESULT_COMMON_VERSION_MISMATCH;
        }
    }

    uint32 kmdDataStreamId = 0;
    // Only enable kmd event streaming on non-Linux target platform. On Linux, kernel
    // events are gathered by the router, so no need to enable it.
    if ((result == DD_RESULT_SUCCESS) && (isTargetLinux == false))
    {
        // Acquire the data stream for the kernel mode driver
        result = AcquireDataStream(&kmdDataStreamId, 0, 0);
        if (result != DD_RESULT_SUCCESS)
        {
            LogError("Failed to acquire KMD stream from data context: %d", result);
        }
    }

    uint32 umdDataStreamId = 0;
    if (result == DD_RESULT_SUCCESS)
    {
        // Acquire the data stream for the user mode driver
        result = AcquireDataStream(&umdDataStreamId, processId, 0);
        if (result != DD_RESULT_SUCCESS)
        {
            LogError("Failed to acquire UMD stream from data context: %d", result);
        }
    }

    uint32 routerDataStreamId = 0;
    if (result == DD_RESULT_SUCCESS)
    {
        // Acquire the data stream for the network router
        result = AcquireDataStream(&routerDataStreamId, 0, 0);
        if (result != DD_RESULT_SUCCESS)
        {
            LogError("Failed to acquire Router stream from data context: %d", result);
        }
    }

    // Start memory tracing on the kernel client
    if ((result == DD_RESULT_SUCCESS) && (isTargetLinux == false))
    {
        // Try to start the stream from the AmdLog provider:
        result = m_pKmdStreamer->BeginStreaming(
            amdLogId,
            hConnection,
            kmdDataStreamId,
            kAmdLogProviderId);

        // Fallback to the KMD client if we fail:
        if (result != DD_RESULT_SUCCESS)
        {
            LogInfo("Failed to connect to AmdLog Client, trying KMD client: %d", result);
            result = m_pKmdStreamer->BeginStreaming(
                gfxKernelId,
                hConnection,
                kmdDataStreamId,
                kKmdProviderId);

            if (result != DD_RESULT_SUCCESS)
            {
                LogError("Failed to begin KMD stream: %d", result);
            }
        }
    }

    // Start memory tracing on the umd client
    if (result == DD_RESULT_SUCCESS)
    {
        result = m_pUmdStreamer->BeginStreaming(
            umdId,
            hConnection,
            umdDataStreamId,
            kUmdProviderId);

        if (result != DD_RESULT_SUCCESS)
        {
            LogError("Failed to begin UMD stream: %d", result);
        }
    }

    // Start memory tracing on the router client
    if (result == DD_RESULT_SUCCESS)
    {
        result = m_pRouterStreamer->BeginStreaming(
            routerId,
            hConnection,
            routerDataStreamId,
            kRouterProviderId);

        if (result != DD_RESULT_SUCCESS)
        {
            LogError("Failed to begin Router stream: %d", result);
        }
        // Allow router streaming to fail
        result = DD_RESULT_SUCCESS;
    }

    if (result == DD_RESULT_SUCCESS)
    {
        LogInfo("Memory trace started successfully");
    }
    else
    {
        // If we fail to begin the trace, we should attempt to undo any state changes we've made.

        // Stop the Router streamer
        if (m_pRouterStreamer->IsStreaming())
        {
            m_pRouterStreamer->EndStreaming(true);
        }

        // Stop the UMD streamer
        if (m_pUmdStreamer->IsStreaming())
        {
            m_pUmdStreamer->EndStreaming(true);
        }

        // Stop the KMD streamer
        if (m_pKmdStreamer->IsStreaming())
        {
            m_pKmdStreamer->EndStreaming(true);
        }

        // Clear tracing data.
        Clear();

        LogError("Memory trace failed to start: %d", result);
    }

    return result;
}

// =================================================================================================
DD_RESULT RmtEventTracer::EndTrace(EndTraceReason endReason, bool isClientInitialized)
{
    // This function should never be called with an unknown trace reason
    // The caller should always have some idea of why the trace is ending.
    DD_ASSERT(endReason != EndTraceReason::Unknown);

    DD_RESULT kmdEndResult = DD_RESULT_SUCCESS;
    DD_RESULT umdEndResult = DD_RESULT_SUCCESS;
    DD_RESULT routerEndResult = DD_RESULT_SUCCESS;
    bool routerWasStreaming = false;

    // Only end the streamers if we are NOT requested to continue
    if (endReason != EndTraceReason::UserRequestedContinue)
    {
        // Stop all of the event streamers since the current trace has come to an end.

        if (m_pKmdStreamer->IsStreaming())
        {
            const bool   isKmdAlive   = true;
            kmdEndResult = m_pKmdStreamer->EndStreaming(isKmdAlive);
        }

        // We need to stop the UMD trace regardless of whether we succeeded on
        // stopping the KMD trace

        // Attempt to stop the user mode driver streaming process. If this
        // trace ended because of a user request, then we expect the user mode
        // driver to be alive still.
        const bool   isUmdAlive   = (endReason == EndTraceReason::UserRequested);
        umdEndResult = m_pUmdStreamer->EndStreaming(isUmdAlive);

        routerWasStreaming = m_pRouterStreamer->IsStreaming();
        if (routerWasStreaming)
        {
            // Stop the Router's trace. It is always expected to be alive since
            // it owns the network we're on.
            const bool isRouterAlive = true;
            routerEndResult = m_pRouterStreamer->EndStreaming(isRouterAlive);
        }
    }

    bool isDataValid = (endReason != EndTraceReason::Abort);

    if (isDataValid)
    {
        // Only consider the data to be valid if:
        // 1. All streamers finished without running into errors
        // 2. The client completed all of the driver initialization steps
        //    successfully (This helps filter out the adapter enumeration
        //    process inside many applications).
        // 3. Router streamer did not start but all other streamers finished
        //    without errors.
        isDataValid = ((m_pKmdStreamer->HasEncounteredErrors() == false) &&
                       (m_pUmdStreamer->HasEncounteredErrors() == false) &&
                       (!routerWasStreaming || (m_pRouterStreamer->HasEncounteredErrors() == false)) &&
                       isClientInitialized);

        LogInfo("Memory trace ending with %s data", isDataValid ? "valid" : "invalid");
    }

    // End the trace in the data context once our streaming process is finished
    const DD_RESULT endTraceResult = EndTraceInternal(endReason, isDataValid);

    DD_RESULT result = DD_RESULT_SUCCESS;

    if (endReason == EndTraceReason::Abort)
    {
        // We don't care about the streamer and data context result codes if the trace was aborted
        result = DD_RESULT_DD_GENERIC_ABORTED;
    }
    else
    {
        if (kmdEndResult != DD_RESULT_SUCCESS)
        {
            result = kmdEndResult;
            LogError("Memory trace ended with kernel mode driver error: %d", result);
        }
        if (umdEndResult != DD_RESULT_SUCCESS)
        {
            result = umdEndResult;
            LogError("Memory trace ended with user mode driver error: %d", result);
        }
        if (routerEndResult != DD_RESULT_SUCCESS)
        {
            result = routerEndResult;
            LogError("Memory trace ended with router error: %d", result);
        }
        if (endTraceResult != DD_RESULT_SUCCESS)
        {
            result = endTraceResult;
            LogError("Memory trace ended with data context error: %d", result);
        }
    }

    if (result == DD_RESULT_SUCCESS)
    {
        LogInfo("Memory trace ended successfully");
    }

    return result;
}

// =================================================================================================
DD_RESULT RmtEventTracer::TransferTraceData(
    const DDByteWriter* pWriter)
{
    DD_RESULT result = DD_RESULT_COMMON_INVALID_PARAMETER;

    if (IsValidDDByteWriter(pWriter))
    {
        // We can transfer data during the Ended state or while the trace is still running
        if ((m_traceState == TraceState::Ended) || (m_traceState == TraceState::Running))
        {
            // Create a temporary RMT writer to write a new file header for the trace
            // This will also be used to write trace data chunk headers later in the transfer process
            RmtWriter tempRmtWriter(m_ddAlloc);
            tempRmtWriter.Init();
            tempRmtWriter.WriteFileHeader();

            // Compute the total size of the data that we need to transfer
            // We need to account for the file header in the temp rmt writer, the chunks already written into the main
            // rmt writer during the trace, and the total size of all data streams.
            const size_t totalTransferSize = static_cast<size_t>(tempRmtWriter.GetRmtDataSize() +
                                                                 m_rmtWriter.GetRmtDataSize()   +
                                                                 static_cast<size_t>(m_totalDataSize));

            // Signal to the caller that a transfer is beginning
            result = pWriter->pfnBegin(pWriter->pUserdata, &totalTransferSize);

            if (result == DD_RESULT_SUCCESS)
            {
                // Transfer the initial file header for our trace
                result = pWriter->pfnWriteBytes(
                    pWriter->pUserdata,
                    tempRmtWriter.GetRmtData(),
                    tempRmtWriter.GetRmtDataSize());
            }

            if (result == DD_RESULT_SUCCESS)
            {
                // Transfer the segment info, adapter info, and snapshot chunks that we've collected during the trace.
                result = pWriter->pfnWriteBytes(
                    pWriter->pUserdata,
                    m_rmtWriter.GetRmtData(),
                    m_rmtWriter.GetRmtDataSize());
            }

            if (result == DD_RESULT_SUCCESS)
            {
                // Transfer all data streams

                // Allocate a temporary scratch buffer on the heap to store
                // data being read back from disk We read data into memory in
                // large chunks to avoid file IO overhead
                constexpr size_t kTransferChunkSizeInBytes = (4 * 1024 * 1024); // 4 MiB
                Vector<uint8> scratchBuffer(m_ddAlloc);
                scratchBuffer.Resize(kTransferChunkSizeInBytes);

                for (uint32 streamIndex = 0;
                    streamIndex < static_cast<uint32>(m_dataStreams.Size());
                    ++streamIndex)
                {
                    TraceDataStream* pStream = &m_dataStreams[streamIndex];
                    DD_ASSERT(pStream != nullptr);

                    if (ferror(pStream->pFileHandle) == 0)
                    {
                        pStream->streamMutex.Lock();
                        // Create an RMT chunk header for the stream in memory
                        ResetRmtWriter(&tempRmtWriter);
                        tempRmtWriter.WriteDataChunkHeader(
                            pStream->processId,
                            pStream->threadId,
                            pStream->totalDataSize,
                            streamIndex,
                            pStream->rmtMajorVersion,
                            pStream->rmtMinorVersion);

                        // Write out the header to the output file
                        result = pWriter->pfnWriteBytes(
                            pWriter->pUserdata,
                            tempRmtWriter.GetRmtData(),
                            tempRmtWriter.GetRmtDataSize());

                        if (result == DD_RESULT_SUCCESS)
                        {
                            // Transfer the data from the current stream
                            result = TransferDataStream(
                                pStream,
                                streamIndex,
                                scratchBuffer.Data(),
                                scratchBuffer.Size(),
                                pWriter);

                            DD_ASSERT(result == DD_RESULT_SUCCESS);
                        }
                        pStream->streamMutex.Unlock();
                    }
                    else
                    {
                        result = DD_RESULT_DD_GENERIC_FILE_IO_ERROR;
                    }

                    if (result != DD_RESULT_SUCCESS)
                    {
                        break;
                    }
                }
            }

            // Signal to the user that the transfer is ending
            pWriter->pfnEnd(pWriter->pUserdata, result);
        }
    }

    return result;
}

// =================================================================================================
void RmtEventTracer::BeginTraceInternal(const system_info_utils::SystemInfo& systemInfo)
{
    if (m_traceState != TraceState::Running)
    {
        // Clear any existing memory trace data before beginning a new trace
        Clear();

        ProcessSystemInfo(systemInfo);

        m_traceState = TraceState::Running;
    }
}

// =================================================================================================
DD_RESULT RmtEventTracer::InsertSnapshot(
    const char* pSnapshotName,
    uint64_t    snapshotTimestamp)
{
    DD_RESULT result = DD_RESULT_SUCCESS;

    if (m_traceState == TraceState::Running)
    {
        m_rmtWriter.WriteSnapshot(pSnapshotName, snapshotTimestamp);
    }
    else
    {
        result = DD_RESULT_DD_GENERIC_UNAVAILABLE;
    }

    return result;
}

// =================================================================================================
DD_RESULT RmtEventTracer::AcquireDataStream(uint32_t* pDataStreamId, ProcessId processId, uint32_t threadId)
{
    DD_RESULT result = DD_RESULT_DD_GENERIC_UNAVAILABLE;

    if (m_traceState == TraceState::Running)
    {
        if (pDataStreamId != nullptr)
        {
            FILE* pFile = tmpfile();
            if (pFile != nullptr)
            {
                TraceDataStream stream = {};

                stream.pFileHandle = pFile;
                stream.processId   = processId;
                stream.threadId    = threadId;
                // Initialize the RMT version to 0.1 to match current behavior.
                stream.rmtMajorVersion = 0;
                stream.rmtMinorVersion = 1;

                const uint32 dataStreamId = static_cast<uint32>(m_dataStreams.Size());

                result = m_dataStreams.PushBack(stream) ?
                    DD_RESULT_SUCCESS : DD_RESULT_DD_GENERIC_INSUFFICIENT_MEMORY;

                if (result == DD_RESULT_SUCCESS)
                {
                    (*pDataStreamId) = dataStreamId;
                }
                else
                {
                    // Clean up the file handle if we fail to add it to our list
                    fclose(pFile);
                }
            }
            else
            {
                result = DD_RESULT_DD_GENERIC_FILE_ACCESS_ERROR;
            }

            UpdateTraceResult(result);
        }
        else
        {
            result = DD_RESULT_COMMON_INVALID_PARAMETER;
        }
    }

    return result;
}

// =================================================================================================
DD_RESULT RmtEventTracer::WriteRmtVersion(uint32_t dataStreamId, uint16_t rmtMajorVersion, uint16_t rmtMinorVersion)
{
    DD_RESULT result = DD_RESULT_DD_GENERIC_UNAVAILABLE;

    if (m_traceState == TraceState::Running)
    {
        if (dataStreamId < static_cast<uint32>(m_dataStreams.Size()))
        {
            TraceDataStream& stream = m_dataStreams[dataStreamId];
            stream.rmtMajorVersion = rmtMajorVersion;
            stream.rmtMinorVersion = rmtMinorVersion;
            result = DD_RESULT_SUCCESS;
        }
        else
        {
            result = DD_RESULT_COMMON_INVALID_PARAMETER;
        }
    }

    return result;
}

// =================================================================================================
DD_RESULT RmtEventTracer::WriteDataStream(uint32_t dataStreamId, const void* pData, size_t dataSize)
{
    DD_RESULT result = DD_RESULT_DD_GENERIC_UNAVAILABLE;

    if (m_traceState == TraceState::Running)
    {
        if ((dataStreamId < static_cast<uint32>(m_dataStreams.Size())) && (pData != nullptr) && (dataSize > 0))
        {
            TraceDataStream& stream = m_dataStreams[dataStreamId];

            // We shouldn't ever have invalid file handles in our stream list
            DD_ASSERT(stream.pFileHandle != nullptr);

            stream.streamMutex.Lock();
            const size_t bytesWritten = fwrite(pData, 1, dataSize, stream.pFileHandle);
            stream.streamMutex.Unlock();

            result = (bytesWritten == dataSize) ? DD_RESULT_SUCCESS : DD_RESULT_DD_GENERIC_FILE_IO_ERROR;

            if (result == DD_RESULT_SUCCESS)
            {
                Platform::AtomicAdd(&m_totalDataSize, static_cast<int64>(bytesWritten));
            }
        }
        else
        {
            result = DD_RESULT_COMMON_INVALID_PARAMETER;
        }
    }

    // We don't update the trace state here since this happens on multiple threads.
    // If there's a problem, we'll see it later when we check ferror.

    return result;
}

// =================================================================================================
DD_RESULT RmtEventTracer::EndTraceInternal(EndTraceReason reason, bool isDataValid)
{
    DD_RESULT result = DD_RESULT_DD_GENERIC_UNAVAILABLE;

    if (m_traceState == TraceState::Running)
    {
        result = DD_RESULT_SUCCESS;

        if (isDataValid)
        {
            // Calculate the total data size for all streams and remove any
            // streams that are in an error state.
            for (auto streamIter = m_dataStreams.Begin();
                streamIter != m_dataStreams.End();
                ++streamIter)
            {
                if (ferror(streamIter->pFileHandle) == 0)
                {
                    streamIter->streamMutex.Lock();
                    streamIter->totalDataSize = ftell(streamIter->pFileHandle);
                    streamIter->streamMutex.Unlock();
                }
                else
                {
                    // Remove the bad stream from the data stream list
                    streamIter = m_dataStreams.Remove(streamIter);

                    LogError(
                        "Removed bad data stream (Process %u) from memory trace data context.",
                        static_cast<uint32>(streamIter->processId));

                    result = DD_RESULT_DD_GENERIC_FILE_IO_ERROR;
                }
            }

            UpdateTraceResult(result);
        }
        else
        {
            DiscardDataStreams();

            // Indicate that the trace failed
            UpdateTraceResult(DD_RESULT_COMMON_UNKNOWN);
        }

        // If the user requested that we continue, then we don't update the trace state or end reason
        if (reason != EndTraceReason::UserRequestedContinue)
        {
            m_traceState = TraceState::Ended;
            m_endReason  = reason;
        }

        if (m_endReason == EndTraceReason::Unknown)
        {
            LogError("Memory trace ended with unknown reason!");
        }
    }

    return result;
}

// =================================================================================================
void RmtEventTracer::ProcessSystemInfo(const system_info_utils::SystemInfo& systemInfo)
{
    // WA: We currently just write gpu 0 to the file here instead of writing all of them because the chunks have
    //     no way to indicate which gpu they're associated with. Update this when the file format gets better
    //     support for multi-gpu configs.

    if (systemInfo.gpus.size() == 0)
    {
        LogError("[RmtEventTracer] SystemInfo is empty.");
        return;
    }

    auto gpu = systemInfo.gpus[0];

    RmtFileChunkSegmentInfo localHeap{};
    RmtFileChunkSegmentInfo invisibleHeap = {};
    RmtFileChunkSegmentInfo systemHeap    = {};
    for (const auto& heap : gpu.memory.heaps)
    {
        if (heap.heap_type == "local")
        {
            localHeap.heapType            = RMT_HEAP_TYPE_LOCAL;
            localHeap.physicalBaseAddress = heap.phys_addr;
            localHeap.size                = heap.size;
        }
        else if (heap.heap_type == "invisible")
        {
            invisibleHeap.heapType            = RMT_HEAP_TYPE_INVISIBLE;
            invisibleHeap.physicalBaseAddress = heap.phys_addr;
            invisibleHeap.size                = heap.size;
        }
    }

    systemHeap.heapType = RMT_HEAP_TYPE_SYSTEM;
    systemHeap.size     = systemInfo.os.memory.physical;

    m_rmtWriter.WriteSegmentInfo(localHeap);
    m_rmtWriter.WriteSegmentInfo(invisibleHeap);
    m_rmtWriter.WriteSegmentInfo(systemHeap);

    // Write Adapter chunk

    RmtFileChunkAdapterInfo adapterInfo = {};
    Platform::Strncpy(adapterInfo.name, gpu.name.data(), 128);
    adapterInfo.familyId   = gpu.asic.id_info.family;
    adapterInfo.revisionId = gpu.asic.id_info.revision;
    adapterInfo.deviceId   = gpu.asic.id_info.device;

    constexpr uint32 kHzToMhzDivisor        = (1000 * 1000);
    constexpr uint32 kByteToMegabyteDivisor = (1024 * 1024);

    uint64 minEngineClock = gpu.asic.engine_clock_hz.min;
    uint64 maxEngineClock = gpu.asic.engine_clock_hz.max;

    adapterInfo.minEngineClock = static_cast<uint32>(minEngineClock / kHzToMhzDivisor);
    adapterInfo.maxEngineClock = static_cast<uint32>(maxEngineClock / kHzToMhzDivisor);

    const RmtMemoryType memoryType = RmtMemoryTypeFromString(gpu.memory.type.c_str());
    if (memoryType == RMT_MEMORY_TYPE_UNKNOWN)
    {
        LogError("[RmtEventTracer] Invalid memory type: %s", gpu.memory.type.c_str());
    }

    adapterInfo.memoryType        = memoryType;
    adapterInfo.memoryOpsPerClock = gpu.memory.mem_ops_per_clock;
    adapterInfo.memoryBusWidth    = gpu.memory.bus_bit_width;
    adapterInfo.memoryBandwidth   = static_cast<uint32>(gpu.memory.bandwidth / kByteToMegabyteDivisor);

    uint64 minMemoryClock = gpu.memory.mem_clock_hz.min;
    uint64 maxMemoryClock = gpu.memory.mem_clock_hz.max;

    adapterInfo.minMemoryClock = static_cast<uint32>(minMemoryClock / kHzToMhzDivisor);
    adapterInfo.maxMemoryClock = static_cast<uint32>(maxMemoryClock / kHzToMhzDivisor);

    m_rmtWriter.WriteAdapterInfo(adapterInfo);
}

// =================================================================================================
// Transfers the data from the provided stream, it is assumed that the caller
// has taken the stream mutex lock
DD_RESULT RmtEventTracer::TransferDataStream(
    TraceDataStream*       pStream,
    uint32_t               streamIndex,
    uint8_t*               pScratchBuffer,
    size_t                 scratchBufferSize,
    const DDByteWriter*    pBinaryStream)
{
    DD_ASSERT(pStream != nullptr);
    DD_ASSERT(pScratchBuffer != nullptr);
    DD_ASSERT(scratchBufferSize > 1);
    DD_ASSERT(pBinaryStream != nullptr);
    DD_ASSERT(pStream->pFileHandle != nullptr);

    const uint32_t currPosition = ftell(pStream->pFileHandle);

    // Rewind to the beginning of the data stream
    rewind(pStream->pFileHandle);

    DD_RESULT result = DD_RESULT_SUCCESS;

    LogInfo("stream (%u) total data size: %llu", streamIndex, pStream->totalDataSize);

    size_t bytesRemaining = pStream->totalDataSize;
    while ((result == DD_RESULT_SUCCESS) && (bytesRemaining > 0))
    {
        // Calculate the size of the current transfer
        // Ideally we want to transfer as much data as possible during each transfer operation.
        // However, we must limit the max transfer size based on the size of the scratch buffer.
        const size_t transferSize = Platform::Min(bytesRemaining, scratchBufferSize);

        // Read a chunk of the data stream into scratch memory.
        result = TransferFileData(pScratchBuffer, transferSize, pStream->pFileHandle, pBinaryStream);

        if (result == DD_RESULT_SUCCESS)
        {
            // The data was successfully transferred so reduce our total number of bytes remaining.
            bytesRemaining -= transferSize;
        }
        else
        {
            // We encountered an error. Break out of the loop.
            break;
        }
    }
    // Now restore the file position we left off at
    fseek(pStream->pFileHandle, currPosition, SEEK_SET);
    return result;
}

// =================================================================================================
void RmtEventTracer::DiscardDataStreams()
{
    // Clean up any existing streams
    for (auto streamIter = m_dataStreams.Begin(); streamIter != m_dataStreams.End(); ++streamIter)
    {
        streamIter->streamMutex.Lock();
        // We should never have null file handles or names in this list
        DD_ASSERT(streamIter->pFileHandle != nullptr);

        fclose(streamIter->pFileHandle);
        streamIter->streamMutex.Unlock();
    }
    m_dataStreams.Clear();

    // Reset our total trace size to 0
    m_totalDataSize = 0;
}

// =================================================================================================
void RmtEventTracer::Clear()
{
    m_traceResult   = DD_RESULT_SUCCESS;
    m_traceState    = TraceState::NotStarted;
    m_endReason     = EndTraceReason::Unknown;

    ResetRmtWriter(&m_rmtWriter);

    DiscardDataStreams();
}

// =================================================================================================
void RmtEventTracer::UpdateTraceResult(DD_RESULT result)
{
    if (m_traceResult == DD_RESULT_SUCCESS)
    {
        m_traceResult = result;
    }
}

// =================================================================================================
void RmtEventTracer::LogError(const char* pFmt, ...)
{
    va_list args;
    va_start(args, pFmt);
    m_logger.Vprintf(DD_MAKE_LOG_EVENT(DD_LOG_LEVEL_ERROR, "RmtEventTracer"), pFmt, args);
    va_end(args);
}

// =================================================================================================
void RmtEventTracer::LogInfo(const char* pFmt, ...)
{
    va_list args;
    va_start(args, pFmt);
    m_logger.Vprintf(DD_MAKE_LOG_EVENT(DD_LOG_LEVEL_INFO, "RmtEventTracer"), pFmt, args);
    va_end(args);
}
