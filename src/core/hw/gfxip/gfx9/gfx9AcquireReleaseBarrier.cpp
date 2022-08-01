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

// Define required HW release info in acquire release: release events and RB cache flags.
// CpDma and PfpSyncMe are not tracked here.
union ReleaseEvents
{
    struct
    {
        uint8 eop      : 1;
        uint8 ps       : 1;
        uint8 vs       : 1;
        uint8 cs       : 1;
        uint8 rbCache  : 1;
        uint8 reserved : 3;
    };

    uint8_t u8All;
};

// Define ordered HW acquire points.
enum class AcquirePoint : uint8
{

    Pfp       = 0,
    Me        = 1,
    PreShader = 2,
    PreDepth  = 3,
    PrePs     = 4,
    PreColor  = 5,
    Eop       = 6, // Invalid, for internal optimization purpose.
    Count
};

static constexpr uint32 PipelineStagePfpMask =  PipelineStageTopOfPipe | PipelineStageFetchIndirectArgs |
                                                PipelineStageFetchIndices;

// =====================================================================================================================
// Translate accessMask to ReleaseMemCaches.
static ReleaseMemCaches GetReleaseCacheFlags(
    uint32                        accessMask,  // Bitmask of CacheCoherencyUsageFlags.
    bool                          refreshTcc,
    Developer::BarrierOperations* pBarrierOps)
{
    PAL_ASSERT(pBarrierOps != nullptr);

    // Note that if add any new cache sync flags, remember to update GetReleaseThenAcquireCacheFlags().
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
SyncGlxFlags Device::GetAcquireCacheFlags(
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
SyncGlxFlags Device::GetReleaseThenAcquireCacheFlags(
    uint32                        srcAccessMask,
    uint32                        dstAccessMask,
    bool                          refreshTcc,
    Developer::BarrierOperations* pBarrierOps
    ) const
{
    const ReleaseMemCaches releaseCaches = GetReleaseCacheFlags(srcAccessMask, refreshTcc, pBarrierOps);
    SyncGlxFlags           acquireCaches = GetAcquireCacheFlags(dstAccessMask, refreshTcc, pBarrierOps);

    // Currently GetReleaseCacheFlags() only set gl2Inv/gl2Wb.
    acquireCaches |= releaseCaches.gl2Inv ? SyncGl2Inv : SyncGlxNone;
    acquireCaches |= releaseCaches.gl2Wb  ? SyncGl2Wb  : SyncGlxNone;

    return acquireCaches;
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
        pBarrierOps->layoutTransitions.fastClearEliminate = 1;
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
        *pStageMask  = PipelineStageColorTarget;
        *pAccessMask = CoherColorTarget;
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
// Prepare and get all image layout transition info
bool Device::GetAcqRelLayoutTransitionBltInfo(
    Pm4CmdBuffer*                 pCmdBuf,
    CmdStream*                    pCmdStream,
    const AcquireReleaseInfo&     barrierInfo,
    AcqRelTransitionInfo*         pTransitonInfo,
    uint32*                       pSrcAccessMask, // OR with all image srcAccessMask as output
    uint32*                       pDstAccessMask, // OR with all image dstAccessMask as output
    Developer::BarrierOperations* pBarrierOps
    ) const
{
    uint32 bltStageMask  = 0;
    uint32 bltAccessMask = 0;
    uint32 srcAccessMask = 0;
    uint32 dstAccessMask = 0;
    uint32 bltCount      = 0;
    bool   refreshTcc    = false;

    // Loop through image transitions to update client requested access.
    for (uint32 i = 0; i < barrierInfo.imageBarrierCount; i++)
    {
        const ImgBarrier& imageBarrier = barrierInfo.pImageBarriers[i];

        PAL_ASSERT(imageBarrier.subresRange.numPlanes == 1);

        srcAccessMask |= imageBarrier.srcAccessMask;
        dstAccessMask |= imageBarrier.dstAccessMask;

        // Prepare a layout transition BLT info and do pre-BLT preparation work.
        const LayoutTransitionInfo layoutTransInfo = PrepareBltInfo(pCmdBuf, imageBarrier);

        uint32 stageMask  = 0;
        uint32 accessMask = 0;
        if (layoutTransInfo.blt[0] != HwLayoutTransition::None)
        {
            GetBltStageAccessInfo(layoutTransInfo, &stageMask, &accessMask);

            (*pTransitonInfo->pBltList)[bltCount].pImgBarrier     = &imageBarrier;
            (*pTransitonInfo->pBltList)[bltCount].layoutTransInfo = layoutTransInfo;
            (*pTransitonInfo->pBltList)[bltCount].stageMask       = stageMask;
            (*pTransitonInfo->pBltList)[bltCount].accessMask      = accessMask;
            bltCount++;

            // Add current BLT's stageMask into a stageMask/accessMask used for an all-in-one pre-BLT acquire.
            bltStageMask  |= stageMask;
            // Optimization: set preBltAccessMask=0 for transition to InitMaskRam since no need cache sync in this case.
            bltAccessMask |= (layoutTransInfo.blt[0] == HwLayoutTransition::InitMaskRam) ? 0 : accessMask;
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

    PAL_ASSERT(pTransitonInfo != nullptr);
    pTransitonInfo->bltCount      = bltCount;
    pTransitonInfo->bltStageMask  = bltStageMask;
    pTransitonInfo->bltAccessMask = bltAccessMask;

    if (pSrcAccessMask != nullptr)
    {
        *pSrcAccessMask |= srcAccessMask;
    }

    if (pDstAccessMask != nullptr)
    {
        *pDstAccessMask |= dstAccessMask;
    }

    return refreshTcc;
}

// =====================================================================================================================
// Issue all image layout transition BLTs and compute info for release the BLTs.
bool Device::IssueAcqRelLayoutTransitionBlt(
    Pm4CmdBuffer*                 pCmdBuf,
    CmdStream*                    pCmdStream,
    AcqRelTransitionInfo*         pTransitonInfo,
    Developer::BarrierOperations* pBarrierOps
    ) const
{
    // If BLTs will be issued, we need to know how to release from it/them.
    uint32 postBltStageMask   = 0;
    uint32 postBltAccessMask  = 0;
    bool   postBltRefreshTcc  = false;
    bool   preInitHtileSynced = false;

    PAL_ASSERT(pTransitonInfo != nullptr);

    // Issue BLTs.
    for (uint32 i = 0; i < pTransitonInfo->bltCount; i++)
    {
        const AcqRelImgTransitionInfo& transition = (*pTransitonInfo->pBltList)[i];
        const ImgBarrier&              imgBarrier = *transition.pImgBarrier;

        if (transition.layoutTransInfo.blt[0] != HwLayoutTransition::None)
        {
            if (IssueBlt(pCmdBuf, pCmdStream, &imgBarrier, transition.layoutTransInfo, preInitHtileSynced, pBarrierOps))
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
                stageMask  = transition.stageMask;
                accessMask = transition.accessMask;
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

    // Output bltStageMask and bltAccessMask for release BLTs. Generally they're the same as the input values in
    // pTransitonInfo, but if (layoutTransInfo.blt[1] != HwLayoutTransition::None), they may be different and we
    // should update the values to release blt[1] correctly.
    pTransitonInfo->bltStageMask  = postBltStageMask;
    pTransitonInfo->bltAccessMask = postBltAccessMask;

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
            RsrcProcMgr().FastClearEliminate(pCmdBuf,
                                             pCmdStream,
                                             gfx9Image,
                                             imgBarrier.pQuadSamplePattern,
                                             imgBarrier.subresRange);
        }

        // Handle corner cases where it needs a second pass.
        if (layoutTransInfo.blt[1] != HwLayoutTransition::None)
        {
            PAL_ASSERT(layoutTransInfo.blt[1] == HwLayoutTransition::MsaaColorDecompress);

            uint32 srcStageMask  = 0;
            uint32 dstStageMask  = 0;
            uint32 srcAccessMask = 0;
            uint32 dstAccessMask = 0;

            // Prepare release info for first pass BLT.
            GetBltStageAccessInfo(layoutTransInfo, &srcStageMask, &srcAccessMask);

            // Prepare acquire info for second pass BLT.
            constexpr LayoutTransitionInfo MsaaBltInfo = { {}, HwLayoutTransition::MsaaColorDecompress };
            GetBltStageAccessInfo(MsaaBltInfo, &dstStageMask, &dstAccessMask);

            const bool refreshTcc = WaRefreshTccOnImageBarrier(&image, imgBarrier.subresRange,
                                                               srcAccessMask, dstAccessMask, false);

            IssueReleaseThenAcquireSync(pCmdBuf, pCmdStream, srcStageMask, dstStageMask,
                                        srcAccessMask, dstAccessMask, refreshTcc, pBarrierOps);

            // Tell RGP about this transition
            BarrierTransition rgpTransition = AcqRelBuildTransition(&imgBarrier, MsaaBltInfo, pBarrierOps);
            DescribeBarrier(pCmdBuf, &rgpTransition, pBarrierOps);

            RsrcProcMgr().FmaskColorExpand(pCmdBuf, gfx9Image, imgBarrier.subresRange);
        }
    }
}

// =====================================================================================================================
static ReleaseEvents GetReleaseEvents(
    Pm4CmdBuffer* pCmdBuf,
    const Device& device,
    uint32        srcStageMask,
    uint32        srcAccessMask,
    bool          splitBarrier,
    AcquirePoint  acquirePoint = AcquirePoint::Pfp) // Assume worst stall if info not available in split barrier
{
    constexpr uint32 EopWaitStageMask = PipelineStageEarlyDsTarget | PipelineStageLateDsTarget |
                                        PipelineStageColorTarget   | PipelineStageBottomOfPipe;
    // PFP sets IB base and size to register VGT_DMA_BASE & VGT_DMA_SIZE and send request to VGT for indices fetch,
    // which is done in GE. So need VsDone to make sure indices fetch done.
    constexpr uint32 VsWaitStageMask  = PipelineStageVs | PipelineStageHs | PipelineStageDs |
                                        PipelineStageGs | PipelineStageFetchIndices;
    constexpr uint32 PsWaitStageMask  = PipelineStagePs;
    constexpr uint32 CsWaitStageMask  = PipelineStageCs;

    // Detect cases where no global execution barrier is required because the acquire point is later than the
    // pipeline stages being released.
    constexpr uint32 StallReqStageMask[] =
    {
        // Pfp       = 0
        VsWaitStageMask | PsWaitStageMask | CsWaitStageMask | EopWaitStageMask,
        // Me        = 1
        VsWaitStageMask | PsWaitStageMask | CsWaitStageMask | EopWaitStageMask,
        // PreShader = 2
        VsWaitStageMask | PsWaitStageMask | CsWaitStageMask | EopWaitStageMask,
        // PreDepth  = 3
        PsWaitStageMask | CsWaitStageMask | EopWaitStageMask,
        // PrePs     = 4
        PsWaitStageMask | CsWaitStageMask | EopWaitStageMask,
        // PreColor  = 5
        // PS exports from distinct packers are not ordered.  Therefore, it is possible for color target writes in an
        // RB associated with one packer to start while pixel shader reads from the previous draw are still active on a
        // different packer.  If the writes and reads in that scenario access the same data, the operations will not
        // occur in the API-defined pipeline order.  So need stall here to guarantee the order.
        PsWaitStageMask | CsWaitStageMask | EopWaitStageMask,
        // Eop       = 6, can skip all release events (except RB cache)
        0,
    };

    ReleaseEvents release = {};

    if (TestAnyFlagSet(srcStageMask, StallReqStageMask[uint32(acquirePoint)]))
    {
        if (TestAnyFlagSet(srcStageMask, EopWaitStageMask))
        {
            release.eop = 1;
        }
        else
        {
            release.ps = TestAnyFlagSet(srcStageMask, PsWaitStageMask);
            release.vs = TestAnyFlagSet(srcStageMask, VsWaitStageMask);
            release.cs = TestAnyFlagSet(srcStageMask, CsWaitStageMask);
        }
    }

    // Use CACHE_FLUSH_AND_INV_TS to sync RB cache. There is no way to INV the CB metadata caches during acquire.
    // So at release always also invalidate if we are to flush CB metadata. Furthermore, CACHE_FLUSH_AND_INV_TS_EVENT
    // always flush and invalidate RB cache, so there is no need to invalidate RB at acquire again.
    if (TestAnyFlagSet(srcAccessMask, CoherColorTarget | CoherDepthStencilTarget))
    {
        release.eop     = 1;
        release.rbCache = 1;
    }

    if (splitBarrier)
    {
        // No VS_DONE event support from HW yet. For ReleaseThenAcquire, can issue VS_PARTIAL_FLUSH instead and for
        // split barrier, need bump to PsDone or Eop instead. However, if rasterKill is enabled (there is no Ps wave),
        // PsDone can't guarantee Vs done where we have to convert to Eop instead.
        if (release.vs)
        {
            release.vs = 0;

            if (pCmdBuf->GetPm4CmdBufState().flags.rasterKillDrawsActive != 0)
            {
                release.eop = 1;
            }
            else
            {
                release.ps = 1;
            }
        }

        // Combine to single event
        if (release.ps && release.cs)
        {
            release.eop = 1;
            release.ps  = 0;
            release.cs  = 0;
        }
    }

    return release;
}

// =====================================================================================================================
static AcquirePoint GetAcquirePoint(
    uint32 dstStageMask)
{
    // Constants to map PAL interface pipe stage masks to HW acquire points.
    constexpr uint32 AcqPfpStages       = PipelineStageTopOfPipe | PipelineStageFetchIndirectArgs |
                                          PipelineStageFetchIndices;
    constexpr uint32 AcqMeStages        = PipelineStageBlt;
    constexpr uint32 AcqPreShaderStages = PipelineStageVs | PipelineStageHs | PipelineStageDs |
                                          PipelineStageGs | PipelineStageCs;
    constexpr uint32 AcqPreDepthStages  = PipelineStageEarlyDsTarget | PipelineStageLateDsTarget;
    constexpr uint32 AcqPrePsStages     = PipelineStagePs;
    constexpr uint32 AcqPreColorStages  = PipelineStageColorTarget;

    // Convert global dstStageMask to HW acquire point.
    return TestAnyFlagSet(dstStageMask, AcqPfpStages)       ? AcquirePoint::Pfp       :
           TestAnyFlagSet(dstStageMask, AcqMeStages)        ? AcquirePoint::Me        :
           TestAnyFlagSet(dstStageMask, AcqPreShaderStages) ? AcquirePoint::PreShader :
           TestAnyFlagSet(dstStageMask, AcqPreDepthStages)  ? AcquirePoint::PreDepth  :
           TestAnyFlagSet(dstStageMask, AcqPrePsStages)     ? AcquirePoint::PrePs     :
           TestAnyFlagSet(dstStageMask, AcqPreColorStages)  ? AcquirePoint::PreColor  :
                                                              AcquirePoint::Eop;
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

    const ReleaseEvents    releaseEvents = GetReleaseEvents(pCmdBuf, *this, stageMask, accessMask, true);
    const ReleaseMemCaches releaseCaches = GetReleaseCacheFlags(accessMask, refreshTcc, pBarrierOps);
    // Pick EOP event if a cache sync is requested because EOS events do not support cache syncs.
    const bool             bumpToEop     = (releaseCaches.u8All != 0) && (releaseEvents.u8All != 0);

    // Note that release event flags for split barrier should meet below conditions,
    //    1). No VsDone as it should be converted to PsDone or Eop.
    //    2). PsDone and CsDone should have been already converted to Eop.
    //    3). rbCache sync must have Eop event set.
    PAL_ASSERT(releaseEvents.vs == 0);
    PAL_ASSERT((releaseEvents.ps & releaseEvents.cs)== 0);
    PAL_ASSERT((releaseEvents.rbCache == 0) || releaseEvents.eop);

    AcqRelSyncToken syncToken = {};
    syncToken.type = (releaseEvents.eop || bumpToEop) ? uint32(AcqRelEventType::Eop)    :
                     releaseEvents.ps                 ? uint32(AcqRelEventType::PsDone) :
                     releaseEvents.cs                 ? uint32(AcqRelEventType::CsDone) :
                                                        uint32(AcqRelEventType::Invalid);

    // Issue RELEASE_MEM packet.
    if (syncToken.type != uint32(AcqRelEventType::Invalid))
    {
        // Request sync fence value after VGT event type is finalized.
        syncToken.fenceVal = pCmdBuf->GetNextAcqRelFenceVal(AcqRelEventType(syncToken.type));

        if (Pal::Device::EngineSupportsGraphics(engineType))
        {
            ReleaseMemGfx releaseMem = {};
            releaseMem.cacheSync = releaseCaches;

            switch (AcqRelEventType(syncToken.type))
            {
            case AcqRelEventType::Eop:
                releaseMem.vgtEvent = releaseEvents.rbCache ? CACHE_FLUSH_AND_INV_TS_EVENT : BOTTOM_OF_PIPE_TS;
                pBarrierOps->pipelineStalls.eopTsBottomOfPipe = 1;
                if (releaseEvents.rbCache)
                {
                    Pm4CmdBuffer::SetBarrierOperationsRbCacheSynced(pBarrierOps);
                }
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
                releaseMem.dataSel = data_sel__me_release_mem__send_32_bit_low;
                releaseMem.dstAddr = pCmdBuf->AcqRelFenceValGpuVa(AcqRelEventType(syncToken.type));
                releaseMem.data    = syncToken.fenceVal;
            }

            pCmdSpace += m_cmdUtil.BuildReleaseMemGfx(releaseMem, pCmdSpace);
        }
        else
        {
            switch (AcqRelEventType(syncToken.type))
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
            releaseMem.cacheSync  = releaseCaches;
            releaseMem.dstAddr    = pCmdBuf->AcqRelFenceValGpuVa(AcqRelEventType(syncToken.type));
            releaseMem.dataSel    = data_sel__me_release_mem__send_32_bit_low;
            releaseMem.data       = syncToken.fenceVal;

            pCmdSpace += m_cmdUtil.BuildReleaseMemGeneric(releaseMem, pCmdSpace);
        }
    }
    else if (releaseCaches.u8All != 0)
    {
        // This is an optimization path to use AcquireMem for cache syncs only (issueSyncEvent = false) case as
        // ReleaseMem requires an EOP or EOS event.
        AcquireMemGeneric acquireMem = {};
        acquireMem.engineType = engineType;

        // This is messy, but the alternative is to return a SyncGlxFlags from GetReleaseCacheFlags. That makes
        // it possible for someone to accidentally add a cache flag later on which is not supported by release_mem.
        // If trying to avoid that kind of bug using an assert is preferable to this mess it can be changed.
        acquireMem.cacheSync  = m_cmdUtil.GetSyncGlxFlagsFromReleaseMemCaches(releaseCaches);

        pCmdSpace += m_cmdUtil.BuildAcquireMemGeneric(acquireMem, pCmdSpace);

        // Required for gfx9: acquire_mem packets can cause a context roll.
        pCmdStream->SetContextRollDetected<false>();
    }

    if (releaseEvents.rbCache)
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

    const EngineType engineType     = pCmdBuf->GetEngineType();
    const bool       isGfxSupported = Pal::Device::EngineSupportsGraphics(engineType);
    bool             needPfpSyncMe  = isGfxSupported && TestAnyFlagSet(stageMask, PipelineStagePfpMask);

    if (isGfxSupported == false)
    {
        stageMask  &= ~PipelineStagesGraphicsOnly;
        accessMask &= ~CacheCoherencyGraphicsOnly;
    }

    const SyncGlxFlags acquireCaches = GetAcquireCacheFlags(accessMask, refreshTcc, pBarrierOps);

    uint32* pCmdSpace = pCmdStream->ReserveCommands();

    uint32 syncTokenToWait[uint32(AcqRelEventType::Count)] = {};
    bool hasValidSyncToken = false;

    // Merge synchronization timestamp entries in the list.
    for (uint32 i = 0; i < syncTokenCount; i++)
    {
        const AcqRelSyncToken curSyncToken = pSyncTokens[i];

        if ((curSyncToken.fenceVal != 0) && (curSyncToken.type != uint32(AcqRelEventType::Invalid)))
        {
            PAL_ASSERT(curSyncToken.type < uint32(AcqRelEventType::Count));

            syncTokenToWait[curSyncToken.type] = Max(curSyncToken.fenceVal, syncTokenToWait[curSyncToken.type]);
            hasValidSyncToken = true;
        }
    }

    bool waitAtPfpOrMe = false;

    {
        for (uint32 i = 0; i < uint32(AcqRelEventType::Count); i++)
        {
            if (syncTokenToWait[i] != 0)
            {
                 pCmdSpace += m_cmdUtil.BuildWaitRegMem(engineType,
                                                        mem_space__me_wait_reg_mem__memory_space,
                                                        function__me_wait_reg_mem__greater_than_or_equal_reference_value,
                                                        engine_sel__me_wait_reg_mem__micro_engine,
                                                        pCmdBuf->AcqRelFenceValGpuVa(AcqRelEventType(i)),
                                                        syncTokenToWait[i],
                                                        UINT32_MAX,
                                                        pCmdSpace);
            }
        }

        waitAtPfpOrMe                         = hasValidSyncToken;
        pBarrierOps->pipelineStalls.waitOnTs |= hasValidSyncToken;

        if (acquireCaches != SyncGlxNone)
        {
            // We need a trailing acquire_mem to handle any cache sync requests.
            AcquireMemGeneric acquireMem = {};
            acquireMem.engineType = engineType;
            acquireMem.cacheSync  = acquireCaches;

            pCmdSpace += m_cmdUtil.BuildAcquireMemGeneric(acquireMem, pCmdSpace);

            // Required for gfx9: acquire_mem packets can cause a context roll.
            pCmdStream->SetContextRollDetected<false>();
        }
    }

    // The code above waits in the ME, if the waitPoint needs to stall at the PFP request a PFP/ME sync.
    if (needPfpSyncMe)
    {
        // Stalls the CP PFP until the ME has processed all previous commands.  Useful in cases where the ME is waiting
        // on some condition, but the PFP needs to stall execution until the condition is satisfied.  This must go last
        // otherwise the PFP could resume execution before the ME is done with all of its waits.
        pCmdSpace += m_cmdUtil.BuildPfpSyncMe(pCmdSpace);
        pBarrierOps->pipelineStalls.pfpSyncMe = 1;
    }

    const Pm4CmdBufferState& cmdBufState       = pCmdBuf->GetPm4CmdBufState();
    const uint32             waitedEopFenceVal = syncTokenToWait[uint32(AcqRelEventType::Eop)];

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

    const uint32 waitedCsDoneFenceVal = syncTokenToWait[uint32(AcqRelEventType::CsDone)];

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
// Issue appropriate cache sync hardware commands to satisfy the cache release requirements.
void Device::IssueReleaseThenAcquireSync(
    Pm4CmdBuffer*                 pCmdBuf,
    CmdStream*                    pCmdStream,
    uint32                        srcStageMask,
    uint32                        dstStageMask,
    uint32                        srcAccessMask,
    uint32                        dstAccessMask,
    bool                          refreshTcc,
    Developer::BarrierOperations* pBarrierOps
    ) const
{
    const EngineType engineType     = pCmdBuf->GetEngineType();
    const bool       isGfxSupported = Pal::Device::EngineSupportsGraphics(engineType);
    bool             needPfpSyncMe  = isGfxSupported && TestAnyFlagSet(dstStageMask, PipelineStagePfpMask);
    uint32*          pCmdSpace      = pCmdStream->ReserveCommands();

    if (isGfxSupported == false)
    {
        srcStageMask  &= ~PipelineStagesGraphicsOnly;
        dstStageMask  &= ~PipelineStagesGraphicsOnly;
        srcAccessMask &= ~CacheCoherencyGraphicsOnly;
        dstAccessMask &= ~CacheCoherencyGraphicsOnly;
    }

    if (pCmdBuf->GetPm4CmdBufState().flags.cpBltActive &&
        TestAnyFlagSet(srcStageMask, PipelineStageBlt | PipelineStageBottomOfPipe))
    {
        pBarrierOps->pipelineStalls.syncCpDma = 1;
        pCmdSpace += m_cmdUtil.BuildWaitDmaData(pCmdSpace);
        pCmdBuf->SetPm4CmdBufCpBltState(false);
    }

    // Converts PipelineStageBlt stage to specific internal pipeline stage, and optimize cache flags for BLTs.
    if (TestAnyFlagSet(srcStageMask, PipelineStageBlt) || TestAnyFlagSet(srcAccessMask, CacheCoherencyBlt))
    {
        pCmdBuf->OptimizePipeAndCacheMaskForRelease(&srcStageMask, &srcAccessMask);
        PAL_ASSERT(TestAnyFlagSet(srcAccessMask, CacheCoherencyBlt) == false);
    }

    AcquirePoint        acquirePoint  = GetAcquirePoint(dstStageMask);
    const ReleaseEvents releaseEvents = GetReleaseEvents(pCmdBuf, *this, srcStageMask, srcAccessMask,
                                                         false, acquirePoint);
    SyncGlxFlags        acquireCaches = GetReleaseThenAcquireCacheFlags(srcAccessMask, dstAccessMask,
                                                                        refreshTcc, pBarrierOps);
    ReleaseMemCaches    releaseCaches = {};
    bool                waitAtPfpOrMe = false;

    if (releaseEvents.rbCache)
    {
        PAL_ASSERT(releaseEvents.eop); // rbCache sync must have Eop event set.
        Pm4CmdBuffer::SetBarrierOperationsRbCacheSynced(pBarrierOps);
    }

    if (dstStageMask != PipelineStageBottomOfPipe)
    {
        {
            waitAtPfpOrMe = (releaseEvents.u8All != 0);

            if (releaseEvents.eop)
            {
                auto*const pGfx9Stream = static_cast<CmdStream*>(pCmdStream);

                if (releaseEvents.rbCache)
                {
                    pCmdSpace = pGfx9Stream->WriteWaitEopGfx(HwPipePostPrefetch, SyncGlxNone, SyncRbWbInv,
                                                             pCmdBuf->TimestampGpuVirtAddr(), pCmdSpace);
                }
                else
                {
                    pCmdSpace = pGfx9Stream->WriteWaitEopGeneric(SyncGlxNone, pCmdBuf->TimestampGpuVirtAddr(),
                                                                 pCmdSpace);
                }

                pBarrierOps->pipelineStalls.eopTsBottomOfPipe = 1;
                pBarrierOps->pipelineStalls.waitOnTs = 1;
            }
            else
            {
                if (releaseEvents.ps)
                {
                    pCmdSpace += m_cmdUtil.BuildNonSampleEventWrite(PS_PARTIAL_FLUSH, engineType, pCmdSpace);
                    pBarrierOps->pipelineStalls.psPartialFlush = 1;

                    // When rasterKillDrawsActive=1 (not accurate status but assumed worst case), PS_PARTIAL_FLUSH
                    // complete can't make sure VS_PARTIAL_FLUSH complete. Need issue VS_PARTIAL_FLUSH as well.
                    if (pCmdBuf->GetPm4CmdBufState().flags.rasterKillDrawsActive != 0)
                    {
                        pCmdSpace += m_cmdUtil.BuildNonSampleEventWrite(VS_PARTIAL_FLUSH, engineType, pCmdSpace);
                        pBarrierOps->pipelineStalls.vsPartialFlush = 1;
                    }
                }
                else if (releaseEvents.vs)
                {
                    pCmdSpace += m_cmdUtil.BuildNonSampleEventWrite(VS_PARTIAL_FLUSH, engineType, pCmdSpace);
                    pBarrierOps->pipelineStalls.vsPartialFlush = 1;
                }

                if (releaseEvents.cs)
                {
                    pCmdSpace += m_cmdUtil.BuildWaitCsIdle(engineType, pCmdBuf->TimestampGpuVirtAddr(), pCmdSpace);
                    pBarrierOps->pipelineStalls.csPartialFlush = 1;
                }
            }
        }
    }
    else // (dstStageMask == PipelineStageBottomOfPipe)
    {
        if (releaseEvents.rbCache)
        {
            // Need issue GCR.gl2Inv/gl2Wb and RB cache sync in single ReleaseMem packet to avoid racing issue. Note
            // that it's possible GCR.glkInv is left in acquireCaches for cases glkInv isn't supported in ReleaseMem
            // packet and that should be fine.
            releaseCaches = m_cmdUtil.SelectReleaseMemCaches(&acquireCaches);

            ReleaseMemGfx releaseMem = {};
            releaseMem.cacheSync = releaseCaches;
            releaseMem.vgtEvent  = CACHE_FLUSH_AND_INV_TS_EVENT;
            releaseMem.dataSel   = data_sel__me_release_mem__none;

            pCmdSpace += m_cmdUtil.BuildReleaseMemGfx(releaseMem, pCmdSpace);
            pBarrierOps->pipelineStalls.eopTsBottomOfPipe = 1;
        }
    }

    if (acquireCaches != SyncGlxNone)
    {
        // We need a trailing acquire_mem to handle any cache sync requests.
        AcquireMemGeneric acquireMem = {};
        acquireMem.engineType = engineType;
        acquireMem.cacheSync  = acquireCaches;

        pCmdSpace += m_cmdUtil.BuildAcquireMemGeneric(acquireMem, pCmdSpace);

        // Required for gfx9: acquire_mem packets can cause a context roll.
        pCmdStream->SetContextRollDetected<false>();
    }

    // BuildWaitRegMem waits in the ME, if the waitPoint needs to stall at the PFP request a PFP/ME sync.
    if (needPfpSyncMe)
    {
        // Stalls the CP PFP until the ME has processed all previous commands.  Useful in cases where the ME is waiting
        // on some condition, but the PFP needs to stall execution until the condition is satisfied.  This must go last
        // otherwise the PFP could resume execution before the ME is done with all of its waits.
        pCmdSpace += m_cmdUtil.BuildPfpSyncMe(pCmdSpace);
        pBarrierOps->pipelineStalls.pfpSyncMe = 1;
    }

    // If we have waited on a valid EOP fence value, update some CmdBufState (e.g. xxxBltActive) flags.
    if (waitAtPfpOrMe)
    {
        if (releaseEvents.eop)
        {
            pCmdBuf->SetPrevCmdBufInactive();

            pCmdBuf->SetPm4CmdBufGfxBltState(false);
            pCmdBuf->SetPm4CmdBufCsBltState(false);
            pCmdBuf->SetPm4CmdBufRasterKillDrawsState(false);

            if (releaseEvents.rbCache)
            {
                pCmdBuf->SetPm4CmdBufGfxBltWriteCacheState(false);
            }
        }
        else if (releaseEvents.cs)
        {
            pCmdBuf->SetPm4CmdBufCsBltState(false);
        }
    }

    pCmdStream->CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// Figure out the specific BLT operation(s) necessary to convert a color image from one ImageLayout to another.
LayoutTransitionInfo Device::PrepareColorBlt(
    Pm4CmdBuffer*      pCmdBuf,
    const Pal::Image&  image,
    const SubresRange& subresRange,
    ImageLayout        oldLayout,
    ImageLayout        newLayout
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
        // If the previous state allowed the possibility of a reg-based fast clear(comp-to-reg) while the new state
        // does not, we need to issue a fast clear eliminate BLT
        if (fastClearEliminateSupported &&
            (gfx9Image.SupportsCompToReg(newLayout, subresRange.startSubres) == false) &&
            gfx9Image.SupportsCompToReg(oldLayout, subresRange.startSubres))
        {
            if (gfx9ImageConst.IsFceOptimizationEnabled() &&
                (gfx9ImageConst.HasSeenNonTcCompatibleClearColor() == false))
            {
                // Skip the fast clear eliminate for this image if the clear color is TC-compatible and the
                // optimization was enabled.
                // Cast to modifiable value for skip-FCE optimization only.
                auto& gfx9ImageNonConst = static_cast<Gfx9::Image&>(*image.GetGfxImage());

                // Skip the fast clear eliminate for this image if the clear color is TC-compatible and the
                // optimization was enabled.
                pCmdBuf->AddFceSkippedImageCounter(&gfx9ImageNonConst);

            }
            else
            {
                // The image has been fast cleared with a non-TC compatible color or the FCE optimization is not
                // enabled.
                transitionInfo.blt[bltIndex++] = HwLayoutTransition::FastClearEliminate;
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
    uint32 srcStageMask        = releaseInfo.srcStageMask;
    uint32 srcGlobalAccessMask = releaseInfo.srcGlobalAccessMask;

    // Check if global barrier needs refresh L2
    bool globalRefreshTcc = WaRefreshTccOnGlobalBarrier(srcGlobalAccessMask, 0);

    // Always do full-range flush sync.
    for (uint32 i = 0; i < releaseInfo.memoryBarrierCount; i++)
    {
        const MemBarrier& barrier = releaseInfo.pMemoryBarriers[i];

        // globallyAvailable is processed in Acquire().
        srcGlobalAccessMask |= barrier.srcAccessMask;
    }

    // A container to cache the calculated BLT transitions and some cache info for reuse.
    AcqRelAutoBuffer transitionList(releaseInfo.imageBarrierCount, GetPlatform());
    AcqRelSyncToken  syncToken = {};

    if (transitionList.Capacity() >= releaseInfo.imageBarrierCount)
    {
        // If BLTs will be issued, we need to know how to acquire for them.
        AcqRelTransitionInfo transInfo = { &transitionList, 0, 0, 0 };

        globalRefreshTcc |= GetAcqRelLayoutTransitionBltInfo(pCmdBuf,
                                                             pCmdStream,
                                                             releaseInfo,
                                                             &transInfo,
                                                             &srcGlobalAccessMask,
                                                             nullptr,
                                                             pBarrierOps);

        // Issue BLTs if there exists transitions that require one.
        if (transInfo.bltCount > 0)
        {
            // Issue all-in-one ReleaseThenAcquire prior to the potential BLTs.
            IssueReleaseThenAcquireSync(pCmdBuf,
                                        pCmdStream,
                                        srcStageMask,
                                        transInfo.bltStageMask,
                                        srcGlobalAccessMask,
                                        transInfo.bltAccessMask,
                                        globalRefreshTcc,
                                        pBarrierOps);

            globalRefreshTcc = IssueAcqRelLayoutTransitionBlt(pCmdBuf, pCmdStream, &transInfo, pBarrierOps);

            // Override srcStageMask and srcGlobalAccessMask to release from BLTs.
            srcStageMask        = transInfo.bltStageMask;
            srcGlobalAccessMask = transInfo.bltAccessMask;
        }

        syncToken = IssueReleaseSync(pCmdBuf,
                                     pCmdStream,
                                     srcStageMask,
                                     srcGlobalAccessMask,
                                     globalRefreshTcc,
                                     pBarrierOps);
    }
    else
    {
        pCmdBuf->NotifyAllocFailure();
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
    const uint32 dstStageMask        = acquireInfo.dstStageMask;
    uint32       dstGlobalAccessMask = acquireInfo.dstGlobalAccessMask;

    // Check if global barrier needs refresh L2
    bool globalRefreshTcc = WaRefreshTccOnGlobalBarrier(0, dstGlobalAccessMask);

    // Always do full-range flush sync.
    for (uint32 i = 0; i < acquireInfo.memoryBarrierCount; i++)
    {
        const MemBarrier& barrier = acquireInfo.pMemoryBarriers[i];

        dstGlobalAccessMask |= barrier.dstAccessMask;
        dstGlobalAccessMask |= barrier.flags.globallyAvailable ? CoherMemory : 0;
    }

    // A container to cache the calculated BLT transitions and some cache info for reuse.
    AcqRelAutoBuffer transitionList(acquireInfo.imageBarrierCount, GetPlatform());

    if (transitionList.Capacity() >= acquireInfo.imageBarrierCount)
    {
        // If BLTs will be issued, we need to know how to acquire for them.
        AcqRelTransitionInfo transInfo = { &transitionList, 0, 0, 0 };

        const uint32 preBltRefreshTcc = GetAcqRelLayoutTransitionBltInfo(pCmdBuf,
                                                                         pCmdStream,
                                                                         acquireInfo,
                                                                         &transInfo,
                                                                         nullptr,
                                                                         &dstGlobalAccessMask,
                                                                         pBarrierOps);
        // Should have no L2 refresh for image barrier as this is done at Release().
        PAL_ASSERT(preBltRefreshTcc == false);

        // Issue acquire for global or pre-BLT sync. No need stall if wait at bottom of pipe
        const uint32 acquireDstStageMask  = (transInfo.bltCount > 0) ? transInfo.bltStageMask : dstStageMask;
        const uint32 acquireDstAccessMask = (transInfo.bltCount > 0) ? transInfo.bltAccessMask : dstGlobalAccessMask;
        const bool   needWaitSyncToken    = (acquireDstStageMask != PipelineStageBottomOfPipe);

        IssueAcquireSync(pCmdBuf,
                         pCmdStream,
                         acquireDstStageMask,
                         acquireDstAccessMask,
                         globalRefreshTcc,
                         (needWaitSyncToken ? syncTokenCount : 0),
                         pSyncTokens,
                         pBarrierOps);

        if (transInfo.bltCount > 0)
        {
            // Issue BLTs.
            globalRefreshTcc = IssueAcqRelLayoutTransitionBlt(pCmdBuf, pCmdStream, &transInfo, pBarrierOps);

            // Issue all-in-one ReleaseThenAcquire prior to the potential BLTs.
            IssueReleaseThenAcquireSync(pCmdBuf,
                                        pCmdStream,
                                        transInfo.bltStageMask,
                                        dstStageMask,
                                        transInfo.bltAccessMask,
                                        dstGlobalAccessMask,
                                        globalRefreshTcc,
                                        pBarrierOps);
        }
    }
    else
    {
        pCmdBuf->NotifyAllocFailure();
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
    uint32 srcStageMask        = barrierInfo.srcStageMask;
    uint32 srcGlobalAccessMask = barrierInfo.srcGlobalAccessMask;
    uint32 dstGlobalAccessMask = barrierInfo.dstGlobalAccessMask;

    // Check if global barrier needs refresh L2
    bool globalRefreshTcc = WaRefreshTccOnGlobalBarrier(srcGlobalAccessMask, dstGlobalAccessMask);

    // Always do full-range flush sync.
    for (uint32 i = 0; i < barrierInfo.memoryBarrierCount; i++)
    {
        const MemBarrier& barrier = barrierInfo.pMemoryBarriers[i];

        srcGlobalAccessMask |= barrier.srcAccessMask;
        dstGlobalAccessMask |= barrier.dstAccessMask;
        dstGlobalAccessMask |= barrier.flags.globallyAvailable ? CoherMemory : 0;
    }

    // A container to cache the calculated BLT transitions and some cache info for reuse.
    AcqRelAutoBuffer transitionList(barrierInfo.imageBarrierCount, GetPlatform());

    if (transitionList.Capacity() >= barrierInfo.imageBarrierCount)
    {
        // If BLTs will be issued, we need to know how to acquire for them.
        AcqRelTransitionInfo transInfo = { &transitionList, 0, 0, 0 };

        globalRefreshTcc |= GetAcqRelLayoutTransitionBltInfo(pCmdBuf,
                                                             pCmdStream,
                                                             barrierInfo,
                                                             &transInfo,
                                                             &srcGlobalAccessMask,
                                                             &dstGlobalAccessMask,
                                                             pBarrierOps);

        // Issue BLTs if there exists transitions that require one.
        if (transInfo.bltCount > 0)
        {
            IssueReleaseThenAcquireSync(pCmdBuf,
                                        pCmdStream,
                                        srcStageMask,
                                        transInfo.bltStageMask,
                                        srcGlobalAccessMask,
                                        transInfo.bltAccessMask,
                                        globalRefreshTcc,
                                        pBarrierOps);

            globalRefreshTcc = IssueAcqRelLayoutTransitionBlt(pCmdBuf, pCmdStream, &transInfo, pBarrierOps);

            // Override srcStageMask and srcGlobalAccessMask to release from BLTs.
            srcStageMask        = transInfo.bltStageMask;
            srcGlobalAccessMask = transInfo.bltAccessMask;
        }

        // Issue acquire for global sync.
        IssueReleaseThenAcquireSync(pCmdBuf,
                                    pCmdStream,
                                    srcStageMask,
                                    barrierInfo.dstStageMask,
                                    srcGlobalAccessMask,
                                    dstGlobalAccessMask,
                                    globalRefreshTcc,
                                    pBarrierOps);
    }
    else
    {
        pCmdBuf->NotifyAllocFailure();
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
            bool fullRangeSynced = false;

            uint32* pCmdSpace = pCmdStream->ReserveCommands();

            if (pCmdBuf->IsGraphicsSupported())
            {
                {
                    AcquireMemGfxSurfSync acquireInfo = {};
                    acquireInfo.cacheSync              = SyncGlvInv | SyncGl1Inv;
                    acquireInfo.rangeBase              = image.GetGpuVirtualAddr();
                    acquireInfo.rangeSize              = gfx9Image.GetGpuMemSyncSize();
                    acquireInfo.flags.dbTargetStall    = 1;
                    acquireInfo.flags.gfx9Gfx10DbWbInv = 1;

                    pCmdSpace += m_cmdUtil.BuildAcquireMemGfxSurfSync(acquireInfo, pCmdSpace);

                    // acquire_mem packets can cause a context roll.
                    pCmdStream->SetContextRollDetected<false>();
                }
            }
            else
            {
                pCmdSpace = pCmdStream->WriteWaitEopGeneric(SyncGlvInv | SyncGl1Inv,
                                                            pCmdBuf->TimestampGpuVirtAddr(), pCmdSpace);
                fullRangeSynced = true;
            }

            pCmdStream->CommitCommands(pCmdSpace);

            if (fullRangeSynced)
            {
                pBarrierOps->pipelineStalls.eopTsBottomOfPipe = 1;
                pBarrierOps->pipelineStalls.waitOnTs          = 1;
                pBarrierOps->caches.invalTcp                  = 1;
                pBarrierOps->caches.invalGl1                  = 1;
                preInitHtileSynced                            = true;
            }
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
            RsrcProcMgr().FastClearEliminate(pCmdBuf,
                                             pCmdStream,
                                             gfx9Image,
                                             imgBarrier.pQuadSamplePattern,
                                             imgBarrier.subresRange);
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
    const BoundGpuMemory& gpuEventBoundMemObj = static_cast<const GpuEvent*>(pGpuEvent)->GetBoundGpuMemory();
    PAL_ASSERT(gpuEventBoundMemObj.IsBound());
    const gpusize         gpuEventStartVa     = gpuEventBoundMemObj.GpuVirtAddr();

    const ReleaseEvents    releaseEvents = GetReleaseEvents(pCmdBuf, *this, stageMask, accessMask, true);
    const ReleaseMemCaches releaseCaches = GetReleaseCacheFlags(accessMask, refreshTcc, pBarrierOps);

    // Note that release event flags for split barrier should meet below conditions,
    //    1). No VsDone as it should be converted to PsDone or Eop.
    //    2). PsDone and CsDone should have been already converted to Eop.
    //    3). rbCache sync must have Eop event set.
    PAL_ASSERT(releaseEvents.vs == 0);
    PAL_ASSERT((releaseEvents.ps & releaseEvents.cs)== 0);
    PAL_ASSERT((releaseEvents.rbCache == 0) || releaseEvents.eop);

    // Pick EOP event if a cache sync is requested because EOS events do not support cache syncs.
    const bool             bumpToEop     = (releaseCaches.u8All != 0) && (releaseEvents.u8All != 0);
    const AcqRelEventType  syncEventType = (releaseEvents.eop || bumpToEop) ? AcqRelEventType::Eop    :
                                           releaseEvents.ps                 ? AcqRelEventType::PsDone :
                                           releaseEvents.cs                 ? AcqRelEventType::CsDone :
                                                                              AcqRelEventType::Invalid;

    // Issue releases with the requested EOP/EOS
    if (syncEventType != AcqRelEventType::Invalid)
    {
        // Build a WRITE_DATA command to first RESET event slots that will be set by event later on.
        WriteDataInfo writeData = {};
        writeData.engineType    = engineType;
        writeData.engineSel     = engine_sel__me_write_data__micro_engine;
        writeData.dstSel        = dst_sel__me_write_data__memory;
        writeData.dstAddr       = gpuEventStartVa;

        pCmdSpace += CmdUtil::BuildWriteData(writeData, GpuEvent::ResetValue, pCmdSpace);

        if (Pal::Device::EngineSupportsGraphics(engineType))
        {
            // Build RELEASE_MEM packet with requested EOP/EOS events at ME engine.
            ReleaseMemGfx releaseMem = {};
            releaseMem.cacheSync = releaseCaches;
            releaseMem.dataSel   = data_sel__me_release_mem__send_32_bit_low;
            releaseMem.data      = GpuEvent::SetValue;
            releaseMem.dstAddr   = gpuEventStartVa;

            switch (syncEventType)
            {
            case AcqRelEventType::Eop:
                releaseMem.vgtEvent = releaseEvents.rbCache ? CACHE_FLUSH_AND_INV_TS_EVENT : BOTTOM_OF_PIPE_TS;
                pBarrierOps->pipelineStalls.eopTsBottomOfPipe = 1;
                if (releaseEvents.rbCache)
                {
                    Pm4CmdBuffer::SetBarrierOperationsRbCacheSynced(pBarrierOps);
                }
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
        else
        {
            ReleaseMemGeneric releaseMem = {};
            releaseMem.engineType = engineType;
            releaseMem.cacheSync  = releaseCaches;
            releaseMem.dataSel    = data_sel__me_release_mem__send_32_bit_low;
            releaseMem.data       = GpuEvent::SetValue;
            releaseMem.dstAddr    = gpuEventStartVa;

            switch (syncEventType)
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
    else // (syncEventType == AcqRelEventType::Invalid)
    {
        WriteDataInfo writeData = {};
        writeData.engineType    = engineType;
        writeData.engineSel     = engine_sel__me_write_data__micro_engine;
        writeData.dstSel        = dst_sel__me_write_data__memory;
        writeData.dstAddr       = gpuEventStartVa;

        pCmdSpace += CmdUtil::BuildWriteData(writeData, GpuEvent::SetValue, pCmdSpace);

        if (releaseCaches.u8All != 0)
        {
            // This is an optimization path to use AcquireMem for cache syncs only (issueSyncEvent = false) case as
            // ReleaseMem requires an EOP or EOS event.
            AcquireMemGeneric acquireMem = {};
            acquireMem.engineType = engineType;

            // This is messy, but the alternative is to return a SyncGlxFlags from GetReleaseCacheFlags. That makes
            // it possible for someone to accidentally add a cache flag later on which is not supported by release_mem.
            // If trying to avoid that kind of bug using an assert is preferable to this mess it can be changed.
            acquireMem.cacheSync  = m_cmdUtil.GetSyncGlxFlagsFromReleaseMemCaches(releaseCaches);

            pCmdSpace += m_cmdUtil.BuildAcquireMemGeneric(acquireMem, pCmdSpace);

            // Required for gfx9: acquire_mem packets can cause a context roll.
            pCmdStream->SetContextRollDetected<false>();
        }
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
    pBarrierOps->pipelineStalls.waitOnTs |= (gpuEventCount != 0);
    for (uint32 i = 0; i < gpuEventCount; i++)
    {
        const GpuEvent* pGpuEvent       = static_cast<const GpuEvent*>(ppGpuEvents[i]);
        const gpusize   gpuEventStartVa = pGpuEvent->GetBoundGpuMemory().GpuVirtAddr();

        pCmdSpace += m_cmdUtil.BuildWaitRegMem(engineType,
                                               mem_space__me_wait_reg_mem__memory_space,
                                               function__me_wait_reg_mem__equal_to_the_reference_value,
                                               engine_sel__me_wait_reg_mem__micro_engine,
                                               gpuEventStartVa,
                                               GpuEvent::SetValue,
                                               UINT32_MAX,
                                               pCmdSpace);
    }

    const SyncGlxFlags acquireCaches = GetAcquireCacheFlags(accessMask, refreshTcc, pBarrierOps);

    if (acquireCaches != SyncGlxNone)
    {
        // We need a trailing acquire_mem to handle any cache sync requests.
        AcquireMemGeneric acquireMem = {};
        acquireMem.engineType = engineType;
        acquireMem.cacheSync  = acquireCaches;

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
    uint32 srcGlobalAccessMask = releaseInfo.srcGlobalAccessMask;

    // Check if global barrier needs refresh L2
    bool globalRefreshTcc = WaRefreshTccOnGlobalBarrier(srcGlobalAccessMask, 0);

    // Always do full-range flush sync.
    for (uint32 i = 0; i < releaseInfo.memoryBarrierCount; i++)
    {
        const MemBarrier& barrier = releaseInfo.pMemoryBarriers[i];

        // globallyAvailable is processed in AcquireEvent().
        srcGlobalAccessMask |= barrier.srcAccessMask;
    }

    // A container to cache the calculated BLT transitions and some cache info for reuse.
    AcqRelAutoBuffer transitionList(releaseInfo.imageBarrierCount, GetPlatform());

    if (transitionList.Capacity() >= releaseInfo.imageBarrierCount)
    {
        // If BLTs will be issued, we need to know how to acquire for them.
        AcqRelTransitionInfo transInfo = { &transitionList, 0, 0, 0 };

        globalRefreshTcc |= GetAcqRelLayoutTransitionBltInfo(pCmdBuf,
                                                             pCmdStream,
                                                             releaseInfo,
                                                             &transInfo,
                                                             &srcGlobalAccessMask,
                                                             nullptr,
                                                             pBarrierOps);

        // Initialize an IGpuEvent* pEvent pointing at the client provided event.
        // If we have internal BLTs, use internal event to signal/wait.
        const IGpuEvent* pActiveEvent = (transInfo.bltCount > 0) ? pCmdBuf->GetInternalEvent() : pClientEvent;

        // Perform an all-in-one release prior to the potential BLTs: IssueReleaseSyncEvent() on pActiveEvent.
        // Defer L2 refresh at acquire if has BLT.
        IssueReleaseSyncEvent(pCmdBuf,
                              pCmdStream,
                              releaseInfo.srcStageMask,
                              srcGlobalAccessMask,
                              (transInfo.bltCount > 0) ? false : globalRefreshTcc,
                              pActiveEvent,
                              pBarrierOps);

        // Issue BLTs if there exists transitions that require one.
        if (transInfo.bltCount > 0)
        {
            // Issue all-in-one acquire prior to the potential BLTs.
            IssueAcquireSyncEvent(pCmdBuf,
                                  pCmdStream,
                                  transInfo.bltStageMask,
                                  transInfo.bltAccessMask,
                                  globalRefreshTcc,
                                  1,
                                  &pActiveEvent,
                                  pBarrierOps);

            // Issue BLTs.
            globalRefreshTcc = IssueAcqRelLayoutTransitionBlt(pCmdBuf, pCmdStream, &transInfo, pBarrierOps);

            // Get back the client provided event and signal it when BLTs are done.
            pActiveEvent = pClientEvent;

            // Release from BLTs.
            IssueReleaseSyncEvent(pCmdBuf,
                                  pCmdStream,
                                  transInfo.bltStageMask,
                                  transInfo.bltAccessMask,
                                  globalRefreshTcc,
                                  pActiveEvent,
                                  pBarrierOps);
        }
    }
    else
    {
        pCmdBuf->NotifyAllocFailure();
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
    uint32 dstGlobalAccessMask = acquireInfo.dstGlobalAccessMask;

    // Check if global barrier needs refresh L2
    bool globalRefreshTcc = WaRefreshTccOnGlobalBarrier(0, dstGlobalAccessMask);

    // Always do full-range flush sync.
    for (uint32 i = 0; i < acquireInfo.memoryBarrierCount; i++)
    {
        const MemBarrier& barrier = acquireInfo.pMemoryBarriers[i];

        dstGlobalAccessMask |= barrier.dstAccessMask;
        dstGlobalAccessMask |= barrier.flags.globallyAvailable ? CoherMemory : 0;
    }

    // A container to cache the calculated BLT transitions and some cache info for reuse.
    AcqRelAutoBuffer transitionList(acquireInfo.imageBarrierCount, GetPlatform());

    if (transitionList.Capacity() >= acquireInfo.imageBarrierCount)
    {
        // If BLTs will be issued, we need to know how to acquire for them.
        AcqRelTransitionInfo transInfo = { &transitionList, 0, 0, 0 };

        const uint32 preBltRefreshTcc = GetAcqRelLayoutTransitionBltInfo(pCmdBuf,
                                                                         pCmdStream,
                                                                         acquireInfo,
                                                                         &transInfo,
                                                                         nullptr,
                                                                         &dstGlobalAccessMask,
                                                                         pBarrierOps);
        // Should have no L2 refresh for image barrier as this is done at Release().
        PAL_ASSERT(preBltRefreshTcc == false);

        const IGpuEvent* const* ppActiveEvents   = ppGpuEvents;
        uint32                  activeEventCount = gpuEventCount;
        const IGpuEvent*        pEvent           = nullptr;

        if (transInfo.bltCount > 0)
        {
            // Issue all-in-one acquire prior to the potential BLTs.
            IssueAcquireSyncEvent(pCmdBuf,
                                  pCmdStream,
                                  transInfo.bltStageMask,
                                  transInfo.bltAccessMask,
                                  false, // refreshTcc
                                  activeEventCount,
                                  ppActiveEvents,
                                  pBarrierOps);

            // Issue BLTs.
            globalRefreshTcc |= IssueAcqRelLayoutTransitionBlt(pCmdBuf, pCmdStream, &transInfo, pBarrierOps);

            // We have internal BLTs, enable internal event to signal/wait.
            pEvent = pCmdBuf->GetInternalEvent();

            // Release from BLTs.
            IssueReleaseSyncEvent(pCmdBuf,
                                  pCmdStream,
                                  transInfo.bltStageMask,
                                  transInfo.bltAccessMask,
                                  0, // Defer Tcc refresh at later global acquire
                                  pEvent,
                                  pBarrierOps);

            ppActiveEvents   = &pEvent;
            activeEventCount = 1;
        }

        // Issue acquire for global cache sync. No need stall if wait at bottom of pipe
        const bool needWaitEvents = (acquireInfo.dstStageMask != PipelineStageBottomOfPipe);

        IssueAcquireSyncEvent(pCmdBuf,
                              pCmdStream,
                              acquireInfo.dstStageMask,
                              dstGlobalAccessMask,
                              globalRefreshTcc,
                              needWaitEvents ? activeEventCount : 0,
                              ppActiveEvents,
                              pBarrierOps);
    }
    else
    {
        pCmdBuf->NotifyAllocFailure();
    }
}

} // Gfx9
} // Pal
