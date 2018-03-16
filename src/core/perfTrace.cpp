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

#include "core/cmdStream.h"
#include "core/device.h"
#include "core/perfTrace.h"
#include "palDequeImpl.h"

namespace Pal
{

// =====================================================================================================================
// Implementation for PerfTrace
PerfTrace::PerfTrace(
    Device* pDevice)
    :
    m_device(*pDevice),
    m_dataOffset(0),
    m_dataSize(0)
{
}

// =====================================================================================================================
// SpmTrace base constructor.
SpmTrace::SpmTrace(
    Device* pDevice)
    :
    PerfTrace(pDevice),
    m_spmCounters(pDevice->GetPlatform()),
    m_spmInterval(0),
    m_numPerfCounters(0),
    m_pPerfCounterCreateInfos(nullptr),
    m_ctrLimitReached(false)
{
    m_flags.u16All = 0;
    memset(&m_muxselRamData, 0, sizeof(m_muxselRamData));
    memset(&m_segmentSizes, 0, sizeof(m_segmentSizes));
}

// =====================================================================================================================
// Adds a streaming counter to this SpmTrace.
Result SpmTrace::AddStreamingCounter(
    StreamingPerfCounter* pCounter)
{
    if (pCounter->IsIndexed())
    {
        m_flags.hasIndexedCounters = true;
    }

    return m_spmCounters.PushBack(pCounter);
}

// =====================================================================================================================
// Destructor has to free the memory stored in the Spm Counter list.
SpmTrace::~SpmTrace()
{
    while (m_spmCounters.NumElements() > 0)
    {
        // Pop the next counter object off of our list.
        StreamingPerfCounter* pCounter = nullptr;
        Result result                  = m_spmCounters.PopBack(&pCounter);

        PAL_ASSERT((result == Result::Success) && (pCounter != nullptr));

        // Destroy the performance counter object.
        PAL_SAFE_DELETE(pCounter, m_device.GetPlatform());
    }

    if (m_pPerfCounterCreateInfos != nullptr)
    {
        PAL_SAFE_FREE(m_pPerfCounterCreateInfos, m_device.GetPlatform());
    }
}

// =====================================================================================================================
// Finalizes the spm trace by calculating some key properties of the trace and the RLC mux select encodings.
Result SpmTrace::Finalize()
{
    PAL_ASSERT(m_spmCounters.NumElements() > 0);

    CalculateSegmentSizes();

    return CalculateMuxselRam();
}

// =====================================================================================================================
uint32 SpmTrace::GetMuxselRamDwords(
    uint32 seIndex
    ) const
{
    // We will always have at least one global line for the timestamp. This value can only be zero if
    // CalculateSegmentSize has not been called.
    PAL_ASSERT((m_segmentSizes[static_cast<uint32>(SpmDataSegmentType::Global)] != 0) &&
               (seIndex <= static_cast<uint32>(SpmDataSegmentType::Global)));

    constexpr uint32 NumDwordsPerBitLine = (NumBitsPerBitline / 32);

    return m_segmentSizes[seIndex] * NumDwordsPerBitLine;
}

// =====================================================================================================================
// Calculates the segment sizes based on the streaming performance counters requested.
void SpmTrace::CalculateSegmentSizes()
{
    const auto& chipProps = m_device.ChipProperties();

    // Array to track counter parity counts. Size is number of shader engines + 1 for global counters.
    ParityCount seParityCounts[static_cast<uint32>(SpmDataSegmentType::Count)];
    memset(&seParityCounts, 0, sizeof(seParityCounts));

    // Increment count in the global segment for GPU timestamp. The last element of the seParityCounts is used for
    // global counts.
    seParityCounts[static_cast<uint32>(SpmDataSegmentType::Global)].evenCount = 4;

    for (auto it = m_spmCounters.Begin(); it.Get(); it.Next())
    {
        // Check if block uses global or per-SE RLC HW.
        Pal::StreamingPerfCounter* pCounter = *it.Get();
        GpuBlock block                      = pCounter->BlockType();
        SpmDataSegmentType segType          = pCounter->GetSpmSegmentType();
        uint32  seIndex                     = static_cast<uint32>(segType);

        // Check if it is an even counter or an odd counter and increment the appropriate counts.
        for (uint32 i = 0; i < MaxNumStreamingCtrPerSummaryCtr; ++i)
        {
            if(pCounter->GetEventId(i) != StreamingPerfCounter::InvalidEventId)
            {
                const uint32 streamingCounterId = (block == GpuBlock::Sq) ? pCounter->GetSlot() :
                                                  (pCounter->GetSlot() * MaxNumStreamingCtrPerSummaryCtr + i);

                if (streamingCounterId % 2)
                {
                    seParityCounts[seIndex].oddCount++;
                }
                else
                {
                    seParityCounts[seIndex].evenCount++;
                }

                if ((seParityCounts[seIndex].oddCount > 31) || (seParityCounts[seIndex].evenCount > 31))
                {
                    m_ctrLimitReached = true;
                }
            }
        }
    }

    // Pad out the even/odd counts to the width of bit lines. There can be a maximum of 16 muxsels per bit line.
    for (uint32 i = 0; i < static_cast<uint32>(SpmDataSegmentType::Count); ++i)
    {
        if ((seParityCounts[i].evenCount % MuxselEntriesPerBitline) != 0)
        {
            seParityCounts[i].evenCount += MuxselEntriesPerBitline - (seParityCounts[i].evenCount %
                                                                          MuxselEntriesPerBitline);
        }

        if ((seParityCounts[i].oddCount % MuxselEntriesPerBitline) != 0)
        {
            seParityCounts[i].oddCount += MuxselEntriesPerBitline - (seParityCounts[i].oddCount %
                                                                         MuxselEntriesPerBitline);
        }
    }

    // Calculate number of bit lines of size 256-bits. This is used for the mux selects as well as the ring buffer.
    // Even lines hold counter0 and counter2, while odd lines hold counter1 and counter3. We need double of whichever
    // we have more of.
    // Example: If we have 32 global deltas coming from counter0 and counter2 and 16 deltas coming from counter1 and
    //          counter3, then we need four lines ( 2 * Max( 2 even, 1 odd)). Lines 0 and 2 hold the delta
    //          values coming from counter0,2 while Line 1 holds the delta values coming from counter1,3. Line 3 is
    //          empty.
    for (uint32 i = 0; i < static_cast<uint32>(SpmDataSegmentType::Count); ++i)
    {
        m_segmentSizes[i] = 2 * Util::Max(seParityCounts[i].evenCount / MuxselEntriesPerBitline,
                                          seParityCounts[i].oddCount / MuxselEntriesPerBitline);

        m_segmentSizes[static_cast<uint32>(SpmDataSegmentType::Count)] += m_segmentSizes[i];
    }
}

// =====================================================================================================================
// Calculates the mux-select encodings required for enabling the hardware to choose the appropriate counter among all
// {blocks, instances, counters} and output the counter value to the RLC ring buffer. These encodings will be written
// to the RLC muxsel RAM.
Result SpmTrace::CalculateMuxselRam()
{
    Result result = Result::Success;

    // Allocate memory for the muxsel ram data based on the segment size previously calculated.
    for (uint32 se = 0; se < static_cast<uint32>(SpmDataSegmentType::Count); se++)
    {
        const uint32 muxselDwords = GetMuxselRamDwords(se);

        if (muxselDwords != 0)
        {
            // We allocate the muxsel RAM space in dwords and write the muxsel RAM in RLC with write_data packets as
            // dwords, but we calculate and write the values in system memory as uint16.
            m_muxselRamData[se].pMuxselRamUint32 = static_cast<uint32*>(
                PAL_CALLOC(sizeof(uint32) * muxselDwords,
                           m_device.GetPlatform(),
                           Util::SystemAllocType::AllocInternal));

            // Memory allocation failed.
            PAL_ASSERT(m_muxselRamData[se].pMuxselRamUint32 != nullptr);

            if (m_muxselRamData[se].pMuxselRamUint32 == nullptr)
            {
                result = Result::ErrorOutOfMemory;
            }
        }
    }

    /*
    *    Example layout of the muxsel ram:
    *
    *      +---------------------+--------------------+---------------------+--
    * SE0: |       Even          |       Odd          |       Even          | ...
    *      +---------------------+--------------------+---------------------+--
    */

    struct MuxselWriteIndex
    {
        uint32 evenIndex;
        uint32 oddIndex;
    };

    // This stores the indices in the mux select ram data to which the next mux select must be written to.
    MuxselWriteIndex muxselWriteIndices[static_cast<uint32>(SpmDataSegmentType::Count)];

    // Initialize the muxsel write indices. Even indices start at 0, while odd indices start at 16.
    for (uint32 index = 0; index < static_cast<uint32>(SpmDataSegmentType::Count); index++)
    {
        muxselWriteIndices[index].evenIndex = 0;
        muxselWriteIndices[index].oddIndex  = MuxselEntriesPerBitline;
    }

    // Enter the muxsel encoding for GPU timestamp in the global section, in the even bit line.
    m_muxselRamData[static_cast<uint32>(SpmDataSegmentType::Global)].pMuxselRamUint32[0] = 0xF0F0F0F0;
    m_muxselRamData[static_cast<uint32>(SpmDataSegmentType::Global)].pMuxselRamUint32[1] = 0xF0F0F0F0;
    muxselWriteIndices[static_cast<uint32>(SpmDataSegmentType::Global)].evenIndex        = 4;

    // Iterate over our deque of counters and write out the muxsel ram data.
    for (auto iter = m_spmCounters.Begin(); iter.Get(); iter.Next())
    {
        StreamingPerfCounter* pCounter = *iter.Get();
        SpmDataSegmentType segType     = pCounter->GetSpmSegmentType();

        for (uint32 subSlot = 0; subSlot < MaxNumStreamingCtrPerSummaryCtr; ++subSlot)
        {
            if (pCounter->GetEventId(subSlot) != StreamingPerfCounter::InvalidEventId)
            {
                uint32* pWriteIndex   = nullptr;
                uint16 muxselEncoding = pCounter->GetMuxselEncoding(subSlot);
                const uint32 seIndex  = static_cast<uint32>(segType);

                // Write the mux select data in the appropriate location based on even/odd counterId (subSlot).
                if (subSlot % 2)
                {
                    pWriteIndex = &muxselWriteIndices[seIndex].oddIndex;
                }
                else
                {
                    pWriteIndex = &muxselWriteIndices[seIndex].evenIndex;
                }

                m_muxselRamData[seIndex].pMuxselRamUint16[*pWriteIndex] = muxselEncoding;

                // Find the offset into the output buffer for this counter.
                uint32 offset = *pWriteIndex;

                // Calculate offset within the sample for this counter's data. This is where the HW will write the
                // counter value. Use the offset as-is for the global block, since it is the first segment within the
                // sample. See the RLC SPM Microarchitecure spec for details regarding the output format.
                if (segType != SpmDataSegmentType::Global)
                {
                    // Skip the first segment which is the global segment.
                    offset += m_segmentSizes[static_cast<uint32>(SpmDataSegmentType::Global)] * NumWordsPerBitLine;

                    // Se1
                    if (seIndex > 0)
                    {
                        offset += m_segmentSizes[static_cast<uint32>(SpmDataSegmentType::Se0)] *  NumWordsPerBitLine;
                    }

                    if (seIndex > 1)
                    {
                        offset += m_segmentSizes[static_cast<uint32>(SpmDataSegmentType::Se1)] * NumWordsPerBitLine;
                    }

                    if (seIndex > 2)
                    {
                        offset += m_segmentSizes[static_cast<uint32>(SpmDataSegmentType::Se3)] * NumWordsPerBitLine;
                    }
                }

                // Offset 0 to 3 holds the GPU timestamp.
                PAL_ASSERT(offset > 3);
                pCounter->SetDataOffset(subSlot, offset);

                ++(*pWriteIndex);

                // Advance the write index to the next even/odd section once 16 mux selects have been written in the
                // current section.
                if ((*pWriteIndex % MuxselEntriesPerBitline) == 0)
                {
                    (*pWriteIndex) += MuxselEntriesPerBitline;
                }
            } // Valid eventID.
        } // Iterate over subSlots in the counter.
    } // Iterate over StreamingPerfCounters.

    return result;
}

// =====================================================================================================================
Result SpmTrace::GetTraceLayout(
    SpmTraceLayout* pLayout
    ) const
{
    Result result = Result::Success;

    pLayout->offset       = m_dataOffset;
    pLayout->wptrOffset   = m_dataOffset;       // The very first dword is the wptr.
    pLayout->sampleOffset = 8 * sizeof(uint32); // Data begins 8 dwords from the beginning of the buffer.

    constexpr uint32 NumBytesPerBitLine = (NumBitsPerBitline / 8);

    // Fill in the segment parents.
    pLayout->sampleSizeInBytes =
        m_segmentSizes[static_cast<uint32>(SpmDataSegmentType::Count)] * NumBytesPerBitLine;

    pLayout->segmentSizeInBytes[static_cast<uint32>(SpmDataSegmentType::Global)] =
        m_segmentSizes[static_cast<uint32>(SpmDataSegmentType::Global)] * NumBytesPerBitLine;
    pLayout->segmentSizeInBytes[static_cast<uint32>(SpmDataSegmentType::Se0)] =
        m_segmentSizes[static_cast<uint32>(SpmDataSegmentType::Se0)] * NumBytesPerBitLine;
    pLayout->segmentSizeInBytes[static_cast<uint32>(SpmDataSegmentType::Se1)] =
        m_segmentSizes[static_cast<uint32>(SpmDataSegmentType::Se1)] * NumBytesPerBitLine;
    pLayout->segmentSizeInBytes[static_cast<uint32>(SpmDataSegmentType::Se2)] =
        m_segmentSizes[static_cast<uint32>(SpmDataSegmentType::Se2)] * NumBytesPerBitLine;
    pLayout->segmentSizeInBytes[static_cast<uint32>(SpmDataSegmentType::Se3)] =
        m_segmentSizes[static_cast<uint32>(SpmDataSegmentType::Se3)] * NumBytesPerBitLine;

    // There must be enough space in the layout allocation for all the counters that were requested.
    PAL_ASSERT(pLayout->numCounters == m_numPerfCounters);

    // Fill in the SpmCounterInfo array.
    for (uint32 i = 0; i < m_numPerfCounters; ++i)
    {
        for (auto iter = m_spmCounters.Begin(); iter.Get() != nullptr; iter.Next())
        {
            Pal::StreamingPerfCounter* pHwCounter = *(iter.Get());

            if ((m_pPerfCounterCreateInfos[i].block    == pHwCounter->BlockType()) &&
                (m_pPerfCounterCreateInfos[i].instance == pHwCounter->GetInstanceId()))
            {
                for (uint32 subSlot = 0; subSlot < MaxNumStreamingCtrPerSummaryCtr; subSlot++)
                {
                    const uint32 eventId = pHwCounter->GetEventId(subSlot);

                    if (m_pPerfCounterCreateInfos[i].eventId == eventId)
                    {
                        // We have found the matching HW counter and the API counter.
                        pLayout->counterData[i].offset   = pHwCounter->GetDataOffset(subSlot);
                        pLayout->counterData[i].segment  = pHwCounter->GetSpmSegmentType();
                        pLayout->counterData[i].eventId  = eventId;
                        pLayout->counterData[i].gpuBlock = m_pPerfCounterCreateInfos[i].block;
                        pLayout->counterData[i].instance = m_pPerfCounterCreateInfos[i].instance;
                    }
                }
            }
        }
    }

    pLayout->wptrOffset = 0;

    return result;
}

// =====================================================================================================================
// Implementation for ThreadTrace
ThreadTrace::ThreadTrace(
    Device*              pDevice,
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 373
    const PerfTraceInfo& info)
#else
    const ThreadTraceInfo& info)
#endif
    :
    PerfTrace(pDevice),
    m_shaderEngine(info.instance),
    m_infoOffset(0),
    m_infoSize(sizeof(ThreadTraceInfoData))
{
}

} // Pal
