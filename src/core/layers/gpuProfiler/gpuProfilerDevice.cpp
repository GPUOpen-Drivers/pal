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

#include "core/layers/gpuProfiler/gpuProfilerCmdBuffer.h"
#include "core/layers/gpuProfiler/gpuProfilerDevice.h"
#include "core/layers/gpuProfiler/gpuProfilerPipeline.h"
#include "core/layers/gpuProfiler/gpuProfilerQueue.h"
#include "palSysUtil.h"
#include <ctype.h>
#include <limits.h>

using namespace Util;

namespace Pal
{
namespace GpuProfiler
{

static GpuBlock StringToGpuBlock(const char* pString);
static constexpr ShaderHash ZeroShaderHash = {};

/// Identifies ray-tracing-related dispatches for tools
constexpr uint64 RayTracingPsoHashPrefix = 0xEEE5FFF600000000;

// =====================================================================================================================
Device::Device(
    PlatformDecorator* pPlatform,
    IDevice*           pNextDevice,
    uint32             id)
    :
    DeviceDecorator(pPlatform, pNextDevice),
    m_id(id),
    m_fragmentSize(0),
    m_colorViewSize(0),
    m_depthViewSize(0),
    m_bufferSrdDwords(0),
    m_imageSrdDwords(0),
    m_timestampFreq(0),
    m_logPipeStats(false),
    m_sqttFilteringEnabled(false),
    m_sqttAddTtvHashes(false),
    m_sqttCompilerHash(0),
    m_maxDrawsForThreadTrace(0),
    m_curDrawsForThreadTrace(0),
    m_profilingModeEnabled(false),
    m_profilerGranularity(GpuProfilerGranularityDraw),
    m_stallMode(GpuProfilerStallAlways),
    m_startFrame(0),
    m_endFrame(0),
    m_startCommandBuffer(0),
    m_endCommandBuffer(0),
    m_pGlobalPerfCounters(nullptr),
    m_numGlobalPerfCounters(0),
    m_pStreamingPerfCounters(nullptr),
    m_numStreamingPerfCounters(0),
    m_pDfStreamingPerfCounters(nullptr),
    m_numDfStreamingPerfCounters(0)
{
    memset(m_queueIds, 0, sizeof(m_queueIds));

    m_sqttTsHash = ZeroShaderHash;
    m_sqttVsHash = ZeroShaderHash;
    m_sqttHsHash = ZeroShaderHash;
    m_sqttDsHash = ZeroShaderHash;
    m_sqttGsHash = ZeroShaderHash;
    m_sqttMsHash = ZeroShaderHash;
    m_sqttPsHash = ZeroShaderHash;
    m_sqttCsHash = ZeroShaderHash;
}

// =====================================================================================================================
Result Device::Cleanup()
{
    PAL_SAFE_DELETE_ARRAY(m_pGlobalPerfCounters, GetPlatform());
    PAL_SAFE_DELETE_ARRAY(m_pStreamingPerfCounters, GetPlatform());

    // Try to leave profiling mode if we're still in it.
    Result result = ProfilingClockMode(false);

    // We should probably clean up the next layer even if we failed above.
    return CollapseResults(result, m_pNextLayer->Cleanup());
}

// =====================================================================================================================
// Track recorded command buffer count across all queues. Thread-safe
void Device::CommandBufferBegin()
{
    AtomicIncrement(&m_commandBufferCount);
}

// =====================================================================================================================
// Determines if logging is currently enabled, either due to the current frame range or because the user hit Shift-F11
// to force this frame to be captured.
bool Device::LoggingEnabled() const
{
    const Platform& platform = *static_cast<const Platform*>(m_pPlatform);
    const uint32    frameId  = platform.FrameId();

    // Command buffer count may not deterministically identify command buffers for measurement. Command buffers can be
    // recorded in parallel across multiple queues' submissions leading to different ordering for which command buffers
    // occur first. Because this only impacts profiling data, the risk is permissible.
    return (platform.IsLoggingForced() || ((frameId >= m_startFrame) && (frameId < m_endFrame)) ||
            ((m_commandBufferCount >= m_startCommandBuffer) && (m_commandBufferCount < m_endCommandBuffer)));
}

// =====================================================================================================================
// Determines if logging is currently enabled for the specified granularity.
bool Device::LoggingEnabled(
    GpuProfilerGranularity granularity
    ) const
{
    return ((m_profilerGranularity == granularity) && LoggingEnabled());
}

// =====================================================================================================================
Result Device::CommitSettingsAndInit()
{
    // Update the public settings before we commit them.
    PalPublicSettings* const pInitialSettings = GetPublicSettings();

    // Force off the command allocator wait-on-submit optimization for embedded data. The profiler permits the client
    // to read and write to client embedded data in the replayed command buffers which breaks this optimization.
    //
    // This is actually a violation of PAL's residency rules because a command buffer must only reference allocations
    // from its command allocator, allocations made resident using AddGpuMemoryReferences or allocations on the
    // per-submit residency list. Unfortunately we must break these rules in order to support a record/replay layer.
    // We won't need to disable this optimization if we rewrite the GPU profiler to instrument the client commands.
    pInitialSettings->cmdAllocResidency &= ~(CmdAllocResWaitOnSubmitEmbeddedData |
                                             CmdAllocResWaitOnSubmitLargeEmbeddedData);

    Result result = DeviceDecorator::CommitSettingsAndInit();

    const PalPlatformSettings& settings = GetPlatform()->PlatformSettings();

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
        m_stallMode            = settings.gpuProfilerSqttConfig.stallBehavior;

        m_sqttTsHash.upper = settings.gpuProfilerSqttConfig.tsHashHi;
        m_sqttTsHash.lower = settings.gpuProfilerSqttConfig.tsHashLo;
        m_sqttVsHash.upper = settings.gpuProfilerSqttConfig.vsHashHi;
        m_sqttVsHash.lower = settings.gpuProfilerSqttConfig.vsHashLo;
        m_sqttHsHash.upper = settings.gpuProfilerSqttConfig.hsHashHi;
        m_sqttHsHash.lower = settings.gpuProfilerSqttConfig.hsHashLo;
        m_sqttDsHash.upper = settings.gpuProfilerSqttConfig.dsHashHi;
        m_sqttDsHash.lower = settings.gpuProfilerSqttConfig.dsHashLo;
        m_sqttGsHash.upper = settings.gpuProfilerSqttConfig.gsHashHi;
        m_sqttGsHash.lower = settings.gpuProfilerSqttConfig.gsHashLo;
        m_sqttMsHash.upper = settings.gpuProfilerSqttConfig.msHashHi;
        m_sqttMsHash.lower = settings.gpuProfilerSqttConfig.msHashLo;
        m_sqttPsHash.upper = settings.gpuProfilerSqttConfig.psHashHi;
        m_sqttPsHash.lower = settings.gpuProfilerSqttConfig.psHashLo;
        m_sqttCsHash.upper = settings.gpuProfilerSqttConfig.csHashHi;
        m_sqttCsHash.lower = settings.gpuProfilerSqttConfig.csHashLo;

        m_sqttFilteringEnabled = ((m_sqttCompilerHash != 0) ||
                                  ShaderHashIsNonzero(m_sqttTsHash) ||
                                  ShaderHashIsNonzero(m_sqttVsHash) ||
                                  ShaderHashIsNonzero(m_sqttHsHash) ||
                                  ShaderHashIsNonzero(m_sqttDsHash) ||
                                  ShaderHashIsNonzero(m_sqttGsHash) ||
                                  ShaderHashIsNonzero(m_sqttMsHash) ||
                                  ShaderHashIsNonzero(m_sqttPsHash) ||
                                  ShaderHashIsNonzero(m_sqttCsHash));
        m_sqttAddTtvHashes     = settings.gpuProfilerSqttConfig.addTtvHashes;

        m_profilerGranularity = settings.gpuProfilerConfig.granularity;

        m_maxDrawsForThreadTrace = settings.gpuProfilerSqttConfig.maxDraws;
        m_curDrawsForThreadTrace = 0;

        m_startFrame         = settings.gpuProfilerConfig.startFrame;
        m_endFrame           = m_startFrame + settings.gpuProfilerConfig.frameCount;
        m_startCommandBuffer = settings.gpuProfilerConfig.startCommandBuffer;
        m_endCommandBuffer   = m_startCommandBuffer + settings.gpuProfilerConfig.commandBufferCount;

        m_32BitSpm = settings.gpuProfilerPerfCounterConfig.use32BitSpmCounters;

        for (uint32 i = 0; i < EngineTypeCount; i++)
        {
            m_minTimestampAlignment[i] = info.engineProperties[i].minTimestampAlignment;
        }
    }

