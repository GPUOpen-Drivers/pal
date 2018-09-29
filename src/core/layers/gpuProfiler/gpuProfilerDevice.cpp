/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/layers/gpuProfiler/gpuProfilerCmdBuffer.h"
#include "core/layers/gpuProfiler/gpuProfilerDevice.h"
#include "core/layers/gpuProfiler/gpuProfilerPipeline.h"
#include "core/layers/gpuProfiler/gpuProfilerQueue.h"

using namespace Util;

namespace Pal
{
namespace GpuProfiler
{

static GpuBlock StringToGpuBlock(const char* pString);
static constexpr ShaderHash ZeroShaderHash = {};

// =====================================================================================================================
Device::Device(
    PlatformDecorator* pPlatform,
    IDevice*           pNextDevice,
    uint32             id)
    :
    DeviceDecorator(pPlatform, pNextDevice),
    m_id(id),
    m_fragmentSize(0),
    m_bufferSrdDwords(0),
    m_imageSrdDwords(0),
    m_timestampFreq(0),
    m_logPipeStats(false),
    m_sqttFilteringEnabled(false),
    m_sqttCompilerHash(0),
    m_maxDrawsForThreadTrace(0),
    m_curDrawsForThreadTrace(0),
    m_profilerGranularity(GpuProfilerGranularityDraw),
    m_stallMode(GpuProfilerStallAlways),
    m_startFrame(0),
    m_endFrame(0),
    m_pGlobalPerfCounters(nullptr),
    m_numGlobalPerfCounters(0),
    m_pStreamingPerfCounters(nullptr),
    m_numStreamingPerfCounters(0)
{
    memset(m_queueIds, 0, sizeof(m_queueIds));

    m_sqttVsHash = ZeroShaderHash;
    m_sqttHsHash = ZeroShaderHash;
    m_sqttDsHash = ZeroShaderHash;
    m_sqttGsHash = ZeroShaderHash;
    m_sqttPsHash = ZeroShaderHash;
    m_sqttCsHash = ZeroShaderHash;
}

// =====================================================================================================================
Device::~Device()
{
    PAL_SAFE_DELETE_ARRAY(m_pGlobalPerfCounters, GetPlatform());

    if (m_pStreamingPerfCounters != nullptr)
    {
        PAL_SAFE_DELETE_ARRAY(m_pStreamingPerfCounters, GetPlatform());
    }
}

// =====================================================================================================================
// Determines if logging is currently enabled for the specified granularity, either due to the current frame range or
// because the user hit Shift-F11 to force this frame to be captured.
bool Device::LoggingEnabled(
    GpuProfilerGranularity granularity
    ) const
{
    const Platform& platform = *static_cast<const Platform*>(m_pPlatform);

    return ((m_profilerGranularity == granularity) &&
            (platform.IsLoggingForced() ||
             ((platform.FrameId() >= m_startFrame) && (platform.FrameId() < m_endFrame))));
}

// =====================================================================================================================
Result Device::CommitSettingsAndInit()
{
    // Update the public settings before we commit them.
    PalPublicSettings*const pInitialSettings = GetPublicSettings();

    // Force off the command allocator wait-on-submit optimization for embedded data. The profiler permits the client
    // to read and write to client embedded data in the replayed command buffers which breaks this optimization.
    //
    // This is actually a violation of PAL's residency rules because a command buffer must only reference allocations
    // from its command allocator, allocations made resident using AddGpuMemoryReferences or allocations on the
    // per-submit residency list. Unfortunately we must break these rules in order to support a record/replay layer.
    // We won't need to disable this optimization if we rewrite the GPU profiler to instrument the client commands.
    pInitialSettings->cmdAllocResidency &= ~CmdAllocResWaitOnSubmitEmbeddedData;

    Result result = DeviceDecorator::CommitSettingsAndInit();

    const auto& settings = GetPlatform()->PlatformSettings();

    // Capture properties and settings needed elsewhere in the GpuProfiler layer.
    DeviceProperties info;
    if (result == Result::Success)
    {
        result = m_pNextLayer->GetProperties(&info);
    }

    if (result == Result::Success)
    {
        const uint32 maxSeMask = (1 << info.gfxipProperties.shaderCore.numShaderEngines) - 1;
        m_fragmentSize         = info.gpuMemoryProperties.fragmentSize;
        m_bufferSrdDwords      = info.gfxipProperties.srdSizes.bufferView / sizeof(uint32);
        m_imageSrdDwords       = info.gfxipProperties.srdSizes.imageView / sizeof(uint32);
        m_timestampFreq        = info.timestampFrequency;
        m_logPipeStats         = settings.gpuProfilerConfig.recordPipelineStats;
        m_sqttCompilerHash     = settings.gpuProfilerSqttConfig.pipelineHash;
        m_seMask               = settings.gpuProfilerSqttConfig.seMask & maxSeMask;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 422
        m_stallMode           = settings.gpuProfilerSqttConfig.stallBehavior;
#endif

        m_sqttVsHash.upper = settings.gpuProfilerSqttConfig.vsHashHi;
        m_sqttVsHash.lower = settings.gpuProfilerSqttConfig.vsHashLo;
        m_sqttHsHash.upper = settings.gpuProfilerSqttConfig.hsHashHi;
        m_sqttHsHash.lower = settings.gpuProfilerSqttConfig.hsHashLo;
        m_sqttDsHash.upper = settings.gpuProfilerSqttConfig.dsHashHi;
        m_sqttDsHash.lower = settings.gpuProfilerSqttConfig.dsHashLo;
        m_sqttGsHash.upper = settings.gpuProfilerSqttConfig.gsHashHi;
        m_sqttGsHash.lower = settings.gpuProfilerSqttConfig.gsHashLo;
        m_sqttPsHash.upper = settings.gpuProfilerSqttConfig.psHashHi;
        m_sqttPsHash.lower = settings.gpuProfilerSqttConfig.psHashLo;
        m_sqttCsHash.upper = settings.gpuProfilerSqttConfig.csHashHi;
        m_sqttCsHash.lower = settings.gpuProfilerSqttConfig.csHashLo;

        m_sqttFilteringEnabled = ((m_sqttCompilerHash != 0)         ||
                                  ShaderHashIsNonzero(m_sqttVsHash) ||
                                  ShaderHashIsNonzero(m_sqttHsHash) ||
                                  ShaderHashIsNonzero(m_sqttDsHash) ||
                                  ShaderHashIsNonzero(m_sqttGsHash) ||
                                  ShaderHashIsNonzero(m_sqttPsHash) ||
                                  ShaderHashIsNonzero(m_sqttCsHash));

        m_profilerGranularity = settings.gpuProfilerPerfCounterConfig.granularity;

        m_maxDrawsForThreadTrace = settings.gpuProfilerSqttConfig.maxDraws;
        m_curDrawsForThreadTrace = 0;

        m_startFrame          = settings.gpuProfilerConfig.startFrame;
        m_endFrame            = m_startFrame + settings.gpuProfilerConfig.frameCount;

        for (uint32 i = 0; i < EngineTypeCount; i++)
        {
            m_minTimestampAlignment[i] = info.engineProperties[i].minTimestampAlignment;
        }
    }

    if (result == Result::Success)
    {
        // Create directory for log files.
        if (GetPlatform()->CreateLogDir(settings.gpuProfilerConfig.logDirectory) != Result::Success)
        {
            PAL_DPWARN("Failed to create folder '%s'", settings.gpuProfilerConfig.logDirectory);
        }
    }

    if ((result == Result::Success) && (settings.gpuProfilerPerfCounterConfig.globalPerfCounterConfigFile[0] != '\0'))
    {
        result = InitGlobalPerfCounterState();
        PAL_ASSERT(result == Result::Success);
    }

    if ((result == Result::Success) && (settings.gpuProfilerSpmConfig.spmPerfCounterConfigFile[0] != '\0'))
    {
        result = InitSpmTraceCounterState();
        PAL_ASSERT(result == Result::Success);
    }

    return result;
}

// =====================================================================================================================
size_t Device::GetQueueSize(
    const QueueCreateInfo& createInfo,
    Result*                pResult
    ) const
{
    return m_pNextLayer->GetQueueSize(createInfo, pResult) + sizeof(Queue);
}

// =====================================================================================================================
Result Device::CreateQueue(
    const QueueCreateInfo& createInfo,
    void*                  pPlacementAddr,
    IQueue**               ppQueue)
{
    IQueue* pNextQueue = nullptr;
    Queue*  pQueue     = nullptr;

    Result result = m_pNextLayer->CreateQueue(createInfo,
                                              NextObjectAddr<Queue>(pPlacementAddr),
                                              &pNextQueue);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextQueue != nullptr);
        PAL_ASSERT(createInfo.engineIndex < MaxEngineCount);
        pNextQueue->SetClientData(pPlacementAddr);

