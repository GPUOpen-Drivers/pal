/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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
/**
***********************************************************************************************************************
* @file  palGpaSession.h
* @brief PAL GPU utility GpaSession class.
***********************************************************************************************************************
*/

#pragma once

#include "palCmdBuffer.h"
#include "palGpaSession.h"
#include "palGpuMemory.h"
#include "palGpuUtil.h"
#include "palMemTrackerImpl.h"
#include "palSysMemory.h"
#include "palSysUtil.h"
#include "sqtt_file_format.h"

// Forward declarations.
namespace Pal
{
    class  ICmdAllocator;
    class  ICmdBuffer;
    class  IDevice;
    class  IGpuMemory;
    class  IPerfExperiment;
    struct GlobalCounterLayout;
    struct ThreadTraceLayout;
    enum   HwPipePoint : uint32;
}

namespace GpuUtil
{

// Internal structure used to track each sample in the GpaSession.
struct GpaSession::SampleItem
{
    GpaSampleConfig       sampleConfig;
    Pal::IPerfExperiment* pPerfExperiment;
    PerfExperimentMemory  perfMemInfo;
    PerfSample*           pPerfSample;
};

// =====================================================================================================================
// Helper classes to store pointers related to various types of perfExperiment operations.
// PerfSamples are used internally by the PerfItem in GpaSession to manage programming of the PerfExeriments.
class GpaSession::PerfSample
{

public:
    virtual ~PerfSample() {}

    void WriteCopySampleData(Pal::ICmdBuffer* pCmdBuffer);
    void SetCopySampleMemInfo(Pal::IGpuMemory* pSrcGpuMem, Pal::gpusize srcOffset);
    void SetSampleMemoryProperties(const GpuMemoryInfo& pGpuMemory, Pal::gpusize offset, Pal::gpusize buffersize);

    GpuMemoryInfo    GetSampleDataGpuMem()     const { return m_sampleDataGpuMemoryInfo; }
    Pal::gpusize     GetGcSampleDataOffset()   const { return m_gcSampleDataOffset; }
    Pal::gpusize     GetGcSampleDataSize()     const { return m_pGcSampleDataBufferSize; }
    void*            GetPerfExpResults()             { return m_pPerfExpResults; }
    const void*      GetPerfExpResults()       const { return m_pPerfExpResults; }
    Pal::IGpuMemory* GetCopySampleDataGpuMem()       { return m_pCopySampleGpuMem; }
    Pal::gpusize     GetCopySampleDataOffset() const { return m_copySampleOffset; }

    const SampleTraceApiInfo& GetSampleTraceApiInfo() const { return m_traceApiInfo; }
    void SetSampleTraceApiInfo(SampleTraceApiInfo traceApiInfo) { m_traceApiInfo = traceApiInfo; }

protected:
    PerfSample(
        Pal::IDevice*         pDevice,
        Pal::IPerfExperiment* pPerfExperiment,
        GpaAllocator*         pAllocator)
        :
        m_pDevice(pDevice),
        m_pAllocator(pAllocator),
        m_pPerfExperiment(pPerfExperiment),
        m_sampleDataGpuMemoryInfo(),
        m_gcSampleDataOffset(),
        m_pGcSampleDataBufferSize(),
        m_pPerfExpResults(),
        m_traceApiInfo(),
        m_pCopySampleGpuMem(),
        m_copySampleOffset()
    {}

    Pal::IDevice*         m_pDevice;
    GpaAllocator*         m_pAllocator;
    Pal::IPerfExperiment* m_pPerfExperiment;           // The PerfExperiment this PerfSample configures.
    GpuMemoryInfo         m_sampleDataGpuMemoryInfo;   // Destination of sample results, read as m_pPerfExpResults by CPU.
    Pal::gpusize          m_gcSampleDataOffset;        // Offset of the graphics core (GC) sample results in gpu memory.
    Pal::gpusize          m_pGcSampleDataBufferSize;   // Size of the graphics core (GC) sample data gpu memory.
    void*                 m_pPerfExpResults;           // Sample data gpu memory mapped to be CPU-readable.
    SampleTraceApiInfo    m_traceApiInfo;

    // The sample data GPU memory location and offset are stored here. On request, a copy is initiated from this
    // location to this sample's sample data memory location.
    Pal::IGpuMemory*      m_pCopySampleGpuMem;      // Src sample's gpu memory containing sample results.
    Pal::gpusize          m_copySampleOffset;       // Src sample's results offset in gpu memory.

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(PerfSample);
    PAL_DISALLOW_DEFAULT_CTOR(PerfSample);
};

// =====================================================================================================================
// Handles PerfSample set-up specific to performance counters.
class GpaSession::CounterSample : public GpaSession::PerfSample
{
public:
    CounterSample(
        Pal::IDevice*         pDevice,
        Pal::IPerfExperiment* pPerfExperiment,
        GpaAllocator*         pAllocator)
        :
        PerfSample(pDevice, pPerfExperiment, pAllocator),
        m_pGlobalCounterLayout(nullptr)
    {}

