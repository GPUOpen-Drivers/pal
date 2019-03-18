/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/gpuMemory.h"
#include "palDeque.h"
#include "palPerfExperiment.h"

namespace Pal
{

// Forward decl's
class CmdBuffer;
class Device;
class PerfCounter;
class Platform;
class StreamingPerfCounter;
class SpmTrace;
class ThreadTrace;

// =====================================================================================================================
// Key for PerfCtrBlockUsage Map
struct BlockUsageKey
{
    GpuBlock block;
    uint32   instance; // Device wide instance number. (Eg.: Tahiti has 12 instances of TCC block).
    uint32   counter;
};

// =====================================================================================================================
// Flags representing the properties of a PerfExperiment object.
union PerfExperimentFlags
{
    struct
    {
        uint32  cacheFlushOnPerfCounter  :  1; // If set, the experiment flushes caches when collecting performance
                                               // counter data.
        uint32  sampleInternalOperations :  1; // If set, the experiment samples during internal operations like
                                               // blts/clears/etc.
        uint32  isFinalized              :  1; // If set, the experiment has been finalized.
        uint32  reserved                 : 29; // Reserved for future use
    };
    uint32 u32All;
};

// =====================================================================================================================
// Core implementation of the IPerfExperiment interface.
class PerfExperiment : public IPerfExperiment
{
public:
    virtual Result AddCounter(
        const PerfCounterInfo& counterInfo) override;

    virtual Result GetGlobalCounterLayout(
        GlobalCounterLayout* pLayout) const override;

    virtual Result AddThreadTrace(
        const ThreadTraceInfo& info) override;

    virtual Result GetThreadTraceLayout(
        ThreadTraceLayout* pLayout) const override;

    virtual Result AddSpmTrace(
        const SpmTraceCreateInfo& counterInfo) override;

    virtual Result GetSpmTraceLayout(
        SpmTraceLayout* pLayout) const override;

    virtual Result Finalize() override;

    virtual void GetGpuMemoryRequirements(
        GpuMemoryRequirements* pGpuMemReqs) const override;

    virtual Result BindGpuMemory(
        IGpuMemory* pGpuMemory,
        gpusize     offset) override;

    virtual void IssueBegin(CmdStream* pCmdStream) const = 0;
    virtual void UpdateSqttTokenMask(
        CmdStream*                    pCmdStream,
        const ThreadTraceTokenConfig& sqttTokenConfig) const = 0;
    virtual void IssueEnd(CmdStream* pCmdStream) const = 0;

    virtual void BeginInternalOps(CmdStream* pCmdStream) const = 0;
    virtual void EndInternalOps(CmdStream* pCmdStream) const = 0;

    virtual void Destroy() override;

protected:
    PerfExperiment(Device* pDevice, const PerfExperimentCreateInfo& info);
    virtual ~PerfExperiment();

    virtual Result CreateCounter(const PerfCounterInfo& info, PerfCounter** ppCounter) = 0;
    virtual Result CreateThreadTrace(const ThreadTraceInfo& info) = 0;

    virtual Result ConstructSpmTraceObj(const SpmTraceCreateInfo& info, SpmTrace** ppSpmTrace) = 0;
    virtual StreamingPerfCounter* CreateStreamingPerfCounter(GpuBlock block, uint32 instance, uint32 slot) = 0;
    virtual void UpdateCounterFlags(GpuBlock block, bool isIndexed) = 0;
    virtual uint32 GetNumStreamingCounters(uint32 block) = 0;

    // Creates an Spm trace object that will be managed by this perf experiment.
    Result CreateSpmTrace(const SpmTraceCreateInfo& info);

    // Returns true if the Experiment issues a cache-flush when sampling perf counters.
    bool CacheFlushOnPerfCounter() const { return m_flags.cacheFlushOnPerfCounter; }

    // Returns true if the Experiment samples internal operations like blts/clears/etc.
    bool SampleInternalOperations() const { return m_flags.sampleInternalOperations; }

    // Returns the shader mask for this Experiment.
    PerfExperimentShaderFlags ShaderMask() const { return m_shaderMask; }

    // Returns true if the Experiment is in the 'Finalized' state.
    bool IsFinalized() const { return (m_flags.isFinalized != 0); }

    // Returns true if the Experiment has any global counters.
    bool HasGlobalCounters() const { return (m_globalCtrs.NumElements() > 0); }

    // Returns true if the Experiment has any thread traces.
    bool HasThreadTraces() const { return (m_numThreadTrace > 0); }

    bool HasSpmTrace() const { return (m_pSpmTrace != nullptr); }

    const PerfExperimentCreateInfo m_info;
    BoundGpuMemory                 m_vidMem;

    gpusize m_ctrBeginOffset;   // GPU mem offset to ctr begin samples
    gpusize m_ctrEndOffset;     // GPU mem offset to ctr end samples
    gpusize m_thdTraceOffset;   // GPU mem offset to thread trace data
    gpusize m_spmTraceOffset;   // GPU mem offset to SPM trace data
    gpusize m_totalMemSize;     // Total GPU memory size

    Util::Deque<PerfCounter*, Platform> m_globalCtrs; //  List of global performance counters

    // Maximum number of thread traces allowed per Experiment: one per Shader Engine.
    static const size_t MaxNumThreadTrace = 4;

    // Array of pointers to thread trace objects for each Shader Engine.
    ThreadTrace*            m_pThreadTrace[MaxNumThreadTrace];
    size_t                  m_numThreadTrace;                   // Number of active thread traces

    SpmTrace*               m_pSpmTrace; // The spm trace can have multiple perf counter instances added to it.
                                         // A single perf experiment can have one spm trace active at a time.

private:
    Result ValidatePerfCounterInfo(const PerfCounterInfo& info) const;

    // Performance Experiment GPU memory alignment requirement:
    static constexpr gpusize PerfExperimentAlignment = 4096;

    const Device&             m_device;
    PerfExperimentFlags       m_flags;
    PerfExperimentShaderFlags m_shaderMask;

    PAL_DISALLOW_DEFAULT_CTOR(PerfExperiment);
    PAL_DISALLOW_COPY_AND_ASSIGN(PerfExperiment);
};

} // Pal
