/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/gfx9/gfx9BorderColorPalette.h"
#include "core/hw/gfxip/gfx9/gfx9CmdUtil.h"
#include "core/hw/gfxip/gfx9/gfx9ComputeCmdBuffer.h"
#include "core/hw/gfxip/gfx9/gfx9ComputePipeline.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9IndirectCmdGenerator.h"
#include "core/hw/gfxip/gfx9/gfx9PerfExperiment.h"
#include "core/hw/gfxip/queryPool.h"
#include "core/imported/hsa/AMDHSAKernelDescriptor.h"
#include "core/imported/hsa/amd_hsa_kernel_code.h"
#include "core/cmdAllocator.h"
#include "g_platformSettings.h"
#include "core/settingsLoader.h"
#include "palHsaAbiMetadata.h"
#include "palInlineFuncs.h"
#include "palVectorImpl.h"

using namespace Util;

namespace Pal
{
namespace Gfx9
{

// =====================================================================================================================
ComputeCmdBuffer::ComputeCmdBuffer(
    const Device&              device,
    const CmdBufferCreateInfo& createInfo)
    :
    Pm4::ComputeCmdBuffer(device, createInfo, &m_cmdStream),
    m_device(device),
    m_cmdUtil(device.CmdUtil()),
    m_issueSqttMarkerEvent(device.Parent()->IssueSqttMarkerEvents()),
    m_cmdStream(device,
                createInfo.pCmdAllocator,
                EngineTypeCompute,
                SubEngineType::Primary,
                CmdStreamUsage::Workload,
                IsNested()),
    m_pSignatureCs(&NullCsSignature),
    m_baseUserDataRegCs(device.GetBaseUserDataReg(HwShaderStage::Cs)),
#if PAL_BUILD_GFX11
    m_supportsShPairsPacketCs(device.Settings().gfx11EnableShRegPairOptimizationCs),
    m_validUserEntryRegPairsCs{},
    m_numValidUserEntriesCs(0),
#endif
    m_predGpuAddr(0),
    m_inheritedPredication(false),
    m_globalInternalTableAddr(0)
{
    // Compute command buffers suppors compute ops and CP DMA.
    m_engineSupport = CmdBufferEngineSupport::Compute | CmdBufferEngineSupport::CpDma;

#if PAL_BUILD_GFX11
    memset(&m_validUserEntryRegPairsLookupCs[0], InvalidRegPairLookupIndex, sizeof(m_validUserEntryRegPairsLookupCs));
#endif

    // Assume PAL ABI compute pipelines by default.
    SetDispatchFunctions(false);
}

// =====================================================================================================================
// Initializes Gfx9-specific functionality.
Result ComputeCmdBuffer::Init(
    const CmdBufferInternalCreateInfo& internalInfo)
{
    Result result = Pm4::ComputeCmdBuffer::Init(internalInfo);

    if (result == Result::Success)
    {
        result = m_cmdStream.Init();
    }

    return result;
}

// =====================================================================================================================
void ComputeCmdBuffer::ResetState()
{
    Pm4::ComputeCmdBuffer::ResetState();

    // Assume PAL ABI compute pipelines by default.
    SetDispatchFunctions(false);

    m_pSignatureCs = &NullCsSignature;

#if PAL_BUILD_GFX11
    // All user data entries are invalid upon a state reset.
    memset(&m_validUserEntryRegPairsLookupCs[0], InvalidRegPairLookupIndex, sizeof(m_validUserEntryRegPairsLookupCs));
    m_numValidUserEntriesCs = 0;
#endif

    // Command buffers start without a valid predicate GPU address.
    m_predGpuAddr = 0;
    m_inheritedPredication = false;
    m_globalInternalTableAddr = 0;
}

// =====================================================================================================================
Result ComputeCmdBuffer::Begin(
    const CmdBufferBuildInfo& info)
{
    const Result result = Pm4::ComputeCmdBuffer::Begin(info);

    if ((result == Result::Success) &&
        (info.pInheritedState != nullptr) && info.pInheritedState->stateFlags.predication)
    {
        m_inheritedPredication = true;

        // Allocate the SET_PREDICATION emulation/COND_EXEC memory to be populated by the root-level command buffer.
        uint32 *pPredCpuAddr = CmdAllocateEmbeddedData(1, 1, &m_predGpuAddr);

        // Initialize the COND_EXEC command memory to non-zero, i.e. always execute
        *pPredCpuAddr = 1;
    }

    return result;
}

// =====================================================================================================================
// Binds a graphics or compute pipeline to this command buffer.
void ComputeCmdBuffer::CmdBindPipeline(
    const PipelineBindParams& params)
{
    auto* const pNewPipeline = static_cast<const ComputePipeline*>(params.pPipeline);
    auto* const pOldPipeline = static_cast<const ComputePipeline*>(m_computeState.pipelineState.pPipeline);

    const bool newUsesHsaAbi = (pNewPipeline != nullptr) && (pNewPipeline->GetInfo().flags.hsaAbi == 1u);
    const bool oldUsesHsaAbi = (pOldPipeline != nullptr) && (pOldPipeline->GetInfo().flags.hsaAbi == 1u);

    if (oldUsesHsaAbi != newUsesHsaAbi)
    {
        // The HSA abi can clobber USER_DATA_0, which holds the global internal table address for PAL ABI, so we must
        // save the address to memory before switching to an HSA ABI or restore it when switching back to PAL ABI.
        if (newUsesHsaAbi && (m_globalInternalTableAddr == 0))
        {
            m_globalInternalTableAddr = AllocateGpuScratchMem(1, 1);
            m_device.RsrcProcMgr().EchoGlobalInternalTableAddr(this, m_globalInternalTableAddr);
        }
        else if (newUsesHsaAbi == false)
        {
            uint32* pCmdSpace = m_cmdStream.ReserveCommands();
            pCmdSpace += m_cmdUtil.BuildLoadShRegsIndex(index__pfp_load_sh_reg_index__direct_addr,
                                                        data_format__pfp_load_sh_reg_index__offset_and_size,
                                                        m_globalInternalTableAddr,
                                                        mmCOMPUTE_USER_DATA_0,
                                                        1,
                                                        Pm4ShaderType::ShaderCompute,
                                                        pCmdSpace);
            m_cmdStream.CommitCommands(pCmdSpace);
        }

        SetDispatchFunctions(newUsesHsaAbi);
    }

    Pal::Pm4CmdBuffer::CmdBindPipeline(params);
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdBarrier(
    const BarrierInfo& barrierInfo)
{
    CmdBuffer::CmdBarrier(barrierInfo);

    // Barriers do not honor predication.
    const uint32 packetPredicate = m_pm4CmdBufState.flags.packetPredicate;
    m_pm4CmdBufState.flags.packetPredicate = 0;

    bool splitMemAllocated;
    BarrierInfo splitBarrierInfo = barrierInfo;
    Result result = Pal::Device::SplitBarrierTransitions(m_device.GetPlatform(), &splitBarrierInfo, &splitMemAllocated);

    if (result == Result::ErrorOutOfMemory)
    {
        NotifyAllocFailure();
    }
    else if (result == Result::Success)
    {
        m_device.Barrier(this, &m_cmdStream, splitBarrierInfo);
    }
    else
    {
        PAL_ASSERT_ALWAYS();
    }

    // Delete memory allocated for splitting the BarrierTransitions if necessary.
    if (splitMemAllocated)
    {
        PAL_SAFE_DELETE_ARRAY(splitBarrierInfo.pTransitions, m_device.GetPlatform());
    }

    m_pm4CmdBufState.flags.packetPredicate = packetPredicate;
}

// =====================================================================================================================
void ComputeCmdBuffer::OptimizePipeAndCacheMaskForRelease(
    uint32* pStageMask,
    uint32* pAccessMask
    ) const
{
    Pm4CmdBuffer::OptimizePipeAndCacheMaskForRelease(pStageMask, pAccessMask);

    // Mark off all graphics path specific stages and caches if command buffer doesn't support graphics.
    if (pStageMask != nullptr)
    {
        *pStageMask  &= ~PipelineStagesGraphicsOnly;
    }

    if (pAccessMask != nullptr)
    {
        *pAccessMask &= ~CacheCoherencyGraphicsOnly;
    }
}

// =====================================================================================================================
uint32 ComputeCmdBuffer::CmdRelease(
    const AcquireReleaseInfo& releaseInfo)
{
    CmdBuffer::CmdRelease(releaseInfo);

    // Barriers do not honor predication.
    const uint32 packetPredicate = m_pm4CmdBufState.flags.packetPredicate;
    m_pm4CmdBufState.flags.packetPredicate = 0;

    // Mark these as traditional barriers in RGP
    m_device.DescribeBarrierStart(this, releaseInfo.reason, Developer::BarrierType::Release);

    bool splitMemAllocated;
    AcquireReleaseInfo splitReleaseInfo = releaseInfo;
    Result result = Pal::Device::SplitImgBarriers(m_device.GetPlatform(), &splitReleaseInfo, &splitMemAllocated);

    Developer::BarrierOperations barrierOps = {};
    AcqRelSyncToken syncToken = {};

    if (result == Result::ErrorOutOfMemory)
    {
        NotifyAllocFailure();
    }
    else if (result == Result::Success)
    {
        syncToken = m_device.Release(this, &m_cmdStream, releaseInfo, &barrierOps);
    }
    else
    {
        PAL_ASSERT_ALWAYS();
    }

    // Delete memory allocated for splitting ImgBarriers if necessary.
    if (splitMemAllocated)
    {
        PAL_SAFE_DELETE_ARRAY(splitReleaseInfo.pImageBarriers, m_device.GetPlatform());
    }

    m_device.DescribeBarrierEnd(this, &barrierOps);

    m_pm4CmdBufState.flags.packetPredicate = packetPredicate;

    return syncToken.u32All;
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdAcquire(
    const AcquireReleaseInfo& acquireInfo,
    uint32                    syncTokenCount,
    const uint32*             pSyncTokens)
{
    CmdBuffer::CmdAcquire(acquireInfo, syncTokenCount, pSyncTokens);

    // Barriers do not honor predication.
    const uint32 packetPredicate = m_pm4CmdBufState.flags.packetPredicate;
    m_pm4CmdBufState.flags.packetPredicate = 0;

    // Mark these as traditional barriers in RGP
    m_device.DescribeBarrierStart(this, acquireInfo.reason, Developer::BarrierType::Acquire);

    bool splitMemAllocated;
    AcquireReleaseInfo splitAcquireInfo = acquireInfo;
    Result result = Pal::Device::SplitImgBarriers(m_device.GetPlatform(), &splitAcquireInfo, &splitMemAllocated);

    Developer::BarrierOperations barrierOps = {};
    if (result == Result::ErrorOutOfMemory)
    {
        NotifyAllocFailure();
    }
    else if (result == Result::Success)
    {
        m_device.Acquire(this,
                         &m_cmdStream,
                         splitAcquireInfo,
                         syncTokenCount,
                         reinterpret_cast<const AcqRelSyncToken*>(pSyncTokens),
                         &barrierOps);
    }
    else
    {
        PAL_ASSERT_ALWAYS();
    }

    // Delete memory allocated for splitting ImgBarriers if necessary.
    if (splitMemAllocated)
    {
        PAL_SAFE_DELETE_ARRAY(splitAcquireInfo.pImageBarriers, m_device.GetPlatform());
    }

    m_device.DescribeBarrierEnd(this, &barrierOps);

    m_pm4CmdBufState.flags.packetPredicate = packetPredicate;
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdReleaseEvent(
    const AcquireReleaseInfo& releaseInfo,
    const IGpuEvent*          pGpuEvent)
{
    CmdBuffer::CmdReleaseEvent(releaseInfo, pGpuEvent);

    // Barriers do not honor predication.
    const uint32 packetPredicate           = m_pm4CmdBufState.flags.packetPredicate;
    m_pm4CmdBufState.flags.packetPredicate = 0;

    // Mark these as traditional barriers in RGP
    m_device.DescribeBarrierStart(this, releaseInfo.reason, Developer::BarrierType::Release);

    bool splitMemAllocated;
    AcquireReleaseInfo splitReleaseInfo = releaseInfo;
    Result result = Pal::Device::SplitImgBarriers(m_device.GetPlatform(), &splitReleaseInfo, &splitMemAllocated);

    Developer::BarrierOperations barrierOps = {};
    if (result == Result::ErrorOutOfMemory)
    {
        NotifyAllocFailure();
    }
    else if (result == Result::Success)
    {
        m_device.ReleaseEvent(this, &m_cmdStream, splitReleaseInfo, pGpuEvent, &barrierOps);
    }
    else
    {
        PAL_ASSERT_ALWAYS();
    }

    // Delete memory allocated for splitting ImgBarriers if necessary.
    if (splitMemAllocated)
    {
        PAL_SAFE_DELETE_ARRAY(splitReleaseInfo.pImageBarriers, m_device.GetPlatform());
    }

    m_device.DescribeBarrierEnd(this, &barrierOps);

    m_pm4CmdBufState.flags.packetPredicate = packetPredicate;
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdAcquireEvent(
    const AcquireReleaseInfo& acquireInfo,
    uint32                    gpuEventCount,
    const IGpuEvent* const*   ppGpuEvents)
{
    CmdBuffer::CmdAcquireEvent(acquireInfo, gpuEventCount, ppGpuEvents);

    // Barriers do not honor predication.
    const uint32 packetPredicate           = m_pm4CmdBufState.flags.packetPredicate;
    m_pm4CmdBufState.flags.packetPredicate = 0;

    // Mark these as traditional barriers in RGP
    m_device.DescribeBarrierStart(this, acquireInfo.reason, Developer::BarrierType::Acquire);

    bool splitMemAllocated;
    AcquireReleaseInfo splitAcquireInfo = acquireInfo;
    Result result = Pal::Device::SplitImgBarriers(m_device.GetPlatform(), &splitAcquireInfo, &splitMemAllocated);

    Developer::BarrierOperations barrierOps = {};
    if (result == Result::ErrorOutOfMemory)
    {
        NotifyAllocFailure();
    }
    else if (result == Result::Success)
    {
        m_device.AcquireEvent(this, &m_cmdStream, splitAcquireInfo, gpuEventCount, ppGpuEvents, &barrierOps);
    }
    else
    {
        PAL_ASSERT_ALWAYS();
    }

    // Delete memory allocated for splitting ImgBarriers if necessary.
    if (splitMemAllocated)
    {
        PAL_SAFE_DELETE_ARRAY(splitAcquireInfo.pImageBarriers, m_device.GetPlatform());
    }

    m_device.DescribeBarrierEnd(this, &barrierOps);

    m_pm4CmdBufState.flags.packetPredicate = packetPredicate;
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdReleaseThenAcquire(
    const AcquireReleaseInfo& barrierInfo)
{
    CmdBuffer::CmdReleaseThenAcquire(barrierInfo);

    // Barriers do not honor predication.
    const uint32 packetPredicate = m_pm4CmdBufState.flags.packetPredicate;
    m_pm4CmdBufState.flags.packetPredicate = 0;

    // Mark these as traditional barriers in RGP
    m_device.DescribeBarrierStart(this, barrierInfo.reason, Developer::BarrierType::Full);

    bool splitMemAllocated;
    AcquireReleaseInfo splitBarrierInfo = barrierInfo;
    Result result = Pal::Device::SplitImgBarriers(m_device.GetPlatform(), &splitBarrierInfo, &splitMemAllocated);

    Developer::BarrierOperations barrierOps = {};
    if (result == Result::ErrorOutOfMemory)
    {
        NotifyAllocFailure();
    }
    else if (result == Result::Success)
    {
        m_device.ReleaseThenAcquire(this, &m_cmdStream, splitBarrierInfo, &barrierOps);
    }
    else
    {
        PAL_ASSERT_ALWAYS();
    }

    // Delete memory allocated for splitting ImgBarriers if necessary.
    if (splitMemAllocated)
    {
        PAL_SAFE_DELETE_ARRAY(splitBarrierInfo.pImageBarriers, m_device.GetPlatform());
    }

    m_device.DescribeBarrierEnd(this, &barrierOps);

    m_pm4CmdBufState.flags.packetPredicate = packetPredicate;
}

// =====================================================================================================================
// Sets-up function pointers for the Dispatch entrypoint and all variants using template parameters.
template <bool HsaAbi, bool IssueSqttMarkerEvent>
void ComputeCmdBuffer::SetDispatchFunctions()
{
    m_funcTable.pfnCmdDispatch       = CmdDispatch<HsaAbi, IssueSqttMarkerEvent>;
    m_funcTable.pfnCmdDispatchOffset = CmdDispatchOffset<HsaAbi, IssueSqttMarkerEvent>;

    if (HsaAbi)
    {
        // Note that CmdDispatchIndirect and CmdDispatchDynamic do not support the HSA ABI.
        m_funcTable.pfnCmdDispatchIndirect = nullptr;
        m_funcTable.pfnCmdDispatchDynamic  = nullptr;
    }
    else
    {
        m_funcTable.pfnCmdDispatchIndirect = CmdDispatchIndirect<IssueSqttMarkerEvent>;
        m_funcTable.pfnCmdDispatchDynamic  = CmdDispatchDynamic<IssueSqttMarkerEvent>;
    }

}

// =====================================================================================================================
// Sets-up function pointers for the Dispatch entrypoint and all variants.
void ComputeCmdBuffer::SetDispatchFunctions(
    bool hsaAbi)
{
    if (hsaAbi)
    {
        if (m_issueSqttMarkerEvent)
        {
            SetDispatchFunctions<true, true>();
        }
        else
        {
            SetDispatchFunctions<true, false>();
        }
    }
    else
    {
        if (m_issueSqttMarkerEvent)
        {
            SetDispatchFunctions<false, true>();
        }
        else
        {
            SetDispatchFunctions<false, false>();
        }
    }
}

// =====================================================================================================================
// Issues a direct dispatch command. X, Y, and Z are in numbers of thread groups. We must discard the dispatch if x, y,
// or z are zero. To avoid branching, we will rely on the HW to discard the dispatch for us.
template <bool HsaAbi, bool IssueSqttMarkerEvent>
void PAL_STDCALL ComputeCmdBuffer::CmdDispatch(
    ICmdBuffer*  pCmdBuffer,
    DispatchDims size)
{
    auto* pThis = static_cast<ComputeCmdBuffer*>(pCmdBuffer);

    if (IssueSqttMarkerEvent)
    {
        pThis->DescribeDispatch(Developer::DrawDispatchType::CmdDispatch, size);
    }

    uint32* pCmdSpace = pThis->m_cmdStream.ReserveCommands();

    if (HsaAbi)
    {
        pCmdSpace = pThis->ValidateDispatchHsaAbi({}, size, pCmdSpace);
    }
    else
    {
        pCmdSpace = pThis->ValidateDispatchPalAbi(0uLL, 0uLL, size, pCmdSpace);
    }

    if (pThis->m_pm4CmdBufState.flags.packetPredicate != 0)
    {
        pCmdSpace += pThis->m_cmdUtil.BuildCondExec(pThis->m_predGpuAddr, CmdUtil::DispatchDirectSize, pCmdSpace);
    }

    pCmdSpace += pThis->m_cmdUtil.BuildDispatchDirect<false, true>(size,
                                                                   PredDisable,
                                                                   pThis->m_pSignatureCs->flags.isWave32,
                                                                   pThis->UsesDispatchTunneling(),
                                                                   pThis->DisablePartialPreempt(),
                                                                   pCmdSpace);

    if (IssueSqttMarkerEvent)
    {
        pCmdSpace += pThis->m_cmdUtil.BuildNonSampleEventWrite(THREAD_TRACE_MARKER, EngineTypeCompute, pCmdSpace);
    }

    pThis->m_cmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// Issues an indirect dispatch command. We must discard the dispatch if x, y, or z are zero. We will rely on the HW to
// discard the dispatch for us.
template <bool IssueSqttMarkerEvent>
void PAL_STDCALL ComputeCmdBuffer::CmdDispatchIndirect(
    ICmdBuffer*       pCmdBuffer,
    const IGpuMemory& gpuMemory,
    gpusize           offset)
{
    auto* pThis = static_cast<ComputeCmdBuffer*>(pCmdBuffer);

    if (IssueSqttMarkerEvent)
    {
        pThis->DescribeDispatchIndirect();
    }

    PAL_ASSERT(IsPow2Aligned(offset, sizeof(uint32)));
    PAL_ASSERT(offset + sizeof(DispatchIndirectArgs) <= gpuMemory.Desc().size);

    uint32* pCmdSpace = pThis->m_cmdStream.ReserveCommands();

    const gpusize gpuVirtAddr = (gpuMemory.Desc().gpuVirtAddr + offset);

    pCmdSpace = pThis->ValidateDispatchPalAbi(gpuVirtAddr, 0uLL, {}, pCmdSpace);

    if (pThis->m_pm4CmdBufState.flags.packetPredicate != 0)
    {
        pCmdSpace += pThis->m_cmdUtil.BuildCondExec(pThis->m_predGpuAddr, CmdUtil::DispatchIndirectMecSize, pCmdSpace);
    }

    pCmdSpace += pThis->m_cmdUtil.BuildDispatchIndirectMec(gpuVirtAddr,
                                                           pThis->m_pSignatureCs->flags.isWave32,
                                                           pThis->UsesDispatchTunneling(),
                                                           pThis->DisablePartialPreempt(),
                                                           pCmdSpace);

    if (IssueSqttMarkerEvent)
    {
        pCmdSpace += pThis->m_cmdUtil.BuildNonSampleEventWrite(THREAD_TRACE_MARKER, EngineTypeCompute, pCmdSpace);
    }

    pThis->m_cmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// Issues an direct dispatch command with immediate threadgroup offsets. We must discard the dispatch if x, y, or z are
// zero. To avoid branching, we will rely on the HW to discard the dispatch for us.
template <bool HsaAbi, bool IssueSqttMarkerEvent>
void PAL_STDCALL ComputeCmdBuffer::CmdDispatchOffset(
    ICmdBuffer*  pCmdBuffer,
    DispatchDims offset,
    DispatchDims launchSize,
    DispatchDims logicalSize)
{
    auto* pThis = static_cast<ComputeCmdBuffer*>(pCmdBuffer);

    if (IssueSqttMarkerEvent)
    {
        pThis->DescribeDispatchOffset(offset, launchSize, logicalSize);
    }

    uint32* pCmdSpace = pThis->m_cmdStream.ReserveCommands();

    if (HsaAbi)
    {
        pCmdSpace = pThis->ValidateDispatchHsaAbi(offset, logicalSize, pCmdSpace);
    }
    else
    {
        pCmdSpace = pThis->ValidateDispatchPalAbi(0uLL, 0uLL, logicalSize, pCmdSpace);
    }

    pCmdSpace = pThis->m_cmdStream.WriteSetSeqShRegs(mmCOMPUTE_START_X,
                                                     mmCOMPUTE_START_Z,
                                                     ShaderCompute,
                                                     &offset,
                                                     pCmdSpace);

    if (pThis->m_pm4CmdBufState.flags.packetPredicate != 0)
    {
        pCmdSpace += pThis->m_cmdUtil.BuildCondExec(pThis->m_predGpuAddr, CmdUtil::DispatchDirectSize, pCmdSpace);
    }

    // The DIM_X/Y/Z in DISPATCH_DIRECT packet are used to program COMPUTE_DIM_X/Y/Z registers, which are actually the
    // end block positions instead of execution block dimensions. So we need to use the dimensions plus offsets.
    pCmdSpace += pThis->m_cmdUtil.BuildDispatchDirect<false, false>(offset + launchSize,
                                                                    PredDisable,
                                                                    pThis->m_pSignatureCs->flags.isWave32,
                                                                    pThis->UsesDispatchTunneling(),
                                                                    pThis->DisablePartialPreempt(),
                                                                    pCmdSpace);

    if (IssueSqttMarkerEvent)
    {
        pCmdSpace += pThis->m_cmdUtil.BuildNonSampleEventWrite(THREAD_TRACE_MARKER, EngineTypeCompute, pCmdSpace);
    }

    pThis->m_cmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
template <bool IssueSqttMarkerEvent>
void PAL_STDCALL ComputeCmdBuffer::CmdDispatchDynamic(
    ICmdBuffer*  pCmdBuffer,
    gpusize      gpuVa,
    DispatchDims size)
{
    auto* pThis = static_cast<ComputeCmdBuffer*>(pCmdBuffer);

    PAL_ASSERT(pThis->m_cmdUtil.HasEnhancedLoadShRegIndex());

    if (IssueSqttMarkerEvent)
    {
        pThis->DescribeDispatch(Developer::DrawDispatchType::CmdDispatchDynamic, size);
    }

    uint32* pCmdSpace = pThis->m_cmdStream.ReserveCommands();

    pCmdSpace = pThis->ValidateDispatchPalAbi(0uLL, gpuVa, size, pCmdSpace);

    if (pThis->m_pm4CmdBufState.flags.packetPredicate != 0)
    {
        pCmdSpace += pThis->m_cmdUtil.BuildCondExec(pThis->m_predGpuAddr, CmdUtil::DispatchDirectSize, pCmdSpace);
    }

    pCmdSpace += pThis->m_cmdUtil.BuildDispatchDirect<false, true>(size,
                                                                   PredDisable,
                                                                   pThis->m_pSignatureCs->flags.isWave32,
                                                                   pThis->UsesDispatchTunneling(),
                                                                   pThis->DisablePartialPreempt(),
                                                                   pCmdSpace);

    if (IssueSqttMarkerEvent)
    {
        pCmdSpace += pThis->m_cmdUtil.BuildNonSampleEventWrite(THREAD_TRACE_MARKER, EngineTypeCompute, pCmdSpace);
    }

    pThis->m_cmdStream.CommitCommands(pCmdSpace);
}

static_assert(((Gfx09::COMPUTE_STATIC_THREAD_MGMT_SE0__SH0_CU_EN_MASK      ==
                Gfx10Plus::COMPUTE_STATIC_THREAD_MGMT_SE0__SA0_CU_EN_MASK) &&
               (Gfx09::COMPUTE_STATIC_THREAD_MGMT_SE0__SH1_CU_EN_MASK      ==
                Gfx10Plus::COMPUTE_STATIC_THREAD_MGMT_SE0__SA1_CU_EN_MASK) &&
               (Gfx09::COMPUTE_STATIC_THREAD_MGMT_SE1__SH0_CU_EN_MASK      ==
                Gfx10Plus::COMPUTE_STATIC_THREAD_MGMT_SE1__SA0_CU_EN_MASK) &&
               (Gfx09::COMPUTE_STATIC_THREAD_MGMT_SE1__SH1_CU_EN_MASK      ==
                Gfx10Plus::COMPUTE_STATIC_THREAD_MGMT_SE1__SA1_CU_EN_MASK) &&
               (Gfx09::COMPUTE_STATIC_THREAD_MGMT_SE2__SH0_CU_EN_MASK      ==
                Gfx10Plus::COMPUTE_STATIC_THREAD_MGMT_SE2__SA0_CU_EN_MASK) &&
               (Gfx09::COMPUTE_STATIC_THREAD_MGMT_SE2__SH1_CU_EN_MASK      ==
                Gfx10Plus::COMPUTE_STATIC_THREAD_MGMT_SE2__SA1_CU_EN_MASK) &&
               (Gfx09::COMPUTE_STATIC_THREAD_MGMT_SE3__SH0_CU_EN_MASK      ==
                Gfx10Plus::COMPUTE_STATIC_THREAD_MGMT_SE3__SA0_CU_EN_MASK) &&
               (Gfx09::COMPUTE_STATIC_THREAD_MGMT_SE3__SH1_CU_EN_MASK      ==
                Gfx10Plus::COMPUTE_STATIC_THREAD_MGMT_SE3__SA1_CU_EN_MASK)),
               "COMPUTE_STATIC_THREAD_MGMT regs have changed between GFX9 and 10!");

// =====================================================================================================================
void ComputeCmdBuffer::CmdCopyMemory(
    const IGpuMemory&       srcGpuMemory,
    const IGpuMemory&       dstGpuMemory,
    uint32                  regionCount,
    const MemoryCopyRegion* pRegions)
{
    m_device.RsrcProcMgr().CmdCopyMemory(this,
                                         static_cast<const GpuMemory&>(srcGpuMemory),
                                         static_cast<const GpuMemory&>(dstGpuMemory),
                                         regionCount,
                                         pRegions);
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdUpdateMemory(
    const IGpuMemory& dstGpuMemory,
    gpusize           dstOffset,
    gpusize           dataSize,
    const uint32*     pData)
{
    PAL_ASSERT(pData != nullptr);
    m_device.RsrcProcMgr().CmdUpdateMemory(this,
                                           static_cast<const GpuMemory&>(dstGpuMemory),
                                           dstOffset,
                                           dataSize,
                                           pData);
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdUpdateBusAddressableMemoryMarker(
    const IGpuMemory& dstGpuMemory,
    gpusize           offset,
    uint32            value)
{
    const GpuMemory* pGpuMemory = static_cast<const GpuMemory*>(&dstGpuMemory);
    WriteDataInfo    writeData  = {};

    writeData.engineType = GetEngineType();
    writeData.dstAddr    = pGpuMemory->GetBusAddrMarkerVa() + offset;
    writeData.dstSel     = dst_sel__mec_write_data__memory;

    uint32* pCmdSpace = m_cmdStream.ReserveCommands();
    pCmdSpace += m_cmdUtil.BuildWriteData(writeData, value, pCmdSpace);
    m_cmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// Use the GPU's command processor to execute an atomic memory operation
void ComputeCmdBuffer::CmdMemoryAtomic(
    const IGpuMemory& dstGpuMemory,
    gpusize           dstOffset,
    uint64            srcData,
    AtomicOp          atomicOp)
{
    uint32* pCmdSpace = m_cmdStream.ReserveCommands();
    pCmdSpace += m_cmdUtil.BuildAtomicMem(atomicOp, dstGpuMemory.Desc().gpuVirtAddr + dstOffset, srcData, pCmdSpace);
    m_cmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// Issues an end-of-pipe timestamp event or immediately copies the current time. Writes the results to the
// pMemObject + destOffset.
void ComputeCmdBuffer::CmdWriteTimestamp(
    HwPipePoint       pipePoint,
    const IGpuMemory& dstGpuMemory,
    gpusize           dstOffset)
{
    const gpusize address   = dstGpuMemory.Desc().gpuVirtAddr + dstOffset;
    uint32*       pCmdSpace = m_cmdStream.ReserveCommands();

    if (pipePoint <= HwPipePreCs)
    {
        pCmdSpace += m_cmdUtil.BuildCopyData(EngineTypeCompute,
                                             0,
                                             dst_sel__mec_copy_data__memory__GFX09,
                                             address,
                                             src_sel__mec_copy_data__gpu_clock_count,
                                             0,
                                             count_sel__mec_copy_data__64_bits_of_data,
                                             wr_confirm__mec_copy_data__wait_for_confirmation,
                                             pCmdSpace);
    }
    else
    {
        PAL_ASSERT(pipePoint == HwPipeBottom);

        ReleaseMemGeneric releaseInfo = {};
        releaseInfo.engineType = EngineTypeCompute;
        releaseInfo.dstAddr    = address;
        releaseInfo.dataSel    = data_sel__mec_release_mem__send_gpu_clock_counter;

        pCmdSpace += m_cmdUtil.BuildReleaseMemGeneric(releaseInfo, pCmdSpace);
    }

    m_cmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// Writes an immediate value either during top-of-pipe or bottom-of-pipe event.
void ComputeCmdBuffer::CmdWriteImmediate(
    HwPipePoint        pipePoint,
    uint64             data,
    ImmediateDataWidth dataSize,
    gpusize            address)
{
    const bool is32Bit = (dataSize == ImmediateDataWidth::ImmediateData32Bit);

    uint32* pCmdSpace = m_cmdStream.ReserveCommands();

    if (pipePoint <= HwPipePreCs)
    {
        pCmdSpace += m_cmdUtil.BuildCopyData(EngineTypeCompute,
                                             0,
                                             dst_sel__mec_copy_data__memory__GFX09,
                                             address,
                                             src_sel__mec_copy_data__immediate_data,
                                             data,
                                             is32Bit ? count_sel__mec_copy_data__32_bits_of_data
                                                     : count_sel__mec_copy_data__64_bits_of_data,
                                             wr_confirm__mec_copy_data__wait_for_confirmation,
                                             pCmdSpace);
    }
    else
    {
        PAL_ASSERT(pipePoint == HwPipeBottom);

        ReleaseMemGeneric releaseInfo = {};
        releaseInfo.engineType = EngineTypeCompute;
        releaseInfo.dstAddr    = address;
        releaseInfo.data       = data;
        releaseInfo.dataSel    = is32Bit ? data_sel__mec_release_mem__send_32_bit_low
                                         : data_sel__mec_release_mem__send_64_bit_data;

        pCmdSpace += m_cmdUtil.BuildReleaseMemGeneric(releaseInfo, pCmdSpace);
    }

    m_cmdStream.CommitCommands(pCmdSpace);
}
// =====================================================================================================================
void ComputeCmdBuffer::CmdBindBorderColorPalette(
    PipelineBindPoint          pipelineBindPoint,
    const IBorderColorPalette* pPalette)
{
    // NOTE: The hardware fundamentally does not support multiple border color palettes for compute as the register
    //       which controls the address of the palette is a config register. We need to support this for our clients,
    //       but it should not be considered a correct implementation. As a result we may see arbitrary hangs that
    //       do not reproduce easily. This setting (disableBorderColorPaletteBinds) should be set to TRUE in the event
    //       that one of these hangs is suspected. At that point we will need to come up with a more robust solution
    //       which may involve getting KMD support.
    if (m_device.Settings().disableBorderColorPaletteBinds == false)
    {
        const auto*const pNewPalette = static_cast<const BorderColorPalette*>(pPalette);

        {
            PAL_ASSERT(pipelineBindPoint == PipelineBindPoint::Compute);
            if (pNewPalette != nullptr)
            {
                uint32* pCmdSpace = m_cmdStream.ReserveCommands();
                pCmdSpace = pNewPalette->WriteCommands(pipelineBindPoint,
                                                       TimestampGpuVirtAddr(),
                                                       &m_cmdStream,
                                                       pCmdSpace);
                m_cmdStream.CommitCommands(pCmdSpace);
            }

            m_computeState.pipelineState.pBorderColorPalette = pNewPalette;
            m_computeState.pipelineState.dirtyFlags.borderColorPalette = 1;
        }
    }
}

// =====================================================================================================================
// Helper function to write a single user-sgpr. This function should always be preferred for user data writes over
// WriteSetOneShReg() if the SGPR is written before or during draw/dispatch validation.
#if PAL_BUILD_GFX11
// On GFX11, this function will add the register offset and value into the relevant array of packed register pairs to be
// written in WritePackedUserDataEntriesToSgprs().
#endif
// Returns the next unused DWORD in pDeCmdSpace.
uint32* ComputeCmdBuffer::SetUserSgprReg(
    uint16  regAddr,
    uint32  regValue,
    uint32* pCmdSpace)
{
    pCmdSpace = SetSeqUserSgprRegs(regAddr,
                                   regAddr,
                                   &regValue,
                                   pCmdSpace);

    return pCmdSpace;
}

// =====================================================================================================================
// Helper function to write a sequence of user-sgprs. This function should always be preferred for user data writes over
// WriteSetSeqShRegs() if the SGPRs are written before or during draw/dispatch validation.
#if PAL_BUILD_GFX11
// On GFX11, this function will add the offsets/values into the relevant array of packed register pairs to be written
// in WritePackedUserDataEntriesToSgprs().
#endif
// Returns the next unused DWORD in pCmdSpace.
uint32* ComputeCmdBuffer::SetSeqUserSgprRegs(
    uint16      startAddr,
    uint16      endAddr,
    const void* pValues,
    uint32*     pCmdSpace)
{
    // This function is exclusively meant for writing user-SGPR regs. Use the regular WriteSetSeqShRegs/OneShReg() for
    // non user-SGPR SH reg writes.
    PAL_ASSERT(InRange<uint16>(startAddr, m_baseUserDataRegCs, m_baseUserDataRegCs + NumUserDataRegistersCompute));

#if PAL_BUILD_GFX11
    if (m_supportsShPairsPacketCs)
    {
        uint16 baseUserDataReg = m_baseUserDataRegCs;
        SetSeqUserDataEntryPairPackedValues(startAddr,
                                            endAddr,
                                            baseUserDataReg,
                                            pValues,
                                            m_validUserEntryRegPairsCs,
                                            &m_validUserEntryRegPairsLookupCs[0],
                                            &m_numValidUserEntriesCs);
    }
    else
#endif
    {
        pCmdSpace = m_cmdStream.WriteSetSeqShRegs(startAddr, endAddr, ShaderCompute, pValues, pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Helper function which is responsible for making sure all user-data entries are written to either the spill table or
// to user-SGPR's, as well as making sure that all indirect user-data tables are up-to-date in GPU memory. Part of
// Dispatch-time validation.
template <bool HasPipelineChanged>
uint32* ComputeCmdBuffer::ValidateUserData(
    const ComputePipelineSignature* pPrevSignature, // In: Signature of pipeline bound for previous Dispatch. Will be
                                                    // nullptr if the pipeline is not changing.
    UserDataEntries*                pUserData,
    UserDataTableState*             pSpillTable,
    uint32*                         pCmdSpace)
{
    PAL_ASSERT((HasPipelineChanged  && (pPrevSignature != nullptr)) ||
               (!HasPipelineChanged && (pPrevSignature == nullptr)));

    // Step #1:
    // Write all dirty user-data entries to their mapped user SGPR's and check if the spill table needs updating.
    // If the pipeline has changed we must also fixup the dirty bits because the prior compute pipeline could use
    // fewer fast sgprs than the current pipeline.

    bool alreadyWritten = false;
    if (HasPipelineChanged)
    {
        alreadyWritten = FixupUserSgprsOnPipelineSwitch(*pUserData, pPrevSignature, &pCmdSpace);
    }

    if (alreadyWritten == false)
    {
#if PAL_BUILD_GFX11
        if (m_supportsShPairsPacketCs)
        {
            CmdStream::AccumulateUserDataEntriesForSgprs<false>(m_pSignatureCs->stage,
                                                                *pUserData,
                                                                m_baseUserDataRegCs,
                                                                m_validUserEntryRegPairsCs,
                                                                &m_validUserEntryRegPairsLookupCs[0],
                                                                &m_numValidUserEntriesCs);
        }
        else
#endif
        {
            pCmdSpace = m_cmdStream.WriteUserDataEntriesToSgprs<false, ShaderCompute>(m_pSignatureCs->stage,
                                                                                      *pUserData,
                                                                                      pCmdSpace);
        }
    }

    const uint16 spillThreshold = m_pSignatureCs->spillThreshold;
    if (spillThreshold != NoUserDataSpilling)
    {
        const uint16 userDataLimit = m_pSignatureCs->userDataLimit;
        PAL_ASSERT(userDataLimit != 0);
        const uint16 lastUserData = (userDataLimit - 1);

        // Step #2:
        // Because the spill table is managed using CPU writes to embedded data, it must be fully re-uploaded for any
        // Dispatch whenever *any* contents have changed.
        bool reUpload = (pSpillTable->dirty != 0);
        if (HasPipelineChanged &&
            ((spillThreshold < pPrevSignature->spillThreshold) || (userDataLimit > pPrevSignature->userDataLimit)))
        {
            // If the pipeline is changing and the spilled region is expanding, we need to re-upload the table because
            // we normally only update the portions useable by the bound pipeline to minimize memory usage.
            reUpload = true;
        }
        else
        {
            // Otherwise, use the following loop to check if any of the spilled user-data entries are dirty.
            const uint32 firstMaskId = (spillThreshold / UserDataEntriesPerMask);
            const uint32 lastMaskId = (lastUserData / UserDataEntriesPerMask);
            for (uint32 maskId = firstMaskId; maskId <= lastMaskId; ++maskId)
            {
                size_t dirtyMask = pUserData->dirty[maskId];
                if (maskId == firstMaskId)
                {
                    // Ignore the dirty bits for any entries below the spill threshold.
                    const uint32 firstEntryInMask = (spillThreshold & (UserDataEntriesPerMask - 1));
                    dirtyMask &= ~BitfieldGenMask(size_t(firstEntryInMask));
                }
                if (maskId == lastMaskId)
                {
                    // Ignore the dirty bits for any entries beyond the user-data limit.
                    const uint32 lastEntryInMask = (lastUserData & (UserDataEntriesPerMask - 1));
                    dirtyMask &= BitfieldGenMask(size_t(lastEntryInMask + 1));
                }

                if (dirtyMask != 0)
                {
                    reUpload = true;
                    break; // We only care if *any* spill table contents change!
                }
            } // for each wide-bitfield sub-mask
        }

        // Step #3:
        // Re-upload spill table contents if necessary, and write the new GPU virtual address to the user-SGPR(s).
        if (reUpload)
        {
            UpdateUserDataTableCpu(pSpillTable,
                                   (userDataLimit - spillThreshold),
                                   spillThreshold,
                                   &pUserData->entries[0]);
        }

        if (reUpload || HasPipelineChanged)
        {
            if (m_pSignatureCs->stage.spillTableRegAddr != UserDataNotMapped)
            {
                pCmdSpace = SetUserSgprReg(m_pSignatureCs->stage.spillTableRegAddr,
                                           LowPart(pSpillTable->gpuVirtAddr),
                                           pCmdSpace);
            }
        }
    } // if current pipeline spills user-data

    // All dirtied user-data entries have been written to user-SGPR's or to the spill table somewhere in this method,
    // so it is safe to clear these bits.
    memset(&pUserData->dirty[0], 0, sizeof(UserDataEntries::dirty));

    return pCmdSpace;
}

// =====================================================================================================================
// Performs PAL ABI dispatch-time validation.
uint32* ComputeCmdBuffer::ValidateDispatchPalAbi(
    gpusize      indirectGpuVirtAddr,
    gpusize      launchDescGpuVirtAddr,
    DispatchDims logicalSize,
    uint32*      pCmdSpace)
{
#if PAL_DEVELOPER_BUILD
    const bool enablePm4Instrumentation = m_device.GetPlatform()->PlatformSettings().pm4InstrumentorEnabled;

    uint32* pStartingCmdSpace = pCmdSpace;
    uint32  pipelineCmdLen    = 0;
    uint32  userDataCmdLen    = 0;
#endif

    const bool supportDynamicDispatch = m_computeState.pipelineState.pPipeline->SupportDynamicDispatch();
    PAL_ASSERT(((supportDynamicDispatch == true)  && (launchDescGpuVirtAddr != 0)) ||
               ((supportDynamicDispatch == false) && (launchDescGpuVirtAddr == 0)));

    if (m_computeState.pipelineState.dirtyFlags.pipeline)
    {
        const auto*const pNewPipeline = static_cast<const ComputePipeline*>(m_computeState.pipelineState.pPipeline);

        pCmdSpace = pNewPipeline->WriteCommands(&m_cmdStream,
                                                pCmdSpace,
                                                m_computeState.dynamicCsInfo,
                                                launchDescGpuVirtAddr,
                                                m_buildFlags.prefetchShaders);

#if PAL_DEVELOPER_BUILD
        if (enablePm4Instrumentation)
        {
            pipelineCmdLen    = (static_cast<uint32>(pCmdSpace - pStartingCmdSpace) * sizeof(uint32));
            pStartingCmdSpace = pCmdSpace;
        }
#endif

        const auto*const pPrevSignature = m_pSignatureCs;
        m_pSignatureCs                  = &pNewPipeline->Signature();

        pCmdSpace = ValidateUserData<true>(pPrevSignature,
                                           &m_computeState.csUserDataEntries,
                                           &m_spillTable.stateCs,
                                           pCmdSpace);
    }
    else
    {
        if ((launchDescGpuVirtAddr != 0) && (launchDescGpuVirtAddr != m_computeState.dynamicLaunchGpuVa))
        {
            const auto* const pPipeline = static_cast<const ComputePipeline*>(m_computeState.pipelineState.pPipeline);
            pCmdSpace = pPipeline->WriteLaunchDescriptor(&m_cmdStream,
                                                         pCmdSpace,
                                                         m_computeState.dynamicCsInfo,
                                                         launchDescGpuVirtAddr);

#if PAL_DEVELOPER_BUILD
            if (enablePm4Instrumentation)
            {
                pipelineCmdLen    = (static_cast<uint32>(pCmdSpace - pStartingCmdSpace) * sizeof(uint32));
                pStartingCmdSpace = pCmdSpace;
            }
#endif
        } // if Launch Descriptor is changing

        pCmdSpace = ValidateUserData<false>(nullptr,
                                            &m_computeState.csUserDataEntries,
                                            &m_spillTable.stateCs,
                                            pCmdSpace);
    }

#if PAL_DEVELOPER_BUILD
    if (enablePm4Instrumentation)
    {
        userDataCmdLen    = (static_cast<uint32>(pCmdSpace - pStartingCmdSpace) * sizeof(uint32));
        pStartingCmdSpace = pCmdSpace;
    }
#endif

    m_computeState.pipelineState.dirtyFlags.u32All = 0;
    m_computeState.dynamicLaunchGpuVa = launchDescGpuVirtAddr;

    if (m_pSignatureCs->numWorkGroupsRegAddr != UserDataNotMapped)
    {
        // Indirect Dispatches by definition have the number of thread-groups to launch stored in GPU memory at the
        // specified address.  However, for direct Dispatches, we must allocate some embedded memory to store this
        // information.
        if (indirectGpuVirtAddr == 0uLL) // This is a direct Dispatch.
        {
            *reinterpret_cast<DispatchDims*>(CmdAllocateEmbeddedData(3, 4, &indirectGpuVirtAddr)) = logicalSize;
        }

        pCmdSpace = SetSeqUserSgprRegs(m_pSignatureCs->numWorkGroupsRegAddr,
                                       (m_pSignatureCs->numWorkGroupsRegAddr + 1),
                                       &indirectGpuVirtAddr,
                                       pCmdSpace);
    }

#if PAL_BUILD_GFX11
    if (m_numValidUserEntriesCs > 0)
    {
        pCmdSpace = WritePackedUserDataEntriesToSgprs(pCmdSpace);
    }
#endif

#if PAL_DEVELOPER_BUILD
    if (enablePm4Instrumentation)
    {
        const uint32 miscCmdLen = (static_cast<uint32>(pCmdSpace - pStartingCmdSpace) * sizeof(uint32));
        m_device.DescribeDrawDispatchValidation(this, userDataCmdLen, pipelineCmdLen, miscCmdLen);
    }
#endif

    return pCmdSpace;
}

// =====================================================================================================================
// Performs PAL ABI dispatch-time validation.
uint32* ComputeCmdBuffer::ValidateDispatchHsaAbi(
    DispatchDims offset,
    DispatchDims logicalSize,
    uint32*      pCmdSpace)
{
#if PAL_DEVELOPER_BUILD
    const bool enablePm4Instrumentation = m_device.GetPlatform()->PlatformSettings().pm4InstrumentorEnabled;

    uint32* pStartingCmdSpace = pCmdSpace;
    uint32  pipelineCmdLen    = 0;
    uint32  userDataCmdLen    = 0;
#endif

    const auto*const pPipeline = static_cast<const ComputePipeline*>(m_computeState.pipelineState.pPipeline);

    // We didn't implement support for dynamic dispatch.
    PAL_ASSERT(pPipeline->SupportDynamicDispatch() == false);

    if (m_computeState.pipelineState.dirtyFlags.pipeline)
    {
        pCmdSpace = pPipeline->WriteCommands(&m_cmdStream,
                                             pCmdSpace,
                                             m_computeState.dynamicCsInfo,
                                             0,
                                             m_buildFlags.prefetchShaders);

        m_pSignatureCs = &pPipeline->Signature();
    }

#if PAL_DEVELOPER_BUILD
    if (enablePm4Instrumentation)
    {
        pipelineCmdLen    = (static_cast<uint32>(pCmdSpace - pStartingCmdSpace) * sizeof(uint32));
        pStartingCmdSpace = pCmdSpace;
    }
#endif

    // PAL thinks in terms of threadgroups but the HSA ABI thinks in terms of global threads, we need to convert.
    const DispatchDims threads = pPipeline->ThreadsPerGroupXyz();

    offset      *= threads;
    logicalSize *= threads;

    // Now we write the required SGPRs. These depend on per-dispatch state so we don't have dirty bit tracking.
    const HsaAbi::CodeObjectMetadata&        metadata = pPipeline->HsaMetadata();
    const llvm::amdhsa::kernel_descriptor_t& desc     = pPipeline->KernelDescriptor();

    uint32 startReg = mmCOMPUTE_USER_DATA_0;

    // Many HSA ELFs request private segment buffer registers, but never actually use them. Space is reserved to
    // adhere to initialization order but will be unset as we do not support scratch space in this execution path.
    if (TestAnyFlagSet(desc.kernel_code_properties, AMD_KERNEL_CODE_PROPERTIES_ENABLE_SGPR_PRIVATE_SEGMENT_BUFFER))
    {
        startReg += 4;
    }

    if (TestAnyFlagSet(desc.kernel_code_properties, AMD_KERNEL_CODE_PROPERTIES_ENABLE_SGPR_DISPATCH_PTR))
    {
        // Fake an AQL dispatch packet for the shader to read.
        gpusize aqlPacketGpu  = 0;
        auto*const pAqlPacket = reinterpret_cast<hsa_kernel_dispatch_packet_t*>(
                                CmdAllocateEmbeddedData(sizeof(hsa_kernel_dispatch_packet_t) / sizeof(uint32),
                                                        1,
                                                        &aqlPacketGpu));

        // Zero everything out then fill in certain fields the shader is likely to read.
        memset(pAqlPacket, 0, sizeof(sizeof(hsa_kernel_dispatch_packet_t)));

        pAqlPacket->workgroup_size_x     = static_cast<uint16>(threads.x);
        pAqlPacket->workgroup_size_y     = static_cast<uint16>(threads.y);
        pAqlPacket->workgroup_size_z     = static_cast<uint16>(threads.z);
        pAqlPacket->grid_size_x          = logicalSize.x;
        pAqlPacket->grid_size_y          = logicalSize.y;
        pAqlPacket->grid_size_z          = logicalSize.z;
        pAqlPacket->private_segment_size = metadata.PrivateSegmentFixedSize();
        pAqlPacket->group_segment_size   = ((m_computeState.dynamicCsInfo.ldsBytesPerTg > 0)
                                                ? m_computeState.dynamicCsInfo.ldsBytesPerTg
                                                : metadata.GroupSegmentFixedSize());

        pCmdSpace = SetSeqUserSgprRegs(startReg, (startReg + 1), &aqlPacketGpu, pCmdSpace);
        startReg += 2;
    }

    if (TestAnyFlagSet(desc.kernel_code_properties, AMD_KERNEL_CODE_PROPERTIES_ENABLE_SGPR_KERNARG_SEGMENT_PTR))
    {
        // Copy the kernel argument buffer into GPU memory.
        gpusize      gpuVa      = 0;
        const uint32 allocSize  = NumBytesToNumDwords(metadata.KernargSegmentSize());
        const uint32 allocAlign = NumBytesToNumDwords(metadata.KernargSegmentAlign());
        uint8*const  pParams    = reinterpret_cast<uint8*>(CmdAllocateEmbeddedData(allocSize, allocAlign, &gpuVa));

        memcpy(pParams, m_computeState.pKernelArguments, metadata.KernargSegmentSize());

        // The global offsets are always zero, except in CmdDispatchOffset where they are dispatch-time values.
        // This could be moved out into CmdDispatchOffset if the overhead is too much but we'd have to return
        // out some extra state to make that work.
        for (uint32 idx = 0; idx < metadata.NumArguments(); ++idx)
        {
            const HsaAbi::KernelArgument& arg = metadata.Arguments()[idx];

            if (arg.valueKind == HsaAbi::ValueKind::HiddenGlobalOffsetX)
            {
                memcpy(pParams + arg.offset, &offset.x, Min<size_t>(sizeof(offset.x), arg.size));
            }
            else if (arg.valueKind == HsaAbi::ValueKind::HiddenGlobalOffsetY)
            {
                memcpy(pParams + arg.offset, &offset.y, Min<size_t>(sizeof(offset.y), arg.size));
            }
            else if (arg.valueKind == HsaAbi::ValueKind::HiddenGlobalOffsetZ)
            {
                memcpy(pParams + arg.offset, &offset.z, Min<size_t>(sizeof(offset.z), arg.size));
            }
        }

        pCmdSpace = SetSeqUserSgprRegs(startReg, (startReg + 1), &gpuVa, pCmdSpace);
        startReg += 2;
    }

#if PAL_ENABLE_PRINTS_ASSERTS
    regCOMPUTE_PGM_RSRC2 computePgmRsrc2 = {};
    computePgmRsrc2.u32All = desc.compute_pgm_rsrc2;

    PAL_ASSERT((startReg - mmCOMPUTE_USER_DATA_0) == computePgmRsrc2.bitfields.USER_SGPR);
#endif

#if PAL_BUILD_GFX11
    if (m_numValidUserEntriesCs > 0)
    {
        pCmdSpace = WritePackedUserDataEntriesToSgprs(pCmdSpace);
    }
#endif

#if PAL_DEVELOPER_BUILD
    if (enablePm4Instrumentation)
    {
        userDataCmdLen    = (static_cast<uint32>(pCmdSpace - pStartingCmdSpace) * sizeof(uint32));
        pStartingCmdSpace = pCmdSpace;
    }
#endif

    m_computeState.pipelineState.dirtyFlags.u32All = 0;
    m_computeState.dynamicLaunchGpuVa = 0;

    // We don't expect HSA ABI pipelines to use these.
    PAL_ASSERT(m_pSignatureCs->numWorkGroupsRegAddr == UserDataNotMapped);

#if PAL_DEVELOPER_BUILD
    if (enablePm4Instrumentation)
    {
        const uint32 miscCmdLen = (static_cast<uint32>(pCmdSpace - pStartingCmdSpace) * sizeof(uint32));
        m_device.DescribeDrawDispatchValidation(this, userDataCmdLen, pipelineCmdLen, miscCmdLen);
    }
#endif

    return pCmdSpace;
}

// =====================================================================================================================
// Helper function responsible for handling user-SGPR updates during Dispatch-time validation when the active pipeline
// has changed since the previous Dispatch operation. It is expected that this will be called only when the pipeline
// is changing and immediately before a call to WriteUserDataEntriesToSgprs<false, ..>().
bool ComputeCmdBuffer::FixupUserSgprsOnPipelineSwitch(
    const UserDataEntries&          userData,
    const ComputePipelineSignature* pPrevSignature,
    uint32**                        ppCmdSpace)
{
    PAL_ASSERT(pPrevSignature != nullptr);

    // The WriteUserDataEntriesToSgprs() method only writes entries which are mapped to user-SGPR's and have
    // been marked dirty.  When the active pipeline is changing, the set of entries mapped to user-SGPR's can change
    // and which entries are mapped to which registers can also change. The simplest way to handle this is to write
    // all mapped user-SGPR's whose mappings are changing.  If mappings are not changing it will be handled through
    // the normal "pipeline not changing" path.

    bool written = false;
    uint32* pCmdSpace = (*ppCmdSpace);

    if (m_pSignatureCs->userDataHash != pPrevSignature->userDataHash)
    {
#if PAL_BUILD_GFX11
        if (m_supportsShPairsPacketCs && (m_numValidUserEntriesCs > 0))
        {
            // Even though we ignore dirty flags here, we still need to accumulate user data entries into packed
            // register pairs for each draw/dispatch when the active pipeline has changed and there are pending register
            // writes (so we only need to write a single packed packet for user entries). If there are no pending writes
            // in the valid user entry packed register pair array, it is more performant to write the user data entries
            // into SGPRs via the non-packed SET_SH_REG packet as we can guarantee SGPRs are contiguous when
            // IgnoreDirtyFlags = true.
            CmdStream::AccumulateUserDataEntriesForSgprs<true>(m_pSignatureCs->stage,
                                                               userData,
                                                               m_baseUserDataRegCs,
                                                               m_validUserEntryRegPairsCs,
                                                               &m_validUserEntryRegPairsLookupCs[0],
                                                               &m_numValidUserEntriesCs);
        }
        else
#endif
        {
            pCmdSpace = m_cmdStream.WriteUserDataEntriesToSgprs<true, ShaderCompute>(m_pSignatureCs->stage,
                                                                                     userData,
                                                                                     pCmdSpace);
        }

        written = true;
        (*ppCmdSpace) = pCmdSpace;
    }

    return written;
}

#if PAL_BUILD_GFX11
// =====================================================================================================================
// Helper function to validate and write packed user data entries to SGPRs. Returns next unused DWORD in command space.
template <bool Pm4OptImmediate>
uint32* ComputeCmdBuffer::WritePackedUserDataEntriesToSgprs(
    uint32* pCmdSpace)
{
    PAL_ASSERT(m_numValidUserEntriesCs <= NumUserDataRegistersCompute);

    pCmdSpace = m_cmdStream.WriteSetShRegPairs<ShaderCompute, Pm4OptImmediate>(m_validUserEntryRegPairsCs,
                                                                               m_numValidUserEntriesCs,
                                                                               pCmdSpace);

    // All entries are invalid once written to the command stream.
    memset(&m_validUserEntryRegPairsLookupCs[0], InvalidRegPairLookupIndex, sizeof(m_validUserEntryRegPairsLookupCs));
    m_numValidUserEntriesCs = 0;

#if PAL_ENABLE_PRINTS_ASSERTS
    memset(&m_validUserEntryRegPairsCs[0], 0, sizeof(m_validUserEntryRegPairsCs));
#endif

    return pCmdSpace;
}

// =====================================================================================================================
// Wrapper for the real WritePackedUserDataEntriesToSgprs() for when the caller doesn't know if the immediate mode
// pm4 optimizer is enabled.
uint32* ComputeCmdBuffer::WritePackedUserDataEntriesToSgprs(
    uint32* pCmdSpace)
{
    if (m_cmdStream.Pm4OptimizerEnabled())
    {
        pCmdSpace = WritePackedUserDataEntriesToSgprs<true>(pCmdSpace);
    }
    else
    {
        pCmdSpace = WritePackedUserDataEntriesToSgprs<false>(pCmdSpace);
    }

    return pCmdSpace;
}
#endif

// =====================================================================================================================
// Adds PM4 commands needed to write any registers associated with starting a query.
void ComputeCmdBuffer::AddQuery(
    QueryPoolType     queryPoolType,
    QueryControlFlags flags)
{
    // Compute command buffers only support pipeline stat queries.
    PAL_ASSERT(queryPoolType == QueryPoolType::PipelineStats);

    // PIPELINE_START event may not have been issued in the preamble, so do this for safety.
    if (IsFirstQuery(queryPoolType))
    {
        // This adds the required active query.
    }
}

// =====================================================================================================================
// Adds PM4 commands needed to write any registers associated with ending the last active query in this command buffer.
void ComputeCmdBuffer::RemoveQuery(
    QueryPoolType queryPoolType)
{
    // Compute command buffers only support pipeline stat queries.
    PAL_ASSERT(queryPoolType == QueryPoolType::PipelineStats);

    // Acknowledge this PIPELINE_STOP event.
    if (IsLastActiveQuery(queryPoolType))
    {
        // This will remove the active query as required.
    }
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdBeginQuery(
    const IQueryPool& queryPool,
    QueryType         queryType,
    uint32            slot,
    QueryControlFlags flags)
{
    static_cast<const QueryPool&>(queryPool).Begin(this, &m_cmdStream, nullptr, queryType, slot, flags);
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdEndQuery(
    const IQueryPool& queryPool,
    QueryType         queryType,
    uint32            slot)
{
    static_cast<const QueryPool&>(queryPool).End(this, &m_cmdStream, nullptr, queryType, slot);
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdResetQueryPool(
    const IQueryPool& queryPool,
    uint32            startQuery,
    uint32            queryCount)
{
    static_cast<const QueryPool&>(queryPool).Reset(this, &m_cmdStream, startQuery, queryCount);
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdIf(
    const IGpuMemory& gpuMemory,
    gpusize           offset,
    uint64            data,
    uint64            mask,
    CompareFunc       compareFunc)
{
    // Nested command buffers don't support control flow yet.
    PAL_ASSERT(IsNested() == false);

    m_cmdStream.If(compareFunc, gpuMemory.Desc().gpuVirtAddr + offset, data, mask);
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdElse()
{
    // Nested command buffers don't support control flow yet.
    PAL_ASSERT(IsNested() == false);

    m_cmdStream.Else();
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdEndIf()
{
    // Nested command buffers don't support control flow yet.
    PAL_ASSERT(IsNested() == false);

    m_cmdStream.EndIf();
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdWhile(
    const IGpuMemory& gpuMemory,
    gpusize           offset,
    uint64            data,
    uint64            mask,
    CompareFunc       compareFunc)
{
    // Nested command buffers don't support control flow yet.
    PAL_ASSERT(IsNested() == false);

    m_cmdStream.While(compareFunc, gpuMemory.Desc().gpuVirtAddr + offset, data, mask);
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdEndWhile()
{
    // Nested command buffers don't support control flow yet.
    PAL_ASSERT(IsNested() == false);

    m_cmdStream.EndWhile();
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdCopyRegisterToMemory(
    uint32            srcRegisterOffset,
    const IGpuMemory& dstGpuMemory,
    gpusize           dstOffset)
{
    uint32* pCmdSpace = m_cmdStream.ReserveCommands();

    DmaDataInfo dmaData = {};
    dmaData.dstSel       = dst_sel__pfp_dma_data__dst_addr_using_das;
    dmaData.dstAddr      = dstGpuMemory.Desc().gpuVirtAddr + dstOffset;
    dmaData.dstAddrSpace = das__pfp_dma_data__memory;
    dmaData.srcSel       = src_sel__pfp_dma_data__src_addr_using_sas;
    dmaData.srcAddr      = srcRegisterOffset;
    dmaData.srcAddrSpace = sas__pfp_dma_data__register;
    dmaData.sync         = true;
    dmaData.usePfp       = false;
    pCmdSpace += m_cmdUtil.BuildDmaData<false>(dmaData, pCmdSpace);

    m_cmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdWaitRegisterValue(
    uint32      registerOffset,
    uint32      data,
    uint32      mask,
    CompareFunc compareFunc)
{
    uint32* pCmdSpace = m_cmdStream.ReserveCommands();

    pCmdSpace += m_cmdUtil.BuildWaitRegMem(EngineTypeCompute,
                                           mem_space__me_wait_reg_mem__register_space,
                                           CmdUtil::WaitRegMemFunc(compareFunc),
                                           engine_sel__me_wait_reg_mem__micro_engine,
                                           registerOffset,
                                           data,
                                           mask,
                                           pCmdSpace);

    m_cmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdWaitMemoryValue(
    const IGpuMemory& gpuMemory,
    gpusize           offset,
    uint32            data,
    uint32            mask,
    CompareFunc       compareFunc)
{
    uint32* pCmdSpace = m_cmdStream.ReserveCommands();

    pCmdSpace += m_cmdUtil.BuildWaitRegMem(EngineTypeCompute,
                                           mem_space__me_wait_reg_mem__memory_space,
                                           CmdUtil::WaitRegMemFunc(compareFunc),
                                           engine_sel__me_wait_reg_mem__micro_engine,
                                           gpuMemory.Desc().gpuVirtAddr + offset,
                                           data,
                                           mask,
                                           pCmdSpace);

    m_cmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdWaitBusAddressableMemoryMarker(
    const IGpuMemory& gpuMemory,
    uint32            data,
    uint32            mask,
    CompareFunc       compareFunc)
{
    const GpuMemory* pGpuMemory = static_cast<const GpuMemory*>(&gpuMemory);

    uint32* pCmdSpace = m_cmdStream.ReserveCommands();

    pCmdSpace += m_cmdUtil.BuildWaitRegMem(EngineTypeCompute,
                                           mem_space__me_wait_reg_mem__memory_space,
                                           CmdUtil::WaitRegMemFunc(compareFunc),
                                           engine_sel__me_wait_reg_mem__micro_engine,
                                           pGpuMemory->GetBusAddrMarkerVa(),
                                           data,
                                           mask,
                                           pCmdSpace);

    m_cmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdCommentString(
    const char* pComment)
{
    uint32* pCmdSpace = m_cmdStream.ReserveCommands();

    pCmdSpace += m_cmdUtil.BuildCommentString(pComment, ShaderCompute, pCmdSpace);

    m_cmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdNop(
    const void* pPayload,
    uint32      payloadSize)
{
    uint32* pCmdSpace = m_cmdStream.ReserveCommands();
    pCmdSpace += m_cmdUtil.BuildNopPayload(pPayload, payloadSize, pCmdSpace);
    m_cmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// Helper method for handling the state "leakage" from a nested command buffer back to its caller. Since the callee has
// tracked its own state during the building phase, we can access the final state of the command buffer since its stored
// in the UniversalCmdBuffer object itself.
void ComputeCmdBuffer::LeakNestedCmdBufferState(
    const ComputeCmdBuffer& callee)
{
    Pm4::ComputeCmdBuffer::LeakNestedCmdBufferState(callee);

    // Invalidate PM4 optimizer state on post-execute since the current command buffer state does not reflect
    // state changes from the nested command buffer. We will need to resolve the nested PM4 state onto the
    // current command buffer for this to work correctly.
    m_cmdStream.NotifyNestedCmdBufferExecute();
}

// =====================================================================================================================
// Adds a preamble to the start of a new command buffer.
// SEE: ComputePreamblePm4Img and CommonPreamblePm4Img structures in gfx9Preambles.h for what is written in the preamble
Result ComputeCmdBuffer::WritePreambleCommands(
    const CmdUtil& cmdUtil,
    CmdStream*     pCmdStream)
{
    // If this trips, it means that this isn't really the preamble -- i.e., somebody has inserted something into the
    // command stream before the preamble.  :-(
    PAL_ASSERT(pCmdStream->IsEmpty());

    uint32* pCmdSpace = pCmdStream->ReserveCommands();
    pCmdSpace += cmdUtil.BuildNonSampleEventWrite(PIPELINESTAT_START, EngineTypeCompute, pCmdSpace);
    pCmdStream->CommitCommands(pCmdSpace);

    return Result::Success;
}

// =====================================================================================================================
// Adds a preamble to the start of a new command buffer.
Result ComputeCmdBuffer::AddPreamble()
{
    Result result = ComputeCmdBuffer::WritePreambleCommands(m_cmdUtil, &m_cmdStream);

    // Initialize acquire/release fence value GPU chunk.
    if (AcqRelFenceValBaseGpuVa() != 0)
    {
        const uint32 data[static_cast<uint32>(AcqRelEventType::Count)] = {};

        WriteDataInfo writeDataInfo = { };
        writeDataInfo.engineType = m_engineType;
        writeDataInfo.dstSel     = dst_sel__mec_write_data__memory;
        writeDataInfo.dstAddr    = AcqRelFenceValBaseGpuVa();

        uint32* pCmdSpace = m_cmdStream.ReserveCommands();
        pCmdSpace += CmdUtil::BuildWriteData(writeDataInfo,
                                             (sizeof(data) / sizeof(uint32)),
                                             reinterpret_cast<const uint32*>(&data),
                                             pCmdSpace);
        m_cmdStream.CommitCommands(pCmdSpace);
    }

    return result;
}

// =====================================================================================================================
// Adds a postamble to the end of a new command buffer.
Result ComputeCmdBuffer::WritePostambleCommands(
    const CmdUtil&     cmdUtil,
    Pm4CmdBuffer*const pCmdBuffer,
    CmdStream*         pCmdStream)
{
    uint32* pCmdSpace = pCmdStream->ReserveCommands();

    if (pCmdBuffer->GetPm4CmdBufState().flags.cpBltActive)
    {
        // Stalls the CP MEC until the CP's DMA engine has finished all previous "CP blts" (DMA_DATA commands
        // without the sync bit set). The ring won't wait for CP DMAs to finish so we need to do this manually.
        pCmdSpace += cmdUtil.BuildWaitDmaData(pCmdSpace);
        pCmdBuffer->SetPm4CmdBufCpBltState(false);
    }

    // The following ATOMIC_MEM packet increments the done-count for the command stream, so that we can probe when the
    // command buffer has completed execution on the GPU.
    // NOTE: Normally, we would need to flush the L2 cache to guarantee that this memory operation makes it out to
    // memory. However, since we're at the end of the command buffer, we can rely on the fact that the KMD inserts
    // an EOP event which flushes and invalidates the caches in between command buffers.
    if (pCmdStream->GetFirstChunk()->BusyTrackerGpuAddr() != 0)
    {
        // We also need a wait-for-idle before the atomic increment because command memory might be read or written
        // by dispatches. If we don't wait for idle then the driver might reset and write over that memory before the
        // shaders are done executing.
        pCmdSpace  = pCmdBuffer->WriteWaitCsIdle(pCmdSpace);
        pCmdSpace += cmdUtil.BuildAtomicMem(AtomicOp::AddInt32,
                                            pCmdStream->GetFirstChunk()->BusyTrackerGpuAddr(),
                                            1,
                                            pCmdSpace);
    }

    pCmdStream->CommitCommands(pCmdSpace);

    return Result::Success;
}

// =====================================================================================================================
// Adds a postamble to the end of a new command buffer.
Result ComputeCmdBuffer::AddPostamble()
{
    Result result = ComputeCmdBuffer::WritePostambleCommands(m_cmdUtil,
                                                             this,
                                                             &m_cmdStream);
    if (result == Result::Success)
    {
    }

    return result;
}

// =====================================================================================================================
// Enables the specified query type.
void ComputeCmdBuffer::ActivateQueryType(
    QueryPoolType queryPoolType)
{
    // Compute command buffers only support pipeline stat queries.
    PAL_ASSERT(queryPoolType == QueryPoolType::PipelineStats);

    Pm4CmdBuffer::ActivateQueryType(queryPoolType);

    uint32* pCmdSpace = m_cmdStream.ReserveCommands();
    pCmdSpace += m_cmdUtil.BuildNonSampleEventWrite(PIPELINESTAT_START, EngineTypeCompute, pCmdSpace);
    m_cmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// Disables the specified query type.
void ComputeCmdBuffer::DeactivateQueryType(
    QueryPoolType queryPoolType)
{
    // Compute command buffers only support pipeline stat queries.
    PAL_ASSERT(queryPoolType == QueryPoolType::PipelineStats);

    Pm4CmdBuffer::DeactivateQueryType(queryPoolType);

    uint32* pCmdSpace = m_cmdStream.ReserveCommands();
    pCmdSpace += m_cmdUtil.BuildNonSampleEventWrite(PIPELINESTAT_STOP, EngineTypeCompute, pCmdSpace);
    m_cmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// Adds commands necessary to write "data" to the specified event's memory.
void ComputeCmdBuffer::WriteEventCmd(
    const BoundGpuMemory& boundMemObj,
    HwPipePoint           pipePoint,
    uint32                data)
{
    uint32* pCmdSpace = m_cmdStream.ReserveCommands();

    if ((pipePoint >= HwPipePostBlt) && (m_pm4CmdBufState.flags.cpBltActive))
    {
        // We must guarantee that all prior CP DMA accelerated blts have completed before we write this event because
        // the CmdSetEvent and CmdResetEvent functions expect that the prior blts have reached the post-blt stage by
        // the time the event is written to memory. Given that our CP DMA blts are asynchronous to the pipeline stages
        // the only way to satisfy this requirement is to force the MEC to stall until the CP DMAs are completed.
        pCmdSpace += m_cmdUtil.BuildWaitDmaData(pCmdSpace);
        SetPm4CmdBufCpBltState(false);
    }

    if ((pipePoint == HwPipeTop) || (pipePoint == HwPipePreCs))
    {
        // Implement set/reset event with a WRITE_DATA command using the CP.
        WriteDataInfo writeData = {};
        writeData.engineType = EngineTypeCompute;
        writeData.dstAddr    = boundMemObj.GpuVirtAddr();
        writeData.dstSel     = dst_sel__mec_write_data__memory;

        pCmdSpace += m_cmdUtil.BuildWriteData(writeData, data, pCmdSpace);
    }
    else
    {
        // Don't expect to see HwPipePreRasterization or HwPipePostPs on the compute queue...
        PAL_ASSERT((pipePoint == HwPipePostCs) || (pipePoint == HwPipeBottom));

        // Implement set/reset with an EOP event written when all prior GPU work completes. Note that waiting on an
        // EOS timestamp and waiting on an EOP timestamp are exactly equivalent on compute queues. There's no reason
        // to implement a CS_DONE path for HwPipePostCs.
        ReleaseMemGeneric releaseInfo = {};
        releaseInfo.engineType = EngineTypeCompute;
        releaseInfo.dstAddr    = boundMemObj.GpuVirtAddr();
        releaseInfo.dataSel    = data_sel__mec_release_mem__send_32_bit_low;
        releaseInfo.data       = data;

        pCmdSpace += m_cmdUtil.BuildReleaseMemGeneric(releaseInfo, pCmdSpace);
    }

    m_cmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// Enables or disables a flexible predication check which the CP uses to determine if a draw or dispatch can be skipped
// based on the results of prior GPU work.
// SEE: CmdUtil::BuildSetPredication(...) for more details on the meaning of this method's parameters.
// Note that this function is currently only implemented for memory-based/DX12 predication
void ComputeCmdBuffer::CmdSetPredication(
    IQueryPool*         pQueryPool,
    uint32              slot,
    const IGpuMemory*   pGpuMemory,
    gpusize             offset,
    PredicateType       predType,
    bool                predPolarity,
    bool                waitResults,
    bool                accumulateData)
{
    // This emulation doesn't work for QueryPool based predication, fortuanately DX12 just has Boolean type
    // predication. TODO: emulation for Zpass and Streamout predication if they are really used on compute.
    PAL_ASSERT(pQueryPool == nullptr);
    PAL_ASSERT((predType == PredicateType::Boolean64) || (predType == PredicateType::Boolean32));

    // When gpuVirtAddr is 0, it means client is disabling/resetting predication
    m_gfxCmdBufStateFlags.clientPredicate  = (pGpuMemory != nullptr);
    m_pm4CmdBufState.flags.packetPredicate = m_gfxCmdBufStateFlags.clientPredicate;

    if (pGpuMemory != nullptr)
    {
        gpusize gpuVirtAddr  = pGpuMemory->Desc().gpuVirtAddr + offset;
        uint32 *pPredCpuAddr = CmdAllocateEmbeddedData(1, 1, &m_predGpuAddr);
        uint32 *pCmdSpace    = m_cmdStream.ReserveCommands();

        // Execute if 64-bit value in memory are all 0 when predPolarity is false,
        // or Execute if one or more bits of 64-bit value in memory are not 0 when predPolarity is true.
        uint32 predCopyData  = (predPolarity == true);
        *pPredCpuAddr        = (predPolarity == false);

        WriteDataInfo writeData = {};
        writeData.engineType = EngineTypeCompute;
        writeData.dstAddr    = m_predGpuAddr;
        writeData.dstSel     = dst_sel__mec_write_data__memory;

        pCmdSpace += m_cmdUtil.BuildCondExec(gpuVirtAddr, CmdUtil::WriteDataSizeDwords + 1, pCmdSpace);
        pCmdSpace += m_cmdUtil.BuildWriteData(writeData, predCopyData, pCmdSpace);

        if (predType == PredicateType::Boolean64)
        {
            pCmdSpace += m_cmdUtil.BuildCondExec(gpuVirtAddr + 4, CmdUtil::WriteDataSizeDwords + 1, pCmdSpace);
            pCmdSpace += m_cmdUtil.BuildWriteData(writeData, predCopyData, pCmdSpace);
        }

        m_cmdStream.CommitCommands(pCmdSpace);
    }
    else
    {
        m_predGpuAddr = 0;
    }
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdExecuteIndirectCmds(
    const IIndirectCmdGenerator& generator,
    const IGpuMemory&            gpuMemory,
    gpusize                      offset,
    uint32                       maximumCount,
    gpusize                      countGpuAddr)
{
    // It is only safe to generate indirect commands on a one-time-submit or exclusive-submit command buffer because
    // there is a potential race condition on the memory used to receive the generated commands.
    PAL_ASSERT(IsOneTimeSubmit() || IsExclusiveSubmit());

    const auto& gfx9Generator = static_cast<const IndirectCmdGenerator&>(generator);

    if (countGpuAddr == 0uLL)
    {
        // If the count GPU address is zero, then we are expected to use the maximumCount value as the actual number
        // of indirect commands to generate and execute.
        uint32* pMemory = CmdAllocateEmbeddedData(1, 1, &countGpuAddr);
        *pMemory = maximumCount;
    }

    AutoBuffer<CmdStreamChunk*, 16, Platform> deChunks(maximumCount, m_device.GetPlatform());

    if (deChunks.Capacity() < maximumCount)
    {
        NotifyAllocFailure();
    }
    else
    {
        CmdStreamChunk** ppChunkList[] =
        {
            deChunks.Data(),
        };
        uint32 numGenChunks = 0;

        // Generate the indirect command buffer chunk(s) using RPM. Since we're wrapping the command generation and
        // execution inside a CmdIf, we want to disable normal predication for this blit.
        const uint32 packetPredicate = m_pm4CmdBufState.flags.packetPredicate;
        m_pm4CmdBufState.flags.packetPredicate = 0;

        constexpr uint32 DummyIndexBufSize = 0; // Compute doesn't care about the index buffer size.
        const Pm4::GenerateInfo genInfo =
        {
            this,
            m_computeState.pipelineState.pPipeline,
            gfx9Generator,
            DummyIndexBufSize,
            maximumCount,
            (gpuMemory.Desc().gpuVirtAddr + offset),
            countGpuAddr
        };

        m_device.RsrcProcMgr().CmdGenerateIndirectCmds(genInfo, &ppChunkList[0], 1, &numGenChunks);

        m_pm4CmdBufState.flags.packetPredicate = packetPredicate;

        uint32* pCmdSpace = m_cmdStream.ReserveCommands();

        // Insert a wait-for-idle to make sure that the generated commands are written out to L2 before we attempt to
        // execute them.
        AcquireMemGeneric acquireInfo = {};
        acquireInfo.engineType = EngineTypeCompute;
        acquireInfo.cacheSync  = SyncGlkInv;

        pCmdSpace  = WriteWaitCsIdle(pCmdSpace);
        pCmdSpace += m_cmdUtil.BuildAcquireMemGeneric(acquireInfo, pCmdSpace);

        // PFP_SYNC_ME cannot be used on an async compute engine so we need to use REWIND packet instead.
        pCmdSpace += m_cmdUtil.BuildRewind(false, true, pCmdSpace);

        // Just like a normal direct/indirect dispatch, we need to perform state validation before executing the
        // generated command chunks.
        pCmdSpace = ValidateDispatchPalAbi(0uLL, 0uLL, {}, pCmdSpace);
        m_cmdStream.CommitCommands(pCmdSpace);

        CommandGeneratorTouchedUserData(m_computeState.csUserDataEntries.touched, gfx9Generator, *m_pSignatureCs);

        // NOTE: The command stream expects an iterator to the first chunk to execute, but this iterator points to the
        // place in the list before the first generated chunk (see comments above).
        m_cmdStream.ExecuteGeneratedCommands(ppChunkList[0], 0, numGenChunks);
    }

}

// =====================================================================================================================
void ComputeCmdBuffer::GetChunkForCmdGeneration(
    const Pm4::IndirectCmdGenerator& generator,
    const Pal::Pipeline&             pipeline,
    uint32                           maxCommands,
    uint32                           numChunkOutputs,
    ChunkOutput*                     pChunkOutputs)
{
    const Pm4::GeneratorProperties& properties = generator.Properties();
    const ComputePipelineSignature& signature  = static_cast<const ComputePipeline&>(pipeline).Signature();

    PAL_ASSERT(m_pCmdAllocator != nullptr);
    PAL_ASSERT(numChunkOutputs == 1);

    CmdStreamChunk*const pChunk = Pal::Pm4CmdBuffer::GetNextGeneratedChunk();
    pChunkOutputs->pChunk = pChunk;

    // NOTE: RPM uses a compute shader to generate indirect commands, so we need to use the saved user-data state
    // because RPM will have pushed its own state before calling this method.
    const uint32* pUserDataEntries = &m_computeRestoreState.csUserDataEntries.entries[0];

    // User-data high watermark for this command Generator. It depends on the command Generator itself, as well as the
    // pipeline signature for the active pipeline. This is due to the fact that if the command Generator modifies the
    // contents of an indirect user-data table, the command Generator must also fix-up the user-data entry used for the
    // table's GPU virtual address.
    uint32 userDataWatermark = properties.userDataWatermark;

    const uint32 commandDwords = (generator.Properties().cmdBufStride / sizeof(uint32));
    // There are three possibilities when determining how much spill-table space a generated command will need:
    //  (1) The active pipeline doesn't spill at all. This requires no spill-table space.
    //  (2) The active pipeline spills, but the generator doesn't update the any user-data entries beyond the
    //      spill threshold. This requires no spill-table space.
    //  (3) The active pipeline spills, and the generator updates user-data entries which are beyond the spill
    //      threshold. This means each generated command needs to relocate the spill table in addition to the other
    //      stuff it would normally do.
    const uint32 spillDwords =
        (signature.spillThreshold < properties.userDataWatermark) ? properties.maxUserDataEntries : 0;
    // Total amount of embedded data space needed for each generated command, including indirect user-data tables and
    // user-data spilling.
    uint32 embeddedDwords = spillDwords;

    // Ask the DE command stream to make sure the command chunk is ready to receive GPU-generated commands (this
    // includes setting up padding for size alignment, allocating command space, etc.
    (pChunkOutputs->commandsInChunk)  = m_cmdStream.PrepareChunkForCmdGeneration(pChunk,
                                                                                 commandDwords,
                                                                                 embeddedDwords,
                                                                                 maxCommands);
    (pChunkOutputs->embeddedDataSize) = ((pChunkOutputs->commandsInChunk) * embeddedDwords);

    // Populate command buffer chain size required later for an indirect command generation optimization.
    (pChunkOutputs->chainSizeInDwords) = m_cmdStream.GetChainSizeInDwords(m_device, EngineTypeCompute, IsNested());

    if (spillDwords > 0)
    {
        // If each generated command requires some amount of spill-table space, then we need to allocate embeded data
        // space for all of the generated commands which will go into this chunk. PrepareChunkForCmdGeneration() should
        // have determined a value for commandsInChunk which allows us to allocate the appropriate amount of embeded
        // data space.
        uint32* pDataSpace = pChunk->ValidateCmdGenerationDataSpace(pChunkOutputs->embeddedDataSize,
                                                                    &(pChunkOutputs->embeddedDataAddr));

        // We also need to seed the embedded data for each generated command with the current indirect user-data table
        // and spill-table contents, because the generator will only update the table entries which get modified.
        for (uint32 cmd = 0; cmd < (pChunkOutputs->commandsInChunk); ++cmd)
        {
            memcpy(pDataSpace, pUserDataEntries, (sizeof(uint32) * spillDwords));
            pDataSpace += spillDwords;
        }
    }
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdPrimeGpuCaches(
    uint32                    rangeCount,
    const PrimeGpuCacheRange* pRanges)
{
    PAL_ASSERT((rangeCount == 0) || (pRanges != nullptr));

    for (uint32 i = 0; i < rangeCount; ++i)
    {
        uint32* pCmdSpace = m_cmdStream.ReserveCommands();

        pCmdSpace += m_cmdUtil.BuildPrimeGpuCaches(pRanges[i], EngineTypeCompute, pCmdSpace);

        m_cmdStream.CommitCommands(pCmdSpace);
    }
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdExecuteNestedCmdBuffers(
    uint32            cmdBufferCount,
    ICmdBuffer*const* ppCmdBuffers)
{
    for (uint32 buf = 0; buf < cmdBufferCount; ++buf)
    {
        auto*const pCallee = static_cast<Gfx9::ComputeCmdBuffer*>(ppCmdBuffers[buf]);
        PAL_ASSERT(pCallee != nullptr);

        if (pCallee->m_inheritedPredication && (m_predGpuAddr != 0))
        {
            PAL_ASSERT(pCallee->m_predGpuAddr != 0);

            uint32 *pCmdSpace = m_cmdStream.ReserveCommands();

            pCmdSpace += m_cmdUtil.BuildCopyData(EngineTypeCompute,
                                                 0,
                                                 dst_sel__mec_copy_data__tc_l2,
                                                 pCallee->m_predGpuAddr,
                                                 src_sel__mec_copy_data__tc_l2,
                                                 m_predGpuAddr,
                                                 count_sel__mec_copy_data__32_bits_of_data,
                                                 wr_confirm__mec_copy_data__wait_for_confirmation,
                                                 pCmdSpace);

            m_cmdStream.CommitCommands(pCmdSpace);
        }

        // Track the most recent OS paging fence value across all nested command buffers called from this one.
        m_lastPagingFence = Max(m_lastPagingFence, pCallee->LastPagingFence());

        // Track the lastest fence token across all nested command buffers called from this one.
        m_maxUploadFenceToken = Max(m_maxUploadFenceToken, pCallee->GetMaxUploadFenceToken());

        // All user-data entries have been uploaded into the GPU memory the callee expects to receive them in, so we
        // can safely "call" the nested command buffer's command stream.
        m_cmdStream.TrackNestedEmbeddedData(pCallee->m_embeddedData.chunkList);
        m_cmdStream.TrackNestedEmbeddedData(pCallee->m_gpuScratchMem.chunkList);
        m_cmdStream.TrackNestedCommands(pCallee->m_cmdStream);
        m_cmdStream.Call(pCallee->m_cmdStream, pCallee->IsExclusiveSubmit(), false);

        // Callee command buffers are also able to leak any changes they made to bound user-data entries and any other
        // state back to the caller.
        LeakNestedCmdBufferState(*pCallee);
    }
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdInsertTraceMarker(
    PerfTraceMarkerType markerType,
    uint32              markerData)
{
    const uint32 userDataAddr =
        (markerType == PerfTraceMarkerType::A) ? mmSQ_THREAD_TRACE_USERDATA_2 : mmSQ_THREAD_TRACE_USERDATA_3;

    uint32* pCmdSpace = m_cmdStream.ReserveCommands();
    pCmdSpace = m_cmdStream.WriteSetOneConfigReg(userDataAddr, markerData, pCmdSpace);
    m_cmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
void ComputeCmdBuffer::CmdInsertRgpTraceMarker(
    RgpMarkerSubQueueFlags subQueueFlags,
    uint32                 numDwords,
    const void*            pData)
{
    PAL_ASSERT((subQueueFlags.includeMainSubQueue == 1) && (subQueueFlags.includeGangedSubQueues == 0));

    // The first dword of every RGP trace marker packet is written to SQ_THREAD_TRACE_USERDATA_2.  The second dword
    // is written to SQ_THREAD_TRACE_USERDATA_3.  For packets longer than 64-bits, continue alternating between
    // user data 2 and 3.
    static_assert(mmSQ_THREAD_TRACE_USERDATA_3 == mmSQ_THREAD_TRACE_USERDATA_2 + 1, "Registers not sequential!");

    const uint32* pDwordData = static_cast<const uint32*>(pData);
    while (numDwords > 0)
    {
        const uint32 dwordsToWrite = Min(numDwords, 2u);

        // Reserve and commit command space inside this loop.  Some of the RGP packets are unbounded, like adding a
        // comment string, so it's not safe to assume the whole packet will fit under our reserve limit.
        uint32* pCmdSpace = m_cmdStream.ReserveCommands();

        pCmdSpace = m_cmdStream.WriteSetSeqConfigRegs(mmSQ_THREAD_TRACE_USERDATA_2,
                                                      mmSQ_THREAD_TRACE_USERDATA_2 + dwordsToWrite - 1,
                                                      pDwordData,
                                                      pCmdSpace);
        pDwordData += dwordsToWrite;
        numDwords -= dwordsToWrite;

        m_cmdStream.CommitCommands(pCmdSpace);
    }
}

// =====================================================================================================================
// Updates the SQTT token mask for all SEs outside of a specific PerfExperiment.  Used by GPA Session when targeting
// a single event for instruction level trace during command buffer building.
void ComputeCmdBuffer::CmdUpdateSqttTokenMask(
    const ThreadTraceTokenConfig& sqttTokenConfig)
{
    PerfExperiment::UpdateSqttTokenMaskStatic(&m_cmdStream, sqttTokenConfig, m_device);
}

// =====================================================================================================================
void ComputeCmdBuffer::AddPerPresentCommands(
    gpusize frameCountGpuAddr,
    uint32  frameCntReg)
{
    uint32* pCmdSpace = m_cmdStream.ReserveCommands();

    pCmdSpace += m_cmdUtil.BuildAtomicMem(AtomicOp::IncUint32,
                                          frameCountGpuAddr,
                                          UINT32_MAX,
                                          pCmdSpace);

    pCmdSpace += m_cmdUtil.BuildCopyData(EngineTypeCompute,
                                         0,
                                         dst_sel__mec_copy_data__perfcounters,
                                         frameCntReg,
                                         src_sel__mec_copy_data__tc_l2,
                                         frameCountGpuAddr,
                                         count_sel__mec_copy_data__32_bits_of_data,
                                         wr_confirm__mec_copy_data__do_not_wait_for_confirmation,
                                         pCmdSpace);

    m_cmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// Bind the last state set on the specified command buffer
void ComputeCmdBuffer::InheritStateFromCmdBuf(
    const Pm4CmdBuffer* pCmdBuffer)
{
    SetComputeState(pCmdBuffer->GetComputeState(), ComputeStateAll);
}

// =====================================================================================================================
// Copy memory using the CP's DMA engine
void ComputeCmdBuffer::CpCopyMemory(
    gpusize dstAddr,
    gpusize srcAddr,
    gpusize numBytes)
{
    PAL_ASSERT(numBytes < (1ull << 32));

    DmaDataInfo dmaDataInfo = {};
    dmaDataInfo.dstSel      = dst_sel__pfp_dma_data__dst_addr_using_l2;
    dmaDataInfo.srcSel      = src_sel__pfp_dma_data__src_addr_using_l2;
    dmaDataInfo.sync        = false;
    dmaDataInfo.usePfp      = false;
    dmaDataInfo.dstAddr     = dstAddr;
    dmaDataInfo.srcAddr     = srcAddr;
    dmaDataInfo.numBytes    = static_cast<uint32>(numBytes);

    uint32* pCmdSpace = m_cmdStream.ReserveCommands();
    if (m_pm4CmdBufState.flags.packetPredicate != 0)
    {
        pCmdSpace += m_cmdUtil.BuildCondExec(m_predGpuAddr, CmdUtil::DmaDataSizeDwords, pCmdSpace);
    }
    pCmdSpace += m_cmdUtil.BuildDmaData<false>(dmaDataInfo, pCmdSpace);
    m_cmdStream.CommitCommands(pCmdSpace);

    SetPm4CmdBufCpBltState(true);
    SetPm4CmdBufCpBltWriteCacheState(true);
}

// =====================================================================================================================
uint32* ComputeCmdBuffer::WriteWaitEop(
    HwPipePoint waitPoint,
    uint32      hwGlxSync,
    uint32      hwRbSync,
    uint32*     pCmdSpace)
{
    SyncGlxFlags glxSync = SyncGlxFlags(hwGlxSync);

    PAL_ASSERT(hwRbSync == SyncRbNone);

    // We prefer to do our GCR in the release_mem if we can. This function always does an EOP wait so we don't have
    // to worry about release_mem not supporting GCRs with EOS events. Any remaining sync flags must be handled in a
    // trailing acquire_mem packet.
    ReleaseMemGeneric releaseInfo = {};
    releaseInfo.engineType = EngineTypeCompute;
    releaseInfo.cacheSync  = m_cmdUtil.SelectReleaseMemCaches(&glxSync);
    releaseInfo.dstAddr    = AcqRelFenceValGpuVa(AcqRelEventType::Eop);
    releaseInfo.dataSel    = data_sel__me_release_mem__send_32_bit_low;
    releaseInfo.data       = GetNextAcqRelFenceVal(AcqRelEventType::Eop);

    pCmdSpace += m_cmdUtil.BuildReleaseMemGeneric(releaseInfo, pCmdSpace);
    pCmdSpace += m_cmdUtil.BuildWaitRegMem(EngineTypeCompute,
                                           mem_space__me_wait_reg_mem__memory_space,
                                           function__me_wait_reg_mem__equal_to_the_reference_value,
                                           engine_sel__me_wait_reg_mem__micro_engine,
                                           releaseInfo.dstAddr,
                                           releaseInfo.data,
                                           UINT32_MAX,
                                           pCmdSpace);

    // If we still have some caches to sync we require a final acquire_mem. It doesn't do any waiting, it just
    // immediately does some full-range cache flush and invalidates. The previous WRM packet is the real wait.
    if (glxSync != SyncGlxNone)
    {
        AcquireMemGeneric acquireInfo = {};
        acquireInfo.engineType = EngineTypeCompute;
        acquireInfo.cacheSync  = glxSync;

        pCmdSpace += m_cmdUtil.BuildAcquireMemGeneric(acquireInfo, pCmdSpace);
    }

    SetPm4CmdBufCsBltState(false);
    SetPrevCmdBufInactive();

    return pCmdSpace;
}

// =====================================================================================================================
uint32* ComputeCmdBuffer::WriteWaitCsIdle(
    uint32* pCmdSpace)
{
    pCmdSpace += m_cmdUtil.BuildWaitCsIdle(GetEngineType(), TimestampGpuVirtAddr(), pCmdSpace);

    SetPm4CmdBufCsBltState(false);

    return pCmdSpace;
}

} // Gfx9
} // Pal
