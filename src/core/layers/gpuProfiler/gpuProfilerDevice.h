/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#pragma once

#include "core/layers/decorators.h"
#include "core/layers/gpuProfiler/gpuProfilerPlatform.h"
#include "g_platformSettings.h"
#include "palMutex.h"

namespace Util { class File; }

namespace Pal
{

enum class GpuBlock : uint32;

namespace GpuProfiler
{

// Forward decl's.
class TargetCmdBuffer;
struct PipelineState;

// Maximum config field widths is characters
constexpr uint32 ConfigBlockNameSize    = 32;
constexpr uint32 ConfigEventIdSize      = 8;
constexpr uint32 ConfigInstanceNameSize = 32;
constexpr uint32 ConfigEventNameSize    = 128;
constexpr uint32 ConfigOptionalDataSize = 64;
// The sum of ConfigEventNameSize and ConfigInstanceNameSize plus 10 for string "_INSTANCE_".
constexpr uint32 EventInstanceNameSize  = ConfigEventNameSize + ConfigInstanceNameSize + 10;

// Defines a single performance counter to be collected, as specified by the end-user via config file.
struct PerfCounter
{
    GpuBlock block;
    uint32   eventId;
    uint32   instanceId;
    uint32   instanceCount;
    uint64   instanceMask;    // Optional mask of Instances to select (ignored if zero)
    bool     hasOptionalData; // Block-specific optional data was read from the config file.
    uint32   optionalData;    // Block-specific optional data from the config file.
    char     name[EventInstanceNameSize];
};

// =====================================================================================================================
class Device final : public DeviceDecorator
{
public:
    Device(PlatformDecorator* pPlatform, IDevice* pNextDevice, uint32 id);

    Result ProfilingClockMode(bool enable);

    uint32 Id() const { return m_id; }

    gpusize FragmentSize() const { return m_fragmentSize; }
    size_t ColorViewSize() const { return m_colorViewSize; }
    size_t DepthViewSize() const { return m_depthViewSize; }
    uint32 BufferSrdDwords() const { return m_bufferSrdDwords; }
    uint32 ImageSrdDwords() const { return m_imageSrdDwords; }
    uint64 TimestampFreq() const { return m_timestampFreq; }
    uint32 MinTimestampAlignment(uint32 engineIdx) const { return m_minTimestampAlignment[engineIdx]; }

    uint32 NumGlobalPerfCounters() const { return m_numGlobalPerfCounters; }
    uint32 NumStreamingPerfCounters() const { return m_numStreamingPerfCounters; }
    uint32 NumDfStreamingPerfCounters() const { return m_numDfStreamingPerfCounters; }
    const PerfCounter* GlobalPerfCounters() const { return m_pGlobalPerfCounters; }
    const PerfCounter* StreamingPerfCounters() const { return m_pStreamingPerfCounters; }
    const PerfCounter* DfStreamingPerfCounters() const { return m_pDfStreamingPerfCounters; }

    GpuProfilerStallMode GetSqttStallMode() const { return m_stallMode; }
    uint32 GetSeMask() const { return m_seMask; }
    uint32 GetSqttMaxDraws() const { return m_maxDrawsForThreadTrace; }
    uint32 GetSqttCurDraws() const { return m_curDrawsForThreadTrace; }
    void   AddSqttCurDraws() { Util::AtomicIncrement(&m_curDrawsForThreadTrace); }
    bool   GetSqttAddTtvHashes() const { return m_sqttAddTtvHashes; }

    bool LoggingEnabled() const;
    bool LoggingEnabled(
        GpuProfilerGranularity granularity) const;

    bool SqttEnabledForPipeline(
        const PipelineState& state,
        PipelineBindPoint bindPoint) const;

    // Public IDevice interface methods:
    virtual Result CommitSettingsAndInit() override;
    virtual Result Cleanup() override;

    virtual size_t GetQueueSize(
        const QueueCreateInfo& createInfo,
        Result*                pResult) const override;
    virtual Result CreateQueue(
        const QueueCreateInfo& createInfo,
        void*                  pPlacementAddr,
        IQueue**               ppQueue) override;
    virtual size_t GetMultiQueueSize(
        uint32                 queueCount,
        const QueueCreateInfo* pCreateInfo,
        Result*                pResult) const override;
    virtual Result CreateMultiQueue(
        uint32                 queueCount,
        const QueueCreateInfo* pCreateInfo,
        void*                  pPlacementAddr,
        IQueue**               ppQueue) override;
    virtual size_t GetCmdBufferSize(
        const CmdBufferCreateInfo& createInfo,
        Result*                    pResult) const override;
    virtual Result CreateCmdBuffer(
        const CmdBufferCreateInfo& createInfo,
        void*                      pPlacementAddr,
        ICmdBuffer**               ppCmdBuffer) override;
    size_t GetTargetCmdBufferSize(
        const CmdBufferCreateInfo& createInfo,
        Result*                    pResult) const;
    Result CreateTargetCmdBuffer(
        const CmdBufferCreateInfo& createInfo,
        void*                      pPlacementAddr,
        TargetCmdBuffer**          ppCmdBuffer,
        uint32                     subQueueIdx);
    virtual size_t GetGraphicsPipelineSize(
        const GraphicsPipelineCreateInfo& createInfo,
        Result*                           pResult) const override;
    virtual Result CreateGraphicsPipeline(
        const GraphicsPipelineCreateInfo& createInfo,
        void*                             pPlacementAddr,
        IPipeline**                       ppPipeline) override;
    virtual size_t GetComputePipelineSize(
        const ComputePipelineCreateInfo& createInfo,
        Result*                          pResult) const override;
    virtual Result CreateComputePipeline(
        const ComputePipelineCreateInfo& createInfo,
        void*                            pPlacementAddr,
        IPipeline**                      ppPipeline) override;

