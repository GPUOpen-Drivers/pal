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

#include "core/cmdBuffer.h"
#include "core/device.h"
#include "core/perfCounter.h"
#include "core/perfExperiment.h"
#include "core/perfTrace.h"
#include "core/platform.h"
#include "palDequeImpl.h"
#include "palHashMapImpl.h"

namespace Pal
{

// =====================================================================================================================
PerfExperiment::PerfExperiment(
    Device*                         pDevice,
    const PerfExperimentCreateInfo& info)
    :
    m_info(info),
    m_vidMem(),
    m_ctrBeginOffset(0),
    m_ctrEndOffset(0),
    m_thdTraceOffset(0),
    m_spmTraceOffset(0),
    m_totalMemSize(0),
    m_globalCtrs(pDevice->GetPlatform()),
    m_numThreadTrace(0),
    m_pSpmTrace(nullptr),
    m_device(*pDevice),
    m_shaderMask(PerfShaderMaskAll)
{
    // Clear the array of thread trace pointers.
    memset(&m_pThreadTrace[0], 0, sizeof(m_pThreadTrace));

    // Initialize the flags to zero.
    m_flags.u32All = 0;

    // Setup the defaults for the perf-experiment flags.
    m_flags.cacheFlushOnPerfCounter  = (info.optionFlags.cacheFlushOnCounterCollection) ?
                                       info.optionValues.cacheFlushOnCounterCollection : 0;

    m_flags.sampleInternalOperations = (info.optionFlags.sampleInternalOperations) ?
                                       info.optionValues.sampleInternalOperations : 0;

    m_shaderMask = (info.optionFlags.sqShaderMask) ?
                   info.optionValues.sqShaderMask : PerfExperimentShaderFlags::PerfShaderMaskAll;
}

// =====================================================================================================================
PerfExperiment::~PerfExperiment()
{
    // Need to clean up all of the global counters added to this Experiment.
    while (m_globalCtrs.NumElements() > 0)
    {
        // Pop the next counter object off of our list.
        PerfCounter* pCounter = nullptr;
        Result result = m_globalCtrs.PopBack(&pCounter);
        PAL_ASSERT((result == Result::Success) && (pCounter != nullptr));

        // Destroy the performance counter object.
        PAL_SAFE_DELETE(pCounter, m_device.GetPlatform());
    }

    // Need to clean up all of the thread trace objects added to this Experiment.
    for (size_t idx = 0; idx < MaxNumThreadTrace; ++idx)
    {
        PAL_SAFE_DELETE(m_pThreadTrace[idx], m_device.GetPlatform());
    }

    PAL_SAFE_DELETE(m_pSpmTrace, m_device.GetPlatform());
}

// =====================================================================================================================
// Adds a new summary performance counter to this Experiment.
Result PerfExperiment::AddCounter(
    const PerfCounterInfo& info)
{
    Result result = ValidatePerfCounterInfo(info);

    if ((result == Result::Success) && IsFinalized())
    {
        // Cannot add counters to an already-finalized Experiment!
        result = Result::ErrorUnavailable;
    }

    if (result == Result::Success)
    {
        // Only global (summary) counters are expected in this path.
        PAL_ASSERT(info.counterType == PerfCounterType::Global);

        // Delegate to the HWL for counter creation.
        PerfCounter* pCounter = nullptr;
        result = CreateCounter(info, &pCounter);

        if (result == Result::Success)
        {
            PAL_ASSERT(pCounter != nullptr);
            result = m_globalCtrs.PushBack(pCounter);

            if (result != Result::Success)
            {
                // Something went wrong when adding the counter to our list...
                // need to clean-up the counter now to prevent leaks.
                PAL_SAFE_DELETE(pCounter, m_device.GetPlatform());
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Adds a new streaming counter trace to this Perf Experiment.
Result PerfExperiment::AddSpmTrace(
    const SpmTraceCreateInfo& info)
{
    // Cannot add traces to an already-finalized Experiment!
    Result result = Result::ErrorUnavailable;

    if (IsFinalized() == false)
    {
        result = CreateSpmTrace(info);
    }

    return result;
}

// =====================================================================================================================
// Performs a bit of validation that the client cannot do.
Result PerfExperiment::ValidatePerfCounterInfo(
    const PerfCounterInfo& info
    ) const
{
    Result result = Result::Success;

    const auto& flags = info.optionFlags;

    if (((flags.sqSimdMask) || (flags.sqSqcBankMask) || (flags.sqSqcClientMask)) && (info.block != GpuBlock::Sq))
    {
        result = Result::ErrorUnavailable;
    }

    if ((result == Result::Success)                        &&
        ((flags.sqSqcBankMask) || (flags.sqSqcClientMask)) &&
        (m_device.ChipProperties().gfxLevel < GfxIpLevel::GfxIp7))
    {
        result = Result::ErrorIncompatibleDevice;
    }

    return result;
}

// =====================================================================================================================
// Adds a new trace to this Experiment.
Result PerfExperiment::AddThreadTrace(
    const ThreadTraceInfo& info)
{
    Result result = Result::Success;

    if (IsFinalized())
    {
        // Cannot add traces to an already-finalized Experiment!
        result = Result::ErrorUnavailable;
    }

    if (result == Result::Success)
    {
        if (info.traceType == PerfTraceType::ThreadTrace)
        {
            if (m_pThreadTrace[info.instance] == nullptr)
            {
                // Delegate to the HWL for thread trace creation.
                result = CreateThreadTrace(info);
                // Assert that either the new thread trace object is valid, or that something
                // failed during trace creation.
                PAL_ASSERT((m_pThreadTrace[info.instance] != nullptr) || (result != Result::Success));

            }
            else
            {
                // The requested Shader Engine already has a thread trace assigned. It is not allowed to have more than
                // one per Engine.
                result = Result::ErrorUnavailable;
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Finalizes this performance Experiment, preparing it for use withhin a command buffer.
Result PerfExperiment::Finalize()
{
    // Assume the operation fails due to already being in the 'Finalized' state.
    Result result = Result::Success;

    if (IsFinalized() == false)
    {
        if (HasGlobalCounters())
        {
            // Loop over each global performance counter and accumulate the memory size needed.
            gpusize counterTotalSize = 0;
            for (auto it = m_globalCtrs.Begin(); it.Get(); it.Next())
            {
                PerfCounter*const pCounter = (*it.Get());
                PAL_ASSERT(pCounter != nullptr);

                // Update the current counter's data offset and accumulate the memory needed
                // for counter samples.
                pCounter->SetDataOffset(counterTotalSize);
                counterTotalSize += pCounter->GetSampleSize();
            }

            // NOTE: We need two samples for each counter: one for the 'begin' sample and another for the 'end' sample.
            //       Start the counter begin samples at the beginning of any memory binding we later receive, and the
            //       end samples immediately thereafter (including alignment).

            m_ctrBeginOffset = 0;
            m_ctrEndOffset   = Util::Pow2Align(counterTotalSize, PerfExperimentAlignment);

            // Current running total memory size is the global counter end offset plus the total counter sample size.
            m_totalMemSize = (m_ctrEndOffset + counterTotalSize);
        }

        if (HasThreadTraces())
        {
            // NOTE: The info and data segments for thread traces have different alignment requirements, and don't need
            //       to be in contiguous regions of GPU memory. We save some memory space by packing all of the info
            //       segments together and all of the data segments together.

            // Loop over each valid thread trace and accumulate the memory needed for their info segments. The info
            // segments begin where any global counters left off.
            for (size_t idx = 0; idx < MaxNumThreadTrace; ++idx)
            {
                if (m_pThreadTrace[idx] != nullptr)
                {
                    // Make sure info segment is properly aligned.
                    m_totalMemSize = Util::Pow2Align(m_totalMemSize, m_pThreadTrace[idx]->GetInfoAlignment());

                    // Update the current trace's info offset and accumulate the memory needed.
                    m_pThreadTrace[idx]->SetInfoOffset(m_totalMemSize);
                    m_totalMemSize += m_pThreadTrace[idx]->GetInfoSize();
                }
            }

            // Loop over each valid thread trace and accumulate the memory needed for their
            // data segments. The data segments begin where info segments left off.
            for (size_t idx = 0; idx < MaxNumThreadTrace; ++idx)
            {
                if (m_pThreadTrace[idx] != nullptr)
                {
                    // Make sure data segment is properly aligned.
                    m_totalMemSize = Util::Pow2Align(m_totalMemSize, m_pThreadTrace[idx]->GetDataAlignment());

                    // Update the current trace's data offset and accumulate the memory needed.
                    m_pThreadTrace[idx]->SetDataOffset(m_totalMemSize);
                    m_totalMemSize += m_pThreadTrace[idx]->GetDataSize();
                }
            }
        }

        if (HasSpmTrace())
        {
             // Finalize the spm trace based on the create info.
             result = m_pSpmTrace->Finalize();

            // Set the start offset of spm data. The Spm trace data starts from where the thread trace results end.
            m_pSpmTrace->SetDataOffset(m_totalMemSize);

            // The size of the spm data is the size of the ring buffer allocated.
            m_totalMemSize += m_pSpmTrace->GetRingSize();
        }

        // Mark this Experiment as 'finalized'.
        m_flags.isFinalized = 1;
    }
    else
    {
        result = Result::ErrorUnavailable;
    }

    return result;
}

// =====================================================================================================================
Result PerfExperiment::GetGlobalCounterLayout(
    GlobalCounterLayout* pLayout
    ) const
{
    PAL_ASSERT(pLayout != nullptr);
    Result result = Result::ErrorInvalidValue;

    const size_t numSamples = m_globalCtrs.NumElements();

    if ((pLayout->sampleCount >= numSamples) && IsFinalized())
    {
        // Populate the output buffer with the sample layout data.
        GlobalSampleLayout* pSample = pLayout->samples;

        pLayout->sampleCount = static_cast<uint32>(numSamples);
        for (auto it = m_globalCtrs.Begin(); it.Get(); it.Next())
        {
            PerfCounter*const pCounter = (*it.Get());
            PAL_ASSERT(pCounter != nullptr);

            pSample->block            = pCounter->BlockType();
            pSample->instance         = pCounter->GetInstanceId();
            pSample->eventId          = pCounter->GetEventId();
            pSample->slot             = pCounter->GetSlot();
            pSample->beginValueOffset = (pCounter->GetDataOffset() + m_ctrBeginOffset);
            pSample->endValueOffset   = (pCounter->GetDataOffset() + m_ctrEndOffset);

            switch (pCounter->GetSampleSize())
            {
            case sizeof(uint32):
                pSample->dataType = PerfCounterDataType::Uint32;
                break;
            case sizeof(uint64):
                pSample->dataType = PerfCounterDataType::Uint64;
                break;
            default:
                PAL_ASSERT_ALWAYS();
                break;
            }
            // Advance to the next sample layout.
            ++pSample;
        }

        result = Result::Success;
    }
    else if (pLayout->sampleCount == 0)
    {
        pLayout->sampleCount = static_cast<uint32>(numSamples);
        result = Result::Success;
    }

    return result;
}

// =====================================================================================================================
Result PerfExperiment::GetThreadTraceLayout(
    ThreadTraceLayout* pLayout
    ) const
{
    PAL_ASSERT(pLayout != nullptr);
    Result       result          = Result::ErrorInvalidValue;
    const size_t numTraceLayouts = m_numThreadTrace;

    if ((pLayout->traceCount >= numTraceLayouts) && IsFinalized())
    {
        // Populate the output buffer with the thread trace layout data.
        ThreadTraceSeLayout* pSeLayout = pLayout->traces;

        pLayout->traceCount = static_cast<uint32>(m_numThreadTrace);
        for (size_t idx = 0; idx < MaxNumThreadTrace; ++idx)
        {
            if (m_pThreadTrace[idx] != nullptr)
            {
                pSeLayout->shaderEngine = m_pThreadTrace[idx]->GetShaderEngine();
                pSeLayout->computeUnit  = m_pThreadTrace[idx]->GetComputeUnit();
                pSeLayout->infoOffset   = m_pThreadTrace[idx]->GetInfoOffset();
                pSeLayout->infoSize     = m_pThreadTrace[idx]->GetInfoSize();
                pSeLayout->dataOffset   = m_pThreadTrace[idx]->GetDataOffset();
                pSeLayout->dataSize     = m_pThreadTrace[idx]->GetDataSize();
                // Advance to the next SE's trace layout.
                ++pSeLayout;
            }
        }

        result = Result::Success;
    }
    else if (pLayout->traceCount == 0)
    {
        pLayout->traceCount = static_cast<uint32>(m_numThreadTrace);
        result = Result::Success;
    }

    return result;
}

// =====================================================================================================================
Result PerfExperiment::GetSpmTraceLayout(
    SpmTraceLayout* pLayout
    ) const
{
    PAL_ASSERT((pLayout != nullptr) && IsFinalized());

    return m_pSpmTrace->GetTraceLayout(pLayout);
}

// =====================================================================================================================
void PerfExperiment::GetGpuMemoryRequirements(
    GpuMemoryRequirements* pGpuMemReqs
    ) const
{
    if (HasThreadTraces() || HasSpmTrace())
    {
        pGpuMemReqs->heapCount = 2;
        pGpuMemReqs->heaps[0]  = Pal::GpuHeapInvisible;
        pGpuMemReqs->heaps[1]  = Pal::GpuHeapLocal;
    }
    else
    {
        pGpuMemReqs->heapCount = 1;
        pGpuMemReqs->heaps[0]  = Pal::GpuHeapGartUswc;
    }

    pGpuMemReqs->size                  = m_totalMemSize;
    pGpuMemReqs->alignment             = PerfExperimentAlignment;
}

// =====================================================================================================================
Result PerfExperiment::BindGpuMemory(
    IGpuMemory* pGpuMemory,
    gpusize     offset)
{
    Result result = m_device.ValidateBindObjectMemoryInput(pGpuMemory,
                                                           offset,
                                                           m_totalMemSize,
                                                           PerfExperimentAlignment,
                                                           false);

    if (result == Result::Success)
    {
        m_vidMem.Update(pGpuMemory, offset);
    }

    return result;
}

// =====================================================================================================================
void PerfExperiment::Destroy()
{
    this->~PerfExperiment();
}

// =====================================================================================================================
// Creates and configures an SpmTrace object.
Result PerfExperiment::CreateSpmTrace(
    const SpmTraceCreateInfo& info)
{
    // Call into the hardware layer to create an spm trace object.
    Result result = ConstructSpmTraceObj(info, &m_pSpmTrace);

    if (result == Result::Success)
    {
        result = m_pSpmTrace->Init(info);
    }

    if(result == Result ::Success)
    {
        Util::HashMap<BlockUsageKey, StreamingPerfCounter*, Platform> spmCounterUsageMap(32, m_device.GetPlatform());
        result = spmCounterUsageMap.Init();

        // Iterate through the list of perf counters provided for this SpmTrace.
        for (uint32 i = 0; (i < info.numPerfCounters) && (result == Result::Success); ++i)
        {
            // Call into the hw layer to get the number of streaming counters supported for this block.
            const uint32 numCounters = GetNumStreamingCounters(static_cast<uint32>(info.pPerfCounterInfos[i].block));

            // Iterate over the number of registers(counters) that can be used as streaming counters in this block.
            // The result can be in Error state until this loop is done.
            for (uint32 counterIdx = 0; counterIdx < numCounters; counterIdx++)
            {
                BlockUsageKey key = { info.pPerfCounterInfos[i].block, info.pPerfCounterInfos[i].instance, counterIdx };

                StreamingPerfCounter** ppStreamingCounter = nullptr;
                bool                   existed            = false;

                result = spmCounterUsageMap.FindAllocate(key, &existed, &ppStreamingCounter);

                if (result == Result::Success)
                {
                    if (existed)
                    {
                        // Attempt to add this perf counter if the streaming perf counter already exists in the hash map
                        // If all the slots in this HW counter are full, then we move on to the next HW counter in this
                        // instance.
                        result = (*ppStreamingCounter)->AddEvent(info.pPerfCounterInfos[i].block,
                                                                 info.pPerfCounterInfos[i].eventId);
                    }
                    else
                    {
                        // Create a new StreamingPerfCounter and add it to the hashmap. The SpmTrace object is
                        // responsible for freeing this memory allocated for each StreamingPerfCounter.
                        StreamingPerfCounter* pNewCounter =
                            CreateStreamingPerfCounter(info.pPerfCounterInfos[i].block,
                                                       info.pPerfCounterInfos[i].instance,
                                                       counterIdx);

                        // Allocation succeeded, so create a StreamingPerfCounter object and add it to the hash map.
                        if (pNewCounter != nullptr)
                        {
                            result = pNewCounter->AddEvent(info.pPerfCounterInfos[i].block,
                                                           info.pPerfCounterInfos[i].eventId);

                            if (result == Result::Success)
                            {
                                // Update the counter flags
                                UpdateCounterFlags(info.pPerfCounterInfos[i].block, pNewCounter->IsIndexed());

                                (*ppStreamingCounter) = pNewCounter;
                            }
                        }
                        else
                        {
                            // Allocation of StreamingPerfCounter failed.
                            result = Result::ErrorOutOfMemory;
                            break;
                        }
                    }
                }

                if (result == Result::Success)
                {
                    // We have either added a perf counter to an existing StreamingPerfCounter or we have
                    // succesfully created a new StreamingPerfCounter and added our perf counter to it. We can skip
                    // to the outer loop for the next perf counter.
                    break;
                }
            } // End iterate over HW streaming counter registers.
        } // End iterate over requested perf counters.

        PAL_ASSERT(result == Result::Success);

        // Add all the StreamingPerfCounters from the hashmap to the SpmTrace.
        for (auto iter = spmCounterUsageMap.Begin(); (iter.Get() && (result == Result::Success)); iter.Next())
        {
            result = m_pSpmTrace->AddStreamingCounter(static_cast<Pal::StreamingPerfCounter*>(iter.Get()->value));
        }
    }
    return result;
}

} // Pal