    if (result == Result::Success)
    {
        m_colorViewSize = GetColorTargetViewSize(&result);
    }

    if (result == Result::Success)
    {
        m_depthViewSize = GetDepthStencilViewSize(&result);
    }

    if (result == Result::Success)
    {
        // Create directory for log files.
        if (GetPlatform()->CreateLogDir(settings.gpuProfilerConfig.logDirectory) != Result::Success)
        {
            GPUPROFILER_WARN("Failed to create folder '%s'", settings.gpuProfilerConfig.logDirectory);
        }
        static_cast<Platform*>(m_pPlatform)->CreateLogger();

        GPUPROFILER_INFO("Setting GpuProfilerMode:%d",                      settings.gpuProfilerMode);
        GPUPROFILER_INFO("Setting GpuProfilerConfig.Granularity:%d",        settings.gpuProfilerConfig.granularity);
        GPUPROFILER_INFO("Setting GpuProfilerSqttConfig.QueueIdMask:%08x",  settings.gpuProfilerConfig.queueIdMask);
        GPUPROFILER_INFO("Setting GpuProfilerSqttConfig.SEMask:%08x",       settings.gpuProfilerSqttConfig.seMask);
    }

    if ((result == Result::Success)                                                    &&
        (settings.gpuProfilerMode == GpuProfilerMode::GpuProfilerCounterAndTimingOnly) &&
        (settings.gpuProfilerPerfCounterConfig.globalPerfCounterConfigFile[0] != '\0'))
    {
        result = InitGlobalPerfCounterState();
        PAL_ASSERT(result == Result::Success);
    }

