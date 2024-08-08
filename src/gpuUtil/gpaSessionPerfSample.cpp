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

#include "gpaSessionPerfSample.h"
#include "palDbgLogger.h"

using namespace Pal;
using namespace Util;

namespace GpuUtil
{
// =====================================================================================================================
// Sets this sample's results gpu mem. This is the ultimate destination of the perf experiment results.
void GpaSession::PerfSample::SetSampleMemoryProperties(
    const GpuMemoryInfo& pGpuMemory,
    Pal::gpusize         offset,
    Pal::gpusize         buffersize)
{
    m_sampleDataGpuMemoryInfo   = pGpuMemory;
    m_gcSampleDataOffset        = offset;
    m_pGcSampleDataBufferSize   = buffersize;

    m_pPerfExpResults = VoidPtrInc(m_sampleDataGpuMemoryInfo.pCpuAddr, static_cast<size_t>(offset));
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
void GpaSession::PerfSample::WriteCopySampleData(
    Pal::ICmdBuffer* pCmdBuffer)
{
    // NOTE: SetCopySampleMemInfo must have been called prior to the copy.
    PAL_ASSERT(m_pCopySampleGpuMem != nullptr);

    Pal::MemoryCopyRegion copyRegions = {};

    copyRegions.srcOffset = m_copySampleOffset;
    copyRegions.dstOffset = m_gcSampleDataOffset;
    copyRegions.copySize  = m_pGcSampleDataBufferSize;

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
                                                                               SystemAllocType::AllocObject));

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
                    void* const               pBegin = VoidPtrInc(m_pPerfExpResults,
                                                                  static_cast<size_t>(sample.beginValueOffset));
                    void* const               pEnd   = VoidPtrInc(m_pPerfExpResults,
                                                                  static_cast<size_t>(sample.endValueOffset));
                    uint64                    end    = 0;
                    uint64                    begin  = 0;

                    if (sample.dataType == PerfCounterDataType::Uint32)
                    {
                        end   = *static_cast<uint32*>(pEnd);
                        begin = *static_cast<uint32*>(pBegin);
                    }
                    else
                    {
                        end   = *static_cast<uint64*>(pEnd);
                        begin = *static_cast<uint64*>(pBegin);
                    }
                    (static_cast<uint64*>(pData))[i] = (end - begin);
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
    PAL_SAFE_FREE(m_pDfSpmEventIds, m_pAllocator);
    PAL_SAFE_FREE(m_pDfSpmEventQualifiers, m_pAllocator);
    PAL_SAFE_FREE(m_pDfSpmInstances, m_pAllocator);
    PAL_SAFE_FREE(m_pDfSpmGpuBlocks, m_pAllocator);
}

// =====================================================================================================================
// Initializes thread trace layout.
Pal::Result GpaSession::TraceSample::InitThreadTrace()
{
    ThreadTraceLayout layout = {};
    Result            result = m_pPerfExperiment->GetThreadTraceLayout(&layout);

    if (result == Result::Success)
    {
        PAL_ASSERT(layout.traceCount > 0);

        // Allocate enough space for one SeLayout per thread trace.
        const size_t size = sizeof(ThreadTraceLayout) + (sizeof(ThreadTraceSeLayout) * (layout.traceCount - 1));

        m_pThreadTraceLayout = static_cast<ThreadTraceLayout*>(PAL_CALLOC(size,
                                                                          m_pAllocator,
                                                                          SystemAllocType::AllocObject));

        if (m_pThreadTraceLayout != nullptr)
        {
            m_flags.threadTraceEnabled = 1;

            m_pThreadTraceLayout->traceCount = layout.traceCount;

            result = m_pPerfExperiment->GetThreadTraceLayout(m_pThreadTraceLayout);
        }
        else
        {
            result = Result::ErrorOutOfMemory;
        }
    }

    return result;
}

