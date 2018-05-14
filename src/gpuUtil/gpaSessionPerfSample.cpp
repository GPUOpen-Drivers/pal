/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "gpaSessionPerfSample.h"

using namespace Pal;

namespace GpuUtil
{
// =====================================================================================================================
// Sets this sample's results gpu mem. This is the ultimate destination of the perf experiment results.
void GpaSession::PerfSample::SetSampleMemoryProperties(
    const GpuMemoryInfo& pGpuMemory,
    Pal::gpusize         offset,
    Pal::gpusize         buffersize)
{
    m_sampleDataGpuMemoryInfo = pGpuMemory;
    m_sampleDataOffset        = offset;
    m_pSampleDataBufferSize   = buffersize;

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
    PAL_SAFE_FREE(m_pThreadTraceLayout, m_pAllocator);
    PAL_SAFE_FREE(m_pSpmTraceLayout, m_pAllocator);
}

// =====================================================================================================================
// Initializes thread trace layout.
Pal::Result GpaSession::TraceSample::InitThreadTrace(
    Pal::uint32 numShaderEngines)
{
    m_flags.threadTraceEnabled = 1;
    return SetThreadTraceLayout(numShaderEngines, nullptr);
}

// =====================================================================================================================
Pal::Result GpaSession::TraceSample::InitSpmTrace(
    Pal::uint32 numCounters)
{
    Result result    = Result::ErrorOutOfMemory;
    m_numSpmCounters = numCounters;

    // Space is already allocated for one counter in the SpmTraceLayout.
    const size_t size = sizeof(SpmTraceLayout) +
                        ((m_numSpmCounters - 1) * sizeof(SpmCounterData));

    void* pMem = PAL_CALLOC(size, m_pAllocator, Util::SystemAllocType::AllocInternal);

    if (pMem != nullptr)
    {
        m_flags.spmTraceEnabled = 1;

        m_pSpmTraceLayout = PAL_PLACEMENT_NEW (pMem) SpmTraceLayout();
        m_pSpmTraceLayout->numCounters = m_numSpmCounters;

        result = m_pPerfExperiment->GetSpmTraceLayout(m_pSpmTraceLayout);
    }

    return result;
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
// Sets the intermediate buffer gpu mem into which the HW will write the trace data.
void GpaSession::TraceSample::SetTraceMemory(
    const GpuMemoryInfo& gpuMemory,
    Pal::gpusize         offset,
    Pal::gpusize         size)
{
    m_traceGpuMemoryInfo = gpuMemory;
    m_traceMemoryOffset  = offset;
    m_traceMemorySize    = size;
}

// =====================================================================================================================
// Copy Counter/SQTT result from primary heap to secondary heap
void GpaSession::TraceSample::WriteCopyTraceData(ICmdBuffer* pCmdBuf)
{
    MemoryCopyRegion copyRegions = {};

    copyRegions.srcOffset = m_traceMemoryOffset;
    copyRegions.dstOffset = m_sampleDataOffset;
    copyRegions.copySize  = m_pSampleDataBufferSize;

    pCmdBuf->CmdCopyMemory(*(m_traceGpuMemoryInfo.pGpuMemory),
                           *(m_sampleDataGpuMemoryInfo.pGpuMemory),
                           1,
                           &copyRegions);
}

// =====================================================================================================================
// Obtains the number of bytes of spm data written in the spm ring buffer and the number of samples.
void GpaSession::TraceSample::GetSpmResultsSize(
    Pal::gpusize* pSizeInBytes,
    Pal::gpusize* pNumSamples)
{
    if (m_numSpmSamples < 0)
    {
        // Cache the number of samples if not computed before.
        m_numSpmSamples = CountNumSamples(Util::VoidPtrInc(m_pPerfExpResults,
                                                           static_cast<size_t>(m_pSpmTraceLayout->offset)));
    }

    (*pNumSamples) = m_numSpmSamples;

    // This is calculated according to the spm data layout in the RGP spec, excluding the header, num timestamps
    // and the timestampOffset fields.
    (*pSizeInBytes) = m_numSpmCounters * sizeof(SpmCounterInfo) +          // SpmCounterInfo for each counter.
                      m_numSpmSamples * sizeof(gpusize) +                  // Timestamp data.
                      m_numSpmCounters * m_numSpmSamples * sizeof(uint16); // Counter data.
}

// =====================================================================================================================
// Returns the size of the SPM counter delta output if nullptr buffer is provided, or outputs the counter sample values
// into the buffer provided.
Result GpaSession::TraceSample::GetSpmTraceResults(
    void*  pDstBuffer,
    size_t bufferSize)
{
    /* RGP Layout for SPM trace data:
     *   1. Header
     *   2. Num Timestamps
     *   3. Num SpmCounterInfo
     *   4. Timestamps[]
     *   5. SpmCounterInfo[]
     *   6. Counter values[]
    */

    Result result = Result::Success;

    const size_t NumMetadataBytes         = 32;
    const gpusize SampleSizeInQWords      = m_pSpmTraceLayout->sampleSizeInBytes / sizeof(uint64);
    const gpusize SampleSizeInWords       = m_pSpmTraceLayout->sampleSizeInBytes / sizeof(uint16);
    const size_t TimestampDataSizeInBytes = m_numSpmSamples * sizeof(gpusize);
    const gpusize CounterDataSizeInBytes  = m_numSpmSamples * sizeof(uint16); // Size of data written for one counter.
    const size_t CounterInfoSizeInBytes   = m_numSpmCounters * sizeof(SpmCounterInfo);
    const uint32  SegmentSizeInWords      = m_pSpmTraceLayout->sampleSizeInBytes / sizeof(uint16);
    const size_t CounterDataOffset        = TimestampDataSizeInBytes + CounterInfoSizeInBytes;

    // A valid destination buffer size is expected.
    PAL_ASSERT((bufferSize > 0) && (pDstBuffer != nullptr));

    // Start of the spm results section.
    void* pSrcBufferStart = Util::VoidPtrInc(m_pPerfExpResults,
                                             static_cast<size_t>(m_pSpmTraceLayout->offset));

    // Extract timestamps from m_perfExpResults
    uint32 numSpmDwords = static_cast<uint32*>(pSrcBufferStart)[0];

    // Move to the actual start of the Spm data. The first dword is the wptr. There are 32 bytes of
    // reserved fields after which the data begins.
    void* pSrcDataStart = Util::VoidPtrInc(pSrcBufferStart, NumMetadataBytes);
    uint64* pTimestamp = static_cast<uint64*>(pSrcDataStart);

    for (int32 sample = 0; sample < m_numSpmSamples; ++sample)
    {
        // RGP Spm output: Write the timestamps.
        static_cast<uint64*>(pDstBuffer)[sample] = *pTimestamp;

        pTimestamp += SampleSizeInQWords;
    }

    // Beginning of the SpmCounterInfo section.
    SpmCounterInfo* pCounterInfo =
        static_cast<SpmCounterInfo*>(Util::VoidPtrInc(pDstBuffer, TimestampDataSizeInBytes));

    // Offset from the beginning of the RGP spm chunk to where the counter values begin.
    gpusize curCounterDataOffset = CounterDataOffset;

    // RGP SPM output: write the SpmCounterInfo for each counter.
    for (uint32 counter = 0; counter < m_numSpmCounters; counter++)
    {
        pCounterInfo[counter].block      = static_cast<SpmGpuBlock>(m_pSpmTraceLayout->counterData[counter].gpuBlock);
        pCounterInfo[counter].instance   = m_pSpmTraceLayout->counterData[counter].instance;
        pCounterInfo[counter].dataOffset = static_cast<uint32>(curCounterDataOffset);

        curCounterDataOffset += CounterDataSizeInBytes;
    }

    // Read pointer points to the first segment of the first sample.
    uint16* pSample = static_cast<uint16*>(pSrcDataStart);

    // Index within the SPM ring buffer, which is considered an array of uint16.
    gpusize index = 0;
    gpusize offset = 0;

    // Write pointer points to the beginning ofthe first counter data.
    uint16* pDstCounterData = static_cast<uint16*>(Util::VoidPtrInc(pDstBuffer, CounterDataOffset));

    for (uint32 counter = 0; counter < m_numSpmCounters; counter++)
    {
        offset = m_pSpmTraceLayout->counterData[counter].offset;

        for (int32 sample = 0; sample < m_numSpmSamples; sample++)
        {
            index = offset + (sample * SampleSizeInWords);

            // RGP SPM OUTPUT: write the delta values of the current counter for all samples.
            (*pDstCounterData) = pSample[index];
            pDstCounterData++;

        } // Iterate over samples.
    } // Iterate over counters.

    return result;
}

// =====================================================================================================================
// Parses the SPM ring buffer to find the number of samples of data written in the buffer.
uint32 GpaSession::TraceSample::CountNumSamples(void* pBufferStart)
{
    // We actually have to read the ring buffer here and use the layout to figure out the number of samples that have
    // been written.
    uint32 numSamples = 0;

    uint32 segmentSizeInDwords = m_pSpmTraceLayout->sampleSizeInBytes / 4;
    uint32 segmentSizeInBitlines = m_pSpmTraceLayout->sampleSizeInBytes / 32;

    if (segmentSizeInDwords > 0)
    {
        //! Not sure if this is in bytes or dwords. Assume this is a dword based size since it is a wptr and not size!
        // The first dword is the buffer size followed by 7 reserved dwords
        uint32 dataSizeInDwords = static_cast<uint32*>(pBufferStart)[0];

        // Number of 256 bit lines written by the
        uint32 numLinesWritten = dataSizeInDwords / 2 / MaxNumCountersPerBitline;

        // Check for overflow. The number of lines written should be a multiple of the number of lines in each sample.
        if (numLinesWritten % segmentSizeInBitlines)
        {
            // Consider increasing the size of the buffer or reducing the number of counters.
            PAL_ASSERT_ALWAYS();
        }
        else
        {
            numSamples = numLinesWritten / segmentSizeInBitlines;
        }
    }

    return numSamples;
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
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 371
                                                       nullptr,
#endif
                                                       pSizeInBytes,
                                                       pData,
                                                       0);
            }
        }
    }

    return result;
}

} // GpuUtil
