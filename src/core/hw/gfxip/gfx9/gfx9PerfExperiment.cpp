/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/gfx9/gfx9CmdStream.h"
#include "core/hw/gfxip/gfx9/gfx9CmdUtil.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9PerfCounter.h"
#include "core/hw/gfxip/gfx9/gfx9PerfCtrInfo.h"
#include "core/hw/gfxip/gfx9/gfx9PerfExperiment.h"
#include "core/hw/gfxip/gfx9/gfx9PerfTrace.h"
#include "palDequeImpl.h"
#include "palHashMapImpl.h"
#include "palInlineFuncs.h"

using namespace Pal::Gfx9::PerfCtrInfo;

namespace Pal
{
namespace Gfx9
{

// =====================================================================================================================
PerfExperiment::PerfExperiment(
    const Device*                   pDevice,
    const PerfExperimentCreateInfo& createInfo)
    :
    Pal::PerfExperiment(pDevice->Parent(), createInfo),
    m_device(*pDevice),
    m_gfxLevel(pDevice->Parent()->ChipProperties().gfxLevel),
    m_spiConfigCntlDefault(0)
{
    InitBlockUsage();
    m_counterFlags.u32All      = 0;
    m_sqPerfCounterCtrl.u32All = 0;

    if (m_gfxLevel == GfxIpLevel::GfxIp9)
    {
        m_spiConfigCntlDefault = Gfx09::mmSPI_CONFIG_CNTL_DEFAULT;
    }
}

// =====================================================================================================================
// Initializes the usage status for each GPU block's performance counters.
void PerfExperiment::InitBlockUsage()
{
    const Gfx9PerfCounterInfo& perfInfo = m_device.Parent()->ChipProperties().gfx9.perfCounterInfo;

    // TODO: 'PerfCtrEmpty' is zero, should we simply memset the whole array?
    //       (This method is more explicit, for what its worth.)
    for (size_t blk = 0; blk < static_cast<size_t>(GpuBlock::Count); ++blk)
    {
        const size_t blockInstances = (perfInfo.block[blk].numInstances    *
                                       perfInfo.block[blk].numShaderArrays *
                                       perfInfo.block[blk].numShaderEngines);
        for (size_t inst = 0; inst < blockInstances; ++inst)
        {
            for (size_t ctr = 0; ctr < perfInfo.block[blk].numCounters; ++ctr)
            {
                // Mark the counter as completely unused.
                m_blockUsage[blk].instance[inst].counter[ctr] = PerfCtrEmpty;
            }
        }
    }
}

// =====================================================================================================================
// Checks that a performance counter resource is available for the specified counter create info. Updates the tracker
// for counter resource usage.
Result PerfExperiment::ReserveCounterResource(
    const PerfCounterInfo& info,            ///< [in] Performance counter creation info
    uint32*                pCounterId,      ///< [out] Available counter slot we found
    uint32*                pCounterSubId)   ///< [out] Available counter sub-slot we found
{
    const Gfx9PerfCounterInfo& perfInfo      = m_device.Parent()->ChipProperties().gfx9.perfCounterInfo;
    const size_t               blockNum      = static_cast<size_t>(info.block);
    const auto&                blockPerfInfo = perfInfo.block[blockNum];
    auto&                      blockUsage    = m_blockUsage[blockNum];
    Result                     result        = Result::Success;

    PAL_ASSERT(info.instance < PerfCtrInfo::MaxNumBlockInstances);

    // Make sure the caller is requesting a valid event ID
    if (info.eventId < blockPerfInfo.maxEventId)
    {
        // Start looping over the first counter for the desired GPU block & instance. If a counter slot is free for the
        // desired instanceId, stop searching and use it.
        bool   emptySlotFound = false;
        uint32 counterId      = 0;
        while ((emptySlotFound == false) && (counterId < blockPerfInfo.numCounters))
        {
            const PerfCtrUseStatus ctrStatus = blockUsage.instance[info.instance].counter[counterId];

            // 64-bit summary counter: Only one can exist per slot, so the slot must be
            // completely empty in order for us to use it. (The SQ has only one streaming
            // counter per summary counter slot, so use the same logic for all SQ ctr's.)
            emptySlotFound = (ctrStatus == PerfCtrEmpty);

            if (emptySlotFound == false)
            {
                counterId++;
            }
        }

        if (emptySlotFound)
        {
            uint32 counterSubId = 0;

            // If we get here, we successfully found a slot at 'counterId' for the new counter. Need
            // to update its usage status to reflect that a counter is being added.
            PerfCtrUseStatus*const pCtrStatus = &blockUsage.instance[info.instance].counter[counterId];

            // 64-bit summary counter: mark the counter as in-use. The sub-slot ID has no meaning here.
            (*pCtrStatus)    = PerfCtr64BitSummary;
            (*pCounterId)    = counterId;
            (*pCounterSubId) = counterSubId;
        }
        else
        {
            result = Result::ErrorOutOfGpuMemory;
        }
    }
    else
    {
        result = Result::ErrorInvalidValue;
    }

    return result;
}

// =====================================================================================================================
void PerfExperiment::SetCntrRate(
    uint32  rate)
{
    if (m_gfxLevel == GfxIpLevel::GfxIp9)
    {
        m_sqPerfCounterCtrl.gfx09.CNTR_RATE = rate;
    }
}

// =====================================================================================================================
// Checks that a performance counter resource is available for the specified counter create info. If the resource is
// available, instantiates a new GcnPerfCounter object for the caller to use.
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
            m_counterFlags.indexedBlocks  |= pCounter->IsIndexed();
            m_counterFlags.eaCounters     |= (info.block == GpuBlock::Ea);
            m_counterFlags.atcCounters    |= (info.block == GpuBlock::Atc);
            m_counterFlags.atcL2Counters  |= (info.block == GpuBlock::AtcL2);
            m_counterFlags.mcVmL2Counters |= (info.block == GpuBlock::McVmL2);
            m_counterFlags.rpbCounters    |= (info.block == GpuBlock::Rpb);
            m_counterFlags.rmiCounters    |= (info.block == GpuBlock::Rmi);
            m_counterFlags.rlcCounters    |= (info.block == GpuBlock::Rlc);
            m_counterFlags.sqCounters     |= (info.block == GpuBlock::Sq);
            m_counterFlags.taCounters     |= (info.block == GpuBlock::Ta);
            m_counterFlags.tdCounters     |= (info.block == GpuBlock::Td);
            m_counterFlags.tcpCounters    |= (info.block == GpuBlock::Tcp);
            m_counterFlags.tccCounters    |= (info.block == GpuBlock::Tcc);
            m_counterFlags.tcaCounters    |= (info.block == GpuBlock::Tca);
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 424
            m_counterFlags.umcchCounters  |= (info.block == GpuBlock::Umcch);
#endif