// =====================================================================================================================
Pal::Result GpaSession::TraceSample::InitDfSpmTrace(
    const GpaSampleConfig& sampleConfig)
{
    Result result = Result::ErrorOutOfMemory;

    // These are used for parsing the final output buffer.
    m_numDfSpmCounters    = sampleConfig.dfSpmPerfCounters.numCounters;
    m_dfSpmSampleInterval = sampleConfig.dfSpmPerfCounters.sampleInterval;

    const size_t size             = m_numDfSpmCounters * sizeof(uint32);
    const size_t spmGpuBlocksSize = m_numDfSpmCounters * sizeof(SpmGpuBlock);

    m_pDfSpmEventIds        = static_cast<uint32*>(PAL_CALLOC(size,
                                                              m_pAllocator,
                                                              SystemAllocType::AllocInternal));
    m_pDfSpmEventQualifiers = static_cast<uint32*>(PAL_CALLOC(size,
                                                              m_pAllocator,
                                                              SystemAllocType::AllocInternal));
    m_pDfSpmInstances       = static_cast<uint32*>(PAL_CALLOC(size,
                                                              m_pAllocator,
                                                              SystemAllocType::AllocInternal));
    m_pDfSpmGpuBlocks       = static_cast<SpmGpuBlock*>(PAL_CALLOC(spmGpuBlocksSize,
                                                                   m_pAllocator,
                                                                   SystemAllocType::AllocInternal));

    if (m_pDfSpmEventIds        != nullptr &&
        m_pDfSpmEventQualifiers != nullptr &&
        m_pDfSpmInstances       != nullptr &&
        m_pDfSpmGpuBlocks       != nullptr)
    {
        result = Result::Success;
        m_flags.dfSpmTraceEnabled = 1;

        const PerfCounterId* pCounters = sampleConfig.dfSpmPerfCounters.pIds;

        for (uint32 idx = 0; idx < m_numDfSpmCounters; ++idx)
        {
            m_pDfSpmEventIds[idx]        = pCounters[idx].eventId;
            m_pDfSpmEventQualifiers[idx] = pCounters[idx].subConfig.df.eventQualifier;
            m_pDfSpmInstances[idx]       = pCounters[idx].instance;
            m_pDfSpmGpuBlocks[idx]       = static_cast<SpmGpuBlock>(pCounters[idx].block);
        }
    }
    return result;
}

// =====================================================================================================================
Pal::Result GpaSession::TraceSample::InitSpmTrace(
    const GpaSampleConfig& sampleconfig)
{
    Result result       = Result::ErrorOutOfMemory;
    m_numSpmCounters    = sampleconfig.perfCounters.numCounters;
    m_spmSampleInterval = sampleconfig.perfCounters.spmTraceSampleInterval;

    // Space is already allocated for one counter in the SpmTraceLayout.
    const size_t size = sizeof(SpmTraceLayout) +
                        ((m_numSpmCounters - 1) * sizeof(SpmCounterData));

    void* pMem = PAL_CALLOC(size, m_pAllocator, SystemAllocType::AllocInternal);

    if (pMem != nullptr)
    {
        m_flags.spmTraceEnabled = 1;

        m_pSpmTraceLayout = PAL_PLACEMENT_NEW (pMem) SpmTraceLayout();
        PAL_ASSERT(m_pSpmTraceLayout != nullptr);
        m_pSpmTraceLayout->numCounters = m_numSpmCounters;

        result = m_pPerfExperiment->GetSpmTraceLayout(m_pSpmTraceLayout);
    }

    return result;
}

