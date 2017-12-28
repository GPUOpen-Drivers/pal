/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017 Advanced Micro Devices, Inc. All Rights Reserved.
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

    GpuMemoryInfo    GetSampleDataGpuMem()     { return m_sampleDataGpuMemoryInfo; }
    Pal::gpusize     GetSampleDataOffset()     { return m_sampleDataOffset; }
    Pal::gpusize     GetSampleDataSize()       { return m_pSampleDataBufferSize; }
    void*            GetPerfExpResults()       { return m_pPerfExpResults; }
    Pal::IGpuMemory* GetCopySampleDataGpuMem() { return m_pCopySampleGpuMem; }
    Pal::gpusize     GetCopySampleDataOffset() { return m_copySampleOffset; }

protected:
    PerfSample(
        Pal::IDevice*         pDevice,
        Pal::IPerfExperiment* pPerfExperiment,
        GpaAllocator*         pAllocator)
        :
        m_pDevice(pDevice),
        m_pAllocator(pAllocator),
        m_pPerfExperiment(pPerfExperiment)
    {}

    Pal::IDevice*         m_pDevice;
    GpaAllocator*         m_pAllocator;
    Pal::IPerfExperiment* m_pPerfExperiment;         // The PerfExperiment this PerfSample configures.
    GpuMemoryInfo         m_sampleDataGpuMemoryInfo; // Destination of sample results, read as m_pPerfExpResults by CPU.
    Pal::gpusize          m_sampleDataOffset;        // Offset of the sample results in gpu memory.
    Pal::gpusize          m_pSampleDataBufferSize;   // Size of the sample data gpu memory.
    void*                 m_pPerfExpResults;         // Sample data gpu memory mapped to be CPU-readable.

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
    {};

    ~CounterSample();

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
        m_pThreadTraceLayout(nullptr)
    {}

    ~TraceSample();

    // Initializes this TraceSample by setting thread trace layout.
    Pal::Result Init(Pal::uint32 numShaderEngines) { return SetThreadTraceLayout(numShaderEngines, nullptr); }

    Pal::ThreadTraceLayout* GetThreadTraceLayout()     { return m_pThreadTraceLayout; }
    Pal::gpusize            GetThreadTraceBufferSize() { return m_threadTraceMemorySize; }

    Pal::Result SetThreadTraceLayout(Pal::uint32 numShaderEngines, Pal::ThreadTraceLayout* pLayout);
    void SetThreadTraceMemory(const GpuMemoryInfo& gpuMemoryInfo, Pal::gpusize offset, Pal::gpusize size);
    void WriteCopyThreadTraceData(Pal::ICmdBuffer* pCmdBuffer);

private:
    GpuMemoryInfo           m_threadTraceGpuMemoryInfo; // CPU invisible memory used as thread trace buffer.
    Pal::gpusize            m_threadTraceMemoryOffset;
    Pal::ThreadTraceLayout* m_pThreadTraceLayout;
    Pal::gpusize            m_threadTraceMemorySize;
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

    ~TimingSample() {}

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

    ~QuerySample();

    Pal::IQueryPool* GetPipeStatsQuery() { return m_pPipeStatsQuery; }
    void SetPipeStatsQuery(Pal::IQueryPool* pQuery) { m_pPipeStatsQuery = pQuery; }
    Pal::Result GetQueryResults(void* pData, size_t* pSizeInBytes);

private:
    Pal::IQueryPool* m_pPipeStatsQuery;
};
} // GpuUtil
