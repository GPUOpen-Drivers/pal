/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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
    if (TestAnyFlagSet(accessMask, CoherCpu | CoherMemory | CoherPresent))
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
    if (TestAnyFlagSet(accessMask, CacheCoherencyBlt))
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

    if (TestAnyFlagSet(accessMask, CoherCpu | CoherMemory | CoherPresent))
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
    PAL_ASSERT(pBarrier->subresRange.numPlanes == 1);

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
        pBarrierOps->layoutTransitions.fastClearEliminate = (transitionInfo.flags.skipFce == false);
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
        {
            const auto& image     = static_cast<const Pal::Image&>(*pBarrier->pImage);
            const auto& gfx9Image = static_cast<const Gfx9::Image&>(*image.GetGfxImage());
            if (gfx9Image.HasDccStateMetaData(pBarrier->subresRange))
            {
                pBarrierOps->layoutTransitions.updateDccStateMetadata = 1;
            }

            pBarrierOps->layoutTransitions.initMaskRam = 1;
        }
        break;
    case None:
    default:
        PAL_NEVER_CALLED();
        break;
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
// Check to see if need update DCC state metadata
static void UpdateDccStateMetaDataIfNeeded(
    GfxCmdBuffer*                 pCmdBuf,
    CmdStream*                    pCmdStream,
    const ImgBarrier*             pBarrier,
    Developer::BarrierOperations* pBarrierOps)
{
    const auto& palImage      = static_cast<const Pal::Image&>(*pBarrier->pImage);
    const auto& gfx9Image     = static_cast<const Image&>(*palImage.GetGfxImage());
    const auto& layoutToState = gfx9Image.LayoutToColorCompressionState();
    const auto& subresRange   = pBarrier->subresRange;

    if (gfx9Image.HasDccStateMetaData(subresRange) &&
        (ImageLayoutCanCompressColorData(layoutToState, pBarrier->oldLayout) == false) &&
        (ImageLayoutCanCompressColorData(layoutToState, pBarrier->newLayout)))
    {
        pBarrierOps->layoutTransitions.updateDccStateMetadata = 1;

        gfx9Image.UpdateDccStateMetaData(pCmdStream,
                                         subresRange,
                                         true,
                                         pCmdBuf->GetEngineType(),
                                         Pm4Predicate::PredDisable);
    }
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
        if (info.flags.skipFce == false)
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
    PAL_ASSERT(imgBarrier.subresRange.numPlanes == 1);
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
    PAL_ASSERT(imgBarrier.subresRange.numPlanes == 1);
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
    PAL_ASSERT(imgBarrier.subresRange.numPlanes == 1);
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
        else if (layoutTransInfo.blt[0] == HwLayoutTransition::FastClearEliminate)
        {
            if (layoutTransInfo.flags.skipFce != 0)
            {
                auto& gfx9ImageNonConst = static_cast<Gfx9::Image&>(*image.GetGfxImage()); // Cast to modifiable value
                                                                                           // for skip-FCE optimization
                                                                                           // only.

                // Skip the fast clear eliminate for this image if the clear color is TC-compatible and the
                // optimization was enabled.
                pCmdBuf->AddFceSkippedImageCounter(&gfx9ImageNonConst);
            }
            else
            {
                RsrcProcMgr().FastClearEliminate(pCmdBuf,
                                                 pCmdStream,
                                                 gfx9Image,
                                                 imgBarrier.pQuadSamplePattern,
                                                 imgBarrier.subresRange);
            }
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
    PAL_ASSERT(pBarrierOps != nullptr);

    const EngineType engineType = pCmdBuf->GetEngineType();
    uint32*          pCmdSpace  = pCmdStream->ReserveCommands();

    if (pCmdBuf->GetGfxCmdBufState().flags.cpBltActive &&
        TestAnyFlagSet(stageMask, PipelineStageBlt | PipelineStageBottomOfPipe))
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
    if (TestAnyFlagSet(stageMask, PipelineStageBlt) || TestAnyFlagSet(accessMask, CacheCoherencyBlt))
    {
        pCmdBuf->OptimizePipeAndCacheMaskForRelease(&stageMask, &accessMask);
    }

    // OptimizePipeAndCacheMaskForRelease() has converted these BLT coherency flags to more specific ones.
    PAL_ASSERT(TestAnyFlagSet(accessMask, CacheCoherencyBlt) == false);

    AcqRelSyncToken syncToken             = {};
    bool            issueSyncEvent        = false;
    bool            issueRbCacheSyncEvent = false; // If set, this EOP event is CACHE_FLUSH_AND_INV_TS, not
                                                   // the pipeline stall-only version BOTTOM_OF_PIPE.

    constexpr uint32 StageVsDoneMask = PipelineStageVs | PipelineStageHs | PipelineStageDs | PipelineStageGs;
    const bool       hasRasterKill   = pCmdBuf->GetGfxCmdBufState().flags.rasterKillDrawsActive;

    const bool requiresEop    = TestAnyFlagSet(stageMask, PipelineStageEarlyDsTarget |
                                                          PipelineStageLateDsTarget  |
                                                          PipelineStageColorTarget   |
                                                          PipelineStageBottomOfPipe);
    // No VS_DONE event support from HW yet, and EOP event will be used instead. Optimize to use PS_DONE instead of
    // heavy VS_DONE (EOP) if there is ps wave. If no ps wave, PS_DONE doesn't safely ensures the completion of VS
    // and prior stage waves.
    const bool requiresPsDone = TestAnyFlagSet(stageMask, PipelineStagePs) ||
                                (TestAnyFlagSet(stageMask, StageVsDoneMask) && (hasRasterKill == false));
    const bool requiresVsDone = TestAnyFlagSet(stageMask, StageVsDoneMask) && hasRasterKill;
    const bool requiresCsDone = TestAnyFlagSet(stageMask, PipelineStageCs);

    // If any of the access mask bits that could result in RB sync are set, use CACHE_FLUSH_AND_INV_TS.
    // There is no way to INV the CB metadata caches during acquire. So at release always also invalidate if we are to
    // flush CB metadata. Furthermore, CACHE_FLUSH_AND_INV_TS_EVENT always flush & invalidate RB, so there is no need
    // to invalidate RB at acquire again.
    if (TestAnyFlagSet(accessMask, CoherColorTarget | CoherDepthStencilTarget))
    {
        // Issue a pipelined EOP event that writes timestamp to a GpuEvent slot when all prior GPU work completes.
        syncToken.type        = static_cast<uint32>(AcqRelEventType::Eop);
        issueSyncEvent        = true;
        issueRbCacheSyncEvent = true;

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
    else if (requiresEop || requiresVsDone || (requiresCsDone && requiresPsDone))
    {
        // Implement set with an EOP event written when all prior GPU work completes if either:
        // 1. End of RB or end of whole pipe is required,
        // 2. There are Vs/Hs/Ds/Gs that require a graphics pipe done event, but there is no PS wave. Since there is no
        //    VS_DONE event, we have to conservatively use an EOP event,
        // 3. Both graphics and compute events are required.
        syncToken.type     = static_cast<uint32>(AcqRelEventType::Eop);
        issueSyncEvent     = true;
    }
    else if (requiresCsDone)
    {
        // Implement set with an EOS event waiting for CS waves to complete.
        syncToken.type     = static_cast<uint32>(AcqRelEventType::CsDone);
        issueSyncEvent     = true;
    }
    else if (requiresPsDone)
    {
        // Implement set with an EOS event waiting for PS waves to complete.
        syncToken.type     = static_cast<uint32>(AcqRelEventType::PsDone);
        issueSyncEvent     = true;
    }

    uint32 coherCntl = 0;
    uint32 gcrCntl   = 0;

    if (IsGfx9(m_gfxIpLevel))
    {
        uint32 cacheSyncFlags = Gfx9ConvertToReleaseSyncFlags(accessMask, flushLlc, pBarrierOps);

        const uint32 tcCacheOp = static_cast<uint32>(SelectTcCacheOp(&cacheSyncFlags));

        // The cache sync requests can be cleared by single release pass.
        PAL_ASSERT(cacheSyncFlags == 0);

        coherCntl = Gfx9TcCacheOpConversionTable[tcCacheOp];

        // If we have cache sync request yet don't assign any VGT event, we need to issue a dummy one.
        if ((issueSyncEvent == false) && (coherCntl != 0))
        {
            // Flush at earliest supported pipe point for RELEASE_MEM (CS_DONE always works).
            syncToken.type = static_cast<uint32>(AcqRelEventType::CsDone);
            issueSyncEvent = true;
        }
    }
    else
    {
        PAL_ASSERT(IsGfx10Plus(m_gfxIpLevel));
        gcrCntl = Gfx10BuildReleaseGcrCntl(accessMask, flushLlc, pBarrierOps);
    }

    // Pick EOP event if GCR cache sync is requested, EOS event is not supported.
    if (issueSyncEvent && (syncToken.type != static_cast<uint32>(AcqRelEventType::Eop)) && (gcrCntl != 0))
    {
        syncToken.type = static_cast<uint32>(AcqRelEventType::Eop);
    }

    // Issue RELEASE_MEM packet
    if (issueSyncEvent)
    {
        ExplicitReleaseMemInfo releaseMemInfo = {};
        releaseMemInfo.engineType = engineType;
        releaseMemInfo.coherCntl  = coherCntl;
        releaseMemInfo.gcrCntl    = gcrCntl;

        if (syncToken.type == static_cast<uint32>(AcqRelEventType::Eop))
        {
            releaseMemInfo.vgtEvent = issueRbCacheSyncEvent ? CACHE_FLUSH_AND_INV_TS_EVENT : BOTTOM_OF_PIPE_TS;
            pBarrierOps->pipelineStalls.eopTsBottomOfPipe = 1;
        }
        else if (syncToken.type == static_cast<uint32>(AcqRelEventType::PsDone))
        {
            releaseMemInfo.vgtEvent = PS_DONE;
            pBarrierOps->pipelineStalls.eosTsPsDone = 1;
        }
        else if (syncToken.type == static_cast<uint32>(AcqRelEventType::CsDone))
        {
            releaseMemInfo.vgtEvent = CS_DONE;
            pBarrierOps->pipelineStalls.eosTsCsDone = 1;
        }

        // Request sync fence value after VGT event type is finalized.
        syncToken.fenceVal = pCmdBuf->GetNextAcqRelFenceVal(static_cast<AcqRelEventType>(syncToken.type));

        {
            releaseMemInfo.dstAddr = pCmdBuf->AcqRelFenceValGpuVa(static_cast<AcqRelEventType>(syncToken.type));
            releaseMemInfo.dataSel = data_sel__me_release_mem__send_32_bit_low;
            releaseMemInfo.data    = syncToken.fenceVal;
        }

        pCmdSpace += m_cmdUtil.ExplicitBuildReleaseMem(releaseMemInfo, pCmdSpace, 0, 0);
    }
    else if (gcrCntl != 0)
    {
        // This is an optimization path to use AcquireMem for GcrCntl only (issueSyncEvent = false) case as
        // ReleaseMem requires an EOP or EOS event.
        Gfx10ReleaseMemGcrCntl relMemGcrCntl;
        relMemGcrCntl.u32All = gcrCntl;

        PAL_ASSERT(IsGfx10Plus(m_gfxIpLevel));

        // Create info for ACQUIRE_MEM. Initialize common part at here.
        ExplicitAcquireMemInfo acquireMemInfo = {};
        acquireMemInfo.engineType   = engineType;
        acquireMemInfo.baseAddress  = FullSyncBaseAddr;
        acquireMemInfo.sizeBytes    = FullSyncSize;
        acquireMemInfo.flags.usePfp = 0;

        Gfx10AcquireMemGcrCntl acqMemGcrCntl = {};
        acqMemGcrCntl.bits.glmWb      = relMemGcrCntl.bits.glmWb;
        acqMemGcrCntl.bits.glmInv     = relMemGcrCntl.bits.glmInv;
        acqMemGcrCntl.bits.glvInv     = relMemGcrCntl.bits.glvInv;
        acqMemGcrCntl.bits.gl1Inv     = relMemGcrCntl.bits.gl1Inv;
        acqMemGcrCntl.bits.gl2Us      = relMemGcrCntl.bits.gl2Us;
        acqMemGcrCntl.bits.gl2Range   = relMemGcrCntl.bits.gl2Range;
        acqMemGcrCntl.bits.gl2Discard = relMemGcrCntl.bits.gl2Discard;
        acqMemGcrCntl.bits.gl2Inv     = relMemGcrCntl.bits.gl2Inv;
        acqMemGcrCntl.bits.gl2Wb      = relMemGcrCntl.bits.gl2Wb;
        acqMemGcrCntl.bits.seq        = relMemGcrCntl.bits.seq;

        acquireMemInfo.gcrCntl = acqMemGcrCntl.u32All;

        // Build ACQUIRE_MEM packet.
        pCmdSpace += m_cmdUtil.ExplicitBuildAcquireMem(acquireMemInfo, pCmdSpace);
    }
    else
    {
        PAL_ALERT_ALWAYS_MSG("Barrier-release does nothing, need to validate the correctness of this barrier call.");
    }

    // Update command buffer state value which will be used for optimization.
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
        stageMask  &= ~PipelineStagesGraphicsOnly;
        accessMask &= ~CacheCoherencyGraphicsOnly;
    }

    // BuildWaitRegMem waits in the ME, if the waitPoint needs to stall at the PFP request a PFP/ME sync.
    const bool pfpSyncMe = TestAnyFlagSet(stageMask, PipelineStageTopOfPipe         |
                                                     PipelineStageFetchIndirectArgs |
                                                     PipelineStageFetchIndices);

    uint32* pCmdSpace = pCmdStream->ReserveCommands();

    uint32 syncTokenToWait[static_cast<uint32>(AcqRelEventType::Count)] = {};
    bool hasValidSyncToken = false;

    // Merge synchronization timestamp entries in the list.
    for (uint32 i = 0; i < syncTokenCount; i++)
    {
        const AcqRelSyncToken curSyncToken = pSyncTokens[i];

        PAL_ASSERT(curSyncToken.type < static_cast<uint32>(AcqRelEventType::Count));

        if (curSyncToken.fenceVal != AcqRelFenceResetVal)
        {
            syncTokenToWait[curSyncToken.type] = Max(curSyncToken.fenceVal, syncTokenToWait[curSyncToken.type]);
            hasValidSyncToken = true;
        }
    }

    {
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
            // Create info for ACQUIRE_MEM. Initialize common part at here.
            ExplicitAcquireMemInfo acquireMemInfo = {};
            acquireMemInfo.engineType   = engineType;
            acquireMemInfo.baseAddress  = rangeStartAddr;
            acquireMemInfo.sizeBytes    = rangeSize;
            acquireMemInfo.flags.usePfp = pfpSyncMe ? 1 : 0;

            if (IsGfx9(m_gfxIpLevel))
            {
                uint32 cacheSyncFlags = Gfx9ConvertToAcquireSyncFlags(accessMask,
                                                                      engineType,
                                                                      invalidateLlc,
                                                                      pBarrierOps);

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
                    pCmdSpace += m_cmdUtil.ExplicitBuildAcquireMem(acquireMemInfo, pCmdSpace);
                }
            }
            else
            {
                PAL_ASSERT(IsGfx10Plus(m_gfxIpLevel));

                // The only difference between the GFX9 and GFX10+ versions of this packet are that GFX10+ added a new
                // "gcr_cntl" field.
                acquireMemInfo.gcrCntl = Gfx10BuildAcquireGcrCntl(accessMask,
                                                                  invalidateLlc,
                                                                  rangeStartAddr,
                                                                  rangeSize,
                                                                  (acquireMemInfo.coherCntl != 0),
                                                                  pBarrierOps);

                // GFX10+'s COHER_CNTL only controls RB flush/inv. "acquire" deson't need to invalidate RB because
                // "release" always flush & invalidate RB, so we never need to set COHER_CNTL here.
                if (acquireMemInfo.gcrCntl != 0)
                {
                    // Build ACQUIRE_MEM packet.
                    pCmdSpace += m_cmdUtil.ExplicitBuildAcquireMem(acquireMemInfo, pCmdSpace);
                }
            }
        }
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

        if (waitedEopFenceVal >= cmdBufState.fences.rasterKillDrawsExecFenceVal)
        {
            // An EOP release sync that is issued after the latest rasterization kill draws must have completed, so
            // mark rasterization kill draws inactive.
            pCmdBuf->SetGfxCmdBufRasterKillDrawsState(false);
        }
    }

    const uint32 waitedCsDoneFenceVal = syncTokenToWait[static_cast<uint32>(AcqRelEventType::CsDone)];

    if ((waitedCsDoneFenceVal != AcqRelFenceResetVal) &&
        (waitedCsDoneFenceVal >= cmdBufState.fences.csBltExecCsDoneFenceVal))
    {
        // An CS_DONE release sync that is issued after the latest CS BLT must have completed, so mark CS BLT idle.
        pCmdBuf->SetGfxCmdBufCsBltState(false);
    }

    pCmdStream->CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// Figure out the specific BLT operation(s) necessary to convert a color image from one ImageLayout to another.