            const auto& chipProps = m_device.Parent()->ChipProperties();
            if ((info.block == GpuBlock::Ta)  ||
                (info.block == GpuBlock::Td)  ||
                (info.block == GpuBlock::Tcp) ||
                (info.block == GpuBlock::Tcc) ||
                (info.block == GpuBlock::Tca))
            {
                static constexpr uint32 SqDefaultCounterRate = 0;

                m_sqPerfCounterCtrl.bits.PS_EN |= 1;
                m_sqPerfCounterCtrl.bits.VS_EN |= 1;
                m_sqPerfCounterCtrl.bits.GS_EN |= 1;
                m_sqPerfCounterCtrl.bits.ES_EN |= 1;
                m_sqPerfCounterCtrl.bits.HS_EN |= 1;
                m_sqPerfCounterCtrl.bits.LS_EN |= 1;
                m_sqPerfCounterCtrl.bits.CS_EN |= 1;

                SetCntrRate(SqDefaultCounterRate);

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

                SetCntrRate(SqDefaultCounterRate);

                if (m_gfxLevel == GfxIpLevel::GfxIp9)
                {
                    // SQ-perWave and TA/TC/TD may interfere each other, consider collect in different pass.
                    PAL_ALERT((HasTaCounters()  ||
                               HasTdCounters()  ||
                               HasTcpCounters() ||
                               HasTccCounters() ||
                               HasTcaCounters()));
                }
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
    const ThreadTraceInfo& info)
{
    PAL_ASSERT(info.traceType == PerfTraceType::ThreadTrace);

    // Instantiate a new thread trace object.
    ThreadTrace* pThreadTrace = nullptr;
    Result       result       = Result::Success;

    if (m_gfxLevel == GfxIpLevel::GfxIp9)
    {
        pThreadTrace = PAL_NEW(Gfx9ThreadTrace,
                               m_device.GetPlatform(),
                               Util::SystemAllocType::AllocInternal)(&m_device, info);
    }

    if (pThreadTrace != nullptr)
    {
        result = pThreadTrace->Init();

        if (result == Result::Success)
        {
            m_pThreadTrace[info.instance] = pThreadTrace;

            ++m_numThreadTrace;
        }
        else
        {
            // Ok, we were able to create the thread-trace object, but it failed validation.
            PAL_SAFE_DELETE(pThreadTrace, m_device.GetPlatform());
        }
    }
    else
    {
        result = Result::ErrorOutOfMemory;
    }

    return result;
}

// =====================================================================================================================
// Validates SPM trace create info and constructs a hardware layer specific Spm trace object.
Result PerfExperiment::ConstructSpmTraceObj(
    const SpmTraceCreateInfo& info,
    Pal::SpmTrace**           ppSpmTrace)
{
    Result result = Result::Success;
    PAL_ASSERT(ppSpmTrace != nullptr);

    const auto& perfCounterInfo = m_device.Parent()->ChipProperties().gfx9.perfCounterInfo;

    PerfExperimentProperties perfExpProperties = { };
    result = m_device.Parent()->GetPerfExperimentProperties(&perfExpProperties);

    // Validate the SPM trace create info.
    for (uint32 i = 0; i < info.numPerfCounters && (result == Result::Success); i++)
    {
        const uint32 blockIdx     = static_cast<uint32>(info.pPerfCounterInfos[i].block);
        const auto& block         = perfCounterInfo.block[blockIdx];
        const uint32 maxInstances = perfExpProperties.blocks[blockIdx].instanceCount;

        // Check if block, eventid and instance number are within bounds.
        if (((info.pPerfCounterInfos[i].block    < GpuBlock::Count)  && // valid block
             (info.pPerfCounterInfos[i].eventId  < block.maxEventId) && // valid event
             (info.pPerfCounterInfos[i].instance < maxInstances)     && // valid instance
             (block.numStreamingCounters         > 0)) == false)        // supports spm
        {
            result = Result::ErrorInvalidValue;
        }
    }

    if (result == Result::Success)
    {
        SpmTrace* pSpmTrace = nullptr;

        if (m_device.Parent()->ChipProperties().gfxLevel == GfxIpLevel::GfxIp9)
        {
            pSpmTrace = static_cast<Pal::SpmTrace*>(PAL_NEW (Gfx9SpmTrace,
                                                             m_device.GetPlatform(),
                                                             Util::SystemAllocType::AllocInternal)(&m_device));
        }
        if (pSpmTrace != nullptr)
        {
            (*ppSpmTrace) = pSpmTrace;
        }
        else
        {
            // Allocation of Spm trace object failed.
            result = Result::ErrorOutOfMemory;
        }
    }

    return result;
}

// =====================================================================================================================
// Creates a StreamingPerfCounter object and returns a pointer.
Pal::StreamingPerfCounter* PerfExperiment::CreateStreamingPerfCounter(
    GpuBlock block,
    uint32   instance,
    uint32   slot)
{
    Pal::StreamingPerfCounter* pStreamingCounter = nullptr;

    if (m_device.Parent()->ChipProperties().gfxLevel == GfxIpLevel::GfxIp9)
    {
        pStreamingCounter = static_cast<Pal::StreamingPerfCounter*>(PAL_NEW (Gfx9StreamingPerfCounter,
                                                                             m_device.GetPlatform(),
                                                                             Util::SystemAllocType::AllocObject)
                                                                             (m_device, block, instance, slot));
    }

    if (pStreamingCounter == nullptr)
    {
        // Allocation of StreamingPerfCounter object failed.
        PAL_ASSERT_ALWAYS();
    }

    return pStreamingCounter;
}

// =====================================================================================================================
// Updates internal flags.
void PerfExperiment::UpdateCounterFlags(
    GpuBlock block,
    bool     isIndexed)
{
    m_counterFlags.indexedBlocks |= isIndexed;
    m_counterFlags.rlcCounters   |= (block == GpuBlock::Rlc);
    m_counterFlags.sqCounters    |= (block == GpuBlock::Sq);
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
        SetCntrRate(SqDefaultCounterRate);

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
        SetCntrRate(SqDefaultCounterRate);

        // SQ-perWave and TA/TC/TD may interfere each other, consider collect in different pass.
        PAL_ALERT((HasTaCounters()  ||
                   HasTdCounters()  ||
                   HasTcpCounters() ||
                   HasTccCounters() ||
                   HasTcaCounters()));
    }
}

// =====================================================================================================================
// Issues commands into the specified command stream which instruct the HW to begin recording performance data.
void PerfExperiment::IssueBegin(
    Pal::CmdStream* pPalCmdStream    ///< [in,out] Command stream to write PM4 commands into
    ) const
{
    PAL_ASSERT(IsFinalized());

    CmdStream*       pCmdStream = static_cast<CmdStream*>(pPalCmdStream);
    const auto&      chipProps  = m_device.Parent()->ChipProperties();
    const auto&      cmdUtil    = m_device.CmdUtil();
    const auto&      regInfo    = cmdUtil.GetRegInfo();
    const EngineType engineType = pCmdStream->GetEngineType();
    uint32*          pCmdSpace  = pCmdStream->ReserveCommands();

    // Wait for GFX engine to become idle before freezing or sampling counters.
    pCmdSpace = WriteWaitIdleClean(pCmdStream, CacheFlushOnPerfCounter(), engineType, pCmdSpace);

    // Enable perfmon clocks for all blocks. This register controls medium grain clock gating.
    if (m_gfxLevel == GfxIpLevel::GfxIp9)
    {
        pCmdSpace = pCmdStream->WriteSetOnePrivilegedConfigReg(Gfx09::mmRLC_PERFMON_CLK_CNTL, 1, pCmdSpace);
    }

    if (chipProps.gfx9.sqgEventsEnabled == false)
    {
        // Both SQ performance counters and traces need the SQG events enabled. Force them on
        // ourselves if KMD doesn't have them active by default.
        regSPI_CONFIG_CNTL spiConfigCntl = {};
        spiConfigCntl.u32All                     = m_spiConfigCntlDefault;
        spiConfigCntl.bits.ENABLE_SQG_TOP_EVENTS = 1;
        spiConfigCntl.bits.ENABLE_SQG_BOP_EVENTS = 1;

        // On some ASICs we have to WaitIdle before writing this register. We do this already, so there isn't a need
        // to do it again.
        pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(regInfo.mmSpiConfigCntl, spiConfigCntl.u32All, pCmdSpace);
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
        pCmdSpace += cmdUtil.BuildNonSampleEventWrite(THREAD_TRACE_START, engineType, pCmdSpace);

        // TODO: Issuing a PS_PARTIAL_FLUSH and a wait-idle clean seems to help us more reliably gather thread-trace
        //       data. Need to investigate why this helps.
        if (engineType != EngineTypeCompute)
        {
            pCmdSpace += cmdUtil.BuildNonSampleEventWrite(PS_PARTIAL_FLUSH,
                                                          engineType,
                                                          pCmdSpace);
        }
        pCmdSpace  = WriteWaitIdleClean(pCmdStream, true, engineType, pCmdSpace);
    }

    if (HasSpmTrace())
    {
        pCmdSpace = m_pSpmTrace->WriteSetupCommands(m_vidMem.GpuVirtAddr(), pPalCmdStream, pCmdSpace);

        pCmdStream->CommitCommands(pCmdSpace);
        pCmdSpace = pCmdStream->ReserveCommands();

        pCmdSpace = WriteResetGrbmGfxIndex(pCmdStream, pCmdSpace);

        pCmdSpace = WriteWaitIdleClean(pCmdStream, true, engineType, pCmdSpace);

        if (m_sqPerfCounterCtrl.u32All != 0)
        {
            pCmdSpace = pCmdStream->WriteSetOneConfigReg(m_device.CmdUtil().GetRegInfo().mmSqPerfCounterCtrl,
                                                         m_sqPerfCounterCtrl.u32All,
                                                         pCmdSpace);
        }

        pCmdSpace = m_pSpmTrace->WriteStartCommands(pPalCmdStream, pCmdSpace);
        pCmdSpace += cmdUtil.BuildNonSampleEventWrite(PERFCOUNTER_START, engineType, pCmdSpace);
    }

    if (HasGlobalCounters())
    {
        pCmdSpace = WriteComputePerfCountEnable(pCmdStream, pCmdSpace, true);

        // Need to freeze and reset performance counters.
        pCmdSpace = WriteStopPerfCounters(true, pCmdStream, pCmdSpace);

        // TODO: Investigate: DXX clears the counter sample buffer here. Do we need to do the same?? Seems that we
        //       wouldn't since we record an initial sample of the counters later-on in this function.

        // Issue commands to setup the finalized performance counter select registers.
        pCmdSpace = WriteSetupPerfCounters(pCmdStream, pCmdSpace);

        // Record an initial sample of the performance counter data at the "begin" offset
        // in GPU memory.
        pCmdSpace = WriteSamplePerfCounters(m_vidMem.GpuVirtAddr() + m_ctrBeginOffset,
                                            pCmdStream,
                                            pCmdSpace);

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

    CmdStream*       pCmdStream = static_cast<CmdStream*>(pPalCmdStream);
    const auto&      cmdUtil    = m_device.CmdUtil();
    const auto&      regInfo    = cmdUtil.GetRegInfo();
    const EngineType engineType = pCmdStream->GetEngineType();
    uint32*          pCmdSpace  = pCmdStream->ReserveCommands();

    // Wait for GFX engine to become idle before freezing or sampling counters.
    pCmdSpace = WriteWaitIdleClean(pCmdStream, CacheFlushOnPerfCounter(), engineType, pCmdSpace);

    if (HasGlobalCounters())
    {
        // Record a final sample of the performance counter data at the "end" offset in GPU memory.
        pCmdSpace = WriteSamplePerfCounters(m_vidMem.GpuVirtAddr() + m_ctrEndOffset,
                                            pCmdStream,
                                            pCmdSpace);

        // Issue commands to stop recording perf counter data.
        pCmdSpace = WriteStopPerfCounters(true, pCmdStream, pCmdSpace);
    }

    if (HasThreadTraces())
    {
        // Issue a VGT event to stop thread traces.
        pCmdSpace += cmdUtil.BuildNonSampleEventWrite(THREAD_TRACE_STOP, engineType, pCmdSpace);

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
        pCmdSpace = pHwlCmdStream->WriteSetOneConfigReg(mmCP_PERFMON_CNTL,
                                                        cpPerfmonCntl.u32All,
                                                        pCmdSpace);

        pCmdSpace += m_device.CmdUtil().BuildNonSampleEventWrite(PERFCOUNTER_SAMPLE, engineType, pCmdSpace);

        // Stop all performance counters.
        cpPerfmonCntl.u32All                 = 0;
        cpPerfmonCntl.bits.PERFMON_STATE     = PerfmonStopCounting;
        cpPerfmonCntl.bits.SPM_PERFMON_STATE = PerfmonStopCounting;

        pCmdSpace = pHwlCmdStream->WriteSetOneConfigReg(mmCP_PERFMON_CNTL,
                                                        cpPerfmonCntl.u32All,
                                                        pCmdSpace);

        pCmdSpace += m_device.CmdUtil().BuildNonSampleEventWrite(PERFCOUNTER_STOP, engineType, pCmdSpace);

        // Need a WaitIdle here before zeroing the RLC SPM controls, else we get a page fault indicating that the data
        // is still being written at the moment.
        pCmdSpace = WriteWaitIdleClean(pCmdStream, false, engineType, pCmdSpace);

        if (m_sqPerfCounterCtrl.u32All != 0)
        {
            pCmdSpace = pCmdStream->WriteSetOneConfigReg(m_device.CmdUtil().GetRegInfo().mmSqPerfCounterCtrl,
                                                         0,
                                                         pCmdSpace);
        }

        pCmdSpace = m_pSpmTrace->WriteEndCommands(pCmdStream, pCmdSpace);
    }

    if (m_device.Parent()->ChipProperties().gfx9.sqgEventsEnabled == false)
    {
        pCmdSpace = WriteWaitIdleClean(pCmdStream, false, engineType, pCmdSpace);

        // Reset the default value of SPI_CONFIG_CNTL if we overrode it in HwlIssueBegin().
        regSPI_CONFIG_CNTL spiConfigCntl = {};
        spiConfigCntl.u32All = m_spiConfigCntlDefault;

        pCmdSpace = pCmdStream->WriteSetOnePerfCtrReg(regInfo.mmSpiConfigCntl, spiConfigCntl.u32All, pCmdSpace);
    }

    if (HasSqCounters())
    {
        // SQ tests require RLC_PERFMON_CLK_CNTL set to work
        if (m_gfxLevel == GfxIpLevel::GfxIp9)
        {
            pCmdSpace = pCmdStream->WriteSetOnePrivilegedConfigReg(Gfx09::mmRLC_PERFMON_CLK_CNTL, 0, pCmdSpace);
        }
    }

    pCmdSpace = WriteComputePerfCountEnable(pCmdStream, pCmdSpace, false);

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

    // NOTE: DXX doesn't seem to stop active thread traces here. Do we need to? How would we do that without resetting
    //       the trace data which has already been recorded?
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
    const auto& perfInfo  = chipProps.gfx9.perfCounterInfo;

    // NOTE: The SDMA block requires special handling for counter setup because multiple counters' state gets
    //       packed into the same registers.
    regSDMA0_PERFMON_CNTL  sdma0PerfmonCntl = {};
    regSDMA1_PERFMON_CNTL  sdma1PerfmonCntl = {};

    if (HasUmcchCounters())
    {
        pCmdSpace = WriteSetupUmcchCntlRegs(pCmdStream, pCmdSpace);
    }

    // Walk the counter list and set select & filter registers.
    for (auto it = m_globalCtrs.Begin(); it.Get(); it.Next())
    {
        const PerfCounter*const pPerfCounter = static_cast<PerfCounter*>(*it.Get());
        PAL_ASSERT(pPerfCounter != nullptr);

        if (pPerfCounter->BlockType() == GpuBlock::Dma)
        {
            // Accumulate the value of the SDMA perfmon control register(s).
            const uint32 regValue = pPerfCounter->SetupSdmaSelectReg(&sdma0PerfmonCntl, &sdma1PerfmonCntl);

            // Special handling for SDMA: the register info is per instance rather than per counter slot.
            const uint32 blockIdx   = static_cast<uint32>(pPerfCounter->BlockType());
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
    const auto&      device     = *(m_device.Parent());
    const auto&      cmdUtil    = m_device.CmdUtil();
    const auto&      regInfo    = cmdUtil.GetRegInfo();
    const EngineType engineType = pCmdStream->GetEngineType();

    if (HasRlcCounters())
    {
        // Start RLC counters: this needs to be done with a COPY_DATA command.
        regRLC_PERFMON_CNTL rlcPerfmonCntl = {};
        rlcPerfmonCntl.bits.PERFMON_STATE = CP_PERFMON_STATE_START_COUNTING;

        if (engineType == EngineTypeCompute)
        {
            pCmdSpace += cmdUtil.BuildCopyDataCompute(dst_sel__mec_copy_data__mem_mapped_register,
                                                      regInfo.mmRlcPerfmonCntl,
                                                      src_sel__mec_copy_data__immediate_data,
                                                      rlcPerfmonCntl.u32All,
                                                      count_sel__mec_copy_data__32_bits_of_data,
                                                      wr_confirm__mec_copy_data__do_not_wait_for_confirmation,
                                                      pCmdSpace);
        }
        else
        {
            pCmdSpace += cmdUtil.BuildCopyDataGraphics(engine_sel__me_copy_data__micro_engine,
                                                       dst_sel__me_copy_data__mem_mapped_register,
                                                       regInfo.mmRlcPerfmonCntl,
                                                       src_sel__me_copy_data__immediate_data,
                                                       rlcPerfmonCntl.u32All,
                                                       count_sel__me_copy_data__32_bits_of_data,
                                                       wr_confirm__me_copy_data__do_not_wait_for_confirmation,
                                                       pCmdSpace);
        }
    }

    // Only configure memory and SQ counters on initial startup.
    if (restart == false)
    {
        if (HasEaCounters())
        {
            // This has to be set for any EA perf counters to work.
            regGCEA_PERFCOUNTER_RSLT_CNTL  gceaPerfCntrResultCntl = {};
            gceaPerfCntrResultCntl.bits.ENABLE_ANY = 1;

            pCmdSpace = pCmdStream->WriteSetOnePrivilegedConfigReg(regInfo.mmEaPerfResultCntl,
                                                                   gceaPerfCntrResultCntl.u32All,
                                                                   pCmdSpace);
        }

        if (HasAtcCounters())
        {
            // This has to be set for any ATC perf counters to work.
            regATC_PERFCOUNTER_RSLT_CNTL  atcPerfCntrResultCntl = {};
            atcPerfCntrResultCntl.bits.ENABLE_ANY = 1;

            pCmdSpace = pCmdStream->WriteSetOnePrivilegedConfigReg(regInfo.mmAtcPerfResultCntl,
                                                                   atcPerfCntrResultCntl.u32All,
                                                                   pCmdSpace);
        }

        if (HasAtcL2Counters())
        {
            // This has to be set for any ATC L2 perf counters to work.
            regATC_L2_PERFCOUNTER_RSLT_CNTL  atcL2PerfCntrResultCntl = {};
            atcL2PerfCntrResultCntl.bits.ENABLE_ANY = 1;

            pCmdSpace = pCmdStream->WriteSetOnePrivilegedConfigReg(regInfo.mmAtcL2PerfResultCntl,
                                                                   atcL2PerfCntrResultCntl.u32All,
                                                                   pCmdSpace);
        }

        if (HasMcVmL2Counters())
        {
            // This has to be set for any MC VM L2 perf counters to work.
            regMC_VM_L2_PERFCOUNTER_RSLT_CNTL  mcVmL2PerfCntrResultCntl = {};
            mcVmL2PerfCntrResultCntl.bits.ENABLE_ANY = 1;

            pCmdSpace = pCmdStream->WriteSetOnePrivilegedConfigReg(regInfo.mmMcVmL2PerfResultCntl,
                                                                   mcVmL2PerfCntrResultCntl.u32All,
                                                                   pCmdSpace);
        }

        if (HasRpbCounters())
        {
            // This has to be set for any RPB perf counters to work.
            regRPB_PERFCOUNTER_RSLT_CNTL  rpbPerfCntrResultCntl = {};
            rpbPerfCntrResultCntl.bits.ENABLE_ANY = 1;

            pCmdSpace = pCmdStream->WriteSetOnePrivilegedConfigReg(regInfo.mmRpbPerfResultCntl,
                                                                   rpbPerfCntrResultCntl.u32All,
                                                                   pCmdSpace);
        }
        if (m_sqPerfCounterCtrl.u32All != 0)
        {
            pCmdSpace = pCmdStream->WriteSetOneConfigReg(regInfo.mmSqPerfCounterCtrl,
                                                         m_sqPerfCounterCtrl.u32All,
                                                         pCmdSpace);
        }
    }

    if (HasRmiCounters())
    {
        static constexpr uint32 RmiEnSelOn                     = 1;
        static constexpr uint32 RmiEventWindowMask0Default     = 0x1;
        static constexpr uint32 RmiEventWindowMask1Default     = 0x2;
        static constexpr uint32 RmiChannelIdAll                = 0x8;
        static constexpr uint32 RmiBurstlengthThresholdDefault = 1;

        regRMI_PERF_COUNTER_CNTL rmiPerfCounterCntl = {0};
        rmiPerfCounterCntl.bits.TRANS_BASED_PERF_EN_SEL             = RmiEnSelOn;
        rmiPerfCounterCntl.bits.EVENT_BASED_PERF_EN_SEL             = RmiEnSelOn;
        rmiPerfCounterCntl.bits.TC_PERF_EN_SEL                      = RmiEnSelOn;
        rmiPerfCounterCntl.bits.PERF_EVENT_WINDOW_MASK0             = RmiEventWindowMask0Default;
        rmiPerfCounterCntl.bits.PERF_COUNTER_CID                    = RmiChannelIdAll;
        rmiPerfCounterCntl.bits.PERF_COUNTER_BURST_LENGTH_THRESHOLD = RmiBurstlengthThresholdDefault;

        if (m_gfxLevel == GfxIpLevel::GfxIp9)
        {
            if (device.ChipProperties().familyId == FAMILY_AI)
            {
                rmiPerfCounterCntl.vega.PERF_EVENT_WINDOW_MASK1 = RmiEventWindowMask1Default;
            }
            else if (IsRaven(device))
            {
                rmiPerfCounterCntl.rv1x.PERF_EVENT_WINDOW_MASK1 = RmiEventWindowMask1Default;
            }
        }

        if (restart == false)
        {
            rmiPerfCounterCntl.bits.PERF_SOFT_RESET = 1;
        }
        pCmdSpace = pCmdStream->WriteSetOneConfigReg(mmRMI_PERF_COUNTER_CNTL,
                                                     rmiPerfCounterCntl.u32All,
                                                     pCmdSpace);
    }

    if (engineType == EngineTypeCompute)
    {
        regCOMPUTE_PERFCOUNT_ENABLE computePerfCounterEn = {};
        computePerfCounterEn.bits.PERFCOUNT_ENABLE = 1;

        pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderCompute>(mmCOMPUTE_PERFCOUNT_ENABLE,
                                                                computePerfCounterEn.u32All,
                                                                pCmdSpace);
    }
    else
    {
        // Write the command sequence to start event-based counters.
        pCmdSpace += cmdUtil.BuildNonSampleEventWrite(PERFCOUNTER_START, engineType, pCmdSpace);
    }

    // Start graphics state based counters.
    regCP_PERFMON_CNTL cpPerfmonCntl = {};
    if (HasGlobalCounters())
    {
        cpPerfmonCntl.bits.PERFMON_STATE = CP_PERFMON_STATE_START_COUNTING;
    }

    // TODO: Add support for issuing the start for SPM counters.
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
    const auto&      cmdUtil    = m_device.CmdUtil();
    const auto&      regInfo    = cmdUtil.GetRegInfo();
    const EngineType engineType = pCmdStream->GetEngineType();

    // Set the perfmon state to 'stop counting' if we're freezing global counters, or to 'disable and reset' otherwise.
    const uint32 perfmonState = (reset ? CP_PERFMON_STATE_DISABLE_AND_RESET : CP_PERFMON_STATE_STOP_COUNTING);

    // Stop graphics state based counters.
    regCP_PERFMON_CNTL cpPerfmonCntl = {};
    if (HasGlobalCounters())
    {
        cpPerfmonCntl.bits.PERFMON_STATE = perfmonState;
    }

    // TODO: Add support for issuing the stop for SPM counters.
    pCmdSpace = pCmdStream->WriteSetOneConfigReg(regInfo.mmCpPerfmonCntl,
                                                 cpPerfmonCntl.u32All,
                                                 pCmdSpace);

    if (HasRlcCounters())
    {
        // Stop RLC counters: this needs to be done with a COPY_DATA command.
        regRLC_PERFMON_CNTL rlcPerfmonCntl = {};
        rlcPerfmonCntl.bits.PERFMON_STATE = perfmonState;

        if (engineType == EngineTypeCompute)
        {
            pCmdSpace += cmdUtil.BuildCopyDataCompute(dst_sel__mec_copy_data__mem_mapped_register,
                                                      regInfo.mmRlcPerfmonCntl,
                                                      src_sel__mec_copy_data__immediate_data,
                                                      rlcPerfmonCntl.u32All,
                                                      count_sel__mec_copy_data__32_bits_of_data,
                                                      wr_confirm__mec_copy_data__do_not_wait_for_confirmation,
                                                      pCmdSpace);
        }
        else
        {
            pCmdSpace += cmdUtil.BuildCopyDataGraphics(engine_sel__me_copy_data__micro_engine,
                                                       dst_sel__me_copy_data__mem_mapped_register,
                                                       regInfo.mmRlcPerfmonCntl,
                                                       src_sel__me_copy_data__immediate_data,
                                                       rlcPerfmonCntl.u32All,
                                                       count_sel__me_copy_data__32_bits_of_data,
                                                       wr_confirm__me_copy_data__do_not_wait_for_confirmation,
                                                       pCmdSpace);
        }
    }

    regGCEA_PERFCOUNTER_RSLT_CNTL  gceaPerfCntrResultCntl = {};
    gceaPerfCntrResultCntl.bits.ENABLE_ANY = 0; // halt all of the EA block perf counters.

    regMC_VM_L2_PERFCOUNTER_RSLT_CNTL  mcVmL2PerfCntrResultCntl = {};
    mcVmL2PerfCntrResultCntl.bits.ENABLE_ANY = 0; // halt all of the MC VM L2 block perf counters.

    regATC_PERFCOUNTER_RSLT_CNTL  atcPerfCntrResultCntl = {};
    atcPerfCntrResultCntl.bits.ENABLE_ANY    = 0; // halt all of the ATC block perf counters.

    regATC_L2_PERFCOUNTER_RSLT_CNTL  atcL2PerfCntrResultCntl = {};
    atcL2PerfCntrResultCntl.bits.ENABLE_ANY  = 0; // halt all of the ATC L2 block perf counters.

    regRPB_PERFCOUNTER_RSLT_CNTL  rpbPerfCntrResultCntl = {};
    rpbPerfCntrResultCntl.bits.ENABLE_ANY    = 0; // halt all of the RPB block perf counters.

    if (reset)
    {
        if (m_sqPerfCounterCtrl.u32All != 0)
        {
            pCmdSpace = pCmdStream->WriteSetOneConfigReg(regInfo.mmSqPerfCounterCtrl, 0, pCmdSpace);
        }

        // Setup the reset for the memory blocks.
        gceaPerfCntrResultCntl.bits.CLEAR_ALL   = 1;
        mcVmL2PerfCntrResultCntl.bits.CLEAR_ALL = 1;
        atcPerfCntrResultCntl.bits.CLEAR_ALL    = 1;
        atcL2PerfCntrResultCntl.bits.CLEAR_ALL  = 1;
        rpbPerfCntrResultCntl.bits.CLEAR_ALL    = 1;
    }

    if (HasEaCounters())
    {
        pCmdSpace = pCmdStream->WriteSetOnePrivilegedConfigReg(regInfo.mmEaPerfResultCntl,
                                                               gceaPerfCntrResultCntl.u32All,
                                                               pCmdSpace);
    }

    if (HasAtcCounters())
    {
        pCmdSpace = pCmdStream->WriteSetOnePrivilegedConfigReg(regInfo.mmAtcPerfResultCntl,
                                                               atcPerfCntrResultCntl.u32All,
                                                               pCmdSpace);
    }

    if (HasAtcL2Counters())
    {
        pCmdSpace = pCmdStream->WriteSetOnePrivilegedConfigReg(regInfo.mmAtcL2PerfResultCntl,
                                                               atcL2PerfCntrResultCntl.u32All,
                                                               pCmdSpace);
    }

    if (HasMcVmL2Counters())
    {
        pCmdSpace = pCmdStream->WriteSetOnePrivilegedConfigReg(regInfo.mmMcVmL2PerfResultCntl,
                                                               mcVmL2PerfCntrResultCntl.u32All,
                                                               pCmdSpace);
    }

    if (HasRpbCounters())
    {
        pCmdSpace = pCmdStream->WriteSetOnePrivilegedConfigReg(regInfo.mmRpbPerfResultCntl,
                                                               rpbPerfCntrResultCntl.u32All,
                                                               pCmdSpace);
    }

    if (HasRmiCounters())
    {
        static constexpr uint32 RmiEnSelOff = 2;

        regRMI_PERF_COUNTER_CNTL rmiPerfCounterCntl = {0};
        rmiPerfCounterCntl.bits.TRANS_BASED_PERF_EN_SEL = RmiEnSelOff;
        rmiPerfCounterCntl.bits.EVENT_BASED_PERF_EN_SEL = RmiEnSelOff;
        rmiPerfCounterCntl.bits.TC_PERF_EN_SEL          = RmiEnSelOff;
        pCmdSpace = pCmdStream->WriteSetOneConfigReg(mmRMI_PERF_COUNTER_CNTL,
                                                     rmiPerfCounterCntl.u32All,
                                                     pCmdSpace);
    }

    if (HasUmcchCounters())
    {
        // The number of UMC channels in the current device is equal to the number of SDP ports.
        const auto&  gfx9ChipProps    = m_device.Parent()->ChipProperties().gfx9;
        const uint32 numUmcChannels   = gfx9ChipProps.numSdpInterfaces;
        const auto&  umcPerfBlockInfo = gfx9ChipProps.perfCounterInfo.umcChannelBlocks;

        for (uint32 i = 0; i < numUmcChannels; i++)
        {
            if (Gfx9::PerfCounter::IsDstRegCopyDataPossible(umcPerfBlockInfo.regInfo[i].ctlClkRegAddr) == false)
            {
                // UMC channel perf counter address offsets for channels 3+ are not compatible with the current
                // COPY_DATA packet. Temporarily skip them. This implies that channels 3+ will not provide valid data.
                break;
            }

            if (engineType == EngineTypeCompute)
            {
                pCmdSpace += cmdUtil.BuildCopyDataCompute(dst_sel__mec_copy_data__perfcounters,
                                                          umcPerfBlockInfo.regInfo[i].ctlClkRegAddr,
                                                          src_sel__mec_copy_data__immediate_data,
                                                          0,
                                                          count_sel__mec_copy_data__32_bits_of_data,
                                                          wr_confirm__mec_copy_data__do_not_wait_for_confirmation,
                                                          pCmdSpace);
            }
            else
            {
                pCmdSpace += cmdUtil.BuildCopyDataGraphics(engine_sel__me_copy_data__micro_engine,
                                                           dst_sel__me_copy_data__perfcounters,
                                                           umcPerfBlockInfo.regInfo[i].ctlClkRegAddr,
                                                           src_sel__me_copy_data__immediate_data,
                                                           0,
                                                           count_sel__me_copy_data__32_bits_of_data,
                                                           wr_confirm__me_copy_data__do_not_wait_for_confirmation,
                                                           pCmdSpace);
            }
        }
    }

    if (engineType == EngineTypeCompute)
    {
        regCOMPUTE_PERFCOUNT_ENABLE computePerfCounterEn = {};
        computePerfCounterEn.bits.PERFCOUNT_ENABLE = 0;

        pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderCompute>(mmCOMPUTE_PERFCOUNT_ENABLE,
                                                                computePerfCounterEn.u32All,
                                                                pCmdSpace);
    }
    else
    {
        // Write the command sequence to stop event-based counters.
        pCmdSpace += cmdUtil.BuildNonSampleEventWrite(PERFCOUNTER_STOP, engineType, pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Samples performance counters by issuing commands into the specified command buffer which will instruct the HW to
// write the counter data to the specified virtual address. Returns the next unused DWORD in pCmdSpace.
uint32* PerfExperiment::WriteSamplePerfCounters(
    gpusize       baseGpuVirtAddr, ///< Base GPU virtual address to write counter data into
    CmdStream*    pCmdStream,
    uint32*       pCmdSpace
    ) const
{
    const auto&      cmdUtil    = m_device.CmdUtil();
    const auto&      regInfo    = cmdUtil.GetRegInfo();
    const EngineType engineType = pCmdStream->GetEngineType();

    // Write the command sequence to stop and sample event-based counters.
    pCmdSpace += cmdUtil.BuildNonSampleEventWrite(PERFCOUNTER_SAMPLE, engineType, pCmdSpace);
    pCmdSpace += cmdUtil.BuildNonSampleEventWrite(PERFCOUNTER_STOP,   engineType, pCmdSpace);
    pCmdSpace  = WriteComputePerfCountEnable(pCmdStream, pCmdSpace, true);

    // Freeze and sample graphics state based counters.
    regCP_PERFMON_CNTL cpPerfmonCntl = {};
    cpPerfmonCntl.bits.PERFMON_STATE         = CP_PERFMON_STATE_STOP_COUNTING;
    cpPerfmonCntl.bits.PERFMON_SAMPLE_ENABLE = 1;

    // TODO: Add support for issuing the sample for SPM counters.
    pCmdSpace = pCmdStream->WriteSetOneConfigReg(regInfo.mmCpPerfmonCntl,
                                                 cpPerfmonCntl.u32All,
                                                 pCmdSpace);

    if (HasRlcCounters())
    {
        // Freeze and sample RLC counters: this needs to be done with a COPY_DATA command.
        regRLC_PERFMON_CNTL rlcPerfmonCntl = {};
        rlcPerfmonCntl.bits.PERFMON_STATE         = CP_PERFMON_STATE_STOP_COUNTING;
        rlcPerfmonCntl.bits.PERFMON_SAMPLE_ENABLE = 1;

        if (engineType == EngineTypeCompute)
        {
            pCmdSpace += cmdUtil.BuildCopyDataCompute(dst_sel__mec_copy_data__mem_mapped_register,
                                                      regInfo.mmRlcPerfmonCntl,
                                                      src_sel__mec_copy_data__immediate_data,
                                                      rlcPerfmonCntl.u32All,
                                                      count_sel__mec_copy_data__32_bits_of_data,
                                                      wr_confirm__mec_copy_data__do_not_wait_for_confirmation,
                                                      pCmdSpace);
        }
        else
        {
            pCmdSpace += cmdUtil.BuildCopyDataGraphics(engine_sel__me_copy_data__micro_engine,
                                                       dst_sel__me_copy_data__mem_mapped_register,
                                                       regInfo.mmRlcPerfmonCntl,
                                                       src_sel__me_copy_data__immediate_data,
                                                       rlcPerfmonCntl.u32All,
                                                       count_sel__me_copy_data__32_bits_of_data,
                                                       wr_confirm__me_copy_data__do_not_wait_for_confirmation,
                                                       pCmdSpace);
        }
    }

    // Need to perform a wait-idle-clean before copying counter data registers.
    pCmdSpace = WriteWaitIdleClean(pCmdStream, true, engineType, pCmdSpace);

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

    return pCmdSpace;
}

// =====================================================================================================================
// Issues commands that either enable or disable (depending on the last parameter) the use of perf-counters with
// the compute engine.
uint32* PerfExperiment::WriteComputePerfCountEnable(
    CmdStream* pCmdStream,    ///< [in,out] Command stream to write PM4 commands into
    uint32*    pCmdSpace,
    bool       enable
    ) const
{
    regCOMPUTE_PERFCOUNT_ENABLE  computePerfCountEnable = {};
    computePerfCountEnable.bits.PERFCOUNT_ENABLE = (enable ? 1 : 0);

    return pCmdStream->WriteSetOnePrivilegedConfigReg(mmCOMPUTE_PERFCOUNT_ENABLE,
                                                      computePerfCountEnable.u32All,
                                                      pCmdSpace);
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
    grbmGfxIndex.gfx09.SH_BROADCAST_WRITES      = 1;
    grbmGfxIndex.bits.INSTANCE_BROADCAST_WRITES = 1;

    return pCmdStream->WriteSetOneConfigReg(m_device.CmdUtil().GetRegInfo().mmGrbmGfxIndex,
                                            grbmGfxIndex.u32All,
                                            pCmdSpace);
}

// =====================================================================================================================
// Helper method which writes commands to do a wait-idle-clean. Returns the next unused DWORD in pCmdSpace.
uint32* PerfExperiment::WriteWaitIdleClean(
    CmdStream*    pCmdStream,
    bool          cacheFlush,       ///< Indicates if we should also flush caches
    EngineType    engineType,
    uint32*       pCmdSpace
    ) const
{
    const CmdUtil& cmdUtil = m_device.CmdUtil();

    // NOTE: On gfx9+, we achieve a wait-idle-clean by issuing a CS_PARTIAL_FLUSH followed by an ACQUIRE_MEM with all
    //       base/action bits enabled to ensure outstanding reads and writes are complete.
    AcquireMemInfo acquireInfo = {};
    acquireInfo.engineType  = engineType;
    acquireInfo.tcCacheOp   = TcCacheOp::Nop;
    acquireInfo.baseAddress = FullSyncBaseAddr;
    acquireInfo.sizeBytes   = FullSyncSize;

    if ((engineType != EngineTypeCompute) && (engineType != EngineTypeExclusiveCompute))
    {
        acquireInfo.cpMeCoherCntl.u32All = CpMeCoherCntlStallMask;
    }

    if (cacheFlush)
    {
        acquireInfo.flags.invSqI$   = 1;
        acquireInfo.flags.invSqK$   = 1;
        acquireInfo.flags.flushSqK$ = 1;
        acquireInfo.tcCacheOp       = TcCacheOp::WbInvL1L2;
        if ((engineType != EngineTypeCompute) && (engineType != EngineTypeExclusiveCompute))
        {
            acquireInfo.flags.wbInvCbData = 1;
            acquireInfo.flags.wbInvDb     = 1;
        }
    }

    pCmdSpace += cmdUtil.BuildNonSampleEventWrite(CS_PARTIAL_FLUSH, engineType, pCmdSpace);

    pCmdSpace += cmdUtil.BuildAcquireMem(acquireInfo, pCmdSpace);

    // NOTE: ACQUIRE_MEM has an implicit context roll if the current context is busy. Since we won't be aware of a busy
    //       context, we must assume all ACQUIRE_MEM's come with a context roll.
    pCmdStream->SetContextRollDetected<false>();

    return pCmdSpace;
}

// =====================================================================================================================
// Writes initialization commands for UMC channel perf counters.
uint32* PerfExperiment::WriteSetupUmcchCntlRegs(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    const auto& chipProps = m_device.Parent()->ChipProperties();
    const auto& umcPerfBlockInfo = chipProps.gfx9.perfCounterInfo.umcChannelBlocks;

    // If Umcch counters have been enabled, simply enable all instances available here:
    const uint32 numUmcChannels = chipProps.gfx9.numSdpInterfaces;
    const auto& cmdUtil         = m_device.CmdUtil();

    regUMCCH0_PerfMonCtlClk umcCtlClkReg = { };
    umcCtlClkReg.bits.GlblResetMsk      = 0x3f;
    umcCtlClkReg.bits.GlblReset         = 1;

    for (uint32 i = 0; i < numUmcChannels; i++)
    {
        if (Gfx9::PerfCounter::IsDstRegCopyDataPossible(umcPerfBlockInfo.regInfo[i].ctlClkRegAddr) == false)
        {
            // UMC channel perf counter address offsets for channels 3+ are not compatible with the current
            // COPY_DATA packet. Temporarily skip them. This implies that channels 3+ will not provide valid data.
            break;
        }

        if (pCmdStream->GetEngineType() == EngineTypeCompute)
        {
            pCmdSpace += cmdUtil.BuildCopyDataCompute(dst_sel__mec_copy_data__perfcounters,
                                                      umcPerfBlockInfo.regInfo[i].ctlClkRegAddr,
                                                      src_sel__mec_copy_data__immediate_data,
                                                      umcCtlClkReg.u32All,
                                                      count_sel__mec_copy_data__32_bits_of_data,
                                                      wr_confirm__mec_copy_data__do_not_wait_for_confirmation,
                                                      pCmdSpace);
        }
        else
        {
            pCmdSpace += cmdUtil.BuildCopyDataGraphics(engine_sel__me_copy_data__micro_engine,
                                                       dst_sel__me_copy_data__perfcounters,
                                                       umcPerfBlockInfo.regInfo[i].ctlClkRegAddr,
                                                       src_sel__me_copy_data__immediate_data,
                                                       umcCtlClkReg.u32All,
                                                       count_sel__me_copy_data__32_bits_of_data,
                                                       wr_confirm__me_copy_data__do_not_wait_for_confirmation,
                                                       pCmdSpace);
        }
    }

    umcCtlClkReg.bits.GlblReset = 0;
    umcCtlClkReg.bits.GlblMonEn = 1;
    umcCtlClkReg.bits.CtrClkEn  = 1;

    for (uint32 i = 0; i < numUmcChannels; i++)
    {
        if (Gfx9::PerfCounter::IsDstRegCopyDataPossible(umcPerfBlockInfo.regInfo[i].ctlClkRegAddr) == false)
        {
            // UMC channel perf counter address offsets for channels 3+ are not compatible with the current
            // COPY_DATA packet. Temporarily skip them. This implies that channels 3+ will not provide valid data.
            break;
        }

        if (pCmdStream->GetEngineType() == EngineTypeCompute)
        {
            pCmdSpace += cmdUtil.BuildCopyDataCompute(dst_sel__mec_copy_data__perfcounters,
                                                      umcPerfBlockInfo.regInfo[i].ctlClkRegAddr,
                                                      src_sel__mec_copy_data__immediate_data,
                                                      umcCtlClkReg.u32All,
                                                      count_sel__mec_copy_data__32_bits_of_data,
                                                      wr_confirm__mec_copy_data__do_not_wait_for_confirmation,
                                                      pCmdSpace);
        }
        else
        {
            pCmdSpace += cmdUtil.BuildCopyDataGraphics(engine_sel__me_copy_data__micro_engine,
                                                       dst_sel__me_copy_data__perfcounters,
                                                       umcPerfBlockInfo.regInfo[i].ctlClkRegAddr,
                                                       src_sel__me_copy_data__immediate_data,
                                                       umcCtlClkReg.u32All,
                                                       count_sel__me_copy_data__32_bits_of_data,
                                                       wr_confirm__me_copy_data__do_not_wait_for_confirmation,
                                                       pCmdSpace);
        }
    }

    return pCmdSpace;
}

} // gfx9
} // Pal