// =====================================================================================================================
// Initializes the thread trace layout of this sample.
Result GpaSession::TraceSample::SetThreadTraceLayout(
    Pal::ThreadTraceLayout* pLayout)
{
    Result result = Result::Success;

    PAL_ASSERT(pLayout != nullptr);

    const size_t size = sizeof(ThreadTraceLayout) + (sizeof(ThreadTraceSeLayout) * (pLayout->traceCount - 1));

    m_pThreadTraceLayout = static_cast<ThreadTraceLayout*>(PAL_CALLOC(size,
                                                                      m_pAllocator,
                                                                      SystemAllocType::AllocObject));

    if (m_pThreadTraceLayout != nullptr)
    {
        memcpy(m_pThreadTraceLayout, pLayout, size);
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
    copyRegions.dstOffset = m_gcSampleDataOffset;
    copyRegions.copySize  = m_pGcSampleDataBufferSize;

    pCmdBuf->CmdCopyMemory(*(m_traceGpuMemoryInfo.pGpuMemory),
                           *(m_sampleDataGpuMemoryInfo.pGpuMemory),
                           1,
                           &copyRegions);
}

// ====================================================================================================================
void GpaSession::TraceSample::WriteCopyDfSpmTraceData(
    ICmdBuffer* pCmdBuf)
{
    // m_gcSampleDataOffset and m_pGcSampleDataBufferSize refer to the normal Counter/SQTT buffer. DF SPM data is stored
    // after that.
    pCmdBuf->CmdCopyDfSpmTraceData(*m_pPerfExperiment,
                                   *(m_sampleDataGpuMemoryInfo.pGpuMemory),
                                   (m_gcSampleDataOffset + m_pGcSampleDataBufferSize));
}

// =====================================================================================================================
// Obtains the number of bytes of df spm data written in the df spm trace buffer and the number of samples
void GpaSession::TraceSample::GetDfSpmResultsSize(
    Pal::gpusize* pSizeInBytes,
    Pal::gpusize* pNumSamples)
{
    if (m_numDfSpmSamples < 0)
    {
        // Cache the number of samples if not computed before.
        m_numDfSpmSamples = CountNumDfSpmSamples();
    }

    (*pNumSamples) = m_numDfSpmSamples;

    // This is calculated according to the spm data layout in the RGP spec, excluding the header, num timestamps
    // and the timestampOffset fields.
    (*pSizeInBytes) = m_numDfSpmCounters * sizeof(SpmCounterInfo) +            // SpmCounterInfo for each counter.
                      m_numDfSpmSamples  * sizeof(uint64)         +            // Timestamp data.
                      m_numDfSpmCounters * m_numDfSpmSamples * sizeof(uint16); // Counter data.
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
        CountNumSpmSamples();
    }

    // This is the size of the arrays that follow the SqttFileChunkSpmDb header.
    // 1. Timestamps[]
    // 2. SpmCounterInfo[]
    // 3. Counter values[]
    gpusize sizeInBytes = (m_numSpmSamples  * sizeof(uint64) +         // Timestamps[]
                           m_numSpmCounters * sizeof(SpmCounterInfo)); // SpmCounterInfo[]

    // The counter value arrays can have different data types depending on the counter configs.
    for (uint32 counter = 0; counter < m_numSpmCounters; counter++)
    {
#if (PAL_BUILD_BRANCH == 0) || (PAL_BUILD_BRANCH >= 2340)
        const uint32 dataSize = m_pSpmTraceLayout->counterData[counter].is32Bit ? sizeof(uint32) : sizeof(uint16);
#else
        const uint32 dataSize = sizeof(uint16);
#endif

        sizeInBytes += m_numSpmSamples * dataSize;
    }

    (*pSizeInBytes) = sizeInBytes;
    (*pNumSamples)  = m_numSpmSamples;
}