        const EngineType engineType = createInfo.engineType;
        const uint32     queueId    = m_queueIds[engineType][createInfo.engineIndex]++;

        pQueue = PAL_PLACEMENT_NEW(pPlacementAddr) Queue(pNextQueue,
                                                         this,
                                                         createInfo.queueType,
                                                         engineType,
                                                         createInfo.engineIndex,
                                                         queueId);

        result = pQueue->Init();
    }

    if (result == Result::Success)
    {
        (*ppQueue) = pQueue;
    }

    return result;
}

// =====================================================================================================================
size_t Device::GetCmdBufferSize(
    const CmdBufferCreateInfo& createInfo,
    Result*                    pResult
    ) const
{
    CmdBufferCreateInfo nextCreateInfo = createInfo;
    nextCreateInfo.pCmdAllocator = NextCmdAllocator(createInfo.pCmdAllocator);

    return m_pNextLayer->GetCmdBufferSize(nextCreateInfo, pResult) + sizeof(CmdBuffer);
}

// =====================================================================================================================
Result Device::CreateCmdBuffer(
    const CmdBufferCreateInfo& createInfo,
    void*                      pPlacementAddr,
    ICmdBuffer**               ppCmdBuffer)
{
    ICmdBuffer* pNextCmdBuffer = nullptr;
    CmdBuffer*  pCmdBuffer     = nullptr;

    CmdBufferCreateInfo nextCreateInfo = createInfo;
    nextCreateInfo.pCmdAllocator = NextCmdAllocator(createInfo.pCmdAllocator);

    Result result = m_pNextLayer->CreateCmdBuffer(nextCreateInfo,
                                                  NextObjectAddr<CmdBuffer>(pPlacementAddr),
                                                  &pNextCmdBuffer);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextCmdBuffer != nullptr);
        pNextCmdBuffer->SetClientData(pPlacementAddr);

        const bool enableSqtt = IsThreadTraceEnabled();

        pCmdBuffer = PAL_PLACEMENT_NEW(pPlacementAddr) CmdBuffer(pNextCmdBuffer,
                                                                 this,
                                                                 createInfo,
                                                                 m_logPipeStats,
                                                                 enableSqtt);
        result = pCmdBuffer->Init();
    }

    if (result == Result::Success)
    {
        (*ppCmdBuffer) = pCmdBuffer;
    }

    return result;
}