    if ((result == Result::Success)                                               &&
        ((settings.gpuProfilerMode == GpuProfilerMode::GpuProfilerTraceEnabledTtv)  ||
         (settings.gpuProfilerMode == GpuProfilerMode::GpuProfilerTraceEnabledRgp)) &&
        (settings.gpuProfilerSpmConfig.spmPerfCounterConfigFile[0] != '\0'))
    {
        result = InitSpmTraceCounterState();
        PAL_ASSERT(result == Result::Success);
    }

    if ((result == Result::Success) && LoggingEnabled())
    {
        // Start out in profiling mode if the user has already enabled logging. This is probably because they set
        // the start frame to 0. This is expected if we're profiling an application with no present calls.
        result = ProfilingClockMode(true);
    }

    return result;
}

// =====================================================================================================================
// Sets the device engine and memory clocks to the configurable profiling clock mode. Restored on false.
Result Device::ProfilingClockMode(
    bool enable)
{
    Result result = Result::Success;

    const GpuProfilerClockMode profilerMode = GetPlatform()->PlatformSettings().gpuProfilerConfig.clockMode;

    // We assume the clocks are already in the default mode when profiling is disabled. If we've been asked to run
    // default clocks when profiling then that's the same as never changing the clock mode in the first place.
    if (profilerMode != GpuProfilerClockModeDefault)
    {
        MutexAuto lock(&m_mutex);

        if (m_profilingModeEnabled != enable)
        {
            m_profilingModeEnabled = enable;

            constexpr DeviceClockMode ModeMap[5] =
            {
                DeviceClockMode::Default,       // GpuProfilerClockModeDefault
                DeviceClockMode::Profiling,     // GpuProfilerClockModeStable
                DeviceClockMode::Peak,          // GpuProfilerClockModePeak
                DeviceClockMode::MinimumMemory, // GpuProfilerClockModeMinMemory
                DeviceClockMode::MinimumEngine, // GpuProfilerClockModeMinEngine
            };

            PAL_ASSERT(profilerMode < ArrayLen(ModeMap));

            SetClockModeInput clockModeInput = {};
            clockModeInput.clockMode = enable ? ModeMap[profilerMode] : DeviceClockMode::Default;

            result = SetClockMode(clockModeInput, nullptr);

            // If the user sets the NeverChangeClockMode setting we'll get ErrorUnavailable. We shouldn't treat this as
            // an actual error so that profiling can continue. It would be better to check the setting directly but it's
            // an internal setting so we can't read it in the layer code.
            if (result == Result::ErrorUnavailable)
            {
                result = Result::Success;
            }
        }
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
                                                         1,
                                                         queueId);

        result = pQueue->Init(&createInfo);
        if (result != Result::Success)
        {
            pQueue->Destroy();
        }
    }

    if (result == Result::Success)
    {
        (*ppQueue) = pQueue;

        uint32 seq = UINT_MAX;

        if (createInfo.engineType == EngineTypeUniversal)
        {
            seq = static_cast<Platform*>(m_pPlatform)->GetUniversalQueueSequenceNumber();
        }

        // Start logging immediately on Frame 0 for the first universal queue created
        if (LoggingEnabled(GpuProfilerGranularityFrame) && (seq == 0))
        {
            pQueue->BeginNextFrame(true);
        }
    }

    return result;
}

// =====================================================================================================================
size_t Device::GetMultiQueueSize(
    uint32                 queueCount,
    const QueueCreateInfo* pCreateInfo,
    Result*                pResult
    ) const
{
    return m_pNextLayer->GetMultiQueueSize(queueCount, pCreateInfo, pResult) + sizeof(Queue);
}

