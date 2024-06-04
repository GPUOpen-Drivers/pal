/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/layers/functionIds.h"
#include "core/layers/gpuProfiler/gpuProfilerCmdBuffer.h"
#include "core/layers/gpuProfiler/gpuProfilerDevice.h"
#include "core/layers/gpuProfiler/gpuProfilerPlatform.h"
#include "core/layers/gpuProfiler/gpuProfilerQueue.h"
#include "palAutoBuffer.h"
#include "palDequeImpl.h"
#include "palGpaSession.h"
#include "palSysUtil.h"
#include "sqtt_file_format.h"

using namespace Util;

namespace Pal
{
namespace GpuProfiler
{

constexpr const char* EngineTypeStrings[] =
{
    "Gfx",
    "Ace",
    "Dma",
    "Timer",
};

static_assert(ArrayLen(EngineTypeStrings) == EngineTypeCount, "Missing entry in EngineTypeStrings.");

// =====================================================================================================================
// Writes .csv entries to file corresponding to the first count items in the m_logItems deque.  The caller guarantees
// that all of these calls are idle.
void Queue::OutputLogItemsToFile(
    size_t count,
    bool   hasDrawsDispatches)
{
    PAL_ASSERT(count <= m_logItems.NumElements());

    // Log items from a nested command buffer are flattened so that they appear the same as regular command buffer
    // calls.  activeCmdBufs tracks how many "open" command buffers there are - 0 during queue calls, 1 inside a submit,
    // and 2 inside a nested command buffer.  m_curLogCmdBufIdx is used to log which command buffer we are logging out
    // of the root-level command buffers submitted during a particular frame.  This is incremented anytime a root-level
    // command buffer is ended.
    uint32 activeCmdBufs = 0;

    const auto& settings = m_pDevice->GetPlatform()->PlatformSettings();

    const bool writeResults = (settings.gpuProfilerConfig.ignoreNonDrawDispatchCmdBufs) ? hasDrawsDispatches : true;

    for (uint32 i = 0; i < count; i++)
    {
        LogItem logItem = { };
        m_logItems.PopFront(&logItem);

        // The fence bundled to this submit wave should promise GpaSession ready.
        PAL_ASSERT((logItem.pGpaSession == nullptr) || logItem.pGpaSession->IsReady());

        if (logItem.type == CmdBufferCall)
        {
            if (logItem.cmdBufCall.callId == CmdBufCallId::Begin)
            {
                PAL_ASSERT(activeCmdBufs <= 1);
                activeCmdBufs++;

                m_curLogTraceIdx = 0;
            }

            // Add a "- " before command buffer calls made in a nested command buffer to differentiate them from calls
            // made in the root command buffer.
            const char* pNestedCmdBufPrefix = (activeCmdBufs == 2) ? "- " : "";

            // If we have received a command buffer call without having received a queue call for this frame,
            // we are using the dynamic start/stop of GPU profiling.  Open a new log file in this case.
            if ((m_logFile.IsOpen() == false) || (m_curLogFrame != logItem.frameId))
            {
                OpenLogFile(logItem.frameId);
                m_curLogFrame = logItem.frameId;
                m_curLogCmdBufIdx = 0;
            }

            if (writeResults)
            {
                OutputCmdBufCallToFile(logItem, pNestedCmdBufPrefix);
            }

            if (logItem.cmdBufCall.callId == CmdBufCallId::End)
            {
                PAL_ASSERT((activeCmdBufs > 0) && (activeCmdBufs <= 2));
                m_curLogCmdBufIdx += (--activeCmdBufs == 0) ? 1 : 0;
            }
        }
        else if ((logItem.type == QueueCall) || (logItem.type == VirtualQueueCall))
        {
            // If this is the first queue call for a new frame, open a new log file.
            if ((m_logFile.IsOpen() == false) || (m_curLogFrame != logItem.frameId))
            {
                OpenLogFile(logItem.frameId);
                m_curLogFrame = logItem.frameId;
                m_curLogCmdBufIdx = 0;
            }

            OutputQueueCallToFile(logItem);
        }
        else if (logItem.type == Frame)
        {
            m_curLogFrame = logItem.frameId;
            OutputFrameToFile(logItem);
        }
    }

    // Flush any buffered log writes to disk.  This is helpful for examining log files while an app is running or
    // dealing with app/driver crashes after the captured frame.
    m_logFile.Flush();
}

// =====================================================================================================================
// Opens and initializes a log file for the specified frame.
void Queue::OpenLogFile(
    uint32 frameId)
{
    const auto& settings = m_pDevice->GetPlatform()->PlatformSettings();

    m_logFile.Close();

    // Build a file name for this frame's log file.  It will have the pattern frameAAAAAADevBEngCD-EE.csv, where:
    //     - AAAAAA: Frame number.
    //     - B:      Device index (mostly relevant when profiling MGPU systems).
    //     - C:      Engine type (U = universal, C = compute, D = DMA, and T = timer).
    //     - D:      Engine index (for cases like compute/DMA where there are multiple instances of the same engine).
    //     - EE:     Queue ID (there can be multiple IQueue objects created for the same engine instance).
    char tempString[512];
    Snprintf(&tempString[0],
             sizeof(tempString),
             "%s/frame%06uDev%uEng%s%u-%02u.csv",
             m_pDevice->GetPlatform()->LogDirPath(),
             frameId,
             m_pDevice->Id(),
             EngineTypeStrings[static_cast<uint32>(m_pQueueInfos[0].engineType)],
             m_pQueueInfos[0].engineIndex,
             m_queueId);

    Result result = m_logFile.Open(&tempString[0], FileAccessWrite);
    PAL_ASSERT(result == Result::Success);

    // Write the CSV column headers to the newly opened file.
    const char* pCsvHeader = "Queue Call,CmdBuffer Index,CmdBuffer Call,SubQueueIdx,Start Clock,End Clock,Time (us) "
                             "[Frequency: %llu],PipelineHash,CompilerHash,VS/CS/TS,HS,DS,MS/GS,PS,"
                             "Verts/ThreadGroups,Instances,Comments,";
    Snprintf(&tempString[0], sizeof(tempString), pCsvHeader, m_pDevice->TimestampFreq());
    m_logFile.Write(&tempString[0], strlen(&tempString[0]));

    // Add some additional column headers based on enabled profiling features.
    if (settings.gpuProfilerConfig.recordPipelineStats)
    {
        const char* pCsvPipelineStatsHeader = "IaVertices,IaPrimitives,VsInvocations,GsInvocations,GsPrimitives,"
                                              "CInvocations,CPrimitives,PsInvocations,HsInvocations,DsInvocations,"
                                              "CsInvocations,TsInvocations,MsInvocations,MsPrimitives,";
        m_logFile.Write(pCsvPipelineStatsHeader, strlen(pCsvPipelineStatsHeader));
    }

    const uint32 numGlobalPerfCounters = m_pDevice->NumGlobalPerfCounters();
    const PerfCounter* pPerfCounters   = m_pDevice->GlobalPerfCounters();
    if (numGlobalPerfCounters > 0)
    {
        for (uint32 i = 0; i < numGlobalPerfCounters; i++)
        {
            m_logFile.Printf("%s,", &pPerfCounters[i].name[0]);
        }
    }

    if (IsSqttEnabled() || m_pDevice->IsSpmTraceEnabled())
    {
        m_logFile.Printf("TraceId,");
    }

    // Add last column for misc trace info
    const char* pCsvInfoHeader = "TraceNote";
    m_logFile.Write(pCsvInfoHeader, strlen(pCsvInfoHeader));

    m_logFile.Printf("\n");
}

// =====================================================================================================================
// Opens and initializes a SQ thread trace file.
void Queue::OpenSqttFile(
    uint32         shaderEngineId,
    uint32         computeUnitId,
    uint32         traceId,
    File*          pFile,
    const LogItem& logItem)
{
    const auto& settings = m_pDevice->GetPlatform()->PlatformSettings();

    // CRC Info
    constexpr uint32 CrcInfoSize = 256;
    char             crcInfo[CrcInfoSize] = {};
    size_t           crcPos = 0;

    if (logItem.type == CmdBufferCall)
    {
        bool addPipelineHash = false;

        if (logItem.cmdBufCall.flags.draw == 1)
        {
            crcPos += Snprintf(crcInfo + crcPos, CrcInfoSize - crcPos, "_DRAW");
            addPipelineHash = true;
        }
        else if (logItem.cmdBufCall.flags.dispatch == 1)
        {
            crcPos += Snprintf(crcInfo + crcPos, CrcInfoSize - crcPos, "_DISPATCH");
            addPipelineHash = true;
        }
        else if (logItem.cmdBufCall.flags.taskmesh == 1)
        {
            crcPos += Snprintf(crcInfo + crcPos, CrcInfoSize - crcPos, "_TASKMESH");
            addPipelineHash = true;
        }
        if (addPipelineHash)
        {
            const PipelineInfo* pPipelineInfo = nullptr;
            uint64 apiPsoHash = 0;

            if (true == logItem.cmdBufCall.flags.draw)
            {
                pPipelineInfo = &(logItem.cmdBufCall.draw.pipelineInfo);
                apiPsoHash = logItem.cmdBufCall.draw.apiPsoHash;
            }
            else if (true == logItem.cmdBufCall.flags.dispatch)
            {
                pPipelineInfo = &(logItem.cmdBufCall.dispatch.pipelineInfo);
                apiPsoHash = logItem.cmdBufCall.dispatch.apiPsoHash;
            }

            if (settings.gpuProfilerConfig.useFullPipelineHash)
            {
                crcPos += Snprintf(crcInfo + crcPos, CrcInfoSize - crcPos, "_PIPELINE%016llx-%016llx",
                                   pPipelineInfo->internalPipelineHash.stable,
                                   pPipelineInfo->internalPipelineHash.unique);
            }
            else if (settings.gpuProfilerSqttConfig.pipelineHashAsApiPsoHash)
            {
                crcPos += Snprintf(crcInfo + crcPos, CrcInfoSize - crcPos, "_PIPELINE%016llx", apiPsoHash);
            }
            else
            {
                crcPos += Snprintf(crcInfo + crcPos, CrcInfoSize - crcPos, "_PIPELINE%016llx",
                                   pPipelineInfo->internalPipelineHash.stable);
            }
        }
    }

    // frameAAAAAADevBEngCD-EE.CmdBufFTraceGSqttSeHCuUI.ttv, where:
    //     - AAAAAA: Frame number.
    //     - B:      Device index (mostly relevant when profiling MGPU systems).
    //     - C:      Engine type (U = universal, C = compute, D = DMA, and T = timer).
    //     - D:      Engine index (for cases like compute/DMA where there are multiple instances of the same engine).
    //     - EE:     Queue ID (there can be multiple IQueue objects created for the same engine instance).
    //     - F:      Command buffer ID.
    //     - G:      Thread-trace ID for correlation between per-Draw output and SQTT logs.
    //     - H:      Shader engine ID.
    //     - U:      Compute unit ID.
    //     - I:      Concatenation of shader IDs bound (for draw/dispatch calls only).
    char logFilePath[512];
    Snprintf(&logFilePath[0],
             sizeof(logFilePath),
             "%s/frame%06uDev%uEng%s%u-%02u.CmdBuf%uTrace%uSqttSe%uCu%u%s.ttv",
             m_pDevice->GetPlatform()->LogDirPath(),
             m_curLogFrame,
             m_pDevice->Id(),
             EngineTypeStrings[static_cast<uint32>(m_pQueueInfos[0].engineType)],
             m_pQueueInfos[0].engineIndex,
             m_queueId,
             m_curLogCmdBufIdx,
             traceId,
             shaderEngineId,
             computeUnitId,
             crcInfo);

    Result result = pFile->Open(&logFilePath[0], FileAccessWrite | FileAccessBinary);
    PAL_ASSERT(result == Result::Success);
}

// =====================================================================================================================
// Opens a .csv file for writing spm trace data
void Queue::OpenSpmFile(
    Util::File*    pFile,
    uint32         traceId,
    const LogItem& logItem,
    bool           isDataFabric)
{
    const auto& settings = m_pDevice->GetPlatform()->PlatformSettings();

    // CRC Info
    constexpr uint32 CrcInfoSize = 256;
    char             crcInfo[CrcInfoSize] = {};
    size_t           crcPos = 0;

    if (logItem.type == CmdBufferCall)
    {
        bool addPipelineHash = false;

        if (logItem.cmdBufCall.flags.draw == 1)
        {
            crcPos += Snprintf(crcInfo + crcPos, CrcInfoSize - crcPos, "_DRAW");
            addPipelineHash = true;
        }
        else if (logItem.cmdBufCall.flags.dispatch == 1)
        {
            crcPos += Snprintf(crcInfo + crcPos, CrcInfoSize - crcPos, "_DISPATCH");
            addPipelineHash = true;
        }
        else if (logItem.cmdBufCall.flags.taskmesh == 1)
        {
            crcPos += Snprintf(crcInfo + crcPos, CrcInfoSize - crcPos, "_TASKMESH");
            addPipelineHash = true;
        }

        if (addPipelineHash)
        {
            if (settings.gpuProfilerConfig.useFullPipelineHash)
            {
                crcPos += Snprintf(crcInfo + crcPos, CrcInfoSize - crcPos, "_PIPELINE%016llx-%016llx",
                                   logItem.cmdBufCall.draw.pipelineInfo.internalPipelineHash.stable,
                                   logItem.cmdBufCall.draw.pipelineInfo.internalPipelineHash.unique);
            }
            else
            {
                crcPos += Snprintf(crcInfo + crcPos, CrcInfoSize - crcPos, "_PIPELINE%016llx",
                                   logItem.cmdBufCall.draw.pipelineInfo.internalPipelineHash.stable);
            }
        }
    }

    // frameAAAAAADevBEngCD-EE.CmdBufFTraceGSpmI.csv, where:
    //     - AAAAAA: Frame number.
    //     - B:      Device index (mostly relevant when profiling MGPU systems).
    //     - C:      Engine type (U = universal, C = compute, D = DMA, and T = timer).
    //     - D:      Engine index (for cases like compute/DMA where there are multiple instances of the same engine).
    //     - EE:     Queue ID (there can be multiple IQueue objects created for the same engine instance).
    //     - F:      Command buffer ID.
    //     - G:      Trace ID for correlation between per-Draw output and SPM logs.
    //     - I:      Concatenation of shader IDs bound (for draw/dispatch calls only).
    char logFilePath[512];
    if (isDataFabric)
    {
        Snprintf(&logFilePath[0],
                 sizeof(logFilePath),
                 "%s/frame%06uDev%uEng%s%u-%02u.CmdBuf%uTrace%uDfSpm%s.csv",
                 m_pDevice->GetPlatform()->LogDirPath(),
                 m_curLogFrame,
                 m_pDevice->Id(),
                 EngineTypeStrings[static_cast<uint32>(m_pQueueInfos[0].engineType)],
                 m_pQueueInfos[0].engineIndex,
                 m_queueId,
                 m_curLogCmdBufIdx,
                 traceId,
                 crcInfo);
    }
    else
    {
        Snprintf(&logFilePath[0],
                 sizeof(logFilePath),
                 "%s/frame%06uDev%uEng%s%u-%02u.CmdBuf%uTrace%uSpm%s.csv",
                 m_pDevice->GetPlatform()->LogDirPath(),
                 m_curLogFrame,
                 m_pDevice->Id(),
                 EngineTypeStrings[static_cast<uint32>(m_pQueueInfos[0].engineType)],
                 m_pQueueInfos[0].engineIndex,
                 m_queueId,
                 m_curLogCmdBufIdx,
                 traceId,
                 crcInfo);
    }

    Result result = pFile->Open(&logFilePath[0], FileAccessWrite);
    PAL_ASSERT(result == Result::Success);
}

// =====================================================================================================================
// Opens and initializes an SQ thread trace file for consumption by RGP (Radeon GPU Profiler).
void Queue::OutputRgpFile(
    const GpuUtil::GpaSession& gpaSession,
    uint32                     sampleId)
{
    // Open a binary file named like frame018479.rgp.
    char logFilePath[512];
    Snprintf(&logFilePath[0],
             sizeof(logFilePath),
             "%s/frame%06u.rgp",
             m_pDevice->GetPlatform()->LogDirPath(),
             m_curLogFrame);

    File file;
    Result result = file.Open(&logFilePath[0], FileAccessBinary | FileAccessWrite);

    void*  pResult  = nullptr;
    size_t dataSize = 0;
    if (result == Result::Success)
    {
        result = gpaSession.GetResults(sampleId, &dataSize, pResult);
        PAL_ASSERT(dataSize != 0);
    }

    if (result == Result::Success)
    {
        pResult = static_cast<void*>(PAL_MALLOC(dataSize, m_pDevice->GetPlatform(), AllocInternal));
        if (pResult == nullptr)
        {
            result = Result::ErrorOutOfMemory;
        }
    }

    if (result == Result::Success)
    {
        result = gpaSession.GetResults(sampleId, &dataSize, pResult);
    }

    if (result == Result::Success)
    {
        file.Write(pResult, dataSize);
    }

    PAL_SAFE_FREE(pResult, m_pDevice->GetPlatform());

    file.Close();
}

// =====================================================================================================================
// Outputs details of a single queue call to the log file.
void Queue::OutputQueueCallToFile(
    const LogItem& logItem)
{
    PAL_ASSERT((logItem.type == QueueCall) || (logItem.type == VirtualQueueCall));
    // Ensure the use of the queueCall.callId is the same as use of virtualQueueCall.callId
    static_assert(sizeof(VirtualQueueCallId) == sizeof(QueueCallId));

    const char* const *strings = ((logItem.type == QueueCall) ? QueueCallIdStrings : VirtualQueueCallIdStrings);

    m_logFile.Printf("%s,,,,,,,,,,,,,,,,,", strings[static_cast<uint32>(logItem.queueCall.callId)]);

    if (m_pDevice->GetPlatform()->PlatformSettings().gpuProfilerConfig.recordPipelineStats)
    {
        m_logFile.Printf(",,,,,,,,,,,,,,");
    }

    for (uint32 i = 0; i < m_numReportedPerfCounters; i++)
    {
        m_logFile.Printf(",");
    }

    m_logFile.Printf("\n");
}

//======================================================================================================================
// Outputs details of a single command buffer call to the log file.
void Queue::OutputCmdBufCallToFile(
    const LogItem& logItem,
    const char*    pNestedCmdBufPrefix)
{
    PAL_ASSERT(logItem.type == CmdBufferCall);
    PAL_ASSERT(m_logFile.IsOpen());

    constexpr uint32 CsIdx = static_cast<uint32>(ShaderType::Compute);
    constexpr uint32 TsIdx = static_cast<uint32>(ShaderType::Task);
    constexpr uint32 VsIdx = static_cast<uint32>(ShaderType::Vertex);
    constexpr uint32 HsIdx = static_cast<uint32>(ShaderType::Hull);
    constexpr uint32 DsIdx = static_cast<uint32>(ShaderType::Domain);
    constexpr uint32 GsIdx = static_cast<uint32>(ShaderType::Geometry);
    constexpr uint32 MsIdx = static_cast<uint32>(ShaderType::Mesh);
    constexpr uint32 PsIdx = static_cast<uint32>(ShaderType::Pixel);

    const auto& settings   = m_pDevice->GetPlatform()->PlatformSettings();
    const auto& cmdBufItem = logItem.cmdBufCall;

    m_logFile.Printf(",%d,%s%s,%d,",
                     m_curLogCmdBufIdx,
                     pNestedCmdBufPrefix,
                     CmdBufCallIdStrings[static_cast<uint32>(cmdBufItem.callId)],
                     cmdBufItem.subQueueIdx);

    OutputTimestampsToFile(logItem);

    // Print any draw/dispatch specific info (shader hashes, etc.).
    if (cmdBufItem.flags.draw || cmdBufItem.flags.dispatch || cmdBufItem.flags.taskmesh)
    {
        m_logFile.Printf("0x%016llx,0x%016llx",
                         cmdBufItem.draw.apiPsoHash,
                         cmdBufItem.draw.pipelineInfo.internalPipelineHash.stable);

        if (settings.gpuProfilerConfig.useFullPipelineHash)
        {
            m_logFile.Printf("-0x%016llx", cmdBufItem.draw.pipelineInfo.internalPipelineHash.unique);
        }

        if (cmdBufItem.flags.draw)
        {
            m_logFile.Printf(",0x%016llx%016llx,0x%016llx%016llx,0x%016llx%016llx,"
                             "0x%016llx%016llx,0x%016llx%016llx,%u,%u,,",
                             cmdBufItem.draw.pipelineInfo.shader[VsIdx].hash.upper,
                             cmdBufItem.draw.pipelineInfo.shader[VsIdx].hash.lower,
                             cmdBufItem.draw.pipelineInfo.shader[HsIdx].hash.upper,
                             cmdBufItem.draw.pipelineInfo.shader[HsIdx].hash.lower,
                             cmdBufItem.draw.pipelineInfo.shader[DsIdx].hash.upper,
                             cmdBufItem.draw.pipelineInfo.shader[DsIdx].hash.lower,
                             cmdBufItem.draw.pipelineInfo.shader[GsIdx].hash.upper,
                             cmdBufItem.draw.pipelineInfo.shader[GsIdx].hash.lower,
                             cmdBufItem.draw.pipelineInfo.shader[PsIdx].hash.upper,
                             cmdBufItem.draw.pipelineInfo.shader[PsIdx].hash.lower,
                             cmdBufItem.draw.vertexCount,
                             cmdBufItem.draw.instanceCount);
        }
        else if (cmdBufItem.flags.dispatch)
        {
            m_logFile.Printf(",0x%016llx%016llx,,,,,%u,,,",
                             cmdBufItem.draw.pipelineInfo.shader[CsIdx].hash.upper,
                             cmdBufItem.draw.pipelineInfo.shader[CsIdx].hash.lower,
                             cmdBufItem.dispatch.threadGroupCount);
        }
        else
        {
            m_logFile.Printf(",0x%016llx%016llx,,,0x%016llx%016llx,0x%016llx%016llx,%u,,,",
                             cmdBufItem.draw.pipelineInfo.shader[TsIdx].hash.upper,
                             cmdBufItem.draw.pipelineInfo.shader[TsIdx].hash.lower,
                             cmdBufItem.draw.pipelineInfo.shader[MsIdx].hash.upper,
                             cmdBufItem.draw.pipelineInfo.shader[MsIdx].hash.lower,
                             cmdBufItem.draw.pipelineInfo.shader[PsIdx].hash.upper,
                             cmdBufItem.draw.pipelineInfo.shader[PsIdx].hash.lower,
                             cmdBufItem.taskmesh.threadGroupCount);
        }
    }
    else if (cmdBufItem.flags.barrier)
    {
        m_logFile.Printf(",,,,,,,,,\"%s\",",
                         (cmdBufItem.barrier.pComment != nullptr) ? cmdBufItem.barrier.pComment : "");
    }
    else if (cmdBufItem.flags.comment)
    {
        m_logFile.Printf(",,,,,,,,,\"%s\",", cmdBufItem.comment.string);
    }
    else
    {
        m_logFile.Printf(",,,,,,,,,,");
    }

    OutputPipelineStatsToFile(logItem);
    OutputGlobalPerfCountersToFile(logItem);
    OutputTraceDataToFile(logItem);

    m_logFile.Printf("\n");
}

//======================================================================================================================
// Outputs details of a frame to the log file (used only for frame-granularity profiling).
void Queue::OutputFrameToFile(
    const LogItem& logItem)
{
    if (m_logFile.IsOpen() == false)
    {
        // Build a file name for this frame's log file.
        char tempString[512];
        Snprintf(&tempString[0],
                 sizeof(tempString),
                 "%s/frameLog.csv",
                 m_pDevice->GetPlatform()->LogDirPath());

        Result result = m_logFile.Open(&tempString[0], FileAccessWrite);
        PAL_ASSERT(result == Result::Success);

        // Write the CSV column headers to the newly opened file.
        const char* pCsvHeader = "Frame #,Start Clock,End Clock,Time (us) [Frequency: %llu],";
        Snprintf(&tempString[0], sizeof(tempString), pCsvHeader, m_pDevice->TimestampFreq());
        m_logFile.Write(&tempString[0], strlen(&tempString[0]));

        const uint32 numGlobalPerfCounters = m_pDevice->NumGlobalPerfCounters();
        const PerfCounter* pPerfCounters   = m_pDevice->GlobalPerfCounters();
        if (numGlobalPerfCounters > 0)
        {
            for (uint32 i = 0; i < numGlobalPerfCounters; i++)
            {
                m_logFile.Printf("%s,", &pPerfCounters[i].name[0]);
            }
        }

        if (IsSqttEnabled())
        {
            m_logFile.Printf("ThreadTraceId,");
        }

        // Add last column for misc trace info
        const char* pCsvInfoHeader = "TraceNote";
        m_logFile.Write(pCsvInfoHeader, strlen(pCsvInfoHeader));

        m_logFile.Printf("\n");
    }

    m_logFile.Printf("%u,", logItem.frameId);

    OutputTimestampsToFile(logItem);
    OutputGlobalPerfCountersToFile(logItem);
    OutputTraceDataToFile(logItem);

    m_logFile.Printf("\n");
    m_logFile.Flush();
}

// =====================================================================================================================
// Output the portion of a .csv with the start/end clock values and time elapsed.  Shared code by all profile
// granularities.
void Queue::OutputTimestampsToFile(
    const LogItem& logItem)
{
    if (HasValidGpaSample(&logItem, GpuUtil::GpaSampleType::Timing))
    {
        uint64 pResult[2] = {};
        Result result     = logItem.pGpaSession->GetResults(logItem.gpaSampleIdTs,
                                                            nullptr,
                                                            pResult);

        m_logFile.Printf("%llu,%llu,", pResult[0], pResult[1]);

        bool hideElapsedTime =
            (m_pDevice->GetPlatform()->PlatformSettings().gpuProfilerConfig.granularity ==
                GpuProfilerGranularityDraw) &&
            (logItem.type == LogItemType::CmdBufferCall) &&
            (logItem.cmdBufCall.callId == CmdBufCallId::Begin);

        // Print the elapsed time for this call if pre-call/post-call timestamps were inserted.
        if (!hideElapsedTime)
        {
            const double tsDiff   = static_cast<double>(pResult[1] - pResult[0]);
            const double timeInUs = 1000000 * tsDiff / m_pDevice->TimestampFreq();

            m_logFile.Printf("%.2lf,", timeInUs);
        }
        else
        {
            m_logFile.Printf(",");
        }
    }
    else
    {
        m_logFile.Printf(",,,");
    }
}

// =====================================================================================================================
// Output pipeline stats to file.  Only supported by draw/cmdbuf granularities.
void Queue::OutputPipelineStatsToFile(
    const LogItem& logItem)
{
    if (HasValidGpaSample(&logItem, GpuUtil::GpaSampleType::Query))
    {
        // Allocate max number of pipeline stats counters.
        uint64 pipelineStats[14] = {};
        size_t pipelineStatsSize = sizeof(pipelineStats);
        const Result result = logItem.pGpaSession->GetResults(logItem.gpaSampleIdQuery,
                                                              &pipelineStatsSize,
                                                              pipelineStats);

        PAL_ASSERT(result == Result::Success);
        PAL_ASSERT(pipelineStatsSize == sizeof(pipelineStats));

        // PAL hardcodes the layout of the return pipeline stats values based on the client, leading to different
        // versions of this code to a uniform log layout.
        m_logFile.Printf("%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,", pipelineStats[0],
                         pipelineStats[1], pipelineStats[2], pipelineStats[3], pipelineStats[4], pipelineStats[5],
                         pipelineStats[6], pipelineStats[7], pipelineStats[8], pipelineStats[9], pipelineStats[10],
                         pipelineStats[11], pipelineStats[12], pipelineStats[13]);

    }
    else if (m_pDevice->GetPlatform()->PlatformSettings().gpuProfilerConfig.recordPipelineStats)
    {
        m_logFile.Printf(",,,,,,,,,,,,,,");
    }
}

// =====================================================================================================================
// Dump the enabled global perf counters to file.  Shared code between draw/cmdbuf and per-frame profile granularities.
void Queue::OutputGlobalPerfCountersToFile(
    const LogItem& logItem)
{
    const PerfCounter* pGlobalPerfCounters   = m_pDevice->GlobalPerfCounters();
    const uint32       numGlobalPerfCounters = m_pDevice->NumGlobalPerfCounters();

    if ((numGlobalPerfCounters > 0) && HasValidGpaSample(&logItem, GpuUtil::GpaSampleType::Cumulative))
    {
        void*  pResult  = nullptr;
        size_t dataSize = 0;

        Result result = logItem.pGpaSession->GetResults(logItem.gpaSampleId,
                                                        &dataSize,
                                                        pResult);
        PAL_ASSERT(dataSize != 0);

        if (result == Result::Success)
        {
            pResult = static_cast<void*>(PAL_MALLOC(dataSize, m_pDevice->GetPlatform(), AllocInternal));
            if (pResult == nullptr)
            {
                result = Result::ErrorOutOfMemory;
            }
        }

        if (result == Result::Success)
        {
            result = logItem.pGpaSession->GetResults(logItem.gpaSampleId,
                                                             &dataSize,
                                                             pResult);
        }

        if (result == Result::Success)
        {
            AutoBuffer<uint64, 128, PlatformDecorator> data(m_numReportedPerfCounters, m_pDevice->GetPlatform());
            PAL_ASSERT(data.Capacity() >= m_numReportedPerfCounters);

            // Zero out the reported value for each counter.  The results from each instance of that counter will be
            // accumulated into this array.

            const PerfCounter* pPerfCounters = m_pDevice->GlobalPerfCounters();
            uint32 pidIndex = 0;

            for (uint32 i = 0; i < numGlobalPerfCounters; i++)
            {
                const uint64 instanceMask = pPerfCounters[i].instanceMask;
                data[i] = 0;
                for (uint32 j = 0; j < pPerfCounters[i].instanceCount; j++)
                {
                    if ((instanceMask == 0) || Util::BitfieldIsSet(instanceMask, j))
                    {
                        data[i] += static_cast<uint64*>(pResult)[pidIndex++];
                    }
                }
            }

            PAL_ASSERT(pidIndex == m_gpaSessionSampleConfig.perfCounters.numCounters);

            PAL_SAFE_FREE(pResult, m_pDevice->GetPlatform());

            // Output into .csv file.
            for (uint32 i = 0; i < m_numReportedPerfCounters; i++)
            {
                m_logFile.Printf("%llu,", data[i]);
            }
        }
    }
    else
    {
        for (uint32 i = 0; i < m_numReportedPerfCounters; i++)
        {
            m_logFile.Printf(",");
        }
    }
}

// =====================================================================================================================
// Dumps the SQ thread trace data and/or spm trace data from this experiment out to file.
void Queue::OutputTraceDataToFile(
    const LogItem& logItem)
{
    const auto& settings = m_pDevice->GetPlatform()->PlatformSettings();

    if ((m_pDevice->NumGlobalPerfCounters() == 0) &&
        (m_pDevice->IsSpmTraceEnabled() || IsSqttEnabled()) &&
        (HasValidGpaSample(&logItem, GpuUtil::GpaSampleType::Trace)))
    {
        // Output trace data in RGP format.
        if ((m_pDevice->GetProfilerMode() == GpuProfilerTraceEnabledRgp))
        {
            if (settings.gpuProfilerConfig.granularity ==
                GpuProfilerGranularity::GpuProfilerGranularityFrame)
            {
                OutputRgpFile(*logItem.pGpaSession, logItem.gpaSampleId);
                m_logFile.Printf("%u", m_curLogFrame);
            }
            else
            {
                m_logFile.Printf("USE FRAME-GRANULARITY FOR RGP");
            }
        }
        else if (m_pDevice->GetProfilerMode() == GpuProfilerTraceEnabledTtv)
        {
            // Output trace data in generic/raw format.
            // Output separate files for thread trace data (.ttv) and spm trace data (.csv)
            void*  pResult  = nullptr;
            size_t dataSize = 0;
            Result result   = logItem.pGpaSession->GetResults(logItem.gpaSampleId, &dataSize, pResult);
            PAL_ASSERT(dataSize != 0);

            if (result == Result::Success)
            {
                pResult = static_cast<void*>(PAL_MALLOC(dataSize, m_pDevice->GetPlatform(), AllocInternal));
                if (pResult == nullptr)
                {
                    result = Result::ErrorOutOfMemory;
                }
            }

            if (result == Result::Success)
            {
                result = logItem.pGpaSession->GetResults(logItem.gpaSampleId, &dataSize, pResult);
            }

            // Below crack open the .rgp blob in the GpuProfiler to extract the raw SQTT data. We assume
            // SQTT data comes before SPM data.
            if (result == Result::Success)
            {
                if (IsSqttEnabled())
                {
                    // Find the first SQTT_DESC chunk, stopping if we reach the end before finding any.
                    size_t      offset = sizeof(SqttFileHeader);
                    const auto* pChunk = static_cast<const SqttFileChunkHeader*>(VoidPtrInc(pResult, offset));

                    while ((offset < dataSize) && (pChunk->chunkIdentifier.chunkType != SQTT_FILE_CHUNK_TYPE_SQTT_DESC))
                    {
                        offset += pChunk->sizeInBytes;
                        pChunk = static_cast<const SqttFileChunkHeader*>(VoidPtrInc(pResult, offset));
                    }

                    // Process all SQTT chunks. These come in pairs of SQTT_DESC and SQTT_DATA chunks.
                    const auto* pDesc = static_cast<const SqttFileChunkSqttDesc*>(VoidPtrInc(pResult, offset));

                    while ((offset < dataSize) &&
                           (pDesc->header.chunkIdentifier.chunkType == SQTT_FILE_CHUNK_TYPE_SQTT_DESC))
                    {
                        offset += pDesc->header.sizeInBytes;
                        const auto* pData = static_cast<const SqttFileChunkSqttData*>(VoidPtrInc(pResult, offset));
                        PAL_ASSERT(pData->header.chunkIdentifier.chunkType == SQTT_FILE_CHUNK_TYPE_SQTT_DATA);

                        File logFile;
                        const uint32 shaderEngine = pDesc->shaderEngineIndex;
                        const uint32 computeUnit = pDesc->v1.computeUnitIndex;

                        OpenSqttFile(shaderEngine, computeUnit, m_curLogTraceIdx, &logFile, logItem);
                        logFile.Write(VoidPtrInc(pResult, pData->offset), pData->size);
                        logFile.Close();

                        offset += pData->header.sizeInBytes;
                        pDesc = static_cast<const SqttFileChunkSqttDesc*>(VoidPtrInc(pResult, offset));
                    }
                }

                // Spm trace chunk: Begin output of Spm trace data as a separate .csv file
                if (m_pDevice->IsSpmTraceEnabled())
                {
                    // Find the first SPM_DB chunk, stopping if we reach the end before finding any.
                    size_t      offset = sizeof(SqttFileHeader);
                    const auto* pChunk = static_cast<const SqttFileChunkHeader*>(VoidPtrInc(pResult, offset));

                    while ((offset < dataSize) && (pChunk->chunkIdentifier.chunkType != SQTT_FILE_CHUNK_TYPE_SPM_DB))
                    {
                        offset += pChunk->sizeInBytes;
                        pChunk = static_cast<const SqttFileChunkHeader*>(VoidPtrInc(pResult, offset));
                    }

                    if (offset < dataSize)
                    {
                        OutputRlcSpmData(logItem,
                                         pResult,
                                         offset,
                                         dataSize);
                    }
                }

                // Df Spm trace chunk: This will also go in a separate .csv file
                if (m_pDevice->IsSpmTraceEnabled())
                {
                    // Find the first DF_SPM_DB chunk, stopping if we reach the end before finding any.
                    size_t      offset = sizeof(SqttFileHeader);
                    const auto* pChunk = static_cast<const SqttFileChunkHeader*>(VoidPtrInc(pResult, offset));

                    while ((offset < dataSize) && (pChunk->chunkIdentifier.chunkType != SQTT_FILE_CHUNK_TYPE_DF_SPM_DB))
                    {
                        offset += pChunk->sizeInBytes;
                        pChunk = static_cast<const SqttFileChunkHeader*>(VoidPtrInc(pResult, offset));
                    }

                    if (offset < dataSize)
                    {
                        OutputDfSpmData(logItem,
                                        pResult,
                                        offset,
                                        dataSize);
                    }
                }

                // The main spreadsheet records the trace IDs to help correlate traces to the execution timeline.
                m_logFile.Printf("%u", m_curLogTraceIdx++);
            }

            PAL_SAFE_FREE(pResult, m_pDevice->GetPlatform());
        }
    }
    else if (logItem.errors.perfExpOutOfMemory != 0)
    {
        m_logFile.Printf("ERROR: OUT OF MEMORY");
    }
    else if (logItem.errors.perfExpOutOfGpuMemory != 0)
    {
        m_logFile.Printf("ERROR: OUT OF GPU MEMORY");
    }
    else if (logItem.errors.perfExpUnsupported != 0)
    {
        m_logFile.Printf("ERROR: THREAD TRACE UNSUPPORTED");
    }
}

// =====================================================================================================================
void Queue::OutputRlcSpmData(
    const LogItem&     logItem,
    void*              pResult,
    size_t             offset,
    size_t             dataSize)
{
    const PerfCounter* pPerfCounters   = m_pDevice->StreamingPerfCounters();
    uint32             numPerfCounters = m_pDevice->NumStreamingPerfCounters();
    uint32             checkNum        = m_gpaSessionSampleConfig.perfCounters.numCounters;
    const auto*        pSpmDbChunk     = static_cast<const SqttFileChunkSpmDb*>(VoidPtrInc(pResult, offset));
    uint32             sizeOfChunk     = sizeof(*pSpmDbChunk);
    uint32             numTimestamps   = pSpmDbChunk->numTimestamps;
    uint32             numCounterInfo  = pSpmDbChunk->numSpmCounterInfo;

    // Extract the SPM DB data arrays.
    offset += sizeOfChunk;

    const size_t offsetToData = offset;

    const auto* pTimestamp = static_cast<const gpusize*>(VoidPtrInc(pResult, offset));
    offset += numTimestamps * sizeof(*pTimestamp);

    const auto* pCounterInfo = static_cast<const SpmCounterInfo*>(VoidPtrInc(pResult, offset));
    offset += numCounterInfo * sizeof(*pCounterInfo);

    File spmFile;
    OpenSpmFile(&spmFile, m_curLogTraceIdx, logItem, false);

    if (numTimestamps > 0)
    {
        // Some tools supports draw and command buffer interval markers. We don't have
        // this hooked up in the GPU profiler currently but we can still write a single
        // "command buffer" indicating where SPM started and stopped.
        spmFile.Printf("frame%u_cb%u,%llu,%llu\n", m_curLogFrame, m_curLogCmdBufIdx,
            pTimestamp[0], pTimestamp[numTimestamps - 1]);
    }

    // The column header must be this exact string for some tools to detect that it
    // can correlate the SPM timeline with the SQTT timeline.
    spmFile.Printf("Time (realtime clock),");

    // Print the first line consisting of the counter names.
    for (uint32 i = 0; i < numPerfCounters; ++i)
    {
        const auto& counter = pPerfCounters[i];
        spmFile.Printf("%s,", &counter.name[0]);
    }

    spmFile.Printf("\n");

    for (uint32 sample = 0; sample < numTimestamps; ++sample)
    {
        // Write the raw sample timestamps so that the tools can correlate the SPM
        // timeline to the SQTT timeline.
        spmFile.Printf("%llu,", pTimestamp[sample]);

        uint32 counterIdx = 0;
        for (uint32 i = 0; i < numPerfCounters; i++)
        {
            uint64 sumAll = 0;
            for (uint32 j = 0; j < pPerfCounters[i].instanceCount; j++)
            {
                // Note we assume GpaSession fills out the SpmCounterInfo array in the same order we filled out its
                // PerfCounterId array. If that ever changes we'll need to inspect the counterInfo to find the counter
                // instances that match up to our current pPerfCounters[i].
                const SpmCounterInfo& counterInfo = pCounterInfo[counterIdx++];
                const void*const      pSamples    = VoidPtrInc(pResult, offsetToData + counterInfo.dataOffset);

                if (counterInfo.dataSize == sizeof(uint32))
                {
                    sumAll += static_cast<const uint32*>(pSamples)[sample];
                }
                else
                {
                    PAL_ASSERT(counterInfo.dataSize == sizeof(uint16));

                    sumAll += static_cast<const uint16*>(pSamples)[sample];
                }
            }
            spmFile.Printf("%llu,", sumAll);
        }

        PAL_ASSERT(counterIdx == checkNum);
        spmFile.Printf("\n");
    }
}

// =====================================================================================================================
void Queue::OutputDfSpmData(
    const LogItem&     logItem,
    void*              pResult,
    size_t             offset,
    size_t             dataSize)
{
    const PerfCounter* pPerfCounters   = m_pDevice->DfStreamingPerfCounters();
    uint32             numPerfCounters = m_pDevice->NumDfStreamingPerfCounters();
    uint32             checkNum        = m_gpaSessionSampleConfig.dfSpmPerfCounters.numCounters;
    const auto*        pSpmDbChunk     = static_cast<const SqttFileChunkDfSpmDb*>(VoidPtrInc(pResult, offset));
    uint32             sizeOfChunk     = sizeof(*pSpmDbChunk);
    uint32             numTimestamps   = pSpmDbChunk->numTimestamps;
    uint32             numCounterInfo  = pSpmDbChunk->numDfSpmCounterInfo;

    // Extract the SPM DB data arrays.
    offset += sizeOfChunk;

    const size_t offsetToData = offset;

    const auto* pTimestamp = static_cast<const gpusize*>(VoidPtrInc(pResult, offset));
    offset += numTimestamps * sizeof(*pTimestamp);

    const auto* pCounterInfo = static_cast<const DfSpmCounterInfo*>(VoidPtrInc(pResult, offset));
    offset += numCounterInfo * sizeof(*pCounterInfo);

    File spmFile;
    OpenSpmFile(&spmFile, m_curLogTraceIdx, logItem, false);

    if (numTimestamps > 0)
    {
        // Some tools supports draw and command buffer interval markers. We don't have
        // this hooked up in the GPU profiler currently but we can still write a single
        // "command buffer" indicating where SPM started and stopped.
        spmFile.Printf("frame%u_cb%u,%llu,%llu\n", m_curLogFrame, m_curLogCmdBufIdx,
            pTimestamp[0], pTimestamp[numTimestamps - 1]);
    }

    // The column header must be this exact string for some tools to detect that it
    // can correlate the SPM timeline with the SQTT timeline.
    spmFile.Printf("Time (realtime clock),");

    // Print the first line consisting of the counter names.
    for (uint32 i = 0; i < numPerfCounters; ++i)
    {
        const auto& counter = pPerfCounters[i];
        spmFile.Printf("%s,", &counter.name[0]);
    }

    spmFile.Printf("\n");

    for (uint32 sample = 0; sample < numTimestamps; ++sample)
    {
        // Write the raw sample timestamps so that the tools can correlate the SPM
        // timeline to the SQTT timeline.
        spmFile.Printf("%llu,", pTimestamp[sample]);

        uint32 counterIdx = 0;
        for (uint32 i = 0; i < numPerfCounters; i++)
        {
            uint32 sumAll = 0;
            for (uint32 j = 0; j < pPerfCounters[i].instanceCount; j++)
            {
                const size_t offsetToCntr  = offsetToData + pCounterInfo[counterIdx++].dataOffset;
                const size_t offsetToValid = offsetToData + pCounterInfo[counterIdx++].dataValidOffset;
                const auto* pData  = static_cast<const uint16*>(VoidPtrInc(pResult, offsetToCntr));
                const auto* pValid = static_cast<const uint32*>(VoidPtrInc(pResult, offsetToValid));
                if (pValid[sample] == 1)
                {
                    sumAll += pData[sample];
                }
            }
            spmFile.Printf("%u,", sumAll);
        }

        PAL_ASSERT(counterIdx == checkNum);
        spmFile.Printf("\n");
    }
}

} // GpuProfiler
} // Pal