    GpuProfilerMode GetProfilerMode() const { return static_cast<Platform*>(GetPlatform())->GetProfilerMode(); }

    // Returns true if the settings config has successfully requested for SQ thread trace.
    bool IsThreadTraceEnabled() const
    {
        return ((GetProfilerMode() > GpuProfilerCounterAndTimingOnly)
                && (Util::TestAnyFlagSet(GetPlatform()->PlatformSettings().gpuProfilerConfig.traceModeMask,
                                         GpuProfilerTraceSqtt))
                && (GetSeMask() > 0));
    }

    // Returns true if the settings config has successfully requested for Streaming counter trace.
    bool IsSpmTraceEnabled() const
    {
        return ((GetProfilerMode() > GpuProfilerCounterAndTimingOnly) &&
                (Util::TestAnyFlagSet(GetPlatform()->PlatformSettings().gpuProfilerConfig.traceModeMask,
                                      GpuProfilerTraceSpm)));
    }

private:
    virtual ~Device() {}

    Result InitGlobalPerfCounterState();
    Result CountPerfCounters(
        Util::File* pFile,
        const PerfExperimentProperties& perfExpProps,
        uint32* pNumPerfCounters,
        uint32* pNumDfPerfCounters);

    Result InitSpmTraceCounterState();
    Result ExtractPerfCounterInfo(
        const PerfExperimentProperties& perfExpProps,
        Util::File*                     pConfigFile,
        bool                            isSpmConfig,
        uint32                          numPerfCounter,
        PerfCounter*                    pPerfCounters,
        uint32                          numDfPerfCounter,
        PerfCounter*                    pDfPerfCounters);

    const uint32 m_id;    // Unique ID for this device for reporting purposes.
    Util::Mutex  m_mutex; // A general purpose mutex for any muti-threaded non-const functions.

    // Properties captured from the core's DeviceProperties or PalPublicSettings structure.  These are cached here to
    // avoid calling the overly expensive IDevice::GetProperties() in high frequency code paths.
    gpusize                m_fragmentSize;
    size_t                 m_colorViewSize;
    size_t                 m_depthViewSize;
    uint32                 m_bufferSrdDwords;
    uint32                 m_imageSrdDwords;
    uint64                 m_timestampFreq;
    bool                   m_logPipeStats;

    bool                   m_sqttFilteringEnabled;
    bool                   m_sqttAddTtvHashes;
    uint64                 m_sqttCompilerHash;
    ShaderHash             m_sqttTsHash;
    ShaderHash             m_sqttVsHash;
    ShaderHash             m_sqttHsHash;
    ShaderHash             m_sqttDsHash;
    ShaderHash             m_sqttGsHash;
    ShaderHash             m_sqttMsHash;
    ShaderHash             m_sqttPsHash;
    ShaderHash             m_sqttCsHash;

    uint32                 m_maxDrawsForThreadTrace;
    uint32                 m_curDrawsForThreadTrace;

    bool                   m_profilingModeEnabled;
    GpuProfilerGranularity m_profilerGranularity;
    GpuProfilerStallMode   m_stallMode;
    uint32                 m_startFrame;
    uint32                 m_endFrame;
    uint32                 m_minTimestampAlignment[EngineTypeCount];
    uint32                 m_seMask;

    // Track array of which performance counters the user has requested to capture.
    PerfCounter*           m_pGlobalPerfCounters;
    uint32                 m_numGlobalPerfCounters;

    PerfCounter* m_pStreamingPerfCounters;
    uint32       m_numStreamingPerfCounters; // Tracks number of counters requested.
    PerfCounter* m_pDfStreamingPerfCounters;
    uint32       m_numDfStreamingPerfCounters;

    // The following array is used for assigning unique IDs when the client creates multiple queues for a single engine.
    // Useful for reporting purposes.
    static constexpr uint32 MaxEngineCount = 8;
    uint32 m_queueIds[EngineTypeCount][MaxEngineCount];

    PAL_DISALLOW_DEFAULT_CTOR(Device);
    PAL_DISALLOW_COPY_AND_ASSIGN(Device);
};

} // GpuProfiler
} // Pal