// =====================================================================================================================
size_t Device::GetTargetCmdBufferSize(
    const CmdBufferCreateInfo& createInfo,
    Result*                    pResult
    ) const
{
    CmdBufferCreateInfo nextCreateInfo = createInfo;
    nextCreateInfo.pCmdAllocator = NextCmdAllocator(createInfo.pCmdAllocator);

    return m_pNextLayer->GetCmdBufferSize(nextCreateInfo, pResult) + sizeof(TargetCmdBuffer);
}

// =====================================================================================================================
Result Device::CreateTargetCmdBuffer(
    const CmdBufferCreateInfo& createInfo,
    void*                      pPlacementAddr,
    TargetCmdBuffer**          ppCmdBuffer)
{
    ICmdBuffer*      pNextCmdBuffer = nullptr;
    TargetCmdBuffer* pCmdBuffer     = nullptr;

    CmdBufferCreateInfo nextCreateInfo = createInfo;
    nextCreateInfo.pCmdAllocator = NextCmdAllocator(createInfo.pCmdAllocator);

    Result result = m_pNextLayer->CreateCmdBuffer(nextCreateInfo,
                                                  NextObjectAddr<TargetCmdBuffer>(pPlacementAddr),
                                                  &pNextCmdBuffer);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextCmdBuffer != nullptr);
        pNextCmdBuffer->SetClientData(pPlacementAddr);

        pCmdBuffer = PAL_PLACEMENT_NEW(pPlacementAddr) TargetCmdBuffer(createInfo, pNextCmdBuffer, this);
        result = pCmdBuffer->Init();
    }

    if (result == Result::Success)
    {
        (*ppCmdBuffer) = pCmdBuffer;
    }

    return result;
}

