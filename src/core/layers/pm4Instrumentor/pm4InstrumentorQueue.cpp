/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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

#if PAL_DEVELOPER_BUILD

#include "core/layers/pm4Instrumentor/pm4InstrumentorCmdBuffer.h"
#include "core/layers/pm4Instrumentor/pm4InstrumentorDevice.h"
#include "core/layers/pm4Instrumentor/pm4InstrumentorPlatform.h"
#include "core/layers/pm4Instrumentor/pm4InstrumentorQueue.h"
#include "palFile.h"
#include "palInlineFuncs.h"
#include "palSysUtil.h"
#include "palVectorImpl.h"

using namespace Util;

namespace Pal
{
namespace Pm4Instrumentor
{

// =====================================================================================================================
static const char* QueueTypeToString(
    QueueType value)
{
    const char*const StringTable[] =
    {
        "Universal",   // QueueTypeUniversal
        "Compute",     // QueueTypeCompute
        "Dma",         // QueueTypeDma
        "Timer",       // QueueTypeTimer
    };

    static_assert(ArrayLen(StringTable) == QueueTypeCount,
                  "The QueueTypeToString string table needs to be updated.");

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < QueueTypeCount);

    return StringTable[idx];
}

// =====================================================================================================================
static const char* InternalEventIdToString(
    InternalEventId id)
{
    const char*const StringTable[] =
    {
        "ValidateComputeUserData()",    // UserDataValidationCs
        "ValidateGraphicsUserData()",   // UserDataValidationGfx
        "ValidateComputePipeline",      // PipelineValidationCs
        "ValidateGraphicsPipeline",     // PipelineValidationGfx
        "ValidateDispatch()",           // MiscDispatchValidation
        "ValidateDraw()",               // MiscDrawValidation
    };

    static_assert(ArrayLen(StringTable) == NumEventIds,
                  "The InternalEventIdToString string table needs to be updated.");

    const uint32 idx = static_cast<uint32>(id);
    PAL_ASSERT(idx < NumEventIds);

    return StringTable[idx];
}

// =====================================================================================================================
Queue::Queue(
    IQueue* pNextQueue,
    Device* pDevice)
    :
    QueueDecorator(pNextQueue, pDevice),
    m_pDevice(pDevice),
    m_cmdBufCount(0),
    m_shRegs(static_cast<Platform*>(pDevice->GetPlatform())),
    m_ctxRegs(static_cast<Platform*>(pDevice->GetPlatform())),
    m_shRegBase(0),
    m_ctxRegBase(0),
    m_dumpMode(Pm4InstrumentorDumpQueueDestroy),
    m_dumpInterval(0),
    m_lastCpuPerfCounter(0)
{
    memset(&m_stats,    0, sizeof(m_stats));
    memset(&m_fileName, 0, sizeof(m_fileName));

    const Pal::PalPlatformSettings& settings = m_pDevice->GetPlatform()->PlatformSettings();

    if (settings.pm4InstrumentorConfig.dumpMode == Pm4InstrumentorDumpQueueSubmit)
    {
        m_dumpMode = Pm4InstrumentorDumpQueueSubmit;
        m_dumpInterval = (GetPerfFrequency() * settings.pm4InstrumentorConfig.dumpInterval);
    }

    Snprintf(&m_fileName[0],
        sizeof(m_fileName),
        "%s/%sQueue-0x%p-%s",
        m_pDevice->GetPlatform()->LogDirPath(),
        QueueTypeToString(m_pNextLayer->Type()),
        this,
        &settings.pm4InstrumentorConfig.filenameSuffix[0]);
}

// =====================================================================================================================
Queue::~Queue()
{
    if (m_dumpMode == Pm4InstrumentorDumpQueueDestroy)
    {
        DumpStatistics();
    }
}

// =====================================================================================================================
Result Queue::Submit(
    const MultiSubmitInfo& submitInfo)
{
    PAL_ASSERT_MSG((submitInfo.perSubQueueInfoCount <= 1),
                   "Multi-Queue support has not yet been implemented in Pm4Instrumentor!");

    if (submitInfo.pPerSubQueueInfo != nullptr)
    {
        AccumulateStatistics(submitInfo.pPerSubQueueInfo[0].ppCmdBuffers,
                             submitInfo.pPerSubQueueInfo[0].cmdBufferCount);
    }

    if (m_dumpMode == Pm4InstrumentorDumpQueueSubmit)
    {
        const int64 currentCounter = GetPerfCpuTime();
        if ((currentCounter - m_lastCpuPerfCounter) >= m_dumpInterval)
        {
            DumpStatistics();
            m_lastCpuPerfCounter = currentCounter;
        }
    }

    return QueueDecorator::Submit(submitInfo);
}

// =====================================================================================================================
// Helper function to accumulate optimized register statistics across multiple command buffers.
static void AccumulateRegisterInfo(
    RegisterInfoVector*       pAccum,
    const RegisterInfoVector& source)
{
    if ((source.IsEmpty() == false) && (pAccum->Reserve(source.NumElements()) == Result::Success))
    {
        if (pAccum->NumElements() == 0)
        {
            for (auto s = source.Begin(); s.IsValid(); s.Next())
            {
                pAccum->PushBack(s.Get());
            }
        }
        else if (pAccum->NumElements() == source.NumElements())
        {
            for (uint32 i = 0; i < source.NumElements(); ++i)
            {
                pAccum->At(i).setPktTotal += source.At(i).setPktTotal;
                pAccum->At(i).setPktKept  += source.At(i).setPktKept;
            }
        }
    }
}

// =====================================================================================================================
// Accumulates the aggregate PM4 statistics from a group of command buffers.
void Queue::AccumulateStatistics(
    const ICmdBuffer*const* ppCmdBuffers,
    uint32                  count)
{
    for (uint32 i = 0; i < count; ++i)
    {
        const CmdBuffer*const pCmdBuf = static_cast<const CmdBuffer*>(ppCmdBuffers[i]);
        const Pm4Statistics&  stats   = pCmdBuf->Statistics();

        for (uint32 j = 0; j < NumCallIds; ++j)
        {
            m_stats.call[j].cmdSize += stats.call[j].cmdSize;
            m_stats.call[j].count   += stats.call[j].count;
        }

        for (uint32 j = 0; j < NumEventIds; ++j)
        {
            m_stats.internalEvent[j].cmdSize += stats.internalEvent[j].cmdSize;
            m_stats.internalEvent[j].count   += stats.internalEvent[j].count;
        }

        m_stats.commandBufferSize += stats.commandBufferSize;
        m_stats.embeddedDataSize  += stats.embeddedDataSize;
        m_stats.gpuScratchMemSize += stats.gpuScratchMemSize;

        AccumulateRegisterInfo(&m_shRegs,  pCmdBuf->ShRegs());
        AccumulateRegisterInfo(&m_ctxRegs, pCmdBuf->CtxRegs());

        m_shRegBase  = pCmdBuf->ShRegBase();
        m_ctxRegBase = pCmdBuf->CtxRegBase();
    }

    m_cmdBufCount += count;
}

// =====================================================================================================================
// Helper function to print out optimized register statistics in .csv format.
static void PrintRegisterStats(
    const File&               logFile,
    const RegisterInfoVector& stats,
    uint16                    registerBase)
{
    PAL_ASSERT(stats.IsEmpty() == false);

    for (auto i = stats.Begin(); i.IsValid(); i.Next())
    {
        const RegisterInfo& info = i.Get();
        const uint16        addr = (i.Position() + registerBase);

        if (info.setPktTotal > 0)
        {
            logFile.Printf("0x%04x,%d,%d\n", addr, info.setPktTotal, info.setPktKept);
        }
    }
}

// =====================================================================================================================
// Dumps PM4 statistics to a file.
void Queue::DumpStatistics()
{
    File logFile;
    if (logFile.Open(&m_fileName[0], FileAccessWrite) == Result::Success)
    {
        logFile.Printf("Operation,Count,Total Bytes\n\n");

        const uint32 frameCount = static_cast<Platform*>(m_pDevice->GetPlatform())->FrameCount();
        if (frameCount != 0)
        {
            logFile.Printf("Frames,%d\n\n", frameCount);
        }

        for (uint32 i = 0; i < NumCallIds; ++i)
        {
            const uint32 count = m_stats.call[i].count;
            if (count == 0)
            {
                continue; // Skip calls which were never hit.
            }

            logFile.Printf("%s,%d,%llu\n", CmdBufCallIdStrings[i], count, m_stats.call[i].cmdSize);
        }

        logFile.Printf("\n");

        for (uint32 i = 0; i < NumEventIds; ++i)
        {
            const uint32 count = m_stats.internalEvent[i].count;
            if (count == 0)
            {
                continue; // Skip events which were never hit.
            }

            const char*const pEventStr = InternalEventIdToString(static_cast<InternalEventId>(i));
            logFile.Printf("%s,%d,%llu\n", pEventStr, count, m_stats.internalEvent[i].cmdSize);
        }

        logFile.Printf("\nCommand Buffer Footprint,%d,%llu\n", m_cmdBufCount, m_stats.commandBufferSize);
        logFile.Printf("Embedded Data Footprint,%d,%llu\n",    m_cmdBufCount, m_stats.embeddedDataSize);
        logFile.Printf("GPU Scratch Mem Footprint,%d,%llu\n",  m_cmdBufCount, m_stats.gpuScratchMemSize);

        if (m_shRegs.IsEmpty() == false)
        {
            logFile.Printf("\nSH Register Offset, Total, Kept\n");
            PrintRegisterStats(logFile, m_shRegs, m_shRegBase);
        }

        if (m_ctxRegs.IsEmpty() == false)
        {
            logFile.Printf("\nCTX Register Offset, Total, Kept\n");
            PrintRegisterStats(logFile, m_ctxRegs, m_ctxRegBase);
        }
    } // If log file was opened
}

} // Pm4Instrumentor
} // Pal

#endif
