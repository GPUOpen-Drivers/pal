/*
 *******************************************************************************
 *
 * Copyright (c) 2015-2017 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#include "core/hw/gfxip/gfx6/gfx6Device.h"
#include "core/hw/gfxip/gfx6/gfx6Image.h"
#include "core/hw/gfxip/gfx6/gfx6UniversalCmdBuffer.h"

using namespace Util;

namespace Pal
{
namespace Gfx6
{

enum DepthStencilBlt : uint32
{
    // Available BLTs for depth stencil image.
    Expand         = 0x01,
    ExpandHiZRange = 0x02,
    Resummarize    = 0x04,
};

enum ColorBlt : uint32
{
    // Available BLTs for color image.
    DccDecompress       = 0x01,
    FmaskDecompress     = 0x02,
    FastClearEliminate  = 0x04,
    MsaaColorDecompress = 0x08,
};

// =====================================================================================================================
// Go through the mip levels from startMip to lastMip, count how many mips have the exact same BLT as the startMip.
static uint32 FindConsecutiveOps(
    const uint32* pBlt,
    uint32        startMip,
    uint32        lastMip)
{
    uint32 processMipCount = 1;
    while ((startMip + processMipCount <= lastMip) && (pBlt[startMip + processMipCount] == pBlt[startMip]))
    {
        processMipCount++;
    }
    return processMipCount;
}

// =====================================================================================================================
// Creates an MSAA state with the sample positions specified by the client for the given transition.
// The caller of this function must destroy the MSAA state object and free the memory associated with it.
static IMsaaState* BarrierMsaaState(
    const Device*                                pDevice,
    GfxCmdBuffer*                                pCmdBuf,
    LinearAllocatorAuto<VirtualLinearAllocator>* pAllocator,
    const BarrierTransition&                     transition)
{
    const auto& imageCreateInfo = transition.imageInfo.pImage->GetImageCreateInfo();

    MsaaStateCreateInfo msaaInfo    = {};
    msaaInfo.sampleMask             = 0xFFFF;
    msaaInfo.coverageSamples        = imageCreateInfo.samples;
    msaaInfo.alphaToCoverageSamples = imageCreateInfo.samples;

    // The following parameters should never be higher than the max number of msaa fragments ( 8 ).
    // All MSAA graphics barrier operations performed by PAL work on a per fragment basis.
    msaaInfo.exposedSamples          = imageCreateInfo.fragments;
    msaaInfo.pixelShaderSamples      = imageCreateInfo.fragments;
    msaaInfo.depthStencilSamples     = imageCreateInfo.fragments;
    msaaInfo.shaderExportMaskSamples = imageCreateInfo.fragments;
    msaaInfo.sampleClusters          = imageCreateInfo.fragments;
    msaaInfo.occlusionQuerySamples   = imageCreateInfo.fragments;

    IMsaaState* pMsaaState = nullptr;
    void*       pMemory    = PAL_MALLOC(pDevice->GetMsaaStateSize(msaaInfo, nullptr), pAllocator, AllocInternalTemp);
    if (pMemory == nullptr)
    {
        pCmdBuf->NotifyAllocFailure();
    }
    else
    {
        Result result = pDevice->CreateMsaaState(msaaInfo, pMemory, &pMsaaState);
        PAL_ASSERT(result == Result::Success);
    }

    return pMsaaState;
}

// =====================================================================================================================
// Issue BLT operations (i.e., decompress, resummarize) necessary to convert a depth/stencil image from one ImageLayout
// to another.
//
// This method is expected to be called twice per transition in a CmdBarrier() call.  The first call (earlyPhase ==
// true) should be made before any client-requested stalls or cache flushes are executed, the second call (earlyPhase ==
// false) should be done after.  This allows a reuse of the logic whether the decompress BLT can be pipelined or not.
//
// pSyncReqs will be updated to reflect synchronization that must be performed after the BLT.
void Device::TransitionDepthStencil(
    GfxCmdBuffer*                 pCmdBuf,
    const BarrierTransition&      transition,
    bool                          earlyPhase,
    SyncReqs*                     pSyncReqs,
    Developer::BarrierOperations* pOperations
    ) const
{
    PAL_ASSERT(transition.imageInfo.pImage != nullptr);

    // The "earlyPhase" for decompress/resummarize BLTs is before any waits and/or cache flushes have been inserted.
    // It is safe to perform a depth expand or htile resummarize in the early phase if the client reports there is dirty
    // data in the DB caches for this image.
    //
    // This indicates:
    //
    //     1) There is no need to flush compressed data out of another cache or invalidate stale data in the DB
    //        caches before issuing the fixed-function DB blt:  the data is already in the right caches.
    //     2) There is no need to stall before beginning the operation.  Data can only be dirty in one source cache
    //        at a time in a well-defined program, so we know the last output to this image was done with the DB.
    //
    // If this transition does not flush dirty data out of the DB caches, we delay the decompress until all client-
    // specified stalls and cache flushes have been executed (the late phase).  This situation should be rare,
    // occurring in cases like a clear to shader read transition without any rendering in between.
    if (earlyPhase == TestAnyFlagSet(transition.srcCacheMask, CoherDepthStencilTarget))
    {
        const auto& image                  = static_cast<const Pal::Image&>(*transition.imageInfo.pImage);
        const auto& gfx6Image              = static_cast<const Image&>(*image.GetGfxImage());
        uint32      blt[MaxImageMipLevels] = {};
        bool        issuedBlt              = GetDepthStencilBltPerSubres(pCmdBuf, blt, transition, earlyPhase);

        PAL_ASSERT(image.IsDepthStencil());

        const SubresRange& inputRange  = transition.imageInfo.subresRange;
        SubresRange        subresRange = inputRange;
        uint32             mip         = inputRange.startSubres.mipLevel;
        const uint32       lastMip     = mip + inputRange.numMips - 1;
        while (mip <= lastMip)
        {
            subresRange.startSubres.mipLevel = mip;
            subresRange.numMips              = FindConsecutiveOps(blt, mip, lastMip);

            if (TestAnyFlagSet(blt[mip], DepthStencilBlt::Expand))
            {
                DepthStencilExpand(pCmdBuf, transition, gfx6Image, subresRange, pOperations);
            }
            else if (TestAnyFlagSet(blt[mip], DepthStencilBlt::ExpandHiZRange))
            {
                DepthStencilExpandHiZRange(pCmdBuf, transition, gfx6Image, subresRange, pSyncReqs, pOperations);
            }
            else if (TestAnyFlagSet(blt[mip], DepthStencilBlt::Resummarize))
            {
                DepthStencilResummarize(pCmdBuf, transition, gfx6Image, subresRange, pOperations);
            }
            mip += subresRange.numMips;
        }

        // Flush DB/TC caches to memory after decompressing/resummarizing.
        if (issuedBlt)
        {
            // Issue surface sync stalls on depth/stencil surface writes and flush DB caches
            pSyncReqs->cpCoherCntl.bits.DB_DEST_BASE_ENA = 1;
            pSyncReqs->cpCoherCntl.bits.DEST_BASE_0_ENA  = 1;
            pSyncReqs->cpCoherCntl.bits.DB_ACTION_ENA    = 1;

            // The decompress/resummarize blit that was just executed was effectively a PAL-initiated draw that wrote to
            // the image and/or htile as a DB destination.  In addition to flushing the data out of the DB cache, we
            // need to invalidate any possible read/write caches that need coherent reads of this image's data.  If the
            // client was already rendering to this image through the DB caches on its own (i.e., srcCacheMask includes
            // CoherDepthStencilTarget), this shouldn't result in any additional sync.
            //
            // Note that we must always invalidate these caches if the client didn't give us any cache information.
            const bool noCacheFlags = ((transition.srcCacheMask == 0) && (transition.dstCacheMask == 0));

            if (TestAnyFlagSet(transition.dstCacheMask, CoherShader | CoherCopy | CoherResolve) || noCacheFlags)
            {
                pSyncReqs->cpCoherCntl.bits.TCL1_ACTION_ENA = 1;
                pSyncReqs->cpCoherCntl.bits.TC_ACTION_ENA   = 1;
            }
        }
    }
}

// =====================================================================================================================
// For each mip level in subResRange, calculate the BLT operations needed during TransitionDepthStencil().
//
// The operations are stored in pBlt,  where each uint32 is the BLT operations for one mip level. If a BLT is needed for
// a mip level, the bit location marked by DepthStencilBlt will be set.  The return value is a bool, indicating whether
// we'll need to flush DB/TC caches.
bool Device::GetDepthStencilBltPerSubres(
    GfxCmdBuffer*            pCmdBuf,
    uint32*                  pBlt,
    const BarrierTransition& transition,
    bool                     earlyPhase
    ) const
{
    const auto& image     = static_cast<const Pal::Image&>(*transition.imageInfo.pImage);
    const auto& gfx6Image = static_cast<const Image&>(*image.GetGfxImage());
    bool        issuedBlt = false;

    const SubresRange& inputRange = transition.imageInfo.subresRange;
    SubresId           subRes     = inputRange.startSubres;
    for (uint32 mip = inputRange.startSubres.mipLevel;
                mip < inputRange.startSubres.mipLevel + inputRange.numMips;
                mip++)
    {
        subRes.mipLevel = mip;

        const DepthStencilCompressionState oldState =
            gfx6Image.LayoutToDepthCompressionState(subRes, transition.imageInfo.oldLayout);
        const DepthStencilCompressionState newState =
            gfx6Image.LayoutToDepthCompressionState(subRes, transition.imageInfo.newLayout);

        if ((oldState == DepthStencilCompressed) && (newState != DepthStencilCompressed))
        {
            // Performing an expand in the late phase is not ideal for performance, as it indicates the decompress
            // could not be pipelined and likely resulted in a bubble.  If an app is hitting this alert too often, it
            // may have an impact on performance.
            PAL_ALERT(earlyPhase == false);

            pBlt[mip] |= DepthStencilBlt::Expand;
            issuedBlt = true;
        }
        // Resummarize the htile values from the depth-stencil surface contents when transitioning from "HiZ invalid"
        // state to something that uses HiZ.
        else if ((transition.imageInfo.subresRange.startSubres.aspect == ImageAspect::Depth) &&
                 (oldState == DepthStencilDecomprNoHiZ) && (newState != DepthStencilDecomprNoHiZ))
        {
            // If we are transitioning from uninitialized, resummarization is redundant.  This is because within
            // this same barrier, we have just initialized the htile to known values.
            if (TestAnyFlagSet(transition.imageInfo.oldLayout.usages, LayoutUninitializedTarget) == false)
            {
                if ((pCmdBuf->GetEngineType() == EngineTypeCompute) || Pal::Image::ForceExpandHiZRangeForResummarize)
                {
                    pBlt[mip] |= DepthStencilBlt::ExpandHiZRange;
                }
                else
                {
                    pBlt[mip] |= DepthStencilBlt::Resummarize;
                    issuedBlt = true;
                }
            }
        }
    }

    return issuedBlt;
}

// =====================================================================================================================
void Device::DepthStencilExpand(
    GfxCmdBuffer*                 pCmdBuf,
    const BarrierTransition&      transition,
    const Image&                  gfx6Image,
    const SubresRange&            subresRange,
    Developer::BarrierOperations* pOperations
    ) const
{
    pOperations->layoutTransitions.depthStencilExpand = 1;
    DescribeBarrier(pCmdBuf,
                    pOperations,
                    &transition);

    LinearAllocatorAuto<VirtualLinearAllocator> allocator(pCmdBuf->Allocator(), false);
    IMsaaState* pMsaaState = BarrierMsaaState(this, pCmdBuf, &allocator, transition);

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 339 && PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 280
    RsrcProcMgr().ExpandDepthStencil(pCmdBuf,
                                     *gfx6Image.Parent(),
                                     pMsaaState,
                                     &transition.imageInfo.samplePattern,
                                     subresRange);
#else
    RsrcProcMgr().ExpandDepthStencil(pCmdBuf,
                                     *gfx6Image.Parent(),
                                     pMsaaState,
                                     transition.imageInfo.pQuadSamplePattern,
                                     subresRange);
#endif

    pMsaaState->Destroy();
    PAL_SAFE_FREE(pMsaaState, &allocator);
}

// =====================================================================================================================
void Device::DepthStencilExpandHiZRange(
    GfxCmdBuffer*                 pCmdBuf,
    const BarrierTransition&      transition,
    const Image&                  gfx6Image,
    const SubresRange&            subresRange,
    SyncReqs*                     pSyncReqs,
    Developer::BarrierOperations* pOperations
    ) const
{
    pOperations->layoutTransitions.htileHiZRangeExpand = 1;
    DescribeBarrier(pCmdBuf,
                    pOperations,
                    &transition);

    // CS blit to open-up the HiZ range.
    RsrcProcMgr().HwlExpandHtileHiZRange(pCmdBuf, gfx6Image, subresRange);

    // We need to wait for the compute shader to finish and also invalidate the texture cache before
    // any further depth rendering can be done to this Image.
    pSyncReqs->csPartialFlush                   = 1;
    pSyncReqs->cpCoherCntl.bits.TCL1_ACTION_ENA = 1;
    pSyncReqs->cpCoherCntl.bits.TC_ACTION_ENA   = 1;
}

// =====================================================================================================================
void Device::DepthStencilResummarize(
    GfxCmdBuffer*                 pCmdBuf,
    const BarrierTransition&      transition,
    const Image&                  gfx6Image,
    const SubresRange&            subresRange,
    Developer::BarrierOperations* pOperations
    ) const
{
    pOperations->layoutTransitions.depthStencilResummarize = 1;
    DescribeBarrier(pCmdBuf,
                    pOperations,
                    &transition);

    LinearAllocatorAuto<VirtualLinearAllocator> allocator(pCmdBuf->Allocator(), false);
    IMsaaState* pMsaaState = BarrierMsaaState(this, pCmdBuf, &allocator, transition);

    // DB blit to resummarize.
    RsrcProcMgr().ResummarizeDepthStencil(pCmdBuf,
                                          *gfx6Image.Parent(),
                                          transition.imageInfo.newLayout,
                                          pMsaaState,
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 339 && PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 280
                                          &transition.imageInfo.samplePattern,
#else
                                          transition.imageInfo.pQuadSamplePattern,
#endif
                                          subresRange);

    pMsaaState->Destroy();
    PAL_SAFE_FREE(pMsaaState, &allocator);
}

// =====================================================================================================================
// Issue any BLT operations (i.e., decompresses) necessary to convert a color image from one ImageLayout to another.
//
// This method is expected to be called twice per transition in a CmdBarrier() call.  The first call (earlyPhase ==
// true) should be made before any client-requested stalls or cache flushes are executed, the second call (earlyPhase ==
// false) should be done after.  This allows a reuse of the logic whether the decompress BLT can be pipelined or not.
//
// This function returns true if an expand BLT was required.  In that case, the caller should ensure the stalls and
// cache flushes are executed.
//
// pSyncReqs will be updated to reflect synchronization that must be performed after the BLT.
void Device::ExpandColor(
    GfxCmdBuffer*                 pCmdBuf,
    CmdStream*                    pCmdStream,
    const BarrierTransition&      transition,
    bool                          earlyPhase,
    SyncReqs*                     pSyncReqs,
    Developer::BarrierOperations* pOperations
    ) const
{
    PAL_ASSERT(transition.imageInfo.pImage != nullptr);

    const auto&        image                  = static_cast<const Pal::Image&>(*transition.imageInfo.pImage);
    const auto&        gfx6Image              = static_cast<const Image&>(*image.GetGfxImage());
    const SubresRange& inputRange             = transition.imageInfo.subresRange;
    bool               postExpandFlush        = false;
    uint32             blt[MaxImageMipLevels] = {};
    const uint32       allBltOperations       = GetColorBltPerSubres(pCmdBuf, blt, transition, earlyPhase);
    PAL_ASSERT(image.IsDepthStencil() == false);

    uint32* pCmdSpace = pCmdStream->ReserveCommands();
    // If any mip level needs a Dcc decompress or Fmask decompress, we'll need to do a pre decompress flush.
    if (TestAnyFlagSet(allBltOperations, ColorBlt::DccDecompress))
    {
        if (earlyPhase && WaEnableDccCacheFlushAndInvalidate())
        {
            pCmdSpace += m_cmdUtil.BuildEventWrite(CACHE_FLUSH_AND_INV_EVENT, pCmdSpace);

            pOperations->caches.flushCb = 1;
            pOperations->caches.invalCb = 1;
            pOperations->caches.flushDb = 1;
            pOperations->caches.invalDb = 1;
            pOperations->caches.flushCbMetadata = 1;
            pOperations->caches.invalCbMetadata = 1;
            pOperations->caches.flushDbMetadata = 1;
            pOperations->caches.invalDbMetadata = 1;
        }
    }
    else if (TestAnyFlagSet(allBltOperations, ColorBlt::FmaskDecompress))
    {
        if (earlyPhase)
        {
            // NOTE:
            // We need to do a full CacheFlushInv event before the FMask decompress.  We're
            // using the lightweight event for now, but if we see issues this should be changed to the timestamp
            // version which waits for completion.
            pCmdSpace += m_cmdUtil.BuildEventWrite(CACHE_FLUSH_AND_INV_EVENT, pCmdSpace);

            pOperations->caches.flushCb = 1;
            pOperations->caches.invalCb = 1;
            pOperations->caches.flushDb = 1;
            pOperations->caches.invalDb = 1;
            pOperations->caches.flushCbMetadata = 1;
            pOperations->caches.invalCbMetadata = 1;
            pOperations->caches.flushDbMetadata = 1;
            pOperations->caches.invalDbMetadata = 1;
        }
        else
        {
            pCmdSpace += m_cmdUtil.BuildEventWrite(FLUSH_AND_INV_CB_META, pCmdSpace);

            pOperations->caches.invalCbMetadata = 1;
            pOperations->caches.flushCbMetadata = 1;
        }
    }
    pCmdStream->CommitCommands(pCmdSpace);

    SubresRange  subresRange = inputRange;
    uint32       mip         = inputRange.startSubres.mipLevel;
    const uint32 lastMip     = mip + inputRange.numMips - 1;
    while (mip <= lastMip)
    {
        subresRange.startSubres.mipLevel = mip;
        subresRange.numMips = FindConsecutiveOps(blt, mip, lastMip); // Group the mips that have exact same flags set.

        // The "earlyPhase" for decompress BLTs is before any waits and/or cache flushes have been inserted.  It is safe
        // to perform a color expand in the early phase if the client reports there is dirty data in the CB caches.
        // This indicates:
        //
        //     1) There is no need to flush compressed data out of another cache or invalidate stale data in the CB
        //        caches before issuing the fixed-function DB expand:  the data is already in the right caches.
        //     2) There is no need to stall before beginning the decompress.  Data can only be dirty in one source cache
        //        at a time in a well-defined program, so we know the last output to this image was done with the CB.
        //
        // If this transition does not flush dirty data out of the CB caches, we delay the decompress until all client-
        // specified stalls and cache flushes have been executed (the late phase).  This situation should be rare,
        // occurring in cases like a clear to shader read transition without any rendering in between.
        if (earlyPhase == TestAnyFlagSet(transition.srcCacheMask, CoherColorTarget))
        {
            if (TestAnyFlagSet(blt[mip], ColorBlt::DccDecompress))
            {
                DccDecompress(pCmdBuf, pCmdStream, transition, gfx6Image, subresRange, pOperations);
                postExpandFlush   = true;
            }
            else if (TestAnyFlagSet(blt[mip], ColorBlt::FmaskDecompress))
            {
                FmaskDecompress(pCmdBuf, pCmdStream, transition, gfx6Image, subresRange, pOperations);
                postExpandFlush   = true;
            }
            else if (TestAnyFlagSet(blt[mip], ColorBlt::FastClearEliminate))
            {
                FastClearEliminate(pCmdBuf, pCmdStream, transition, gfx6Image, subresRange, pOperations);
                postExpandFlush = true;
            }
        }

        // Issue an MSAA color decompress, if necessary.  This BLT is always performed during the late phase, since it
        // is implied that an fmask decompress BLT would have to be executed first, occupying the early phase.
        if (TestAnyFlagSet(blt[mip], ColorBlt::MsaaColorDecompress))
        {
            MsaaDecompress(pCmdBuf, pCmdStream, transition, gfx6Image, subresRange, blt[mip], pOperations);
            postExpandFlush   = true;
        }

        mip += subresRange.numMips;
    }

    // If a CB decompress operation  was performed on the universal queue then we need to flush out some caches, etc.
    // Some decompress operations can be done on the compute queue...  for those, it is the compute function's
    // responsibility to ensure the necessary caches are flushed, etc.
    if ((pCmdBuf->GetEngineType() == EngineTypeUniversal) && postExpandFlush)
    {
        // Performing an expand in the late phase is not ideal for performance, as it indicates the decompress could not
        // be pipelined and likely resulted in a bubble.  If an app is hitting this alert too often, it may have an
        // impact on performance.
        PAL_ALERT(earlyPhase == false);

        // CB metadata caches can only be flushed with a pipelined VGT event, like CACHE_FLUSH_AND_INV.  In order to
        // ensure the cache flush finishes before continuing, we must wait on a timestamp.
        pSyncReqs->waitOnEopTs      = 1;
        pSyncReqs->cacheFlushAndInv = 1;

        // The decompression that was just executed was effectively a PAL-initiated draw that wrote to the image as a
        // CB destination.  In addition to flushing the data out of the CB cache, we need to invalidate any possible
        // read/write caches that need coherent reads of this image's data.  If the client was already rendering to
        // this image through the CB caches on its own (i.e., srcCacheMask includes CoherColorTarget), this shouldn't
        // result in any additional sync.
        //
        // Also, MSAA color decompress does some fmask fixup work with a compute shader.  The waitOnEopTs
        // requirement set for all CB BLTs will ensure the CS work completes, but we need to specifically request the
        // texture caches to be flushed.
        //
        // Note that we must always invalidate these caches if the client didn't give us any cache information.
        const bool noCacheFlags = ((transition.srcCacheMask == 0) && (transition.dstCacheMask == 0));

        if (TestAnyFlagSet(transition.dstCacheMask, CoherShader | CoherCopy | CoherResolve) || noCacheFlags)
        {
            pSyncReqs->cpCoherCntl.bits.TCL1_ACTION_ENA = 1;
            pSyncReqs->cpCoherCntl.bits.TC_ACTION_ENA   = 1;
        }
    }
}

// =====================================================================================================================
// For each mip level in subResRange, calculate the BLT operations needed during ExpandColor().
//
// The operations are stored in pBlt,  where each uint32 is the BLT operations for one mip level. If a BLT is needed for
// a mip level, the bit location marked by ColorBlt will be set.  The return value is a uint32, containing the
// ORs of all the mips,  will be used to decide whether we'll need a pre decompress flush or not later in the code.
uint32 Device::GetColorBltPerSubres(
    GfxCmdBuffer*            pCmdBuf,
    uint32*                  pBlt,
    const BarrierTransition& transition,
    bool                     earlyPhase
    ) const
{
    const auto& image            = static_cast<const Pal::Image&>(*transition.imageInfo.pImage);
    const auto& gfx6Image        = static_cast<const Image&>(*image.GetGfxImage());
    uint32      allBltOperations = 0;

    // Fast clear eliminates are only possible on universal queue command buffers and will be ignored on others.  This
    // should be okay because prior operations should be aware of this fact (based on layout), and prohibit us from
    // getting to a situation where one is needed but has not been performed yet.
    const bool fastClearEliminateSupported = (pCmdBuf->GetEngineType() == EngineTypeUniversal);

    const SubresRange& inputRange = transition.imageInfo.subresRange;
    SubresId           subRes     = inputRange.startSubres;

    for (uint32 mip = inputRange.startSubres.mipLevel;
                mip < inputRange.startSubres.mipLevel + inputRange.numMips;
                mip++)
    {
        subRes.mipLevel = mip;

        const SubResourceInfo*const pSubresInfo = image.SubresourceInfo(subRes);
        const ColorCompressionState oldState    =
            gfx6Image.LayoutToColorCompressionState(subRes, transition.imageInfo.oldLayout);
        const ColorCompressionState newState    =
            gfx6Image.LayoutToColorCompressionState(subRes, transition.imageInfo.newLayout);

        if ((oldState != ColorDecompressed) && (newState == ColorDecompressed))
        {
            if (gfx6Image.HasDccData())
            {
                pBlt[mip] |= (((oldState == ColorCompressed) || pSubresInfo->flags.supportMetaDataTexFetch) ?
                              ColorBlt::DccDecompress : 0);
            }
            else if (image.GetImageCreateInfo().samples > 1)
            {
                // Needed in preparation for the full MSAA color decompress, which is always handled in the late
                // phase, below.
                pBlt[mip] |= ((oldState == ColorCompressed) ? ColorBlt::FmaskDecompress : 0);
            }
            else
            {
                PAL_ASSERT(oldState == ColorCompressed);
                pBlt[mip] |= (fastClearEliminateSupported ? ColorBlt::FastClearEliminate : 0);
            }
        }
        else if ((oldState == ColorCompressed) && (newState == ColorFmaskDecompressed))
        {
            PAL_ASSERT(image.GetImageCreateInfo().samples > 1);
            if (pSubresInfo->flags.supportMetaDataTexFetch == false)
            {
                if (gfx6Image.HasDccData())
                {
                    // If the base pixel data is DCC compressed, but the image can't support metadata texture fetches,
                    // we need a DCC decompress.  The DCC decompress effectively executes an fmask decompress
                    // implicitly.
                    pBlt[mip] |= ColorBlt::DccDecompress;
                }
                else
                {
                    pBlt[mip] |= ColorBlt::FmaskDecompress;
                }
            }
            else
            {
                // if the image is TC compatible just need to do a fast clear eliminate
                if ((gfx6Image.HasDccData()) && (fastClearEliminateSupported == true))
                {
                    pBlt[mip] |= ColorBlt::FastClearEliminate;
                }
            }
        }
        else if ((oldState == ColorCompressed) && (newState == ColorCompressed))
        {
            // This case indicates that the layout capabilities changed, but the color image is able to remain in
            // the compressed state.  If the image is about to be read, we may need to perform a fast clear
            // eliminate BLT if the clear color is not texture compatible.  This BLT will end up being skipped on
            // the GPU side if the latest clear color was supported by the texture hardware (i.e., black or white).
            constexpr uint32 TcCompatReadFlags = LayoutShaderRead           |
                                                 LayoutShaderFmaskBasedRead |
                                                 LayoutCopySrc              |
                                                 LayoutResolveSrc;

            // Mantle use default compute engine to do resolve when create resource.  When newLayout.usages is
            // LayoutResolveSrc and no dcc but has cmask fast clear, need FCE when do resolve using compute engine.
            if (TestAnyFlagSet(transition.imageInfo.newLayout.usages, TcCompatReadFlags) &&
                ((gfx6Image.HasDccData() && pSubresInfo->flags.supportMetaDataTexFetch) ||
                (gfx6Image.HasCmaskData() && gfx6Image.GetCmask(subRes)->UseFastClear())))
            {
                pBlt[mip] |= (fastClearEliminateSupported ? ColorBlt::FastClearEliminate : 0);
            }
        }

        if ((earlyPhase == false)                    &&
            (image.GetImageCreateInfo().samples > 1) &&
            (oldState != ColorDecompressed)          &&
            (newState == ColorDecompressed))
        {
            pBlt[mip] |= ColorBlt::MsaaColorDecompress;
        }

        allBltOperations |= pBlt[mip];
    }
    return allBltOperations;
}

// =====================================================================================================================
// Writes decompressed pixel data to the base image and updates DCC to reflect the decompressed state.  Single sample
// or MSAA.  Causes a fast clear eliminate and fmask decompress implicitly.
void Device::DccDecompress(
    GfxCmdBuffer*                 pCmdBuf,
    CmdStream*                    pCmdStream,
    const BarrierTransition&      transition,
    const Image&                  gfx6Image,
    const SubresRange&            subresRange,
    Developer::BarrierOperations* pOperations
    ) const
{
    pOperations->layoutTransitions.dccDecompress = 1;

    DescribeBarrier(pCmdBuf,
                    pOperations,
                    &transition);

    LinearAllocatorAuto<VirtualLinearAllocator> allocator(pCmdBuf->Allocator(), false);
    IMsaaState* pMsaaState = BarrierMsaaState(this, pCmdBuf, &allocator, transition);

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 339 && PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 280
    RsrcProcMgr().DccDecompress(pCmdBuf,
                                pCmdStream,
                                gfx6Image,
                                pMsaaState,
                                &transition.imageInfo.samplePattern,
                                subresRange);
#else
    RsrcProcMgr().DccDecompress(pCmdBuf,
                                pCmdStream,
                                gfx6Image,
                                pMsaaState,
                                transition.imageInfo.pQuadSamplePattern,
                                subresRange);
#endif
    pMsaaState->Destroy();
    PAL_SAFE_FREE(pMsaaState, &allocator);
}

// =====================================================================================================================
// Leaves FMask-compressed pixel data in the base image, but puts FMask in a texture-readable state (CMask marks all
// blocks as having the max number of samples).  Causes a fast clear eliminate implicitly (if not using DCC).
void Device::FmaskDecompress(
    GfxCmdBuffer*                 pCmdBuf,
    CmdStream*                    pCmdStream,
    const BarrierTransition&      transition,
    const Image&                  gfx6Image,
    const SubresRange&            subresRange,
    Developer::BarrierOperations* pOperations
    ) const
{
    pOperations->layoutTransitions.fmaskDecompress = 1;

    DescribeBarrier(pCmdBuf,
                    pOperations,
                    &transition);

    LinearAllocatorAuto<VirtualLinearAllocator> allocator(pCmdBuf->Allocator(), false);
    IMsaaState* pMsaaState = BarrierMsaaState(this, pCmdBuf, &allocator, transition);

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 339 && PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 280
    RsrcProcMgr().FmaskDecompress(pCmdBuf,
                                  pCmdStream,
                                  gfx6Image,
                                  pMsaaState,
                                  &transition.imageInfo.samplePattern,
                                  subresRange);
#else
    RsrcProcMgr().FmaskDecompress(pCmdBuf,
                                  pCmdStream,
                                  gfx6Image,
                                  pMsaaState,
                                  transition.imageInfo.pQuadSamplePattern,
                                  subresRange);
#endif

    // On gfx6 hardware, the CB Fmask cache writes corrupted data if cache lines are flushed after their
    // context has been retired. To avoid this, we must flush the CB metadata caches after every Fmask
    // decompress.
    if (Parent()->ChipProperties().gfxLevel == GfxIpLevel::GfxIp6)
    {
        uint32* pCmdSpace = pCmdStream->ReserveCommands();
        pCmdSpace += m_cmdUtil.BuildEventWrite(FLUSH_AND_INV_CB_META, pCmdSpace);
        pCmdStream->CommitCommands(pCmdSpace);

        pOperations->caches.flushCbMetadata = 1;
        pOperations->caches.invalCbMetadata = 1;
    }

    pMsaaState->Destroy();
    PAL_SAFE_FREE(pMsaaState, &allocator);
}

// =====================================================================================================================
// Shader based decompress that writes every sample's color value to the base image. An FMask decompress must be
// executed before this BLT.
void Device::MsaaDecompress(
    GfxCmdBuffer*                 pCmdBuf,
    CmdStream*                    pCmdStream,
    const BarrierTransition&      transition,
    const Image&                  gfx6Image,
    const SubresRange&            subresRange,
    uint32                        blt,
    Developer::BarrierOperations* pOperations
    ) const
{
    // Check if the fmask decompress or DCC decompress was already executed during this phase.  If so, we need to
    // wait for those to finish and flush everything out of the CB caches first.
    if (TestAnyFlagSet(blt, ColorBlt::FmaskDecompress | ColorBlt::DccDecompress))
    {
        // This must execute on the universal queue.
        PAL_ASSERT(pCmdBuf->GetEngineType() == EngineTypeUniversal);

        uint32* pCmdSpace = pCmdStream->ReserveCommands();
        pCmdSpace += m_cmdUtil.BuildWaitOnEopEvent(CACHE_FLUSH_AND_INV_TS_EVENT,
                                                   pCmdBuf->TimestampGpuVirtAddr(),
                                                   pCmdSpace);
        pCmdStream->CommitCommands(pCmdSpace);
    }

    pOperations->layoutTransitions.fmaskColorExpand = 1;
    DescribeBarrier(pCmdBuf,
                    pOperations,
                    &transition);

    RsrcProcMgr().FmaskColorExpand(pCmdBuf, gfx6Image, subresRange);
}

// =====================================================================================================================
// Writes the last clear color values to the base image for any pixel blocks that are marked as fast cleared in CMask
// or DCC.  Single sample or MSAA.
void Device::FastClearEliminate(
    GfxCmdBuffer*                 pCmdBuf,
    CmdStream*                    pCmdStream,
    const BarrierTransition&      transition,
    const Image&                  gfx6Image,
    const SubresRange&            subresRange,
    Developer::BarrierOperations* pOperations
    ) const
{
    pOperations->layoutTransitions.fastClearEliminate = 1;
    DescribeBarrier(pCmdBuf,
                    pOperations,
                    &transition);

    LinearAllocatorAuto<VirtualLinearAllocator> allocator(pCmdBuf->Allocator(), false);
    IMsaaState* pMsaaState = BarrierMsaaState(this, pCmdBuf, &allocator, transition);
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 339 && PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 280
    RsrcProcMgr().FastClearEliminate(pCmdBuf,
                                     pCmdStream,
                                     gfx6Image,
                                     pMsaaState,
                                     &transition.imageInfo.samplePattern,
                                     subresRange);
#else
    RsrcProcMgr().FastClearEliminate(pCmdBuf,
                                     pCmdStream,
                                     gfx6Image,
                                     pMsaaState,
                                     transition.imageInfo.pQuadSamplePattern,
                                     subresRange);
#endif
    pMsaaState->Destroy();
    PAL_SAFE_FREE(pMsaaState, &allocator);
}

// =====================================================================================================================
// Examines the specified sync reqs, and the corresponding hardware commands to satisfy the requirements.
void Device::IssueSyncs(
    GfxCmdBuffer*                 pCmdBuf,
    CmdStream*                    pCmdStream,
    SyncReqs                      syncReqs,
    HwPipePoint                   waitPoint,
    gpusize                       rangeStartAddr,
    gpusize                       rangeSize,
    Developer::BarrierOperations* pOperations
    ) const
{
    const bool isUniversal = (pCmdBuf->GetEngineType() == EngineTypeUniversal);
    uint32*    pCmdSpace   = pCmdStream->ReserveCommands();

    FillCacheOperations(syncReqs, pOperations);

    if (syncReqs.waitOnEopTs)
    {
        // Issue a pipelined event that will write a timestamp value to GPU memory when finished. Then, stall the CP ME
        // until that timestamp is seen written to the GPU memory. This is a very heavyweight sync, and ensures all
        // previous graphics and compute work has completed.
        //
        // We will also flush the CB and DB caches (when executed on the universal queue) if it was requested.
        VGT_EVENT_TYPE eopEvent;

        if (syncReqs.cacheFlushAndInv)
        {
            eopEvent = CACHE_FLUSH_AND_INV_TS_EVENT;
        }
        else
        {
            eopEvent = BOTTOM_OF_PIPE_TS;
        }

        pOperations->pipelineStalls.waitOnEopTsBottomOfPipe = 1;
        pCmdSpace += m_cmdUtil.BuildWaitOnGenericEopEvent(eopEvent,
                                                          pCmdBuf->TimestampGpuVirtAddr(),
                                                          isUniversal == false,
                                                          pCmdSpace);
        pCmdBuf->SetPrevCmdBufInactive();

        // WriteWaitOnEopEvent waits in the ME, if the waitPoint needs to stall at the PFP request a PFP/ME sync.
        syncReqs.pfpSyncMe = (waitPoint == HwPipeTop);

        // The previous sync has already ensured that the graphics contexts are idle and all CS waves have completed.
        syncReqs.cpCoherCntl.u32All &= ~CpCoherCntlStallMask;

        if (syncReqs.cacheFlushAndInv)
        {
            // The previous sync has already ensured that the CB/DB caches have been flushed/invalidated.
            syncReqs.cpCoherCntl.bits.CB_ACTION_ENA = 0;
            syncReqs.cpCoherCntl.bits.DB_ACTION_ENA = 0;
        }
    }
    else
    {
        // If the address range covers from 0 to all Fs, and any of the BASE_ENA bits in the CP_COHER_CNTL value are
        // set, the SURFACE_SYNC issued at the end of this function is guaranteed to idle all graphics contexts.  Based
        // on that knowledge, some other commands may be skipped.
        if (isUniversal &&
            ((rangeStartAddr != FullSyncBaseAddr) ||
             (rangeSize != FullSyncSize)  ||
             (TestAnyFlagSet(syncReqs.cpCoherCntl.u32All, CpCoherCntlStallMask) == false)))
        {
            if (syncReqs.vsPartialFlush)
            {
                // Waits in the CP ME for all previously issued VS waves to complete.
                pCmdSpace += m_cmdUtil.BuildEventWrite(VS_PARTIAL_FLUSH, pCmdSpace);
                pOperations->pipelineStalls.vsPartialFlush = 1;
            }

            if (syncReqs.psPartialFlush)
            {
                // Waits in the CP ME for all previously issued PS waves to complete.
                pCmdSpace += m_cmdUtil.BuildEventWrite(PS_PARTIAL_FLUSH, pCmdSpace);
                pOperations->pipelineStalls.psPartialFlush = 1;
            }
        }

        if (syncReqs.csPartialFlush)
        {
            // Waits in the CP ME for all previously issued CS waves to complete.
            pCmdSpace += m_cmdUtil.BuildEventWrite(CS_PARTIAL_FLUSH, pCmdSpace);
            pOperations->pipelineStalls.csPartialFlush = 1;
        }
    }

    if (syncReqs.syncCpDma)
    {
        // Stalls the CP ME until the CP's DMA engine has finished all async CP_DMA/DMA_DATA commands. This needs to
        // go before the call to BuildGenericSync so that the results of CP blts are flushed properly. Also note that
        // DMA packets are the only way to wait for DMA work, we can't use something like a bottom-of-pipe timestamp.
        pCmdSpace += m_cmdUtil.BuildWaitDmaData(pCmdSpace);
        pOperations->pipelineStalls.syncCpDma = 1;
    }

    if (syncReqs.cpCoherCntl.u32All != 0)
    {
        const uint32 syncPoint = (waitPoint == HwPipeTop) ? SURFACE_SYNC_ENGINE_PFP : SURFACE_SYNC_ENGINE_ME;

        // Issue accumulated SURFACE_SYNC or ACQUIRE_MEM command on the specified memory range.
        pCmdSpace += m_cmdUtil.BuildGenericSync(syncReqs.cpCoherCntl,
                                                syncPoint,
                                                rangeStartAddr,
                                                rangeSize,
                                                isUniversal == false,
                                                pCmdSpace);
    }

    if (syncReqs.pfpSyncMe && isUniversal)
    {
        // Stalls the CP PFP until the ME has processed all previous commands.  Useful in cases where the ME is waiting
        // on some condition, but the PFP needs to stall execution until the condition is satisfied.  This must go last
        // otherwise the PFP could resume execution before the ME is done with all of its waits.
        pCmdSpace += m_cmdUtil.BuildPfpSyncMe(pCmdSpace);
        pOperations->pipelineStalls.pfpSyncMe = 1;
    }

    pCmdStream->CommitCommands(pCmdSpace);

    // Clear up xxxBltActive flags
    if (syncReqs.waitOnEopTs || TestAnyFlagSet(syncReqs.cpCoherCntl.u32All, CpCoherCntlStallMask))
    {
        pCmdBuf->SetGfxCmdBufGfxBltState(false);
    }
    if ((pCmdBuf->GetGfxCmdBufState().gfxBltActive == false) &&
        ((syncReqs.cacheFlushAndInv != 0) && (syncReqs.waitOnEopTs != 0)))
    {
        pCmdBuf->SetGfxCmdBufGfxBltWriteCacheState(false);
    }

    if (syncReqs.waitOnEopTs || syncReqs.csPartialFlush)
    {
        pCmdBuf->SetGfxCmdBufCsBltState(false);
    }
    if ((pCmdBuf->GetGfxCmdBufState().csBltActive == false) && (syncReqs.cpCoherCntl.bits.TC_ACTION_ENA != 0))
    {
        pCmdBuf->SetGfxCmdBufCsBltWriteCacheState(false);
    }

    if (syncReqs.syncCpDma)
    {
        pCmdBuf->SetGfxCmdBufCpBltState(false);
    }
    if ((pCmdBuf->GetGfxCmdBufState().cpBltActive == false) && (syncReqs.cpCoherCntl.bits.TC_ACTION_ENA != 0))
    {
        pCmdBuf->SetGfxCmdBufCpBltWriteCacheState(false);
        pCmdBuf->SetGfxCmdBufCpMemoryWriteL2CacheStaleState(false);
    }
}

// =====================================================================================================================
void Device::FillCacheOperations(
    const SyncReqs&               syncReqs,
    Developer::BarrierOperations* pOperations
    ) const
{
    const uint32 cpCoherCntl = syncReqs.cpCoherCntl.u32All;
    const bool   cbActionSet = TestAnyFlagSet(cpCoherCntl, CP_COHER_CNTL__CB_ACTION_ENA_MASK);
    const bool   dbActionSet = TestAnyFlagSet(cpCoherCntl, CP_COHER_CNTL__DB_ACTION_ENA_MASK);

    pOperations->caches.invalTcp         |= TestAnyFlagSet(cpCoherCntl, CP_COHER_CNTL__TCL1_ACTION_ENA_MASK);
    pOperations->caches.invalSqI$        |= TestAnyFlagSet(cpCoherCntl, CP_COHER_CNTL__SH_ICACHE_ACTION_ENA_MASK);
    pOperations->caches.invalSqK$        |= TestAnyFlagSet(cpCoherCntl, CP_COHER_CNTL__SH_KCACHE_ACTION_ENA_MASK);
    pOperations->caches.flushTcc         |= TestAnyFlagSet(cpCoherCntl, CP_COHER_CNTL__TC_ACTION_ENA_MASK);
    pOperations->caches.invalTcc         |= TestAnyFlagSet(cpCoherCntl, CP_COHER_CNTL__TC_ACTION_ENA_MASK);
    pOperations->caches.flushCb          |= syncReqs.cacheFlushAndInv | cbActionSet;
    pOperations->caches.invalCb          |= syncReqs.cacheFlushAndInv | cbActionSet;
    pOperations->caches.flushDb          |= syncReqs.cacheFlushAndInv | dbActionSet;
    pOperations->caches.invalDb          |= syncReqs.cacheFlushAndInv | dbActionSet;
    pOperations->caches.flushCbMetadata  |= syncReqs.cacheFlushAndInv;
    pOperations->caches.invalCbMetadata  |= syncReqs.cacheFlushAndInv;
    pOperations->caches.flushDbMetadata  |= syncReqs.cacheFlushAndInv | dbActionSet;
    pOperations->caches.invalDbMetadata  |= syncReqs.cacheFlushAndInv | dbActionSet;
}

// =====================================================================================================================
// Inserts a barrier in the current command stream that can stall GPU execution, flush/invalidate caches, or decompress
// images before further, dependent work can continue in this command buffer.
//
// The barrier implementation is executed in 3 phases:
//
//     1. Early image layout transitions: Perform any layout transition (i.e., decompress BLT) that is pipelined with
//        previous work such that it can be executed before the stall phase.  For example, on a transition from
//        rendering to a depth target to reading from that image as a texture, a stall may not be necessary since both
//        the old usage and decompress are executed by the DB and pipelined.
//     2. Stalls and global cache flush management:
//            - Examine wait point and stall points to determine globally require operations (graphics idle,
//              ps_partial_flush, etc.).
//            - Examine all cache transitions to determine which global cache flush/invalidate commands are required.
//              Note that this includes all caches but DB, the only GPU cache with some range checking ability.
//            - Issue any requested range-checked target stalls or GPU event stalls.
//            - Issue the formulated "global" sync commands.
//     3. Late image transitions:
//            - Issue metadata initialization BLTs.
//            - Issue range-checked DB cache flushes.
//            - Issue any decompress BLTs that couldn't be performed in phase 1.
void Device::Barrier(
    GfxCmdBuffer*      pCmdBuf,
    CmdStream*         pCmdStream,
    const BarrierInfo& barrier
    ) const
{
    SyncReqs globalSyncReqs = {};
    Developer::BarrierOperations barrierOps = {};

    // -----------------------------------------------------------------------------------------------------------------
    // -- Early image layout transitions.
    // -----------------------------------------------------------------------------------------------------------------
    if (barrier.flags.splitBarrierLatePhase == 0)
    {
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 360
        DescribeBarrierStart(pCmdBuf, barrier.reason);
#else
        DescribeBarrierStart(pCmdBuf);
#endif

        for (uint32 i = 0; i < barrier.transitionCount; i++)
        {
            const auto& imageInfo = barrier.pTransitions[i].imageInfo;

            if (imageInfo.pImage != nullptr)
            {
                // At least one usage must be specified for the old and new layouts.
                PAL_ASSERT((imageInfo.oldLayout.usages != 0) && (imageInfo.newLayout.usages != 0));

                // With the exception of a transition out of the uninitialized state, at least one queue type must be
                // valid for every layout.

                PAL_ASSERT(((imageInfo.oldLayout.usages == LayoutUninitializedTarget) ||
                            (imageInfo.oldLayout.engines != 0)) &&
                           (imageInfo.newLayout.engines != 0));

                if ((TestAnyFlagSet(imageInfo.oldLayout.usages, LayoutUninitializedTarget) == false) &&
                    (TestAnyFlagSet(imageInfo.newLayout.usages, LayoutUninitializedTarget) == false))
                {
                    const auto& image = static_cast<const Pal::Image&>(*imageInfo.pImage);

                    if (image.IsDepthStencil())
                    {
                        TransitionDepthStencil(pCmdBuf, barrier.pTransitions[i], true, &globalSyncReqs, &barrierOps);
                    }
                    else
                    {
                        ExpandColor(pCmdBuf, pCmdStream, barrier.pTransitions[i], true, &globalSyncReqs, &barrierOps);
                    }
                }
            }
        }
    }

    // -----------------------------------------------------------------------------------------------------------------
    // -- Stalls and global cache management.
    // -----------------------------------------------------------------------------------------------------------------
    GfxCmdBufferState cmdBufState = pCmdBuf->GetGfxCmdBufState();

    // Determine sync requirements for global pipeline waits.
    for (uint32 i = 0; i < barrier.pipePointWaitCount; i++)
    {
        HwPipePoint pipePoint = barrier.pPipePoints[i];

        // CP blts use asynchronous CP DMA operations which are executed in parallel to our usual pipeline. This means
        // that we must sync CP DMA in any case that might expect the results of the CP blt to be available. Luckily
        // PAL only uses CP blts to optimize blt operations so we only need to sync if a pipe point is HwPipePostBlt
        // or later.
        if (cmdBufState.cpBltActive && (pipePoint >= HwPipePostBlt))
        {
            globalSyncReqs.syncCpDma = 1;
        }

        if (pipePoint == HwPipePostBlt)
        {
            // HwPipePostBlt barrier optimization
            pipePoint = pCmdBuf->OptimizeHwPipePostBlit();
        }

        if (pipePoint > barrier.waitPoint)
        {
            switch (pipePoint)
            {
            case HwPipePostIndexFetch:
                PAL_ASSERT(barrier.waitPoint == HwPipeTop);
                globalSyncReqs.pfpSyncMe = 1;
                break;
            case HwPipePreRasterization:
                globalSyncReqs.vsPartialFlush = 1;
                globalSyncReqs.pfpSyncMe = (barrier.waitPoint == HwPipeTop);
                break;
            case HwPipePostPs:
                globalSyncReqs.vsPartialFlush = 1;
                globalSyncReqs.psPartialFlush = 1;
                globalSyncReqs.pfpSyncMe = (barrier.waitPoint == HwPipeTop);
                break;
            case HwPipePostCs:
                globalSyncReqs.csPartialFlush = 1;
                globalSyncReqs.pfpSyncMe = (barrier.waitPoint == HwPipeTop);
                break;
            case HwPipeBottom:
                globalSyncReqs.waitOnEopTs = 1;
                break;
            case HwPipeTop:
            default:
                PAL_NEVER_CALLED();
                break;
            }
        }
    }

    // Determine sync requirements for global cache flushes and invalidations.
    for (uint32 i = 0; i < barrier.transitionCount; i++)
    {
        const auto& transition = barrier.pTransitions[i];

        uint32 srcCacheMask = transition.srcCacheMask;

        // There are various srcCache BLTs (Copy, Clear, and Resolve) which we can further optimize if we know which
        // write caches have been dirtied:
        // - If a graphics BLT occurred, alias these srcCaches to CoherColorTarget.
        // - If a compute BLT occurred, alias these srcCaches to CoherShader.
        // - If a CP L2 BLT occured, alias these srcCaches to CoherShader (this isn't good but we have no CoherL2).
        // - If a CP direct-to-memory write occured, alias these srcCaches to CoherMemory.
        // Clear the original srcCaches from the srcCache mask for the rest of this scope.
        if (TestAnyFlagSet(srcCacheMask, CoherCopy | CoherClear | CoherResolve))
        {
            srcCacheMask &= ~(CoherCopy | CoherClear | CoherResolve);

            srcCacheMask |= cmdBufState.gfxWriteCachesDirty       ? CoherColorTarget : 0;
            srcCacheMask |= cmdBufState.csWriteCachesDirty        ? CoherShader      : 0;
            srcCacheMask |= cmdBufState.cpWriteCachesDirty        ? CoherShader      : 0;
            srcCacheMask |= cmdBufState.cpMemoryWriteL2CacheStale ? CoherMemory      : 0;
        }

        // alwaysL2Mask is a mask of usages that always read/write through the L2 cache.
        uint32 alwaysL2Mask = CoherShader | CoherStreamOut | CoherQueueAtomic | CoherCeDump;

        if (Parent()->ChipProperties().gfxLevel >= GfxIpLevel::GfxIp8)
        {
            alwaysL2Mask |= CoherCeLoad;
        }

        // MaybeL2Mask is a mask of usages that may or may not read/write through the L2 cache.
        const uint32 MaybeL2Mask = alwaysL2Mask | CoherCopy | CoherResolve | CoherClear;

        // Flush/invalidate L2 if:
        //     - Flush case:      Prior output might have been through L2 and upcoming reads/writes might not be
        //                        through L2.
        //     - Invalidate case: Prior output might not have been through L2 and upcoming reads/writes might be
        //                        through L2.
        if ((TestAnyFlagSet(srcCacheMask, MaybeL2Mask) &&
             TestAnyFlagSet(transition.dstCacheMask, ~alwaysL2Mask)) ||
            (TestAnyFlagSet(srcCacheMask, ~alwaysL2Mask) &&
             TestAnyFlagSet(transition.dstCacheMask, MaybeL2Mask)))
        {
            globalSyncReqs.cpCoherCntl.bits.TC_ACTION_ENA = 1;
        }

        constexpr uint32 MaybeL1ShaderMask = CoherShader | CoherStreamOut | CoherCopy | CoherResolve | CoherClear;

        // Invalidate L1 shader caches if the previous output may have done shader writes, since there is no coherence
        // between different CUs' TCP (vector L1) caches.  Invalidate TCP and SQ-K cache (scalar read cache) if this
        // barrier is forcing shader read coherency.
        if (TestAnyFlagSet(srcCacheMask, MaybeL1ShaderMask) ||
            TestAnyFlagSet(transition.dstCacheMask, MaybeL1ShaderMask))
        {
            globalSyncReqs.cpCoherCntl.bits.TCL1_ACTION_ENA      = 1;
            globalSyncReqs.cpCoherCntl.bits.SH_KCACHE_ACTION_ENA = 1;
        }

        if (TestAnyFlagSet(srcCacheMask, CoherColorTarget) &&
            (TestAnyFlagSet(srcCacheMask, ~CoherColorTarget) ||
             TestAnyFlagSet(transition.dstCacheMask, ~CoherColorTarget)))
        {
            // CB metadata caches can only be flushed with a pipelined VGT event, like CACHE_FLUSH_AND_INV.  In order to
            // ensure the cache flush finishes before continuing, we must wait on a timestamp.  Catch those cases early
            // here so that we can perform it along with the rest of the stalls so that we might hide the bubble this
            // will introduce.
            globalSyncReqs.waitOnEopTs      = 1;
            globalSyncReqs.cacheFlushAndInv = 1;
        }
    }

    // Check conditions that end up requiring a stall for all GPU work to complete.  The cases are:
    //     - A pipelined wait has been requested.
    //     - Any DEST_BASE_ENA bit is set in the global surface sync request, waiting for all gfx contexts to be idle.
    //     - If a CS_PARTIAL_FLUSH AND either VS/PS_PARTIAL_FLUSH are requested, we have to idle the whole pipe to
    //       ensure both sets of potentially parallel work have completed.
    const bool bottomOfPipeStall = (globalSyncReqs.waitOnEopTs ||
                                    TestAnyFlagSet(globalSyncReqs.cpCoherCntl.u32All, CpCoherCntlStallMask) ||
                                    (globalSyncReqs.csPartialFlush &&
                                     (globalSyncReqs.vsPartialFlush || globalSyncReqs.psPartialFlush)));

    if (barrier.pSplitBarrierGpuEvent != nullptr)
    {
        if (barrier.flags.splitBarrierEarlyPhase)
        {
            // This is the early phase of a split barrier.  We've already performed any early phase decompresses, etc.
            // that were possible.

            // Reset the split barrier event to get it in a known state.
            pCmdBuf->CmdResetEvent(*barrier.pSplitBarrierGpuEvent, HwPipeTop);

            // If this barrier requires CB/DB caches to be flushed, enqueue a pipeline event to do that now.  In
            // particular, note that CB/DB flushes performed by a surface sync with a regular barrier is converted to
            // a pipelined event in a split barrier.
            if (globalSyncReqs.cacheFlushAndInv               ||
                globalSyncReqs.cpCoherCntl.bits.CB_ACTION_ENA ||
                globalSyncReqs.cpCoherCntl.bits.DB_ACTION_ENA)
            {
                uint32* pCmdSpace = pCmdStream->ReserveCommands();
                pCmdSpace += m_cmdUtil.BuildEventWrite(CACHE_FLUSH_AND_INV_EVENT, pCmdSpace);
                pCmdStream->CommitCommands(pCmdSpace);

                barrierOps.caches.flushCb = globalSyncReqs.cpCoherCntl.bits.CB_ACTION_ENA;
                barrierOps.caches.flushDb = globalSyncReqs.cpCoherCntl.bits.DB_ACTION_ENA;
            }

            // Determine the "release point" for the barrier.  We want to choose the earliest point in the pipe that
            // ensures the early phase barrier is complete.
            HwPipePoint releasePoint = HwPipeTop;

            if (bottomOfPipeStall)
            {
                releasePoint = HwPipeBottom;
            }
            else if (globalSyncReqs.csPartialFlush)
            {
                PAL_ASSERT((globalSyncReqs.vsPartialFlush == 0) && (globalSyncReqs.psPartialFlush == 0));
                releasePoint = HwPipePostCs;
            }
            else if (globalSyncReqs.psPartialFlush)
            {
                PAL_ASSERT(globalSyncReqs.csPartialFlush == 0);
                releasePoint = HwPipePostPs;
            }
            else if (globalSyncReqs.vsPartialFlush)
            {
                PAL_ASSERT((globalSyncReqs.csPartialFlush == 0) && (globalSyncReqs.psPartialFlush == 0));
                releasePoint = HwPipePreRasterization;
            }

            // Set event at the computed pipeline point.
            pCmdBuf->CmdSetEvent(*barrier.pSplitBarrierGpuEvent, releasePoint);
        }
        else if (barrier.flags.splitBarrierLatePhase)
        {
            // Wait for the event set during the early phase to be set.
            const GpuEvent* pGpuEvent = static_cast<const GpuEvent*>(barrier.pSplitBarrierGpuEvent);

            uint32* pCmdSpace = pCmdStream->ReserveCommands();
            pCmdSpace += m_cmdUtil.BuildWaitRegMem(WAIT_REG_MEM_SPACE_MEMORY,
                                                   WAIT_REG_MEM_FUNC_EQUAL,
                                                   WAIT_REG_MEM_ENGINE_PFP,
                                                   pGpuEvent->GetBoundGpuMemory().GpuVirtAddr(),
                                                   GpuEvent::SetValue,
                                                   0xFFFFFFFF,
                                                   false,
                                                   pCmdSpace);
            pCmdStream->CommitCommands(pCmdSpace);

            if (globalSyncReqs.waitOnEopTs)
            {
                pCmdBuf->SetPrevCmdBufInactive();
            }

            // Clear any global sync requirements that we know have been satisfied by the wait on pSplitBarrierGpuEvent.
            globalSyncReqs.waitOnEopTs                     = 0;
            globalSyncReqs.cacheFlushAndInv                = 0;
            globalSyncReqs.vsPartialFlush                  = 0;
            globalSyncReqs.psPartialFlush                  = 0;
            globalSyncReqs.csPartialFlush                  = 0;
            globalSyncReqs.pfpSyncMe                       = 0;
            globalSyncReqs.cpCoherCntl.u32All             &= ~CpCoherCntlStallMask;
            globalSyncReqs.cpCoherCntl.bits.CB_ACTION_ENA  = 0;
            globalSyncReqs.cpCoherCntl.bits.DB_ACTION_ENA  = 0;

            // Some globalSyncReqs bits may still be set.  These will allow any late cache flush/invalidations that have
            // to be performed with SURFACE_SYNC to be executed during the IssueSyncs() call, below.
        }
    }

    if (barrier.flags.splitBarrierEarlyPhase == 0)
    {
        // Skip the range-checked stalls if we know a global stall already ensured all graphics contexts are idle.
        if (bottomOfPipeStall == false)
        {
            // Issue any range-checked target stalls.  This will wait for any active graphics contexts that reference
            // the VA range of the specified image to be idle.
            for (uint32 i = 0; i < barrier.rangeCheckedTargetWaitCount; i++)
            {
                const Pal::Image* pImage = static_cast<const Pal::Image*>(barrier.ppTargets[i]);

                SyncReqs targetStallSyncReqs = { };
                targetStallSyncReqs.cpCoherCntl.u32All = CpCoherCntlStallMask;

                if (pImage != nullptr)
                {
                    IssueSyncs(pCmdBuf,
                               pCmdStream,
                               targetStallSyncReqs,
                               barrier.waitPoint,
                               pImage->GetGpuVirtualAddr(),
                               pImage->GetGpuMemSize(),
                               &barrierOps);
                }
                else
                {
                    IssueSyncs(pCmdBuf,
                               pCmdStream,
                               targetStallSyncReqs,
                               barrier.waitPoint,
                               FullSyncBaseAddr,
                               FullSyncSize,
                               &barrierOps);
                    // Ignore the rest since we are syncing on the full range.
                    break;
                }
            }
        }

        // Wait on all GPU events specified in barrier.ppGpuEvents to be in the "set" state.  Note that this is done
        // even if other sync guarantees an idle pipeline since these events could be signaled from a different queue or
        // CPU.
        for (uint32 i = 0; i < barrier.gpuEventWaitCount; i++)
        {
            const GpuEvent* pGpuEvent  = static_cast<const GpuEvent*>(barrier.ppGpuEvents[i]);
            const uint32    waitEngine = (barrier.waitPoint == HwPipeTop) ? WAIT_REG_MEM_ENGINE_PFP :
                                                                            WAIT_REG_MEM_ENGINE_ME;

            uint32* pCmdSpace = pCmdStream->ReserveCommands();
            pCmdSpace += m_cmdUtil.BuildWaitRegMem(WAIT_REG_MEM_SPACE_MEMORY,
                                                   WAIT_REG_MEM_FUNC_EQUAL,
                                                   waitEngine,
                                                   pGpuEvent->GetBoundGpuMemory().GpuVirtAddr(),
                                                   GpuEvent::SetValue,
                                                   0xFFFFFFFF,
                                                   false,
                                                   pCmdSpace);
            pCmdStream->CommitCommands(pCmdSpace);
        }

        IssueSyncs(pCmdBuf, pCmdStream, globalSyncReqs, barrier.waitPoint, FullSyncBaseAddr, FullSyncSize, &barrierOps);

        // -------------------------------------------------------------------------------------------------------------
        // -- Perform late image transitions (layout changes and range-checked DB cache flushes).
        // -------------------------------------------------------------------------------------------------------------
        SyncReqs initSyncReqs = { };

        for (uint32 i = 0; i < barrier.transitionCount; i++)
        {
            const auto& imageInfo = barrier.pTransitions[i].imageInfo;

            if (imageInfo.pImage != nullptr)
            {
                if (TestAnyFlagSet(imageInfo.oldLayout.usages, LayoutUninitializedTarget))
                {
                    // If the LayoutUninitializedTarget usage is set, no other usages should be set.
                    PAL_ASSERT(TestAnyFlagSet(imageInfo.oldLayout.usages, ~LayoutUninitializedTarget) == false);

                    const auto& image       = static_cast<const Pal::Image&>(*imageInfo.pImage);
                    const auto& gfx6Image   = static_cast<const Image&>(*image.GetGfxImage());
                    const auto& subresRange = imageInfo.subresRange;

#if PAL_ENABLE_PRINTS_ASSERTS
                    const auto& engineProps  = Parent()->EngineProperties().perEngine[pCmdBuf->GetEngineType()];
                    const auto& createInfo   = image.GetImageCreateInfo();
                    const bool  isWholeImage = image.IsFullSubResRange(subresRange);

                    // This queue must support this barrier transition.
                    PAL_ASSERT(engineProps.flags.supportsImageInitBarrier == 1);

                    // By default, the entire image must be initialized in one go. Per-subres support can be requested
                    // using an image flag as long as the queue supports it.
                    PAL_ASSERT(isWholeImage || ((engineProps.flags.supportsImageInitPerSubresource == 1) &&
                                                (createInfo.flags.perSubresInit == 1)));
#endif

                    if (gfx6Image.HasColorMetaData() || gfx6Image.HasHtileData())
                    {
                        barrierOps.layoutTransitions.initMaskRam = 1;
                        DescribeBarrier(pCmdBuf,
                                        &barrierOps,
                                        &barrier.pTransitions[i]);

                        RsrcProcMgr().InitMaskRam(pCmdBuf, pCmdStream, gfx6Image, subresRange, &initSyncReqs);
                    }
                }
                else if (TestAnyFlagSet(imageInfo.newLayout.usages, LayoutUninitializedTarget))
                {
                    // If the LayoutUninitializedTarget usage is set, no other usages should be set.
                    PAL_ASSERT(TestAnyFlagSet(imageInfo.newLayout.usages, ~LayoutUninitializedTarget) == false);

                    // We do no decompresses, expands, or any other kind of blt in this case.
                }
            }
        }

        IssueSyncs(pCmdBuf, pCmdStream, initSyncReqs, barrier.waitPoint, FullSyncBaseAddr, FullSyncSize, &barrierOps);

        for (uint32 i = 0; i < barrier.transitionCount; i++)
        {
            const auto& transition = barrier.pTransitions[i];
            uint32 srcCacheMask = transition.srcCacheMask;

            if (transition.imageInfo.pImage != nullptr)
            {
                if ((TestAnyFlagSet(transition.imageInfo.oldLayout.usages, LayoutUninitializedTarget) == false) &&
                    (TestAnyFlagSet(transition.imageInfo.newLayout.usages, LayoutUninitializedTarget) == false))
                {
                    const auto& image = static_cast<const Pal::Image&>(*transition.imageInfo.pImage);

                    SyncReqs imageSyncReqs = { };

                    if (image.IsDepthStencil())
                    {
                        // Issue a late-phase DB decompress, if necessary.
                        TransitionDepthStencil(pCmdBuf, transition, false, &imageSyncReqs, &barrierOps);

                        // There are two various srcCache Clear which we can further optimize if we know which
                        // write caches have been dirtied:
                        // - If a graphics clear occurred, alias these srcCaches to CoherDepthStencilTarget.
                        // - If a compute clear occurred, alias these srcCaches to CoherShader.
                        // Clear the original srcCaches from the srcCache mask for the rest of this scope.
                        if (TestAnyFlagSet(srcCacheMask, CoherClear))
                        {
                            srcCacheMask &= ~CoherClear;

                            srcCacheMask |= cmdBufState.gfxWriteCachesDirty ? CoherDepthStencilTarget : 0;
                            srcCacheMask |= cmdBufState.csWriteCachesDirty  ? CoherShader             : 0;
                        }

                        if (TestAnyFlagSet(srcCacheMask, CoherDepthStencilTarget) &&
                            TestAnyFlagSet(transition.dstCacheMask, ~CoherDepthStencilTarget))
                        {
                            // Issue surface sync stalls on depth/stencil surface writes and flush DB caches
                            imageSyncReqs.cpCoherCntl.bits.DB_DEST_BASE_ENA = 1;
                            imageSyncReqs.cpCoherCntl.bits.DEST_BASE_0_ENA  = 1;
                            imageSyncReqs.cpCoherCntl.bits.DB_ACTION_ENA    = 1;
                        }
                    }
                    else
                    {
                        ExpandColor(pCmdBuf, pCmdStream, transition, false, &imageSyncReqs, &barrierOps);
                    }

                    IssueSyncs(pCmdBuf,
                               pCmdStream,
                               imageSyncReqs,
                               barrier.waitPoint,
                               image.GetGpuVirtualAddr(),
                               image.GetGpuMemSize(),
                               &barrierOps);
                }
            }
        }
        DescribeBarrierEnd(pCmdBuf, &barrierOps);
    }
}

// =====================================================================================================================
// Call back to above layers before starting the barrier execution.
void Device::DescribeBarrierStart(
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 360
    GfxCmdBuffer* pCmdBuf,
    uint32        reason
#else
    GfxCmdBuffer* pCmdBuf
#endif
    ) const
{
    Developer::BarrierData data = {};

    data.pCmdBuffer = pCmdBuf;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 360
    // Make sure we have an acceptable barrier reason.
    PAL_ASSERT_MSG(reason != Developer::BarrierReasonInvalid,
                   "Invalid barrier reason codes are not allowed!");

    data.reason = reason;
#endif

    m_pParent->DeveloperCb(Developer::CallbackType::BarrierBegin, &data);
}

// =====================================================================================================================
// Callback to above layers with summary information at end of barrier execution.
void Device::DescribeBarrierEnd(
    GfxCmdBuffer*                 pCmdBuf,
    Developer::BarrierOperations* pOperations
    ) const
{
    Developer::BarrierData data  = {};

    // Set the barrier type to an invalid type.
    data.pCmdBuffer    = pCmdBuf;

    PAL_ASSERT(pOperations != nullptr);
    memcpy(&data.operations, pOperations, sizeof(Developer::BarrierOperations));

    m_pParent->DeveloperCb(Developer::CallbackType::BarrierEnd, &data);
}

// =====================================================================================================================
// Describes the image barrier to the above layers but only if we're a developer build. Clears the BarrierOperations
// passed in after calling back in case of layout transitions. This function is expected to be called only on layout
// transitions.
void Device::DescribeBarrier(
    GfxCmdBuffer*                 pCmdBuf,
    Developer::BarrierOperations* pOperations,
    const BarrierTransition*      pTransition
    ) const
{
    constexpr BarrierTransition NullTransition = {};
    Developer::BarrierData data                = {};

    data.pCmdBuffer    = pCmdBuf;
    data.transition    = (pTransition != nullptr) ? (*pTransition) : NullTransition;
    data.hasTransition = (pTransition != nullptr);

    PAL_ASSERT(pOperations != nullptr);
    // The callback is expected to be made only on layout transitions.
    memcpy(&data.operations, pOperations, sizeof(Developer::BarrierOperations));

    // Callback to the above layers if there is a transition and clear the BarrierOperations.
    m_pParent->DeveloperCb(Developer::CallbackType::ImageBarrier, &data);
    memset(pOperations, 0, sizeof(Developer::BarrierOperations));
}

} // Gfx6
} // Pal
