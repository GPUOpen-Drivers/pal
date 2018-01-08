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
    m_pPerfCounterCreateInfos(nullptr)
{
    m_flags.u16All = 0;
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
// Encodes the counter information to a format expected by the RLC for the streaming counter mux selects.
// The format is as follows: Bits [0-5]: counterId, [6-10]: block, [11:15]: instance. The encodings are defined in
// HW spec for SPM.
PerfmonSelData SpmTrace::GetGlobalMuxselData(
    GpuBlock block,
    uint32   instance,  // Per-SE instance index.
    uint32   counterId  // Per-instance counterID.
    ) const
{
    PerfmonSelData muxselData = {};
    muxselData.counter        = counterId;
    muxselData.instance       = instance;

    switch (block)
    {
    case GpuBlock::Cpg:
        muxselData.block = 0;
        break;
    case GpuBlock::Cpc:
        muxselData.block = 1;
        break;
    case GpuBlock::Cpf:
        muxselData.block = 2;
        break;
    case GpuBlock::Gds:
        muxselData.block = 3;
        break;
    case GpuBlock::Tcc:
        muxselData.block = 4;
        break;
    case GpuBlock::Tca:
        muxselData.block = 5;
        break;
    case GpuBlock::Ia:
        muxselData.block = 6;
        break;
    case GpuBlock::Tcs:
        muxselData.block = 7;
        break;
    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    return muxselData;
}

// =====================================================================================================================
// Encodes the counter information to a format expected by the RLC for the streaming counter mux selects.
// The format is as follows: Bits [0-5]: counterId, [6-10]: block, [11:15]: instance. The encodings are defined in
// HW spec for SPM.
// #SEE: //gfxip/gcB/doc/design/blocks/rlc/73_Stream_Perfmon_v1_12.docx
PerfmonSelData SpmTrace::GetPerSeMuxselData(
    GpuBlock block,
    uint32   instance,  // Per-SE instance index.
    uint32   counterId  // Per-instance counterID.
    ) const
{
    PerfmonSelData muxselData = {};
    muxselData.counter        = counterId;
    muxselData.instance       = instance;

    switch (block)
    {
    case GpuBlock::Cb:
        muxselData.block = 0;
        break;
    case GpuBlock::Db:
        muxselData.block = 1;
        break;
    case GpuBlock::Pa:
        muxselData.block = 2;
        break;
    case GpuBlock::Sx:
        muxselData.block = 3;
        break;
    case GpuBlock::Sc:
        muxselData.block = 4;
        break;
    case GpuBlock::Ta:
        muxselData.block = 5;
        break;
    case GpuBlock::Td:
        muxselData.block = 6;
        break;
    case GpuBlock::Tcp:
        muxselData.block = 7;
        break;
    case GpuBlock::Spi:
        muxselData.block = 8;
        break;
    case GpuBlock::Sq:
        muxselData.block = 9;
        break;
    case GpuBlock::Vgt:
        muxselData.block = 10;
        break;
    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    return muxselData;
}

// =====================================================================================================================
// Returns true if the GpuBlock uses the global muxsel HW resources.
bool SpmTrace::BlockUsesGlobalMuxsel(
    GpuBlock gpuBlock
    ) const
{
    bool globalBlock = false;

    switch (gpuBlock)
    {
    case GpuBlock::Cpg:
    case GpuBlock::Cpc:
    case GpuBlock::Cpf:
    case GpuBlock::Gds:
    case GpuBlock::Tcc:
    case GpuBlock::Tca:
    case GpuBlock::Ia:
    case GpuBlock::Tcs:
        globalBlock = true;
        break;

    default:
        break;
    }

    return globalBlock;
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