// =====================================================================================================================
Result Device::CreateMultiQueue(
    uint32                 queueCount,
    const QueueCreateInfo* pCreateInfo,
    void*                  pPlacementAddr,
    IQueue**               ppQueue)
{
    IQueue* pNextQueue = nullptr;
    Queue*  pQueue = nullptr;

    Result result = m_pNextLayer->CreateMultiQueue(
                                        queueCount,
                                        pCreateInfo,
                                        NextObjectAddr<Queue>(pPlacementAddr),
                                        &pNextQueue);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextQueue != nullptr);
        PAL_ASSERT(pCreateInfo[0].engineIndex < MaxEngineCount);
        pNextQueue->SetClientData(pPlacementAddr);

        const uint32 masterQueueId = m_queueIds[pCreateInfo[0].engineType][pCreateInfo[0].engineIndex];

        for (uint32 i = 0; i < queueCount; i++)
        {
            m_queueIds[pCreateInfo[i].engineType][pCreateInfo[i].engineIndex]++;
        }

        pQueue = PAL_PLACEMENT_NEW(pPlacementAddr) Queue(pNextQueue,
                                                         this,
                                                         queueCount,
                                                         masterQueueId);

        result = pQueue->Init(pCreateInfo);
        if (result != Result::Success)
        {
            pQueue->Destroy();
        }
    }

    if (result == Result::Success)
    {
        (*ppQueue) = pQueue;

        uint32 seq = UINT_MAX;

        if (pCreateInfo[0].engineType == EngineTypeUniversal)
        {
            seq = static_cast<Platform*>(m_pPlatform)->GetUniversalQueueSequenceNumber();
        }

        // Start logging immediately on Frame 0 for the first universal queue created
        if (LoggingEnabled(GpuProfilerGranularityFrame) && (seq == 0))
        {
            pQueue->BeginNextFrame(true);
        }
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

    CmdBufferCreateInfo nextCreateInfo = createInfo;
    nextCreateInfo.pCmdAllocator = NextCmdAllocator(createInfo.pCmdAllocator);

    Result result = m_pNextLayer->CreateCmdBuffer(nextCreateInfo,
                                                  NextObjectAddr<CmdBuffer>(pPlacementAddr),
                                                  &pNextCmdBuffer);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextCmdBuffer != nullptr);
        pNextCmdBuffer->SetClientData(pPlacementAddr);

        (*ppCmdBuffer) = PAL_PLACEMENT_NEW(pPlacementAddr) CmdBuffer(pNextCmdBuffer,
                                                                     this,
                                                                     createInfo,
                                                                     m_logPipeStats);
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
    TargetCmdBuffer**          ppCmdBuffer,
    uint32                     subQueueIdx)
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

        pCmdBuffer = PAL_PLACEMENT_NEW(pPlacementAddr) TargetCmdBuffer(createInfo, pNextCmdBuffer, this, subQueueIdx);
        result = pCmdBuffer->Init();
        if (result != Result::Success)
        {
            pCmdBuffer->Destroy();
        }
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
    Result result = CallNextCreateGraphicsPipeline(createInfo,
                                                   NextObjectAddr<Pipeline>(pPlacementAddr),
                                                   &pNextPipeline);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextPipeline != nullptr);
        pNextPipeline->SetClientData(pPlacementAddr);

        pPipeline = PAL_PLACEMENT_NEW(pPlacementAddr) Pipeline(pNextPipeline, this);
        result = pPipeline->InitGfx();
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
// A helper function which converts a C-string to upper case characters.
static void ToUpperCase(
    char* pString)
{
    const size_t len = strlen(pString);

    for (size_t idx = 0; idx < len; ++idx)
    {
        pString[idx] = static_cast<char>(toupper(pString[idx]));
    }
}

// =====================================================================================================================
// A helper function which converts a C-string to unsigned integer.
static bool StrToUInt(
    char*   pString,
    uint32* pIntOut)
{
    bool   validInt = false;
    uint32 value    = 0;
    if ((pString[0] == '0') && ((pString[1] == 'x') || (pString[1] == 'X'))) // hex value
    {
        char* endChar;
        value = strtol(pString + 2, &endChar, 16);
        if (endChar != (pString + 2))
        {
            // strtol success
            validInt = true;
        }
    }
    else // decimal value
    {
        char* endChar;
        value = strtol(pString, &endChar, 10);
        if (endChar != pString)
        {
            // strtol success
            validInt = true;
        }
    }
    if (validInt)
    {
        *pIntOut = value;
    }
    return validInt;
}

// =====================================================================================================================
// Helper function to Validate and Set perf counter info which had been extracted from the config file.
static void SetPerfCounterInfo(
    char*                         eventName,
    GpuBlock                      block,
    const GpuBlockPerfProperties& blockPerfProps,
    uint32                        eventId,
    uint32                        instanceId,
    uint32                        instanceCount,
    uint64                        instanceMask,
    bool                          hasOptionalData,
    uint32                        optionalData,
    PerfCounter*                  pPerfCounterOut)
{
    // block specific validations
    if (block == GpuBlock::DfMall)
    {
        // Unlike other perf blocks the DF sub-blocks may not support enough counters to sample all instances.
        const uint32 maxCounters = blockPerfProps.maxGlobalOnlyCounters;
        if (instanceMask != 0)
        {
            // A mask selecting instances was specified in config.
            // We'll only select up to the maximum number of instances possible.
            uint32 selectedInstanceCount = 0;
            uint64 instanceBit = 1;
            for (uint32 i = 0; i < instanceCount; i++, instanceBit <<= 1)
            {
                if ((instanceMask & instanceBit) != 0)
                {
                    selectedInstanceCount++;
                    if (selectedInstanceCount >= maxCounters)
                    {
                        // clear all higher bits for which are instances beyond the maximum
                        instanceMask &= ((instanceBit << 1) - 1);
                        break;
                    }
                }
            }
        }
        else if (instanceCount > maxCounters)
        {
            // there are more instances than counters for capturing, so limit the count to max
            instanceCount = maxCounters;
        }
    }

    // general validation, and construct the counter field name with appended instance identifier
    char strInstances[32] = {0};
    if (instanceCount <= 1)
    {
        // This is not a multi-instance sampling config like "ALL" or "MASK".
        // Note if instanceCount==0, there's nothing to sample and leave it to the caller to throw error as needed.
        instanceMask = 0;  // any multi-instance mask specified is pointless, so just clear it
        Snprintf(strInstances, sizeof(strInstances), "%u", instanceId);
    }
    else if (instanceMask != 0)
    {
        // A 64-bit mask was specified in the config (i.e. 0xFF00 to select only instances 8-15)
        if ((instanceId + instanceCount) < 64)
        {
            // Clear all of the mask's higher bits for instances beyond the last (id + instanceCount).
            // Technically this should be not necessary because logic elsewhere does not look past the last, however
            // we'll do this for extra clarity and also because we write the mask value in counter name below.
            instanceMask &= ((1llu << (instanceId + instanceCount)) - 1);
        }
        // the field name will reflect the actual mask for instances profiled
        Snprintf(strInstances, sizeof(strInstances), "MASK0x%X", instanceMask);
    }
    else
    {
        // we are capturing ALL instances
        Snprintf(strInstances, sizeof(strInstances), "ALL");
    }
    Snprintf(
        &pPerfCounterOut->name[0],
        EventInstanceNameSize,
        "%s_INSTANCE_%s",
        &eventName[0], strInstances);

    // populate the PerfCounter info
    pPerfCounterOut->block           = block;
    pPerfCounterOut->eventId         = eventId;
    pPerfCounterOut->instanceId      = instanceId;
    pPerfCounterOut->instanceCount   = instanceCount;
    pPerfCounterOut->instanceMask    = instanceMask;
    pPerfCounterOut->hasOptionalData = hasOptionalData;
    pPerfCounterOut->optionalData    = optionalData;
}

