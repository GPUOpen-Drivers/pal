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

using namespace Util;

namespace Pal
{
namespace Gfx9
{

static constexpr uint32 PipelineStagePfpMask =  PipelineStageTopOfPipe | PipelineStageFetchIndirectArgs |
                                                PipelineStageFetchIndices;

static constexpr uint32 PipelineStageHsGsMask = PipelineStageVs | PipelineStageHs | PipelineStageDs | PipelineStageGs;

static constexpr uint32 PipelineStageEopMask =  PipelineStageEarlyDsTarget | PipelineStageLateDsTarget |
                                                PipelineStageColorTarget   | PipelineStageBottomOfPipe;

// =====================================================================================================================
// Translate accessMask to ReleaseMemCaches.
ReleaseMemCaches Device::ConvertReleaseCacheFlags(
    uint32                        accessMask,  // Bitmask of CacheCoherencyUsageFlags.
    bool                          refreshTcc,
    Developer::BarrierOperations* pBarrierOps
    ) const
{
    PAL_ASSERT(pBarrierOps != nullptr);

    ReleaseMemCaches flags = {};

    if (refreshTcc)
    {
        flags.gl2Wb  = 1;
        flags.gl2Inv = 1;
        pBarrierOps->caches.flushTcc = 1;
        pBarrierOps->caches.invalTcc = 1;
    }
    else if (TestAnyFlagSet(accessMask, CoherCpu | CoherMemory | CoherPresent))
    {
        // At release we want to invalidate L2 so any future read to L2 would go down to memory, at acquire we want to
        // flush L2 so that main memory gets the latest data.
        flags.gl2Inv = 1;
        pBarrierOps->caches.invalTcc = 1;
    }

    return flags;
}

// =====================================================================================================================
// Translate accessMask to SyncGlxFlags.
SyncGlxFlags Device::ConvertAcquireCacheFlags(
    uint32                        accessMask,  // Bitmask of CacheCoherencyUsageFlags.
    bool                          refreshTcc,
    Developer::BarrierOperations* pBarrierOps
    ) const
{
    PAL_ASSERT(pBarrierOps != nullptr);

    SyncGlxFlags flags = SyncGlxNone;

    if (IsGfx9(m_gfxIpLevel))
    {
        // There are various BLTs (Copy, Clear, and Resolve) that can involve different caches based on what engine
        // does the BLT.
        // - If a graphics BLT occurred, alias to CB/DB. This was handled during release.
        // - If a compute BLT occurred, alias to shader. -> SyncGlkInv, SyncGlvInv, SyncGlmInv
        // - If a CP L2 BLT occured, alias to L2.        -> None (data is always in GL2 as it's the central cache)
        if (TestAnyFlagSet(accessMask, CoherShader | CacheCoherencyBlt))
        {
            flags |= SyncGlkInv | SyncGlvInv | SyncGlmInv;
            pBarrierOps->caches.invalSqK$        = 1;
            pBarrierOps->caches.invalTcp         = 1;
            pBarrierOps->caches.invalTccMetadata = 1;
        }

        if (TestAnyFlagSet(accessMask, CoherStreamOut))
        {
            // Read/write through V$ and read through K$.
            flags |= SyncGlkInv | SyncGlvInv;
            pBarrierOps->caches.invalSqK$ = 1;
            pBarrierOps->caches.invalTcp  = 1;
        }
    }
    else
    {
        // Invalidate all of the L0 and L1 caches (except for the I$, it can be left alone).
        // Note that PAL never really needs to write-back the K$ because we should never write to it.
        if (TestAnyFlagSet(accessMask, CacheCoherencyBlt | CoherShader | CoherStreamOut | CoherSampleRate))
        {
            flags |= SyncGlmInv | SyncGlkInv | SyncGlvInv | SyncGl1Inv;
            pBarrierOps->caches.invalTccMetadata = 1;
            pBarrierOps->caches.invalSqK$        = 1;
            pBarrierOps->caches.invalTcp         = 1;
            pBarrierOps->caches.invalGl1         = 1;
        }
    }

    if (refreshTcc)
    {
        flags |= SyncGl2WbInv;
        pBarrierOps->caches.flushTcc = 1;
        pBarrierOps->caches.invalTcc = 1;
    }
    else if (TestAnyFlagSet(accessMask, CoherCpu | CoherMemory | CoherPresent))
    {
        flags |= SyncGl2Wb;
        pBarrierOps->caches.flushTcc = 1;
    }

    return flags;
}

// =====================================================================================================================
static bool WaRefreshTccOnGlobalBarrier(
    uint32 srcCacheMask,
    uint32 dstCacheMask)
{
    constexpr uint32 MaybeTextureCache = CacheCoherencyBlt | CoherPresent | CoherShader | CoherSampleRate;

    // srcCacheMask == 0 and dstCacheMask == 0 case is expected if this is called from Release or Acquire,
    // and then we have to "assume the worst".
    return ((TestAnyFlagSet(srcCacheMask, MaybeTextureCache) || (srcCacheMask == 0)) &&
            TestAnyFlagSet(dstCacheMask, CoherColorTarget | CoherDepthStencilTarget)) ||
           (TestAnyFlagSet(srcCacheMask, CoherColorTarget | CoherDepthStencilTarget) &&
            (TestAnyFlagSet(dstCacheMask, MaybeTextureCache) || (dstCacheMask == 0)));
}

// =====================================================================================================================
//  We will need flush & inv L2 on MSAA Z, MSAA color, mips in the metadata tail, or any stencil.
//
// The driver assumes that all meta-data surfaces are channel-aligned, but there are cases where the HW does not
// actually channel-align the data.  In these cases, the L2 cache needs to be flushed and invalidated prior to the
// metadata being read by a shader.
static bool WaRefreshTccOnImageBarrier(
    const IImage*      pImage,
    const SubresRange& subresRange,
    uint32             srcAccessMask,
    uint32             dstAccessMask,
    bool               shaderMdAccessIndirectOnly)
{
    // Direct metadata access mode:
    // - Accessed as metadata for color target and depth stencil target.
    // - Accessed directly by shader read or write, like Cs fast clear metadata, copy or fixup metadata.
    // Indirect metadata access mode:
    // - Accessed as metadata for shader resource or UAV resource.
    //
    // The workaround requires inserting L2 flush and invalidation when transition between direct mode and indirect
    // mode. For split barrier, unfortunately not both srcAccessMask and dstAccessMask are available in either
    // CmdRelease() or CmdAcquire() call. A different solution is to refresh L2 at any cache write in CmdRelease().
    // L2 refresh can be optimized to be skipped for back to back same access mode.

    const auto& palImage  = static_cast<const Pal::Image&>(*pImage);
    const auto& gfx9Image = static_cast<const Image&>(*palImage.GetGfxImage());

    bool needRefreshL2 = false;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 740
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
#else
    if (gfx9Image.NeedFlushForMetadataPipeMisalignment(subresRange))
    {
        constexpr uint32 WaRefreshTccCoherMask = CoherColorTarget | CoherShaderWrite | CoherDepthStencilTarget |
                                                 CoherCopyDst     | CoherResolveDst  | CoherClear | CoherPresent;

        if (TestAnyFlagSet(srcAccessMask, WaRefreshTccCoherMask))
        {
            const bool backToBackDirectWrite =
                ((srcAccessMask == CoherColorTarget) && (dstAccessMask == CoherColorTarget)) ||
                ((srcAccessMask == CoherDepthStencilTarget) && (dstAccessMask == CoherDepthStencilTarget));

            // For CoherShaderWrite from image layout transition blt, it doesn't exactly indicate an indirect write
            // as image layout transition blt may direct write to fix up metadata which is direct write.
            const bool backToBackIndirectWrite =
                shaderMdAccessIndirectOnly                      &&
                TestAnyFlagSet(srcAccessMask, CoherShaderWrite) &&
                TestAnyFlagSet(dstAccessMask, CoherShaderWrite) &&
                (TestAnyFlagSet(srcAccessMask | dstAccessMask, ~CoherShader) == 0);

            // Can optimize to skip L2 refresh for back to back write with same access mode
            needRefreshL2 = (backToBackDirectWrite == false) && (backToBackIndirectWrite == false);
        }
    }
#endif

    return needRefreshL2;
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
    Pm4CmdBuffer*                 pCmdBuf,
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
// Group all acquire dst access mask into global acquire access mask. No support ranged cache operation.
static uint32 GetAcquireSyncDstGlobalAccessMask(
    const AcquireReleaseInfo& barrierInfo)
{
    uint32 dstGlobalAccessMask = barrierInfo.dstGlobalAccessMask;

    // Loop through memory transitions to check if can group non-ranged acquire sync into global sync
    for (uint32 i = 0; i < barrierInfo.memoryBarrierCount; i++)
    {
        const MemBarrier& barrier = barrierInfo.pMemoryBarriers[i];

        dstGlobalAccessMask |= barrier.dstAccessMask;
        dstGlobalAccessMask |= barrier.flags.globallyAvailable ? CoherMemory : 0;
    }

    // Loop through image transitions to check if can group non-ranged acquire sync into global sync
    for (uint32 i = 0; i < barrierInfo.imageBarrierCount; i++)
    {
        const ImgBarrier& imgBarrier = barrierInfo.pImageBarriers[i];

        dstGlobalAccessMask |= imgBarrier.dstAccessMask;
    }

    return dstGlobalAccessMask;
}

// =====================================================================================================================
// Prepare and get all image layout transition info
bool Device::GetAcqRelLayoutTransitionBltInfo(
    Pm4CmdBuffer*                 pCmdBuf,
    CmdStream*                    pCmdStream,
    const AcquireReleaseInfo&     barrierInfo,
    AcqRelAutoBuffer*             pTransitionList,
    Developer::BarrierOperations* pBarrierOps,
    uint32*                       pBltCount,
    uint32*                       pBltStageMask,
    uint32*                       pBltAccessMask,
    uint32*                       pSrcAccessMask = nullptr
    ) const
{
    uint32 bltStageMask  = 0;
    uint32 bltAccessMask = 0;
    uint32 srcAccessMask = 0;
    uint32 bltCount      = 0;
    bool   refreshTcc    = false;

    // Loop through image transitions to update client requested access.
    for (uint32 i = 0; i < barrierInfo.imageBarrierCount; i++)
    {
        const ImgBarrier& imageBarrier = barrierInfo.pImageBarriers[i];

        PAL_ASSERT(imageBarrier.subresRange.numPlanes == 1);

        // Update client requested access mask.
        srcAccessMask |= imageBarrier.srcAccessMask;

        // Prepare a layout transition BLT info and do pre-BLT preparation work.
        LayoutTransitionInfo layoutTransInfo = PrepareBltInfo(pCmdBuf, imageBarrier);

        (*pTransitionList)[i].pImgBarrier     = &imageBarrier;
        (*pTransitionList)[i].layoutTransInfo = layoutTransInfo;

        uint32 stageMask  = 0;
        uint32 accessMask = 0;
        if (layoutTransInfo.blt[0] != HwLayoutTransition::None)
        {
            GetBltStageAccessInfo(layoutTransInfo, &stageMask, &accessMask);

            (*pTransitionList)[i].bltStageMask  = stageMask;
            (*pTransitionList)[i].bltAccessMask = accessMask;

            // Add current BLT's stageMask into a stageMask/accessMask used for an all-in-one pre-BLT acquire.
            bltStageMask  |= stageMask;
            // Optimization: set preBltAccessMask=0 for transition to InitMaskRam since no need cache sync in this case.
            bltAccessMask |= (layoutTransInfo.blt[0] == HwLayoutTransition::InitMaskRam) ? 0 : accessMask;
            bltCount++;
        }
        else
        {
            (*pTransitionList)[i].bltStageMask  = 0;
            (*pTransitionList)[i].bltAccessMask = 0;
        }

        // Check refresh L2 WA at Release() call and skip for Acquire() to save CPU overhead.
        if (imageBarrier.srcAccessMask != 0)
        {
            // (accessMask != 0) indicates a layout transition BLT. If don't need a BLT then assume CoherShader
            // in imageBarrier.dstAccessMask is indirect access only.
            const bool shaderMdAccessIndirectOnly = (accessMask == 0);

            refreshTcc |= WaRefreshTccOnImageBarrier(imageBarrier.pImage,
                                                     imageBarrier.subresRange,
                                                     imageBarrier.srcAccessMask,
                                                     (accessMask != 0) ? accessMask : imageBarrier.dstAccessMask,
                                                     shaderMdAccessIndirectOnly);
        }

        // For InitMaskRam case, call UpdateDccStateMetaDataIfNeeded after AcqRelInitMaskRam to avoid a racing issue.
        if (layoutTransInfo.blt[0] != HwLayoutTransition::InitMaskRam)
        {
            UpdateDccStateMetaDataIfNeeded(pCmdBuf, pCmdStream, &imageBarrier, pBarrierOps);
        }
    }

    PAL_ASSERT((pBltCount != nullptr) && (pBltStageMask != nullptr) && (pBltAccessMask != nullptr));

    *pBltCount      = bltCount;
    *pBltStageMask  = bltStageMask;
    *pBltAccessMask = bltAccessMask;

    if (pSrcAccessMask != nullptr)
    {
        *pSrcAccessMask = srcAccessMask;
    }

    return refreshTcc;
}

// =====================================================================================================================
// Issue all image layout transition BLT(s) and compute info for release the BLT(s).
bool Device::IssueAcqRelLayoutTransitionBlt(
    Pm4CmdBuffer*                 pCmdBuf,
    CmdStream*                    pCmdStream,
    uint32                        imageBarrierCount,
    const AcqRelAutoBuffer&       transitionList,
    Developer::BarrierOperations* pBarrierOps,
    uint32*                       pPostBltStageMask,
    uint32*                       pPostBltAccessMask
    ) const
{
    // If BLT(s) will be issued, we need to know how to release from it/them.
    uint32 postBltStageMask   = 0;
    uint32 postBltAccessMask  = 0;
    bool   postBltRefreshTcc  = false;
    bool   preInitHtileSynced = false;

    // Issue BLTs.
    for (uint32 i = 0; i < imageBarrierCount; i++)
    {
        const AcqRelTransitionInfo& transition = transitionList[i];
        const ImgBarrier&           imgBarrier = *transition.pImgBarrier;

        if (transition.layoutTransInfo.blt[0] != HwLayoutTransition::None)
        {
            if (IssueBlt(pCmdBuf, pCmdStream, transition.pImgBarrier,
                         transition.layoutTransInfo, preInitHtileSynced, pBarrierOps))
            {
                preInitHtileSynced = true;
            }

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

            postBltRefreshTcc |= WaRefreshTccOnImageBarrier(imgBarrier.pImage,
                                                            imgBarrier.subresRange,
                                                            accessMask,
                                                            imgBarrier.dstAccessMask,
                                                            false); // shaderMdAccessIndirectOnly

            // Add current BLT's stageMask/accessMask into a stageMask/accessMask used for an all-in-one
            // post-BLT release.
            postBltStageMask  |= stageMask;
            postBltAccessMask |= accessMask;
        }
    }

    PAL_ASSERT((pPostBltStageMask != nullptr) && (pPostBltAccessMask != nullptr));

    *pPostBltStageMask  = postBltStageMask;
    *pPostBltAccessMask = postBltAccessMask;

    return postBltRefreshTcc;
}

// =====================================================================================================================
// Wrapper to call RPM's InitMaskRam to issues a compute shader blt to initialize the Mask RAM allocatons for an Image.
// Returns "true" if the compute engine was used for the InitMaskRam operation.
bool Device::AcqRelInitMaskRam(
    Pm4CmdBuffer*      pCmdBuf,
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
    Pm4CmdBuffer*        pCmdBuf,
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
    Pm4CmdBuffer*                 pCmdBuf,
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
            IssueAcquireSync(pCmdBuf, pCmdStream, stageMask, accessMask, false, 1, &syncToken, pBarrierOps);

            // Tell RGP about this transition
            BarrierTransition rgpTransition = AcqRelBuildTransition(&imgBarrier, MsaaBltInfo, pBarrierOps);
            DescribeBarrier(pCmdBuf, &rgpTransition, pBarrierOps);

            RsrcProcMgr().FmaskColorExpand(pCmdBuf, gfx9Image, imgBarrier.subresRange);
        }
    }
}

// =====================================================================================================================
// Issue appropriate cache sync hardware commands to satisfy the cache release requirements.
AcqRelSyncToken Device::IssueReleaseSync(
    Pm4CmdBuffer*                 pCmdBuf,
    CmdStream*                    pCmdStream,
    uint32                        stageMask,  // Bitmask of PipelineStageFlag.
    uint32                        accessMask, // Bitmask of CacheCoherencyUsageFlags.
    bool                          refreshTcc,
    Developer::BarrierOperations* pBarrierOps
    ) const
{
    // Validate input.
    PAL_ASSERT(pBarrierOps != nullptr);

    const EngineType engineType     = pCmdBuf->GetEngineType();
    uint32*          pCmdSpace      = pCmdStream->ReserveCommands();
    const bool       isGfxSupported = Pal::Device::EngineSupportsGraphics(engineType);

    if (isGfxSupported == false)
    {
        stageMask  &= ~PipelineStagesGraphicsOnly;
        accessMask &= ~CacheCoherencyGraphicsOnly;
    }

    if (pCmdBuf->GetPm4CmdBufState().flags.cpBltActive &&
        TestAnyFlagSet(stageMask, PipelineStageBlt | PipelineStageBottomOfPipe))
    {
        // We must guarantee that all prior CP DMA accelerated blts have completed before we write this event because
        // the CmdSetEvent and CmdResetEvent functions expect that the prior blts have reached the post-blt stage by
        // the time the event is written to memory. Given that our CP DMA blts are asynchronous to the pipeline stages
        // the only way to satisfy this requirement is to force the MEC to stall until the CP DMAs are completed.
        pBarrierOps->pipelineStalls.syncCpDma = 1;
        pCmdSpace += m_cmdUtil.BuildWaitDmaData(pCmdSpace);
        pCmdBuf->SetPm4CmdBufCpBltState(false);
    }

    // Converts PipelineStageBlt stage to specific internal pipeline stage, and optimize cache flags for BLTs.
    if (TestAnyFlagSet(stageMask, PipelineStageBlt) || TestAnyFlagSet(accessMask, CacheCoherencyBlt))
    {
        pCmdBuf->OptimizePipeAndCacheMaskForRelease(&stageMask, &accessMask);

        // OptimizePipeAndCacheMaskForRelease() has converted these BLT coherency flags to more specific ones.
        PAL_ASSERT(TestAnyFlagSet(accessMask, CacheCoherencyBlt) == false);
    }

    AcqRelSyncToken syncToken             = {};
    syncToken.type                        = static_cast<uint32>(AcqRelEventType::Invalid);
    bool            issueRbCacheSyncEvent = false; // If set, this EOP event is CACHE_FLUSH_AND_INV_TS, not
                                                   // the pipeline stall-only version BOTTOM_OF_PIPE.

    const bool hasRasterKill  = pCmdBuf->GetPm4CmdBufState().flags.rasterKillDrawsActive;
    const bool requiresEop    = TestAnyFlagSet(stageMask, PipelineStageEopMask);
    // No VS_DONE event support from HW yet, and EOP event will be used instead. Optimize to use PS_DONE instead of
    // heavy VS_DONE (EOP) if there is ps wave. If no ps wave, PS_DONE doesn't safely ensures the completion of VS
    // and prior stage waves.
    // PFP sets IB base and size to register VGT_DMA_BASE & VGT_DMA_SIZE and send request to VGT for indices fetch,
    // so need VsDone to make sure indices fetch is fully done.
    const bool requiresPsDone = TestAnyFlagSet(stageMask, PipelineStagePs) ||
                                ((hasRasterKill == false) &&
                                 TestAnyFlagSet(stageMask, PipelineStageFetchIndices | PipelineStageHsGsMask));
    const bool requiresVsDone = hasRasterKill &&
                                TestAnyFlagSet(stageMask, PipelineStageFetchIndices | PipelineStageHsGsMask);
    const bool requiresCsDone = TestAnyFlagSet(stageMask, PipelineStageCs);

    // If any of the access mask bits that could result in RB sync are set, use CACHE_FLUSH_AND_INV_TS.
    // There is no way to INV the CB metadata caches during acquire. So at release always also invalidate if we are to
    // flush CB metadata. Furthermore, CACHE_FLUSH_AND_INV_TS_EVENT always flush & invalidate RB, so there is no need
    // to invalidate RB at acquire again.
    if (TestAnyFlagSet(accessMask, CoherColorTarget | CoherDepthStencilTarget))
    {
        // Issue a pipelined EOP event that writes timestamp to a GpuEvent slot when all prior GPU work completes.
        syncToken.type        = static_cast<uint32>(AcqRelEventType::Eop);
        issueRbCacheSyncEvent = true;

        accessMask &= ~(CoherColorTarget | CoherDepthStencilTarget);

        Pm4CmdBuffer::SetBarrierOperationsRbCacheSynced(pBarrierOps);
    }
    else if (requiresEop || requiresVsDone || (requiresCsDone && requiresPsDone))
    {
        // Implement set with an EOP event written when all prior GPU work completes if either:
        // 1. End of RB or end of whole pipe is required,
        // 2. There are Vs/Hs/Ds/Gs that require a graphics pipe done event, but there is no PS wave. Since there is no
        //    VS_DONE event, we have to conservatively use an EOP event,
        // 3. Both graphics and compute events are required.
        syncToken.type = static_cast<uint32>(AcqRelEventType::Eop);
    }
    else if (requiresCsDone)
    {
        // Implement set with an EOS event waiting for CS waves to complete.
        syncToken.type = static_cast<uint32>(AcqRelEventType::CsDone);
    }
    else if (requiresPsDone)
    {
        // Implement set with an EOS event waiting for PS waves to complete.
        syncToken.type = static_cast<uint32>(AcqRelEventType::PsDone);
    }

    const ReleaseMemCaches cacheSync = ConvertReleaseCacheFlags(accessMask, refreshTcc, pBarrierOps);

    // Pick EOP event if a cache sync is requested because EOS events do not support cache syncs.
    if ((syncToken.type != static_cast<uint32>(AcqRelEventType::Invalid)) && (cacheSync.u8All != 0))
    {
        syncToken.type = static_cast<uint32>(AcqRelEventType::Eop);
    }

    // Issue RELEASE_MEM packet.
    if (syncToken.type != static_cast<uint32>(AcqRelEventType::Invalid))
    {
        // Request sync fence value after VGT event type is finalized.
        syncToken.fenceVal = pCmdBuf->GetNextAcqRelFenceVal(static_cast<AcqRelEventType>(syncToken.type));

        if (Pal::Device::EngineSupportsGraphics(engineType))
        {
            ReleaseMemGfx releaseMem = {};
            releaseMem.cacheSync = cacheSync;
            releaseMem.dataSel   = data_sel__me_release_mem__send_32_bit_low;

            switch (static_cast<AcqRelEventType>(syncToken.type))
            {
            case AcqRelEventType::Eop:
                releaseMem.vgtEvent = issueRbCacheSyncEvent ? CACHE_FLUSH_AND_INV_TS_EVENT : BOTTOM_OF_PIPE_TS;
                pBarrierOps->pipelineStalls.eopTsBottomOfPipe = 1;
                break;
            case AcqRelEventType::PsDone:
                releaseMem.vgtEvent = PS_DONE;
                pBarrierOps->pipelineStalls.eosTsPsDone = 1;
                break;
            case AcqRelEventType::CsDone:
                releaseMem.vgtEvent = CS_DONE;
                pBarrierOps->pipelineStalls.eosTsCsDone = 1;
                break;
            default:
                PAL_ASSERT_ALWAYS();
                break;
            }

            {
                releaseMem.dstAddr = pCmdBuf->AcqRelFenceValGpuVa(static_cast<AcqRelEventType>(syncToken.type));
                releaseMem.data    = syncToken.fenceVal;
            }

            pCmdSpace += m_cmdUtil.BuildReleaseMemGfx(releaseMem, pCmdSpace);
        }
        else
        {
            // Compute engines can't touch the RB caches or wait for PS_DONE.
            PAL_ASSERT(issueRbCacheSyncEvent == false);

            switch (static_cast<AcqRelEventType>(syncToken.type))
            {
            case AcqRelEventType::Eop:
                pBarrierOps->pipelineStalls.eopTsBottomOfPipe = 1;
                break;
            case AcqRelEventType::CsDone:
                pBarrierOps->pipelineStalls.eosTsCsDone = 1;
                break;
            default:
                PAL_ASSERT_ALWAYS();
                break;
            }

            ReleaseMemGeneric releaseMem = {};
            releaseMem.engineType = engineType;
            releaseMem.cacheSync  = cacheSync;
            releaseMem.dstAddr    = pCmdBuf->AcqRelFenceValGpuVa(static_cast<AcqRelEventType>(syncToken.type));
            releaseMem.dataSel    = data_sel__me_release_mem__send_32_bit_low;
            releaseMem.data       = syncToken.fenceVal;

            pCmdSpace += m_cmdUtil.BuildReleaseMemGeneric(releaseMem, pCmdSpace);
        }
    }
    else if (cacheSync.u8All != 0)
    {
        // This is an optimization path to use AcquireMem for cache syncs only (issueSyncEvent = false) case as
        // ReleaseMem requires an EOP or EOS event.
        AcquireMemGeneric acquireMem = {};
        acquireMem.engineType = engineType;

        // This is messy, but the alternative is to return a SyncGlxFlags from ConvertReleaseCacheFlags. That makes
        // it possible for someone to accidentally add a cache flag later on which is not supported by release_mem.
        // If trying to avoid that kind of bug using an assert is preferable to this mess it can be changed.
        acquireMem.cacheSync = (((cacheSync.gl2Inv != 0) ? SyncGl2Inv : SyncGlxNone) |
                                ((cacheSync.gl2Wb  != 0) ? SyncGl2Wb  : SyncGlxNone) |
                                ((cacheSync.glmInv != 0) ? SyncGlmInv : SyncGlxNone) |
                                ((cacheSync.gl1Inv != 0) ? SyncGl1Inv : SyncGlxNone) |
                                ((cacheSync.glvInv != 0) ? SyncGlvInv : SyncGlxNone));

        pCmdSpace += m_cmdUtil.BuildAcquireMemGeneric(acquireMem, pCmdSpace);

        // Required for gfx9: acquire_mem packets can cause a context roll.
        pCmdStream->SetContextRollDetected<false>();
    }
    else
    {
        PAL_ALERT_ALWAYS_MSG("Barrier-release does nothing, need to validate the correctness of this barrier call.");
    }

    if (issueRbCacheSyncEvent)
    {
        pCmdBuf->UpdatePm4CmdBufGfxBltWbEopFence(syncToken.fenceVal);
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
    Pm4CmdBuffer*                 pCmdBuf,
    CmdStream*                    pCmdStream,
    uint32                        stageMask,  // Bitmask of PipelineStageFlag.
    uint32                        accessMask, // Bitmask of CacheCoherencyUsageFlags.
    bool                          refreshTcc,
    uint32                        syncTokenCount,
    const AcqRelSyncToken*        pSyncTokens,
    Developer::BarrierOperations* pBarrierOps
    ) const
{
    PAL_ASSERT(pBarrierOps != nullptr);
    PAL_ASSERT((syncTokenCount == 0) || (pSyncTokens != nullptr));

    const EngineType engineType     = pCmdBuf->GetEngineType();
    const bool       isGfxSupported = Pal::Device::EngineSupportsGraphics(engineType);
    bool             pfpSyncMe      = isGfxSupported && TestAnyFlagSet(stageMask, PipelineStagePfpMask);

    if (isGfxSupported == false)
    {
        stageMask  &= ~PipelineStagesGraphicsOnly;
        accessMask &= ~CacheCoherencyGraphicsOnly;
    }

    const SyncGlxFlags glxSync = ConvertAcquireCacheFlags(accessMask, refreshTcc, pBarrierOps);

    uint32* pCmdSpace = pCmdStream->ReserveCommands();

    uint32 syncTokenToWait[static_cast<uint32>(AcqRelEventType::Count)] = {};
    bool hasValidSyncToken = false;

    // Merge synchronization timestamp entries in the list.
    for (uint32 i = 0; i < syncTokenCount; i++)
    {
        const AcqRelSyncToken curSyncToken = pSyncTokens[i];

        if ((curSyncToken.fenceVal != 0) &&
            (curSyncToken.type != static_cast<uint32>(AcqRelEventType::Invalid)))
        {
            PAL_ASSERT(curSyncToken.type < static_cast<uint32>(AcqRelEventType::Count));

            syncTokenToWait[curSyncToken.type] = Max(curSyncToken.fenceVal, syncTokenToWait[curSyncToken.type]);
            hasValidSyncToken = true;
        }
    }

    bool waitAtPfpOrMe = false;

    {
        waitAtPfpOrMe = true;

        if (syncTokenToWait[static_cast<uint32>(AcqRelEventType::Eop)] != 0)
        {
            // Issue wait on EOP.
            pCmdSpace += m_cmdUtil.BuildWaitRegMem(engineType,
                                                   mem_space__me_wait_reg_mem__memory_space,
                                                   function__me_wait_reg_mem__greater_than_or_equal_reference_value,
                                                   engine_sel__me_wait_reg_mem__micro_engine,
                                                   pCmdBuf->AcqRelFenceValGpuVa(AcqRelEventType::Eop),
                                                   syncTokenToWait[static_cast<uint32>(AcqRelEventType::Eop)],
                                                   UINT32_MAX,
                                                   pCmdSpace);

            pBarrierOps->pipelineStalls.waitOnTs          = 1;
            pBarrierOps->pipelineStalls.eopTsBottomOfPipe = 1;
        }

        if (syncTokenToWait[static_cast<uint32>(AcqRelEventType::PsDone)] != 0)
        {
            // Issue wait on PS.
            pCmdSpace += m_cmdUtil.BuildWaitRegMem(engineType,
                                                   mem_space__me_wait_reg_mem__memory_space,
                                                   function__me_wait_reg_mem__greater_than_or_equal_reference_value,
                                                   engine_sel__me_wait_reg_mem__micro_engine,
                                                   pCmdBuf->AcqRelFenceValGpuVa(AcqRelEventType::PsDone),
                                                   syncTokenToWait[static_cast<uint32>(AcqRelEventType::PsDone)],
                                                   UINT32_MAX,
                                                   pCmdSpace);

            pBarrierOps->pipelineStalls.waitOnTs    = 1;
            pBarrierOps->pipelineStalls.eosTsPsDone = 1;
        }

        if (syncTokenToWait[static_cast<uint32>(AcqRelEventType::CsDone)] != 0)
        {
            // Issue wait on CS.
            pCmdSpace += m_cmdUtil.BuildWaitRegMem(engineType,
                                                   mem_space__me_wait_reg_mem__memory_space,
                                                   function__me_wait_reg_mem__greater_than_or_equal_reference_value,
                                                   engine_sel__me_wait_reg_mem__micro_engine,
                                                   pCmdBuf->AcqRelFenceValGpuVa(AcqRelEventType::CsDone),
                                                   syncTokenToWait[static_cast<uint32>(AcqRelEventType::CsDone)],
                                                   UINT32_MAX,
                                                   pCmdSpace);

            pBarrierOps->pipelineStalls.waitOnTs    = 1;
            pBarrierOps->pipelineStalls.eosTsCsDone = 1;
        }

        if (glxSync != SyncGlxNone)
        {
            // We need a trailing acquire_mem to handle any cache sync requests.
            AcquireMemGeneric acquireMem = {};
            acquireMem.engineType = engineType;
            acquireMem.cacheSync  = glxSync;

            pCmdSpace += m_cmdUtil.BuildAcquireMemGeneric(acquireMem, pCmdSpace);

            // Required for gfx9: acquire_mem packets can cause a context roll.
            pCmdStream->SetContextRollDetected<false>();
        }
    }

    // The code above waits in the ME, if the waitPoint needs to stall at the PFP request a PFP/ME sync.
    if (pfpSyncMe)
    {
        // Stalls the CP PFP until the ME has processed all previous commands.  Useful in cases where the ME is waiting
        // on some condition, but the PFP needs to stall execution until the condition is satisfied.  This must go last
        // otherwise the PFP could resume execution before the ME is done with all of its waits.
        pCmdSpace += m_cmdUtil.BuildPfpSyncMe(pCmdSpace);
        pBarrierOps->pipelineStalls.pfpSyncMe = 1;
    }

    const Pm4CmdBufferState& cmdBufState       = pCmdBuf->GetPm4CmdBufState();
    const uint32             waitedEopFenceVal = syncTokenToWait[static_cast<uint32>(AcqRelEventType::Eop)];

    // If we have waited on a valid EOP fence value, update some CmdBufState (e.g. xxxBltActive) flags.
    if ((waitedEopFenceVal != 0) && waitAtPfpOrMe)
    {
        pCmdBuf->SetPrevCmdBufInactive();

        if (waitedEopFenceVal >= cmdBufState.fences.gfxBltExecEopFenceVal)
        {
            // An EOP release sync that is issued after the latest GFX BLT must have completed, so mark GFX BLT idle.
            pCmdBuf->SetPm4CmdBufGfxBltState(false);
        }

        if (waitedEopFenceVal >= cmdBufState.fences.gfxBltWbEopFenceVal)
        {
            // An EOP release sync that issued GFX BLT cache flush must have completed, so mark GFX BLT cache clean.
            pCmdBuf->SetPm4CmdBufGfxBltWriteCacheState(false);
        }

        if (waitedEopFenceVal >= cmdBufState.fences.csBltExecEopFenceVal)
        {
            // An EOP release sync that is issued after the latest CS BLT must have completed, so mark CS BLT idle.
            pCmdBuf->SetPm4CmdBufCsBltState(false);
        }

        if (waitedEopFenceVal >= cmdBufState.fences.rasterKillDrawsExecFenceVal)
        {
            // An EOP release sync that is issued after the latest rasterization kill draws must have completed, so
            // mark rasterization kill draws inactive.
            pCmdBuf->SetPm4CmdBufRasterKillDrawsState(false);
        }
    }

    const uint32 waitedCsDoneFenceVal = syncTokenToWait[static_cast<uint32>(AcqRelEventType::CsDone)];

    if ((waitedCsDoneFenceVal != 0) &&
        (waitAtPfpOrMe == true)     &&
        (waitedCsDoneFenceVal >= cmdBufState.fences.csBltExecCsDoneFenceVal))
    {
        // An CS_DONE release sync that is issued after the latest CS BLT must have completed, so mark CS BLT idle.
        pCmdBuf->SetPm4CmdBufCsBltState(false);
    }

    pCmdStream->CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// Figure out the specific BLT operation(s) necessary to convert a color image from one ImageLayout to another.
LayoutTransitionInfo Device::PrepareColorBlt(
    const Pm4CmdBuffer* pCmdBuf,
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
    const Pm4CmdBuffer* pCmdBuf,
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
    Pm4CmdBuffer*     pCmdBuf,
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
// Release perform any necessary layout transition, availability operation, and enqueue command(s) to set a given
// IGpuEvent object once the prior operations' intersection with the given synchronization scope is confirmed complete.
// The availability operation will flush the requested local caches.
AcqRelSyncToken Device::Release(
    Pm4CmdBuffer*                 pCmdBuf,
    CmdStream*                    pCmdStream,
    const AcquireReleaseInfo&     releaseInfo,
    Developer::BarrierOperations* pBarrierOps
    ) const
{
    const uint32 preBltStageMask  = releaseInfo.srcStageMask;
    uint32       preBltAccessMask = releaseInfo.srcGlobalAccessMask;

    // Check if global barrier needs refresh L2
    bool globalRefreshTcc = WaRefreshTccOnGlobalBarrier(releaseInfo.srcGlobalAccessMask, 0);

    // Assumes always do full-range flush sync.
    for (uint32 i = 0; i < releaseInfo.memoryBarrierCount; i++)
    {
        const MemBarrier& barrier = releaseInfo.pMemoryBarriers[i];

        preBltAccessMask |= barrier.srcAccessMask;
    }

    // A container to cache the calculated BLT transitions and some cache info for reuse.
    AcqRelAutoBuffer transitionList(releaseInfo.imageBarrierCount, GetPlatform());
    AcqRelSyncToken  syncToken = {};

    if (transitionList.Capacity() < releaseInfo.imageBarrierCount)
    {
        pCmdBuf->NotifyAllocFailure();
    }
    else
    {
        // If BLT(s) will be issued, we need to know how to acquire for them.
        uint32 bltCount;
        uint32 bltStageMask;
        uint32 bltAccessMask;
        uint32 srcAccessMask;

        globalRefreshTcc |= GetAcqRelLayoutTransitionBltInfo(pCmdBuf,
                                                             pCmdStream,
                                                             releaseInfo,
                                                             &transitionList,
                                                             pBarrierOps,
                                                             &bltCount,
                                                             &bltStageMask,
                                                             &bltAccessMask,
                                                             &srcAccessMask);
        preBltAccessMask |= srcAccessMask;

        // Perform an all-in-one release prior to the potential BLT(s).
        syncToken = IssueReleaseSync(pCmdBuf,
                                     pCmdStream,
                                     preBltStageMask,
                                     preBltAccessMask,
                                     (bltCount > 0) ? false : globalRefreshTcc,// Defer L2 refresh at acquire if has BLT
                                     pBarrierOps);

        // Issue BLT(s) if there exists transitions that require one.
        bool syncTokenWaited = false;
        if (bltCount > 0)
        {
            // Issue all-in-one acquire prior to the potential BLT(s).
            IssueAcquireSync(pCmdBuf,
                             pCmdStream,
                             bltStageMask,
                             bltAccessMask,
                             globalRefreshTcc,
                             1,
                             &syncToken,
                             pBarrierOps);

            // If BLT(s) will be issued, we need to know how to release from it/them.
            uint32 postBltStageMask  = 0;
            uint32 postBltAccessMask = 0;

            // Issue BLTs.
            globalRefreshTcc = IssueAcqRelLayoutTransitionBlt(pCmdBuf,
                                                              pCmdStream,
                                                              releaseInfo.imageBarrierCount,
                                                              transitionList,
                                                              pBarrierOps,
                                                              &postBltStageMask,
                                                              &postBltAccessMask);

            // Release from BLTs.
            syncToken = IssueReleaseSync(pCmdBuf,
                                         pCmdStream,
                                         postBltStageMask,
                                         postBltAccessMask,
                                         globalRefreshTcc,
                                         pBarrierOps);
        }
    }

    return syncToken;
}

// =====================================================================================================================
// Acquire will wait on the specified IGpuEvent object to be signaled, perform any necessary layout transition, and
// issue the required visibility operations. The visibility operation will invalidate the required ranges in local
// caches.
void Device::Acquire(
    Pm4CmdBuffer*                 pCmdBuf,
    CmdStream*                    pCmdStream,
    const AcquireReleaseInfo&     acquireInfo,
    uint32                        syncTokenCount,
    const AcqRelSyncToken*        pSyncTokens,
    Developer::BarrierOperations* pBarrierOps
    ) const
{
    // Check if global barrier needs refresh L2
    bool globalRefreshTcc = WaRefreshTccOnGlobalBarrier(0, acquireInfo.dstGlobalAccessMask);

    // A container to cache the calculated BLT transitions and some cache info for reuse.
    AcqRelAutoBuffer transitionList(acquireInfo.imageBarrierCount, GetPlatform());

    if (transitionList.Capacity() < acquireInfo.imageBarrierCount)
    {
        pCmdBuf->NotifyAllocFailure();
    }
    else
    {
        // If BLT(s) will be issued, we need to know how to acquire for them.
        uint32 bltCount;
        uint32 bltStageMask;
        uint32 bltAccessMask;

        const uint32 preBltRefreshTcc = GetAcqRelLayoutTransitionBltInfo(pCmdBuf,
                                                                         pCmdStream,
                                                                         acquireInfo,
                                                                         &transitionList,
                                                                         pBarrierOps,
                                                                         &bltCount,
                                                                         &bltStageMask,
                                                                         &bltAccessMask);
        // Should have no L2 refresh for image barrier as this is done at Release().
        PAL_ASSERT(preBltRefreshTcc == false);

        const AcqRelSyncToken* pCurSyncTokens = pSyncTokens;

        // A new sync token may be generated by internal BLT.
        AcqRelSyncToken bltSyncToken = {};

        // Issue BLT(s) if there exists transitions that require one.
        if (bltCount > 0)
        {
            // Issue all-in-one acquire prior to the potential BLT(s).
            IssueAcquireSync(pCmdBuf,
                             pCmdStream,
                             bltStageMask,
                             bltAccessMask,
                             false, // refreshTcc
                             syncTokenCount,
                             pCurSyncTokens,
                             pBarrierOps);

            // If BLT(s) will be issued, we need to know how to release from it/them.
            uint32 postBltStageMask  = 0;
            uint32 postBltAccessMask = 0;

            // Issue BLTs.
            globalRefreshTcc |= IssueAcqRelLayoutTransitionBlt(pCmdBuf,
                                                               pCmdStream,
                                                               acquireInfo.imageBarrierCount,
                                                               transitionList,
                                                               pBarrierOps,
                                                               &postBltStageMask,
                                                               &postBltAccessMask);

            // Release from BLTs.
            bltSyncToken = IssueReleaseSync(pCmdBuf,
                                            pCmdStream,
                                            postBltStageMask,
                                            postBltAccessMask,
                                            0, // Defer tcc refresh at the following global acquire
                                            pBarrierOps);

            syncTokenCount = 1;
            pCurSyncTokens = &bltSyncToken;
        }

        // Issue acquire for global cache sync. No need stall if wait at bottom of pipe
        const bool   needWaitSyncToken   = (acquireInfo.dstStageMask != PipelineStageBottomOfPipe);
        const uint32 dstGlobalAccessMask = GetAcquireSyncDstGlobalAccessMask(acquireInfo);

        IssueAcquireSync(pCmdBuf,
                         pCmdStream,
                         acquireInfo.dstStageMask,
                         dstGlobalAccessMask,
                         globalRefreshTcc,
                         needWaitSyncToken ? syncTokenCount : 0,
                         pCurSyncTokens,
                         pBarrierOps);
    }
}

// =====================================================================================================================
// ReleaseThenAcquire is effectively the same as calling Release immediately by calling Acquire.
// This is a convenience method for clients implementing single point barriers. Since ReleaseThenAcquire knows
// both srcAccessMask and dstAccessMask info, instead of calling Release and Acquire directly, can
// implement independently to issue barrier optimally.
void Device::ReleaseThenAcquire(
    Pm4CmdBuffer*                 pCmdBuf,
    CmdStream*                    pCmdStream,
    const AcquireReleaseInfo&     barrierInfo,
    Developer::BarrierOperations* pBarrierOps
    ) const
{
    const uint32 preBltStageMask  = barrierInfo.srcStageMask;
    uint32       preBltAccessMask = barrierInfo.srcGlobalAccessMask;

    // Check if global barrier needs refresh L2
    bool globalRefreshTcc = WaRefreshTccOnGlobalBarrier(barrierInfo.srcGlobalAccessMask,
                                                        barrierInfo.dstGlobalAccessMask);

    // Assumes always do full-range flush sync.
    for (uint32 i = 0; i < barrierInfo.memoryBarrierCount; i++)
    {
        const MemBarrier& barrier = barrierInfo.pMemoryBarriers[i];

        preBltAccessMask |= barrier.srcAccessMask;
    }

    // A container to cache the calculated BLT transitions and some cache info for reuse.
    AcqRelAutoBuffer transitionList(barrierInfo.imageBarrierCount, GetPlatform());

    if (transitionList.Capacity() < barrierInfo.imageBarrierCount)
    {
        pCmdBuf->NotifyAllocFailure();
    }
    else
    {
        AcqRelSyncToken syncToken = {};

        // If BLT(s) will be issued, we need to know how to acquire for them.
        uint32 bltCount;
        uint32 bltStageMask;
        uint32 bltAccessMask;
        uint32 srcAccessMask;

        globalRefreshTcc |= GetAcqRelLayoutTransitionBltInfo(pCmdBuf,
                                                             pCmdStream,
                                                             barrierInfo,
                                                             &transitionList,
                                                             pBarrierOps,
                                                             &bltCount,
                                                             &bltStageMask,
                                                             &bltAccessMask,
                                                             &srcAccessMask);
        preBltAccessMask |= srcAccessMask;

        // Perform an all-in-one release prior to the potential BLT(s).
        syncToken = IssueReleaseSync(pCmdBuf,
                                     pCmdStream,
                                     preBltStageMask,
                                     preBltAccessMask,
                                     0, // Defer tcc refresh at later pre-BLT acquire or global acquire
                                     pBarrierOps);

        // Issue BLT(s) if there exists transitions that require one.
        if (bltCount > 0)
        {
            // Issue all-in-one acquire prior to the potential BLT(s).
            IssueAcquireSync(pCmdBuf,
                             pCmdStream,
                             bltStageMask,
                             bltAccessMask,
                             globalRefreshTcc,
                             1,
                             &syncToken,
                             pBarrierOps);

            // If BLT(s) will be issued, we need to know how to release from it/them.
            uint32 postBltStageMask  = 0;
            uint32 postBltAccessMask = 0;

            // Issue BLTs.
            globalRefreshTcc = IssueAcqRelLayoutTransitionBlt(pCmdBuf,
                                                              pCmdStream,
                                                              barrierInfo.imageBarrierCount,
                                                              transitionList,
                                                              pBarrierOps,
                                                              &postBltStageMask,
                                                              &postBltAccessMask);

            // Release from BLTs.
            syncToken = IssueReleaseSync(pCmdBuf,
                                         pCmdStream,
                                         postBltStageMask,
                                         postBltAccessMask,
                                         0, // Defer tcc refresh at later global acquire
                                         pBarrierOps);
        }

        // Issue acquire for global cache sync. No need stall if wait at bottom of pipe
        const uint32 dstGlobalAccessMask = GetAcquireSyncDstGlobalAccessMask(barrierInfo);
        const bool   needWaitSyncToken   = (barrierInfo.dstStageMask != PipelineStageBottomOfPipe);

        IssueAcquireSync(pCmdBuf,
                         pCmdStream,
                         barrierInfo.dstStageMask,
                         dstGlobalAccessMask,
                         globalRefreshTcc,
                         needWaitSyncToken ? 1 : 0,
                         &syncToken,
                         pBarrierOps);
    }
}

// =====================================================================================================================
// Helper function that issues requested transition for the image barrier.
bool Device::IssueBlt(
    Pm4CmdBuffer*                 pCmdBuf,
    CmdStream*                    pCmdStream,
    const ImgBarrier*             pImgBarrier,
    LayoutTransitionInfo          layoutTransInfo,
    bool                          needPreInitHtileSync,
    Developer::BarrierOperations* pBarrierOps
    ) const
{
    PAL_ASSERT(pImgBarrier->subresRange.numPlanes == 1);
    PAL_ASSERT(pImgBarrier != nullptr);
    PAL_ASSERT(layoutTransInfo.blt[0] != HwLayoutTransition::None);
    PAL_ASSERT(pBarrierOps != nullptr);

    // Tell RGP about this transition
    const BarrierTransition rgpTransition = AcqRelBuildTransition(pImgBarrier, layoutTransInfo, pBarrierOps);
    const auto&             image         = static_cast<const Pal::Image&>(*pImgBarrier->pImage);

    bool preInitHtileSynced = false;

    if (layoutTransInfo.blt[0] == HwLayoutTransition::InitMaskRam)
    {
        const auto& gfx9Image = static_cast<const Image&>(*image.GetGfxImage());

        // Barrier transition is per plane and for split stencil image which has two planes, one for depth and
        // the other for stencil. App may use one plane for a while and later then barrier the other plane from
        // LayoutUninitializedTarget to start using it. DX12 has below case,
        //
        // 1. Barrier from LayoutUninitializedTarget to LayoutDepthStencilTarget, only on plane 0.
        // 2. Bind the image as a depth target, leave stencil disabled. Do some depth writes.
        // 3. Barrier from LayoutUninitializedTarget to LayoutDepthStencilTarget, only on plane 1.
        // 4. Do some more draws, this time using depth and stencil.
        //
        // Need do pre/post InitMaskRam sync to avoiding read/write racing issue.
        // - Pre-init sync: idle prior draws that use this image and refresh the DB cache. Also invalidate Tcp cache
        //   since the InitMaskRam here may read back HTILE data and update with new init value.
        // - Post-init sync: wait InitMaskRam done and invalidate Tcp. Done in FastDepthStencilClearComputeCommon().
        if ((preInitHtileSynced == false)         &&
            (gfx9Image.HasHtileData() == true)    &&
            (image.GetImageInfo().numPlanes == 2) &&
            (gfx9Image.GetHtile()->TileStencilDisabled() == false))
        {
            uint32* pCmdSpace = pCmdStream->ReserveCommands();

            if (pCmdBuf->IsGraphicsSupported())
            {
                pCmdSpace = pCmdStream->WriteWaitEopGfx(HwPipePostPrefetch, SyncGlvInv | SyncGl1Inv, SyncDbWbInv,
                                                        pCmdBuf->TimestampGpuVirtAddr(), pCmdSpace);

                pBarrierOps->caches.flushDb         = 1;
                pBarrierOps->caches.invalDb         = 1;
                pBarrierOps->caches.flushDbMetadata = 1;
                pBarrierOps->caches.invalDbMetadata = 1;
            }
            else
            {
                pCmdSpace = pCmdStream->WriteWaitEopGeneric(SyncGlvInv | SyncGl1Inv,
                                                            pCmdBuf->TimestampGpuVirtAddr(), pCmdSpace);
            }

            pCmdStream->CommitCommands(pCmdSpace);

            pBarrierOps->pipelineStalls.eopTsBottomOfPipe = 1;
            pBarrierOps->pipelineStalls.waitOnTs          = 1;
            pBarrierOps->caches.invalTcp                  = 1;
            pBarrierOps->caches.invalGl1                  = 1;
            preInitHtileSynced                            = true;
        }

        // Transition out of LayoutUninitializedTarget needs to initialize metadata memories.
        AcqRelInitMaskRam(pCmdBuf, pCmdStream, *pImgBarrier);

        // DXC only waits resource alias barrier on ME stage. Need UpdateDccStateMetaData (via PFP) after InitMaskRam
        // which contains an inside PfpSyncMe before initializing MaskRam. So the PfpSyncMe can prevent a racing issue
        // that DccStateMetadata update (by PFP) is done while resource memory to be aliased is stilled being used.
        UpdateDccStateMetaDataIfNeeded(pCmdBuf, pCmdStream, pImgBarrier, pBarrierOps);
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

    // There is barrier operation before AcqRelInitMaskRam(), delay DescribeBarrier() here.
    DescribeBarrier(pCmdBuf, &rgpTransition, pBarrierOps);

    return preInitHtileSynced;
}

// =====================================================================================================================
// Issue the specified BLT operation(s) (i.e., decompresses) necessary to convert a color image from one ImageLayout to
// another.
void Device::AcqRelColorTransitionEvent(
    Pm4CmdBuffer*                 pCmdBuf,
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
            IssueAcquireSyncEvent(pCmdBuf, pCmdStream, stageMask, accessMask, false, 1, &pEvent, pBarrierOps);

            // Tell RGP about this transition
            BarrierTransition rgpTransition = AcqRelBuildTransition(&imgBarrier, MsaaBltInfo, pBarrierOps);
            DescribeBarrier(pCmdBuf, &rgpTransition, pBarrierOps);

            RsrcProcMgr().FmaskColorExpand(pCmdBuf, gfx9Image, imgBarrier.subresRange);
        }
    }
}

// =====================================================================================================================
// Issue appropriate cache sync hardware commands to satisfy the cache release requirements.
void Device::IssueReleaseSyncEvent(
    Pm4CmdBuffer*                 pCmdBuf,
    CmdStream*                    pCmdStream,
    uint32                        stageMask,  // Bitmask of PipelineStageFlag.
    uint32                        accessMask, // Bitmask of CacheCoherencyUsageFlags.
    bool                          refreshTcc,
    const IGpuEvent*              pGpuEvent,
    Developer::BarrierOperations* pBarrierOps
    ) const
{
    // Validate input.
    PAL_ASSERT(stageMask != 0);
    PAL_ASSERT(pGpuEvent != nullptr);
    PAL_ASSERT(pBarrierOps != nullptr);

    const EngineType engineType     = pCmdBuf->GetEngineType();
    uint32*          pCmdSpace      = pCmdStream->ReserveCommands();
    const bool       isGfxSupported = Pal::Device::EngineSupportsGraphics(engineType);

    if (isGfxSupported == false)
    {
        stageMask  &= ~PipelineStagesGraphicsOnly;
        accessMask &= ~CacheCoherencyGraphicsOnly;
    }

    if (pCmdBuf->GetPm4CmdBufState().flags.cpBltActive &&
        TestAnyFlagSet(stageMask, PipelineStageBlt | PipelineStageBottomOfPipe))
    {
        // We must guarantee that all prior CP DMA accelerated blts have completed before we write this event because
        // the CmdSetEvent and CmdResetEvent functions expect that the prior blts have reached the post-blt stage by
        // the time the event is written to memory. Given that our CP DMA blts are asynchronous to the pipeline stages
        // the only way to satisfy this requirement is to force the MEC to stall until the CP DMAs are completed.
        pBarrierOps->pipelineStalls.syncCpDma = 1;
        pCmdSpace += m_cmdUtil.BuildWaitDmaData(pCmdSpace);
        pCmdBuf->SetPm4CmdBufCpBltState(false);
    }

    if (TestAnyFlagSet(stageMask, PipelineStageBlt) || TestAnyFlagSet(accessMask, CacheCoherencyBlt))
    {
        pCmdBuf->OptimizePipeAndCacheMaskForRelease(&stageMask, &accessMask);

        // OptimizePipeAndCacheMaskForRelease() has converted these BLT coherency flags to more specific ones.
        PAL_ASSERT(TestAnyFlagSet(accessMask, CacheCoherencyBlt) == false);
    }

    // Issue RELEASE_MEM packets to flush caches (optional) and signal gpuEvent.
    const uint32          numEventSlots             = Parent()->ChipProperties().gfxip.numSlotsPerEvent;
    const BoundGpuMemory& gpuEventBoundMemObj       = static_cast<const GpuEvent*>(pGpuEvent)->GetBoundGpuMemory();
    PAL_ASSERT(gpuEventBoundMemObj.IsBound());
    const gpusize         gpuEventStartVa           = gpuEventBoundMemObj.GpuVirtAddr();

    AcqRelEventType syncEventType[MaxSlotsPerEvent] = {};
    uint32          syncEventCount                  = 0;
    bool            issueRbCacheSyncEvent           = false; // If set, this EOP event is CACHE_FLUSH_AND_INV_TS, not
                                                             // the pipeline stall-only version BOTTOM_OF_PIPE.

    const bool requiresEop    = TestAnyFlagSet(stageMask, PipelineStageEopMask);
    const bool requiresPsDone = TestAnyFlagSet(stageMask, PipelineStagePs);
    // If no ps wave, PS_DONE doesn't safely ensures the completion of VS and prior stage waves.
    // PFP sets IB base and size to register VGT_DMA_BASE & VGT_DMA_SIZE and send request to VGT for indices fetch,
    // so need VsDone to make sure indices fetch is fully done.
    const bool requiresVsDone = (requiresPsDone == false) &&
                                TestAnyFlagSet(stageMask, PipelineStageFetchIndices | PipelineStageHsGsMask);
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

        Pm4CmdBuffer::SetBarrierOperationsRbCacheSynced(pBarrierOps);
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

    const ReleaseMemCaches cacheSync = ConvertReleaseCacheFlags(accessMask, refreshTcc, pBarrierOps);

    // If we have cache sync request yet don't assign any VGT event, we need to issue a dummy one.
    if ((syncEventCount == 0) && (cacheSync.u8All != 0))
    {
        // Flush at earliest supported pipe point for RELEASE_MEM (CS_DONE always works).
        syncEventType[syncEventCount++] = AcqRelEventType::CsDone;
    }

    // Pick EOP event if GCR cache sync is requested, EOS event is not supported.
    if ((syncEventCount > 0) && (cacheSync.u8All != 0))
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

        if (Pal::Device::EngineSupportsGraphics(engineType))
        {
            // Build RELEASE_MEM packet with requested eop/eos events at ME engine.
            ReleaseMemGfx releaseMem = {};
            releaseMem.cacheSync = cacheSync;
            releaseMem.dataSel   = data_sel__me_release_mem__send_32_bit_low;
            releaseMem.data      = GpuEvent::SetValue;

            for (uint32 i = 0; i < syncEventCount; i++)
            {
                releaseMem.dstAddr = gpuEventStartVa + (i * sizeof(uint32));

                switch (syncEventType[i])
                {
                case AcqRelEventType::Eop:
                    releaseMem.vgtEvent = issueRbCacheSyncEvent ? CACHE_FLUSH_AND_INV_TS_EVENT : BOTTOM_OF_PIPE_TS;
                    pBarrierOps->pipelineStalls.eopTsBottomOfPipe = 1;
                    break;
                case AcqRelEventType::PsDone:
                    releaseMem.vgtEvent = PS_DONE;
                    pBarrierOps->pipelineStalls.eosTsPsDone = 1;
                    break;
                case AcqRelEventType::CsDone:
                    releaseMem.vgtEvent = CS_DONE;
                    pBarrierOps->pipelineStalls.eosTsCsDone = 1;
                    break;
                default:
                    PAL_ASSERT_ALWAYS();
                    break;
                }

                pCmdSpace += m_cmdUtil.BuildReleaseMemGfx(releaseMem, pCmdSpace);
            }
        }
        else
        {
            ReleaseMemGeneric releaseMem = {};
            releaseMem.engineType = engineType;
            releaseMem.cacheSync  = cacheSync;
            releaseMem.dataSel    = data_sel__me_release_mem__send_32_bit_low;
            releaseMem.data       = GpuEvent::SetValue;

            for (uint32 i = 0; i < syncEventCount; i++)
            {
                releaseMem.dstAddr = gpuEventStartVa + (i * sizeof(uint32));

                // Compute engines can't touch the RB caches or wait for PS_DONE.
                PAL_ASSERT(issueRbCacheSyncEvent == false);

                switch (syncEventType[i])
                {
                case AcqRelEventType::Eop:
                    pBarrierOps->pipelineStalls.eopTsBottomOfPipe = 1;
                    break;
                case AcqRelEventType::CsDone:
                    pBarrierOps->pipelineStalls.eosTsCsDone = 1;
                    break;
                default:
                    PAL_ASSERT_ALWAYS();
                    break;
                }

                pCmdSpace += m_cmdUtil.BuildReleaseMemGeneric(releaseMem, pCmdSpace);
            }
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
    Pm4CmdBuffer*                 pCmdBuf,
    CmdStream*                    pCmdStream,
    uint32                        stageMask,  // Bitmask of PipelineStageFlag.
    uint32                        accessMask, // Bitmask of CacheCoherencyUsageFlags.
    bool                          refreshTcc,
    uint32                        gpuEventCount,
    const IGpuEvent* const*       ppGpuEvents,
    Developer::BarrierOperations* pBarrierOps
    ) const
{
    const EngineType engineType     = pCmdBuf->GetEngineType();
    const bool       isGfxSupported = Pal::Device::EngineSupportsGraphics(engineType);

    if (isGfxSupported == false)
    {
        stageMask  &= ~PipelineStagesGraphicsOnly;
        accessMask &= ~CacheCoherencyGraphicsOnly;
    }

    uint32* pCmdSpace = pCmdStream->ReserveCommands();

    // Wait on the GPU memory slot(s) in all specified IGpuEvent objects.
    if (gpuEventCount != 0)
    {
        PAL_ASSERT(ppGpuEvents != nullptr);
        pBarrierOps->pipelineStalls.waitOnTs = 1;

        const uint32 numEventSlots = Parent()->ChipProperties().gfxip.numSlotsPerEvent;

        for (uint32 i = 0; i < gpuEventCount; i++)
        {
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
                                                       UINT32_MAX,
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
                                                         UINT64_MAX,
                                                         pCmdSpace);
            }
        }
    }

    const SyncGlxFlags glxSync = ConvertAcquireCacheFlags(accessMask, refreshTcc, pBarrierOps);

    if (glxSync != SyncGlxNone)
    {
        // We need a trailing acquire_mem to handle any cache sync requests.
        AcquireMemGeneric acquireMem = {};
        acquireMem.engineType = engineType;
        acquireMem.cacheSync  = glxSync;

        pCmdSpace += m_cmdUtil.BuildAcquireMemGeneric(acquireMem, pCmdSpace);

        // Required for gfx9: acquire_mem packets can cause a context roll.
        pCmdStream->SetContextRollDetected<false>();
    }

    // The code above waits in the ME, if the waitPoint needs to stall at the PFP request a PFP/ME sync.
    if (isGfxSupported && TestAnyFlagSet(stageMask, PipelineStagePfpMask))
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
// Release perform any necessary layout transition, availability operation, and enqueue command(s) to set a given
// IGpuEvent object once the prior operations' intersection with the given synchronization scope is confirmed complete.
// The availability operation will flush the requested local caches.
void Device::ReleaseEvent(
    Pm4CmdBuffer*                 pCmdBuf,
    CmdStream*                    pCmdStream,
    const AcquireReleaseInfo&     releaseInfo,
    const IGpuEvent*              pClientEvent,
    Developer::BarrierOperations* pBarrierOps
    ) const
{
    const uint32 preBltStageMask  = releaseInfo.srcStageMask;
    uint32       preBltAccessMask = releaseInfo.srcGlobalAccessMask;

    // Check if global barrier needs refresh L2
    bool globalRefreshTcc = WaRefreshTccOnGlobalBarrier(releaseInfo.srcGlobalAccessMask, 0);

    // Assumes always do full-range flush sync.
    for (uint32 i = 0; i < releaseInfo.memoryBarrierCount; i++)
    {
        const MemBarrier& barrier = releaseInfo.pMemoryBarriers[i];

        preBltAccessMask |= barrier.srcAccessMask;
    }

    // A container to cache the calculated BLT transitions and some cache info for reuse.
    AcqRelAutoBuffer transitionList(releaseInfo.imageBarrierCount, GetPlatform());

    if (transitionList.Capacity() < releaseInfo.imageBarrierCount)
    {
        pCmdBuf->NotifyAllocFailure();
    }
    else
    {
        // If BLT(s) will be issued, we need to know how to acquire for them.
        uint32 bltCount;
        uint32 bltStageMask;
        uint32 bltAccessMask;
        uint32 srcAccessMask;

        globalRefreshTcc |= GetAcqRelLayoutTransitionBltInfo(pCmdBuf,
                                                             pCmdStream,
                                                             releaseInfo,
                                                             &transitionList,
                                                             pBarrierOps,
                                                             &bltCount,
                                                             &bltStageMask,
                                                             &bltAccessMask,
                                                             &srcAccessMask);
        preBltAccessMask |= srcAccessMask;

        // Initialize an IGpuEvent* pEvent pointing at the client provided event.
        // If we have internal BLT(s), use internal event to signal/wait.
        const IGpuEvent* pActiveEvent = (bltCount > 0) ? pCmdBuf->GetInternalEvent() : pClientEvent;

        // Perform an all-in-one release prior to the potential BLT(s): IssueReleaseSyncEvent() on pActiveEvent.
        IssueReleaseSyncEvent(pCmdBuf,
                              pCmdStream,
                              preBltStageMask,
                              preBltAccessMask,
                              (bltCount > 0) ? false : globalRefreshTcc, // Defer L2 refresh at acquire if has BLT
                              pActiveEvent,
                              pBarrierOps);

        // Issue BLT(s) if there exists transitions that require one.
        if (bltCount > 0)
        {
            // Issue all-in-one acquire prior to the potential BLT(s).
            IssueAcquireSyncEvent(pCmdBuf,
                                  pCmdStream,
                                  bltStageMask,
                                  bltAccessMask,
                                  globalRefreshTcc,
                                  1,
                                  &pActiveEvent,
                                  pBarrierOps);

            // If BLT(s) will be issued, we need to know how to release from it/them.
            uint32 postBltStageMask  = 0;
            uint32 postBltAccessMask = 0;

            // Issue BLTs.
            globalRefreshTcc = IssueAcqRelLayoutTransitionBlt(pCmdBuf,
                                                              pCmdStream,
                                                              releaseInfo.imageBarrierCount,
                                                              transitionList,
                                                              pBarrierOps,
                                                              &postBltStageMask,
                                                              &postBltAccessMask);

            // Get back the client provided event and signal it when the whole barrier-release is done.
            pActiveEvent = pClientEvent;

            // Release from BLTs.
            IssueReleaseSyncEvent(pCmdBuf,
                                  pCmdStream,
                                  postBltStageMask,
                                  postBltAccessMask,
                                  globalRefreshTcc,
                                  pActiveEvent,
                                  pBarrierOps);
        }
    }
}

// =====================================================================================================================
// Acquire will wait on the specified IGpuEvent object to be signaled, perform any necessary layout transition,
// and issue the required visibility operations. The visibility operation will invalidate the required ranges in local
// caches.
void Device::AcquireEvent(
    Pm4CmdBuffer*                 pCmdBuf,
    CmdStream*                    pCmdStream,
    const AcquireReleaseInfo&     acquireInfo,
    uint32                        gpuEventCount,
    const IGpuEvent* const*       ppGpuEvents,
    Developer::BarrierOperations* pBarrierOps
    ) const
{
    // Check if global barrier needs refresh L2
    bool globalRefreshTcc = WaRefreshTccOnGlobalBarrier(0, acquireInfo.dstGlobalAccessMask);

    // A container to cache the calculated BLT transitions and some cache info for reuse.
    AcqRelAutoBuffer transitionList(acquireInfo.imageBarrierCount, GetPlatform());

    if (transitionList.Capacity() < acquireInfo.imageBarrierCount)
    {
        pCmdBuf->NotifyAllocFailure();
    }
    else
    {
        // If BLT(s) will be issued, we need to know how to acquire for them.
        uint32 bltCount;
        uint32 bltStageMask;
        uint32 bltAccessMask;

        const uint32 preBltRefreshTcc = GetAcqRelLayoutTransitionBltInfo(pCmdBuf,
                                                                         pCmdStream,
                                                                         acquireInfo,
                                                                         &transitionList,
                                                                         pBarrierOps,
                                                                         &bltCount,
                                                                         &bltStageMask,
                                                                         &bltAccessMask);
        // Should have no L2 refresh for image barrier as this is done at Release().
        PAL_ASSERT(preBltRefreshTcc == false);

        const IGpuEvent* const* ppActiveEvents   = ppGpuEvents;
        uint32                  activeEventCount = gpuEventCount;
        const IGpuEvent*        pEvent           = nullptr;

        if (bltCount > 0)
        {
            // Issue all-in-one acquire prior to the potential BLT(s).
            IssueAcquireSyncEvent(pCmdBuf,
                                  pCmdStream,
                                  bltStageMask,
                                  bltAccessMask,
                                  false, // refreshTcc
                                  activeEventCount,
                                  ppActiveEvents,
                                  pBarrierOps);

            // If BLT(s) will be issued, we need to know how to release from it/them.
            uint32 postBltStageMask  = 0;
            uint32 postBltAccessMask = 0;

            // Issue BLTs.
            globalRefreshTcc |= IssueAcqRelLayoutTransitionBlt(pCmdBuf,
                                                               pCmdStream,
                                                               acquireInfo.imageBarrierCount,
                                                               transitionList,
                                                               pBarrierOps,
                                                               &postBltStageMask,
                                                               &postBltAccessMask);

            // We have internal BLT(s), enable internal event to signal/wait.
            pEvent = pCmdBuf->GetInternalEvent();

            // Release from BLTs.
            IssueReleaseSyncEvent(pCmdBuf,
                                  pCmdStream,
                                  postBltStageMask,
                                  postBltAccessMask,
                                  0, // Defer tcc refresh at later global acquire
                                  pEvent,
                                  pBarrierOps);

            ppActiveEvents   = &pEvent;
            activeEventCount = 1;
        }

        // Issue acquire for global cache sync. No need stall if wait at bottom of pipe
        const uint32 dstGlobalAccessMask = GetAcquireSyncDstGlobalAccessMask(acquireInfo);
        const bool   needWaitEvents      = (acquireInfo.dstStageMask != PipelineStageBottomOfPipe);

        IssueAcquireSyncEvent(pCmdBuf,
                              pCmdStream,
                              acquireInfo.dstStageMask,
                              dstGlobalAccessMask,
                              globalRefreshTcc,
                              needWaitEvents ? activeEventCount : 0,
                              ppActiveEvents,
                              pBarrierOps);
    }
}

} // Gfx9
} // Pal
