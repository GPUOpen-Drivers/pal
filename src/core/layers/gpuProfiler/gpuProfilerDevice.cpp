/*
 *******************************************************************************
 *
 * Copyright (c) 2015-2017 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

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
    m_numGlobalPerfCounters(0)
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

    // The residency flags must map to the command allocator types.
    static_assert((1 << CommandDataAlloc)  == CmdAllocResWaitOnSubmitCommandData,  "CmdAllocRes enum mismatch.");
    static_assert((1 << EmbeddedDataAlloc) == CmdAllocResWaitOnSubmitEmbeddedData, "CmdAllocRes enum mismatch.");

    // Force off the command allocator wait-on-submit optimization for embedded data. The profiler permits the client
    // to read and write to client embedded data in the replayed command buffers which breaks this optimization.
    //
    // This is actually a violation of PAL's residency rules because a command buffer must only reference allocations
    // from its command allocator, allocations made resident using AddGpuMemoryReferences or allocations on the
    // per-submit residency list. Unfortunately we must break these rules in order to support a record/replay layer.
    // We won't need to disable this optimization if we rewrite the GPU profiler to instrument the client commands.
    pInitialSettings->cmdAllocResidency &= ~CmdAllocResWaitOnSubmitEmbeddedData;

    Result result = DeviceDecorator::CommitSettingsAndInit();

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
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 298
        m_sqttCompilerHash    = (static_cast<uint64>(settings.gpuProfilerSqttPipelineHashLo) |
                                 (static_cast<uint64>(settings.gpuProfilerSqttPipelineHashHi) << 32));
#else
        m_sqttCompilerHash    = settings.gpuProfilerSqttPipelineHash;
#endif

        // TODO: Add support for 128-bit hashes
        m_sqttVsHash.lower = (static_cast<uint64>(settings.gpuProfilerSqttVsHashLo) |
                             (static_cast<uint64>(settings.gpuProfilerSqttVsHashHi) << 32));
        m_sqttHsHash.lower = (static_cast<uint64>(settings.gpuProfilerSqttHsHashLo) |
                             (static_cast<uint64>(settings.gpuProfilerSqttHsHashHi) << 32));
        m_sqttDsHash.lower = (static_cast<uint64>(settings.gpuProfilerSqttDsHashLo) |
                             (static_cast<uint64>(settings.gpuProfilerSqttDsHashHi) << 32));
        m_sqttGsHash.lower = (static_cast<uint64>(settings.gpuProfilerSqttGsHashLo) |
                             (static_cast<uint64>(settings.gpuProfilerSqttGsHashHi) << 32));
        m_sqttPsHash.lower = (static_cast<uint64>(settings.gpuProfilerSqttPsHashLo) |
                             (static_cast<uint64>(settings.gpuProfilerSqttPsHashHi) << 32));
        m_sqttCsHash.lower = (static_cast<uint64>(settings.gpuProfilerSqttCsHashLo) |
                             (static_cast<uint64>(settings.gpuProfilerSqttCsHashHi) << 32));

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

        const bool enableSqtt = (GetProfilerMode() > GpuProfilerSqttOff);

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
size_t Device::GetShaderSize(
    const ShaderCreateInfo& createInfo,
    Result*                 pResult
    ) const
{
    return m_pNextLayer->GetShaderSize(createInfo, pResult) + sizeof(Shader);
}

// =====================================================================================================================
Result Device::CreateShader(
    const ShaderCreateInfo& createInfo,
    void*                   pPlacementAddr,
    IShader**               ppShader
    ) const
{

    IShader* pNextShader = nullptr;
    Shader*  pShader     = nullptr;

    Result result = m_pNextLayer->CreateShader(createInfo,
                                               NextObjectAddr<Shader>(pPlacementAddr),
                                               &pNextShader);

    if (result == Result::Success)
    {
        PAL_ASSERT(pNextShader != nullptr);
        pNextShader->SetClientData(pPlacementAddr);

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 357
        const bool hasPerformanceData = (createInfo.optStrategy.flags.enabledPerformanceData != 0);
#else
        const bool hasPerformanceData = false;
#endif

        pShader = PAL_PLACEMENT_NEW(pPlacementAddr) Shader(pNextShader, this, hasPerformanceData);
    }

    if (result == Result::Success)
    {
        (*ppShader) = pShader;
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

    GraphicsPipelineCreateInfo nextCreateInfo = createInfo;
    nextCreateInfo.vs.pShader   = NextShader(createInfo.vs.pShader);
    nextCreateInfo.hs.pShader   = NextShader(createInfo.hs.pShader);
    nextCreateInfo.ds.pShader   = NextShader(createInfo.ds.pShader);
    nextCreateInfo.gs.pShader   = NextShader(createInfo.gs.pShader);
    nextCreateInfo.ps.pShader   = NextShader(createInfo.ps.pShader);
    nextCreateInfo.pShaderCache = NextShaderCache(createInfo.pShaderCache);

    Result result = m_pNextLayer->CreateGraphicsPipeline(nextCreateInfo,
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

    ComputePipelineCreateInfo nextCreateInfo = createInfo;
    nextCreateInfo.cs.pShader   = NextShader(createInfo.cs.pShader);
    nextCreateInfo.pShaderCache = NextShaderCache(createInfo.pShaderCache);

    Result result = m_pNextLayer->CreateComputePipeline(nextCreateInfo,
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
        m_numGlobalPerfCounters = CountGlobalPerfCounters(&configFile);

        if (m_numGlobalPerfCounters > 0)
        {
            m_pGlobalPerfCounters =
                PAL_NEW_ARRAY(GlobalPerfCounter, m_numGlobalPerfCounters, GetPlatform(), AllocInternal);
        }

        uint32 counterIdx = 0;
        while ((counterIdx < m_numGlobalPerfCounters) && (result == Result::Success))
        {
            constexpr size_t BufSize = 512;
            char buf[BufSize];
            size_t lineLength;

            if (configFile.ReadLine(&buf[0], BufSize, &lineLength) == Result::Success)
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
                                                &m_pGlobalPerfCounters[counterIdx].eventId,
                                                &m_pGlobalPerfCounters[counterIdx].name[0]);

                    if (scanfRet == 3)
                    {
                        const GpuBlock block    = StringToGpuBlock(&blockName[0]);
                        const uint32   blockIdx = static_cast<uint32>(block);

                        if ((block != GpuBlock::Count) && perfExpProps.blocks[blockIdx].available)
                        {
                            m_pGlobalPerfCounters[counterIdx].block         = block;
                            m_pGlobalPerfCounters[counterIdx].instanceCount =
                                perfExpProps.blocks[blockIdx].instanceCount;
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
    }

    if (result == Result::Success)
    {
        // Counts how many counters are enabled per hardware block.
        uint32 count[static_cast<size_t>(GpuBlock::Count)] = { };

        for (uint32 i = 0; (i < m_numGlobalPerfCounters) && (result == Result::Success); i++)
        {
            const uint32 blockIdx = static_cast<uint32>(m_pGlobalPerfCounters[i].block);

            if (++count[blockIdx] > perfExpProps.blocks[blockIdx].maxGlobalSharedCounters)
            {
                // Too many counters enabled for this block.
                result = Result::ErrorInitializationFailed;
            }
            else if (m_pGlobalPerfCounters[i].eventId > perfExpProps.blocks[blockIdx].maxEventId)
            {
                // Invalid event ID.
                result = Result::ErrorInitializationFailed;
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Reads the specified global perf counter config file to determine how many global perf counters should be enabled.
uint32 Device::CountGlobalPerfCounters(
    File* pFile)
{
    uint32 numGlobalPerfCounters = 0;

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
            numGlobalPerfCounters++;
        }
    }

    pFile->Rewind();
    return numGlobalPerfCounters;
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