// =====================================================================================================================
// Helper function to extract SPM or global perf counter info from config file.
Result Device::ExtractPerfCounterInfo(
    const PerfExperimentProperties& perfExpProps,
    File*                           pConfigFile,
    bool                            isSpmConfig,
    uint32                          numPerfCounter,
    PerfCounter*                    pPerfCounters,
    uint32                          numDfPerfCounter,
    PerfCounter*                    pDfPerfCounters)
{
    Result result = Result::Success;
    uint32 counterIdx   = 0;
    uint32 dfCounterIdx = 0;
    uint32*      pIndexPointer   = nullptr;
    PerfCounter* pCounterPointer = nullptr;
    uint32 lineNum = 1;
    while (((counterIdx + dfCounterIdx) < (numPerfCounter + numDfPerfCounter)) && (result == Result::Success))
    {
        constexpr size_t BufSize = 512;
        char buf[BufSize];
        size_t lineLength;

        if (pConfigFile->ReadLine(&buf[0], BufSize, &lineLength) == Result::Success)
        {
            buf[lineLength] = '\0';

            const char* pFirstChar = &buf[0];
            while (isspace(*pFirstChar))
            {
                // Skip leading whitespace
                pFirstChar++;
            }

            // Ignore blank lines or comment lines that start with a #/;.
            if ((*pFirstChar == '#') || (*pFirstChar == ';') || (*pFirstChar == 0))
            {
                continue;
            }
            else
            {
                char  blockName[ConfigBlockNameSize];
                char  eventIdStr[ConfigEventIdSize];
                char  instanceName[ConfigInstanceNameSize];
                char  eventName[ConfigEventNameSize];
                char  optDataStr[ConfigOptionalDataSize];

                // Read a line of the form "BlockName EventId InstanceName CounterName OptionalData".
                const int scanfRet = sscanf(&buf[0],
                                            "%31s %7s %31s %127s %63s",
                                            &blockName[0],
                                            &eventIdStr[0],
                                            &instanceName[0],
                                            &eventName[0],
                                            &optDataStr[0]);

                // We expect our block names and instance names to be upper case.
                ToUpperCase(blockName);
                ToUpperCase(instanceName);

                if (scanfRet >= 4)
                {
                    const GpuBlock block = StringToGpuBlock(&blockName[0]);
                    const uint32 blockIdx = static_cast<uint32>(block);
                    bool hasOptionalData = false;
                    uint32 optionalData = 0;

                    // Convert eventIdStr(decimal or hex) to eventId integer
                    uint32 eventId;
                    if (StrToUInt(eventIdStr, &eventId) == false)
                    {
                        GPUPROFILER_ERROR("Bad perfcounter config (%d): eventId '%s' is not a valid number.",
                                    lineNum, eventIdStr);
                        result = Result::ErrorInitializationFailed;
                    }

                    if (result == Result::Success)
                    {
                        // Parse Optional data field, if specified
                        if (scanfRet >= 5)
                        {
                            // Convert optDataStr(decimal or hex) to integer
                            //   Could be extended to parse other specialized field formatting in future.
                            if (StrToUInt(optDataStr, &optionalData))
                            {
                                hasOptionalData = true;
                            }
                            else
                            {
                                GPUPROFILER_ERROR("Bad perfcounter config (%d): OptData '%s' is not a number (ignored).",
                                            lineNum, optDataStr);
                            }
                        }
                    }

                    // If we have a DF trace, then use the DF index and counter list
                    if ((block == GpuBlock::DfMall) && isSpmConfig)
                    {
                        pIndexPointer   = &dfCounterIdx;
                        pCounterPointer = pDfPerfCounters;
                    }
                    else
                    {
                        pIndexPointer   = &counterIdx;
                        pCounterPointer = pPerfCounters;
                    }

                    if (result == Result::Success)
                    {
                        // Handle instanceName(EACH, ALL, MAX, EVEN, ODD, MASK:<mask>, or number)
                        if (strcmp(instanceName, "EACH") == 0)
                        {
                            const uint32 instanceCount = perfExpProps.blocks[blockIdx].instanceCount;
                            for (uint32 i = 0; i < instanceCount; i++)
                            {
                                SetPerfCounterInfo(eventName,
                                                   block,
                                                   perfExpProps.blocks[blockIdx],
                                                   eventId,
                                                   i, // instanceId
                                                   1, // instanceCount: each captured count is for a single instance
                                                   0, // instanceMask: unused
                                                   hasOptionalData,
                                                   optionalData,
                                                   &pCounterPointer[*pIndexPointer]);
                                (*pIndexPointer)++;
                            }
                        }
                        else if (strcmp(instanceName,  "ALL")  == 0  ||
                                 strcmp(instanceName,  "MAX")  == 0  ||
                                 strcmp(instanceName,  "EVEN") == 0  ||
                                 strcmp(instanceName,  "ODD")  == 0  ||
                                 strncmp(instanceName, "MASK:", 5) == 0)
                        {
                            // multiple instances are summed into one counter result
                            uint32 instanceCount     = perfExpProps.blocks[blockIdx].instanceCount;

                            // The bits in "instanceMask" are iterated through from 0(lsb) to "instanceCount" and
                            // only those instances with a bit set are captured and summed into final result.
                            // See SetPerfCounterInfo() for any special mask validation logic.
                            uint64 instanceMask      = 0;
                            if (strcmp(instanceName, "EVEN") == 0)
                            {
                                instanceMask = 0x5555555555555555llu;
                            }
                            else if (strcmp(instanceName, "ODD") == 0)
                            {
                                instanceMask = 0xAAAAAAAAAAAAAAAAllu;
                            }
                            else if (strcmp(instanceName, "MAX") == 0)
                            {
                                instanceMask = 0xFFFFFFFFFFFFFFFFllu;
                            }
                            else if (strncmp(instanceName, "MASK:", 5) == 0)
                            {
                                uint32 mask;
                                if (StrToUInt(instanceName + 5, &mask) == true)
                                {
                                    instanceMask = mask;
                                }
                                else
                                {
                                    GPUPROFILER_ERROR("Bad perfcounter config (%d): Instance MASK '%s' is not valid.",
                                                lineNum, instanceName);
                                }
                            }

                            SetPerfCounterInfo(eventName,
                                               block,
                                               perfExpProps.blocks[blockIdx],
                                               eventId,
                                               0, // instanceId: capture count from 0->instanceCount, unless mask overrides
                                               instanceCount,
                                               instanceMask,
                                               hasOptionalData,
                                               optionalData,
                                               &pCounterPointer[*pIndexPointer]);
                            if (pCounterPointer[*pIndexPointer].instanceCount == 0)
                            {
                                GPUPROFILER_ERROR("Bad perfcounter config (%d): Block %s not available.",
                                    lineNum, blockName);
                                result = Result::ErrorInitializationFailed;
                            }
                            (*pIndexPointer)++;
                        }
                        else
                        {
                            uint32 instanceId;
                            if (StrToUInt(instanceName, &instanceId) == true)
                            {
                                SetPerfCounterInfo(eventName,
                                                   block,
                                                   perfExpProps.blocks[blockIdx],
                                                   eventId,
                                                   instanceId,
                                                   1, // instanceCount: capture single count for "instanceId"
                                                   0, // instanceMask: unused
                                                   hasOptionalData,
                                                   optionalData,
                                                   &pCounterPointer[*pIndexPointer]);
                                (*pIndexPointer)++;
                            }
                            else
                            {
                                GPUPROFILER_ERROR("Bad perfcounter config (%d): instanceId '%s' is invalid number/keyword.",
                                            lineNum, instanceName);
                                result = Result::ErrorInitializationFailed;
                            }
                        }
                    }
                }
                else
                {
                    // sscanf failed
                    GPUPROFILER_ERROR("Bad perfcounter config (%d): Invalid syntax or missing argument", lineNum);
                    result = Result::ErrorInitializationFailed;
                }
            }
        }
        else
        {
            // ReadLine() failed.
            // This probably means we hit the end of the file before finding the expected number of valid config
            // lines.  Probably indicates an invalid configuration file.
            GPUPROFILER_ERROR("Bad perfcounter config (%d): Unexpected end-of-file", lineNum);
            result = Result::ErrorInitializationFailed;
        }
        lineNum++;
    }

    if (result == Result::Success)
    {
        // Count number of perf counters per block and per instance group.
        // Assume the max number of instance groups in any block cannot exceed 256.
        constexpr uint32 MaxGroups = 256;
        uint8 count[static_cast<uint32>(GpuBlock::Count)][MaxGroups] = {};

        for (uint32 i = 0; i < numPerfCounter; i++)
        {
            const uint32 blockIdx           = static_cast<uint32>(pPerfCounters[i].block);
            const uint32 blockInstanceCount = perfExpProps.blocks[blockIdx].instanceCount;
            const uint32 blockGroupSize     = perfExpProps.blocks[blockIdx].instanceGroupSize;
            const uint32 blockGroupCount    = blockInstanceCount / blockGroupSize;

            PAL_ASSERT(blockGroupCount <= MaxGroups);

            if (pPerfCounters[i].instanceId >= blockInstanceCount)
            {
                // The block instance ID given in the config file is out of range.
                // pPerfCounter of instance name ALL will always pass this test.
                GPUPROFILER_ERROR("PerfCounter[%s]: instanceId out of range (got %d, max %d)",
                            pPerfCounters[i].name,
                            pPerfCounters[i].instanceId,
                            blockInstanceCount);
                result = Result::ErrorInitializationFailed;
                break;
            }

            if (pPerfCounters[i].eventId > perfExpProps.blocks[blockIdx].maxEventId)
            {
                // Invalid event ID.
                GPUPROFILER_ERROR("PerfCounter[%s]: eventId out of range (got %d, max %d)",
                            pPerfCounters[i].name,
                            pPerfCounters[i].eventId,
                            perfExpProps.blocks[blockIdx].maxEventId);
                result = Result::ErrorInitializationFailed;
                break;
            }

            // The maximum number of counters depends on whether this is an SPM file or a global counters file.
            const uint32 maxCounters = isSpmConfig ? perfExpProps.blocks[blockIdx].maxSpmCounters
                                                   : perfExpProps.blocks[blockIdx].maxGlobalSharedCounters;

            const uint64 instanceMask = pPerfCounters[i].instanceMask;

            for (uint32 j = 0; j < pPerfCounters[i].instanceCount; j++)
            {
                if ((instanceMask == 0) || Util::BitfieldIsSet(instanceMask, j))
                {
                    const uint32 instanceId = pPerfCounters[i].instanceId + j;
                    const uint32 groupId    = instanceId / blockGroupSize;

                    if (++count[blockIdx][groupId] > maxCounters)
                    {
                        // Too many counters enabled for this block or instance.
                        GPUPROFILER_ERROR("PerfCounter[%s]: too many counters enabled for this block (count %u > max %u)",
                                    pPerfCounters[i].name, count[blockIdx][instanceId], maxCounters);
                        result = Result::ErrorInitializationFailed;
                        break;
                    }
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
        result = CountPerfCounters(&configFile, perfExpProps, &m_numGlobalPerfCounters, nullptr);

        if ((result == Result::Success) && (m_numGlobalPerfCounters > 0))
        {
            m_pGlobalPerfCounters =
                PAL_NEW_ARRAY(GpuProfiler::PerfCounter, m_numGlobalPerfCounters, GetPlatform(), AllocInternal);
        }

        if (m_pGlobalPerfCounters != nullptr)
        {
            result = ExtractPerfCounterInfo(perfExpProps,
                                            &configFile,
                                            false,
                                            m_numGlobalPerfCounters,
                                            m_pGlobalPerfCounters,
                                            0,
                                            nullptr);
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
    uint32*                         pNumPerfCounters,
    uint32*                         pNumDfPerfCounters)
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

        const char* pFirstChar = &buf[0];
        while (isspace(*pFirstChar))
        {
            // Skip leading whitespace
            pFirstChar++;
        }

        // Ignore blank lines or comment lines that start with a #/;.
        if ((*pFirstChar == '#') || (*pFirstChar == ';') || (*pFirstChar == 0))
        {
            continue;
        }
        else
        {
            char  blockName[ConfigBlockNameSize];
            char  eventIdStr[ConfigEventIdSize];
            char  instanceName[ConfigInstanceNameSize];
            char  eventName[ConfigEventNameSize];

            // Read a line of the form "BlockName EventId InstanceName CounterName" (ignore OptionalData field+)
            const int scanfRet = sscanf(&buf[0],
                                        "%31s %7s %31s %127s",
                                        &blockName[0],
                                        &eventIdStr[0],
                                        &instanceName[0],
                                        &eventName[0]);

            // We expect our block names and instance names to be upper case.
            ToUpperCase(blockName);
            ToUpperCase(instanceName);

            if (scanfRet == 4)
            {
                const GpuBlock block = StringToGpuBlock(&blockName[0]);
                const uint32   blockIdx = static_cast<uint32>(block);

                if ((block != GpuBlock::Count) && perfExpProps.blocks[blockIdx].available)
                {
                    if ((block == GpuBlock::DfMall) && (pNumDfPerfCounters != nullptr))
                    {
                        if (strcmp(instanceName, "EACH") == 0)
                        {
                            *pNumDfPerfCounters += perfExpProps.blocks[blockIdx].instanceCount;
                        }
                        else
                        {
                            *pNumDfPerfCounters += 1;
                        }
                    }
                    else
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
                }
                else
                {
                    // Unrecognized or unavailable block in the config file.
                    GPUPROFILER_ERROR("Bad perfcounter config (%d): Block '%s' not recognized", lineNum, blockName);
                    result = Result::ErrorInitializationFailed;
                }
            }
            else
            {
                // Malformed line in the config file.
                GPUPROFILER_ERROR("Bad perfcounter config (%d): Invalid syntax or missing argument", lineNum);
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

    File         configFile;
    uint32       numCounters     = 0;
    uint32       numDfCounters   = 0;
    PerfCounter* pDfPerfCounters = nullptr;
    PerfCounter* pPerfCounters   = nullptr;

    result = configFile.Open(GetPlatform()->PlatformSettings().gpuProfilerSpmConfig.spmPerfCounterConfigFile,
                             FileAccessRead);

    PerfExperimentProperties perfExpProps;
    if (result == Result::Success)
    {
        result = m_pNextLayer->GetPerfExperimentProperties(&perfExpProps);
    }

    if (result == Result::Success)
    {
        result = CountPerfCounters(&configFile, perfExpProps, &numCounters, &numDfCounters);

        if ((result == Result::Success) && (numCounters > 0))
        {
            pPerfCounters =
                PAL_NEW_ARRAY(GpuProfiler::PerfCounter, numCounters, GetPlatform(), AllocInternal);
        }

        if ((result == Result::Success) && (numDfCounters > 0))
        {
            pDfPerfCounters =
                PAL_NEW_ARRAY(GpuProfiler::PerfCounter, numDfCounters, GetPlatform(), AllocInternal);
        }

        if ((result == Result::Success)                        &&
            (((numCounters > 0) && (pPerfCounters != nullptr)) ||
             ((numDfCounters > 0) && (pDfPerfCounters != nullptr))))
        {
            result = ExtractPerfCounterInfo(perfExpProps,
                                            &configFile,
                                            true,
                                            numCounters,
                                            pPerfCounters,
                                            numDfCounters,
                                            pDfPerfCounters);
        }
    }

    m_numDfStreamingPerfCounters = numDfCounters;
    m_pDfStreamingPerfCounters   = pDfPerfCounters;
    m_numStreamingPerfCounters   = numCounters;
    m_pStreamingPerfCounters     = pPerfCounters;

    // Draw granularity is not supported with DF SPM becaue the enable/disable
    // flag is submitted per command buffer.
    if ((m_numDfStreamingPerfCounters > 0) &&
        LoggingEnabled(GpuProfilerGranularityDraw))
    {
        result = Result::ErrorInitializationFailed;
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
        "UMCCH",   // GpuBlock::Umcch
        "GE",      // GpuBlock::Ge
        "GL1A",    // GpuBlock::GL1A
        "GL1C",    // GpuBlock::GL1C
        "GL1CG",   // GpuBlock::GL1CG
        "GL2A",    // GpuBlock::GL2A
        "GL2C",    // GpuBlock::GL2C
        "CHA",     // GpuBlock::Cha
        "CHC",     // GpuBlock::Chc
        "CHCG",    // GpuBlock::Chcg
        "GUS",     // GpuBlock::Gus
        "GCR",     // GpuBlock::Gcr
        "PH",      // GpuBlock::Ph
        "UTCL1",   // GpuBlock::UtcL1
        "GE_DIST", // GpuBlock::GeDist
        "GE_SE",   // GpuBlock::GeSe
        "DF_MALL", // GpuBlock::DfMall
        "SQWGP",   // GpuBlock::SqWgp
        "PC",      // GpuBlock::Pc
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
    const PipelineState& state,
    PipelineBindPoint    bindPoint
    ) const
{
    // All pipelines pass if filtering is disabled, otherwise we need to check the hashes.
    bool enabled = (m_sqttFilteringEnabled == false);

    const PipelineInfo& info = state.pipelineInfo;

    if (m_sqttFilteringEnabled)
    {
        constexpr uint32 CsIdx = static_cast<uint32>(ShaderType::Compute);
        constexpr uint32 TsIdx = static_cast<uint32>(ShaderType::Task);
        constexpr uint32 VsIdx = static_cast<uint32>(ShaderType::Vertex);
        constexpr uint32 HsIdx = static_cast<uint32>(ShaderType::Hull);
        constexpr uint32 DsIdx = static_cast<uint32>(ShaderType::Domain);
        constexpr uint32 GsIdx = static_cast<uint32>(ShaderType::Geometry);
        constexpr uint32 MsIdx = static_cast<uint32>(ShaderType::Mesh);
        constexpr uint32 PsIdx = static_cast<uint32>(ShaderType::Pixel);

        const auto& settings = GetPlatform()->PlatformSettings();

        uint64 hashForComparison = info.internalPipelineHash.stable;

        if (settings.gpuProfilerSqttConfig.pipelineHashAsApiPsoHash == true)
        {
            hashForComparison = state.apiPsoHash;
        }

        // Return true if we find a non-zero matching hash.
        if (m_sqttCompilerHash != 0)
        {
            if (m_sqttCompilerHash == hashForComparison)
            {
                enabled = true;
            }
            else if ((hashForComparison != Pal::InternalApiPsoHash) &&
                     (m_sqttCompilerHash == RayTracingPsoHashPrefix) &&
                     ((m_sqttCompilerHash & hashForComparison) == RayTracingPsoHashPrefix))
            {
                enabled = true;
            }
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
                 (ShaderHashIsNonzero(m_sqttTsHash) && ShaderHashesEqual(m_sqttTsHash, info.shader[TsIdx].hash)) ||
                 (ShaderHashIsNonzero(m_sqttMsHash) && ShaderHashesEqual(m_sqttMsHash, info.shader[MsIdx].hash)) ||
                 (ShaderHashIsNonzero(m_sqttPsHash) && ShaderHashesEqual(m_sqttPsHash, info.shader[PsIdx].hash)));
        }
    }

    return enabled;
}

} // GpuProfiler
} // Pal