// =====================================================================================================================
// Returns the size of the DF SPM counter delta output if nullptr buffer is provided, or outputs the counter sample
// values into the buffer provided.
Result GpaSession::TraceSample::GetDfSpmTraceResults(
    void*  pDstBuffer,
    size_t bufferSize)
{
    /* RGP Layout for DF SPM trace data:
     *    1. Header
     *    2. Flags
     *    3. Preamble Size
     *    4. Num Timestamps
     *    5. Num DfSpmCounterInfo
     *    6. DfSpmCounterInfo Size
     *    6. Sampling Interval
     *    8. Timestamps[]
     *    9. DfSampleFlags[]
     *   10. DfSpmCounterInfo[]
     *   11. CounterData[]
    */

    Result result = Result::Success;

    // These offsets are defined by HW. They are hardcoded in the chunk returned.
    constexpr uint32 CounterValidOffsetInBits           = 244;
    constexpr uint32 GlobalTimestampCounterOffsetInBits = 160;
    constexpr uint32 GtscLimitHitOffsetInBits           = 253;
    constexpr uint32 OverFlowBitOffsetInBits            = 254;
    constexpr uint32 SegmentSizeInBytes                 = 32;

    constexpr gpusize SampleSizeInQWords       = SegmentSizeInBytes / sizeof(uint8);
    constexpr gpusize SampleSizeInWords        = SegmentSizeInBytes / 4;
    constexpr size_t  TwoCounterSizeInBytes    = 5;
    constexpr size_t  OneCounterDataSizeInBits = 20;

    const size_t  TimestampDataSizeInBytes = m_numDfSpmSamples  * sizeof(uint64);
    const size_t  FlagsDataSizeInBytes     = m_numDfSpmSamples  * sizeof(uint32);
    const gpusize CounterDataSizeInBytes   = m_numDfSpmSamples  * sizeof(uint32); // Size of data written for one counter.
    const size_t  CounterInfoSizeInBytes   = m_numDfSpmCounters * sizeof(DfSpmCounterInfo);
    const size_t  CounterInfoOffset        = TimestampDataSizeInBytes + FlagsDataSizeInBytes;
    const size_t  CounterDataOffset        = CounterInfoOffset + CounterInfoSizeInBytes;
    const size_t  TotalCounterDataSizeInBytes = m_numDfSpmCounters * m_numDfSpmSamples * sizeof(uint32);
    const size_t  CounterValidSizeInBytes  = m_numDfSpmSamples * sizeof(uint32);

    // A valid destination buffer size is expected.
    PAL_ASSERT((bufferSize > 0) && (pDstBuffer != nullptr));

    gpusize location = m_gcSampleDataOffset + m_pGcSampleDataBufferSize + sizeof(DfSpmTraceMetadataLayout);
    // Start of the spm results section.
    void* pSrcBufferStart = VoidPtrInc(m_pPerfExpResults, static_cast<size_t>(location));

    // Move to the actual start of the Spm data. The first dword is the wptr. There are 32 bytes of
    // reserved fields after which the data begins.
    void*   pTimestampData = VoidPtrInc(pSrcBufferStart, (GlobalTimestampCounterOffsetInBits / sizeof(uint8)));
    uint64* pTimestamp     = static_cast<uint64*>(pTimestampData);

    for (int32 sample = 0; sample < m_numDfSpmSamples; ++sample)
    {
        // RGP Spm output: Write the timestamps.
        static_cast<uint64*>(pDstBuffer)[sample] = *pTimestamp & 0xFFFFFFFFFF;

        pTimestamp += SampleSizeInQWords;
    }

    // Beginning of DfSampleFlags section.
    uint32* pFlags    = static_cast<uint32*>(VoidPtrInc(pDstBuffer, TimestampDataSizeInBytes));
    uint32* pFlagData = static_cast<uint32*>(VoidPtrInc(pSrcBufferStart, (GtscLimitHitOffsetInBits / sizeof(uint8))));
    for (int32 sample = 0; sample < m_numDfSpmSamples; ++sample)
    {
        //Write out the flags
        if (((*pFlagData) & 0x20) != 0)
        {
            pFlags[sample] |= 0x2;
        }
        if (((*pFlagData) & 0x40) != 0)
        {
            pFlags[sample] |= 0x1;
        }
        pFlagData += SampleSizeInQWords;
    }

    // Beginning of the DfSpmCounterInfo section.
    DfSpmCounterInfo* pCounterInfo =
        static_cast<DfSpmCounterInfo*>(VoidPtrInc(pDstBuffer, CounterInfoOffset));

    // Offset from the beginning of the RGP spm chunk to where the counter values begin.
    gpusize curCounterDataOffset  = CounterDataOffset;
    gpusize curCounterValidOffset = CounterDataOffset + TotalCounterDataSizeInBytes;

    // RGP SPM output: write the DfSpmCounterInfo for each counter.
    for (uint32 counter = 0; counter < m_numDfSpmCounters; counter++)
    {
        pCounterInfo[counter].block           = m_pDfSpmGpuBlocks[counter];
        pCounterInfo[counter].eventQualifier  = m_pDfSpmEventQualifiers[counter];
        pCounterInfo[counter].eventIndex      = m_pDfSpmEventIds[counter];
        pCounterInfo[counter].instance        = m_pDfSpmInstances[counter];
        pCounterInfo[counter].dataOffset      = static_cast<uint32>(curCounterDataOffset);
        pCounterInfo[counter].dataValidOffset = static_cast<uint32>(curCounterValidOffset);
        pCounterInfo[counter].dataSize        = sizeof(uint16);

        curCounterDataOffset  += CounterDataSizeInBytes;
        curCounterValidOffset += CounterValidSizeInBytes;
    }

    // Read pointer points to the first segment of the first sample.
    void* pSample = pSrcBufferStart;

    // Write pointer points to the beginning ofthe first counter data.
    uint16* pDstCounterData   = static_cast<uint16*>(VoidPtrInc(pDstBuffer, CounterDataOffset));
    uint32* pDstCounterValid  = static_cast<uint32*>(VoidPtrInc(pDstBuffer,
                                                                CounterDataOffset + TotalCounterDataSizeInBytes));
    void*   pOverflow         = VoidPtrInc(pSrcBufferStart, (OverFlowBitOffsetInBits / sizeof(uint8)));
    uint64* counter1And2      = nullptr;
    uint32  counter1          = 0;
    uint32  counter2          = 0;
    void*   pCounterValid     = nullptr;
    uint16* counterValid1And2 = nullptr;
    uint32  counterValid1     = 0;
    uint32  counterValid2     = 0;

    // Go through all counters 2 at a time
    for (uint32 counter = 0; counter < 4 && result == Result::Success; counter++)
    {
        // Move by 2 counters because that is 40 bits and 40 bits can be divided into bytes evenly
        pSample       = VoidPtrInc(pSrcBufferStart, TwoCounterSizeInBytes * counter);

        // Just reset the CounterValid pointer b/c all valid bits of a segment fit in the same uint16
        pCounterValid = VoidPtrInc(pSrcBufferStart, (CounterValidOffsetInBits/ sizeof(uint8)));

        for (int32 sample = 0; sample < m_numDfSpmSamples; sample++)
        {
            // The first two counters are stored in the first 40 bits. One in the first 20
            // and the second in the second 20.
            counter1And2 = static_cast<uint64*>(pSample);
            counter1     = *counter1And2 & 0xFFFFF;
            counter2     = (*counter1And2 >> OneCounterDataSizeInBits) & 0xFFFFF;

            // Check the counter valid bits
            counterValid1And2 = static_cast<uint16*>(pCounterValid);
            counterValid1 = ((*counterValid1And2) >> (4 + (counter * 2))) & 1;
            counterValid2 = ((*counterValid1And2) >> (5 + (counter * 2))) & 1;

            // Move to the next sample segment
            pSample       = VoidPtrInc(pSample, SegmentSizeInBytes);
            pCounterValid = VoidPtrInc(pCounterValid, SegmentSizeInBytes);

            // RGP SPM OUTPUT: write the delta values of the current counter for all samples.
            (*pDstCounterData) = static_cast<uint16>(counter1);
            pDstCounterData++;
            (*pDstCounterData) = static_cast<uint16>(counter2);
            pDstCounterData++;

            (*pDstCounterValid) = counterValid1;
            pDstCounterValid++;
            (*pDstCounterValid) = counterValid2;
            pDstCounterValid++;

        } // Iterate over samples.
    } // Iterate over counters.

    return result;
}

