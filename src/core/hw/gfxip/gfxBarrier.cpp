/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2023-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/gfxDevice.h"
#include "core/hw/gfxip/pm4CmdBuffer.h"
#include "core/device.h"
#include "core/image.h"

using namespace Util;

namespace Pal
{

// =====================================================================================================================
GfxBarrierMgr::GfxBarrierMgr(
    GfxDevice* pGfxDevice)
    :
    m_pGfxDevice(pGfxDevice),
    m_pDevice(pGfxDevice->Parent()),
    m_pPlatform(m_pDevice->GetPlatform())
{
}

// =====================================================================================================================
// Describes the image barrier to the above layers but only if we're a developer build. Clears the BarrierOperations
// passed in after calling back in case of layout transitions. This function is expected to be called only on layout
// transitions.
void GfxBarrierMgr::DescribeBarrier(
    GfxCmdBuffer*                 pGfxCmdBuf,
    const BarrierTransition*      pTransition,
    Developer::BarrierOperations* pOperations
    ) const
{
    constexpr BarrierTransition NullTransition = {};
    Developer::BarrierData data                = {};

    data.pCmdBuffer    = pGfxCmdBuf;
    data.transition    = (pTransition != nullptr) ? (*pTransition) : NullTransition;
    data.hasTransition = (pTransition != nullptr);

    PAL_ASSERT(pOperations != nullptr);
    // The callback is expected to be made only on layout transitions.
    memcpy(&data.operations, pOperations, sizeof(Developer::BarrierOperations));

    // Callback to the above layers if there is a transition and clear the BarrierOperations.
    m_pDevice->DeveloperCb(Developer::CallbackType::ImageBarrier, &data);
    memset(pOperations, 0, sizeof(Developer::BarrierOperations));
}

// =====================================================================================================================
// Call back to above layers before starting the barrier execution.
void GfxBarrierMgr::DescribeBarrierStart(
    GfxCmdBuffer*          pGfxCmdBuf,
    uint32                 reason,
    Developer::BarrierType type
    ) const
{
    Developer::BarrierData data = {};

    data.pCmdBuffer = pGfxCmdBuf;

    // Make sure we have an acceptable barrier reason.
    PAL_ALERT_MSG((m_pDevice->GetPlatform()->IsDevDriverProfilingEnabled() &&
                  (reason == Developer::BarrierReasonInvalid)),
                  "Invalid barrier reason codes are not allowed!");

    data.reason = reason;
    data.type   = type;

    m_pDevice->DeveloperCb(Developer::CallbackType::BarrierBegin, &data);
}

// =====================================================================================================================
// Callback to above layers with summary information at end of barrier execution.
void GfxBarrierMgr::DescribeBarrierEnd(
    GfxCmdBuffer*                 pGfxCmdBuf,
    Developer::BarrierOperations* pOperations
    ) const
{
    Developer::BarrierData data  = {};

    // Set the barrier type to an invalid type.
    data.pCmdBuffer = pGfxCmdBuf;

    PAL_ASSERT(pOperations != nullptr);
    memcpy(&data.operations, pOperations, sizeof(Developer::BarrierOperations));

    m_pDevice->DeveloperCb(Developer::CallbackType::BarrierEnd, &data);
}

// =====================================================================================================================
// Return converted dstStageMask.
uint32 GfxBarrierMgr::GetPipelineStageMaskFromBarrierInfo(
    const BarrierInfo& barrierInfo,
    uint32*            pSrcStageMask)
{
    PAL_ASSERT(pSrcStageMask != nullptr);
    *pSrcStageMask = 0;

    for (uint32 i = 0; i < barrierInfo.pipePointWaitCount; i++)
    {
        // Note that don't convert HwPipePostPrefetch to FetchIndices as it will cause heavier VS stall.
        constexpr uint32 SrcPipeStageTbl[] =
        {
            PipelineStageTopOfPipe,         // HwPipeTop              = 0x0
            PipelineStageFetchIndirectArgs, // HwPipePostPrefetch     = 0x1
            PipelineStageVs |
            PipelineStageHs |
            PipelineStageDs |
            PipelineStageGs,                // HwPipePreRasterization = 0x2
            PipelineStagePs,                // HwPipePostPs           = 0x3
            PipelineStageLateDsTarget,      // HwPipePreColorTarget   = 0x4
            PipelineStageCs,                // HwPipePostCs           = 0x5
            PipelineStageBlt,               // HwPipePostBlt          = 0x6
            PipelineStageBottomOfPipe,      // HwPipeBottom           = 0x7
        };

        *pSrcStageMask |= SrcPipeStageTbl[barrierInfo.pPipePoints[i]];
    }

    for (uint32 i = 0; i < barrierInfo.rangeCheckedTargetWaitCount; i++)
    {
        const Pal::Image* pImage = static_cast<const Pal::Image*>(barrierInfo.ppTargets[i]);

        if (pImage != nullptr)
        {
            *pSrcStageMask |= pImage->IsDepthStencilTarget() ? PipelineStageDsTarget : PipelineStageColorTarget;
        }
    }

    constexpr uint32 DstPipeStageTbl[] =
    {
        PipelineStageTopOfPipe,     // HwPipeTop              = 0x0
        PipelineStageCs |
        PipelineStageVs |
        PipelineStageBlt,           // HwPipePostPrefetch     = 0x1
        PipelineStageEarlyDsTarget, // HwPipePreRasterization = 0x2
        PipelineStageLateDsTarget,  // HwPipePostPs           = 0x3
        PipelineStageColorTarget,   // HwPipePreColorTarget   = 0x4
        PipelineStageBottomOfPipe,  // HwPipePostCs           = 0x5
        PipelineStageBottomOfPipe,  // HwPipePostBlt          = 0x6
        PipelineStageBottomOfPipe,  // HwPipeBottom           = 0x7
    };

    const uint32 dstStageMask = DstPipeStageTbl[barrierInfo.waitPoint];

    return dstStageMask;
}

// =====================================================================================================================
bool GfxBarrierMgr::IsReadOnlyTransition(
    uint32 srcAccessMask,
    uint32 dstAccessMask)
{
    return (srcAccessMask != 0) &&
           (dstAccessMask != 0) &&
           (TestAnyFlagSet(srcAccessMask | dstAccessMask, CacheCoherWriteMask) == false);
}

// =====================================================================================================================
// Helper function that takes a BarrierInfo, and splits the SubresRanges in the BarrierTransitions that have multiple
// planes specified into BarrierTransitions with a single plane SubresRange specified. If the this function allocates
// memory pMemAllocated is set to true and the caller is responsible for deleting the memory.
Result GfxBarrierMgr::SplitBarrierTransitions(
    Platform*    pPlatform,
    BarrierInfo* pBarrier,      // Copy of a BarrierInfo struct that can have its pTransitions replaced.
                                // If pTransitions contains imageInfos with SubresRanges that contain
                                // multiple planes then a new list of transitions will be allocated,
                                // so that the list of transitions only contain single plane ranges. If
                                // memory is allocated for a new list of transitions true is returned
                                // (false otherwise), and the caller is responsible for deleting the
                                // memory.
    bool*        pMemAllocated) // If the this function allocates memory true is set and the caller is
                                // responsible for deleting the memory.
{
    PAL_ASSERT(pBarrier != nullptr);

    Result result = Result::Success;

    *pMemAllocated = false;

    uint32 splitCount = 0;
    for (uint32 i = 0; i < pBarrier->transitionCount; i++)
    {
        const BarrierTransition& transition = pBarrier->pTransitions[i];
        splitCount += (transition.imageInfo.pImage != nullptr) ? transition.imageInfo.subresRange.numPlanes : 1;
    }

    PAL_ASSERT(splitCount >= pBarrier->transitionCount);

    if (splitCount > pBarrier->transitionCount)
    {
        BarrierTransition* pNewSplitTransitions =
            PAL_NEW_ARRAY(BarrierTransition, splitCount, pPlatform, AllocInternalTemp);
        if (pNewSplitTransitions == nullptr)
        {
            result = Result::ErrorOutOfMemory;
        }
        else
        {
            *pMemAllocated = true;

            // Copy the transitions to the new memory and split them when necessary.
            uint32 newSplitCount = 0;
            for (uint32 i = 0; i < pBarrier->transitionCount; i++)
            {
                const BarrierTransition& transition = pBarrier->pTransitions[i];
                pNewSplitTransitions[newSplitCount] = transition;
                newSplitCount++;

                if (transition.imageInfo.pImage != nullptr)
                {
                    // newSplitCount was incremented above so use - 1 here.
                    pNewSplitTransitions[newSplitCount-1].imageInfo.subresRange.numPlanes = 1;

                    const SubresRange& subresRange = pBarrier->pTransitions[i].imageInfo.subresRange;

                    for (uint32 plane = subresRange.startSubres.plane + 1;
                        plane < (subresRange.startSubres.plane + subresRange.numPlanes);
                        plane++)
                    {
                        pNewSplitTransitions[newSplitCount] = pBarrier->pTransitions[i];
                        pNewSplitTransitions[newSplitCount].imageInfo.subresRange.numPlanes = 1;
                        pNewSplitTransitions[newSplitCount].imageInfo.subresRange.startSubres.plane = plane;
                        newSplitCount++;
                    }
                }
            }

            PAL_ASSERT(newSplitCount == splitCount);

            pBarrier->transitionCount = newSplitCount;
            pBarrier->pTransitions    = pNewSplitTransitions;
        }
    }

    return result;
}

// =====================================================================================================================
// Helper function that takes an AcquireReleaseInfo, and splits the SubresRanges in the ImgBarriers that have multiple
// planes specified into ImgBarriers with a single plane SubresRanges specified. If the this function allocates memory
// pMemAllocated is set to true and the caller is responsible for deleting the memory.
Result GfxBarrierMgr::SplitImgBarriers(
    Platform*           pPlatform,
    AcquireReleaseInfo* pBarrier,      // Copy of a AcquireReleaseInfo struct that can have its pImageBarriers
                                       // replaced. If pImageBarriers has SubresRanges that contain
                                       // multiple planes then a new list of image barriers will be allocated,
                                       // so that the list of barriers only contain single plane ranges. If
                                       // memory is allocated for a new list of barriers true is returned
                                       // (false otherwise), and the caller is responsible for deleting the
                                       // memory.
    bool*               pMemAllocated) // If the this function allocates memory true is set and the caller is
                                       // responsible for deleting the memory.
{
    PAL_ASSERT(pBarrier != nullptr);

    Result result = Result::Success;

    *pMemAllocated = false;

    uint32 splitCount = 0;
    for (uint32 i = 0; i < pBarrier->imageBarrierCount; i++)
    {
        splitCount += pBarrier->pImageBarriers[i].subresRange.numPlanes;
    }

    PAL_ASSERT(splitCount >= pBarrier->imageBarrierCount);

    if (splitCount > pBarrier->imageBarrierCount)
    {
        ImgBarrier* pNewSplitTransitions = PAL_NEW_ARRAY(ImgBarrier, splitCount, pPlatform, AllocInternalTemp);
        if (pNewSplitTransitions == nullptr)
        {
            result = Result::ErrorOutOfMemory;
        }
        else
        {
            *pMemAllocated = true;

            // Copy the transitions to the new memory and split them when necessary.
            uint32 newSplitCount = 0;
            for (uint32 i = 0; i < pBarrier->imageBarrierCount; i++)
            {
                pNewSplitTransitions[newSplitCount] = pBarrier->pImageBarriers[i];
                pNewSplitTransitions[newSplitCount].subresRange.numPlanes = 1;
                newSplitCount++;

                const SubresRange& subresRange = pBarrier->pImageBarriers[i].subresRange;

                for (uint32 plane = subresRange.startSubres.plane + 1;
                    plane < (subresRange.startSubres.plane + subresRange.numPlanes);
                    plane++)
                {
                    pNewSplitTransitions[newSplitCount] = pBarrier->pImageBarriers[i];
                    pNewSplitTransitions[newSplitCount].subresRange.numPlanes = 1;
                    pNewSplitTransitions[newSplitCount].subresRange.startSubres.plane = plane;
                    newSplitCount++;
                }
            }

            PAL_ASSERT(newSplitCount == splitCount);

            pBarrier->imageBarrierCount = newSplitCount;
            pBarrier->pImageBarriers    = pNewSplitTransitions;
        }
    }

    return result;
}

// =====================================================================================================================
// Helper function to convert certain pipeline points to more accurate ones. This is for legacy barrier interface.
// Note: HwPipePostBlt will be converted to a more accurate stage based on the underlying implementation of
//       outstanding BLTs, but will be left as HwPipePostBlt if the internal outstanding BLTs can't be expressed as
//       a client-facing HwPipePoint (e.g., if there are CP DMA BLTs in flight).
void GfxBarrierMgr::OptimizePipePoint(
    const Pm4CmdBuffer* pCmdBuf,
    HwPipePoint*        pPipePoint)
{
    if (pPipePoint != nullptr)
    {
        if (*pPipePoint == HwPipePostBlt)
        {
            // Check xxxBltActive states in order
            const Pm4CmdBufferStateFlags cmdBufStateFlags = pCmdBuf->GetPm4CmdBufState().flags;
            if (cmdBufStateFlags.gfxBltActive)
            {
                *pPipePoint = HwPipeBottom;
            }
            else if (cmdBufStateFlags.csBltActive)
            {
                *pPipePoint = HwPipePostCs;
            }
            else if (cmdBufStateFlags.cpBltActive)
            {
                // Leave it as HwPipePostBlt because CP DMA BLTs cannot be expressed as more specific HwPipePoint.
            }
            else
            {
                // If there are no BLTs in flight at this point, we will set the pipe point to HwPipeTop. This will
                // optimize any redundant stalls when called from the barrier implementation. Otherwise, this function
                // remaps the pipe point based on the gfx block that performed the BLT operation.
                *pPipePoint = HwPipeTop;
            }
        }
        else if (*pPipePoint == HwPipePreColorTarget)
        {
            // HwPipePreColorTarget is only valid as wait point. But for the sake of robustness, if it's used as pipe
            // point to wait on, it's equivalent to HwPipePostPs.
            *pPipePoint = HwPipePostPs;
        }
    }
}

// =====================================================================================================================
// Helper function to optimize cache mask by clearing unnecessary coherency flags. This is for legacy barrier interface.
void GfxBarrierMgr::OptimizeSrcCacheMask(
    const Pm4CmdBuffer* pCmdBuf,
    uint32*             pCacheMask)
{
    if (pCacheMask != nullptr)
    {
        // There are various srcCache BLTs (Copy, Clear, and Resolve) which we can further optimize if we know which
        // write caches have been dirtied:
        // - If a graphics BLT occurred, alias these srcCaches to CoherColorTarget.
        // - If a compute BLT occurred, alias these srcCaches to CoherShader.
        // - If a CP L2 BLT occurred, alias these srcCaches to CoherCp.
        // - If a CP direct-to-memory write occurred, alias these srcCaches to CoherMemory.
        // Clear the original srcCaches from the srcCache mask for the rest of this scope.
        if (TestAnyFlagSet(*pCacheMask, CacheCoherencyBlt))
        {
            const Pm4CmdBufferStateFlags cmdBufStateFlags = pCmdBuf->GetPm4CmdBufState().flags;
            const bool                   isCopySrcOnly    = (*pCacheMask == CoherCopySrc);

            *pCacheMask |= cmdBufStateFlags.cpWriteCachesDirty ? CoherCp : 0;
            *pCacheMask |= cmdBufStateFlags.cpMemoryWriteL2CacheStale ? CoherMemory : 0;

            if (isCopySrcOnly)
            {
                *pCacheMask |= cmdBufStateFlags.gfxWriteCachesDirty ? CoherShaderRead : 0;
                *pCacheMask |= cmdBufStateFlags.csWriteCachesDirty ? CoherShaderRead : 0;
            }
            else
            {
                *pCacheMask |= cmdBufStateFlags.gfxWriteCachesDirty ? CoherColorTarget : 0;
                *pCacheMask |= cmdBufStateFlags.csWriteCachesDirty ? CoherShader : 0;
            }

            *pCacheMask &= ~CacheCoherencyBlt;
        }
    }
}

// =====================================================================================================================
// Helper function to optimize pipeline access masks for BLTs. This is for acquire/release interface.
// This function also mask off all graphics path specific stage mask flags for non-universal command buffer as well as
// remove some invalid pfp stage mask on dstStageMask to avoid unnecessary PFP_SYNC_ME stall.
void GfxBarrierMgr::OptimizeStageMask(
    const Pm4CmdBuffer* pCmdBuf,
    BarrierType         barrierType,
    uint32*             pSrcStageMask,
    uint32*             pDstStageMask,
    bool                isClearToTarget // Optimization hint.
    ) const
{
    PAL_ASSERT((pSrcStageMask != nullptr) && (pDstStageMask != nullptr));

    const Pm4CmdBufferStateFlags stateFlags = pCmdBuf->GetPm4CmdBufState().flags;

    // Update pipeline stages if valid input stage mask is provided.
    if (TestAnyFlagSet(*pSrcStageMask, PipelineStageBlt))
    {
        constexpr uint32 PipelineStagesRb = PipelineStageDsTarget | PipelineStageColorTarget;
        const bool       nonBufferBarrier = (barrierType != BarrierType::Buffer);

        // For Clear to target transition, if the clear was done through graphics draw, can skip the barrier;
        // so no need OR with CoherColorTarget here in this case.
        // Buffer RPM calls never go through graphics draw (either compute or CP DMA).
        const bool checkGfxBltActive = stateFlags.gfxBltActive &&
                                       nonBufferBarrier        &&
                                       (isClearToTarget == false);
        // CP DMA copy is buffer only. Note that there are corner cases that image copy uses CP DMA in
        // CmdCopyMemoryFromToImageViaPixels() and CmdCopyImageToImageViaPixels() but both cases explicitly wait on
        // CP DMA copy done post the copy.
        const bool checkCpBltActive  = stateFlags.cpBltActive && (barrierType != BarrierType::Image);

        *pSrcStageMask &= ~PipelineStageBlt;
        *pSrcStageMask |= (checkGfxBltActive      ? PipelineStagesRb : 0) |
                          (stateFlags.csBltActive ? PipelineStageCs  : 0) |
                          // Add back PipelineStageBlt because we cannot express it with a more accurate stage.
                          (checkCpBltActive       ? PipelineStageBlt : 0);
    }

    // Mark off all graphics path specific stages and caches if command buffer doesn't support graphics.
    if (pCmdBuf->GetEngineType() != EngineTypeUniversal)
    {
        *pSrcStageMask &= ~PipelineStagesGraphicsOnly;
        *pDstStageMask &= ~PipelineStagesGraphicsOnly;
    }

    // No need acquire at PFP for image barriers. Image may have metadata that's accessed by PFP by
    // it's handled properly internally and no need concern here.
    if ((barrierType == BarrierType::Image) &&
        TestAnyFlagSet(*pDstStageMask, PipelineStagePfpMask))
    {
        *pDstStageMask &= ~PipelineStagePfpMask;

        // If no dstStageMask flag after removing PFP flags, force waiting at ME.
        if (*pDstStageMask == 0)
        {
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 835
            *pDstStageMask = PipelineStagePostPrefetch;
#else
            *pDstStageMask = PipelineStageBlt;
#endif
        }
    }
}

// =====================================================================================================================
// Helper function to optimize pipeline cache access masks for BLTs. This is for acquire/release interface.
// This function also mask off all graphics path specific access mask flags for non-universal command buffer.
// Return if need flush and invalidate GL2 cache.
bool GfxBarrierMgr::OptimizeAccessMask(
    const Pm4CmdBuffer* pCmdBuf,
    BarrierType         barrierType,
    const Pal::Image*   pImage,        // Optimization hint
    uint32*             pSrcAccessMask,
    uint32*             pDstAccessMask,
    bool                shaderMdAccessIndirectOnly
    ) const
{
    PAL_ASSERT((pSrcAccessMask != nullptr) && (pDstAccessMask != nullptr));

    const Pm4CmdBufferStateFlags stateFlags       = pCmdBuf->GetPm4CmdBufState().flags;
    const uint32                 orgSrcAccessMask = *pSrcAccessMask;
    const bool                   nonBufferBarrier = (barrierType != BarrierType::Buffer);

    // Update cache access masks if valid input access mask is provided.
    if (TestAnyFlagSet(*pSrcAccessMask, CacheCoherencyBlt))
    {
        const bool isBltCopySrcOnly = ((orgSrcAccessMask & CacheCoherencyBlt) == CoherCopySrc);
        // Allow clear to target transition to skip checking gfxWriteCachesDirty as graphics clear to target doesn't
        // requires cache sync (flush/inv RB cache).
        const bool isClearToTarget  = IsClearToTargetTransition(*pSrcAccessMask, *pDstAccessMask);

        *pSrcAccessMask &= ~CacheCoherencyBlt;

        *pSrcAccessMask |= (stateFlags.cpWriteCachesDirty         ? CoherCp     : 0) |
                           (stateFlags.cpMemoryWriteL2CacheStale  ? CoherMemory : 0);

        if (isBltCopySrcOnly)
        {
            *pSrcAccessMask |= ((stateFlags.gfxWriteCachesDirty && nonBufferBarrier) ||
                                stateFlags.csWriteCachesDirty) ? CoherShaderRead : 0;
        }
        else // For Clear, CopyDst, ResolveSrc and ResolveDst. ResolveSrc is bound to CB for fixed-func resolve.
        {
            // For Clear to target transition, if the clear was done through graphics draw, can skip the barrier;
            // so no need OR with CoherColorTarget here in this case.
            // Buffer RPM calls never go through graphics draw (either compute or CP DMA).
            const bool checkGfxWriteCacheDirty = stateFlags.gfxWriteCachesDirty &&
                                                 nonBufferBarrier               &&
                                                 (isClearToTarget == false);

            *pSrcAccessMask |= (checkGfxWriteCacheDirty       ? CoherColorTarget : 0) |
                               (stateFlags.csWriteCachesDirty ? CoherShader      : 0);
        }
    }

    // Mark off all graphics path specific stages and caches if command buffer doesn't support graphics.
    if (pCmdBuf->GetEngineType() != EngineTypeUniversal)
    {
        *pSrcAccessMask &= ~CacheCoherencyGraphicsOnly;
        *pDstAccessMask &= ~CacheCoherencyGraphicsOnly;
    }

    // Handle misaligned metadata WA for CacheCoherencyBltDst access flags: if orgSrcAccessMask contains any
    // CacheCoherencyBltDst flag and there is outstanding misaligned metadata write dirty flag, need sync GL2 cache.
    // Note that we must handle GL2 sync here as clients may call OptimizeAcqRelReleaseInfo() to convert BLT access
    // flags into explicit access flags and then call barrier interface with converted explicit access flags back
    // to PAL, where PAL may lose the info to handle the WA.
    //
    // Mask off buffer only coherency flags that never applied to image.
    const uint32 waSrcAccessMask = orgSrcAccessMask  & ~CoherBufferOnlyMask;
    const uint32 waDstAccessMask = (*pDstAccessMask) & ~CoherBufferOnlyMask;
    bool         syncGl2         = false;

    if (TestAnyFlagSet(waSrcAccessMask, CacheCoherencyBltDst)    &&
        nonBufferBarrier                                         &&
        pCmdBuf->IsBltWriteMisalignedMdDirty()                   &&
        ((pImage == nullptr) || pImage->HasMisalignedMetadata()))
    {
        const bool anySrcDirectWrite       = (stateFlags.csBltDirectWriteMisalignedMdDirty |
                                              stateFlags.gfxBltDirectWriteMisalignedMdDirty) != 0;
        const bool srcIndirectWriteOnly    = (anySrcDirectWrite == false) &&
                                             (stateFlags.csBltIndirectWriteMisalignedMdDirty != 0);
        const bool srcDirectWriteOnly      = anySrcDirectWrite &&
                                             (stateFlags.csBltIndirectWriteMisalignedMdDirty == 0);
        const bool backToBackDirectWrite   = srcDirectWriteOnly                                                &&
                                             (TestAnyFlagSet(waSrcAccessMask, ~CacheCoherencyBltDst) == false) &&
                                             ((waDstAccessMask == CoherColorTarget)                            ||
                                              (waDstAccessMask == CoherDepthStencilTarget));
        // For CoherShaderWrite from image layout transition blt, it doesn't exactly indicate an indirect write
        // mode as image layout transition blt may direct write to fix up metadata. optimizeBacktoBackShaderWrite
        // makes sure when it's safe to optimize it.
        const bool backToBackIndirectWrite = srcIndirectWriteOnly                                              &&
                                             (TestAnyFlagSet(waSrcAccessMask, ~CacheCoherencyBltDst) == false) &&
                                             shaderMdAccessIndirectOnly                                        &&
                                             TestAnyFlagSet(waDstAccessMask, CoherShaderWrite)                 &&
                                             (TestAnyFlagSet(waDstAccessMask, ~CoherShader) == false);

        // Need sync GL2 if not back to back direct/indirect write.
        syncGl2 = (backToBackDirectWrite == false) && (backToBackIndirectWrite == false);
    }

    return syncGl2;
}

}