// =====================================================================================================================
size_t Device::GetGraphicsPipelineSize(
    const GraphicsPipelineCreateInfo& createInfo,
    Result*                           pResult
    ) const
{
    return m_pNextLayer->GetGraphicsPipelineSize(createInfo, pResult) + sizeof(Pipeline);
}

// =====================================================================================================================
Result Device::CreateGraphicsPipeline(
    const GraphicsPipelineCreateInfo& createInfo,
    void*                             pPlacementAddr,
    IPipeline**                       ppPipeline)
{
    IPipeline* pNextPipeline = nullptr;
    Pipeline* pPipeline = nullptr;

    Result result = m_pNextLayer->CreateGraphicsPipeline(createInfo,
                                                         NextObjectAddr<Pipeline>(pPlacementAddr),
                                                         &pNextPipeline);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextPipeline != nullptr);
        pNextPipeline->SetClientData(pPlacementAddr);

        pPipeline = PAL_PLACEMENT_NEW(pPlacementAddr) Pipeline(pNextPipeline, this);
        result = pPipeline->InitGfx(createInfo);
    }

    if (result == Result::Success)
    {
        (*ppPipeline) = pPipeline;
    }

    return result;
}

// =====================================================================================================================
size_t Device::GetComputePipelineSize(
    const ComputePipelineCreateInfo& createInfo,
    Result*                          pResult
    ) const
{
    return m_pNextLayer->GetComputePipelineSize(createInfo, pResult) + sizeof(Pipeline);
}

// =====================================================================================================================
Result Device::CreateComputePipeline(
    const ComputePipelineCreateInfo& createInfo,
    void*                            pPlacementAddr,
    IPipeline**                      ppPipeline)
{
    IPipeline* pNextPipeline = nullptr;
    Pipeline* pPipeline = nullptr;

    Result result = m_pNextLayer->CreateComputePipeline(createInfo,
                                                        NextObjectAddr<Pipeline>(pPlacementAddr),
                                                        &pNextPipeline);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextPipeline != nullptr);
        pNextPipeline->SetClientData(pPlacementAddr);

        pPipeline = PAL_PLACEMENT_NEW(pPlacementAddr) Pipeline(pNextPipeline, this);
        result = pPipeline->InitCompute(createInfo);
    }

    if (result == Result::Success)
    {
        (*ppPipeline) = pPipeline;
    }

    return result;
}