    virtual ~CounterSample();

    // Initializes this CounterSample by setting the counter layout.
    Pal::Result Init(Pal::uint32 numGlobalCounters) { return SetCounterLayout(numGlobalCounters, nullptr); }

    Pal::Result SetCounterLayout(Pal::uint32 numGlobalCounters, Pal::GlobalCounterLayout* pLayout);
    Pal::GlobalCounterLayout* GetCounterLayout() { return m_pGlobalCounterLayout; }
    Pal::Result GetCounterResults(void* pData, size_t* pSizeInBytes);

private:
    Pal::GlobalCounterLayout* m_pGlobalCounterLayout;
};

// =====================================================================================================================
// Handles PerfSample configuration specific to thread trace and/or spm trace.
class GpaSession::TraceSample : public GpaSession::PerfSample
{
public:
    TraceSample(
        Pal::IDevice*         pDevice,
        Pal::IPerfExperiment* pPerfExperiment,
        GpaAllocator*         pAllocator)
        :
        PerfSample(pDevice, pPerfExperiment, pAllocator),
        m_traceMemoryOffset(0),
        m_traceMemorySize(0),
        m_pThreadTraceLayout(nullptr),
        m_numSpmCounters(0),
        m_pOldestSample(nullptr),
        m_spmSampleInterval(0),
        m_pSpmTraceLayout(nullptr),
        m_numSpmSamples(-1),
        m_numDfSpmCounters(0),
        m_dfSpmSampleInterval(0),
        m_numDfSpmSamples(-1),
        m_pDfSpmEventQualifiers(nullptr),
        m_pDfSpmEventIds(nullptr),
        m_pDfSpmInstances(nullptr),
        m_pDfSpmGpuBlocks(nullptr)
    {
        m_traceGpuMemoryInfo = {};
        m_flags.u32All       = 0;
    }

    virtual ~TraceSample();

    // Initializes this TraceSample by setting thread trace layout.
    Pal::Result InitThreadTrace();
    Pal::Result InitSpmTrace(const GpaSampleConfig& sampleconfig);
    Pal::Result InitDfSpmTrace(const GpaSampleConfig& sampleConfig);

    Pal::ThreadTraceLayout* GetThreadTraceLayout()   const { return m_pThreadTraceLayout; }
    Pal::gpusize            GetTraceBufferSize()     const { return m_traceMemorySize; }
    Pal::uint32             GetNumSpmCounters()      const { return m_numSpmCounters; }
    Pal::uint32             GetNumDfSpmCounters()    const { return m_numDfSpmCounters; }
    Pal::uint32             GetSpmSampleInterval()   const { return m_spmSampleInterval; }
    Pal::uint32             GetDfSpmSampleInterval() const { return m_dfSpmSampleInterval; }

    Pal::Result GetSpmTraceResults(
        void*  pDstBuffer,
        size_t bufferSize);
    Pal::Result GetDfSpmTraceResults(
        void*  pDstBuffer,
        size_t bufferSize);
    void GetSpmResultsSize(
        Pal::gpusize* pSizeInBytes,
        Pal::gpusize* pNumSamples);
    void GetDfSpmResultsSize(
        Pal::gpusize* pSizeInBytes,
        Pal::gpusize* pNumSamples);

    Pal::Result SetThreadTraceLayout(
        Pal::ThreadTraceLayout* pLayout);
    void SetTraceMemory(
        const GpuMemoryInfo& gpuMemoryInfo,
        Pal::gpusize         offset,
        Pal::gpusize         size);
    void WriteCopyTraceData(
        Pal::ICmdBuffer* pCmdBuffer);
    void WriteCopyDfSpmTraceData(
        Pal::ICmdBuffer* pCmdBuffer);

    bool IsThreadTraceEnabled() const { return m_flags.threadTraceEnabled; }
    bool IsSpmTraceEnabled()    const { return m_flags.spmTraceEnabled; }
    bool IsDfSpmTraceEnabled()  const { return m_flags.dfSpmTraceEnabled; }

private:
    static const Pal::uint32 MaxNumCountersPerBitline = 16;

    void        CountNumSpmSamples();
    Pal::uint32 CountNumDfSpmSamples() const;

    // Common trace specific memory properties.
    GpuMemoryInfo m_traceGpuMemoryInfo; // CPU invisible memory used as thread trace buffer.
    Pal::gpusize  m_traceMemoryOffset;
    Pal::gpusize  m_traceMemorySize;

    union TraceSampleFlags
    {
        struct
        {
            Pal::uint32 threadTraceEnabled : 1;
            Pal::uint32 spmTraceEnabled    : 1;
            Pal::uint32 dfSpmTraceEnabled  : 1;
            Pal::uint32 reserved           : 29;
        };
        Pal::uint32 u32All;
    } m_flags;

