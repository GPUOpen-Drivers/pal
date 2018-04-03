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
#include "core/layers/gpuProfiler/gpuProfilerShader.h"
#include "palSysUtil.h"

using namespace Util;

namespace Pal
{
namespace GpuProfiler
{

static GpuBlock StringToGpuBlock(const char* pString);

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
    m_startFrame(0),
    m_endFrame(0),
    m_pGlobalPerfCounters(nullptr),
    m_numGlobalPerfCounters(0),
    m_pStreamingPerfCounters(nullptr),
    m_numStreamingPerfCounters(0)
{
    memset(m_queueIds, 0, sizeof(m_queueIds));

    constexpr ShaderHash ZeroShaderHash = {};
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

    if (result == Result::Success)
    {
        result = UpdateSettings();
    }

    const Platform& platform = *static_cast<const Platform*>(GetPlatform());
    const auto&     settings = ProfilerSettings();

    // Capture properties and settings needed elsewhere in the GpuProfiler layer.
    DeviceProperties info;
    if (result == Result::Success)
    {
        result = m_pNextLayer->GetProperties(&info);
    }

    if (result == Result::Success)
    {
        m_fragmentSize        = info.gpuMemoryProperties.fragmentSize;
        m_bufferSrdDwords     = info.gfxipProperties.srdSizes.bufferView / sizeof(uint32);
        m_imageSrdDwords      = info.gfxipProperties.srdSizes.imageView / sizeof(uint32);
        m_timestampFreq       = info.timestampFrequency;
        m_logPipeStats        = settings.gpuProfilerRecordPipelineStats;
        m_sqttCompilerHash    = settings.gpuProfilerSqttPipelineHash;

        m_sqttVsHash = settings.gpuProfilerSqttVsHash;
        m_sqttHsHash = settings.gpuProfilerSqttHsHash;
        m_sqttDsHash = settings.gpuProfilerSqttDsHash;
        m_sqttGsHash = settings.gpuProfilerSqttGsHash;
        m_sqttPsHash = settings.gpuProfilerSqttPsHash;
        m_sqttCsHash = settings.gpuProfilerSqttCsHash;

        m_sqttFilteringEnabled = ((m_sqttCompilerHash != 0)         ||
                                  ShaderHashIsNonzero(m_sqttVsHash) ||
                                  ShaderHashIsNonzero(m_sqttHsHash) ||
                                  ShaderHashIsNonzero(m_sqttDsHash) ||
                                  ShaderHashIsNonzero(m_sqttGsHash) ||
                                  ShaderHashIsNonzero(m_sqttPsHash) ||
                                  ShaderHashIsNonzero(m_sqttCsHash));

        m_profilerGranularity = settings.gpuProfilerGranularity;

        m_maxDrawsForThreadTrace = settings.gpuProfilerSqttMaxDraws;
        m_curDrawsForThreadTrace = 0;

        m_startFrame          = settings.gpuProfilerStartFrame;
        m_endFrame            = m_startFrame + settings.gpuProfilerFrameCount;

        for (uint32 i = 0; i < EngineTypeCount; i++)
        {
            m_minTimestampAlignment[i] = info.engineProperties[i].minTimestampAlignment;
        }
    }

    // Create directory for log files.
    if (result == Result::Success)
    {
        // Try to create the root log directory specified in settings first, which may already exist.
        const Result tmpResult = MkDir(settings.gpuProfilerLogDirectory);
        result = (tmpResult == Result::AlreadyExists) ? Result::Success: tmpResult;
    }

    if (result == Result::Success)
    {
        // Create the sub-directory for this app run using the name generated by the platform.  This may also exist
        // already, in an MGPU configuration.
        char logDirPath[1024];
        Snprintf(&logDirPath[0], sizeof(logDirPath), "%s/%s", settings.gpuProfilerLogDirectory, platform.LogDirName());
        const Result tmpResult = MkDir(&logDirPath[0]);
        result = (tmpResult == Result::AlreadyExists) ? Result::Success: tmpResult;
    }

    if ((result == Result::Success) && (settings.gpuProfilerGlobalPerfCounterConfigFile[0] != '\0'))
    {
        result = InitGlobalPerfCounterState();
        PAL_ASSERT(result == Result::Success);
    }

    if ((result == Result::Success) && (settings.gpuProfilerSpmPerfCounterConfigFile[0] != '\0'))
    {
        result = InitSpmTraceCounterState();
        PAL_ASSERT(result == Result::Success);
    }

    return result;
}

// =====================================================================================================================
Result Device::UpdateSettings()
{
    memset(m_profilerSettings.gpuProfilerLogDirectory, 0, 512);
    strncpy(m_profilerSettings.gpuProfilerLogDirectory, "/tmp/amdpal/", 512);
    m_profilerSettings.gpuProfilerStartFrame = 0;
    m_profilerSettings.gpuProfilerFrameCount = 0;
    m_profilerSettings.gpuProfilerRecordPipelineStats = false;
    memset(m_profilerSettings.gpuProfilerGlobalPerfCounterConfigFile, 0, 256);
    strncpy(m_profilerSettings.gpuProfilerGlobalPerfCounterConfigFile, "", 256);
    m_profilerSettings.gpuProfilerGlobalPerfCounterPerInstance = false;
    m_profilerSettings.gpuProfilerBreakSubmitBatches = false;
    m_profilerSettings.gpuProfilerCacheFlushOnCounterCollection = false;
    m_profilerSettings.gpuProfilerGranularity = GpuProfilerGranularityDraw;
    m_profilerSettings.gpuProfilerSqThreadTraceTokenMask = 0xFFFF;
    m_profilerSettings.gpuProfilerSqttPipelineHash = 0;

    constexpr ShaderHash ZeroShaderHash = {};
    m_profilerSettings.gpuProfilerSqttVsHash = ZeroShaderHash;
    m_profilerSettings.gpuProfilerSqttHsHash = ZeroShaderHash;
    m_profilerSettings.gpuProfilerSqttDsHash = ZeroShaderHash;
    m_profilerSettings.gpuProfilerSqttGsHash = ZeroShaderHash;
    m_profilerSettings.gpuProfilerSqttPsHash = ZeroShaderHash;
    m_profilerSettings.gpuProfilerSqttCsHash = ZeroShaderHash;
    m_profilerSettings.gpuProfilerSqttMaxDraws = 0;
    m_profilerSettings.gpuProfilerSqttBufferSize = 1048576;

    // Spm trace config.
    memset(m_profilerSettings.gpuProfilerSpmPerfCounterConfigFile, 0, 256);
    m_profilerSettings.gpuProfilerSpmTraceBufferSize = 1048576;
    m_profilerSettings.gpuProfilerSpmTraceInterval   = 4096;

    // Temporarily override the hard coded setting with the copy of the layer settings the core layer has initialized.
    const auto coreLayerSettings = GetGpuProfilerSettings();
    m_profilerSettings = coreLayerSettings;

    return Result::Success;
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
// Helper function to extract perf counter info from config file and store in member variable according to
// PerfCounterType.
Result Device::ExtractPerfCounterInfo(
    const PerfExperimentProperties& perfExpProps,
    const PerfCounterType&          type,
    const uint32                    numCounters,
    File*                           pConfigFile,
    PerfCounter*                    pPerfCounters)
{
    Result result = Result::Success;
     uint32 counterIdx = 0;
        while ((counterIdx < numCounters) && (result == Result::Success))
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
                    constexpr uint32 BlockNameSize = 32;
                    char blockName[BlockNameSize];

                    // Read a line of the form "BlockName EventId CounterName".
                    const int scanfRet = sscanf(&buf[0],
                                                "%31s %u %127s",
                                                &blockName[0],
                                                &pPerfCounters[counterIdx].eventId,
                                                &pPerfCounters[counterIdx].name[0]);

                    if (scanfRet == 3)
                    {
                        const GpuBlock block    = StringToGpuBlock(&blockName[0]);
                        const uint32   blockIdx = static_cast<uint32>(block);

                        if ((block != GpuBlock::Count) && perfExpProps.blocks[blockIdx].available)
                        {
                            pPerfCounters[counterIdx].block         = block;
                            pPerfCounters[counterIdx].instanceCount = perfExpProps.blocks[blockIdx].instanceCount;
                            counterIdx++;
                        }
                        else
                        {
                            // Unrecognized or unavailable block in the config file.
                            result = Result::ErrorInitializationFailed;
                        }
                    }
                    else
                    {
                        // Malformed line in the config file.
                        result = Result::ErrorInitializationFailed;
                    }
                }
            }
            else
            {
                // This probably means we hit the end of the file before finding the expected number of valid config
                // lines.  Probably indicates an invalid configuration file.
                result = Result::ErrorInitializationFailed;
            }
        }

