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

#include "core/platform.h"
#include "core/hw/gfxip/gfxCmdBuffer.h"
#include "core/hw/gfxip/gfx6/gfx6CmdStream.h"
#include "core/hw/gfxip/gfx6/gfx6CmdUtil.h"
#include "core/hw/gfxip/gfx6/gfx6Device.h"
#include "core/hw/gfxip/gfx6/gfx6PerfCounter.h"
#include "core/hw/gfxip/gfx6/gfx6PerfCtrInfo.h"
#include "core/hw/gfxip/gfx6/gfx6PerfExperiment.h"
#include "core/hw/gfxip/gfx6/gfx6PerfTrace.h"
#include "palDequeImpl.h"
#include "palHashMapImpl.h"

using namespace Pal::Gfx6::PerfCtrInfo;

namespace Pal
{
namespace Gfx6
{

// =====================================================================================================================
PerfExperiment::PerfExperiment(
    const Device*                   pDevice,
    const PerfExperimentCreateInfo& createInfo)
    :
    Pal::PerfExperiment(pDevice->Parent(), createInfo),
    m_device(*pDevice),
    m_blockUsageMap(32, pDevice->GetPlatform())
{
    m_counterFlags.u32All      = 0;
    m_sqPerfCounterCtrl.u32All = 0;
}

// =====================================================================================================================
// Initializes the usage status for each GPU block's performance counters.
Result PerfExperiment::Init()
{
    return m_blockUsageMap.Init();
}

// =====================================================================================================================
// Checks that a performance counter resource is available for the specified counter create info. Updates the tracker
// for counter resource usage.
Result PerfExperiment::ReserveCounterResource(
    const PerfCounterInfo& info,            ///< [in] Performance counter creation info
    uint32*                pCounterId,       ///< [out] Available counter slot we found
    uint32*                pCounterSubId)    ///< [out] Available counter sub-slot we found
{
    // Assume we are unable to find a counter slot for the caller.
    Result result = Result::ErrorOutOfGpuMemory;

    const Gfx6PerfCounterInfo& perfInfo = m_device.Parent()->ChipProperties().gfx6.perfCounterInfo;
    const uint32               blockNum = static_cast<uint32>(info.block);

    // The number of counters to check is based on the type of counter being requested: the loop below will loop over
    // each counter register. (SPM counters are packed four per register in all blocks but the SQ.)
    size_t numCounters = 0;
    if (info.counterType == PerfCounterType::Global)
    {
        numCounters = perfInfo.block[blockNum].numCounters;
    }
    else
    {
        PAL_ASSERT(info.counterType == PerfCounterType::Spm);
        numCounters = perfInfo.block[blockNum].numStreamingCounterRegs;
    }

    // Start looping over the first counter for the desired GPU block & instance. If a counter slot is free for the
    // desired instanceId, stop searching and use it.
    uint32 counterId = 0;
    PerfCtrUseStatus* pCtrStatus;
    for (; counterId < numCounters; ++counterId)
    {
        BlockUsageKey keyInfo = { info.block, info.instance, counterId};

        // "key not exist" is equivalent to "counter slot in PerfCtrEmpty state", so we can use this slot.
        bool   existed = true;

        // Here we call FindAllocate instead of Insert because we want pCtrStatus points to allocated
        // map entry to avoid redundant Findkey call.
        result = m_blockUsageMap.FindAllocate(keyInfo, &existed, &pCtrStatus);

        if (result == Result::Success)
        {
            result = Result::ErrorOutOfGpuMemory;

            if (existed == false)
            {
                // Add the new value if it did not exist already. If FindAllocate returns Success, pValue != nullptr.
                *pCtrStatus = PerfCtrEmpty;
            }

            if ((info.counterType == PerfCounterType::Global) ||
                (info.block == GpuBlock::Sq))
            {
                // 64-bit summary counter: Only one can exist per slot, so the slot must be
                // completely empty in order for us to use it. (The SQ has only one streaming
                // counter per summary counter slot, so use the same logic for all SQ ctr's.)
                if (*pCtrStatus == PerfCtrEmpty)
                {
                    result = Result::Success;
                    break;
                }
            }
            else
            {
                // 16-bit streaming counter: Up to four of these exist per summary counter slot,
                // (except for the SQ). We can use the slot if all four SPM slots are not full,
                // and if it is not being used as a summary counter.
                if ((*pCtrStatus != PerfCtr64BitSummary) &&
                    (*pCtrStatus <  PerfCtr16BitStreaming4))
                {
                    // NOTE: Special case: some blocks have only six streaming counters, which
                    // means that counter0 has four slots, but counter1 has only two.
                    if ((*pCtrStatus < PerfCtr16BitStreaming2) || (counterId != 1) ||
                        (perfInfo.block[blockNum].numStreamingCounters != 6))
                    {
                        result = Result::Success;
                        break;
                    }
                }
            }
        }

    }

    if (result == Result::Success)
    {
        uint32 counterSubId = 0;

        // If we get here, we successfully found a slot at 'counterId' for the new counter. Need
        // to update its usage status to reflect that a counter is being added.
        if (info.counterType == PerfCounterType::Global)
        {
            // 64-bit summary counter: mark the counter as in-use. The sub-slot ID has no meaning here.
            (*pCtrStatus) = PerfCtr64BitSummary;
        }
        else
        {
            // 16-bit streaming counter: update the number of occupied streaming counter sub-slots and record which
            // sub-slot we got.
            switch (*pCtrStatus)
            {
            case PerfCtrEmpty:
                (*pCtrStatus) = PerfCtr16BitStreaming1;
                counterSubId  = 0;
                break;
            case PerfCtr16BitStreaming1:
                (*pCtrStatus) = PerfCtr16BitStreaming2;
                counterSubId  = 1;
                break;
            case PerfCtr16BitStreaming2:
                (*pCtrStatus) = PerfCtr16BitStreaming3;
                counterSubId  = 2;
                break;
            case PerfCtr16BitStreaming3:
                (*pCtrStatus) = PerfCtr16BitStreaming4;
                counterSubId  = 3;
                break;
            default:
                // Something above went wrong if we find our way in here.
                PAL_ASSERT_ALWAYS();
                break;
            }
        }

        (*pCounterId)    = counterId;
        (*pCounterSubId) = counterSubId;
    }

    return result;
}

// =====================================================================================================================
// Checks that a performance counter resource is available for the specified counter create info. If the resource is
// available, instantiates a new PerfCounter object for the caller to use.
//
// This function only should be used for global performance counters!
Result PerfExperiment::CreateCounter(
    const PerfCounterInfo& info,
    Pal::PerfCounter**     ppCounter)
{
    PAL_ASSERT(info.counterType == PerfCounterType::Global);

    uint32 counterId    = 0;
    uint32 counterSubId = 0;

    // Search for an available counter slot to use for the new counter. (The counter sub-ID has no meaning for global
    // counters.)
    Result result = ReserveCounterResource(info, &counterId, &counterSubId);
    if (result == Result::Success)
    {
        PerfCounter*const pCounter = PAL_NEW(PerfCounter,
                                             m_device.GetPlatform(),
                                             Util::SystemAllocType::AllocInternal)(m_device, info, counterId);

        if (pCounter == nullptr)
        {
            // Object instantiation failed due to lack of memory.
            result = Result::ErrorOutOfMemory;
        }
        else
        {
            // Update the counter flags
            m_counterFlags.indexedBlocks |= pCounter->IsIndexed();
            m_counterFlags.mcCounters    |= (info.block == GpuBlock::Mc);
            m_counterFlags.rlcCounters   |= (info.block == GpuBlock::Rlc);
            m_counterFlags.sqCounters    |= (info.block == GpuBlock::Sq);
            m_counterFlags.srbmCounters  |= (info.block == GpuBlock::Srbm);
            m_counterFlags.taCounters    |= (info.block == GpuBlock::Ta);
            m_counterFlags.tdCounters    |= (info.block == GpuBlock::Td);
            m_counterFlags.tcpCounters   |= (info.block == GpuBlock::Tcp);
            m_counterFlags.tccCounters   |= (info.block == GpuBlock::Tcc);
            m_counterFlags.tcaCounters   |= (info.block == GpuBlock::Tca);

            const auto& chipProps = m_device.Parent()->ChipProperties();
            if ((chipProps.gfxLevel != GfxIpLevel::GfxIp6) &&
                ((info.block == GpuBlock::Ta)  ||
                 (info.block == GpuBlock::Td)  ||
                 (info.block == GpuBlock::Tcp) ||
                 (info.block == GpuBlock::Tcc) ||
                 (info.block == GpuBlock::Tca)))
            {
                static constexpr uint32 SqDefaultCounterRate = 0;

                m_sqPerfCounterCtrl.bits.PS_EN |= 1;
                m_sqPerfCounterCtrl.bits.VS_EN |= 1;
                m_sqPerfCounterCtrl.bits.GS_EN |= 1;
                m_sqPerfCounterCtrl.bits.ES_EN |= 1;
                m_sqPerfCounterCtrl.bits.HS_EN |= 1;
                m_sqPerfCounterCtrl.bits.LS_EN |= 1;
                m_sqPerfCounterCtrl.bits.CS_EN |= 1;
                m_sqPerfCounterCtrl.bits.CNTR_RATE = SqDefaultCounterRate;

                // SQ-perWave and TA/TC/TD may interfere each other, consider collect in different pass.
                PAL_ALERT(HasSqCounters());
            }
            else if (info.block == GpuBlock::Sq)
            {
                static constexpr uint32 SqDefaultCounterRate = 0;

                m_sqPerfCounterCtrl.bits.PS_EN |= ((ShaderMask() & PerfShaderMaskPs) ? 1 : 0);
                m_sqPerfCounterCtrl.bits.VS_EN |= ((ShaderMask() & PerfShaderMaskVs) ? 1 : 0);
                m_sqPerfCounterCtrl.bits.GS_EN |= ((ShaderMask() & PerfShaderMaskGs) ? 1 : 0);
                m_sqPerfCounterCtrl.bits.ES_EN |= ((ShaderMask() & PerfShaderMaskEs) ? 1 : 0);
                m_sqPerfCounterCtrl.bits.HS_EN |= ((ShaderMask() & PerfShaderMaskHs) ? 1 : 0);
                m_sqPerfCounterCtrl.bits.LS_EN |= ((ShaderMask() & PerfShaderMaskLs) ? 1 : 0);
                m_sqPerfCounterCtrl.bits.CS_EN |= ((ShaderMask() & PerfShaderMaskCs) ? 1 : 0);
                m_sqPerfCounterCtrl.bits.CNTR_RATE = SqDefaultCounterRate;

                // SQ-perWave and TA/TC/TD may interfere each other, consider collect in different pass.
                PAL_ALERT((chipProps.gfxLevel != GfxIpLevel::GfxIp6) &&
                          (HasTaCounters()  ||
                           HasTdCounters()  ||
                           HasTcpCounters() ||
                           HasTccCounters() ||
                           HasTcaCounters()));
            }

            (*ppCounter) = pCounter;
        }
    }

    return result;
}

// =====================================================================================================================
// Instantiates a new ThreadTrace object for the specified Shader Engine.
//
// This function only should be used for thread traces!
Result PerfExperiment::CreateThreadTrace(
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 373
    const PerfTraceInfo& info)
#else
    const ThreadTraceInfo& info)
#endif
{
    PAL_ASSERT(info.traceType == PerfTraceType::ThreadTrace);

    Result result = ValidateThreadTraceOptions(*m_device.Parent(), info);

    if (result == Result::Success)
    {
        // Instantiate a new thread trace object.
        m_pThreadTrace[info.instance] = PAL_NEW(ThreadTrace,
                                                m_device.GetPlatform(),
                                                Util::SystemAllocType::AllocInternal)(&m_device, info);

        if (m_pThreadTrace[info.instance] != nullptr)
        {
            ++m_numThreadTrace;
        }
        else
        {
            result = Result::ErrorOutOfMemory;
        }
    }

    return result;
}

// =====================================================================================================================
// Instantiates a new SpmTrace object.
Result PerfExperiment::CreateSpmTrace(
    const SpmTraceCreateInfo& info)
{
    Result result = Result::Success;

    result = ValidateSpmTraceOptions(*m_device.Parent(), info);

    // Create the SpmTrace to which we will add the PerfCounters provided in SpmTraceCreateInfo.
    if (result == Result::Success)
    {
        m_pSpmTrace = PAL_NEW(SpmTrace,
                              m_device.GetPlatform(),
                              Util::SystemAllocType::AllocInternal) (&m_device);

        if (m_pSpmTrace != nullptr)
        {
            result = m_pSpmTrace->Init(info);
        }
        else
        {
            result = Result::ErrorOutOfMemory;
        }
    }

    // A single StreamingPerfCounter represents a single 64-bit hw counter that can track 4 16-bit streaming perf
    // counter events. Each PerfCounter described in the spm trace create info represents a 16-bit counter that is
    // added to the StreamingPerfCounter for tracking and register programming. StreamingPerfCounter(s) that
    // represent SQ counters can only track/program one 16-bit counter due to the hw design.
    if(result == Result ::Success)
    {
        const Gfx6PerfCounterInfo& perfInfo = m_device.Parent()->ChipProperties().gfx6.perfCounterInfo;

        Util::HashMap<BlockUsageKey, StreamingPerfCounter*, Platform> spmCounterUsageMap(32, m_device.GetPlatform());
        result = spmCounterUsageMap.Init();

        // Iterate through the list of perf counters provided for this SpmTrace.
        for (uint32 i = 0; (i < info.numPerfCounters) && (result == Result::Success); ++i)
        {
            uint32 numCounters =
                perfInfo.block[static_cast<uint32>(info.pPerfCounterInfos[i].block)].numStreamingCounterRegs;

            // Iterate over the number of registers(counters) that can be used as streaming counters in this block.
            // The result can be in Error state until this loop is done.
            for (uint32 counterIdx = 0; counterIdx < numCounters; counterIdx++)
            {
                BlockUsageKey key = { info.pPerfCounterInfos[i].block, info.pPerfCounterInfos[i].instance, counterIdx };

                StreamingPerfCounter** ppStreamingCounter = nullptr;
                bool                   existed            = false;

                result = spmCounterUsageMap.FindAllocate(key, &existed, &ppStreamingCounter);

                if (existed)
                {
                    // Attempt to add this perf counter if the streaming perf counter already exists in the hash map.
                    // If all the slots in this HW counter are full, then we move on to the next HW counter in this
                    // instance.
                    result = (*ppStreamingCounter)->AddEvent(info.pPerfCounterInfos[i].block,
                                                             info.pPerfCounterInfos[i].eventId);
                }
                else
                {
                    // Create a new StreamingPerfConter and add it to the hashmap. The SpmTrace object is responsible
                    // for freeing this memory allocated for each StreamingPerfCounter.
                    StreamingPerfCounter* pNewCounter = PAL_NEW (StreamingPerfCounter,
                                                                 m_device.GetPlatform(),
                                                                 Util::SystemAllocType::AllocObject)
                                                                 (m_device,
                                                                  info.pPerfCounterInfos[i].block,
                                                                  info.pPerfCounterInfos[i].instance,
                                                                  counterIdx);

                    // Allocation succeeded, so create a StreamingPerfCounter object and add it to the hash map.
                    if (pNewCounter != nullptr)
                    {
                        result = pNewCounter->AddEvent(info.pPerfCounterInfos[i].block,
                                                       info.pPerfCounterInfos[i].eventId);

                        if (result == Result::Success)
                        {
                            const auto& block = info.pPerfCounterInfos[i].block;

                            // Update the counter flags
                            m_counterFlags.indexedBlocks |= pNewCounter->IsIndexed();
                            m_counterFlags.mcCounters    |= (block == GpuBlock::Mc);
                            m_counterFlags.rlcCounters   |= (block == GpuBlock::Rlc);
                            m_counterFlags.sqCounters    |= (block == GpuBlock::Sq);
                            m_counterFlags.srbmCounters  |= (block == GpuBlock::Srbm);
                            m_counterFlags.taCounters    |= (block == GpuBlock::Ta);
                            m_counterFlags.tdCounters    |= (block == GpuBlock::Td);
                            m_counterFlags.tcpCounters   |= (block == GpuBlock::Tcp);
                            m_counterFlags.tccCounters   |= (block == GpuBlock::Tcc);
                            m_counterFlags.tcaCounters   |= (block == GpuBlock::Tca);

                            const auto& chipProps = m_device.Parent()->ChipProperties();
                            if ((chipProps.gfxLevel != GfxIpLevel::GfxIp6) &&
                                ((block == GpuBlock::Ta)  ||
                                 (block == GpuBlock::Td)  ||
                                 (block == GpuBlock::Tcp) ||
                                 (block == GpuBlock::Tcc) ||
                                 (block == GpuBlock::Tca)))
                            {
                                constexpr uint32 SqDefaultCounterRate = 0;

                                m_sqPerfCounterCtrl.bits.PS_EN |= 1;
                                m_sqPerfCounterCtrl.bits.VS_EN |= 1;
                                m_sqPerfCounterCtrl.bits.GS_EN |= 1;
                                m_sqPerfCounterCtrl.bits.ES_EN |= 1;
                                m_sqPerfCounterCtrl.bits.HS_EN |= 1;
                                m_sqPerfCounterCtrl.bits.LS_EN |= 1;
                                m_sqPerfCounterCtrl.bits.CS_EN |= 1;
                                m_sqPerfCounterCtrl.bits.CNTR_RATE = SqDefaultCounterRate;

                                // SQ-perWave and TA/TC/TD may interfere each other, consider collect in different pass.
                                PAL_ALERT(HasSqCounters());
                            }
                            else if (block == GpuBlock::Sq)
                            {
                                static constexpr uint32 SqDefaultCounterRate = 0;

                                m_sqPerfCounterCtrl.bits.PS_EN |= ((ShaderMask() & PerfShaderMaskPs) ? 1 : 0);
                                m_sqPerfCounterCtrl.bits.VS_EN |= ((ShaderMask() & PerfShaderMaskVs) ? 1 : 0);
                                m_sqPerfCounterCtrl.bits.GS_EN |= ((ShaderMask() & PerfShaderMaskGs) ? 1 : 0);
                                m_sqPerfCounterCtrl.bits.ES_EN |= ((ShaderMask() & PerfShaderMaskEs) ? 1 : 0);
                                m_sqPerfCounterCtrl.bits.HS_EN |= ((ShaderMask() & PerfShaderMaskHs) ? 1 : 0);
                                m_sqPerfCounterCtrl.bits.LS_EN |= ((ShaderMask() & PerfShaderMaskLs) ? 1 : 0);
                                m_sqPerfCounterCtrl.bits.CS_EN |= ((ShaderMask() & PerfShaderMaskCs) ? 1 : 0);
                                m_sqPerfCounterCtrl.bits.CNTR_RATE = SqDefaultCounterRate;

                                // SQ-perWave and TA/TC/TD may interfere each other, consider collect in different pass.
                                PAL_ALERT((chipProps.gfxLevel != GfxIpLevel::GfxIp6) &&
                                          (HasTaCounters()  ||
                                           HasTdCounters()  ||
                                           HasTcpCounters() ||
                                           HasTccCounters() ||
                                           HasTcaCounters()));
                            }
                            (*ppStreamingCounter) = pNewCounter;
                        }
                    }
                    else
                    {
                        // Allocation of StreamingPerfCounter failed.
                        result = Result::ErrorOutOfMemory;
                        break;
                    }

                    if (result == Result::Success)
                    {
                        // We have either added a perf counter to an existing StreamingPerfCounter or we have
                        // succesfully created a new StreamingPerfCounter and added our perf counter to it. We can skip
                        // to the outer loop for the next perf counter.
                        break;
                    }

                } // New StreamingPerfCounter allocation.
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

// =====================================================================================================================
// Issues commands into the specified command stream which instruct the HW to begin recording performance data.
void PerfExperiment::IssueBegin(
    Pal::CmdStream* pPalCmdStream    ///< [in,out] Command stream to write PM4 commands into
    ) const
{
    PAL_ASSERT(IsFinalized());

    CmdStream* pCmdStream = static_cast<CmdStream*>(pPalCmdStream);

    const auto& chipProps  = m_device.Parent()->ChipProperties();
    const auto& cmdUtil    = m_device.CmdUtil();
    const bool  forCompute = pCmdStream->GetEngineType() == EngineTypeCompute;
    uint32*     pCmdSpace  = pCmdStream->ReserveCommands();

    // Optionally flush and invalidate caches before sampling counter data.
    if (CacheFlushOnPerfCounter())
    {
        // CmdUtil will properly route to EventWriteEop/ReleaseMem as appropriate.
        pCmdSpace += m_device.CmdUtil().BuildGenericEopEvent(CACHE_FLUSH_AND_INV_TS_EVENT,
                                                             0x0,
                                                             EVENTWRITEEOP_DATA_SEL_DISCARD,
                                                             0x0,
                                                             forCompute,
                                                             true,
                                                             pCmdSpace);
    }

    // Wait for GFX engine to become idle before freezing or sampling counters.
    pCmdSpace = WriteWaitIdleClean(false, forCompute, pCmdSpace);

    if (chipProps.gfx6.sqgEventsEnabled == false)
    {
        // Both SQ performance counters and traces need the SQG events enabled. Force them on
        // ourselves if KMD doesn't have them active by default.
        regSPI_CONFIG_CNTL spiConfigCntl = {};
        spiConfigCntl.bits.ENABLE_SQG_TOP_EVENTS = 1;
        spiConfigCntl.bits.ENABLE_SQG_BOP_EVENTS = 1;

        // On some ASICs we have to WaitIdle before writing this register. We do this already, so there isn't a need
        // to do it again.
        pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(mmSPI_CONFIG_CNTL, spiConfigCntl.u32All, pCmdSpace);
    }

    if (HasThreadTraces())
    {
        // Issue commands to setup each thread trace's state. No more than four thread traces can be active at once so
        // it should be safe to use the same reserve buffer.
        for (size_t idx = 0; idx < MaxNumThreadTrace; ++idx)
        {
            if (m_pThreadTrace[idx] != nullptr)
            {
                auto*const pTrace = static_cast<ThreadTrace*>(m_pThreadTrace[idx]);
                pCmdSpace = pTrace->WriteSetupCommands(m_vidMem.GpuVirtAddr(), pCmdStream, pCmdSpace);
            }
        }

        // Issue commands to setup each thread trace's state. No more than four thread traces can be active at once so
        // it should be safe to use the same reserve buffer.
        for (size_t idx = 0; idx < MaxNumThreadTrace; ++idx)
        {
            if (m_pThreadTrace[idx] != nullptr)
            {
                auto*const pTrace = static_cast<ThreadTrace*>(m_pThreadTrace[idx]);
                pCmdSpace = pTrace->WriteStartCommands(pCmdStream, pCmdSpace);
            }
        }

        pCmdSpace = WriteResetGrbmGfxIndex(pCmdStream, pCmdSpace);

        // Issue a VGT event to start thread traces. This is done out here because we want to reset GRBM_GFX_INDEX
        // before issuing the event. No more than four thread traces can be active at once so it should be safe to use
        // the same reserve buffer.
        pCmdSpace += cmdUtil.BuildEventWrite(THREAD_TRACE_START, pCmdSpace);

        if (forCompute == false)
        {
            pCmdSpace += cmdUtil.BuildEventWrite(PS_PARTIAL_FLUSH, pCmdSpace);
        }
        pCmdSpace  = WriteWaitIdleClean(true, forCompute, pCmdSpace);
    }

    if (HasSpmTrace())
    {
        pCmdSpace = m_pSpmTrace->WriteSetupCommands(m_vidMem.GpuVirtAddr(), pPalCmdStream, pCmdSpace);

        pCmdStream->CommitCommands(pCmdSpace);
        pCmdSpace = pCmdStream->ReserveCommands();

        pCmdSpace = WriteResetGrbmGfxIndex(pCmdStream, pCmdSpace);

        pCmdSpace = WriteWaitIdleClean(true, forCompute, pCmdSpace);

        if (m_sqPerfCounterCtrl.u32All != 0)
        {
            pCmdSpace = pCmdStream->WriteSetOneConfigReg(m_device.CmdUtil().GetRegInfo().mmSqPerfCounterCtrl,
                                                         m_sqPerfCounterCtrl.u32All,
                                                         pCmdSpace);
        }

        pCmdSpace = m_pSpmTrace->WriteStartCommands(pPalCmdStream, pCmdSpace);
        pCmdSpace += cmdUtil.BuildEventWrite(PERFCOUNTER_START, pCmdSpace);
    }

    if (HasGlobalCounters())
    {
        // Having both summary perf counters and streaming perf counter trace is not supported.
        PAL_ASSERT(HasSpmTrace() == false);

        // Need to freeze and reset performance counters.
        pCmdSpace = WriteStopPerfCounters(true, pCmdStream, pCmdSpace);

        // Issue commands to setup the finalized performance counter select registers.
        pCmdSpace = WriteSetupPerfCounters(pCmdStream, pCmdSpace);

        // Record an initial sample of the performance counter data at the "begin" offset
        // in GPU memory.
        pCmdSpace = WriteSamplePerfCounters(m_vidMem.GpuVirtAddr() + m_ctrBeginOffset, pCmdStream, pCmdSpace);

        // Issue commands to start recording perf counter data.
        pCmdSpace = WriteStartPerfCounters(false, pCmdStream, pCmdSpace);
    }

    pCmdStream->CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// Issues update commands into the specified command stream which instruct the HW to modify the sqtt token mask.
void PerfExperiment::UpdateSqttTokenMask(
    Pal::CmdStream* pPalCmdStream, ///< [in,out] Command stream to write PM4 commands into
    uint32          sqttTokenMask  ///< [in] Updated SQTT token mask
    ) const
{
    PAL_ASSERT(IsFinalized());

    // This should only be called on thread trace performance experiments.
    PAL_ASSERT(HasThreadTraces());

    if (HasThreadTraces())
    {
        CmdStream* pCmdStream = static_cast<CmdStream*>(pPalCmdStream);
        uint32*    pCmdSpace  = pCmdStream->ReserveCommands();

        // Issue commands to update each thread trace's state. No more than four thread traces can be active at once so
        // it should be safe to use the same reserve buffer.
        for (size_t idx = 0; idx < MaxNumThreadTrace; ++idx)
        {
            if (m_pThreadTrace[idx] != nullptr)
            {
                auto*const pTrace = static_cast<ThreadTrace*>(m_pThreadTrace[idx]);
                pCmdSpace = pTrace->WriteUpdateSqttTokenMaskCommands(pCmdStream,
                                                                     pCmdSpace,
                                                                     sqttTokenMask);
            }
        }

        pCmdSpace = WriteResetGrbmGfxIndex(pCmdStream, pCmdSpace);

        pCmdStream->CommitCommands(pCmdSpace);
    }
}

// =====================================================================================================================
// Issues commands into the specified command stream which instruct the HW to halt recording performance data.
void PerfExperiment::IssueEnd(
    Pal::CmdStream* pPalCmdStream    ///< [in,out] Command stream to write PM4 commands into
    ) const
{
    PAL_ASSERT(IsFinalized());

    CmdStream*  pCmdStream = static_cast<CmdStream*>(pPalCmdStream);
    const auto& cmdUtil    = m_device.CmdUtil();
    const bool  forCompute = pCmdStream->GetEngineType() == EngineTypeCompute;
    uint32*     pCmdSpace  = pCmdStream->ReserveCommands();

    // Optionally flush and invalidate caches before sampling counter data.
    if (CacheFlushOnPerfCounter())
    {
        // CmdUtil will properly route to EventWriteEop/ReleaseMem as appropriate.
        pCmdSpace += m_device.CmdUtil().BuildGenericEopEvent(CACHE_FLUSH_AND_INV_TS_EVENT,
                                                             0x0,
                                                             EVENTWRITEEOP_DATA_SEL_DISCARD,
                                                             0x0,
                                                             forCompute,
                                                             true,
                                                             pCmdSpace);
    }

    // Wait for GFX engine to become idle before freezing or sampling counters.
    pCmdSpace = WriteWaitIdleClean(false, pCmdStream->GetEngineType() == EngineTypeCompute, pCmdSpace);

    if (HasGlobalCounters())
    {
        // Record a final sample of the performance counter data at the "end" offset in GPU memory.
        pCmdSpace = WriteSamplePerfCounters(m_vidMem.GpuVirtAddr() + m_ctrEndOffset, pCmdStream, pCmdSpace);

        // Issue commands to stop recording perf counter data.
        pCmdSpace = WriteStopPerfCounters(true, pCmdStream, pCmdSpace);
    }

    if (HasThreadTraces())
    {
        // Issue a VGT event to stop thread traces for future waves.
        pCmdSpace += cmdUtil.BuildEventWrite(THREAD_TRACE_STOP, pCmdSpace);

        // Stop recording each active thread trace. No more than four thread traces can be active at once so it should
        // be safe to use the same reserve buffer.
        for (size_t idx = 0; idx < MaxNumThreadTrace; ++idx)
        {
            if (m_pThreadTrace[idx] != nullptr)
            {
                auto*const pTrace = static_cast<ThreadTrace*>(m_pThreadTrace[idx]);
                pCmdSpace = pTrace->WriteStopCommands(m_vidMem.GpuVirtAddr(), pCmdStream, pCmdSpace);
            }
        }

        pCmdSpace = WriteResetGrbmGfxIndex(pCmdStream, pCmdSpace);
    }

    if (HasSpmTrace())
    {
        CmdStream* pHwlCmdStream = static_cast<CmdStream*>(pCmdStream);

        regCP_PERFMON_CNTL cpPerfmonCntl = {};

        // Enable sampling. This writes samples the counter values and writes in *_PERFCOUNTER*_LO/HI registers.
        cpPerfmonCntl.bits.PERFMON_SAMPLE_ENABLE = 1;
        pCmdSpace = pHwlCmdStream->WriteSetOneConfigReg(mmCP_PERFMON_CNTL__CI__VI,
                                                        cpPerfmonCntl.u32All,
                                                        pCmdSpace);
        pCmdSpace += m_device.CmdUtil().BuildEventWrite(PERFCOUNTER_SAMPLE, pCmdSpace);

        // Stop all performance counters.
        cpPerfmonCntl.u32All                         = 0;
        cpPerfmonCntl.bits.PERFMON_STATE             = PerfmonStopCounting;
        cpPerfmonCntl.bits.SPM_PERFMON_STATE__CI__VI = PerfmonStopCounting;

        pCmdSpace = pHwlCmdStream->WriteSetOneConfigReg(mmCP_PERFMON_CNTL__CI__VI,
                                                        cpPerfmonCntl.u32All,
                                                        pCmdSpace);

        pCmdSpace += m_device.CmdUtil().BuildEventWrite(PERFCOUNTER_STOP, pCmdSpace);

        // Need a WaitIdle here before zeroing the RLC SPM controls, else we get a page fault indicating that the data
        // is still being written at the moment.
        pCmdSpace = WriteWaitIdleClean(false, pCmdStream->GetEngineType() == EngineTypeCompute, pCmdSpace);

        if (m_sqPerfCounterCtrl.u32All != 0)
        {
            static constexpr regSQ_PERFCOUNTER_CTRL SqPerfCounterCtrl = {};

            pCmdSpace = pCmdStream->WriteSetOneConfigReg(m_device.CmdUtil().GetRegInfo().mmSqPerfCounterCtrl,
                                                         SqPerfCounterCtrl.u32All,
                                                         pCmdSpace);
        }

        pCmdSpace = m_pSpmTrace->WriteEndCommands(pCmdStream, pCmdSpace);
    }

    if (m_device.Parent()->ChipProperties().gfx6.sqgEventsEnabled == false)
    {
        if (m_device.WaWaitIdleBeforeSpiConfigCntl())
        {
            pCmdSpace = WriteWaitIdleClean(false, pCmdStream->GetEngineType() == EngineTypeCompute, pCmdSpace);
        }

        // Reset the default value of SPI_CONFIG_CNTL if we overrode it in HwlIssueBegin().
        static constexpr regSPI_CONFIG_CNTL SpiConfigCntl = {};
        pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(mmSPI_CONFIG_CNTL, SpiConfigCntl.u32All, pCmdSpace);
    }

    pCmdStream->CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// Issues commands into the specified command stream which instruct the HW to pause the recording of performance data.
void PerfExperiment::IssuePause(
    CmdStream* pCmdStream    ///< [in,out] Command stream to write PM4 commands into
    ) const
{
    // NOTE: This should only be called if this Experiment doesn't sample internal operations.
    PAL_ASSERT(SampleInternalOperations() == false);

    if (HasGlobalCounters())
    {
        // Issue commands to stop recording perf counter data, without resetting the counters.
        uint32* pCmdSpace = pCmdStream->ReserveCommands();
        pCmdSpace = WriteStopPerfCounters(false, pCmdStream, pCmdSpace);
        pCmdStream->CommitCommands(pCmdSpace);
    }

}

// =====================================================================================================================
// Issues commands into the specified command stream which instruct the HW to resume the recording of performance data.
void PerfExperiment::IssueResume(
    CmdStream* pCmdStream    ///< [in,out] Command stream to write PM4 commands into
    ) const
{
    // NOTE: This should only be called if this Experiment doesn't sample internal operations.
    PAL_ASSERT(SampleInternalOperations() == false);

    if (HasGlobalCounters())
    {
        // Issue commands to start recording perf counter data.
        uint32* pCmdSpace = pCmdStream->ReserveCommands();
        pCmdSpace = WriteStartPerfCounters(true, pCmdStream, pCmdSpace);
        pCmdStream->CommitCommands(pCmdSpace);
    }

    // SEE: HwlIssuePause concerning behavior regarding thread traces.
}

// =====================================================================================================================
// Asks all active thread traces to insert a trace marker into their trace data streams.
void PerfExperiment::InsertTraceMarker(
    CmdStream*          pCmdStream,
    PerfTraceMarkerType markerType,
    uint32              data
    ) const
{
    PAL_ASSERT(IsFinalized() && HasThreadTraces());

    uint32* pCmdSpace = pCmdStream->ReserveCommands();

    // Loop over all active thread traces and instruct them to insert a trace marker. No more than four thread traces
    // can be active at once so it should be safe to use the same reserve buffer.
    for (size_t idx = 0; idx < m_numThreadTrace; ++idx)
    {
        if (m_pThreadTrace[idx] != nullptr)
        {
            auto*const pTrace = static_cast<ThreadTrace*>(m_pThreadTrace[idx]);
            pCmdSpace = pTrace->WriteInsertMarker(markerType, data, pCmdStream, pCmdSpace);
        }
    }

    pCmdStream->CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// Optionally pause the recording of performance data if this Experiment does not record during internal operations
// (e.g., blts, resource preparation, etc.).
void PerfExperiment::BeginInternalOps(
    CmdStream* pCmdStream
    ) const
{
    if (SampleInternalOperations() == false)
    {
        // If this Experiment doesn't sample internal operations, delegate to the hardware layer to pause the
        // collection of data.
        IssuePause(pCmdStream);
    }
}

// =====================================================================================================================
// Optionally resumethe recording of performance data if this Experiment does not record during internal operations
// (e.g., blts, resource preparation, etc.).
void PerfExperiment::EndInternalOps(
    CmdStream* pCmdStream
    ) const
{
    if (SampleInternalOperations() == false)
    {
        // If this Experiment doesn't sample internal operations, delegate to the hardware layer to pause the
        // collection of data.
        IssueResume(pCmdStream);
    }
}

// =====================================================================================================================
// Sets-up performance counters by issuing commands into the specified command buffer which will instruct the HW to
// initialize the data select and filter registers for the counters. Returns the next unused DWORD in pCmdSpace.
uint32* PerfExperiment::WriteSetupPerfCounters(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    const auto& chipProps = m_device.Parent()->ChipProperties();
    const auto& perfInfo  = chipProps.gfx6.perfCounterInfo;

    // NOTE: The MC and SDMA blocks require special handling for counter setup because multiple counters' state gets
    //       packed into the same registers.
    regMC_SEQ_PERF_SEQ_CTL         mcSeqPerfCtl     = {};
    regMC_SEQ_PERF_CNTL_1          mcSeqPerfCtl1    = {};
    regSDMA0_PERFMON_CNTL__CI__VI  sdma0PerfmonCntl = {};
    regSDMA1_PERFMON_CNTL__CI__VI  sdma1PerfmonCntl = {};

    // Walk the counter list and set select & filter registers.
    for (auto it = m_globalCtrs.Begin(); it.Get(); it.Next())
    {
        const PerfCounter*const pPerfCounter = static_cast<PerfCounter*>(*it.Get());
        PAL_ASSERT(pPerfCounter != nullptr);

        if (pPerfCounter->BlockType() == GpuBlock::Mc)
        {
            // Accumulate counter state in the MC registers.
            pPerfCounter->SetupMcSeqRegisters(&mcSeqPerfCtl, &mcSeqPerfCtl1);
        }
        else if ((pPerfCounter->BlockType() == GpuBlock::Dma) &&
                 (chipProps.gfxLevel != GfxIpLevel::GfxIp6))
        {
            // Accumulate the value of the SDMA perfmon control register(s).
            const uint32 regValue = pPerfCounter->SetupSdmaSelectReg(&sdma0PerfmonCntl, &sdma1PerfmonCntl);

            // Special handling for SDMA: the register info is per instance rather than per counter slot.
            const uint32 blockIdx = static_cast<uint32>(pPerfCounter->BlockType());
            const uint32 regAddress = perfInfo.block[blockIdx].regInfo[pPerfCounter->GetInstanceId()].perfSel0RegAddr;

            // Issue a write to the appropriate SDMA perfmon control register.
            pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(regAddress, regValue, pCmdSpace);
        }
        else
        {
            // No special handling needed... the counter can issue its own setup commands.
            pCmdSpace = pPerfCounter->WriteSetupCommands(pCmdStream, pCmdSpace);
        }

        // This loop doesn't have a trivial upper-limit so we must be careful to not overflow the reserve buffer.
        // If CPU-performance of perf counters is later deemed to be important we can make this code smarter.
        pCmdStream->CommitCommands(pCmdSpace);
        pCmdSpace = pCmdStream->ReserveCommands();
    }

    if (HasIndexedCounters())
    {
        pCmdSpace = WriteResetGrbmGfxIndex(pCmdStream, pCmdSpace);
    }

    if (HasMcCounters())
    {
        // Write the MC perf control registers.

        pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(perfInfo.mcConfigRegAddress,
                                                      perfInfo.mcWriteEnableMask,
                                                      pCmdSpace);

        pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(mmMC_SEQ_PERF_SEQ_CTL, mcSeqPerfCtl.u32All, pCmdSpace);
        pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(mmMC_SEQ_PERF_CNTL_1,  mcSeqPerfCtl1.u32All, pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Starts performance counters by issuing commands into the specified command buffer which will instruct the HW to start
// accumulating performance data. Returns the next unused DWORD in pCmdSpace.
uint32* PerfExperiment::WriteStartPerfCounters(
    bool       restart,    ///< If true, restart the counters post-sampling
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    const auto& cmdUtil    = m_device.CmdUtil();
    const auto& regInfo    = cmdUtil.GetRegInfo();
    const bool  forCompute = (pCmdStream->GetEngineType() == EngineTypeCompute);

    if (HasSrbmCounters())
    {
        // Start SRBM counters.
        regSRBM_PERFMON_CNTL srbmPerfmonCntl = {};
        srbmPerfmonCntl.bits.PERFMON_STATE = PerfmonStartCounting;

        pCmdSpace = pCmdStream->WriteSetOnePrivilegedConfigReg(regInfo.mmSrbmPerfmonCntl,
                                                               srbmPerfmonCntl.u32All,
                                                               pCmdSpace);
    }

    if (HasRlcCounters())
    {
        // Start RLC counters: this needs to be done with a COPY_DATA command.
        regRLC_PERFMON_CNTL rlcPerfmonCntl = {};
        rlcPerfmonCntl.bits.PERFMON_STATE = PerfmonStartCounting;

        pCmdSpace += cmdUtil.BuildCopyData(COPY_DATA_SEL_REG,
                                           regInfo.mmRlcPerfmonCntl,
                                           COPY_DATA_SEL_SRC_IMME_DATA,
                                           rlcPerfmonCntl.u32All,
                                           COPY_DATA_SEL_COUNT_1DW,
                                           COPY_DATA_ENGINE_ME,
                                           COPY_DATA_WR_CONFIRM_NO_WAIT,
                                           pCmdSpace);
    }

    // Only configure MC and SQ counters on initial startup.
    if (restart == false)
    {
        if (HasMcCounters())
        {
            regMC_SEQ_PERF_CNTL mcSeqPerfCntl = {};
            mcSeqPerfCntl.bits.MONITOR_PERIOD = McSeqMonitorPeriod;
            mcSeqPerfCntl.bits.CNTL           = McSeqClearCounter;

            pCmdSpace = pCmdStream->WriteSetOneConfigReg(mmMC_SEQ_PERF_CNTL, mcSeqPerfCntl.u32All, pCmdSpace);

            mcSeqPerfCntl.bits.CNTL = McSeqStartCounter;

            pCmdSpace = pCmdStream->WriteSetOneConfigReg(mmMC_SEQ_PERF_CNTL, mcSeqPerfCntl.u32All, pCmdSpace);
        }

        if (m_sqPerfCounterCtrl.u32All != 0)
        {
            pCmdSpace = pCmdStream->WriteSetOneConfigReg(regInfo.mmSqPerfCounterCtrl,
                                                         m_sqPerfCounterCtrl.u32All,
                                                         pCmdSpace);
        }
    }

    if (forCompute == true)
    {
        regCOMPUTE_PERFCOUNT_ENABLE__CI__VI computePerfCounterEn = {};
        computePerfCounterEn.bits.PERFCOUNT_ENABLE = 1;

        pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderCompute>(mmCOMPUTE_PERFCOUNT_ENABLE__CI__VI,
                                                                computePerfCounterEn.u32All,
                                                                pCmdSpace);
    }
    else
    {
        // Write the command sequence to start event-based counters.
        pCmdSpace += cmdUtil.BuildEventWrite(PERFCOUNTER_START, pCmdSpace);
    }

    // Start graphics state based counters.
    regCP_PERFMON_CNTL cpPerfmonCntl = {};
    if (HasGlobalCounters())
    {
        cpPerfmonCntl.bits.PERFMON_STATE = PerfmonStartCounting;
    }

    pCmdSpace = pCmdStream->WriteSetOneConfigReg(regInfo.mmCpPerfmonCntl,
                                                 cpPerfmonCntl.u32All,
                                                 pCmdSpace);

    return pCmdSpace;
}

// =====================================================================================================================
// Stops performance counters by issuing commands into the specified command buffer which will instruct the HW to stop
// accumulating performance data. Returns the next unused DWORD in pCmdSpace.
uint32* PerfExperiment::WriteStopPerfCounters(
    bool       reset,      ///< If true, resets the global counters
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    const auto& cmdUtil    = m_device.CmdUtil();
    const auto& regInfo    = cmdUtil.GetRegInfo();
    const bool  forCompute = (pCmdStream->GetEngineType() == EngineTypeCompute);

    // Set the perfmon state to 'stop counting' if we're freezing global counters, or to 'disable and reset' otherwise.
    const uint32 perfmonState = (reset ? PerfmonDisableAndReset: PerfmonStopCounting);

    // Stop graphics state based counters.
    regCP_PERFMON_CNTL cpPerfmonCntl = {};
    if (HasGlobalCounters())
    {
        cpPerfmonCntl.bits.PERFMON_STATE = perfmonState;
    }

    pCmdSpace = pCmdStream->WriteSetOneConfigReg(regInfo.mmCpPerfmonCntl,
                                                 cpPerfmonCntl.u32All,
                                                 pCmdSpace);

    if (HasSrbmCounters())
    {
        // Stop SRBM counters.
        regSRBM_PERFMON_CNTL srbmPerfmonCntl = {};
        srbmPerfmonCntl.bits.PERFMON_STATE = perfmonState;

        pCmdSpace = pCmdStream->WriteSetOnePrivilegedConfigReg(regInfo.mmSrbmPerfmonCntl,
                                                               srbmPerfmonCntl.u32All,
                                                               pCmdSpace);
    }

    if (HasRlcCounters())
    {
        // Stop RLC counters: this needs to be done with a COPY_DATA command.
        regRLC_PERFMON_CNTL rlcPerfmonCntl = {};
        rlcPerfmonCntl.bits.PERFMON_STATE = perfmonState;

        pCmdSpace += cmdUtil.BuildCopyData(COPY_DATA_SEL_REG,
                                           regInfo.mmRlcPerfmonCntl,
                                           COPY_DATA_SEL_SRC_IMME_DATA,
                                           rlcPerfmonCntl.u32All,
                                           COPY_DATA_SEL_COUNT_1DW,
                                           COPY_DATA_ENGINE_ME,
                                           COPY_DATA_WR_CONFIRM_NO_WAIT,
                                           pCmdSpace);
    }

    // Reset SQ_PERFCOUNTER_CTRL and clear MC counters.
    if (reset)
    {
        if (m_sqPerfCounterCtrl.u32All != 0)
        {
            static constexpr regSQ_PERFCOUNTER_CTRL SqPerfCounterCtrl = {};

            pCmdSpace = pCmdStream->WriteSetOneConfigReg(regInfo.mmSqPerfCounterCtrl,
                                                         SqPerfCounterCtrl.u32All,
                                                         pCmdSpace);
        }

        if (HasMcCounters())
        {
            regMC_SEQ_PERF_CNTL mcSeqPerfCntl = {};
            mcSeqPerfCntl.bits.MONITOR_PERIOD = McSeqMonitorPeriod;
            mcSeqPerfCntl.bits.CNTL           = McSeqClearCounter;

            pCmdSpace = pCmdStream->WriteSetOneConfigReg(mmMC_SEQ_PERF_CNTL,
                                                         mcSeqPerfCntl.u32All,
                                                         pCmdSpace);
        }
    }

    if (forCompute == true)
    {
        regCOMPUTE_PERFCOUNT_ENABLE__CI__VI computePerfCounterEn = {};
        computePerfCounterEn.bits.PERFCOUNT_ENABLE = 0;

        pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderCompute>(mmCOMPUTE_PERFCOUNT_ENABLE__CI__VI,
                                                                computePerfCounterEn.u32All,
                                                                pCmdSpace);
    }
    else
    {
        // Write the command sequence to stop event-based counters.
        pCmdSpace += cmdUtil.BuildEventWrite(PERFCOUNTER_STOP, pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Samples performance counters by issuing commands into the specified command buffer which will instruct the HW to
// write the counter data to the specified virtual address. Returns the next unused DWORD in pCmdSpace.
uint32* PerfExperiment::WriteSamplePerfCounters(
    gpusize     baseGpuVirtAddr, ///< Base GPU virtual address to write counter data into
    CmdStream*  pCmdStream,
    uint32*     pCmdSpace
    ) const
{
    const auto& cmdUtil = m_device.CmdUtil();
    const auto& regInfo = cmdUtil.GetRegInfo();

    // Write the command sequence to stop and sample event-based counters.
    pCmdSpace += cmdUtil.BuildEventWrite(PERFCOUNTER_SAMPLE, pCmdSpace);
    pCmdSpace += cmdUtil.BuildEventWrite(PERFCOUNTER_STOP, pCmdSpace);

    // Freeze and sample graphics state based counters.
    regCP_PERFMON_CNTL cpPerfmonCntl = {};
    cpPerfmonCntl.bits.PERFMON_STATE         = PerfmonStopCounting;
    cpPerfmonCntl.bits.PERFMON_SAMPLE_ENABLE = 1;

    pCmdSpace = pCmdStream->WriteSetOneConfigReg(regInfo.mmCpPerfmonCntl,
                                                 cpPerfmonCntl.u32All,
                                                 pCmdSpace);

    if (HasSrbmCounters())
    {
        // Freeze and sample SRBM counters.
        regSRBM_PERFMON_CNTL srbmPerfmonCntl = {};
        srbmPerfmonCntl.bits.PERFMON_STATE         = PerfmonStopCounting;
        srbmPerfmonCntl.bits.PERFMON_SAMPLE_ENABLE = 1;

        pCmdSpace = pCmdStream->WriteSetOnePrivilegedConfigReg(regInfo.mmSrbmPerfmonCntl,
                                                               srbmPerfmonCntl.u32All,
                                                               pCmdSpace);
    }

    if (HasRlcCounters())
    {
        // Freeze and sample RLC counters: this needs to be done with a COPY_DATA command.
        regRLC_PERFMON_CNTL rlcPerfmonCntl = {};
        rlcPerfmonCntl.bits.PERFMON_STATE         = PerfmonStopCounting;
        rlcPerfmonCntl.bits.PERFMON_SAMPLE_ENABLE = 1;

        pCmdSpace += cmdUtil.BuildCopyData(COPY_DATA_SEL_REG,
                                           regInfo.mmRlcPerfmonCntl,
                                           COPY_DATA_SEL_SRC_IMME_DATA,
                                           rlcPerfmonCntl.u32All,
                                           COPY_DATA_SEL_COUNT_1DW,
                                           COPY_DATA_ENGINE_ME,
                                           COPY_DATA_WR_CONFIRM_NO_WAIT,
                                           pCmdSpace);
    }

    // Need to perform a wait-idle-clean before copying counter data registers.
    pCmdSpace = WriteWaitIdleClean(true, pCmdStream->GetEngineType() == EngineTypeCompute, pCmdSpace);

    // Next, walk the counter list and copy counter data to GPU memory.
    for (auto it = m_globalCtrs.Begin(); it.Get(); it.Next())
    {
        const PerfCounter*const pPerfCounter = static_cast<PerfCounter*>(*it.Get());
        PAL_ASSERT(pPerfCounter != nullptr);

        // Issue commands for the performance counter to write data to GPU memory.
        pCmdSpace = pPerfCounter->WriteSampleCommands(baseGpuVirtAddr, pCmdStream, pCmdSpace);

        // This loop doesn't have a trivial upper-limit so we must be careful to not overflow the reserve buffer.
        // If CPU-performance of perf counters is later deemed to be important we can make this code smarter.
        pCmdStream->CommitCommands(pCmdSpace);
        pCmdSpace = pCmdStream->ReserveCommands();
    }

    if (HasIndexedCounters())
    {
        pCmdSpace = WriteResetGrbmGfxIndex(pCmdStream, pCmdSpace);
    }

    // Reset MC_SEQ broadcasts
    if (HasMcCounters())
    {
        const auto& perfInfo = m_device.Parent()->ChipProperties().gfx6.perfCounterInfo;

        pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(perfInfo.mcConfigRegAddress,
                                                      perfInfo.mcWriteEnableMask,
                                                      pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Counters associated with indexed GPU blocks need to write GRBM_GFX_INDEX to mask-off the SE/SH/Instance the counter
// is sampling from. Also, thread traces are tied to a specific SE/SH and need to write this as well.
//
// This issues the PM4 command which resets GRBM_GFX_INDEX to broadcast to the whole chip if any of our perf counters
// or thread traces would have modified the value of GRBM_GFX_INDEX.
//
// Returns the next unused DWORD in pCmdSpace.
uint32* PerfExperiment::WriteResetGrbmGfxIndex(
    CmdStream*  pCmdStream,
    uint32*     pCmdSpace
    ) const
{
    PAL_ASSERT(HasIndexedCounters() || HasThreadTraces() || HasSpmTrace());

    regGRBM_GFX_INDEX grbmGfxIndex = {};
    grbmGfxIndex.bits.SE_BROADCAST_WRITES       = 1;
    grbmGfxIndex.bits.SH_BROADCAST_WRITES       = 1;
    grbmGfxIndex.bits.INSTANCE_BROADCAST_WRITES = 1;

    return pCmdStream->WriteSetOneConfigReg(m_device.CmdUtil().GetRegInfo().mmGrbmGfxIndex,
                                            grbmGfxIndex.u32All,
                                            pCmdSpace);
}

// =====================================================================================================================
// Helper method which writes commands to do a wait-idle-clean. Returns the next unused DWORD in pCmdSpace.
uint32* PerfExperiment::WriteWaitIdleClean(
    bool    cacheFlush,       ///< Indicates if we should also flush caches
    bool    forComputeEngine,
    uint32* pCmdSpace
    ) const
{
    const CmdUtil& cmdUtil = m_device.CmdUtil();

    // NOTE: On Gfx6+, we achieve a wait-idle-clean by issuing a CS_PARTIAL_FLUSH followed by a SURFACE_SYNC with all
    //       base/action bits enabled to ensure outstanding reads and writes are complete.

    regCP_COHER_CNTL cpCoherCntl;
    cpCoherCntl.u32All = CpCoherCntlStallMask;

    if (cacheFlush)
    {
        cpCoherCntl.u32All |= (CP_COHER_CNTL__TCL1_ACTION_ENA_MASK      |
                               CP_COHER_CNTL__TC_ACTION_ENA_MASK        |
                               CP_COHER_CNTL__CB_ACTION_ENA_MASK        |
                               CP_COHER_CNTL__DB_ACTION_ENA_MASK        |
                               CP_COHER_CNTL__SH_KCACHE_ACTION_ENA_MASK |
                               CP_COHER_CNTL__SH_ICACHE_ACTION_ENA_MASK);
    }

    pCmdSpace += cmdUtil.BuildEventWrite(CS_PARTIAL_FLUSH, pCmdSpace);
    pCmdSpace += cmdUtil.BuildGenericSync(cpCoherCntl,
                                          SURFACE_SYNC_ENGINE_ME,
                                          FullSyncBaseAddr,
                                          FullSyncSize,
                                          forComputeEngine,
                                          pCmdSpace);

    return pCmdSpace;
}

} // Gfx6
} // Pal