    // SQTT specific member variables.
    Pal::ThreadTraceLayout* m_pThreadTraceLayout;

    // SPM specific member variables.
    Pal::uint32             m_numSpmCounters;    // Number of spm counters requested.
    Pal::uint32             m_spmSampleInterval; // Sample interval of this spm trace.
    Pal::SpmTraceLayout*    m_pSpmTraceLayout;   // Layout describing the results of the spm trace.

    // SPM state set by CountNumSpmSamples, they're inferred from the trace data.
    Pal::int32              m_numSpmSamples;     // Number of samples of data written by HW.
    const void*             m_pOldestSample;     // Points to the oldest sample written by HW.

    // DF SPM specific member variables.
    Pal::uint32             m_numDfSpmCounters;
    Pal::uint32             m_dfSpmSampleInterval;
    Pal::int32              m_numDfSpmSamples;
    Pal::uint32*            m_pDfSpmEventQualifiers;
    Pal::uint32*            m_pDfSpmEventIds;
    Pal::uint32*            m_pDfSpmInstances;
    SpmGpuBlock*            m_pDfSpmGpuBlocks;
};

// =====================================================================================================================
// Handles PerfSample config specifc to Timing samples. Timing samples don't use the perf experiment.
class GpaSession::TimingSample : public GpaSession::PerfSample
{
public:
    TimingSample(
        Pal::IDevice*         pDevice,
        Pal::IPerfExperiment* pPerfExperiment,
        GpaAllocator*         pAllocator)
        :
        PerfSample(pDevice, pPerfExperiment, pAllocator),
        m_preSample(Pal::HwPipePoint::HwPipeTop),
        m_postSample(Pal::HwPipePoint::HwPipeTop),
        m_pBeginTs(nullptr),
        m_pEndTs(nullptr),
        m_pBeginTsGpuMem(nullptr),
        m_pEndTsGpuMem(nullptr),
        m_beginTsGpuMemOffset(0),
        m_endTsGpuMemOffset(0)
    {}

    virtual ~TimingSample() {}

    void Init(Pal::HwPipePoint preSample, Pal::HwPipePoint postSample)
    {
        m_preSample  = preSample;
        m_postSample = postSample;
    }

    void SetTimestampMemoryInfo(const GpuMemoryInfo& gpuMemInfo, Pal::gpusize offset, Pal::uint32 timestampAlignment);

    Pal::Result      GetTimingSampleResults(void* pData, size_t* pSizeInBytes);
    Pal::IGpuMemory* GetBeginTsGpuMem()       { return m_pBeginTsGpuMem; }
    Pal::gpusize     GetBeginTsGpuMemOffset() { return m_beginTsGpuMemOffset; }
    Pal::IGpuMemory* GetEndTsGpuMem()         { return m_pEndTsGpuMem; }
    Pal::gpusize     GetEndTsGpuMemOffset()   { return m_endTsGpuMemOffset; }
    Pal::HwPipePoint GetPostSamplePoint()     { return m_postSample; }

private:
    Pal::HwPipePoint   m_preSample;
    Pal::HwPipePoint   m_postSample;
    const Pal::uint64* m_pBeginTs;            // CPU address of the pre-call timestamp.
    const Pal::uint64* m_pEndTs;              // CPU address of the post-call timestamp.
    Pal::IGpuMemory*   m_pBeginTsGpuMem;      // GPU object holding the pre-call timestamp.
    Pal::IGpuMemory*   m_pEndTsGpuMem;        // GPU object holding the post-call timestamp.
    Pal::gpusize       m_beginTsGpuMemOffset; // Offset into the GPU object where the pre-call timestamp is.
    Pal::gpusize       m_endTsGpuMemOffset;   // Offset into the GPU object where the post-call timestamp is.
};

// =====================================================================================================================
// Handles PerfSample config specifc to pipeline stats query samples. Query samples don't use the perf experiment.
class GpaSession::QuerySample : public GpaSession::PerfSample
{
public:
    QuerySample(
        Pal::IDevice*         pDevice,
        Pal::IPerfExperiment* pPerfExperiment,
        GpaAllocator*         pAllocator)
        :
        PerfSample(pDevice, pPerfExperiment, pAllocator),
        m_pPipeStatsQuery(nullptr)
    {}

    virtual ~QuerySample();

    Pal::IQueryPool* GetPipeStatsQuery() { return m_pPipeStatsQuery; }
    void SetPipeStatsQuery(Pal::IQueryPool* pQuery) { m_pPipeStatsQuery = pQuery; }
    Pal::Result GetQueryResults(void* pData, size_t* pSizeInBytes);

private:
    Pal::IQueryPool* m_pPipeStatsQuery;
};
} // GpuUtil