    if (result == Result::Success)
    {
        // Counts how many counters are enabled per hardware block.
        uint32 count[static_cast<size_t>(GpuBlock::Count)] = { };

        for (uint32 i = 0; (i < numCounters) && (result == Result::Success); i++)
        {
            const uint32 blockIdx = static_cast<uint32>(pPerfCounters[i].block);

            const uint32 maxCounters = (type == PerfCounterType::Global) ?
                                       perfExpProps.blocks[blockIdx].maxGlobalSharedCounters :
                                       perfExpProps.blocks[blockIdx].maxSpmCounters;

            if (++count[blockIdx] > maxCounters)
            {
                // Too many counters enabled for this block.
                result = Result::ErrorInitializationFailed;
            }
            else if (pPerfCounters[i].eventId > perfExpProps.blocks[blockIdx].maxEventId)
            {
                // Invalid event ID.
                result = Result::ErrorInitializationFailed;
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
    Result result = configFile.Open(ProfilerSettings().gpuProfilerGlobalPerfCounterConfigFile, FileAccessRead);

    // Get performance experiment properties from the device in order to validate the requested counters.
    PerfExperimentProperties perfExpProps;
    if (result == Result::Success)
    {
        result = m_pNextLayer->GetPerfExperimentProperties(&perfExpProps);
    }

    if (result == Result::Success)
    {
        m_numGlobalPerfCounters = CountPerfCounters(&configFile);

        if (m_numGlobalPerfCounters > 0)
        {
            m_pGlobalPerfCounters =
                PAL_NEW_ARRAY(GpuProfiler::PerfCounter, m_numGlobalPerfCounters, GetPlatform(), AllocInternal);
        }

        if (m_pGlobalPerfCounters != nullptr)
        {
            result = ExtractPerfCounterInfo(perfExpProps,
                                            PerfCounterType::Global,
                                            m_numGlobalPerfCounters,
                                            &configFile,
                                            m_pGlobalPerfCounters);
        }
    }

    return result;
}

// =====================================================================================================================
// Reads the specified global perf counter config file to determine how many global perf counters should be enabled.
uint32 Device::CountPerfCounters(
    File* pFile)
{
    uint32 numPerfCounters = 0;

    constexpr size_t BufSize = 512;
    char buf[BufSize];
    size_t lineLength;

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
            numPerfCounters++;
        }
    }

    pFile->Rewind();
    return numPerfCounters;
}

// =====================================================================================================================
// Configures streaming performance counters based on device support and number requested in config file.
Result Device::InitSpmTraceCounterState()
{
    Result result = Result::Success;

    File configFile;
    result = configFile.Open(ProfilerSettings().gpuProfilerSpmPerfCounterConfigFile, FileAccessRead);

    PerfExperimentProperties perfExpProps;
    if (result == Result::Success)
    {
        result = m_pNextLayer->GetPerfExperimentProperties(&perfExpProps);
    }

    if (result == Result::Success)
    {
        m_numStreamingPerfCounters = CountPerfCounters(&configFile);

        if (m_numStreamingPerfCounters > 0)
        {
            m_pStreamingPerfCounters =
                PAL_NEW_ARRAY(GpuProfiler::PerfCounter, m_numStreamingPerfCounters, GetPlatform(), AllocInternal);
        }

        if (m_pStreamingPerfCounters != nullptr)
        {
            result = ExtractPerfCounterInfo(perfExpProps,
                                            PerfCounterType::Spm,
                                            m_numStreamingPerfCounters,
                                            &configFile,
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
    };

    static_assert((sizeof(TranslationTbl) / sizeof(TranslationTbl[0])) == static_cast<uint32>(GpuBlock::Count),
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
