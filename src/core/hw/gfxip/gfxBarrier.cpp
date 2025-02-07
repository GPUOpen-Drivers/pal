/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2023-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/gfxCmdBuffer.h"
#include "core/hw/gfxip/gfxDevice.h"
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
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 902
    const ImgBarrier*             pTransition,
#else
    const BarrierTransition*      pTransition,
#endif
    Developer::BarrierOperations* pOperations
    ) const
{
    Developer::BarrierData data = {};

    data.pCmdBuffer = pGfxCmdBuf;

    if (pTransition != nullptr)
    {
        data.transition    = *pTransition;
        data.hasTransition = true;
    }

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
bool GfxBarrierMgr::NeedWaitCpDma(
    const GfxCmdBuffer* pCmdBuf,
    uint32              srcStageMask)
{
    return TestAnyFlagSet(srcStageMask, PipelineStageBlt | PipelineStageBottomOfPipe) &&
           (pCmdBuf->GetCmdBufState().flags.cpBltActive != 0);
}

}
