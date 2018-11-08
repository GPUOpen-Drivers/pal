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

#pragma once

#include "core/perfExperiment.h"
#include "core/hw/gfxip/gfx6/gfx6PerfCtrInfo.h"
#include "palHashMap.h"

namespace Pal
{
namespace Gfx6
{

class CmdStream;
class Device;

// =====================================================================================================================
// Enumerates the possible "usage" states of a single performance counter resource. Each counter can be shared between 1
// 64-bit summary counter, or up to 4 16-bit streaming counters.
enum PerfCtrUseStatus: uint32
{
    PerfCtrEmpty            = 0,    ///< Perf counter resource is unused
    PerfCtr64BitSummary     = 1,    ///< Perf counter resource is used for a 64bit summary ctr
    PerfCtr16BitStreaming1  = 2,    ///< Perf counter resource is used for 1 16bit streaming ctr
    PerfCtr16BitStreaming2  = 3,    ///< Perf counter resource is used for 2 16bit streaming ctrs
    PerfCtr16BitStreaming3  = 4,    ///< Perf counter resource is used for 3 16bit streaming ctrs
    PerfCtr16BitStreaming4  = 5,    ///< Perf counter resource is used for 4 16bit streaming ctrs
};

// =====================================================================================================================
// Structure defining the performance counter resource usage for a GPU block.
struct PerfCtrBlockUsage
{
    struct
    {
        /// Usage status for each counter belonging to this particular instance.
        PerfCtrUseStatus  counter[Gfx6MaxCountersPerBlock];
    }  instance[PerfCtrInfo::MaxNumBlockInstances];   ///< Usage status for each instance of this GPU block
};

// =====================================================================================================================
// Flags describing the configuration of the performance counters within an Experiment.
union CounterFlags
{
    struct
    {
        uint32 indexedBlocks  :  1; ///< If set, one or more ctr's are on an indexed GPU block
        uint32 mcCounters     :  1; ///< If set, MC counters are present
        uint32 rlcCounters    :  1; ///< If set, RLC counters are present
        uint32 sqCounters     :  1; ///< If set, SQ counters are present
        uint32 srbmCounters   :  1; ///< If set, SRBM counters are present
        uint32 taCounters     :  1; ///< If set, TA counters are present
        uint32 tdCounters     :  1; ///< If set, TD counters are present
        uint32 tcpCounters    :  1; ///< If set, TCP counters are present
        uint32 tccCounters    :  1; ///< If set, TCC counters are present
        uint32 tcaCounters    :  1; ///< If set, TCA counters are present
        uint32 reserved       : 22; ///< Reserved bits
    };
    uint32 u32All; ///< Value of the flags bitfield
};

// =====================================================================================================================
// Provides GCN-specific behavior for perf experiment objects.
class PerfExperiment : public Pal::PerfExperiment
{
public:
    PerfExperiment(const Device* pDevice, const PerfExperimentCreateInfo& createInfo);

    Result Init();

    virtual void IssueBegin(Pal::CmdStream* pPalCmdStream) const override;
    virtual void UpdateSqttTokenMask(Pal::CmdStream*               pCmdStream,
                                     const ThreadTraceTokenConfig& sqttTokenConfig) const override;
    virtual void IssueEnd(Pal::CmdStream* pPalCmdStream) const override;

    void BeginInternalOps(CmdStream* pCmdStream) const;
    void EndInternalOps(CmdStream* pCmdStream) const;

protected:
    virtual ~PerfExperiment() {}

    virtual Result CreateCounter(const PerfCounterInfo& info, Pal::PerfCounter** ppCounter) override;
    virtual Result CreateThreadTrace(const ThreadTraceInfo& info) override;

    Result ConstructSpmTraceObj(const SpmTraceCreateInfo& info, Pal::SpmTrace** ppSpmTrace) override;
    Pal::StreamingPerfCounter* CreateStreamingPerfCounter(GpuBlock block, uint32 instance, uint32 slot) override;
    void UpdateCounterFlags(GpuBlock block, bool isIndexed) override;
    PAL_INLINE uint32 GetNumStreamingCounters(uint32 block) override
    {
       return m_device.Parent()->ChipProperties().gfx6.perfCounterInfo.block[block].numStreamingCounterRegs;
    }

private:
    Result ReserveCounterResource(const PerfCounterInfo& info, uint32* pCounterId, uint32* pCounterSubId);

    void IssuePause(CmdStream* pCmdStream) const;
    void IssueResume(CmdStream* pCmdStream) const;

    uint32* WriteSetupPerfCounters(CmdStream* pCmdStream, uint32* pCmdSpace) const;
    uint32* WriteStartPerfCounters(bool restart, CmdStream* pCmdStream, uint32* pCmdSpace) const;
    uint32* WriteStopPerfCounters(bool reset, CmdStream* pCmdStream, uint32* pCmdSpace) const;
    uint32* WriteSamplePerfCounters(gpusize baseGpuVirtAddr, CmdStream* pCmdStream, uint32* pCmdSpace) const;

    uint32* WriteResetGrbmGfxIndex(CmdStream* pCmdStream, uint32* pCmdSpace) const;
    uint32* WriteWaitIdleClean(bool cacheFlush, bool forComputeEngine, uint32* pCmdSpace) const;

    // Returns true if one or more perf counters are on an indexed GPU block.
    bool HasIndexedCounters() const { return m_counterFlags.indexedBlocks; }

    // Returns true if MC counters are present.
    bool HasMcCounters() const { return m_counterFlags.mcCounters; }

    // Returns true if RLC counters are present.
    bool HasRlcCounters() const { return m_counterFlags.rlcCounters; }

    // Returns true if SQ counters are present.
    bool HasSqCounters() const { return m_counterFlags.sqCounters; }

    // Returns true if SRBM counters are present.
    bool HasSrbmCounters() const { return m_counterFlags.srbmCounters; }

    // Returns true if TA counters are present.
    bool HasTaCounters() const { return m_counterFlags.taCounters; }

    // Returns true if TD counters are present.
    bool HasTdCounters() const { return m_counterFlags.tdCounters; }

    // Returns true if TCP counters are present.
    bool HasTcpCounters() const { return m_counterFlags.tcpCounters; }

    // Returns true if TCC counters are present.
    bool HasTccCounters() const { return m_counterFlags.tccCounters; }

    // Returns true if TCA counters are present.
    bool HasTcaCounters() const { return m_counterFlags.tcaCounters; }

    const Device&          m_device;
    CounterFlags           m_counterFlags;   // Flags describing the set of perf counters (summary and SPM counters
                                             // share these flags).

    // Performance counter usage status for currently used GPU blocks.
    Util::HashMap<BlockUsageKey, PerfCtrUseStatus, Platform> m_blockUsageMap;

    regSQ_PERFCOUNTER_CTRL m_sqPerfCounterCtrl; // Pre-compute SQ_PERFCOUNTER_CTRL register

    PAL_DISALLOW_DEFAULT_CTOR(PerfExperiment);
    PAL_DISALLOW_COPY_AND_ASSIGN(PerfExperiment);
};

} // Gfx6
} // Pal