LayoutTransitionInfo Device::PrepareColorBlt(
    const GfxCmdBuffer* pCmdBuf,
    const Pal::Image&   image,
    const SubresRange&  subresRange,
    ImageLayout         oldLayout,
    ImageLayout         newLayout
    ) const
{
    PAL_ASSERT(subresRange.numPlanes == 1);

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
        (pCmdBuf->IsGraphicsSupported() &&
         (gfx9Image.GetFastClearEliminateMetaDataAddr(subresRange.startSubres) != 0));

    const bool isMsaaImage = (image.GetImageCreateInfo().samples > 1);

    LayoutTransitionInfo transitionInfo = {};

    uint32 bltIndex = 0;

    if ((oldState != ColorDecompressed) && (newState == ColorDecompressed))
    {
        if (gfx9ImageConst.HasDccData())
        {
            if ((oldState == ColorCompressed) || pSubresInfo->flags.supportMetaDataTexFetch)
            {
                transitionInfo.blt[bltIndex++] = HwLayoutTransition::DccDecompress;

                if (RsrcProcMgr().WillDecompressWithCompute(pCmdBuf, gfx9ImageConst, subresRange))
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
            transitionInfo.blt[bltIndex++] = HwLayoutTransition::MsaaColorDecompress;
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
                transitionInfo.blt[bltIndex++] = HwLayoutTransition::DccDecompress;

                if (RsrcProcMgr().WillDecompressWithCompute(pCmdBuf, gfx9ImageConst, subresRange))
                {
                    transitionInfo.flags.useComputePath = 1;
                }
            }
            else
            {
                transitionInfo.blt[bltIndex++] = HwLayoutTransition::FmaskDecompress;
            }
        }
        else
        {
            // if the image is TC compatible just need to do a fast clear eliminate
            if (fastClearEliminateSupported)
            {
                transitionInfo.blt[bltIndex++] = HwLayoutTransition::FastClearEliminate;
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
            // The image has been fast cleared with a non-TC compatible color or the FCE optimization is not
            // enabled.
            transitionInfo.blt[bltIndex++] = HwLayoutTransition::FastClearEliminate;

            if (gfx9ImageConst.IsFceOptimizationEnabled() &&
                (gfx9ImageConst.HasSeenNonTcCompatibleClearColor() == false))
            {
                // Skip the fast clear eliminate for this image if the clear color is TC-compatible and the
                // optimization was enabled.
                transitionInfo.flags.skipFce = 1;
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
    PAL_ASSERT(subresRange.numPlanes == 1);

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
    PAL_ASSERT(imgBarrier.subresRange.numPlanes == 1);
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
        constexpr uint32 MaybeTextureCache  = CacheCoherencyBlt | CoherPresent | CoherShader | CoherSampleRate;
        constexpr uint32 MaybeFixedFunction = CacheCoherencyBlt | CoherPresent | CoherColorTarget |
                                              CoherDepthStencilTarget;

        if ((TestAnyFlagSet(srcAccessMask, MaybeFixedFunction) && TestAnyFlagSet(dstAccessMask, MaybeTextureCache)) ||
            (TestAnyFlagSet(srcAccessMask, MaybeTextureCache) && TestAnyFlagSet(dstAccessMask, MaybeFixedFunction)))
        {
            needRefreshL2 = true;
        }
    }

    return needRefreshL2;
}

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

            transitionList[i].pImgBarrier                  = &imageBarrier;
            transitionList[i].layoutTransInfo              = layoutTransInfo;
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
                globallyAvailable = true;
            }

            UpdateDccStateMetaDataIfNeeded(pCmdBuf, pCmdStream, &imageBarrier, pBarrierOps);
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
                    const auto& palImage  = static_cast<const Pal::Image&>(*transition.pImgBarrier->pImage);
                    const auto& gfx9Image = static_cast<const Image&>(*palImage.GetGfxImage());

                    // Metadata initialization is categorized as direct metadata write, always flush invalidate LLC for
                    // InitMaskRam to align with legacy barrier implementation.
                    const bool initMaskRamNeedRefreshLlc =
                        (transition.layoutTransInfo.blt[0] == HwLayoutTransition::InitMaskRam) &&
                        gfx9Image.NeedFlushForMetadataPipeMisalignment(transition.pImgBarrier->subresRange);

                    globallyAvailable |= initMaskRamNeedRefreshLlc;

                    IssueAcquireSync(pCmdBuf,
                                     pCmdStream,
                                     transition.bltStageMask,
                                     transition.bltAccessMask,
                                     transition.waMetaMisalignNeedRefreshLlc | initMaskRamNeedRefreshLlc,
                                     palImage.GetGpuVirtualAddr(),
                                     palImage.GetGpuMemSize(),
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

            UpdateDccStateMetaDataIfNeeded(pCmdBuf, pCmdStream, &imgBarrier, pBarrierOps);
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

        uint32 dstGlobalAccessMask = barrierAcquireInfo.dstGlobalAccessMask;
        bool   issueGlobalSync     = (dstGlobalAccessMask > 0) ||
                                     ((barrierAcquireInfo.memoryBarrierCount == 0) &&
                                      (barrierAcquireInfo.imageBarrierCount == 0));
        bool   invalidateLlc       = false;

        // Loop through memory transitions to check if can group non-ranged acquire sync into global sync
        for (uint32 i = 0; i < barrierAcquireInfo.memoryBarrierCount; i++)
        {
            const MemBarrier& barrier    = barrierAcquireInfo.pMemoryBarriers[i];
            const gpusize rangedSyncSize = barrier.memory.size;

            if (rangedSyncSize > CmdUtil::Gfx10AcquireMemGl1Gl2RangedCheckMaxSurfaceSizeBytes)
            {
                dstGlobalAccessMask |= barrier.dstAccessMask;
                issueGlobalSync      = true;
            }
        }

        // Loop through image transitions to check if can group non-ranged acquire sync into global sync
        for (uint32 i = 0; i < barrierAcquireInfo.imageBarrierCount; i++)
        {
            const ImgBarrier& imgBarrier = barrierAcquireInfo.pImageBarriers[i];
            const Pal::Image& image      = static_cast<const Pal::Image&>(*imgBarrier.pImage);

            if (image.GetGpuMemSize() > CmdUtil::Gfx10AcquireMemGl1Gl2RangedCheckMaxSurfaceSizeBytes)
            {
                dstGlobalAccessMask |= imgBarrier.dstAccessMask;
                invalidateLlc       |= transitionList[i].waMetaMisalignNeedRefreshLlc;
                issueGlobalSync      = true;
            }
        }

        // Issue acquire for global cache sync.
        bool syncTokenWaited = false;
        if (issueGlobalSync)
        {
            IssueAcquireSync(pCmdBuf,
                             pCmdStream,
                             barrierAcquireInfo.dstStageMask,
                             dstGlobalAccessMask,
                             invalidateLlc,
                             FullSyncBaseAddr,
                             FullSyncSize,
                             syncTokenCount,
                             pCurSyncTokens,
                             pBarrierOps);

            syncTokenWaited = true;
        }

        // Loop through memory transitions to issue client-requested acquires for ranged memory syncs.
        for (uint32 i = 0; i < barrierAcquireInfo.memoryBarrierCount; i++)
        {
            const MemBarrier& barrier              = barrierAcquireInfo.pMemoryBarriers[i];
            const GpuMemSubAllocInfo& memAllocInfo = barrier.memory;

            const gpusize rangedSyncBaseAddr = memAllocInfo.pGpuMemory->Desc().gpuVirtAddr + memAllocInfo.offset;
            const gpusize rangedSyncSize     = memAllocInfo.size;

            if ((rangedSyncSize <= CmdUtil::Gfx10AcquireMemGl1Gl2RangedCheckMaxSurfaceSizeBytes) &&
                ((barrier.dstAccessMask & ~dstGlobalAccessMask) != 0))
            {
                IssueAcquireSync(pCmdBuf,
                                 pCmdStream,
                                 barrierAcquireInfo.dstStageMask,
                                 barrier.dstAccessMask,
                                 false,
                                 rangedSyncBaseAddr,
                                 rangedSyncSize,
                                 syncTokenWaited ? 0 : syncTokenCount,
                                 syncTokenWaited ? nullptr : pCurSyncTokens,
                                 pBarrierOps);

                syncTokenWaited = true;
            }
        }

        // Loop through image transitions to issue client-requested acquires for image syncs.
        for (uint32 i = 0; i < barrierAcquireInfo.imageBarrierCount; i++)
        {
            const ImgBarrier& imgBarrier = barrierAcquireInfo.pImageBarriers[i];
            PAL_ASSERT(imgBarrier.subresRange.numPlanes == 1);
            const Pal::Image& image      = static_cast<const Pal::Image&>(*imgBarrier.pImage);

            if ((image.GetGpuMemSize() <= CmdUtil::Gfx10AcquireMemGl1Gl2RangedCheckMaxSurfaceSizeBytes) &&
                ((imgBarrier.dstAccessMask & ~dstGlobalAccessMask) != 0))
            {
                IssueAcquireSync(pCmdBuf,
                                 pCmdStream,
                                 barrierAcquireInfo.dstStageMask,
                                 imgBarrier.dstAccessMask,
                                 transitionList[i].waMetaMisalignNeedRefreshLlc,
                                 image.GetGpuVirtualAddr(),
                                 image.GetGpuMemSize(),
                                 syncTokenWaited ? 0 : syncTokenCount,
                                 syncTokenWaited ? nullptr : pCurSyncTokens,
                                 pBarrierOps);

                syncTokenWaited = true;
            }
        }
    }
}

// =====================================================================================================================
// BarrierReleaseThenAcquire is effectively the same as calling BarrierRelease immediately by calling BarrierAcquire.
// This is a convenience method for clients implementing single point barriers, and is functionally equivalent to the
// current CmdBarrier() interface.
// The BarrierReleaseand BarrierAcquire calls are associated by a sync token.
void Device::BarrierReleaseThenAcquire(
    GfxCmdBuffer*                 pCmdBuf,
    CmdStream*                    pCmdStream,
    const AcquireReleaseInfo&     barrierInfo,
    Developer::BarrierOperations* pBarrierOps
    ) const
{
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
                PAL_ASSERT(barrierInfo.pImageBarriers[i].subresRange.numPlanes == 1);
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

    const AcqRelSyncToken syncToken = BarrierRelease(pCmdBuf,
                                                     pCmdStream,
                                                     releaseInfo,
                                                     pBarrierOps,
                                                     waMetaMisalignNeedRefreshLlc);

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

    BarrierAcquire(pCmdBuf, pCmdStream, acquireInfo, 1, &syncToken, pBarrierOps, waMetaMisalignNeedRefreshLlc);
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
    PAL_ASSERT(pImgBarrier->subresRange.numPlanes == 1);
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

            AcqRelColorTransition(pCmdBuf, pCmdStream, *pImgBarrier, layoutTransInfo, pBarrierOps);
        }
    }
}

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

    if (TestAnyFlagSet(accessMask, CoherCpu | CoherMemory | CoherPresent))
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

    // GLM_WB[0]  - write-back control for the meta-data cache of GL2. L2MD is write-through, ignore this bit.
    // GLK_WB[0]  - write-back control for shaded scalar L0 cache
    gcrCntl.bits.glmWb = 0;
    gcrCntl.bits.glkWb = 0;

    // GLM_INV[0] - invalidate enable for the meta-data cache of GL2
    // GLK_INV[0] - invalidate enable for shader scalar L0 cache
    // GLV_INV[0] - invalidate enable for shader vector L0 cache
    // GL1_INV[0] - invalidate enable for GL1
    if (TestAnyFlagSet(accessMask, CacheCoherencyBlt | CoherShader | CoherStreamOut |
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
    if (TestAnyFlagSet(accessMask, CoherCpu | CoherMemory | CoherPresent))
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

    // Don't set bits gl1Range/gl2Range if there are no other gcrCntl bits set.
    if (gcrCntl.u32All != 0)
    {
        // The L1 / L2 caches are physical address based. When specify the range, the GCR will perform virtual address
        // to physical address translation before the wb / inv. If the acquired op is full sync, we must ignore the
        // range, otherwise page fault may occur because page table cannot cover full range virtual address.
        //    When the source address is virtual , the GCR block will have to perform the virtual address to physical
        //    address translation before the wb / inv. Since the pages in memory are a collection of fragments, you
        //    can't specify the full range without walking into a page that has no PTE triggering a fault. In the cases
        //    where the driver wants to wb / inv the entire cache, you should not use range based method, and instead
        //    flush the entire cache without it. The range based method is not meant to be used this way, it is for
        //    selective page invalidation.
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
    }

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
    PAL_ASSERT(imgBarrier.subresRange.numPlanes == 1);
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
        else if (layoutTransInfo.blt[0] == HwLayoutTransition::FastClearEliminate)
        {
            if (layoutTransInfo.flags.skipFce != 0)
            {
                auto& gfx9ImageNonConst = static_cast<Gfx9::Image&>(*image.GetGfxImage()); // Cast to modifiable value
                                                                                           // for skip-FCE optimization
                                                                                           // only.

                // Skip the fast clear eliminate for this image if the clear color is TC-compatible and the
                // optimization was enabled.
                pCmdBuf->AddFceSkippedImageCounter(&gfx9ImageNonConst);
            }
            else
            {
                RsrcProcMgr().FastClearEliminate(pCmdBuf,
                                                 pCmdStream,
                                                 gfx9Image,
                                                 imgBarrier.pQuadSamplePattern,
                                                 imgBarrier.subresRange);
            }
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

    if (pCmdBuf->GetGfxCmdBufState().flags.cpBltActive &&
        TestAnyFlagSet(stageMask, PipelineStageBlt | PipelineStageBottomOfPipe))
    {
        // We must guarantee that all prior CP DMA accelerated blts have completed before we write this event because
        // the CmdSetEvent and CmdResetEvent functions expect that the prior blts have reached the post-blt stage by
        // the time the event is written to memory. Given that our CP DMA blts are asynchronous to the pipeline stages
        // the only way to satisfy this requirement is to force the MEC to stall until the CP DMAs are completed.
        pBarrierOps->pipelineStalls.syncCpDma = 1;
        pCmdSpace += m_cmdUtil.BuildWaitDmaData(pCmdSpace);
        pCmdBuf->SetGfxCmdBufCpBltState(false);
    }

    if (TestAnyFlagSet(stageMask, PipelineStageBlt) || TestAnyFlagSet(accessMask, CacheCoherencyBlt))
    {
        pCmdBuf->OptimizePipeAndCacheMaskForRelease(&stageMask, &accessMask);
    }

    // OptimizePipeAndCacheMaskForRelease() has converted these BLT coherency flags to more specific ones.
    PAL_ASSERT(TestAnyFlagSet(accessMask, CacheCoherencyBlt) == false);

    // Issue RELEASE_MEM packets to flush caches (optional) and signal gpuEvent.
    const uint32          numEventSlots               = Parent()->ChipProperties().gfxip.numSlotsPerEvent;
    const BoundGpuMemory& gpuEventBoundMemObj         = static_cast<const GpuEvent*>(pGpuEvent)->GetBoundGpuMemory();
    PAL_ASSERT(gpuEventBoundMemObj.IsBound());
    const gpusize         gpuEventStartVa             = gpuEventBoundMemObj.GpuVirtAddr();

    AcqRelEventType syncEventType[MaxSlotsPerEvent] = {};
    uint32          syncEventCount                  = 0;
    bool            issueRbCacheSyncEvent           = false; // If set, this EOP event is CACHE_FLUSH_AND_INV_TS, not
                                                             // the pipeline stall-only version BOTTOM_OF_PIPE.

    const bool requiresEop    = TestAnyFlagSet(stageMask, PipelineStageEarlyDsTarget |
                                                          PipelineStageLateDsTarget  |
                                                          PipelineStageColorTarget   |
                                                          PipelineStageBottomOfPipe);
    const bool requiresPsDone = TestAnyFlagSet(stageMask, PipelineStagePs);
    // If no ps wave, PS_DONE doesn't safely ensures the completion of VS and prior stage waves.
    const bool requiresVsDone = (requiresPsDone == false) &&
                                TestAnyFlagSet(stageMask, PipelineStageVs |
                                                          PipelineStageHs |
                                                          PipelineStageDs |
                                                          PipelineStageGs);
    const bool requiresCsDone = TestAnyFlagSet(stageMask, PipelineStageCs);

    // If any of the access mask bits that could result in RB sync are set, use CACHE_FLUSH_AND_INV_TS.
    // There is no way to INV the CB metadata caches during acquire. So at release always also invalidate if we are to
    // flush CB metadata. Furthermore, CACHE_FLUSH_AND_INV_TS_EVENT always flush & invalidate RB, so there is no need
    // to invalidate RB at acquire again.
    if (TestAnyFlagSet(accessMask, CoherColorTarget | CoherDepthStencilTarget))
    {
        // Issue a pipelined EOP event that writes timestamp to a GpuEvent slot when all prior GPU work completes.
        syncEventType[syncEventCount++] = AcqRelEventType::Eop;
        issueRbCacheSyncEvent         = true;

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
    else if (requiresEop || requiresVsDone)
    {
        // Implement set with an EOP event written when all prior GPU work completes if either:
        // 1. End of RB or end of whole pipe is required,
        // 2. There are Vs/Hs/Ds/Gs that require a graphics pipe done event, but there is no PS wave. Since there is no
        //    VS_DONE event, we have to conservatively use an EOP event.
        syncEventType[syncEventCount++] = AcqRelEventType::Eop;
    }
    else
    {
        // The signal/wait event may have multiple slots, we can utilize it to issue separate EOS event for PS and CS
        // waves.
        if (requiresCsDone)
        {
            // Implement set/reset with an EOS event waiting for CS waves to complete.
            syncEventType[syncEventCount++] = AcqRelEventType::CsDone;
        }

        if (requiresPsDone)
        {
            // Implement set with an EOS event waiting for PS waves to complete.
            syncEventType[syncEventCount++] = AcqRelEventType::PsDone;
        }

        if (syncEventCount > numEventSlots)
        {
            // Fall back to single EOP pipe point if available event slots are not sufficient for multiple pipe points.
            syncEventType[0] = AcqRelEventType::Eop;
            syncEventCount   = 1;
        }
    }

    uint32 coherCntl = 0;
    uint32 gcrCntl   = 0;

    if (IsGfx9(m_gfxIpLevel))
    {
        uint32 cacheSyncFlags = Gfx9ConvertToReleaseSyncFlags(accessMask, flushLlc, pBarrierOps);

        const uint32 tcCacheOp = static_cast<uint32>(SelectTcCacheOp(&cacheSyncFlags));

        // The cache sync requests can be cleared by single release pass.
        PAL_ASSERT(cacheSyncFlags == 0);

        coherCntl = Gfx9TcCacheOpConversionTable[tcCacheOp];
    }
    else
    {
        PAL_ASSERT(IsGfx10Plus(m_gfxIpLevel));
        gcrCntl = Gfx10BuildReleaseGcrCntl(accessMask, flushLlc, pBarrierOps);
    }

    // If we have cache sync request yet don't assign any VGT event, we need to issue a dummy one.
    if ((syncEventCount == 0) && ((coherCntl != 0) || (gcrCntl != 0)))
    {
        // Flush at earliest supported pipe point for RELEASE_MEM (CS_DONE always works).
        syncEventType[syncEventCount++] = AcqRelEventType::CsDone;
    }

    // Pick EOP event if GCR cache sync is requested, EOS event is not supported.
    if ((syncEventCount > 0) && (gcrCntl != 0))
    {
        for (uint32 i = 0; i < syncEventCount; i++)
        {
            if (syncEventType[i] != AcqRelEventType::Eop)
            {
                syncEventType[0] = AcqRelEventType::Eop;
                syncEventCount   = 1;
                break;
            }
        }
    }

    // Issue releases with the requested eop/eos
    if (syncEventCount > 0)
    {
        // Build a WRITE_DATA command to first RESET event slots that will be set by event later on.
        WriteDataInfo writeData = {};
        writeData.engineType    = engineType;
        writeData.engineSel     = engine_sel__me_write_data__micro_engine;
        writeData.dstSel        = dst_sel__me_write_data__memory;
        writeData.dstAddr       = gpuEventStartVa;

        const uint32 dword = GpuEvent::ResetValue;

        pCmdSpace += CmdUtil::BuildWriteDataPeriodic(writeData, 1, syncEventCount, &dword, pCmdSpace);

        // Build RELEASE_MEM packet with requested eop/eos events at ME engine.
        ExplicitReleaseMemInfo releaseMemInfo = {};
        releaseMemInfo.engineType = engineType;
        releaseMemInfo.coherCntl  = coherCntl;
        releaseMemInfo.gcrCntl    = gcrCntl;

        for (uint32 i = 0; i < syncEventCount; i++)
        {
            VGT_EVENT_TYPE event = {};
            if (syncEventType[i] == AcqRelEventType::Eop)
            {
                releaseMemInfo.vgtEvent = issueRbCacheSyncEvent ? CACHE_FLUSH_AND_INV_TS_EVENT : BOTTOM_OF_PIPE_TS;
                pBarrierOps->pipelineStalls.eopTsBottomOfPipe = 1;
            }
            else if (syncEventType[i] == AcqRelEventType::PsDone)
            {
                releaseMemInfo.vgtEvent = PS_DONE;
                pBarrierOps->pipelineStalls.eosTsPsDone = 1;
            }
            else if (syncEventType[i] == AcqRelEventType::CsDone)
            {
                releaseMemInfo.vgtEvent = CS_DONE;
                pBarrierOps->pipelineStalls.eosTsCsDone = 1;
            }

            releaseMemInfo.dstAddr  = gpuEventStartVa + (i * sizeof(uint32));
            releaseMemInfo.dataSel  = data_sel__me_release_mem__send_32_bit_low;
            releaseMemInfo.data     = GpuEvent::SetValue;

            pCmdSpace += m_cmdUtil.ExplicitBuildReleaseMem(releaseMemInfo, pCmdSpace, 0, 0);
        }
    }

    // Issue a WRITE_DATA command to SET the remaining (unused) event slots as early as possible.
    if (syncEventCount < numEventSlots)
    {
        WriteDataInfo writeData = {};
        writeData.engineType    = engineType;
        writeData.engineSel     = engine_sel__me_write_data__micro_engine;
        writeData.dstSel        = dst_sel__me_write_data__memory;
        writeData.dstAddr       = gpuEventStartVa + (sizeof(uint32) * syncEventCount);

        const uint32 dword      = GpuEvent::SetValue;

        pCmdSpace += CmdUtil::BuildWriteDataPeriodic(writeData, 1, numEventSlots - syncEventCount, &dword, pCmdSpace);
    }

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
        stageMask &= ~PipelineStagesGraphicsOnly;
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
        // Create info for ACQUIRE_MEM. Initialize common part at here.
        ExplicitAcquireMemInfo acquireMemInfo = {};
        acquireMemInfo.engineType   = engineType;
        acquireMemInfo.baseAddress  = rangeStartAddr;
        acquireMemInfo.sizeBytes    = rangeSize;
        acquireMemInfo.flags.usePfp = TestAnyFlagSet(stageMask, PipelineStageTopOfPipe         |
                                                                PipelineStageFetchIndirectArgs |
                                                                PipelineStageFetchIndices);

        if (IsGfx9(m_gfxIpLevel))
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
                pCmdSpace += m_cmdUtil.ExplicitBuildAcquireMem(acquireMemInfo, pCmdSpace);
            }
        }
        else if (IsGfx10(m_gfxIpLevel))
        {
            // The only difference between the GFX9 and GFX10+ versions of this packet are that GFX10+ added a new
            // "gcr_cntl" field.
            acquireMemInfo.gcrCntl = Gfx10BuildAcquireGcrCntl(accessMask,
                                                              invalidateLlc,
                                                              rangeStartAddr,
                                                              rangeSize,
                                                              (acquireMemInfo.coherCntl != 0),
                                                              pBarrierOps);

            // GFX10's COHER_CNTL only controls RB flush/inv. "acquire" deson't need to invalidate RB because "release"
            // always flush & invalidate RB, so we never need to set COHER_CNTL here.
            if (acquireMemInfo.gcrCntl != 0)
            {
                // Build ACQUIRE_MEM packet.
                pCmdSpace += m_cmdUtil.ExplicitBuildAcquireMem(acquireMemInfo, pCmdSpace);
            }
        }
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
            PAL_ASSERT(imageBarrier.subresRange.numPlanes == 1);

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
                globallyAvailable = true;
            }

            UpdateDccStateMetaDataIfNeeded(pCmdBuf, pCmdStream, &imageBarrier, pBarrierOps);
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
                    IssueBlt(pCmdBuf, pCmdStream, transition.pImgBarrier, transition.layoutTransInfo, pBarrierOps);

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
            PAL_ASSERT(imgBarrier.subresRange.numPlanes == 1);

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

            UpdateDccStateMetaDataIfNeeded(pCmdBuf, pCmdStream, &imgBarrier, pBarrierOps);
        }

        const IGpuEvent* const* ppActiveEvents = ppGpuEvents;
        uint32 activeEventCount                = gpuEventCount;
        const IGpuEvent* pEvent                = nullptr;

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
            pEvent = pCmdBuf->GetInternalEvent();

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

        uint32 dstGlobalAccessMask = barrierAcquireInfo.dstGlobalAccessMask;
        bool   issueGlobalSync     = (dstGlobalAccessMask > 0) ||
                                     ((barrierAcquireInfo.memoryBarrierCount == 0) &&
                                      (barrierAcquireInfo.imageBarrierCount == 0));
        bool   invalidateLlc       = false;

        // Loop through memory transitions to check if can group non-ranged acquire sync into global sync
        for (uint32 i = 0; i < barrierAcquireInfo.memoryBarrierCount; i++)
        {
            const MemBarrier& barrier        = barrierAcquireInfo.pMemoryBarriers[i];
            const gpusize     rangedSyncSize = barrier.memory.size;

            if (rangedSyncSize > CmdUtil::Gfx10AcquireMemGl1Gl2RangedCheckMaxSurfaceSizeBytes)
            {
                dstGlobalAccessMask |= barrier.dstAccessMask;
                issueGlobalSync      = true;
            }
        }

        // Loop through image transitions to check if can group non-ranged acquire sync into global sync
        for (uint32 i = 0; i < barrierAcquireInfo.imageBarrierCount; i++)
        {
            const ImgBarrier& imgBarrier = barrierAcquireInfo.pImageBarriers[i];

            PAL_ASSERT(imgBarrier.subresRange.numPlanes == 1);
            const Pal::Image& image = static_cast<const Pal::Image&>(*imgBarrier.pImage);

            if (image.GetGpuMemSize() > CmdUtil::Gfx10AcquireMemGl1Gl2RangedCheckMaxSurfaceSizeBytes)
            {
                dstGlobalAccessMask |= imgBarrier.dstAccessMask;
                invalidateLlc       |= transitionList[i].waMetaMisalignNeedRefreshLlc;
                issueGlobalSync      = true;
            }
        }

        // Issue acquire for global cache sync.
        bool syncEventWaited = false;
        if (issueGlobalSync)
        {
            IssueAcquireSyncEvent(pCmdBuf,
                                  pCmdStream,
                                  barrierAcquireInfo.dstStageMask,
                                  dstGlobalAccessMask,
                                  invalidateLlc,
                                  FullSyncBaseAddr,
                                  FullSyncSize,
                                  activeEventCount,
                                  ppActiveEvents,
                                  pBarrierOps);

            syncEventWaited = true;
        }

        // Loop through memory transitions to issue client-requested acquires for ranged memory syncs.
        for (uint32 i = 0; i < barrierAcquireInfo.memoryBarrierCount; i++)
        {
            const MemBarrier&         barrier            = barrierAcquireInfo.pMemoryBarriers[i];
            const GpuMemSubAllocInfo& memAllocInfo       = barrier.memory;

            const gpusize rangedSyncBaseAddr = memAllocInfo.pGpuMemory->Desc().gpuVirtAddr + memAllocInfo.offset;
            const gpusize rangedSyncSize     = memAllocInfo.size;

            if ((rangedSyncSize <= CmdUtil::Gfx10AcquireMemGl1Gl2RangedCheckMaxSurfaceSizeBytes) &&
                ((barrier.dstAccessMask & ~dstGlobalAccessMask) != 0))
            {
                IssueAcquireSyncEvent(pCmdBuf,
                                      pCmdStream,
                                      barrierAcquireInfo.dstStageMask,
                                      barrier.dstAccessMask,
                                      false,
                                      rangedSyncBaseAddr,
                                      rangedSyncSize,
                                      syncEventWaited ? 0 : activeEventCount,
                                      syncEventWaited ? nullptr : ppActiveEvents,
                                      pBarrierOps);

                syncEventWaited = true;
            }
        }

        // Loop through image transitions to issue client-requested acquires for image syncs.
        for (uint32 i = 0; i < barrierAcquireInfo.imageBarrierCount; i++)
        {
            const ImgBarrier& imgBarrier = barrierAcquireInfo.pImageBarriers[i];

            PAL_ASSERT(imgBarrier.subresRange.numPlanes == 1);
            const Pal::Image& image = static_cast<const Pal::Image&>(*imgBarrier.pImage);

            if ((image.GetGpuMemSize() <= CmdUtil::Gfx10AcquireMemGl1Gl2RangedCheckMaxSurfaceSizeBytes) &&
                ((imgBarrier.dstAccessMask & ~dstGlobalAccessMask) != 0))
            {
                IssueAcquireSyncEvent(pCmdBuf,
                                      pCmdStream,
                                      barrierAcquireInfo.dstStageMask,
                                      imgBarrier.dstAccessMask,
                                      transitionList[i].waMetaMisalignNeedRefreshLlc,
                                      image.GetGpuVirtualAddr(),
                                      image.GetGpuMemSize(),
                                      syncEventWaited ? 0 : activeEventCount,
                                      syncEventWaited ? nullptr : ppActiveEvents,
                                      pBarrierOps);

                syncEventWaited = true;
            }
        }
    }
}

} // Gfx9
} // Pal
