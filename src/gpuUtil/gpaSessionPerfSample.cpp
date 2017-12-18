/*
 *******************************************************************************
 *
 * Copyright (c) 2017 Advanced Micro Devices, Inc. All rights reserved.
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

#include "gpaSessionPerfSample.h"

using namespace Pal;

namespace GpuUtil
{
// =====================================================================================================================
// Initializes this PerfSample's perf results destination gpu memory properties.
void GpaSession::PerfSample::SetSampleMemoryProperties(
    const GpuMemoryInfo& pGpuMemory,
    Pal::gpusize         offset,
    Pal::gpusize         buffersize)
{
    m_sampleDataGpuMemoryInfo = pGpuMemory;
    m_sampleDataOffset        = offset;
    m_pSampleDataBufferSize   = buffersize;

    // Map the gpu memory.
    m_pPerfExpResults = Util::VoidPtrInc(m_sampleDataGpuMemoryInfo.pCpuAddr, static_cast<size_t>(offset));
}

// =====================================================================================================================
// Copying a sample's perf experiment results requires that only the memory info of the src session's samples be stored.
// A GPU copy is performed later when the GpaSession's CopyResults method is called.
void GpaSession::PerfSample::SetCopySampleMemInfo(
    Pal::IGpuMemory* pSrcSampleGpuMem,
    Pal::gpusize     srcSampleOffset)
{
    m_pCopySampleGpuMem = pSrcSampleGpuMem;
    m_copySampleOffset  = srcSampleOffset;
}

// =====================================================================================================================
// Writes commands to copy the sample data from the src PerfSample to this PerfSample's sample data gpu mem.
// The src session's sample data gpu mem is saved during GpaSesssion initialization into this PerfSample.
void GpaSession::PerfSample::WriteCopySampleData(Pal::ICmdBuffer* pCmdBuffer)
{
    // NOTE: SetCopySampleMemInfo must have been called prior to the copy.
    PAL_ASSERT(m_pCopySampleGpuMem != nullptr);

    Pal::MemoryCopyRegion copyRegions = {};

    copyRegions.srcOffset = m_copySampleOffset;
    copyRegions.dstOffset = m_sampleDataOffset;
    copyRegions.copySize  = m_pSampleDataBufferSize;

    pCmdBuffer->CmdCopyMemory(*m_pCopySampleGpuMem, *(m_sampleDataGpuMemoryInfo.pGpuMemory), 1, &copyRegions);
}

// =====================================================================================================================
GpaSession::CounterSample::~CounterSample()
{
    if (m_pGlobalCounterLayout != nullptr)
    {
        PAL_SAFE_FREE(m_pGlobalCounterLayout, m_pAllocator);
    }
}

// =====================================================================================================================
// Initializes the counter layout of this PerfSample.
Result GpaSession::CounterSample::SetCounterLayout(
    Pal::uint32               numGlobalCounters,
    Pal::GlobalCounterLayout* pLayout)
{
    // Note that global perf counters are disabled if this value is zero.
    PAL_ASSERT(numGlobalCounters > 0);

    Result result = Result::Success;

    // Allocate enough space for one SampleLayout per perf-experiment sample.
    const size_t size = sizeof(Pal::GlobalCounterLayout) +
                        (sizeof(Pal::GlobalSampleLayout) * (numGlobalCounters - 1));

    // Create GlobalCounterLayout
    m_pGlobalCounterLayout = static_cast<Pal::GlobalCounterLayout*>(PAL_CALLOC(size,
                                                                               m_pAllocator,
                                                                               Util::SystemAllocType::AllocObject));

    if (m_pGlobalCounterLayout == nullptr)
    {
        result = Result::ErrorOutOfMemory;
    }
    else
    {
        m_pGlobalCounterLayout->sampleCount = numGlobalCounters;

        // Update layout info
        if (pLayout == nullptr)
        {
            result = m_pPerfExperiment->GetGlobalCounterLayout(m_pGlobalCounterLayout);
        }
        else
        {
            memcpy(m_pGlobalCounterLayout->samples, pLayout->samples, sizeof(GlobalSampleLayout) * numGlobalCounters);
        }
    }

    return result;
}

// =====================================================================================================================
// Returns perf counter results in the buffer provided or returns the size required for the results.
Result GpaSession::CounterSample::GetCounterResults(
    void*   pData,
    size_t* pSizeInBytes)
{
    Result result = Result::Success;

    const uint32 numGlobalPerfCounters = m_pGlobalCounterLayout->sampleCount;
    const size_t size = sizeof(uint64) * numGlobalPerfCounters;

    if (pSizeInBytes == nullptr)
    {
        result = Result::ErrorInvalidPointer;
    }

    if (result == Result::Success)
    {
        // Copy result to pData
        if (pData != nullptr)
        {
            // Check if pData has enough amount of available space
            if (*pSizeInBytes >= size)
            {
                *pSizeInBytes = size; // Amount of space required for pData

                for (uint32 i = 0; i < numGlobalPerfCounters; i++)
                {
                    const GlobalSampleLayout& sample = m_pGlobalCounterLayout->samples[i];

                    // Accumulate the (end - begin) value of the counter into the appropriate output value.
                    if (sample.dataType == PerfCounterDataType::Uint32)
                    {
                        const uint32 beginVal =
                            *static_cast<uint32*>(Util::VoidPtrInc(m_pPerfExpResults,
                                static_cast<size_t>(sample.beginValueOffset)));
                        const uint32 endVal =
                            *static_cast<uint32*>(Util::VoidPtrInc(m_pPerfExpResults,
                                static_cast<size_t>(sample.endValueOffset)));

                        (static_cast<uint64*>(pData))[i] = endVal - beginVal;
                    }
                    else
                    {
                        const uint64 beginVal =
                            *static_cast<uint64*>(Util::VoidPtrInc(m_pPerfExpResults,
                                static_cast<size_t>(sample.beginValueOffset)));
                        const uint64 endVal =
                            *static_cast<uint64*>(Util::VoidPtrInc(m_pPerfExpResults,
                                static_cast<size_t>(sample.endValueOffset)));

                        (static_cast<uint64*>(pData))[i] = endVal - beginVal;
                    }
                }
            }
            else
            {
                result = Result::ErrorInvalidMemorySize;
            }
        }
        else
        {
            // Only query size
            *pSizeInBytes = size; // Amount of space required for pData
        }
    }

    return result;
}

// =====================================================================================================================
GpaSession::TraceSample::~TraceSample()
{
    if (m_pThreadTraceLayout != nullptr)
    {
        PAL_SAFE_FREE(m_pThreadTraceLayout, m_pAllocator);
    }
}

// =====================================================================================================================
// Initializes the thread trace layout of this sample.
Result GpaSession::TraceSample::SetThreadTraceLayout(
    Pal::uint32             numShaderEngines,
    Pal::ThreadTraceLayout* pLayout)
{
    Result result = Result::Success;

    // Allocate enough space for one SeLayout per shader engine.
    const size_t size = sizeof(ThreadTraceLayout) +
                        (sizeof(ThreadTraceSeLayout) * (numShaderEngines - 1));

    m_pThreadTraceLayout = static_cast<ThreadTraceLayout*>(PAL_CALLOC(size,
                                                                      m_pAllocator,
                                                                      Util::SystemAllocType::AllocObject));
    PAL_ASSERT(m_pThreadTraceLayout != nullptr);

    if (m_pThreadTraceLayout != nullptr)
    {
        m_pThreadTraceLayout->traceCount = numShaderEngines;

        // Update layout info
        if (pLayout == nullptr)
        {
            m_pPerfExperiment->GetThreadTraceLayout(m_pThreadTraceLayout);
        }
        else
        {
            memcpy(m_pThreadTraceLayout->traces, pLayout->traces, sizeof(ThreadTraceSeLayout) * numShaderEngines);
        }
    }
    else
    {
        result = Result::ErrorOutOfMemory;
    }

    return result;
}

// =====================================================================================================================
// Saves the CPU-invisible thread trace memory buffer.
void GpaSession::TraceSample::SetThreadTraceMemory(
    const GpuMemoryInfo& gpuMemory,
    Pal::gpusize         offset,
    Pal::gpusize         size)
{
    m_threadTraceGpuMemoryInfo = gpuMemory;
    m_threadTraceMemoryOffset  = offset;
    m_threadTraceMemorySize    = size;
}

// =====================================================================================================================
// Copy Counter/SQTT result from primary heap to secondary heap
void GpaSession::TraceSample::WriteCopyThreadTraceData(ICmdBuffer* pCmdBuf)
{
    MemoryCopyRegion copyRegions = {};

    copyRegions.srcOffset = m_threadTraceMemoryOffset;
    copyRegions.dstOffset = m_sampleDataOffset;
    copyRegions.copySize  = m_pSampleDataBufferSize;

    pCmdBuf->CmdCopyMemory(*(m_threadTraceGpuMemoryInfo.pGpuMemory),
                           *(m_sampleDataGpuMemoryInfo.pGpuMemory),
                           1,
                           &copyRegions);
}

// =====================================================================================================================
// Initializes TimingSample memory info.
void GpaSession::TimingSample::SetTimestampMemoryInfo(
    const GpuMemoryInfo& gpuMemInfo,
    Pal::gpusize         offset,
    Pal::uint32          timestampAlignment)
{
    // Save the memory info of beginTs/endTs to be used for logging timestamp; and used for
    // initialization only if it's a copy-session.
    m_pBeginTsGpuMem      = gpuMemInfo.pGpuMemory;
    m_beginTsGpuMemOffset = offset;

    m_pBeginTs = static_cast<uint64*>(Util::VoidPtrInc(gpuMemInfo.pCpuAddr, static_cast<size_t>(offset)));

    offset += timestampAlignment; // Skip beginTs to get the address for endTs.
    m_pEndTsGpuMem      = gpuMemInfo.pGpuMemory;
    m_endTsGpuMemOffset = offset;

    m_pEndTs = static_cast<uint64*>(Util::VoidPtrInc(gpuMemInfo.pCpuAddr, static_cast<size_t>(offset)));

    SetSampleMemoryProperties(gpuMemInfo, offset, static_cast<gpusize>(timestampAlignment + sizeof(uint64)));
}

// =====================================================================================================================
// Copies the begin and end timestamp values to the data buffer provided if the size is non-null.
Result GpaSession::TimingSample::GetTimingSampleResults(
    void*   pData,
    size_t* pSizeInBytes)
{
    Result result = Result::Success;

    if (pData != nullptr)
    {
        (static_cast<uint64*>(pData))[0] = *(m_pBeginTs);
        (static_cast<uint64*>(pData))[1] = *(m_pEndTs);
    }
    else
    {
        // In this case only query size
        if (pSizeInBytes == nullptr)
        {
            result = Result::ErrorInvalidPointer;
        }

        if (result == Result::Success)
        {
            *pSizeInBytes = 2 * sizeof(uint64);; // Amount of space required for pData
        }
    }

    return result;
}

// =====================================================================================================================
GpaSession::QuerySample::~QuerySample()
{
    // Should we free it here? Refactor GpaSession's destruction logic.
    if (m_pPipeStatsQuery != nullptr)
    {
        m_pPipeStatsQuery->Destroy();
        PAL_SAFE_FREE(m_pPipeStatsQuery, m_pAllocator);
    }
}

// =====================================================================================================================
// Returns perf counter results in the buffer provided or returns the size required for the results.
Result GpaSession::QuerySample::GetQueryResults(
    void*   pData,
    size_t* pSizeInBytes)
{
    Result result = (pSizeInBytes == nullptr) ? Result::ErrorInvalidPointer : Result::Success;

    if (result == Result::Success)
    {
        if (*pSizeInBytes == 0)
        {
            // Query result size.
            PAL_NOT_IMPLEMENTED();
        }
        else
        {
            if (pData == nullptr)
            {
                result = Result::ErrorInvalidPointer;
            }

            if (result == Result::Success)
            {
                const QueryResultFlags flags = static_cast<QueryResultFlags>(QueryResult64Bit | QueryResultWait);
                result = m_pPipeStatsQuery->GetResults(flags,
                                                       QueryType::PipelineStats,
                                                       0,
                                                       1,
                                                       pSizeInBytes,
                                                       pData,
                                                       0);
            }
        }
    }

    return result;
}

} // GpuUtil
