/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/gfx9/gfx9CmdUtil.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9Image.h"
#include "core/hw/gfxip/gfx9/gfx9UniversalCmdBuffer.h"
#include "palAutoBuffer.h"

using namespace Util;

namespace Pal
{
namespace Gfx9
{

constexpr uint32 GraphicsOnlyPipeStages = PipelineStageVs            |
                                          PipelineStageHs            |
                                          PipelineStageDs            |
                                          PipelineStageGs            |
                                          PipelineStagePs            |
                                          PipelineStageEarlyDsTarget |
                                          PipelineStageLateDsTarget  |
                                          PipelineStageColorTarget;

constexpr uint32 GraphicsOnlyCoherFlags = CoherColorTarget        |
                                          CoherDepthStencilTarget |
                                          CoherSampleRate         |
                                          CoherCeLoad             |
                                          CoherCeDump             |
                                          CoherStreamOut;

// A structure that helps cache and reuse the calculated BLT transition and sync requests for an image barrier in
// acquire-release based barrier.
struct AcqRelTransitionInfo
{
    const ImgBarrier*    pImgBarrier;
    LayoutTransitionInfo layoutTransInfo;
    uint32               bltStageMask;
    uint32               bltAccessMask;
    bool                 waMetaMisalignNeedRefreshLlc; // Finer-grain refresh LLC flag.
};

// =====================================================================================================================
// Translate acquire's accessMask (CacheCoherencyUsageFlags type) to cacheSyncFlags (CacheSyncFlags type)
// This function is GFX9-ONLY.
static uint32 Gfx9ConvertToAcquireSyncFlags(
    uint32                        accessMask,
    EngineType                    engineType,
    bool                          invalidateTcc,
    Developer::BarrierOperations* pBarrierOps)
{
    PAL_ASSERT(pBarrierOps != nullptr);
    uint32 cacheSyncFlagsMask = 0;

    // The acquire-release barrier treats L2 as the central cache, so we never flush/inv TCC unless it's
    // direct-to-memory access.
    if (TestAnyFlagSet(accessMask, CoherCpu | CoherMemory))
    {
        cacheSyncFlagsMask |= CacheSyncFlushTcc;
        pBarrierOps->caches.flushTcc = 1;
    }

    if (TestAnyFlagSet(accessMask, CoherShader))
    {
        cacheSyncFlagsMask |= CacheSyncInvSqK$ | CacheSyncInvTcp | CacheSyncInvTccMd;
        pBarrierOps->caches.invalSqK$        = 1;
        pBarrierOps->caches.invalTcp         = 1;
        pBarrierOps->caches.invalTccMetadata = 1;
    }

    // There are various BLTs (Copy, Clear, and Resolve) that can involve different caches based on what engine
    // does the BLT.
    // - If a graphics BLT occurred, alias to CB/DB. -> CacheSyncInvRb
    // - If a compute BLT occurred, alias to shader. -> CacheSyncInvSqK$,SqI$,Tcp,TccMd
    // - If a CP L2 BLT occured, alias to L2.        -> None (data is always in TCC as it's the central cache)
    // RB invalidations are guaranteed to be handled in earlier release, so skip any RB sync at acquire.
    if (TestAnyFlagSet(accessMask, CoherCopy | CoherResolve | CoherClear))
    {
        cacheSyncFlagsMask |= CacheSyncInvSqK$ | CacheSyncInvTcp | CacheSyncInvTccMd;
        pBarrierOps->caches.invalSqK$        = 1;
        pBarrierOps->caches.invalTcp         = 1;
        pBarrierOps->caches.invalTccMetadata = 1;
    }

    if (TestAnyFlagSet(accessMask, CoherStreamOut))
    {
        // Read/write through Tcp$ and SqK$. Tcp$ is read-only.
        cacheSyncFlagsMask |= CacheSyncInvSqK$ | CacheSyncInvTcp;
        pBarrierOps->caches.invalSqK$ = 1;
        pBarrierOps->caches.invalTcp  = 1;
    }

    if (invalidateTcc)
    {
        cacheSyncFlagsMask |= CacheSyncInvTcc;
        pBarrierOps->caches.invalTcc = 1;
    }

    return cacheSyncFlagsMask;
}

// =====================================================================================================================
// Translate release's accessMask (CacheCoherencyUsageFlags type) to cacheSyncFlags (CacheSyncFlags type)
// This function is GFX9-ONLY.
static uint32 Gfx9ConvertToReleaseSyncFlags(
    uint32                        accessMask,
    bool                          flushTcc,
    Developer::BarrierOperations* pBarrierOps)
{
    PAL_ASSERT(pBarrierOps != nullptr);

    // If CB/DB sync is requested, it should have been converted to VGT event at an earlier point.
    PAL_ASSERT(TestAnyFlagSet(accessMask, CoherColorTarget | CoherDepthStencilTarget) == false);

    uint32 cacheSyncFlagsMask = 0;

    if (TestAnyFlagSet(accessMask, CoherCpu | CoherMemory))
    {
        // At release we want to invalidate L2 so any future read to L2 would go down to memory, at acquire we want to
        // flush L2 so that main memory gets the latest data.
        cacheSyncFlagsMask |= CacheSyncInvTcc;
        pBarrierOps->caches.invalTcc = 1;
    }

    if (flushTcc)
    {
        cacheSyncFlagsMask |= CacheSyncFlushTcc;
        pBarrierOps->caches.flushTcc = 1;
    }

    return cacheSyncFlagsMask;
}

// =====================================================================================================================
// Fill in a given BarrierOperations struct with info about a layout transition
static BarrierTransition AcqRelBuildTransition(
    const ImgBarrier*             pBarrier,
    LayoutTransitionInfo          transitionInfo,
    Developer::BarrierOperations* pBarrierOps)
{
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 642
    PAL_ASSERT(pBarrier->subresRange.numPlanes == 1);
#endif

    switch (transitionInfo.blt[0])
    {
    case ExpandDepthStencil:
        pBarrierOps->layoutTransitions.depthStencilExpand = 1;
        break;
    case HwlExpandHtileHiZRange:
        pBarrierOps->layoutTransitions.htileHiZRangeExpand = 1;
        break;
    case ResummarizeDepthStencil:
        pBarrierOps->layoutTransitions.depthStencilResummarize = 1;
        break;
    case FastClearEliminate:
        if (transitionInfo.flags.fceIsSkipped == false)
        {
            pBarrierOps->layoutTransitions.fastClearEliminate = 1;
        }
        break;
    case FmaskDecompress:
        pBarrierOps->layoutTransitions.fmaskDecompress = 1;
        break;
    case DccDecompress:
        pBarrierOps->layoutTransitions.dccDecompress = 1;
        break;
    case MsaaColorDecompress:
        pBarrierOps->layoutTransitions.fmaskColorExpand = 1;
        break;
    case InitMaskRam:
        pBarrierOps->layoutTransitions.initMaskRam = 1;
        break;
    case None:
    default:
        PAL_NEVER_CALLED();
    }

    if (transitionInfo.blt[1] != None)
    {
        // Second decompress pass can only be MSAA color decompress.
        PAL_ASSERT(transitionInfo.blt[1] == MsaaColorDecompress);
        pBarrierOps->layoutTransitions.fmaskColorExpand = 1;
    }

    BarrierTransition out = {};
    out.srcCacheMask                 = pBarrier->srcAccessMask;
    out.dstCacheMask                 = pBarrier->dstAccessMask;
    out.imageInfo.pImage             = pBarrier->pImage;
    out.imageInfo.subresRange        = pBarrier->subresRange;
    out.imageInfo.oldLayout          = pBarrier->oldLayout;
    out.imageInfo.newLayout          = pBarrier->newLayout;
    out.imageInfo.pQuadSamplePattern = pBarrier->pQuadSamplePattern;

    return out;
}

// =====================================================================================================================
// Look up for the stage and access mask associate with the first-pass BLT.
static void GetBltStageAccessInfo(
    LayoutTransitionInfo info,
    uint32*              pStageMask,
    uint32*              pAccessMask)
{
    PAL_ASSERT((pStageMask  != nullptr) && (pAccessMask != nullptr));

    // Initialize value
    *pStageMask  = 0;
    *pAccessMask = 0;

    switch (info.blt[0])
    {
    case HwLayoutTransition::ExpandDepthStencil:
        if (info.flags.useComputePath != 0)
        {
            *pStageMask  = PipelineStageCs;
            *pAccessMask = CoherShader;
        }
        else
        {
            *pStageMask  = PipelineStageEarlyDsTarget;
            *pAccessMask = CoherDepthStencilTarget;
        }
        break;

    case HwLayoutTransition::HwlExpandHtileHiZRange:
    case HwLayoutTransition::MsaaColorDecompress:
    case HwLayoutTransition::InitMaskRam:
        *pStageMask  = PipelineStageCs;
        *pAccessMask = CoherShader;
        break;

    case HwLayoutTransition::ResummarizeDepthStencil:
        *pStageMask  = PipelineStageEarlyDsTarget;
        *pAccessMask = CoherDepthStencilTarget;
        break;

    case HwLayoutTransition::FastClearEliminate:
        if (info.flags.fceIsSkipped == false)
        {
            *pStageMask  = PipelineStageColorTarget;
            *pAccessMask = CoherColorTarget;
        }
        break;
    case HwLayoutTransition::FmaskDecompress:
        *pStageMask  = PipelineStageColorTarget;
        *pAccessMask = CoherColorTarget;
        break;

    case HwLayoutTransition::DccDecompress:
        if (info.flags.useComputePath != 0)
        {
            *pStageMask  = PipelineStageCs;
            *pAccessMask = CoherShader;
        }
        else
        {
            *pStageMask  = PipelineStageColorTarget;
            *pAccessMask = CoherColorTarget;
        }
        break;

    case HwLayoutTransition::None:
        // Do nothing.
        break;

    default:
        PAL_NEVER_CALLED();
        break;
    }
}

// =====================================================================================================================
// Wrapper to call RPM's InitMaskRam to issues a compute shader blt to initialize the Mask RAM allocatons for an Image.
// Returns "true" if the compute engine was used for the InitMaskRam operation.
bool Device::AcqRelInitMaskRam(
    GfxCmdBuffer*      pCmdBuf,
    CmdStream*         pCmdStream,
    const ImgBarrier&  imgBarrier
    ) const
{
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 642
    PAL_ASSERT(imgBarrier.subresRange.numPlanes == 1);
#endif
    // If the LayoutUninitializedTarget usage is set, no other usages should be set.
    PAL_ASSERT(TestAnyFlagSet(imgBarrier.oldLayout.usages, ~LayoutUninitializedTarget) == false);

    const EngineType engineType  = pCmdStream->GetEngineType();
    const auto&      image       = static_cast<const Pal::Image&>(*imgBarrier.pImage);
    const auto&      gfx9Image   = static_cast<const Gfx9::Image&>(*image.GetGfxImage());
    const auto&      subresRange = imgBarrier.subresRange;

#if PAL_ENABLE_PRINTS_ASSERTS
    const auto& engineProps = Parent()->EngineProperties().perEngine[engineType];
    const auto& createInfo  = image.GetImageCreateInfo();
    const bool  isFullPlane = image.IsRangeFullPlane(subresRange);

    // This queue must support this barrier transition.
    PAL_ASSERT(engineProps.flags.supportsImageInitBarrier == 1);

    // By default, the entire plane must be initialized in one go. Per-subres support can be requested
    // using an image flag as long as the queue supports it.
    PAL_ASSERT(isFullPlane || ((engineProps.flags.supportsImageInitPerSubresource == 1) &&
                               (createInfo.flags.perSubresInit == 1)));
#endif

    PAL_ASSERT(gfx9Image.HasColorMetaData() || gfx9Image.HasHtileData());

    const bool usedCompute = RsrcProcMgr().InitMaskRam(pCmdBuf,
                                                       pCmdStream,
                                                       gfx9Image,
                                                       subresRange,
                                                       imgBarrier.newLayout);

    return usedCompute;
}

// =====================================================================================================================
// Issue the specified BLT operation(s) (i.e., decompress, resummarize) necessary to convert a depth/stencil image from
// one ImageLayout to another.
void Device::AcqRelDepthStencilTransition(
    GfxCmdBuffer*        pCmdBuf,
    const ImgBarrier&    imgBarrier,
    LayoutTransitionInfo layoutTransInfo
    ) const
{
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 642
    PAL_ASSERT(imgBarrier.subresRange.numPlanes == 1);
#endif
    PAL_ASSERT(imgBarrier.pImage != nullptr);

    const auto& image = static_cast<const Pal::Image&>(*imgBarrier.pImage);

    if (layoutTransInfo.blt[0] == HwLayoutTransition::HwlExpandHtileHiZRange)
    {
        const auto& gfx9Image = static_cast<const Image&>(*image.GetGfxImage());

        // CS blit to resummarize Htile.
        RsrcProcMgr().HwlResummarizeHtileCompute(pCmdBuf, gfx9Image, imgBarrier.subresRange);
    }
    else
    {
        if (layoutTransInfo.blt[0] == HwLayoutTransition::ExpandDepthStencil)
        {
            RsrcProcMgr().ExpandDepthStencil(pCmdBuf,
                                             image,
                                             imgBarrier.pQuadSamplePattern,
                                             imgBarrier.subresRange);
        }
        else
        {
            PAL_ASSERT(layoutTransInfo.blt[0] == HwLayoutTransition::ResummarizeDepthStencil);

            // DB blit to resummarize.
            RsrcProcMgr().ResummarizeDepthStencil(pCmdBuf,
                                                  image,
                                                  imgBarrier.newLayout,
                                                  imgBarrier.pQuadSamplePattern,
                                                  imgBarrier.subresRange);
        }
    }
}

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 648
// =====================================================================================================================
// Issue the specified BLT operation(s) (i.e., decompresses) necessary to convert a color image from one ImageLayout to
// another.
void Device::AcqRelColorTransition(
    GfxCmdBuffer*                 pCmdBuf,
    CmdStream*                    pCmdStream,
    const ImgBarrier&             imgBarrier,
    LayoutTransitionInfo          layoutTransInfo,
    Developer::BarrierOperations* pBarrierOps
    ) const
{
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 642
    PAL_ASSERT(imgBarrier.subresRange.numPlanes == 1);
#endif
    PAL_ASSERT(imgBarrier.pImage != nullptr);

    const EngineType            engineType  = pCmdBuf->GetEngineType();
    const auto&                 image       = static_cast<const Pal::Image&>(*imgBarrier.pImage);
    const auto&                 gfx9Image   = static_cast<const Gfx9::Image&>(*image.GetGfxImage());

    PAL_ASSERT(image.IsDepthStencilTarget() == false);

    if (layoutTransInfo.blt[0] == HwLayoutTransition::MsaaColorDecompress)
    {
        RsrcProcMgr().FmaskColorExpand(pCmdBuf, gfx9Image, imgBarrier.subresRange);
    }
    else
    {
        if (layoutTransInfo.blt[0] == HwLayoutTransition::DccDecompress)
        {
            RsrcProcMgr().DccDecompress(pCmdBuf,
                                        pCmdStream,
                                        gfx9Image,
                                        imgBarrier.pQuadSamplePattern,
                                        imgBarrier.subresRange);
        }
        else if (layoutTransInfo.blt[0] == HwLayoutTransition::FmaskDecompress)
        {
            RsrcProcMgr().FmaskDecompress(pCmdBuf,
                                          pCmdStream,
                                          gfx9Image,
                                          imgBarrier.pQuadSamplePattern,
                                          imgBarrier.subresRange);
        }
        else
        {
            PAL_ASSERT(layoutTransInfo.blt[0] == HwLayoutTransition::FastClearEliminate);

            // Note: if FCE is not submitted to GPU, we don't need to update cache flags.
            const bool isSubmitted = RsrcProcMgr().FastClearEliminate(pCmdBuf,
                                                                      pCmdStream,
                                                                      gfx9Image,
                                                                      imgBarrier.pQuadSamplePattern,
                                                                      imgBarrier.subresRange);
            layoutTransInfo.flags.fceIsSkipped = (isSubmitted == false);
        }

        // Handle corner cases where it needs a second pass.
        if (layoutTransInfo.blt[1] != HwLayoutTransition::None)
        {
            PAL_ASSERT(layoutTransInfo.blt[1] == HwLayoutTransition::MsaaColorDecompress);

            uint32 stageMask  = 0;
            uint32 accessMask = 0;

            // Prepare release info for first pass BLT.
            GetBltStageAccessInfo(layoutTransInfo, &stageMask, &accessMask);

            // Release from first pass.
            const AcqRelSyncToken syncToken = IssueReleaseSync(pCmdBuf,
                                                               pCmdStream,
                                                               stageMask,
                                                               accessMask,
                                                               false,
                                                               pBarrierOps);

            // Prepare second pass info.
            constexpr LayoutTransitionInfo MsaaBltInfo = { {}, HwLayoutTransition::MsaaColorDecompress };

            GetBltStageAccessInfo(MsaaBltInfo, &stageMask, &accessMask);

            // Acquire for second pass.
            IssueAcquireSync(pCmdBuf,
                             pCmdStream,
                             stageMask,
                             accessMask,
                             false,
                             FullSyncBaseAddr,
                             FullSyncSize,
                             1,
                             &syncToken,
                             pBarrierOps);

            // Tell RGP about this transition
            BarrierTransition rgpTransition = AcqRelBuildTransition(&imgBarrier, MsaaBltInfo, pBarrierOps);
            DescribeBarrier(pCmdBuf, &rgpTransition, pBarrierOps);

            // And clear it so it can differentiate sync and async flushes
            *pBarrierOps = {};

            RsrcProcMgr().FmaskColorExpand(pCmdBuf, gfx9Image, imgBarrier.subresRange);
        }
    }
}
#endif

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 648
// =====================================================================================================================
// Issue appropriate cache sync hardware commands to satisfy the cache release requirements.
AcqRelSyncToken Device::IssueReleaseSync(
    GfxCmdBuffer*                 pCmdBuf,
    CmdStream*                    pCmdStream,
    uint32                        stageMask,
    uint32                        accessMask,
    bool                          flushLlc,
    Developer::BarrierOperations* pBarrierOps
    ) const
{
    // Validate input.
    PAL_ASSERT(stageMask != 0);

    PAL_ASSERT(pBarrierOps != nullptr);

    const EngineType engineType = pCmdBuf->GetEngineType();

    uint32* pCmdSpace = pCmdStream->ReserveCommands();

    if (pCmdBuf->GetGfxCmdBufState().flags.cpBltActive && TestAnyFlagSet(stageMask, PipelineStageBlt))
    {
        // We must guarantee that all prior CP DMA accelerated blts have completed before we write this event because
        // the CmdSetEvent and CmdResetEvent functions expect that the prior blts have reached the post-blt stage by
        // the time the event is written to memory. Given that our CP DMA blts are asynchronous to the pipeline stages
        // the only way to satisfy this requirement is to force the MEC to stall until the CP DMAs are completed.
        pBarrierOps->pipelineStalls.syncCpDma = 1;
        pCmdSpace += m_cmdUtil.BuildWaitDmaData(pCmdSpace);
        pCmdBuf->SetGfxCmdBufCpBltState(false);
    }

    // Converts PipelineStageBlt stage to specific internal pipeline stage, and optimize cache flags for BLTs.
    pCmdBuf->OptimizePipeAndCacheMaskForRelease(&stageMask, &accessMask);

    if (pCmdBuf->IsGraphicsSupported())
    {
        // Mark off PS stage if current pipeline doesn't do rasterization.
        const auto* pUniversalCmdBuf = static_cast<UniversalCmdBuffer*>(pCmdBuf);
        if (pUniversalCmdBuf->IsRasterizationKilled())
        {
            stageMask &= ~PipelineStagePs;
        }
    }
    else
    {
        // Mark off all graphics path specific stages if command buffer doesn't support graphics.
        stageMask  &= ~GraphicsOnlyPipeStages;
        accessMask &= ~GraphicsOnlyCoherFlags;
    }

    AcqRelSyncToken syncToken = {};

    pCmdSpace += BuildReleaseSyncPackets(pCmdBuf,
                                         engineType,
                                         stageMask,
                                         accessMask,
                                         flushLlc,
                                         &syncToken,
                                         pCmdSpace,
                                         pBarrierOps);

    if ((syncToken.fenceVal != AcqRelFenceResetVal) && (syncToken.type == static_cast<uint32>(AcqRelEventType::Eop)))
    {
        const GfxCmdBufferState& cmdBufState = pCmdBuf->GetGfxCmdBufState();

        // If gfxBltExecEopFenceVal <= gfxBltWbEopFenceVal there hasn't been any newer BLT, meaning we could already
        // be tracking an EOP fence that writes back the CB/DB caches after the last BLT. So no need to update in
        // this case.
        if ((pBarrierOps->caches.flushCb         != 0) &&
            (pBarrierOps->caches.flushCbMetadata != 0) &&
            (pBarrierOps->caches.flushDb         != 0) &&
            (pBarrierOps->caches.flushDbMetadata != 0) &&
            (cmdBufState.fences.gfxBltExecEopFenceVal > cmdBufState.fences.gfxBltWbEopFenceVal))
        {
            pCmdBuf->UpdateGfxCmdBufGfxBltWbEopFence(syncToken.fenceVal);
        }
    }

    pCmdStream->CommitCommands(pCmdSpace);

    return syncToken;
}
#endif

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 648
// =====================================================================================================================
// Issue appropriate cache sync hardware commands to satisfy the cache acquire requirements.
// Note: The list of sync tokens cannot be cleared by this function and make it const.If multiple IssueAcquireSync are
//       called in the for loop, typically we'd think only the first call needs to effectively wait on the token(s).
//       After the first call the tokens have been waited and can be cleared.That's under wrong assumption that all the
//       IssueAcquireSync calls wait at same pipeline point, like PFP / ME. But in reality these syncs can end up wait
//       at different pipeline points. For example one wait at PFP while another wait at ME, or even later in the
//       pipeline. Every IssueAcquireSync needs to effectively wait on the list of release tokens.
void Device::IssueAcquireSync(
    GfxCmdBuffer*                 pCmdBuf,
    CmdStream*                    pCmdStream,
    uint32                        stageMask,
    uint32                        accessMask,
    bool                          invalidateLlc,
    gpusize                       rangeStartAddr,
    gpusize                       rangeSize,
    uint32                        syncTokenCount,
    const AcqRelSyncToken*        pSyncTokens,
    Developer::BarrierOperations* pBarrierOps
    ) const
{
    PAL_ASSERT(pBarrierOps != nullptr);
    PAL_ASSERT((syncTokenCount == 0) || (pSyncTokens != nullptr));

    const EngineType engineType     = pCmdBuf->GetEngineType();
    const bool       isGfxSupported = Pal::Device::EngineSupportsGraphics(engineType);

    if (isGfxSupported == false)
    {
        stageMask  &= ~GraphicsOnlyPipeStages;
        accessMask &= ~GraphicsOnlyCoherFlags;
    }

    // BuildWaitRegMem waits in the ME, if the waitPoint needs to stall at the PFP request a PFP/ME sync.
    const bool pfpSyncMe = TestAnyFlagSet(stageMask, PipelineStageTopOfPipe         |
                                                     PipelineStageFetchIndirectArgs |
                                                     PipelineStageFetchIndices);

    uint32* pCmdSpace = pCmdStream->ReserveCommands();

    uint32 syncTokenToWait[static_cast<uint32>(AcqRelEventType::Count)] = {};

    // Merge synchronization timestamp entries in the list.
    for (uint32 i = 0; i < syncTokenCount; i++)
    {
        const AcqRelSyncToken curSyncToken =  pSyncTokens[i];

        PAL_ASSERT(curSyncToken.type < static_cast<uint32>(AcqRelEventType::Count));

        if (curSyncToken.fenceVal > syncTokenToWait[curSyncToken.type])
        {
            syncTokenToWait[curSyncToken.type] = curSyncToken.fenceVal;
        }
    }

    if (syncTokenToWait[static_cast<uint32>(AcqRelEventType::Eop)] != AcqRelFenceResetVal)
    {
        // Issue wait on EOP.
        pCmdSpace += m_cmdUtil.BuildWaitRegMem(engineType,
                                               mem_space__me_wait_reg_mem__memory_space,
                                               function__me_wait_reg_mem__greater_than_or_equal_reference_value,
                                               engine_sel__me_wait_reg_mem__micro_engine,
                                               pCmdBuf->AcqRelFenceValGpuVa(AcqRelEventType::Eop),
                                               syncTokenToWait[static_cast<uint32>(AcqRelEventType::Eop)],
                                               0xFFFFFFFF,
                                               pCmdSpace);

        pBarrierOps->pipelineStalls.waitOnTs          = 1;
        pBarrierOps->pipelineStalls.eopTsBottomOfPipe = 1;

        pCmdBuf->SetPrevCmdBufInactive();
    }
    if (syncTokenToWait[static_cast<uint32>(AcqRelEventType::PsDone)] != AcqRelFenceResetVal)
    {
        // Issue wait on PS.
        pCmdSpace += m_cmdUtil.BuildWaitRegMem(engineType,
                                               mem_space__me_wait_reg_mem__memory_space,
                                               function__me_wait_reg_mem__greater_than_or_equal_reference_value,
                                               engine_sel__me_wait_reg_mem__micro_engine,
                                               pCmdBuf->AcqRelFenceValGpuVa(AcqRelEventType::PsDone),
                                               syncTokenToWait[static_cast<uint32>(AcqRelEventType::PsDone)],
                                               0xFFFFFFFF,
                                               pCmdSpace);

        pBarrierOps->pipelineStalls.waitOnTs    = 1;
        pBarrierOps->pipelineStalls.eosTsPsDone = 1;
    }
    if (syncTokenToWait[static_cast<uint32>(AcqRelEventType::CsDone)] != AcqRelFenceResetVal)
    {
        // Issue wait on CS.
        pCmdSpace += m_cmdUtil.BuildWaitRegMem(engineType,
                                               mem_space__me_wait_reg_mem__memory_space,
                                               function__me_wait_reg_mem__greater_than_or_equal_reference_value,
                                               engine_sel__me_wait_reg_mem__micro_engine,
                                               pCmdBuf->AcqRelFenceValGpuVa(AcqRelEventType::CsDone),
                                               syncTokenToWait[static_cast<uint32>(AcqRelEventType::CsDone)],
                                               0xFFFFFFFF,
                                               pCmdSpace);

        pBarrierOps->pipelineStalls.waitOnTs    = 1;
        pBarrierOps->pipelineStalls.eosTsCsDone = 1;
    }

    if (accessMask != 0)
    {
        // OptimizeBltCacheAccess() doesn't apply to acquire-sync. Acquire is for the future state, however the state
        // tracking mechanism tracks past operations.
        pCmdSpace += BuildAcquireSyncPackets(engineType,
                                             stageMask,
                                             accessMask,
                                             invalidateLlc,
                                             rangeStartAddr,
                                             rangeSize,
                                             pCmdSpace,
                                             pBarrierOps);
    }

    if (pfpSyncMe && isGfxSupported)
    {
        // Stalls the CP PFP until the ME has processed all previous commands.  Useful in cases where the ME is waiting
        // on some condition, but the PFP needs to stall execution until the condition is satisfied.  This must go last
        // otherwise the PFP could resume execution before the ME is done with all of its waits.
        pCmdSpace += m_cmdUtil.BuildPfpSyncMe(pCmdSpace);
        pBarrierOps->pipelineStalls.pfpSyncMe = 1;
    }

    const GfxCmdBufferState& cmdBufState       = pCmdBuf->GetGfxCmdBufState();
    const uint32             waitedEopFenceVal = syncTokenToWait[static_cast<uint32>(AcqRelEventType::Eop)];

    // If we have waited on a valid EOP fence value, update some CmdBufState (e.g. xxxBltActive) flags.
    if (waitedEopFenceVal != AcqRelFenceResetVal)
    {
        if (waitedEopFenceVal >= cmdBufState.fences.gfxBltExecEopFenceVal)
        {
            // An EOP release sync that is issued after the latest GFX BLT must have completed, so mark GFX BLT idle.
            pCmdBuf->SetGfxCmdBufGfxBltState(false);
        }

        if ((waitedEopFenceVal >= cmdBufState.fences.gfxBltWbEopFenceVal) &&
            (cmdBufState.fences.gfxBltWbEopFenceVal >= cmdBufState.fences.gfxBltExecEopFenceVal))
        {
            // An EOP release sync that issued GFX BLT cache flush must have completed, and there hasn't been any new
            // GFX BLT issued since that completed release sync, so mark GFX BLT cache clean.
            pCmdBuf->SetGfxCmdBufGfxBltWriteCacheState(false);
        }

        if (waitedEopFenceVal >= cmdBufState.fences.csBltExecEopFenceVal)
        {
            // An EOP release sync that is issued after the latest CS BLT must have completed, so mark CS BLT idle.
            pCmdBuf->SetGfxCmdBufCsBltState(false);
        }
    }

    pCmdStream->CommitCommands(pCmdSpace);
}
#endif

// =====================================================================================================================
// Figure out the specific BLT operation(s) necessary to convert a color image from one ImageLayout to another.
LayoutTransitionInfo Device::PrepareColorBlt(
    GfxCmdBuffer*      pCmdBuf,
    const Pal::Image&  image,
    const SubresRange& subresRange,
    ImageLayout        oldLayout,
    ImageLayout        newLayout
    ) const
{
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 642
    PAL_ASSERT(subresRange.numPlanes == 1);
#endif

    const GfxCmdBuffer*         pCmdBufConst   = pCmdBuf;
    auto&                       gfx9Image      = static_cast<Gfx9::Image&>(*image.GetGfxImage());
    const Gfx9::Image&          gfx9ImageConst = gfx9Image;
    const SubResourceInfo*const pSubresInfo    = image.SubresourceInfo(subresRange.startSubres);

    const ColorLayoutToState    layoutToState = gfx9ImageConst.LayoutToColorCompressionState();
    const ColorCompressionState oldState      =
        ImageLayoutToColorCompressionState(layoutToState, oldLayout);
    const ColorCompressionState newState      =
        ImageLayoutToColorCompressionState(layoutToState, newLayout);

    // Fast clear eliminates are only possible on universal queue command buffers and with valid fast clear eliminate
    // address otherwsie will be ignored on others. This should be okay because prior operations should be aware of
    // this fact (based on layout), and prohibit us from getting to a situation where one is needed but has not been
    // performed yet.
    const bool fastClearEliminateSupported =
        (pCmdBufConst->IsGraphicsSupported() &&
         (gfx9Image.GetFastClearEliminateMetaDataAddr(subresRange.startSubres) != 0));

    const bool isMsaaImage = (image.GetImageCreateInfo().samples > 1);

    LayoutTransitionInfo transitionInfo = {};

    if ((oldState != ColorDecompressed) && (newState == ColorDecompressed))
    {
        uint32 bltIndex = 0;

        if (gfx9ImageConst.HasDccData())
        {
            if ((oldState == ColorCompressed) || pSubresInfo->flags.supportMetaDataTexFetch)
            {
                transitionInfo.blt[bltIndex++] = HwLayoutTransition::DccDecompress;

                if (RsrcProcMgr().WillDecompressWithCompute(pCmdBufConst, gfx9ImageConst, subresRange))
                {
                    transitionInfo.flags.useComputePath = 1;
                }
            }
        }
        else if (isMsaaImage)
        {
            // Need FmaskDecompress in preparation for the following full MSAA color decompress.
            transitionInfo.blt[bltIndex++] = HwLayoutTransition::FmaskDecompress;
        }
        else
        {
            // Not Dcc image, nor Msaa image.
            PAL_ASSERT(oldState == ColorCompressed);

            if (fastClearEliminateSupported)
            {
                transitionInfo.blt[bltIndex++] = HwLayoutTransition::FastClearEliminate;
            }
        }

        // MsaaColorDecompress can be the only BLT, or following DccDecompress/FmaskDecompress/FastClearEliminate.
        if ((image.GetImageCreateInfo().samples > 1) && gfx9Image.HasFmaskData())
        {
            transitionInfo.blt[bltIndex] = HwLayoutTransition::MsaaColorDecompress;
        }
    }
    else if ((oldState == ColorCompressed) && (newState == ColorFmaskDecompressed))
    {
        PAL_ASSERT(isMsaaImage);
        if (pSubresInfo->flags.supportMetaDataTexFetch == false)
        {
            if (gfx9ImageConst.HasDccData())
            {
                // If the base pixel data is DCC compressed, but the image can't support metadata texture fetches,
                // we need a DCC decompress.  The DCC decompress effectively executes an fmask decompress
                // implicitly.
                transitionInfo.blt[0] = HwLayoutTransition::DccDecompress;

                if (RsrcProcMgr().WillDecompressWithCompute(pCmdBufConst, gfx9ImageConst, subresRange))
                {
                    transitionInfo.flags.useComputePath = 1;
                }
            }
            else
            {
                transitionInfo.blt[0] = HwLayoutTransition::FmaskDecompress;
            }
        }
        else
        {
            // if the image is TC compatible just need to do a fast clear eliminate
            if (fastClearEliminateSupported)
            {
                transitionInfo.blt[0] = HwLayoutTransition::FastClearEliminate;
            }
        }
    }
    else if ((oldState == ColorCompressed) && (newState == ColorCompressed) && (oldLayout.usages != newLayout.usages))
    {
        // This case indicates that the layout capabilities changed, but the color image is able to remain in
        // the compressed state.  If the image is about to be read, we may need to perform a fast clear
        // eliminate BLT if the clear color is not texture compatible.  This BLT will end up being skipped on
        // the GPU side if the latest clear color was supported by the texture hardware (i.e., black or white).
        constexpr uint32 TcCompatReadFlags = LayoutShaderRead           |
                                             LayoutShaderFmaskBasedRead |
                                             LayoutCopySrc              |
                                             LayoutSampleRate;

        // LayoutResolveSrc is treated as a color compressed state and if any decompression is required at resolve
        // time, @ref RsrcProcMgr::LateExpandResolveSrc will do the job.  So LayoutResolveSrc isn't added into
        // 'TcCompatReadFlags' above to skip performing a fast clear eliminate BLT.  If a shader resolve is to be
        // used, a barrier transiton to either LayoutShaderRead or LayoutShaderFmaskBasedRead is issued, which would
        // really trigger an FCE operation.
        if (fastClearEliminateSupported && TestAnyFlagSet(newLayout.usages, TcCompatReadFlags))
        {
            if (gfx9ImageConst.IsFceOptimizationEnabled() &&
                (gfx9ImageConst.HasSeenNonTcCompatibleClearColor() == false))
            {
                // Skip the fast clear eliminate for this image if the clear color is TC-compatible and the
                // optimization was enabled.
                Result result = pCmdBuf->AddFceSkippedImageCounter(&gfx9Image);

                if (result != Result::Success)
                {
                    // Fallback to performing the Fast clear eliminate if the above step of the optimization failed.
                    transitionInfo.blt[0] = HwLayoutTransition::FastClearEliminate;
                }
            }
            else
            {
                // The image has been fast cleared with a non-TC compatible color or the FCE optimization is not
                // enabled.
                transitionInfo.blt[0] = HwLayoutTransition::FastClearEliminate;
            }
        }
    }

    return transitionInfo;
}

// =====================================================================================================================
// Figure out the specific BLT operation(s) necessary to convert a depth/stencil image from one ImageLayout to another.
LayoutTransitionInfo Device::PrepareDepthStencilBlt(
    const GfxCmdBuffer* pCmdBuf,
    const Pal::Image&   image,
    const SubresRange&  subresRange,
    ImageLayout         oldLayout,
    ImageLayout         newLayout
    ) const
{
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 642
    PAL_ASSERT(subresRange.numPlanes == 1);
#endif

    const auto& gfx9Image = static_cast<const Image&>(*image.GetGfxImage());

    const DepthStencilLayoutToState    layoutToState =
        gfx9Image.LayoutToDepthCompressionState(subresRange.startSubres);
    const DepthStencilCompressionState oldState =
        ImageLayoutToDepthCompressionState(layoutToState, oldLayout);
    const DepthStencilCompressionState newState =
        ImageLayoutToDepthCompressionState(layoutToState, newLayout);

    LayoutTransitionInfo transitionInfo = {};

    if ((oldState == DepthStencilCompressed) && (newState != DepthStencilCompressed))
    {
        transitionInfo.blt[0] = HwLayoutTransition::ExpandDepthStencil;

        if (RsrcProcMgr().WillDecompressWithCompute(pCmdBuf, gfx9Image, subresRange))
        {
            transitionInfo.flags.useComputePath = 1;
        }
    }
    // Resummarize the htile values from the depth-stencil surface contents when transitioning from "HiZ invalid"
    // state to something that uses HiZ.
    else if ((oldState == DepthStencilDecomprNoHiZ) && (newState != DepthStencilDecomprNoHiZ))
    {
        // Use compute if:
        //   - We're on the compute engine
        //   - or we should force ExpandHiZRange for resummarize and we support compute operations
        //   - or we have a workaround which indicates if we need to use the compute path.
        const auto& createInfo = image.GetImageCreateInfo();
        const bool  z16Unorm1xAaDecompressUninitializedActive =
            (Settings().waZ16Unorm1xAaDecompressUninitialized &&
             (createInfo.samples == 1) &&
             ((createInfo.swizzledFormat.format == ChNumFormat::X16_Unorm) ||
              (createInfo.swizzledFormat.format == ChNumFormat::D16_Unorm_S8_Uint)));
        const bool  useCompute = ((pCmdBuf->GetEngineType() == EngineTypeCompute) ||
                                  (pCmdBuf->IsComputeSupported() &&
                                   (Pal::Image::ForceExpandHiZRangeForResummarize ||
                                    z16Unorm1xAaDecompressUninitializedActive)));
        if (useCompute)
        {
            // CS blit to open-up the HiZ range.
            transitionInfo.blt[0] = HwLayoutTransition::HwlExpandHtileHiZRange;
        }
        else
        {
            transitionInfo.blt[0] = HwLayoutTransition::ResummarizeDepthStencil;
        }
    }

    return transitionInfo;
}

// =====================================================================================================================
// Helper function that figures out what BLT transition is needed based on the image's old and new layout.
// Can only be called once before each layout transition.
LayoutTransitionInfo Device::PrepareBltInfo(
    GfxCmdBuffer*     pCmdBuf,
    const ImgBarrier& imgBarrier
    ) const
{
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 642
    PAL_ASSERT(imgBarrier.subresRange.numPlanes == 1);
#endif
    // At least one usage must be specified for the old and new layouts.
    PAL_ASSERT((imgBarrier.oldLayout.usages != 0) && (imgBarrier.newLayout.usages != 0));

    // With the exception of a transition out of the uninitialized state, at least one queue type must be
    // valid for every layout.
    PAL_ASSERT(((imgBarrier.oldLayout.usages == LayoutUninitializedTarget) ||
                (imgBarrier.oldLayout.engines != 0)) &&
                (imgBarrier.newLayout.engines != 0));

    PAL_ASSERT(imgBarrier.pImage != nullptr);

    const ImageLayout oldLayout   = imgBarrier.oldLayout;
    const ImageLayout newLayout   = imgBarrier.newLayout;
    const auto&       image       = static_cast<const Pal::Image&>(*imgBarrier.pImage);
    const auto&       subresRange = imgBarrier.subresRange;

    LayoutTransitionInfo layoutTransInfo = {};

    if (TestAnyFlagSet(oldLayout.usages, LayoutUninitializedTarget))
    {
        // If the LayoutUninitializedTarget usage is set, no other usages should be set.
        PAL_ASSERT(TestAnyFlagSet(oldLayout.usages, ~LayoutUninitializedTarget) == false);

        const auto& gfx9Image   = static_cast<const Gfx9::Image&>(*image.GetGfxImage());

#if PAL_ENABLE_PRINTS_ASSERTS
        const auto& engineProps = Parent()->EngineProperties().perEngine[pCmdBuf->GetEngineType()];
        const auto& createInfo  = image.GetImageCreateInfo();
        const bool  isFullPlane = image.IsRangeFullPlane(subresRange);

        // This queue must support this barrier transition.
        PAL_ASSERT(engineProps.flags.supportsImageInitBarrier == 1);

        // By default, the entire plane must be initialized in one go. Per-subres support can be requested
        // using an image flag as long as the queue supports it.
        PAL_ASSERT(isFullPlane || ((engineProps.flags.supportsImageInitPerSubresource == 1) &&
                                   (createInfo.flags.perSubresInit == 1)));
#endif

        if (gfx9Image.HasColorMetaData() || gfx9Image.HasHtileData())
        {
            layoutTransInfo.blt[0] = HwLayoutTransition::InitMaskRam;
        }
    }
    else if (TestAnyFlagSet(newLayout.usages, LayoutUninitializedTarget))
    {
        // If the LayoutUninitializedTarget usage is set, no other usages should be set.
        PAL_ASSERT(TestAnyFlagSet(newLayout.usages, ~LayoutUninitializedTarget) == false);

        // We do no decompresses, expands, or any other kind of blt in this case.
    }
    else if ((TestAnyFlagSet(oldLayout.usages, LayoutUninitializedTarget) == false) &&
             (TestAnyFlagSet(newLayout.usages, LayoutUninitializedTarget) == false))
    {
        // Call helper function to calculate specific BLT operation(s) (can be none) for an image layout transition.
        if (image.IsDepthStencilTarget())
        {
            layoutTransInfo = PrepareDepthStencilBlt(pCmdBuf, image, subresRange, oldLayout, newLayout);
        }
        else
        {
            layoutTransInfo = PrepareColorBlt(pCmdBuf, image, subresRange, oldLayout, newLayout);
        }
    }

    return layoutTransInfo;
}

// =====================================================================================================================
//  We will need flush & inv L2 on MSAA Z, MSAA color, mips in the metadata tail, or any stencil.
//
// The driver assumes that all meta-data surfaces are pipe-aligned, but there are cases where the HW does not actually
// pipe-align the data.  In these cases, the L2 cache needs to be flushed prior to the metadata being read by a shader.
static bool WaRefreshTccToAlignMetadata(
    const IImage*     pImage,
    const SubresRange subresRange,
    uint32            srcAccessMask,
    uint32            dstAccessMask)
{
    const auto& palImage  = static_cast<const Pal::Image&>(*pImage);
    const auto& gfx9Image = static_cast<const Image&>(*palImage.GetGfxImage());

    bool needRefreshL2 = false;

    if (gfx9Image.NeedFlushForMetadataPipeMisalignment(subresRange))
    {
        // Because we are not able to convert CoherCopy, CoherClear, CoherResolve to specific frontend or backend
        // coherency flags, we cannot make accurate decision here. This code works hard to not over-sync too much.
        constexpr uint32 UncertainCoherMask = CoherCopy | CoherResolve | CoherClear;
        constexpr uint32 MaybeTextureCache  = UncertainCoherMask | CoherShader | CoherSampleRate;
        constexpr uint32 MaybeFixedFunction = UncertainCoherMask | CoherColorTarget | CoherDepthStencilTarget;

        if ((TestAnyFlagSet(srcAccessMask, MaybeFixedFunction) && TestAnyFlagSet(dstAccessMask, MaybeTextureCache)) ||
            (TestAnyFlagSet(srcAccessMask, MaybeTextureCache) && TestAnyFlagSet(dstAccessMask, MaybeFixedFunction)))
        {
            needRefreshL2 = true;
        }
    }

    return needRefreshL2;
}

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 648
// =====================================================================================================================
// BarrierRelease perform any necessary layout transition, availability operation, and enqueue command(s) to set a given
// IGpuEvent object once the prior operations' intersection with the given synchronization scope is confirmed complete.
// The availability operation will flush the requested local caches.
AcqRelSyncToken Device::BarrierRelease(
    GfxCmdBuffer*                 pCmdBuf,
    CmdStream*                    pCmdStream,
    const AcquireReleaseInfo&     barrierReleaseInfo,
    Developer::BarrierOperations* pBarrierOps,
    bool                          waMetaMisalignNeedRefreshLlc
    ) const
{
    // Validate input data.
    PAL_ASSERT(barrierReleaseInfo.dstStageMask == 0);
    PAL_ASSERT(barrierReleaseInfo.dstGlobalAccessMask == 0);
    for (uint32 i = 0; i < barrierReleaseInfo.memoryBarrierCount; i++)
    {
        PAL_ASSERT(barrierReleaseInfo.pMemoryBarriers[i].dstAccessMask == 0);
    }
    for (uint32 i = 0; i < barrierReleaseInfo.imageBarrierCount; i++)
    {
        PAL_ASSERT(barrierReleaseInfo.pImageBarriers[i].dstAccessMask == 0);
    }

    const uint32 preBltStageMask   = barrierReleaseInfo.srcStageMask;
    uint32       preBltAccessMask  = barrierReleaseInfo.srcGlobalAccessMask;
    bool         globallyAvailable = waMetaMisalignNeedRefreshLlc;

    // Assumes always do full-range flush sync.
    for (uint32 i = 0; i < barrierReleaseInfo.memoryBarrierCount; i++)
    {
        const MemBarrier& barrier = barrierReleaseInfo.pMemoryBarriers[i];

        preBltAccessMask  |= barrier.srcAccessMask;
        globallyAvailable |= barrier.flags.globallyAvailable;
    }

    // A container to cache the calculated BLT transitions and some cache info for reuse.
    AutoBuffer<AcqRelTransitionInfo, 8, Platform> transitionList(barrierReleaseInfo.imageBarrierCount, GetPlatform());
    uint32 bltTransitionCount = 0;

    AcqRelSyncToken syncToken = {};

    Result result = Result::Success;

    if (transitionList.Capacity() < barrierReleaseInfo.imageBarrierCount)
    {
        pCmdBuf->NotifyAllocFailure();
    }
    else
    {
        // Loop through image transitions to update client requested access.
        for (uint32 i = 0; i < barrierReleaseInfo.imageBarrierCount; i++)
        {
            const ImgBarrier& imageBarrier = barrierReleaseInfo.pImageBarriers[i];

            PAL_ASSERT(imageBarrier.subresRange.numPlanes == 1);

            // Update client requested access mask.
            preBltAccessMask |= imageBarrier.srcAccessMask;

            // Prepare a layout transition BLT info and do pre-BLT preparation work.
            LayoutTransitionInfo layoutTransInfo = PrepareBltInfo(pCmdBuf, imageBarrier);

            transitionList[i].pImgBarrier      = &imageBarrier;
            transitionList[i].layoutTransInfo  = layoutTransInfo;
            transitionList[i].waMetaMisalignNeedRefreshLlc = waMetaMisalignNeedRefreshLlc;

            uint32 bltStageMask  = 0;
            uint32 bltAccessMask = 0;

            if (layoutTransInfo.blt[0] != HwLayoutTransition::None)
            {
                GetBltStageAccessInfo(layoutTransInfo, &bltStageMask, &bltAccessMask);

                transitionList[i].bltStageMask  = bltStageMask;
                transitionList[i].bltAccessMask = bltAccessMask;
                bltTransitionCount++;
            }
            else
            {
                transitionList[i].bltStageMask  = 0;
                transitionList[i].bltAccessMask = 0;
            }

            if (WaRefreshTccToAlignMetadata(imageBarrier.pImage,
                                            imageBarrier.subresRange,
                                            imageBarrier.srcAccessMask,
                                            bltAccessMask))
            {
                transitionList[i].waMetaMisalignNeedRefreshLlc = true;
                globallyAvailable |= true;
            }
        }

        // Perform an all-in-one release prior to the potential BLT(s).
        syncToken = IssueReleaseSync(pCmdBuf,
                                     pCmdStream,
                                     preBltStageMask,
                                     preBltAccessMask,
                                     globallyAvailable,
                                     pBarrierOps);

        // Issue BLT(s) if there exists transitions that require one.
        if (bltTransitionCount > 0)
        {
            // If BLT(s) will be issued, we need to know how to release from it/them.
            uint32 postBltStageMask  = 0;
            uint32 postBltAccessMask = 0;

            // Issue pre-BLT acquires.
            for (uint32 i = 0; i < barrierReleaseInfo.imageBarrierCount; i++)
            {
                const AcqRelTransitionInfo& transition = transitionList[i];

                if (transition.layoutTransInfo.blt[0] != HwLayoutTransition::None)
                {
                    const auto& image = static_cast<const Pal::Image&>(*transition.pImgBarrier->pImage);

                    IssueAcquireSync(pCmdBuf,
                                     pCmdStream,
                                     transition.bltStageMask,
                                     transition.bltAccessMask,
                                     transition.waMetaMisalignNeedRefreshLlc,
                                     image.GetGpuVirtualAddr(),
                                     image.GetGpuMemSize(),
                                     1,
                                     &syncToken,
                                     pBarrierOps);
                }
            }

            // Issue BLTs.
            for (uint32 i = 0; i < barrierReleaseInfo.imageBarrierCount; i++)
            {
                const AcqRelTransitionInfo& transition = transitionList[i];

                if (transition.layoutTransInfo.blt[0] != HwLayoutTransition::None)
                {
                    IssueBlt(pCmdBuf,
                             pCmdStream,
                             transition.pImgBarrier,
                             transition.layoutTransInfo,
                             pBarrierOps);

                    uint32 stageMask  = 0;
                    uint32 accessMask = 0;

                    if (transition.layoutTransInfo.blt[1] != HwLayoutTransition::None)
                    {
                        PAL_ASSERT(transition.layoutTransInfo.blt[1] == HwLayoutTransition::MsaaColorDecompress);
                        constexpr LayoutTransitionInfo MsaaBltInfo = { {}, HwLayoutTransition::MsaaColorDecompress};
                        GetBltStageAccessInfo(MsaaBltInfo, &stageMask, &accessMask);
                    }
                    else
                    {
                        stageMask  = transition.bltStageMask;
                        accessMask = transition.bltAccessMask;
                    }

                    // Add current BLT's stageMask/accessMask into a stageMask/accessMask used for an all-in-one
                    // post-BLT release.
                    postBltStageMask  |= stageMask;
                    postBltAccessMask |= accessMask;
                }
            }

            // Release from BLTs.
            syncToken = IssueReleaseSync(pCmdBuf,
                                         pCmdStream,
                                         postBltStageMask,
                                         postBltAccessMask,
                                         globallyAvailable,
                                         pBarrierOps);
        }
    }

    return syncToken;
}
#endif

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 648
// =====================================================================================================================
// BarrierAcquire will wait on the specified IGpuEvent object to be signaled, perform any necessary layout transition,
// and issue the required visibility operations. The visibility operation will invalidate the required ranges in local
// caches.
void Device::BarrierAcquire(
    GfxCmdBuffer*                 pCmdBuf,
    CmdStream*                    pCmdStream,
    const AcquireReleaseInfo&     barrierAcquireInfo,
    uint32                        syncTokenCount,
    const AcqRelSyncToken*        pSyncTokens,
    Developer::BarrierOperations* pBarrierOps,
    bool                          waMetaMisalignNeedRefreshLlc
    ) const
{
    // Validate input data.
    PAL_ASSERT(barrierAcquireInfo.srcStageMask == 0);
    PAL_ASSERT(barrierAcquireInfo.srcGlobalAccessMask == 0);
    for (uint32 i = 0; i < barrierAcquireInfo.memoryBarrierCount; i++)
    {
        PAL_ASSERT(barrierAcquireInfo.pMemoryBarriers[i].srcAccessMask == 0);
    }
    for (uint32 i = 0; i < barrierAcquireInfo.imageBarrierCount; i++)
    {
        PAL_ASSERT(barrierAcquireInfo.pImageBarriers[i].srcAccessMask == 0);
    }

    bool globallyAvailable = waMetaMisalignNeedRefreshLlc;

    // A container to cache the calculated BLT transitions and some cache info for reuse.
    AutoBuffer<AcqRelTransitionInfo, 8, Platform> transitionList(barrierAcquireInfo.imageBarrierCount, GetPlatform());
    uint32 bltTransitionCount = 0;

    if (transitionList.Capacity() < barrierAcquireInfo.imageBarrierCount)
    {
        pCmdBuf->NotifyAllocFailure();
    }
    else
    {
        // Acquire for BLTs.
        for (uint32 i = 0; i < barrierAcquireInfo.imageBarrierCount; i++)
        {
            const auto& imgBarrier = barrierAcquireInfo.pImageBarriers[i];
            PAL_ASSERT(imgBarrier.subresRange.numPlanes == 1);

            // Prepare a layout transition BLT info and do pre-BLT preparation work.
            const LayoutTransitionInfo layoutTransInfo = PrepareBltInfo(pCmdBuf, imgBarrier);

            transitionList[i].pImgBarrier      = &imgBarrier;
            transitionList[i].layoutTransInfo  = layoutTransInfo;
            transitionList[i].waMetaMisalignNeedRefreshLlc = waMetaMisalignNeedRefreshLlc;

            uint32 bltStageMask  = 0;
            uint32 bltAccessMask = 0;

            if (layoutTransInfo.blt[0] != HwLayoutTransition::None)
            {
                GetBltStageAccessInfo(layoutTransInfo, &bltStageMask, &bltAccessMask);

                transitionList[i].bltStageMask  = bltStageMask;
                transitionList[i].bltAccessMask = bltAccessMask;
                bltTransitionCount++;

                if (WaRefreshTccToAlignMetadata(imgBarrier.pImage,
                                                imgBarrier.subresRange,
                                                bltAccessMask,
                                                imgBarrier.dstAccessMask))
                {
                    transitionList[i].waMetaMisalignNeedRefreshLlc = true;
                    globallyAvailable = true;
                }
            }
            else
            {
                transitionList[i].bltStageMask  = 0;
                transitionList[i].bltAccessMask = 0;
            }
        }

        const AcqRelSyncToken* pCurSyncTokens = pSyncTokens;

        // A new sync token may be generated by internal BLT.
        AcqRelSyncToken bltSyncToken = {};

        if (bltTransitionCount > 0)
        {
            // If BLT(s) will be issued, we need to know how to release from it/them.
            uint32 postBltStageMask  = 0;
            uint32 postBltAccessMask = 0;

            // Issue pre-BLT acquires.
            for (uint32 i = 0; i < barrierAcquireInfo.imageBarrierCount; i++)
            {
                const AcqRelTransitionInfo& transition = transitionList[i];

                if (transition.layoutTransInfo.blt[0] != HwLayoutTransition::None)
                {
                    const auto& image = static_cast<const Pal::Image&>(*transitionList[i].pImgBarrier->pImage);

                    IssueAcquireSync(pCmdBuf,
                                     pCmdStream,
                                     transition.bltStageMask,
                                     transition.bltAccessMask,
                                     transition.waMetaMisalignNeedRefreshLlc,
                                     image.GetGpuVirtualAddr(),
                                     image.GetGpuMemSize(),
                                     syncTokenCount,
                                     pCurSyncTokens,
                                     pBarrierOps);
                }
            }

            // Issue BLTs.
            for (uint32 i = 0; i < barrierAcquireInfo.imageBarrierCount; i++)
            {
                const AcqRelTransitionInfo& transition = transitionList[i];

                if (transition.layoutTransInfo.blt[0] != HwLayoutTransition::None)
                {
                    IssueBlt(pCmdBuf,
                             pCmdStream,
                             transition.pImgBarrier,
                             transition.layoutTransInfo,
                             pBarrierOps);

                    uint32 stageMask  = 0;
                    uint32 accessMask = 0;

                    if (transition.layoutTransInfo.blt[1] != HwLayoutTransition::None)
                    {
                        PAL_ASSERT(transition.layoutTransInfo.blt[1] == HwLayoutTransition::MsaaColorDecompress);
                        constexpr LayoutTransitionInfo MsaaBltInfo = { {}, HwLayoutTransition::MsaaColorDecompress };
                        GetBltStageAccessInfo(MsaaBltInfo, &stageMask, &accessMask);
                    }
                    else
                    {
                        stageMask  = transition.bltStageMask;
                        accessMask = transition.bltAccessMask;
                    }

                    // Add current BLT's stageMask/accessMask into a stageMask/accessMask used for an all-in-one
                    // post-BLT release.
                    postBltStageMask  |= stageMask;
                    postBltAccessMask |= accessMask;
                }
            }

            // Release from BLTs.
            bltSyncToken = IssueReleaseSync(pCmdBuf,
                                            pCmdStream,
                                            postBltStageMask,
                                            postBltAccessMask,
                                            globallyAvailable,
                                            pBarrierOps);

            syncTokenCount = 1;
            pCurSyncTokens = &bltSyncToken;
        }

        // Issue acquire for client requested global cache sync.
        IssueAcquireSync(pCmdBuf,
                         pCmdStream,
                         barrierAcquireInfo.dstStageMask,
                         barrierAcquireInfo.dstGlobalAccessMask,
                         false,
                         FullSyncBaseAddr,
                         FullSyncSize,
                         syncTokenCount,
                         pCurSyncTokens,
                         pBarrierOps);

        // Loop through memory transitions to issue client-requested acquires for ranged memory syncs.
        for (uint32 i = 0; i < barrierAcquireInfo.memoryBarrierCount; i++)
        {
            const MemBarrier& barrier              = barrierAcquireInfo.pMemoryBarriers[i];
            const GpuMemSubAllocInfo& memAllocInfo = barrier.memory;

            const uint32  acquireAccessMask  = barrier.dstAccessMask;
            const gpusize rangedSyncBaseAddr = memAllocInfo.pGpuMemory->Desc().gpuVirtAddr + memAllocInfo.offset;
            const gpusize rangedSyncSize     = memAllocInfo.size;

            IssueAcquireSync(pCmdBuf,
                             pCmdStream,
                             barrierAcquireInfo.dstStageMask,
                             acquireAccessMask,
                             false,
                             rangedSyncBaseAddr,
                             rangedSyncSize,
                             0,
                             nullptr,
                             pBarrierOps);
        }

        // Loop through memory transitions to issue client-requested acquires for image syncs.
        for (uint32 i = 0; i < barrierAcquireInfo.imageBarrierCount; i++)
        {
            const ImgBarrier& imgBarrier = barrierAcquireInfo.pImageBarriers[i];
            PAL_ASSERT(imgBarrier.subresRange.numPlanes == 1);
            const Pal::Image& image      = static_cast<const Pal::Image&>(*imgBarrier.pImage);

            IssueAcquireSync(pCmdBuf,
                             pCmdStream,
                             barrierAcquireInfo.dstStageMask,
                             imgBarrier.dstAccessMask,
                             transitionList[i].waMetaMisalignNeedRefreshLlc,
                             image.GetGpuVirtualAddr(),
                             image.GetGpuMemSize(),
                             0,
                             nullptr,
                             pBarrierOps);
        }
    }
}
#endif

// =====================================================================================================================
// BarrierReleaseThenAcquire is effectively the same as calling BarrierRelease immediately by calling BarrierAcquire.
// This is a convenience method for clients implementing single point barriers, and is functionally equivalent to the
// current CmdBarrier() interface.
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 648
// The BarrierReleaseand BarrierAcquire calls are associated by a sync token.
#else
// The BarrierReleaseand BarrierAcquire calls are associated by an internal GPU event.
#endif
void Device::BarrierReleaseThenAcquire(
    GfxCmdBuffer*                 pCmdBuf,
    CmdStream*                    pCmdStream,
    const AcquireReleaseInfo&     barrierInfo,
    Developer::BarrierOperations* pBarrierOps
    ) const
{
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 648
    // Internal event per command buffer is used for ReleaseThenAcquire case. All release/acquire-based barriers in the
    // same command buffer use the same event.
    const IGpuEvent* pEvent = pCmdBuf->GetInternalEvent();
#endif

    Result result = Result::Success;

    AutoBuffer<MemBarrier, 8, Platform> memBarriers(barrierInfo.memoryBarrierCount, GetPlatform());
    if (barrierInfo.memoryBarrierCount > 0)
    {
        if (memBarriers.Capacity() < barrierInfo.memoryBarrierCount)
        {
            pCmdBuf->NotifyAllocFailure();
        }
        else
        {
            for (uint32 i = 0; i < barrierInfo.memoryBarrierCount; i++)
            {
                memBarriers[i].memory        = barrierInfo.pMemoryBarriers[i].memory;
                memBarriers[i].srcAccessMask = barrierInfo.pMemoryBarriers[i].srcAccessMask;
                memBarriers[i].dstAccessMask = 0;
            }
        }
    }

    bool waMetaMisalignNeedRefreshLlc = false;

    AutoBuffer<ImgBarrier, 8, Platform> imgBarriers(barrierInfo.imageBarrierCount, GetPlatform());
    if ((result==Result::Success) && (barrierInfo.imageBarrierCount > 0))
    {
        if (imgBarriers.Capacity() < barrierInfo.imageBarrierCount)
        {
            pCmdBuf->NotifyAllocFailure();
        }
        else
        {
            for (uint32 i = 0; i < barrierInfo.imageBarrierCount; i++)
            {
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 642
                PAL_ASSERT(barrierInfo.pImageBarriers[i].subresRange.numPlanes == 1);
#endif
                imgBarriers[i].pImage             = barrierInfo.pImageBarriers[i].pImage;
                imgBarriers[i].subresRange        = barrierInfo.pImageBarriers[i].subresRange;
                imgBarriers[i].box                = barrierInfo.pImageBarriers[i].box;
                imgBarriers[i].srcAccessMask      = barrierInfo.pImageBarriers[i].srcAccessMask;
                imgBarriers[i].dstAccessMask      = 0;
                imgBarriers[i].oldLayout          = barrierInfo.pImageBarriers[i].oldLayout;
                imgBarriers[i].newLayout          = barrierInfo.pImageBarriers[i].newLayout; // Do decompress in release.
                imgBarriers[i].pQuadSamplePattern = barrierInfo.pImageBarriers[i].pQuadSamplePattern;

                // Only at this point we know both source and destination cache mask.
                if (WaRefreshTccToAlignMetadata(barrierInfo.pImageBarriers[i].pImage,
                                                barrierInfo.pImageBarriers[i].subresRange,
                                                barrierInfo.pImageBarriers[i].srcAccessMask,
                                                barrierInfo.pImageBarriers[i].dstAccessMask))
                {
                    waMetaMisalignNeedRefreshLlc = true;
                }
            }
        }
    }

    // Build BarrierRelease function.
    AcquireReleaseInfo releaseInfo;
    releaseInfo.srcStageMask        = barrierInfo.srcStageMask;
    releaseInfo.srcGlobalAccessMask = barrierInfo.srcGlobalAccessMask;
    releaseInfo.dstStageMask        = 0;
    releaseInfo.dstGlobalAccessMask = 0;
    releaseInfo.memoryBarrierCount  = barrierInfo.memoryBarrierCount;
    releaseInfo.pMemoryBarriers     = &memBarriers[0];
    releaseInfo.imageBarrierCount   = barrierInfo.imageBarrierCount;
    releaseInfo.pImageBarriers      = &imgBarriers[0];

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 648
    const AcqRelSyncToken syncToken = BarrierRelease(pCmdBuf,
                                                     pCmdStream,
                                                     releaseInfo,
                                                     pBarrierOps,
                                                     waMetaMisalignNeedRefreshLlc);
#else
    BarrierReleaseEvent(pCmdBuf, pCmdStream, releaseInfo, pEvent, pBarrierOps, waMetaMisalignNeedRefreshLlc);
#endif

    // Build BarrierAcquire function.
    AcquireReleaseInfo acquireInfo;
    acquireInfo.srcStageMask        = 0;
    acquireInfo.srcGlobalAccessMask = 0;
    acquireInfo.dstStageMask        = barrierInfo.dstStageMask;
    acquireInfo.dstGlobalAccessMask = barrierInfo.dstGlobalAccessMask;

    acquireInfo.memoryBarrierCount  = barrierInfo.memoryBarrierCount;
    for (uint32 i = 0; i < barrierInfo.memoryBarrierCount; i++)
    {
        memBarriers[i].srcAccessMask = 0;
        memBarriers[i].dstAccessMask = barrierInfo.pMemoryBarriers[i].dstAccessMask;
    }
    acquireInfo.pMemoryBarriers = &memBarriers[0];

    acquireInfo.imageBarrierCount = barrierInfo.imageBarrierCount;
    for (uint32 i = 0; i < barrierInfo.imageBarrierCount; i++)
    {
        imgBarriers[i].srcAccessMask = 0;
        imgBarriers[i].dstAccessMask = barrierInfo.pImageBarriers[i].dstAccessMask;
        imgBarriers[i].oldLayout     = barrierInfo.pImageBarriers[i].newLayout;
        imgBarriers[i].newLayout     = barrierInfo.pImageBarriers[i].newLayout;
    }
    acquireInfo.pImageBarriers = &imgBarriers[0];

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 648
    BarrierAcquire(pCmdBuf, pCmdStream, acquireInfo, 1, &syncToken, pBarrierOps, waMetaMisalignNeedRefreshLlc);
#else
    BarrierAcquireEvent(pCmdBuf, pCmdStream, acquireInfo, 1, &pEvent, pBarrierOps, waMetaMisalignNeedRefreshLlc);
#endif
}

// =====================================================================================================================
// Helper function that issues requested transition for the image barrier.
void Device::IssueBlt(
    GfxCmdBuffer*                 pCmdBuf,
    CmdStream*                    pCmdStream,
    const ImgBarrier*             pImgBarrier,
    LayoutTransitionInfo          layoutTransInfo,
    Developer::BarrierOperations* pBarrierOps
    ) const
{
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 642
    PAL_ASSERT(pImgBarrier->subresRange.numPlanes == 1);
#endif
    PAL_ASSERT(pImgBarrier != nullptr);
    PAL_ASSERT(layoutTransInfo.blt[0] != HwLayoutTransition::None);
    PAL_ASSERT(pBarrierOps != nullptr);

    // Tell RGP about this transition
    BarrierTransition rgpTransition = AcqRelBuildTransition(pImgBarrier, layoutTransInfo, pBarrierOps);
    DescribeBarrier(pCmdBuf, &rgpTransition, pBarrierOps);

    // And clear it so it can differentiate sync and async flushes
    *pBarrierOps = {};

    const auto& image = static_cast<const Pal::Image&>(*pImgBarrier->pImage);

    if (layoutTransInfo.blt[0] == HwLayoutTransition::InitMaskRam)
    {
        // Transition out of LayoutUninitializedTarget needs to initialize metadata memories.
        AcqRelInitMaskRam(pCmdBuf, pCmdStream, *pImgBarrier);
    }
    else
    {
        // Image does normal BLT.
        if (image.IsDepthStencilTarget())
        {
            AcqRelDepthStencilTransition(pCmdBuf, *pImgBarrier, layoutTransInfo);
        }
        else
        {

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 648
            AcqRelColorTransition(pCmdBuf, pCmdStream, *pImgBarrier, layoutTransInfo, pBarrierOps);
#else
            AcqRelColorTransitionEvent(pCmdBuf, pCmdStream, *pImgBarrier, layoutTransInfo, pBarrierOps);
#endif
        }
    }
}

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 648
// =====================================================================================================================
// Build the necessary packets to fulfill the requested cache sync for release.
size_t Device::BuildReleaseSyncPackets(
    GfxCmdBuffer*                 pCmdBuf,
    EngineType                    engineType,
    uint32                        stageMask,
    uint32                        accessMask,
    bool                          flushLlc,
    AcqRelSyncToken*              pSyncTokenOut,
    void*                         pBuffer,
    Developer::BarrierOperations* pBarrierOps
    ) const
{
    // OptimizeBltCacheAccess() should've been called to convert these BLT coherency flags to more specific ones.
    PAL_ASSERT(TestAnyFlagSet(accessMask, CoherCopy | CoherResolve | CoherClear) == false);

    PAL_ASSERT(pSyncTokenOut != nullptr);

    PAL_ASSERT(pBarrierOps != nullptr);

    bool rbCacheSync = false;

    const bool requiresEop    = TestAnyFlagSet(stageMask, PipelineStageEarlyDsTarget |
                                                          PipelineStageLateDsTarget  |
                                                          PipelineStageColorTarget   |
                                                          PipelineStageBottomOfPipe);
    const bool requiresVsDone = TestAnyFlagSet(stageMask, PipelineStageVs |
                                                          PipelineStageHs |
                                                          PipelineStageDs |
                                                          PipelineStageGs);
    const bool requiresPsDone = TestAnyFlagSet(stageMask, PipelineStagePs);
    const bool requiresCsDone = TestAnyFlagSet(stageMask, PipelineStageCs);

    // If any of the access mask bits that could result in RB sync are set, use CACHE_FLUSH_AND_INV_TS.
    // There is no way to INV the CB metadata caches during acquire. So at release always also invalidate if we are to
    // flush CB metadata. Furthermore, CACHE_FLUSH_AND_INV_TS_EVENT always flush & invalidate RB, so there is no need
    // to invalidate RB at acquire again.
    if (TestAnyFlagSet(accessMask, CoherColorTarget | CoherDepthStencilTarget))
    {
        // Issue a pipelined EOP event that writes timestamp to a GpuEvent slot when all prior GPU work completes.

        rbCacheSync = true;

        pSyncTokenOut->type     = static_cast<uint32>(AcqRelEventType::Eop);
        pSyncTokenOut->fenceVal = pCmdBuf->GetNextAcqRelFenceVal(AcqRelEventType::Eop);

        // Clear up CB/DB request
        accessMask &= ~(CoherColorTarget | CoherDepthStencilTarget);

        pBarrierOps->caches.flushCb         = 1;
        pBarrierOps->caches.invalCb         = 1;
        pBarrierOps->caches.flushCbMetadata = 1;
        pBarrierOps->caches.invalCbMetadata = 1;

        pBarrierOps->caches.flushDb         = 1;
        pBarrierOps->caches.invalDb         = 1;
        pBarrierOps->caches.flushDbMetadata = 1;
        pBarrierOps->caches.invalDbMetadata = 1;
    }

    else if (requiresEop || (requiresCsDone && requiresPsDone) || requiresVsDone)
    {
        // Implement set with an EOP event written when all prior GPU work completes if either:
        // 1. End of RB or end of whole pipe is required,
        // 2. Both graphics and compute events are required,
        // 3. Graphics event required but no ps wave. In this case PS_DONE doesn't safely ensures VS waves completion.
        //    And because there is no VS_DONE event, we have to conservatively use an EOP event.
        pSyncTokenOut->type     = static_cast<uint32>(AcqRelEventType::Eop);
        pSyncTokenOut->fenceVal = pCmdBuf->GetNextAcqRelFenceVal(AcqRelEventType::Eop);
    }
    else if (requiresCsDone)
    {
        // Implement set with an EOS event waiting for CS waves to complete.
        pSyncTokenOut->type     = static_cast<uint32>(AcqRelEventType::CsDone);
        pSyncTokenOut->fenceVal = pCmdBuf->GetNextAcqRelFenceVal(AcqRelEventType::CsDone);
    }
    else if (requiresPsDone)
    {
        // Implement set with an EOS event waiting for PS waves to complete.
        pSyncTokenOut->type     = static_cast<uint32>(AcqRelEventType::PsDone);
        pSyncTokenOut->fenceVal = pCmdBuf->GetNextAcqRelFenceVal(AcqRelEventType::PsDone);
    }

    // Create info for RELEASE_MEM. Initialize common part at here.
    ExplicitReleaseMemInfo releaseMemInfo = {};
    releaseMemInfo.engineType = engineType;

    if (m_gfxIpLevel == GfxIpLevel::GfxIp9)
    {
        uint32 cacheSyncFlags = Gfx9ConvertToReleaseSyncFlags(accessMask, flushLlc, pBarrierOps);

        const uint32 tcCacheOp = static_cast<uint32>(SelectTcCacheOp(&cacheSyncFlags));

        // The cache sync requests can be cleared by single release pass.
        PAL_ASSERT(cacheSyncFlags == 0);

        releaseMemInfo.coherCntl = Gfx9TcCacheOpConversionTable[tcCacheOp];
    }
    else if (IsGfx10(m_gfxIpLevel))
    {
        releaseMemInfo.gcrCntl = Gfx10BuildReleaseGcrCntl(accessMask, flushLlc, pBarrierOps);
    }

    bool issueReleaseSync = (pSyncTokenOut->fenceVal != AcqRelFenceResetVal);

    // If we have cache sync request yet don't assign any VGT event, we need to issue a dummy one.
    if ((issueReleaseSync == false) && ((releaseMemInfo.coherCntl != 0) || (releaseMemInfo.gcrCntl != 0)))
    {
        // Flush at earliest supported pipe point for RELEASE_MEM (CS_DONE always works).
        pSyncTokenOut->type     = static_cast<uint32>(AcqRelEventType::CsDone);
        pSyncTokenOut->fenceVal = pCmdBuf->GetNextAcqRelFenceVal(AcqRelEventType::CsDone);

        issueReleaseSync = true;
    }

    // Issue RELEASE_MEM packets to flush caches (optional) and write timestamp to fenceVal.
    size_t dwordsWritten = 0;

    if (issueReleaseSync)
    {
        releaseMemInfo.dstAddr = pCmdBuf->AcqRelFenceValGpuVa(static_cast<AcqRelEventType>(pSyncTokenOut->type));
        releaseMemInfo.dataSel = data_sel__me_release_mem__send_32_bit_low;
        releaseMemInfo.data    = pSyncTokenOut->fenceVal;

        if (pSyncTokenOut->type == static_cast<uint32>(AcqRelEventType::Eop))
        {
            releaseMemInfo.vgtEvent = rbCacheSync ? CACHE_FLUSH_AND_INV_TS_EVENT : BOTTOM_OF_PIPE_TS;

            pBarrierOps->pipelineStalls.eopTsBottomOfPipe = 1;
        }
        else if (pSyncTokenOut->type == static_cast<uint32>(AcqRelEventType::PsDone))
        {
            releaseMemInfo.vgtEvent = PS_DONE;

            pBarrierOps->pipelineStalls.eosTsPsDone = 1;
        }
        else if (pSyncTokenOut->type == static_cast<uint32>(AcqRelEventType::CsDone))
        {
            releaseMemInfo.vgtEvent = CS_DONE;

            pBarrierOps->pipelineStalls.eosTsCsDone = 1;
        }

        dwordsWritten += m_cmdUtil.ExplicitBuildReleaseMem(releaseMemInfo,
                                                           VoidPtrInc(pBuffer, sizeof(uint32) * dwordsWritten),
                                                           0,
                                                           0);
    }
    return dwordsWritten;
}
#endif

// =====================================================================================================================
// Translate accessMask to syncReqs.cacheFlags. (CacheCoherencyUsageFlags -> GcrCntl)
uint32 Device::Gfx10BuildReleaseGcrCntl(
    uint32                        accessMask,
    bool                          flushGl2,
    Developer::BarrierOperations* pBarrierOps
    ) const
{
    PAL_ASSERT(pBarrierOps != nullptr);
    Gfx10ReleaseMemGcrCntl gcrCntl = {};

    if (TestAnyFlagSet(accessMask, CoherCpu | CoherMemory))
    {
        // At release we want to invalidate L2 so any future read to L2 would go down to memory, at acquire we want to
        // flush L2 so that main memory gets the latest data.
        gcrCntl.bits.gl2Inv = 1;
        pBarrierOps->caches.invalTcc = 1;
    }

    // Setup GL2Range and Sequence only if cache flush/inv is requested.
    if (gcrCntl.u32All != 0)
    {
        // GL2_RANGE[1:0]
        //  0:ALL          wb/inv op applies to entire physical cache (ignore range)
        //  1:VOL          wb/inv op applies to all volatile tagged lines in the GL2 (ignore range)
        //  2:RANGE      - wb/inv ops applies to just the base/limit virtual address range
        //  3:FIRST_LAST - wb/inv ops applies to 128B at BASE_VA and 128B at LIMIT_VA
        gcrCntl.bits.gl2Range = 0; // ReleaseMem doesn't support RANGE.

        // SEQ[1:0]   controls the sequence of operations on the cache hierarchy (L0/L1/L2)
        //      0: PARALLEL   initiate wb/inv ops on specified caches at same time
        //      1: FORWARD    L0 then L1/L2, complete L0 ops then initiate L1/L2
        //                    Typically only needed when doing WB of L0 K$, M$, or RB w/ WB of GL2
        //      2: REVERSE    L2 -> L1 -> L0
        //                    Typically only used for post-unaligned-DMA operation (invalidate only)
        // Because GCR can issue any cache flush, we need to ensure the flush sequence unconditionally.
        gcrCntl.bits.seq = 1;
    }

    if (flushGl2)
    {
        gcrCntl.bits.gl2Wb = 1;
        pBarrierOps->caches.flushTcc = 1;
    }

    return gcrCntl.u32All;
}

// =====================================================================================================================
// Build the necessary packets to fulfill the requested cache sync for acquire.
size_t Device::BuildAcquireSyncPackets(
    EngineType                    engineType,
    uint32                        stageMask,
    uint32                        accessMask,
    bool                          invalidateLlc,
    gpusize                       baseAddress,
    gpusize                       sizeBytes,
    void*                         pBuffer,      // [out] Build the PM4 packet in this buffer.
    Developer::BarrierOperations* pBarrierOps
    ) const
{
    size_t dwordsWritten = 0;

    // Create info for ACQUIRE_MEM. Initialize common part at here.
    ExplicitAcquireMemInfo acquireMemInfo = {};
    acquireMemInfo.engineType   = engineType;
    acquireMemInfo.baseAddress  = baseAddress;
    acquireMemInfo.sizeBytes    = sizeBytes;
    acquireMemInfo.flags.usePfp = TestAnyFlagSet(stageMask, PipelineStageTopOfPipe         |
                                                            PipelineStageFetchIndirectArgs |
                                                            PipelineStageFetchIndices);

    if (m_gfxIpLevel == GfxIpLevel::GfxIp9)
    {
        uint32 cacheSyncFlags = Gfx9ConvertToAcquireSyncFlags(accessMask, engineType, invalidateLlc, pBarrierOps);

        while (cacheSyncFlags != 0)
        {
            const uint32 tcCacheOp = static_cast<uint32>(SelectTcCacheOp(&cacheSyncFlags));

            regCP_COHER_CNTL cpCoherCntl = {};
            cpCoherCntl.u32All                       = Gfx9TcCacheOpConversionTable[tcCacheOp];
            cpCoherCntl.bits.SH_KCACHE_ACTION_ENA    = TestAnyFlagSet(cacheSyncFlags, CacheSyncInvSqK$);
            cpCoherCntl.bits.SH_ICACHE_ACTION_ENA    = TestAnyFlagSet(cacheSyncFlags, CacheSyncInvSqI$);
            cpCoherCntl.bits.SH_KCACHE_WB_ACTION_ENA = TestAnyFlagSet(cacheSyncFlags, CacheSyncFlushSqK$);

            acquireMemInfo.coherCntl = cpCoherCntl.u32All;

            // Clear up requests
            cacheSyncFlags &= ~(CacheSyncInvSqK$ | CacheSyncInvSqI$ | CacheSyncFlushSqK$);

            // Build ACQUIRE_MEM packet.
            dwordsWritten += m_cmdUtil.ExplicitBuildAcquireMem(acquireMemInfo,
                                                               VoidPtrInc(pBuffer, sizeof(uint32) * dwordsWritten));
        }
    }
    else if (IsGfx10(m_gfxIpLevel))
    {
        // The only difference between the GFX9 and GFX10 versions of this packet are that GFX10
        // added a new "gcr_cntl" field.
        acquireMemInfo.gcrCntl = Gfx10BuildAcquireGcrCntl(accessMask,
                                                          invalidateLlc,
                                                          baseAddress,
                                                          sizeBytes,
                                                          (acquireMemInfo.coherCntl != 0),
                                                          pBarrierOps);

        // GFX10's COHER_CNTL only controls RB flush/inv. "acquire" deson't need to invalidate RB because "release"
        // always flush & invalidate RB, so we never need to set COHER_CNTL here.
        if (acquireMemInfo.gcrCntl != 0)
        {
            // Build ACQUIRE_MEM packet.
            dwordsWritten += m_cmdUtil.ExplicitBuildAcquireMem(acquireMemInfo,
                                                               VoidPtrInc(pBuffer, sizeof(uint32) * dwordsWritten));
        }
    }

    return dwordsWritten;
}

// =====================================================================================================================
// Translate accessMask to GcrCntl.
uint32 Device::Gfx10BuildAcquireGcrCntl(
    uint32                        accessMask,
    bool                          invalidateGl2,
    gpusize                       baseAddress,
    gpusize                       sizeBytes,
    bool                          isFlushing,
    Developer::BarrierOperations* pBarrierOps
    ) const
{
    PAL_ASSERT(pBarrierOps != nullptr);
    // The CP_COHER_CNTL bits are set independently.
    // K$ and I$ and all previous tcCacheOp controlled caches are moved to GCR fields, set in CalcAcquireMemGcrCntl().

    // Cache operations supported by ACQUIRE_MEM's gcr_cntl.
    Gfx10AcquireMemGcrCntl gcrCntl = {};

    // The L1 / L2 caches are physical address based. When specify the range, the GCR will perform virtual address
    // to physical address translation before the wb / inv. If the acquired op is full sync, we must ignore the range,
    // otherwise page fault may occur because page table cannot cover full range virtual address.
    //    When the source address is virtual , the GCR block will have to perform the virtual address to physical
    //    address translation before the wb / inv. Since the pages in memory are a collection of fragments, you can't
    //    specify the full range without walking into a page that has no PTE triggering a fault. In the cases where
    //    the driver wants to wb / inv the entire cache, you should not use range based method, and instead flush the
    //    entire cache without it. The range based method is not meant to be used this way, it is for selective page
    //    invalidation.
    //
    // GL1_RANGE[1:0] - range control for L0 / L1 physical caches(K$, V$, M$, GL1)
    //  0:ALL         - wb / inv op applies to entire physical cache (ignore range)
    //  1:reserved
    //  2:RANGE       - wb / inv op applies to just the base / limit virtual address range
    //  3:FIRST_LAST  - wb / inv op applies to 128B at BASE_VA and 128B at LIMIT_VA
    //
    // GL2_RANGE[1:0]
    //  0:ALL         - wb / inv op applies to entire physical cache (ignore range)
    //  1:VOL         - wb / inv op applies to all volatile tagged lines in the GL2 (ignore range)
    //  2:RANGE       - wb / inv op applies to just the base/limit virtual address range
    //  3:FIRST_LAST  - wb / inv op applies to 128B at BASE_VA and 128B at LIMIT_VA
    if (((baseAddress == FullSyncBaseAddr) && (sizeBytes == FullSyncSize)) ||
        (sizeBytes > CmdUtil::Gfx10AcquireMemGl1Gl2RangedCheckMaxSurfaceSizeBytes))
    {
        gcrCntl.bits.gl1Range = 0;
        gcrCntl.bits.gl2Range = 0;
    }
    else
    {
        gcrCntl.bits.gl1Range = 2;
        gcrCntl.bits.gl2Range = 2;
    }

    // GLM_WB[0]  - write-back control for the meta-data cache of GL2. L2MD is write-through, ignore this bit.
    // GLK_WB[0]  - write-back control for shaded scalar L0 cache
    gcrCntl.bits.glmWb = 0;
    gcrCntl.bits.glkWb = 0;

    // GLM_INV[0] - invalidate enable for the meta-data cache of GL2
    // GLK_INV[0] - invalidate enable for shader scalar L0 cache
    // GLV_INV[0] - invalidate enable for shader vector L0 cache
    // GL1_INV[0] - invalidate enable for GL1
    if (TestAnyFlagSet(accessMask, CoherShader | CoherCopy | CoherResolve | CoherClear | CoherStreamOut |
                                   CoherSampleRate))
    {
        gcrCntl.bits.glmInv = 1;
        gcrCntl.bits.glkInv = 1;
        gcrCntl.bits.glvInv = 1;
        gcrCntl.bits.gl1Inv = 1;

        pBarrierOps->caches.invalTccMetadata = 1;
        pBarrierOps->caches.invalSqK$        = 1;
        pBarrierOps->caches.invalTcp         = 1;
        pBarrierOps->caches.invalGl1         = 1;
    }

    // Leave gcrCntl.bits.gl2Us unset.
    // Leave gcrCntl.bits.gl2Discard unset.

    // GL2_INV[0] - invalidate enable for GL2
    // GL2_WB[0]  - writeback enable for GL2
    if (invalidateGl2)
    {
        gcrCntl.bits.gl2Inv          = 1;
        pBarrierOps->caches.invalTcc = 1;
    }
    if (TestAnyFlagSet(accessMask, CoherCpu | CoherMemory))
    {
        gcrCntl.bits.gl2Wb           = 1;
        pBarrierOps->caches.flushTcc = 1;
    }

    // SEQ[1:0]   controls the sequence of operations on the cache hierarchy (L0/L1/L2)
    //      0: PARALLEL   initiate wb/inv ops on specified caches at same time
    //      1: FORWARD    L0 then L1/L2, complete L0 ops then initiate L1/L2
    //                    Typically only needed when doing WB of L0 K$, M$, or RB w/ WB of GL2
    //      2: REVERSE    L2 -> L1 -> L0
    //                    Typically only used for post-unaligned-DMA operation (invalidate only)
    // If we're issuing an RB cache flush while writing back GL2, we need to ensure the bottom-up flush sequence.
    //  Note: If we ever start flushing K$ or M$, isFlushing should be updated
    PAL_ASSERT((gcrCntl.bits.glmWb == 0) && (gcrCntl.bits.glkWb == 0));
    gcrCntl.bits.seq = (isFlushing && gcrCntl.bits.gl2Wb) ? 1 : 0;

    return gcrCntl.u32All;
}

// =====================================================================================================================
// Issue the specified BLT operation(s) (i.e., decompresses) necessary to convert a color image from one ImageLayout to
// another.
void Device::AcqRelColorTransitionEvent(
    GfxCmdBuffer*                 pCmdBuf,
    CmdStream*                    pCmdStream,
    const ImgBarrier&             imgBarrier,
    LayoutTransitionInfo          layoutTransInfo,
    Developer::BarrierOperations* pBarrierOps
    ) const
{
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 642
    PAL_ASSERT(imgBarrier.subresRange.numPlanes == 1);
#endif

    PAL_ASSERT(imgBarrier.pImage != nullptr);

    const EngineType engineType = pCmdBuf->GetEngineType();
    const auto&      image      = static_cast<const Pal::Image&>(*imgBarrier.pImage);
    const auto&      gfx9Image  = static_cast<const Gfx9::Image&>(*image.GetGfxImage());

    PAL_ASSERT(image.IsDepthStencilTarget() == false);

    if (layoutTransInfo.blt[0] == HwLayoutTransition::MsaaColorDecompress)
    {
        RsrcProcMgr().FmaskColorExpand(pCmdBuf, gfx9Image, imgBarrier.subresRange);
    }
    else
    {
        if (layoutTransInfo.blt[0] == HwLayoutTransition::DccDecompress)
        {
            RsrcProcMgr().DccDecompress(pCmdBuf,
                                        pCmdStream,
                                        gfx9Image,
                                        imgBarrier.pQuadSamplePattern,
                                        imgBarrier.subresRange);
        }
        else if (layoutTransInfo.blt[0] == HwLayoutTransition::FmaskDecompress)
        {
            RsrcProcMgr().FmaskDecompress(pCmdBuf,
                                          pCmdStream,
                                          gfx9Image,
                                          imgBarrier.pQuadSamplePattern,
                                          imgBarrier.subresRange);
        }
        else
        {
            PAL_ASSERT(layoutTransInfo.blt[0] == HwLayoutTransition::FastClearEliminate);

            // Note: if FCE is not submitted to GPU, we don't need to update cache flags.
            const bool isSubmitted = RsrcProcMgr().FastClearEliminate(pCmdBuf,
                                                                      pCmdStream,
                                                                      gfx9Image,
                                                                      imgBarrier.pQuadSamplePattern,
                                                                      imgBarrier.subresRange);
            layoutTransInfo.flags.fceIsSkipped = (isSubmitted == false);
        }

        // Handle corner cases where it needs a second pass.
        if (layoutTransInfo.blt[1] != HwLayoutTransition::None)
        {
            PAL_ASSERT(layoutTransInfo.blt[1] == HwLayoutTransition::MsaaColorDecompress);

            uint32 stageMask  = 0;
            uint32 accessMask = 0;

            // Prepare release info for first pass BLT.
            GetBltStageAccessInfo(layoutTransInfo, &stageMask, &accessMask);

            const IGpuEvent* pEvent = pCmdBuf->GetInternalEvent();

            // Release from first pass.
            IssueReleaseSyncEvent(pCmdBuf, pCmdStream, stageMask, accessMask, false, pEvent, pBarrierOps);

            // Prepare second pass info.
            constexpr LayoutTransitionInfo MsaaBltInfo = { {}, HwLayoutTransition::MsaaColorDecompress };

            GetBltStageAccessInfo(MsaaBltInfo, &stageMask, &accessMask);

            // Acquire for second pass.
            IssueAcquireSyncEvent(pCmdBuf,
                                  pCmdStream,
                                  stageMask,
                                  accessMask,
                                  false,
                                  FullSyncBaseAddr,
                                  FullSyncSize,
                                  1,
                                  &pEvent,
                                  pBarrierOps);

            // Tell RGP about this transition
            BarrierTransition rgpTransition = AcqRelBuildTransition(&imgBarrier, MsaaBltInfo, pBarrierOps);
            DescribeBarrier(pCmdBuf, &rgpTransition, pBarrierOps);

            // And clear it so it can differentiate sync and async flushes
            *pBarrierOps = {};

            RsrcProcMgr().FmaskColorExpand(pCmdBuf, gfx9Image, imgBarrier.subresRange);
        }
    }
}

// =====================================================================================================================
// Issue appropriate cache sync hardware commands to satisfy the cache release requirements.
void Device::IssueReleaseSyncEvent(
    GfxCmdBuffer*                 pCmdBuf,
    CmdStream*                    pCmdStream,
    uint32                        stageMask,
    uint32                        accessMask,
    bool                          flushLlc,
    const IGpuEvent*              pGpuEvent,
    Developer::BarrierOperations* pBarrierOps
    ) const
{
    // Validate input.
    PAL_ASSERT(stageMask != 0);

    PAL_ASSERT(pGpuEvent != nullptr);

    PAL_ASSERT(pBarrierOps != nullptr);

    const EngineType engineType = pCmdBuf->GetEngineType();

    uint32*          pCmdSpace  = pCmdStream->ReserveCommands();

    if (pCmdBuf->GetGfxCmdBufState().flags.cpBltActive && TestAnyFlagSet(stageMask, PipelineStageBlt))
    {
        // We must guarantee that all prior CP DMA accelerated blts have completed before we write this event because
        // the CmdSetEvent and CmdResetEvent functions expect that the prior blts have reached the post-blt stage by
        // the time the event is written to memory. Given that our CP DMA blts are asynchronous to the pipeline stages
        // the only way to satisfy this requirement is to force the MEC to stall until the CP DMAs are completed.
        pBarrierOps->pipelineStalls.syncCpDma = 1;
        pCmdSpace += m_cmdUtil.BuildWaitDmaData(pCmdSpace);
        pCmdBuf->SetGfxCmdBufCpBltState(false);
    }

    pCmdBuf->OptimizePipeAndCacheMaskForRelease(&stageMask, &accessMask);

    if (pCmdBuf->IsGraphicsSupported())
    {
        // Mark off PS stage if current pipeline doesn't do rasterization.
        const auto* pUniversalCmdBuf = static_cast<UniversalCmdBuffer*>(pCmdBuf);
        if (pUniversalCmdBuf->IsRasterizationKilled())
        {
            stageMask &= ~PipelineStagePs;
        }
    }
    else
    {
        // Mark off all graphics path specific stages if command buffer doesn't support graphics.
        stageMask &= ~GraphicsOnlyPipeStages;
    }

    pCmdSpace += BuildReleaseSyncPacketsEvent(engineType,
                                              stageMask,
                                              accessMask,
                                              flushLlc,
                                              pGpuEvent,
                                              pCmdSpace,
                                              pBarrierOps);

    pCmdStream->CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// Issue appropriate cache sync hardware commands to satisfy the cache acquire requirements.
void Device::IssueAcquireSyncEvent(
    GfxCmdBuffer*                 pCmdBuf,
    CmdStream*                    pCmdStream,
    uint32                        stageMask,
    uint32                        accessMask,
    bool                          invalidateLlc,
    gpusize                       rangeStartAddr,
    gpusize                       rangeSize,
    uint32                        gpuEventCount,
    const IGpuEvent* const*       ppGpuEvents,
    Developer::BarrierOperations* pBarrierOps
    ) const
{
    const EngineType engineType = pCmdBuf->GetEngineType();
    const bool   isGfxSupported = Pal::Device::EngineSupportsGraphics(engineType);

    if (isGfxSupported == false)
    {
        stageMask &= ~GraphicsOnlyPipeStages;
    }

    // BuildWaitRegMem waits in the ME, if the waitPoint needs to stall at the PFP request a PFP/ME sync.
    const bool pfpSyncMe = TestAnyFlagSet(stageMask, PipelineStageTopOfPipe         |
                                                     PipelineStageFetchIndirectArgs |
                                                     PipelineStageFetchIndices);

    uint32* pCmdSpace = pCmdStream->ReserveCommands();

    // Wait on the GPU memory slot(s) in all specified IGpuEvent objects.
    if (gpuEventCount != 0)
    {
        PAL_ASSERT(ppGpuEvents != nullptr);
        pBarrierOps->pipelineStalls.waitOnTs = 1;

        for (uint32 i = 0; i < gpuEventCount; i++)
        {
            const uint32    numEventSlots   = Parent()->ChipProperties().gfxip.numSlotsPerEvent;
            const GpuEvent* pGpuEvent       = static_cast<const GpuEvent*>(ppGpuEvents[i]);
            const gpusize   gpuEventStartVa = pGpuEvent->GetBoundGpuMemory().GpuVirtAddr();

            if (numEventSlots == 1)
            {
                pCmdSpace += m_cmdUtil.BuildWaitRegMem(engineType,
                                                       mem_space__me_wait_reg_mem__memory_space,
                                                       function__me_wait_reg_mem__equal_to_the_reference_value,
                                                       engine_sel__me_wait_reg_mem__micro_engine,
                                                       gpuEventStartVa,
                                                       GpuEvent::SetValue,
                                                       0xFFFFFFFF,
                                                       pCmdSpace);
            }
            else
            {
                PAL_ASSERT(numEventSlots == 2);
                pCmdSpace += m_cmdUtil.BuildWaitRegMem64(engineType,
                                                         mem_space__me_wait_reg_mem__memory_space,
                                                         function__me_wait_reg_mem__equal_to_the_reference_value,
                                                         engine_sel__me_wait_reg_mem__micro_engine,
                                                         gpuEventStartVa,
                                                         GpuEvent::SetValue64,
                                                         0xFFFFFFFFFFFFFFFF,
                                                         pCmdSpace);
            }
        }
    }

    if (accessMask != 0)
    {
        // OptimizeBltCacheAccess() doesn't apply to acquire-sync. Acquire is for the future state, however the state
        // tracking mechanism tracks past operations.
        pCmdSpace += BuildAcquireSyncPackets(engineType,
                                             stageMask,
                                             accessMask,
                                             invalidateLlc,
                                             rangeStartAddr,
                                             rangeSize,
                                             pCmdSpace,
                                             pBarrierOps);
    }

    if (pfpSyncMe && isGfxSupported)
    {
        // Stalls the CP PFP until the ME has processed all previous commands.  Useful in cases where the ME is waiting
        // on some condition, but the PFP needs to stall execution until the condition is satisfied.  This must go last
        // otherwise the PFP could resume execution before the ME is done with all of its waits.
        pCmdSpace += m_cmdUtil.BuildPfpSyncMe(pCmdSpace);
        pBarrierOps->pipelineStalls.pfpSyncMe = 1;
    }

    pCmdStream->CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// BarrierRelease perform any necessary layout transition, availability operation, and enqueue command(s) to set a given
// IGpuEvent object once the prior operations' intersection with the given synchronization scope is confirmed complete.
// The availability operation will flush the requested local caches.
void Device::BarrierReleaseEvent(
    GfxCmdBuffer*                 pCmdBuf,
    CmdStream*                    pCmdStream,
    const AcquireReleaseInfo&     barrierReleaseInfo,
    const IGpuEvent*              pClientEvent,
    Developer::BarrierOperations* pBarrierOps,
    bool                          waMetaMisalignNeedRefreshLlc
    ) const
{
    // Validate input data.
    PAL_ASSERT(barrierReleaseInfo.dstStageMask == 0);
    PAL_ASSERT(barrierReleaseInfo.dstGlobalAccessMask == 0);
    for (uint32 i = 0; i < barrierReleaseInfo.memoryBarrierCount; i++)
    {
        PAL_ASSERT(barrierReleaseInfo.pMemoryBarriers[i].dstAccessMask == 0);
    }
    for (uint32 i = 0; i < barrierReleaseInfo.imageBarrierCount; i++)
    {
        PAL_ASSERT(barrierReleaseInfo.pImageBarriers[i].dstAccessMask == 0);
    }
    PAL_ASSERT(pClientEvent != nullptr);

    const uint32 preBltStageMask   = barrierReleaseInfo.srcStageMask;
    uint32       preBltAccessMask  = barrierReleaseInfo.srcGlobalAccessMask;
    bool         globallyAvailable = waMetaMisalignNeedRefreshLlc;

    // Assumes always do full-range flush sync.
    for (uint32 i = 0; i < barrierReleaseInfo.memoryBarrierCount; i++)
    {
        const MemBarrier& barrier = barrierReleaseInfo.pMemoryBarriers[i];

        preBltAccessMask  |= barrier.srcAccessMask;
        globallyAvailable |= barrier.flags.globallyAvailable;
    }

    // A container to cache the calculated BLT transitions and some cache info for reuse.
    AutoBuffer<AcqRelTransitionInfo, 8, Platform> transitionList(barrierReleaseInfo.imageBarrierCount, GetPlatform());
    uint32 bltTransitionCount = 0;

    Result result = Result::Success;

    if (transitionList.Capacity() < barrierReleaseInfo.imageBarrierCount)
    {
        pCmdBuf->NotifyAllocFailure();
    }
    else
    {
        // Loop through image transitions to update client requested access.
        for (uint32 i = 0; i < barrierReleaseInfo.imageBarrierCount; i++)
        {
            const ImgBarrier& imageBarrier = barrierReleaseInfo.pImageBarriers[i];
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 642
            PAL_ASSERT(imageBarrier.subresRange.numPlanes == 1);
#endif

            // Update client requested access mask.
            preBltAccessMask |= imageBarrier.srcAccessMask;

            // Prepare a layout transition BLT info and do pre-BLT preparation work.
            LayoutTransitionInfo layoutTransInfo           = PrepareBltInfo(pCmdBuf, imageBarrier);

            transitionList[i].pImgBarrier                  = &imageBarrier;
            transitionList[i].layoutTransInfo              = layoutTransInfo;
            transitionList[i].waMetaMisalignNeedRefreshLlc = waMetaMisalignNeedRefreshLlc;

            uint32 bltStageMask = 0;
            uint32 bltAccessMask = 0;

            if (layoutTransInfo.blt[0] != HwLayoutTransition::None)
            {
                GetBltStageAccessInfo(layoutTransInfo, &bltStageMask, &bltAccessMask);

                transitionList[i].bltStageMask  = bltStageMask;
                transitionList[i].bltAccessMask = bltAccessMask;
                bltTransitionCount++;
            }
            else
            {
                transitionList[i].bltStageMask  = 0;
                transitionList[i].bltAccessMask = 0;
            }

            if (WaRefreshTccToAlignMetadata(imageBarrier.pImage,
                                            imageBarrier.subresRange,
                                            imageBarrier.srcAccessMask,
                                            bltAccessMask))
            {
                transitionList[i].waMetaMisalignNeedRefreshLlc = true;
                globallyAvailable |= true;
            }
        }

        // Initialize an IGpuEvent* pEvent pointing at the client provided event.
        // If we have internal BLT(s), use internal event to signal/wait.
        const IGpuEvent* pActiveEvent = (bltTransitionCount > 0) ? pCmdBuf->GetInternalEvent() : pClientEvent;

        // Perform an all-in-one release prior to the potential BLT(s): IssueReleaseSyncEvent() on pActiveEvent.
        IssueReleaseSyncEvent(pCmdBuf,
                              pCmdStream,
                              preBltStageMask,
                              preBltAccessMask,
                              globallyAvailable,
                              pActiveEvent,
                              pBarrierOps);

        // Issue BLT(s) if there exists transitions that require one.
        if (bltTransitionCount > 0)
        {
            // If BLT(s) will be issued, we need to know how to release from it/them.
            uint32 postBltStageMask  = 0;
            uint32 postBltAccessMask = 0;

            bool   needEventWait     = true;

            // Issue pre-BLT acquires.
            for (uint32 i = 0; i < barrierReleaseInfo.imageBarrierCount; i++)
            {
                const AcqRelTransitionInfo& transition = transitionList[i];

                if (transition.layoutTransInfo.blt[0] != HwLayoutTransition::None)
                {
                    const auto& image = static_cast<const Pal::Image&>(*transition.pImgBarrier->pImage);

                    IssueAcquireSyncEvent(pCmdBuf,
                                          pCmdStream,
                                          transition.bltStageMask,
                                          transition.bltAccessMask,
                                          transition.waMetaMisalignNeedRefreshLlc,
                                          image.GetGpuVirtualAddr(),
                                          image.GetGpuMemSize(),
                                          (needEventWait ? 1 : 0),
                                          &pActiveEvent,
                                          pBarrierOps);

                    needEventWait = false;
                }
            }

            // Issue BLTs.
            for (uint32 i = 0; i < barrierReleaseInfo.imageBarrierCount; i++)
            {
                const AcqRelTransitionInfo& transition = transitionList[i];

                if (transition.layoutTransInfo.blt[0] != HwLayoutTransition::None)
                {
                    IssueBlt(pCmdBuf,
                             pCmdStream,
                             transition.pImgBarrier,
                             transition.layoutTransInfo,
                             pBarrierOps);

                    uint32 stageMask  = 0;
                    uint32 accessMask = 0;

                    if (transition.layoutTransInfo.blt[1] != HwLayoutTransition::None)
                    {
                        PAL_ASSERT(transition.layoutTransInfo.blt[1] == HwLayoutTransition::MsaaColorDecompress);
                        constexpr LayoutTransitionInfo MsaaBltInfo = { {}, HwLayoutTransition::MsaaColorDecompress };
                        GetBltStageAccessInfo(MsaaBltInfo, &stageMask, &accessMask);
                    }
                    else
                    {
                        stageMask  = transition.bltStageMask;
                        accessMask = transition.bltAccessMask;
                    }

                    // Add current BLT's stageMask/accessMask into a stageMask/accessMask used for an all-in-one
                    // post-BLT release.
                    postBltStageMask  |= stageMask;
                    postBltAccessMask |= accessMask;
                }
            }

            // Get back the client provided event and signal it when the whole barrier-release is done.
            pActiveEvent = pClientEvent;

            // Release from BLTs.
            IssueReleaseSyncEvent(pCmdBuf,
                                  pCmdStream,
                                  postBltStageMask,
                                  postBltAccessMask,
                                  globallyAvailable,
                                  pActiveEvent,
                                  pBarrierOps);
        }
    }
}

// =====================================================================================================================
// BarrierAcquire will wait on the specified IGpuEvent object to be signaled, perform any necessary layout transition,
// and issue the required visibility operations. The visibility operation will invalidate the required ranges in local
// caches.
void Device::BarrierAcquireEvent(
    GfxCmdBuffer*                 pCmdBuf,
    CmdStream*                    pCmdStream,
    const AcquireReleaseInfo&     barrierAcquireInfo,
    uint32                        gpuEventCount,
    const IGpuEvent* const*       ppGpuEvents,
    Developer::BarrierOperations* pBarrierOps,
    bool                          waMetaMisalignNeedRefreshLlc
    ) const
{
    // Validate input data.
    PAL_ASSERT(barrierAcquireInfo.srcStageMask == 0);
    PAL_ASSERT(barrierAcquireInfo.srcGlobalAccessMask == 0);
    for (uint32 i = 0; i < barrierAcquireInfo.memoryBarrierCount; i++)
    {
        PAL_ASSERT(barrierAcquireInfo.pMemoryBarriers[i].srcAccessMask == 0);
    }
    for (uint32 i = 0; i < barrierAcquireInfo.imageBarrierCount; i++)
    {
        PAL_ASSERT(barrierAcquireInfo.pImageBarriers[i].srcAccessMask == 0);
    }

    bool globallyAvailable = waMetaMisalignNeedRefreshLlc;

    // A container to cache the calculated BLT transitions and some cache info for reuse.
    AutoBuffer<AcqRelTransitionInfo, 8, Platform> transitionList(barrierAcquireInfo.imageBarrierCount, GetPlatform());
    uint32 bltTransitionCount = 0;

    if (transitionList.Capacity() < barrierAcquireInfo.imageBarrierCount)
    {
        pCmdBuf->NotifyAllocFailure();
    }
    else
    {
        // Acquire for BLTs.
        for (uint32 i = 0; i < barrierAcquireInfo.imageBarrierCount; i++)
        {
            const auto& imgBarrier = barrierAcquireInfo.pImageBarriers[i];
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 642
            PAL_ASSERT(imgBarrier.subresRange.numPlanes == 1);
#endif

            // Prepare a layout transition BLT info and do pre-BLT preparation work.
            const LayoutTransitionInfo layoutTransInfo     = PrepareBltInfo(pCmdBuf, imgBarrier);

            transitionList[i].pImgBarrier                  = &imgBarrier;
            transitionList[i].layoutTransInfo              = layoutTransInfo;
            transitionList[i].waMetaMisalignNeedRefreshLlc = waMetaMisalignNeedRefreshLlc;

            uint32 bltStageMask                            = 0;
            uint32 bltAccessMask                           = 0;

            if (layoutTransInfo.blt[0] != HwLayoutTransition::None)
            {
                GetBltStageAccessInfo(layoutTransInfo, &bltStageMask, &bltAccessMask);

                transitionList[i].bltStageMask  = bltStageMask;
                transitionList[i].bltAccessMask = bltAccessMask;
                bltTransitionCount++;

                if (WaRefreshTccToAlignMetadata(imgBarrier.pImage,
                                                imgBarrier.subresRange,
                                                bltAccessMask,
                                                imgBarrier.dstAccessMask))
                {
                    transitionList[i].waMetaMisalignNeedRefreshLlc = true;
                    globallyAvailable                              = true;
                }
            }
            else
            {
                transitionList[i].bltStageMask  = 0;
                transitionList[i].bltAccessMask = 0;
            }
        }

        const IGpuEvent* const* ppActiveEvents = ppGpuEvents;
        uint32 activeEventCount                = gpuEventCount;

        if (bltTransitionCount > 0)
        {
            // If BLT(s) will be issued, we need to know how to release from it/them.
            uint32 postBltStageMask  = 0;
            uint32 postBltAccessMask = 0;

            bool   needEventWait     = true;

            // Issue pre-BLT acquires.
            for (uint32 i = 0; i < barrierAcquireInfo.imageBarrierCount; i++)
            {
                const AcqRelTransitionInfo& transition = transitionList[i];

                if (transition.layoutTransInfo.blt[0] != HwLayoutTransition::None)
                {
                    const auto& image = static_cast<const Pal::Image&>(*transitionList[i].pImgBarrier->pImage);

                    IssueAcquireSyncEvent(pCmdBuf,
                                          pCmdStream,
                                          transition.bltStageMask,
                                          transition.bltAccessMask,
                                          transition.waMetaMisalignNeedRefreshLlc,
                                          image.GetGpuVirtualAddr(),
                                          image.GetGpuMemSize(),
                                          (needEventWait ? activeEventCount : 0),
                                          ppActiveEvents,
                                          pBarrierOps);

                    needEventWait = false;
                }
            }

            // Issue BLTs.
            for (uint32 i = 0; i < barrierAcquireInfo.imageBarrierCount; i++)
            {
                const AcqRelTransitionInfo& transition = transitionList[i];

                if (transition.layoutTransInfo.blt[0] != HwLayoutTransition::None)
                {
                    IssueBlt(pCmdBuf,
                             pCmdStream,
                             transition.pImgBarrier,
                             transition.layoutTransInfo,
                             pBarrierOps);

                    uint32 stageMask  = 0;
                    uint32 accessMask = 0;

                    if (transition.layoutTransInfo.blt[1] != HwLayoutTransition::None)
                    {
                        PAL_ASSERT(transition.layoutTransInfo.blt[1] == HwLayoutTransition::MsaaColorDecompress);
                        constexpr LayoutTransitionInfo MsaaBltInfo = { {}, HwLayoutTransition::MsaaColorDecompress };
                        GetBltStageAccessInfo(MsaaBltInfo, &stageMask, &accessMask);
                    }
                    else
                    {
                        stageMask  = transition.bltStageMask;
                        accessMask = transition.bltAccessMask;
                    }

                    // Add current BLT's stageMask/accessMask into a stageMask/accessMask used for an all-in-one
                    // post-BLT release.
                    postBltStageMask  |= stageMask;
                    postBltAccessMask |= accessMask;
                }
            }

            // We have internal BLT(s), enable internal event to signal/wait.
            const IGpuEvent* pEvent = pCmdBuf->GetInternalEvent();

            // Release from BLTs.
            IssueReleaseSyncEvent(pCmdBuf,
                                  pCmdStream,
                                  postBltStageMask,
                                  postBltAccessMask,
                                  globallyAvailable,
                                  pEvent,
                                  pBarrierOps);

            ppActiveEvents   = &pEvent;
            activeEventCount = 1;
        }

        // Issue acquire for client requested global cache sync.
        IssueAcquireSyncEvent(pCmdBuf,
                              pCmdStream,
                              barrierAcquireInfo.dstStageMask,
                              barrierAcquireInfo.dstGlobalAccessMask,
                              false,
                              FullSyncBaseAddr,
                              FullSyncSize,
                              activeEventCount,
                              ppActiveEvents,
                              pBarrierOps);

        // Loop through memory transitions to issue client-requested acquires for ranged memory syncs.
        for (uint32 i = 0; i < barrierAcquireInfo.memoryBarrierCount; i++)
        {
            const MemBarrier&         barrier            = barrierAcquireInfo.pMemoryBarriers[i];
            const GpuMemSubAllocInfo& memAllocInfo       = barrier.memory;

            const uint32              acquireAccessMask  = barrier.dstAccessMask;
            const gpusize             rangedSyncBaseAddr = memAllocInfo.pGpuMemory->Desc().gpuVirtAddr + memAllocInfo.offset;
            const gpusize             rangedSyncSize     = memAllocInfo.size;

            IssueAcquireSyncEvent(pCmdBuf,
                                  pCmdStream,
                                  barrierAcquireInfo.dstStageMask,
                                  acquireAccessMask,
                                  false,
                                  rangedSyncBaseAddr,
                                  rangedSyncSize,
                                  0,
                                  nullptr,
                                  pBarrierOps);
        }

        // Loop through memory transitions to issue client-requested acquires for image syncs.
        for (uint32 i = 0; i < barrierAcquireInfo.imageBarrierCount; i++)
        {
            const ImgBarrier& imgBarrier = barrierAcquireInfo.pImageBarriers[i];

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 642
            PAL_ASSERT(imgBarrier.subresRange.numPlanes == 1);
#endif
            const Pal::Image& image = static_cast<const Pal::Image&>(*imgBarrier.pImage);

            IssueAcquireSyncEvent(pCmdBuf,
                                  pCmdStream,
                                  barrierAcquireInfo.dstStageMask,
                                  imgBarrier.dstAccessMask,
                                  transitionList[i].waMetaMisalignNeedRefreshLlc,
                                  image.GetGpuVirtualAddr(),
                                  image.GetGpuMemSize(),
                                  0,
                                  nullptr,
                                  pBarrierOps);
        }
    }
}

// =====================================================================================================================
// Build the necessary packets to fulfill the requested cache sync for release.
size_t Device::BuildReleaseSyncPacketsEvent(
    EngineType                    engineType,
    uint32                        stageMask,
    uint32                        accessMask,
    bool                          flushLlc,
    const IGpuEvent*              pGpuEventToSignal,
    void*                         pBuffer,
    Developer::BarrierOperations* pBarrierOps
    ) const
{
    // OptimizeBltCacheAccess() should've been called to convert these BLT coherency flags to more specific ones.
    PAL_ASSERT(TestAnyFlagSet(accessMask, CoherCopy | CoherResolve | CoherClear) == false);

    PAL_ASSERT(pGpuEventToSignal != nullptr);

    PAL_ASSERT(pBarrierOps != nullptr);

    // Issue RELEASE_MEM packets to flush caches (optional) and signal gpuEvent.
    const uint32          numEventSlots               = Parent()->ChipProperties().gfxip.numSlotsPerEvent;
    const GpuEvent*       pGpuEvent                   = static_cast<const GpuEvent*>(pGpuEventToSignal);
    const BoundGpuMemory& gpuEventBoundMemObj         = pGpuEvent->GetBoundGpuMemory();
    PAL_ASSERT(gpuEventBoundMemObj.IsBound());
    const gpusize         gpuEventStartVa             = gpuEventBoundMemObj.GpuVirtAddr();

    VGT_EVENT_TYPE        vgtEvents[MaxSlotsPerEvent] = {}; // Always create the max size.
    uint32                vgtEventCount               = 0;

    // If any of the access mask bits that could result in RB sync are set, use CACHE_FLUSH_AND_INV_TS.
    // There is no way to INV the CB metadata caches during acquire. So at release always also invalidate if we are to
    // flush CB metadata. Furthermore, CACHE_FLUSH_AND_INV_TS_EVENT always flush & invalidate RB, so there is no need
    // to invalidate RB at acquire again.
    if (TestAnyFlagSet(accessMask, CoherColorTarget | CoherDepthStencilTarget))
    {
        // Issue a pipelined EOP event that writes timestamp to a GpuEvent slot when all prior GPU work completes.
        vgtEvents[vgtEventCount++]                    = CACHE_FLUSH_AND_INV_TS_EVENT;
        pBarrierOps->pipelineStalls.eopTsBottomOfPipe = 1;

        // Clear up CB/DB request
        accessMask &= ~(CoherColorTarget | CoherDepthStencilTarget);

        pBarrierOps->caches.flushCb         = 1;
        pBarrierOps->caches.invalCb         = 1;
        pBarrierOps->caches.flushCbMetadata = 1;
        pBarrierOps->caches.invalCbMetadata = 1;

        pBarrierOps->caches.flushDb         = 1;
        pBarrierOps->caches.invalDb         = 1;
        pBarrierOps->caches.flushDbMetadata = 1;
        pBarrierOps->caches.invalDbMetadata = 1;
    }
    else if (TestAnyFlagSet(stageMask, PipelineStageEarlyDsTarget |
                                       PipelineStageLateDsTarget  |
                                       PipelineStageColorTarget   |
                                       PipelineStageBottomOfPipe))
    {
        // Implement set with an EOP event written when all prior GPU work completes.
        vgtEvents[vgtEventCount++]                    = BOTTOM_OF_PIPE_TS;
        pBarrierOps->pipelineStalls.eopTsBottomOfPipe = 1;
    }
    else if (TestAnyFlagSet(stageMask, PipelineStageVs | PipelineStageHs | PipelineStageDs | PipelineStageGs) &&
        (TestAnyFlagSet(stageMask, PipelineStagePs) == false))
    {
        // Unfortunately, there is no VS_DONE event with which to implement PipelineStageVs/Hs/Ds/Gs, so it has to
        // conservatively use BottomOfPipe.
        vgtEvents[vgtEventCount++]                    = BOTTOM_OF_PIPE_TS;
        pBarrierOps->pipelineStalls.eopTsBottomOfPipe = 1;
    }
    else
    {
        // The signal/wait event may have multiple slots, we can utilize it to issue separate EOS event for PS and CS
        // waves.
        if (TestAnyFlagSet(stageMask, PipelineStageCs))
        {
            // Implement set/reset with an EOS event waiting for CS waves to complete.
            vgtEvents[vgtEventCount++]              = CS_DONE;
            pBarrierOps->pipelineStalls.eosTsCsDone = 1;
        }

        if (TestAnyFlagSet(stageMask, PipelineStageVs |
                                      PipelineStageHs |
                                      PipelineStageDs |
                                      PipelineStageGs |
                                      PipelineStagePs))
        {
            // Implement set with an EOS event waiting for PS waves to complete.
            vgtEvents[vgtEventCount++]              = PS_DONE;
            pBarrierOps->pipelineStalls.eosTsPsDone = 1;
        }

        if (vgtEventCount > numEventSlots)
        {
            // Fall back to single EOP pipe point if available event slots are not sufficient for multiple pipe points.
            vgtEvents[0]                                  = BOTTOM_OF_PIPE_TS;
            vgtEventCount                                 = 1;
            pBarrierOps->pipelineStalls.eosTsCsDone       = 0;
            pBarrierOps->pipelineStalls.eosTsPsDone       = 0;
            pBarrierOps->pipelineStalls.eopTsBottomOfPipe = 1;
        }
    }

    // Create info for RELEASE_MEM. Initialize common part at here.
    ExplicitReleaseMemInfo releaseMemInfo = {};
    releaseMemInfo.engineType = engineType;

    if (m_gfxIpLevel == GfxIpLevel::GfxIp9)
    {
        uint32 cacheSyncFlags = Gfx9ConvertToReleaseSyncFlags(accessMask, flushLlc, pBarrierOps);

        const uint32 tcCacheOp = static_cast<uint32>(SelectTcCacheOp(&cacheSyncFlags));

        // The cache sync requests can be cleared by single release pass.
        PAL_ASSERT(cacheSyncFlags == 0);

        releaseMemInfo.coherCntl = Gfx9TcCacheOpConversionTable[tcCacheOp];
    }
    else if (IsGfx10(m_gfxIpLevel))
    {
        releaseMemInfo.gcrCntl = Gfx10BuildReleaseGcrCntl(accessMask, flushLlc, pBarrierOps);
    }
    // If we have cache sync request yet don't assign any VGT event, we need to issue a dummy one.
    if (((releaseMemInfo.coherCntl != 0) || (releaseMemInfo.gcrCntl != 0)) && (vgtEventCount == 0))
    {
        // Flush at earliest supported pipe point for RELEASE_MEM (CS_DONE always works).
        vgtEvents[vgtEventCount++]              = CS_DONE;
        pBarrierOps->pipelineStalls.eosTsCsDone = 1;
    }

    PAL_ASSERT(vgtEventCount <= numEventSlots);

    size_t dwordsWritten = 0;

    // Issue releases with the requested eop/eos
    if (vgtEventCount > 0)
    {
        // Build a WRITE_DATA command to first RESET event slots that will be set by event later on.
        WriteDataInfo writeData = {};
        writeData.engineType = engineType;
        writeData.engineSel  = engine_sel__me_write_data__micro_engine;
        writeData.dstSel     = dst_sel__me_write_data__memory;
        writeData.dstAddr    = gpuEventStartVa;

        const uint32 dword   = GpuEvent::ResetValue;

        dwordsWritten += CmdUtil::BuildWriteDataPeriodic(writeData,
                                                         1,
                                                         vgtEventCount,
                                                         &dword,
                                                         VoidPtrInc(pBuffer, sizeof(uint32) * dwordsWritten));

        // Build release packets with requested eop/eos events on ME engine.
        for (uint32 i = 0; i < vgtEventCount; i++)
        {
            const gpusize gpuEventSlotVa = gpuEventStartVa + (i * sizeof(uint32));

            releaseMemInfo.vgtEvent      = vgtEvents[i];
            releaseMemInfo.dstAddr       = gpuEventSlotVa;
            releaseMemInfo.dataSel       = data_sel__me_release_mem__send_32_bit_low;
            releaseMemInfo.data          = GpuEvent::SetValue;

            dwordsWritten += m_cmdUtil.ExplicitBuildReleaseMem(releaseMemInfo,
                                                               VoidPtrInc(pBuffer, sizeof(uint32) * dwordsWritten),
                                                               0,
                                                               0);
        }
    }

    // Issue a WRITE_DATA command to SET the remaining (unused) event slots as early as possible.
    if (vgtEventCount < numEventSlots)
    {
        WriteDataInfo writeData = {};
        writeData.engineType    = engineType;
        writeData.engineSel     = engine_sel__me_write_data__micro_engine;
        writeData.dstSel        = dst_sel__me_write_data__memory;
        writeData.dstAddr       = gpuEventStartVa + (sizeof(uint32) * vgtEventCount);

        const uint32 dword      = GpuEvent::SetValue;

        dwordsWritten += CmdUtil::BuildWriteDataPeriodic(writeData,
                                                         1,
                                                         numEventSlots - vgtEventCount,
                                                         &dword,
                                                         VoidPtrInc(pBuffer, sizeof(uint32) * dwordsWritten));
    }

    return dwordsWritten;
}

} // Gfx9
} // Pal
