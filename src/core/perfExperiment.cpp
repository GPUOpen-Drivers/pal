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

#include "core/device.h"
#include "core/perfExperiment.h"

namespace Pal
{

// =====================================================================================================================
PerfExperiment::PerfExperiment(
    Device*                         pDevice,
    const PerfExperimentCreateInfo& createInfo,
    gpusize                         memAlignment)
    :
    m_device(*pDevice),
    m_createInfo(createInfo),
    m_memAlignment(memAlignment),
    m_isFinalized(false),
    m_hasGlobalCounters(false),
    m_hasThreadTrace(false),
    m_hasSpmTrace(false),
    m_globalBeginOffset(0),
    m_globalEndOffset(0),
    m_spmRingOffset(0),
    m_totalMemSize(0)
{
}

// =====================================================================================================================
void PerfExperiment::GetGpuMemoryRequirements(
    GpuMemoryRequirements* pGpuMemReqs
    ) const
{
    // We don't know this information until the perf experiment has been finalized.
    PAL_ASSERT(m_isFinalized);

    if (m_hasThreadTrace || m_hasSpmTrace)
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

    pGpuMemReqs->size      = m_totalMemSize;
    pGpuMemReqs->alignment = m_memAlignment;
}

// =====================================================================================================================
Result PerfExperiment::BindGpuMemory(
    IGpuMemory* pGpuMemory,
    gpusize     offset)
{
    Result result = Result::Success;

    if (m_isFinalized == false)
    {
        // The perf experiment must be finalized first.
        result = Result::ErrorUnavailable;
    }
    else
    {
        result = m_device.ValidateBindObjectMemoryInput(pGpuMemory, offset, m_totalMemSize, m_memAlignment, false);
    }

    if (result == Result::Success)
    {
        m_gpuMemory.Update(pGpuMemory, offset);
    }

    return result;
}

} // Pal