// =====================================================================================================================
// Helper function to extract SPM or global perf counter info from config file.
Result Device::ExtractPerfCounterInfo(
    const PerfExperimentProperties& perfExpProps,
    File*                           pConfigFile,
    uint32                          numPerfCounter,
    PerfCounter*                    pPerfCounters)
{
    Result result = Result::Success;
    uint32 counterIdx = 0;
    uint32 lineNum = 1;
    while ((counterIdx < numPerfCounter) && (result == Result::Success))
    {
        constexpr size_t BufSize = 512;
        char buf[BufSize];
        size_t lineLength;

        if (pConfigFile->ReadLine(&buf[0], BufSize, &lineLength) == Result::Success)
        {
            buf[lineLength] = '\0';

            if ((buf[0] == '#') || (buf[0] == 0))
            {
                // Ignore empty and comment lines.
                continue;
            }
            else
            {
                char blockName[BlockNameSize];
                char instanceName[InstanceNameSize];
                char eventName[EventNameSize];
                uint32 eventId;

                // Read a line of the form "BlockName EventId InstanceName CounterName".
                const int scanfRet = sscanf(&buf[0],
                                            "%31s %u %7s %127s",
                                            &blockName[0],
                                            &eventId,
                                            &instanceName[0],
                                            &eventName[0]);

                if (scanfRet == 4)
                {
                    const GpuBlock block = StringToGpuBlock(&blockName[0]);
                    const uint32   blockIdx = static_cast<uint32>(block);

                    if (strcmp(instanceName, "EACH") == 0)
                    {
                        const uint32 instanceCount = perfExpProps.blocks[blockIdx].instanceCount;
                        for (uint32 i = 0; i < instanceCount; i++)
                        {
                            pPerfCounters[counterIdx].block         = block;
                            pPerfCounters[counterIdx].eventId       = eventId;
                            pPerfCounters[counterIdx].instanceId    = i;
                            pPerfCounters[counterIdx].instanceCount = 1;
                            Snprintf(
                                &pPerfCounters[counterIdx].name[0],
                                EventInstanceNameSize,
                                "%s_INSTANCE_%d",
                                &eventName[0],
                                i);
                            counterIdx++;
                        }
                    }
                    else if (strcmp(instanceName, "ALL") == 0)
                    {
                        const uint32 instanceCount              = perfExpProps.blocks[blockIdx].instanceCount;
                        pPerfCounters[counterIdx].block         = block;
                        pPerfCounters[counterIdx].eventId       = eventId;
                        pPerfCounters[counterIdx].instanceId    = 0;
                        pPerfCounters[counterIdx].instanceCount = instanceCount;
                        Snprintf(
                            &pPerfCounters[counterIdx].name[0],
                            EventInstanceNameSize,
                            "%s_INSTANCE_ALL",
                            &eventName[0]);
                        counterIdx++;
                        if (instanceCount == 0)
                        {
                            PAL_DPERROR("Bad perfcounter config (%d): Block %s not available.", lineNum, blockName);
                            result = Result::ErrorInitializationFailed;
                        }
                    }
                    else
                    {
                        char* endChar;
                        uint32 instanceId = strtol(instanceName, &endChar, 10);
                        if (endChar != instanceName)
                        {
                            // strtol succeeded
                            pPerfCounters[counterIdx].block         = block;
                            pPerfCounters[counterIdx].eventId       = eventId;
                            pPerfCounters[counterIdx].instanceId    = instanceId;
                            pPerfCounters[counterIdx].instanceCount = 1;
                            Snprintf(
                                &pPerfCounters[counterIdx].name[0],
                                EventInstanceNameSize,
                                "%s_INSTANCE_%s",
                                &eventName[0],
                                &instanceName[0]);
                            counterIdx++;
                        }
                        else
                        {
                            // strtol failed
                            PAL_DPERROR("Bad perfcounter config (%d): instanceId '%s' is not a number, EACH, or ALL.",
                                        lineNum, instanceName);
                            result = Result::ErrorInitializationFailed;
                        }
                    }
                }
                else
                {
                    // sscanf failed
                    PAL_DPERROR("Bad perfcounter config (%d): Invalid syntax or missing argument", lineNum);
                    result = Result::ErrorInitializationFailed;
                }
            }
        }
        else
        {
            // ReadLine() failed.
            // This probably means we hit the end of the file before finding the expected number of valid config
            // lines.  Probably indicates an invalid configuration file.
            PAL_DPERROR("Bad perfcounter config (%d): Unexpected end-of-file", lineNum);
            result = Result::ErrorInitializationFailed;
        }
        lineNum++;
    }

    if (result == Result::Success)
    {
        uint32 blockIdx = 0;
        // Count number of perf counters per block instace.
        // Assume the max number of instances in any block cannot exceed 256.
        uint8 count[static_cast<uint32>(GpuBlock::Count)][256] = {};

        for (uint32 i = 0; (i < numPerfCounter) && (result == Result::Success); i++)
        {
            blockIdx = static_cast<uint32>(pPerfCounters[i].block);

            uint32 blockInstanceCount = perfExpProps.blocks[blockIdx].instanceCount;
            PAL_ASSERT(blockInstanceCount <= 256);
            if (pPerfCounters[i].instanceId >= blockInstanceCount)
            {
                // The block instance ID given in the config file is out of range.
                // pPerfCounter of instance name ALL will always pass this test.
                PAL_DPERROR("PerfCounter[%s]: instanceId out of range (got %d, max %d)",
                            pPerfCounters[i].name,
                            pPerfCounters[i].instanceId,
                            blockInstanceCount);
                result = Result::ErrorInitializationFailed;
                continue;
            }

            if (pPerfCounters[i].eventId > perfExpProps.blocks[blockIdx].maxEventId)
            {
                // Invalid event ID.
                PAL_DPERROR("PerfCounter[%s]: eventId out of range (got %d, max %d)",
                            pPerfCounters[i].name,
                            pPerfCounters[i].eventId,
                            perfExpProps.blocks[blockIdx].maxEventId);
                result = Result::ErrorInitializationFailed;
                continue;
            }

            const uint32 maxCounters = perfExpProps.blocks[blockIdx].maxGlobalSharedCounters;

            for (uint32 j = 0; j < pPerfCounters[i].instanceCount; j++)
            {
                const uint32 instanceId = pPerfCounters[i].instanceId + j;

                if (++count[blockIdx][instanceId] > maxCounters)
                {
                    // Too many counters enabled for this block instance.
                    PAL_DPERROR("PerfCounter[%s]: too many counters enabled for this block", pPerfCounters[i].name);
                    result = Result::ErrorInitializationFailed;
                }
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Parse the setting-specified global perf counter config file to determine which global perf counters should be
// captured.
Result Device::InitGlobalPerfCounterState()
{
    File configFile;
    Result result = configFile.Open(
        GetPlatform()->PlatformSettings().gpuProfilerPerfCounterConfig.globalPerfCounterConfigFile,
        FileAccessRead);

    // Get performance experiment properties from the device in order to validate the requested counters.
    PerfExperimentProperties perfExpProps;
    if (result == Result::Success)
    {
        result = m_pNextLayer->GetPerfExperimentProperties(&perfExpProps);
    }

    if (result == Result::Success)
    {
        result = CountPerfCounters(&configFile, perfExpProps, &m_numGlobalPerfCounters);

        if ((result == Result::Success) && (m_numGlobalPerfCounters > 0))
        {
            m_pGlobalPerfCounters =
                PAL_NEW_ARRAY(GpuProfiler::PerfCounter, m_numGlobalPerfCounters, GetPlatform(), AllocInternal);
        }

        if (m_pGlobalPerfCounters != nullptr)
        {
            result = ExtractPerfCounterInfo(perfExpProps,
                                            &configFile,
                                            m_numGlobalPerfCounters,
                                            m_pGlobalPerfCounters);
        }
    }

    return result;
}

// =====================================================================================================================
// Reads the specified global perf counter config file to determine how many global perf counters should be enabled.
// This is for the config file with block instances specified only.
Result Device::CountPerfCounters(
    File*                           pFile,
    const PerfExperimentProperties& perfExpProps,
    uint32*                         pNumPerfCounters)
{
    Result result = Result::Success;
    constexpr size_t BufSize = 512;
    char buf[BufSize];
    size_t lineLength;
    uint32 lineNum = 1;

    // Loop through the config file.  One counter will be enabled for every line in the file that is not blank or a
    // comment.
    while (pFile->ReadLine(&buf[0], BufSize, &lineLength) == Result::Success)
    {
        buf[lineLength] = '\0';

        // Ignore blank lines or comment lines that start with a #.
        if ((buf[0] == '#') || (buf[0] == 0))
        {
            continue;
        }
        else
        {
            constexpr uint32 BlockNameSize    = 32;
            char blockName[BlockNameSize];
            uint32 eventId;
            constexpr uint32 InstanceNameSize = 8;
            char instanceName[InstanceNameSize];
            constexpr uint32 EventNameSize    = 128;
            char eventName[EventNameSize];

            // Read a line of the form "BlockName EventId InstanceName CounterName".
            const int scanfRet = sscanf(&buf[0],
                                        "%31s %u %7s %127s",
                                        &blockName[0],
                                        &eventId,
                                        &instanceName[0],
                                        &eventName[0]);

            if (scanfRet == 4)
            {
                const GpuBlock block = StringToGpuBlock(&blockName[0]);
                const uint32   blockIdx = static_cast<uint32>(block);

                if ((block != GpuBlock::Count) && perfExpProps.blocks[blockIdx].available)
                {
                    if (strcmp(instanceName, "EACH") == 0)
                    {
                        *pNumPerfCounters += perfExpProps.blocks[blockIdx].instanceCount;
                    }
                    else
                    {
                        *pNumPerfCounters += 1;
                    }
                }
                else
                {
                    // Unrecognized or unavailable block in the config file.
                    PAL_DPERROR("Bad perfcounter config (%d): Block '%s' not recognized", lineNum, blockName);
                    result = Result::ErrorInitializationFailed;
                }
            }
            else
            {
                // Malformed line in the config file.
                PAL_DPERROR("Bad perfcounter config (%d): Invalid syntax or missing argument", lineNum);
                result = Result::ErrorInitializationFailed;
            }
        }
        lineNum++;
    }

    pFile->Rewind();
    return result;
}

// =====================================================================================================================
// Configures streaming performance counters based on device support and number requested in config file.
Result Device::InitSpmTraceCounterState()
{
    Result result = Result::Success;

    File configFile;
    result = configFile.Open(GetPlatform()->PlatformSettings().gpuProfilerSpmConfig.spmPerfCounterConfigFile,
                             FileAccessRead);

    PerfExperimentProperties perfExpProps;
    if (result == Result::Success)
    {
        result = m_pNextLayer->GetPerfExperimentProperties(&perfExpProps);
    }

    if (result == Result::Success)
    {
        result = CountPerfCounters(&configFile, perfExpProps, &m_numStreamingPerfCounters);

        if ((result == Result::Success) && (m_numStreamingPerfCounters > 0))
        {
            m_pStreamingPerfCounters =
                PAL_NEW_ARRAY(GpuProfiler::PerfCounter, m_numStreamingPerfCounters, GetPlatform(), AllocInternal);
        }

        if ((result == Result::Success) && (m_pStreamingPerfCounters != nullptr))
        {
            result = ExtractPerfCounterInfo(perfExpProps,
                                            &configFile,
                                            m_numStreamingPerfCounters,
                                            m_pStreamingPerfCounters);
        }
    }

    return result;

}

// =====================================================================================================================
// Convert the specified string (e.g., "TCC") into the corresponding GpuBlock enum value or GpuBlock::Count on error.
GpuBlock StringToGpuBlock(
    const char* pString)
{
    const char* TranslationTbl[] =
    {
        "CPF",     // GpuBlock::Cpf
        "IA",      // GpuBlock::Ia
        "VGT",     // GpuBlock::Vgt
        "PA",      // GpuBlock::Pa
        "SC",      // GpuBlock::Sc
        "SPI",     // GpuBlock::Spi
        "SQ",      // GpuBlock::Sq
        "SX",      // GpuBlock::Sx
        "TA",      // GpuBlock::Ta
        "TD",      // GpuBlock::Td
        "TCP",     // GpuBlock::Tcp
        "TCC",     // GpuBlock::Tcc
        "TCA",     // GpuBlock::Tca
        "DB",      // GpuBlock::Db
        "CB",      // GpuBlock::Cb
        "GDS",     // GpuBlock::Gds
        "SRBM",    // GpuBlock::Srbm
        "GRBM",    // GpuBlock::Grbm
        "GRBM_SE", // GpuBlock::GrbmSe
        "RLC",     // GpuBlock::Rlc
        "DMA",     // GpuBlock::Dma
        "MC",      // GpuBlock::Mc
        "CPG",     // GpuBlock::Cpg
        "CPC",     // GpuBlock::Cpc
        "WD",      // GpuBlock::Wd
        "TCS",     // GpuBlock::Tcs
        "ATC",     // GpuBlock::Atc
        "ATCL2",   // GpuBlock::AtcL2
        "MCVML2",  // GpuBlock::McVmL2
        "EA",      // GpuBlock::Ea
        "RPB",     // GpuBlock::Rpb
        "RMI",     // GpuBlock::Rmi
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 424
        "UMCCH",    // GpuBlock::Umcch
#endif
    };

    static_assert(ArrayLen(TranslationTbl) == static_cast<uint32>(GpuBlock::Count),
                  "Missing entry in TranslationTbl.");

    uint32 blockIdx = 0;
    while((blockIdx < static_cast<uint32>(GpuBlock::Count)) &&
          (strcmp(TranslationTbl[blockIdx], pString) != 0))
    {
        blockIdx++;
    }

    return static_cast<GpuBlock>(blockIdx);
}

// =====================================================================================================================
// Returns true if the given pipeline info passes the SQTT hash filters.
bool Device::SqttEnabledForPipeline(
    const PipelineInfo& info,
    PipelineBindPoint   bindPoint
    ) const
{
    // All pipelines pass if filtering is disabled, otherwise we need to check the hashes.
    bool enabled = (m_sqttFilteringEnabled == false);

    if (m_sqttFilteringEnabled)
    {
        constexpr uint32 CsIdx = static_cast<uint32>(ShaderType::Compute);
        constexpr uint32 VsIdx = static_cast<uint32>(ShaderType::Vertex);
        constexpr uint32 HsIdx = static_cast<uint32>(ShaderType::Hull);
        constexpr uint32 DsIdx = static_cast<uint32>(ShaderType::Domain);
        constexpr uint32 GsIdx = static_cast<uint32>(ShaderType::Geometry);
        constexpr uint32 PsIdx = static_cast<uint32>(ShaderType::Pixel);

        // Return true if we find a non-zero matching hash.
        if ((m_sqttCompilerHash != 0) && (m_sqttCompilerHash == info.compilerHash))
        {
            enabled = true;
        }
        else if (bindPoint == PipelineBindPoint::Compute)
        {
            enabled = (ShaderHashIsNonzero(m_sqttCsHash) && ShaderHashesEqual(m_sqttCsHash, info.shader[CsIdx].hash));
        }
        else
        {
            PAL_ASSERT(bindPoint == PipelineBindPoint::Graphics);

            enabled =
                ((ShaderHashIsNonzero(m_sqttVsHash) && ShaderHashesEqual(m_sqttVsHash, info.shader[VsIdx].hash)) ||
                 (ShaderHashIsNonzero(m_sqttHsHash) && ShaderHashesEqual(m_sqttHsHash, info.shader[HsIdx].hash)) ||
                 (ShaderHashIsNonzero(m_sqttDsHash) && ShaderHashesEqual(m_sqttDsHash, info.shader[DsIdx].hash)) ||
                 (ShaderHashIsNonzero(m_sqttGsHash) && ShaderHashesEqual(m_sqttGsHash, info.shader[GsIdx].hash)) ||
                 (ShaderHashIsNonzero(m_sqttPsHash) && ShaderHashesEqual(m_sqttPsHash, info.shader[PsIdx].hash)));
        }
    }

    return enabled;
}

} // GpuProfiler
} // Pal