// =====================================================================================================================
// Returns the size of the SPM counter delta output if nullptr buffer is provided, or outputs the counter sample values
// into the buffer provided.
Result GpaSession::TraceSample::GetSpmTraceResults(
    void*  pDstBuffer,
    size_t bufferSize)
{
    // A valid destination buffer size is expected.
    PAL_ASSERT((bufferSize > 0) && (pDstBuffer != nullptr));

    // We assume these values are always the same.
    PAL_ASSERT(m_numSpmCounters == m_pSpmTraceLayout->numCounters);

    // This function writes the arrays that follow SqttFileChunkSpmDb to pDstBuffer:
    // 1. Timestamps[]
    // 2. SpmCounterInfo[]
    // 3. Counter values[]
    const size_t timestampDataSizeInBytes = m_numSpmSamples * sizeof(uint64);
    const size_t counterInfoSizeInBytes   = m_numSpmCounters * sizeof(SpmCounterInfo);
    const size_t counterDataOffset        = timestampDataSizeInBytes + counterInfoSizeInBytes;

    auto*const pDstTimestamps   = static_cast<uint64*>(pDstBuffer);
    auto*const pDstCounterInfos = static_cast<SpmCounterInfo*>(VoidPtrInc(pDstBuffer, timestampDataSizeInBytes));

    // The [start, end) range of valid samples in the SPM ring buffer. We'll need these to help handle ring wrapping.
    const size_t     sampleOffset = size_t(m_pSpmTraceLayout->offset + m_pSpmTraceLayout->sampleOffset);
    const void*const pRingStart   = VoidPtrInc(m_pPerfExpResults, sampleOffset);
    const void*const pRingEnd     = VoidPtrInc(pRingStart, m_pSpmTraceLayout->sampleStride * m_numSpmSamples);

    // Start of the spm counter data. First copy out every sample's 64-bit timestamp.
    const void* pSrcTimestamp = m_pOldestSample;

    for (uint32 sample = 0; sample < m_numSpmSamples; ++sample)
    {
        pDstTimestamps[sample] = *static_cast<const uint64*>(pSrcTimestamp);
        pSrcTimestamp          = VoidPtrInc(pSrcTimestamp, m_pSpmTraceLayout->sampleStride);

        // Once we reach the end of the ring we need to wrap back around just like the RLC does when writing to it.
        if (pSrcTimestamp == pRingEnd)
        {
            pSrcTimestamp = pRingStart;
        }
    }

    // The SpmCounterInfo and counter value array-of-arrays are both in counter order. This outer loop walks both
    // at the same time. We'll end up looping over the source counter data once for each counter.
    //
    // Most of the counter info comes directly from the perf experiment layout but we also need to track the byte
    // offset from the beginning of the RGP SPM chunk to this counter's data.
    size_t curCounterDataOffset = counterDataOffset;

    for (uint32 counter = 0; counter < m_numSpmCounters; counter++)
    {
        const SpmCounterData& layout       = m_pSpmTraceLayout->counterData[counter];
        SpmCounterInfo*const  pCounterInfo = pDstCounterInfos + counter;
#if (PAL_BUILD_BRANCH == 0) || (PAL_BUILD_BRANCH >= 2340)
        const uint32          dataSize     = layout.is32Bit ? sizeof(uint32) : sizeof(uint16);
#else
        const uint32          dataSize     = sizeof(uint16);
#endif

        // The cast below assumes this always fits.
        PAL_ASSERT(curCounterDataOffset < UINT32_MAX);

        pCounterInfo->block      = static_cast<SpmGpuBlock>(layout.gpuBlock);
        pCounterInfo->instance   = layout.instance;
        pCounterInfo->eventIndex = layout.eventId;
        pCounterInfo->dataOffset = uint32(curCounterDataOffset);
        pCounterInfo->dataSize   = dataSize;

        // The SPM source data is grouped by sample but we want individual arrays for each counter.
        // Walk the source buffer and do a sparse read for each sample, writing the value into the destination array.
        void*const  pDstValues = VoidPtrInc(pDstBuffer, curCounterDataOffset);
        const void* pSrcSample = m_pOldestSample;

        for (uint32 sample = 0; sample < m_numSpmSamples; sample++)
        {
            const uint16 valueLo = *static_cast<const uint16*>(VoidPtrInc(pSrcSample, layout.offsetLo));

            if (layout.is32Bit)
            {
#if (PAL_BUILD_BRANCH == 0) || (PAL_BUILD_BRANCH >= 2340)
                const uint32 valueHi = *static_cast<const uint16*>(VoidPtrInc(pSrcSample, layout.offsetHi));
                const uint32 value   = (valueHi << 16) | valueLo;

                static_cast<uint32*>(pDstValues)[sample] = value;
#else
                // PAL has just truncated 32-bit SPM to 16-bit for years now without any complaints so we'll just keep
                // doing that until RGP is ready for 32-bit data.
                static_cast<uint16*>(pDstValues)[sample] = valueLo;
#endif
            }
            else
            {
                static_cast<uint16*>(pDstValues)[sample] = valueLo;
            }

            pSrcSample = VoidPtrInc(pSrcSample, m_pSpmTraceLayout->sampleStride);

            // Once we reach the end of the ring we need to wrap back around just like the RLC does when writing to it.
            if (pSrcSample == pRingEnd)
            {
                pSrcSample = pRingStart;
            }
        }

        // Find the start of the next counter's data array.
        curCounterDataOffset += m_numSpmSamples * dataSize;
    }

    return Result::Success;
}

// =====================================================================================================================
// Parses the DF SPM trace buffer to find the number of samples of data written in the buffer.
uint32 GpaSession::TraceSample::CountNumDfSpmSamples() const
{
    // This offset is defined by HW. It is hardcoded in the chunk returned.
    constexpr uint32 LastSpmPktOffsetInBits = 252;
    constexpr uint32 SegmentSizeInBytes     = 32;

    // m_gcSampleDataOffset and m_pGcSampleDataBufferSize refer to the normal Counter/SQTT buffer. DF SPM data is
    // stored after that.
    const size_t location     = static_cast<size_t>(m_gcSampleDataOffset + m_pGcSampleDataBufferSize);
    const void*  pBufferStart = VoidPtrInc(m_pPerfExpResults, location);

    // Trace size is stored in metadata and is in 64-byte blocks and there are 2 samples per block.
    uint32      numRecords      = static_cast<const uint32*>(pBufferStart)[0] * 2;
    const void* pDataStart      = VoidPtrInc(pBufferStart, sizeof(DfSpmTraceMetadataLayout));
    const void* pLastSpmPktByte = VoidPtrInc(pDataStart, LastSpmPktOffsetInBits / 8);

    // Move to the second to last record and check the lastSpmPkt bit
    pLastSpmPktByte = VoidPtrInc(pLastSpmPktByte, (SegmentSizeInBytes * (numRecords - 2)));

    const uint32* pLastSpmPktBit = static_cast<const uint32*>(pLastSpmPktByte);
    const uint32  lastSpmPktBit  = ((*pLastSpmPktBit) >> (LastSpmPktOffsetInBits % 8)) & 1;

    if (lastSpmPktBit == 1)
    {
        numRecords -= 1;
    }

    return numRecords;
}

// =====================================================================================================================
// Parses the SPM ring buffer to find the number of samples of data written in the buffer.
void GpaSession::TraceSample::CountNumSpmSamples()
{
    // Default to zero if we hit some sort of issue.
    m_numSpmSamples = 0;

    const uint32 sampleStride  = m_pSpmTraceLayout->sampleStride;
    const uint32 maxNumSamples = m_pSpmTraceLayout->maxNumSamples;

    if ((sampleStride != 0) && (maxNumSamples != 0))
    {
        // Basic SPM trace layout.
        const void*const pStart       = VoidPtrInc(m_pPerfExpResults, size_t(m_pSpmTraceLayout->offset));
        const void*const pWrPtr       = VoidPtrInc(pStart, m_pSpmTraceLayout->wrPtrOffset);
        const void*const pFirstSample = VoidPtrInc(pStart, size_t(m_pSpmTraceLayout->sampleOffset));
        const void*const pLastSample  = VoidPtrInc(pFirstSample, (maxNumSamples - 1) * sampleStride);

        // The write pointer is a 32-bit offset relative to the start of the ring buffer. We need to multiply its value
        // by the wptrGranularity to convert it into an offset in bytes.
        const uint32 wrPtrInBytes = *static_cast<const uint32*>(pWrPtr) * m_pSpmTraceLayout->wrPtrGranularity;

        // The RLC should always write out complete bitlines.
        PAL_ASSERT((wrPtrInBytes % (MaxNumCountersPerBitline * sizeof(uint16))) == 0);

        // The amount of data written to the ring should be a multiple of our expected sample stride.
        PAL_ASSERT((wrPtrInBytes % sampleStride) == 0);

        // We aren't told how many times the ring buffer has wrapped so the write pointer may not directly tell us the
        // amount of written sample data. We need to inspect the ring data to figure out how many samples were written.
        // According to the palPerfExperiment.h comments, the last sample's timestamp will be non-zero only if the
        // ring has wrapped. Note that PAL always put the timestamp muxsels first in the muxsel RAMs so we can cast
        // the last sample directly to a 64-bit timestamp.
        const uint64 lastTs = *static_cast<const uint64*>(pLastSample);

        if (lastTs != 0)
        {
            // The ring must be full and may have wrapped one or more times. The write pointer's value indicates where
            // the oldest sample must be because its the location where the next sample would have been written.
            m_numSpmSamples = int32(maxNumSamples);
            m_pOldestSample = VoidPtrInc(pFirstSample, wrPtrInBytes);

            DbgLog(SeverityLevel::Warning, OriginationType::GpuProfiler, "GPUProfiler",
                   "SPM Buffer Wrapped. Larger buffer size recommended to avoid sample loss.");
        }
        else
        {
            // Zero isn't a valid timestamp so the ring must not have wrapped yet. The oldest sample is the first one.
            // The write pointer hasn't wrapped yet so it tells us how many bytes have been written so far.
            m_numSpmSamples = int32(wrPtrInBytes / sampleStride);
            m_pOldestSample = pFirstSample;
        }
    }
    else
    {
        // It doesn't seem reasonable that the user actually wanted an empty trace.
        PAL_ASSERT_ALWAYS();
    }
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

    m_pBeginTs = static_cast<uint64*>(VoidPtrInc(gpuMemInfo.pCpuAddr, static_cast<size_t>(offset)));

    offset += timestampAlignment; // Skip beginTs to get the address for endTs.
    m_pEndTsGpuMem      = gpuMemInfo.pGpuMemory;
    m_endTsGpuMemOffset = offset;

    m_pEndTs = static_cast<uint64*>(VoidPtrInc(gpuMemInfo.pCpuAddr, static_cast<size_t>(offset)));

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
                                                       nullptr,
                                                       pSizeInBytes,
                                                       pData,
                                                       0);
            }
        }
    }

    return result;
}

} // GpuUtil
