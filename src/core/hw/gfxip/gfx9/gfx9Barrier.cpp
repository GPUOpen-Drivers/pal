/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/gfx9/gfx9UniversalCmdBuffer.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9Image.h"
#include "palVectorImpl.h"

using namespace Util;

namespace Pal
{
namespace Gfx9
{

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
    GfxCmdBufferState             cmdBufState,
    const BarrierTransition&      transition,
    bool                          earlyPhase,
    SyncReqs*                     pSyncReqs,
    Developer::BarrierOperations* pOperations
    ) const
{
    PAL_ASSERT(transition.imageInfo.pImage != nullptr);

    bool       issuedBlt    = false;
    const bool noCacheFlags = ((transition.srcCacheMask == 0) && (transition.dstCacheMask == 0));

    const auto& image       = static_cast<const Pal::Image&>(*transition.imageInfo.pImage);
    const auto& gfx9Image   = static_cast<const Image&>(*image.GetGfxImage());
    const auto& subresRange = transition.imageInfo.subresRange;

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
        PAL_ASSERT(image.IsDepthStencil());

        const DepthStencilLayoutToState    layoutToState =
            gfx9Image.LayoutToDepthCompressionState(subresRange.startSubres);
        const DepthStencilCompressionState oldState      =
            ImageLayoutToDepthCompressionState(layoutToState, transition.imageInfo.oldLayout);
        const DepthStencilCompressionState newState      =
            ImageLayoutToDepthCompressionState(layoutToState, transition.imageInfo.newLayout);

        if ((oldState == DepthStencilCompressed) && (newState != DepthStencilCompressed))
        {
            // Performing an expand in the late phase is not ideal for performance, as it indicates the decompress
            // could not be pipelined and likely resulted in a bubble.  If an app is hitting this alert too often, it
            // may have an impact on performance.
            PAL_ALERT(earlyPhase == false);

            pOperations->layoutTransitions.depthStencilExpand = 1;
            DescribeBarrier(pCmdBuf,
                            &transition,
                            pOperations);

            LinearAllocatorAuto<VirtualLinearAllocator> allocator(pCmdBuf->Allocator(), false);
            IMsaaState* pMsaaState = BarrierMsaaState(this, pCmdBuf, &allocator, transition);

            if (pMsaaState != nullptr)
            {
                RsrcProcMgr().ExpandDepthStencil(pCmdBuf,
                                                 image,
                                                 pMsaaState,
                                                 transition.imageInfo.pQuadSamplePattern,
                                                 subresRange);

                pMsaaState->Destroy();
                PAL_SAFE_FREE(pMsaaState, &allocator);
            }

            issuedBlt = true;
        }
        // Resummarize the htile values from the depth-stencil surface contents when transitioning from "HiZ invalid"
        // state to something that uses HiZ.
        else if ((oldState == DepthStencilDecomprNoHiZ) && (newState != DepthStencilDecomprNoHiZ))
        {
            // If we are transitioning from uninitialized, resummarization is redundant.  This is because within
            // this same barrier, we have just initialized the htile to known values.
            if (TestAnyFlagSet(transition.imageInfo.oldLayout.usages, LayoutUninitializedTarget) == false)
            {
                // Use compute if:
                //   - We're on the compute engine
                //   - or we should force ExpandHiZRange for resummarize and we support compute operations
                const bool useCompute =
                    ((pCmdBuf->GetEngineType() == EngineTypeCompute) ||
                     (Pal::Image::ForceExpandHiZRangeForResummarize && pCmdBuf->IsComputeSupported()));

                if (useCompute)
                {
                    pOperations->layoutTransitions.htileHiZRangeExpand = 1;
                    DescribeBarrier(pCmdBuf,
                                    &transition,
                                    pOperations);

                    // CS blit to open-up the HiZ range.
                    RsrcProcMgr().HwlExpandHtileHiZRange(pCmdBuf, gfx9Image, subresRange);

                    // We need to wait for the compute shader to finish and also invalidate the texture L1 cache, TCC's
                    // meta cache before any further depth rendering can be done to this Image.
                    pSyncReqs->csPartialFlush = 1;
                    pSyncReqs->cacheFlags    |= CacheSyncInvTcp;
                    pSyncReqs->cacheFlags    |= CacheSyncInvTccMd;

                    // We also need to flush and invalidate L2 if we don't have any cache information just in case the
                    // client expects direct memory access to work after this barrier.
                    if (noCacheFlags)
                    {
                        pSyncReqs->cacheFlags |= CacheSyncFlushTcc | CacheSyncInvTcc;
                    }
                }
                else
                {
                    pOperations->layoutTransitions.depthStencilResummarize = 1;
                    DescribeBarrier(pCmdBuf,
                                    &transition,
                                    pOperations);

                    LinearAllocatorAuto<VirtualLinearAllocator> allocator(pCmdBuf->Allocator(), false);
                    IMsaaState* pMsaaState = BarrierMsaaState(this, pCmdBuf, &allocator, transition);

                    if (pMsaaState != nullptr)
                    {
                        // DB blit to resummarize.
                        RsrcProcMgr().ResummarizeDepthStencil(pCmdBuf,
                                                              image,
                                                              transition.imageInfo.newLayout,
                                                              pMsaaState,
                                                              transition.imageInfo.pQuadSamplePattern,
                                                              subresRange);

                        pMsaaState->Destroy();
                        PAL_SAFE_FREE(pMsaaState, &allocator);
                    }

                    issuedBlt = true;
                }
            }
        }

        // Flush DB/TC caches after to memory after decompressing/resummarizing.
        if (issuedBlt)
        {
            // Issue ACQUIRE_MEM stalls on depth/stencil surface writes and flush DB caches
            pSyncReqs->cpMeCoherCntl.bits.DB_DEST_BASE_ENA = 1;
            pSyncReqs->cpMeCoherCntl.bits.DEST_BASE_0_ENA  = 1;
            pSyncReqs->cacheFlags                         |= CacheSyncFlushAndInvDb;

            // The decompress/resummarize blit that was just executed was effectively a PAL-initiated draw that wrote to
            // the image and/or htile as a DB destination.  In addition to flushing the data out of the DB cache, we
            // need to invalidate any possible read/write caches that need coherent reads of this image's data.  If the
            // client was already rendering to this image through the DB caches on its own (i.e., srcCacheMask includes
            // CoherDepthStencilTarget), this shouldn't result in any additional sync.
            //
            // Note that we must always invalidate these caches if the client didn't give us any cache information.
            if (TestAnyFlagSet(transition.dstCacheMask, CoherShader | CoherCopy | CoherResolve) || noCacheFlags)
            {
                pSyncReqs->cacheFlags |= CacheSyncInvTcp;
                pSyncReqs->cacheFlags |= CacheSyncInvTccMd;
            }

            // We also need to flush and invalidate L2 if we don't have any cache information just in case the client
            // expects direct memory access to work after this barrier.
            if (noCacheFlags)
            {
                pSyncReqs->cacheFlags |= CacheSyncFlushTcc | CacheSyncInvTcc;
            }
        }
    }

    if (earlyPhase == false)
    {
        uint32 srcCacheMask = transition.srcCacheMask;

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
            // Issue ACQUIRE_MEM stalls on depth/stencil surface writes and flush DB caches
            pSyncReqs->cpMeCoherCntl.bits.DB_DEST_BASE_ENA = 1;
            pSyncReqs->cpMeCoherCntl.bits.DEST_BASE_0_ENA  = 1;
            pSyncReqs->cacheFlags                         |= CacheSyncFlushAndInvDb;

            //  We will need flush & inv L2 on MSAA Z, MSAA color, mips in the metadata tail, or any stencil.
            //
            // The driver assumes that all meta-data surfaces are pipe-aligned, but there are cases where the
            // HW does not actually pipe-align the data.  In these cases, the L2 cache needs to be flushed prior
            // to the metadata being read by a shader.  The following case is for depth/stencil metadata.
            const SubresId         firstSubresId        = subresRange.startSubres;
            const SubResourceInfo& firstSubres          = *image.SubresourceInfo(firstSubresId);
            const uint32           lastMipInRange       = (firstSubresId.mipLevel + (subresRange.numMips - 1));
            const bool             hasTcCompatibleHtile = (gfx9Image.HasHtileData() &&
                                                           (firstSubres.flags.supportMetaDataTexFetch == 1));
            if (hasTcCompatibleHtile                                 &&
                ((image.GetImageCreateInfo().samples > 1)            ||
                 (firstSubresId.aspect == Pal::ImageAspect::Stencil) ||
                 gfx9Image.IsInMetadataMipTail(lastMipInRange)))
            {
                pSyncReqs->cacheFlags |= CacheSyncFlushTcc | CacheSyncInvTcc;
            }
        }
    }
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

    const EngineType            engineType  = pCmdBuf->GetEngineType();
    const auto&                 image       = static_cast<const Pal::Image&>(*transition.imageInfo.pImage);
    auto&                       gfx9Image   = static_cast<Gfx9::Image&>(*image.GetGfxImage());
    const auto&                 subresRange = transition.imageInfo.subresRange;
    const SubResourceInfo*const pSubresInfo = image.SubresourceInfo(subresRange.startSubres);

    PAL_ASSERT(image.IsDepthStencil() == false);

    const ColorLayoutToState    layoutToState = gfx9Image.LayoutToColorCompressionState();
    const ColorCompressionState oldState      =
        ImageLayoutToColorCompressionState(layoutToState, transition.imageInfo.oldLayout);
    const ColorCompressionState newState      =
        ImageLayoutToColorCompressionState(layoutToState, transition.imageInfo.newLayout);

    // Menu of available BLTs.
    bool fastClearEliminate  = false;  // Writes the last clear color values to the base image for any pixel blocks that
                                       // are marked as fast cleared in CMask or DCC.  Single sample or MSAA.
    bool fmaskDecompress     = false;  // Leaves FMask-compressed pixel data in the base image, but puts FMask in a
                                       // texture-readable state (CMask marks all blocks as having the max number of
                                       // samples).  Causes a fast clear eliminate implicitly (if not using DCC).
    bool dccDecompress       = false;  // Writes decompressed pixel data to the base image and updates DCC to reflect
                                       // the decompressed state.  Single sample or MSAA.  Causes a fast clear eliminate
                                       // and fmask decompress implicitly.
    bool msaaColorDecompress = false;  // Shader based decompress that writes every sample's color value to the base
                                       // image.  An FMask decompress must be executed before this BLT.

    // Fast clear eliminates are only possible on universal queue command buffers and will be ignored on others.  This
    // should be okay because prior operations should be aware of this fact (based on layout), and prohibit us from
    // getting to a situation where one is needed but has not been performed yet.
    const bool fastClearEliminateSupported = pCmdBuf->IsGraphicsSupported();

    // The "earlyPhase" for decompress BLTs is before any waits and/or cache flushes have been inserted.  It is safe to
    // perform a color expand in the early phase if the client reports there is dirty data in the CB caches.  This
    // indicates:
    //
    //     1) There is no need to flush compressed data out of another cache or invalidate stale data in the CB caches
    //        before issuing the fixed-function DB expand:  the data is already in the right caches.
    //     2) There is no need to stall before beginning the decompress.  Data can only be dirty in one source cache at
    //        a time in a well-defined program, so we know the last output to this image was done with the CB.
    //
    // If this transition does not flush dirty data out of the CB caches, we delay the decompress until all client-
    // specified stalls and cache flushes have been executed (the late phase).  This situation should be rare, occurring
    // in cases like a clear to shader read transition without any rendering in between.
    if (earlyPhase == TestAnyFlagSet(transition.srcCacheMask, CoherColorTarget))
    {
        if ((oldState != ColorDecompressed) && (newState == ColorDecompressed))
        {
            if (gfx9Image.HasDccData())
            {
                dccDecompress = ((oldState == ColorCompressed) || pSubresInfo->flags.supportMetaDataTexFetch);
            }
            else if (image.GetImageCreateInfo().samples > 1)
            {
                // Needed in preparation for the full MSAA color decompress, which is always handled in the late
                // phase, below.
                fmaskDecompress = (oldState == ColorCompressed);
            }
            else
            {
                PAL_ASSERT(oldState == ColorCompressed);
                fastClearEliminate = fastClearEliminateSupported;
            }
        }
        else if ((oldState == ColorCompressed) && (newState == ColorFmaskDecompressed))
        {
            PAL_ASSERT(image.GetImageCreateInfo().samples > 1);
            if (pSubresInfo->flags.supportMetaDataTexFetch == false)
            {
                if (gfx9Image.HasDccData())
                {
                    // If the base pixel data is DCC compressed, but the image can't support metadata texture fetches,
                    // we need a DCC decompress.  The DCC decompress effectively executes an fmask decompress
                    // implicitly.
                    dccDecompress = true;
                }
                else
                {
                    fmaskDecompress = true;
                }
            }
            else
            {
                // if the image is TC compatible just need to do a fast clear eliminate
                fastClearEliminate = fastClearEliminateSupported;
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
                                                 LayoutCopySrc;

            // LayoutResolveSrc is treated as a color compressed state and if any decompression is required at resolve
            // time, @ref RsrcProcMgr::LateExpandResolveSrc will do the job.  So LayoutResolveSrc isn't added into
            // 'TcCompatReadFlags' above to skip performing a fast clear eliminate BLT.  If a shader resolve is to be
            // used, a barrier transiton to either LayoutShaderRead or LayoutShaderFmaskBasedRead is issued, which would
            // really trigger an FCE operation.
            if (fastClearEliminateSupported                                              &&
                TestAnyFlagSet(transition.imageInfo.newLayout.usages, TcCompatReadFlags) &&
                (gfx9Image.HasDccData()                                                  &&
                pSubresInfo->flags.supportMetaDataTexFetch))
            {
                if ((gfx9Image.HasSeenNonTcCompatibleClearColor() == false) && gfx9Image.IsFceOptimizationEnabled())
                {
                    // Skip the fast clear eliminate for this image if the clear color is TC-compatible and the
                    // optimization was enabled.
                    Result result = pCmdBuf->AddFceSkippedImageCounter(&gfx9Image);

                    if (result != Result::Success)
                    {
                        // Fallback to performing the Fast clear eliminate if the above step of the optimization failed.
                        fastClearEliminate = true;
                    }
                }
                else
                {
                    // The image has been fast cleared with a non-TC compatible color or the FCE optimization is not
                    // enabled.
                    fastClearEliminate = true;
                }
            }
        }

        if (dccDecompress)
        {
            if (earlyPhase && WaEnableDccCacheFlushAndInvalidate())
            {
                uint32* pCmdSpace = pCmdStream->ReserveCommands();
                pCmdSpace += m_cmdUtil.BuildNonSampleEventWrite(CACHE_FLUSH_AND_INV_EVENT, engineType, pCmdSpace);
                pCmdStream->CommitCommands(pCmdSpace);
            }

            pOperations->layoutTransitions.dccDecompress = 1;
            DescribeBarrier(pCmdBuf, &transition, pOperations);

            LinearAllocatorAuto<VirtualLinearAllocator> allocator(pCmdBuf->Allocator(), false);
            IMsaaState* pMsaaState = BarrierMsaaState(this, pCmdBuf, &allocator, transition);

            if (pMsaaState != nullptr)
            {
                RsrcProcMgr().DccDecompress(pCmdBuf,
                                            pCmdStream,
                                            gfx9Image,
                                            pMsaaState,
                                            transition.imageInfo.pQuadSamplePattern,
                                            subresRange);

                pMsaaState->Destroy();
                PAL_SAFE_FREE(pMsaaState, &allocator);
            }
        }
        else if (fmaskDecompress)
        {
            uint32* pCmdSpace = pCmdStream->ReserveCommands();

            if (earlyPhase)
            {
                // NOTE: CB.doc says we need to do a full CacheFlushInv event before the FMask decompress.  We're
                // using the lightweight event for now, but if we see issues this should be changed to the timestamp
                // version which waits for completion.
                pCmdSpace += m_cmdUtil.BuildNonSampleEventWrite(CACHE_FLUSH_AND_INV_EVENT, engineType, pCmdSpace);
            }
            else
            {
                // NOTE: If earlyPhase is false, that means that the previous usage of this Image was not by the CB.
                // (An example of this would be a fast-clear which uses a compute shader to fill Cmask.) This shouldn't
                // require us to flush the metadata cache before doing the decompress, since the CB wasn't previously
                // accessing the Image.
                pCmdSpace += m_cmdUtil.BuildNonSampleEventWrite(FLUSH_AND_INV_CB_META, engineType, pCmdSpace);
                pOperations->caches.flushCbMetadata = 1;
                pOperations->caches.invalCbMetadata = 1;
            }

            pCmdStream->CommitCommands(pCmdSpace);
            pOperations->layoutTransitions.fmaskDecompress = 1;
            DescribeBarrier(pCmdBuf, &transition, pOperations);

            LinearAllocatorAuto<VirtualLinearAllocator> allocator(pCmdBuf->Allocator(), false);
            IMsaaState* pMsaaState = BarrierMsaaState(this, pCmdBuf, &allocator, transition);

            if (pMsaaState != nullptr)
            {
                RsrcProcMgr().FmaskDecompress(pCmdBuf,
                                              pCmdStream,
                                              gfx9Image,
                                              pMsaaState,
                                              transition.imageInfo.pQuadSamplePattern,
                                              subresRange);

                pMsaaState->Destroy();
                PAL_SAFE_FREE(pMsaaState, &allocator);
            }
        }
        else if (fastClearEliminate)
        {
            if (earlyPhase && WaEnableDccCacheFlushAndInvalidate() && gfx9Image.HasDccData())
            {
                uint32* pCmdSpace = pCmdStream->ReserveCommands();
                pCmdSpace += m_cmdUtil.BuildNonSampleEventWrite(CACHE_FLUSH_AND_INV_EVENT, engineType, pCmdSpace);
                pCmdStream->CommitCommands(pCmdSpace);
            }

            pOperations->layoutTransitions.fastClearEliminate = 1;
            DescribeBarrier(pCmdBuf, &transition, pOperations);

            LinearAllocatorAuto<VirtualLinearAllocator> allocator(pCmdBuf->Allocator(), false);
            IMsaaState* pMsaaState = BarrierMsaaState(this, pCmdBuf, &allocator, transition);

            if (pMsaaState != nullptr)
            {
                // Note: if FCE is not submitted to GPU, we don't need to update cache flags.
                fastClearEliminate = RsrcProcMgr().FastClearEliminate(pCmdBuf,
                                                                      pCmdStream,
                                                                      gfx9Image,
                                                                      pMsaaState,
                                                                      transition.imageInfo.pQuadSamplePattern,
                                                                      subresRange);

                pMsaaState->Destroy();
                PAL_SAFE_FREE(pMsaaState, &allocator);
            }
        }
    }

    // Issue an MSAA color decompress, if necessary.  This BLT is always performed during the late phase, since it is
    // implied that an fmask decompress BLT would have to be executed first, occupying the early phase.
    if ((earlyPhase == false)                    &&
        (image.GetImageCreateInfo().samples > 1) &&
        (oldState != ColorDecompressed)          &&
        (newState == ColorDecompressed))
    {
        msaaColorDecompress = true;

        // Check if the fmask decompress or DCC decompress was already executed during this phase.  If so, we need to
        // wait for those to finish and flush everything out of the CB caches first.
        if (fmaskDecompress || dccDecompress)
        {
            // This must execute on a queue that supports graphics operations
            PAL_ASSERT(pCmdBuf->IsGraphicsSupported());

            uint32* pCmdSpace = pCmdStream->ReserveCommands();
            pCmdSpace += m_cmdUtil.BuildWaitOnReleaseMemEvent(engineType,
                                                              CACHE_FLUSH_AND_INV_TS_EVENT,
                                                              TcCacheOp::Nop,
                                                              pCmdBuf->TimestampGpuVirtAddr(),
                                                              pCmdSpace);
            pCmdStream->CommitCommands(pCmdSpace);
        }

        pOperations->layoutTransitions.fmaskColorExpand = 1;
        DescribeBarrier(pCmdBuf,
                        &transition,
                        pOperations);

        RsrcProcMgr().FmaskColorExpand(pCmdBuf, gfx9Image, subresRange);
    }

    // These CB decompress operations can only be performed on queues that support graphics
    const bool didGfxBlt = (pCmdBuf->IsGraphicsSupported() &&
                            (dccDecompress || fastClearEliminate || fmaskDecompress || msaaColorDecompress));

    if (didGfxBlt)
    {
        // Performing an expand in the late phase is not ideal for performance, as it indicates the decompress could not
        // be pipelined and likely resulted in a bubble.  If an app is hitting this alert too often, it may have an
        // impact on performance.
        PAL_ALERT_MSG(earlyPhase == false, "Performing an expand in the late phase, oldLayout=0x%x, newLayout=0x%x",
                      transition.imageInfo.oldLayout, transition.imageInfo.newLayout);

        // CB metadata caches can only be flushed with a pipelined VGT event, like CACHE_FLUSH_AND_INV.  In order to
        // ensure the cache flush finishes before continuing, we must wait on a timestamp.
        pSyncReqs->waitOnEopTs = 1;
        pSyncReqs->cacheFlags |= CacheSyncFlushAndInvRb;

        // The decompression that was just executed was effectively a PAL-initiated draw that wrote to the image as a
        // CB destination.  In addition to flushing the data out of the CB cache, we need to invalidate any possible
        // read/write caches that need coherent reads of this image's data.  If the client was already rendering to
        // this image through the CB caches on its own (i.e., srcCacheMask includes CoherColorTarget), this shouldn't
        // result in any additional sync.
        //
        // Also, MSAA color decompress does some fmask fixup work with a compute shader.  The waitOnEopTs
        // requirement set for all CB BLTs will ensure the CS work completes, but we need to specifically request the
        // texture L1 caches and TCC's meta caches to be flushed.
        //
        // Note that we must always invalidate these caches if the client didn't give us any cache information.
        const bool noCacheFlags = ((transition.srcCacheMask == 0) && (transition.dstCacheMask == 0));

        if (TestAnyFlagSet(transition.dstCacheMask, CoherShader | CoherCopy | CoherResolve) || noCacheFlags)
        {
            pSyncReqs->cacheFlags |= CacheSyncInvTcp;
            pSyncReqs->cacheFlags |= CacheSyncInvTccMd;
        }

        // We also need to flush and invalidate L2 if we don't have any cache information just in case the client
        // expects direct memory access to work after this barrier.
        if (noCacheFlags)
        {
            pSyncReqs->cacheFlags |= CacheSyncFlushTcc | CacheSyncInvTcc;
        }
    }

    if ((earlyPhase == false) &&
        (TestAnyFlagSet(transition.srcCacheMask, CoherColorTarget | CoherClear) || didGfxBlt))
    {
        //  We will need flush & inv L2 on MSAA Z, MSAA color, mips in the metadata tail, or any stencil.
        //
        // The driver assumes that all meta-data surfaces are pipe-aligned, but there are cases where the
        // HW does not actually pipe-align the data.  In these cases, the L2 cache needs to be flushed prior
        // to the metadata being read by a shader.  The following case is for color metadata.
        const SubresId         firstSubresId      = subresRange.startSubres;
        const SubResourceInfo& firstSubres        = *image.SubresourceInfo(firstSubresId);
        const uint32           lastMipInRange     = (firstSubresId.mipLevel + (subresRange.numMips - 1));
        const bool             hasTcCompatibleDcc = (gfx9Image.HasDccData() &&
                                                     (firstSubres.flags.supportMetaDataTexFetch == 1));
        if ((hasTcCompatibleDcc && ((image.GetImageCreateInfo().samples > 1) ||
                                    gfx9Image.IsInMetadataMipTail(lastMipInRange))) ||
            (gfx9Image.HasFmaskData() && (gfx9Image.HasDccData() == false)))
        {
            pSyncReqs->cacheFlags |= CacheSyncFlushTcc | CacheSyncInvTcc;
        }
    }
}

// =====================================================================================================================
void Device::FillCacheOperations(
    const SyncReqs&               syncReqs,
    Developer::BarrierOperations* pOperations
    ) const
{
    PAL_ASSERT(pOperations != nullptr);

    pOperations->caches.invalTcp        |= TestAnyFlagSet(syncReqs.cacheFlags, CacheSyncInvTcp);
    pOperations->caches.invalSqI$       |= TestAnyFlagSet(syncReqs.cacheFlags, CacheSyncInvSqI$);
    pOperations->caches.invalSqK$       |= TestAnyFlagSet(syncReqs.cacheFlags, CacheSyncInvSqK$);
    pOperations->caches.flushTcc        |= TestAnyFlagSet(syncReqs.cacheFlags, CacheSyncFlushTcc);
    pOperations->caches.invalTcc        |= TestAnyFlagSet(syncReqs.cacheFlags, CacheSyncInvTcc);
    pOperations->caches.flushCb         |= TestAnyFlagSet(syncReqs.cacheFlags, CacheSyncFlushCbData);
    pOperations->caches.invalCb         |= TestAnyFlagSet(syncReqs.cacheFlags, CacheSyncInvCbData);
    pOperations->caches.flushDb         |= TestAnyFlagSet(syncReqs.cacheFlags, CacheSyncFlushDbData);
    pOperations->caches.invalDb         |= TestAnyFlagSet(syncReqs.cacheFlags, CacheSyncInvDbData);
    pOperations->caches.invalCbMetadata |= TestAnyFlagSet(syncReqs.cacheFlags, CacheSyncInvCbMd);
    pOperations->caches.flushCbMetadata |= TestAnyFlagSet(syncReqs.cacheFlags, CacheSyncFlushCbMd);
    pOperations->caches.invalDbMetadata |= TestAnyFlagSet(syncReqs.cacheFlags, CacheSyncInvDbMd);
    pOperations->caches.flushDbMetadata |= TestAnyFlagSet(syncReqs.cacheFlags, CacheSyncFlushDbMd);
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
    const EngineType engineType     = pCmdBuf->GetEngineType();
    const bool       isGfxSupported = pCmdBuf->IsGraphicsSupported();
    const uint32     origCacheFlags = syncReqs.cacheFlags;
    uint32*          pCmdSpace      = pCmdStream->ReserveCommands();

    FillCacheOperations(syncReqs, pOperations);

    if (syncReqs.syncCpDma)
    {
        // Stalls the CP ME until the CP's DMA engine has finished all async DMA_DATA commands. This needs to
        // go before the calls to BuildWaitOnReleaseMemEvent and BuildAcquireMem so that the results of CP blts are
        // flushed properly. Also note that DMA packets are the only way to wait for DMA work, we can't use something
        // like a bottom-of-pipe timestamp.
        pCmdSpace += m_cmdUtil.BuildWaitDmaData(pCmdSpace);
        pOperations->pipelineStalls.syncCpDma = 1;
    }

    // We can't flush or invalidate CB metadata using an ACQUIRE_MEM so we must force a wait-on-eop-ts.
    if (TestAnyFlagSet(syncReqs.cacheFlags, CacheSyncFlushAndInvCbMd))
    {
        syncReqs.waitOnEopTs = 1;
    }

    if (syncReqs.waitOnEopTs)
    {
        // Issue a pipelined event that will write a timestamp value to GPU memory when finished. Then, stall the CP ME
        // until that timestamp is seen written to the GPU memory. This is a very heavyweight sync, and ensures all
        // previous graphics and compute work has completed.
        //
        // We will also issue any cache flushes or invalidations that can be pipelined with the timestamp.
        VGT_EVENT_TYPE eopEvent = BOTTOM_OF_PIPE_TS;

        if (TestAnyFlagSet(syncReqs.cacheFlags, CacheSyncFlushAndInvRb))
        {
            syncReqs.cacheFlags &= ~CacheSyncFlushAndInvRb;
            eopEvent             = CACHE_FLUSH_AND_INV_TS_EVENT;
        }

        pOperations->pipelineStalls.waitOnEopTsBottomOfPipe = 1;
        pCmdSpace += m_cmdUtil.BuildWaitOnReleaseMemEvent(engineType,
                                                          eopEvent,
                                                          SelectTcCacheOp(&syncReqs.cacheFlags),
                                                          pCmdBuf->TimestampGpuVirtAddr(),
                                                          pCmdSpace);
        pCmdBuf->SetPrevCmdBufInactive();

        // WriteWaitOnEopEvent waits in the ME, if the waitPoint needs to stall at the PFP request a PFP/ME sync.
        syncReqs.pfpSyncMe = (waitPoint == HwPipeTop);

        // The previous sync has already ensured that the graphics contexts are idle and all CS waves have completed.
        syncReqs.cpMeCoherCntl.u32All &= ~CpMeCoherCntlStallMask;
    }
    else
    {
        // If the address range covers from 0 to all Fs, and any of the BASE_ENA bits in the CP_COHER_CNTL value are
        // set, the ACQUIRE_MEM issued at the end of this function is guaranteed to idle all graphics contexts.  Based
        // on that knowledge, some other commands may be skipped.
        if (isGfxSupported &&
            ((rangeStartAddr != FullSyncBaseAddr) ||
             (rangeSize != FullSyncSize)          ||
             (TestAnyFlagSet(syncReqs.cpMeCoherCntl.u32All, CpMeCoherCntlStallMask) == false)))
        {
            if (syncReqs.vsPartialFlush)
            {
                // Waits in the CP ME for all previously issued VS waves to complete.
                pCmdSpace += m_cmdUtil.BuildNonSampleEventWrite(VS_PARTIAL_FLUSH, engineType, pCmdSpace);
                pOperations->pipelineStalls.vsPartialFlush = 1;
            }

            if (syncReqs.psPartialFlush)
            {
                // Waits in the CP ME for all previously issued PS waves to complete.
                pCmdSpace += m_cmdUtil.BuildNonSampleEventWrite(PS_PARTIAL_FLUSH, engineType, pCmdSpace);
                pOperations->pipelineStalls.psPartialFlush = 1;
            }
        }

        if (syncReqs.csPartialFlush)
        {
            // Waits in the CP ME for all previously issued CS waves to complete.
            pCmdSpace += m_cmdUtil.BuildNonSampleEventWrite(CS_PARTIAL_FLUSH, engineType, pCmdSpace);
            pOperations->pipelineStalls.csPartialFlush = 1;
        }
    }

    // Issue accumulated ACQUIRE_MEM commands on the specified memory range. Note that we must issue one ACQUIRE_MEM
    // if cacheFlags is zero but cpMeCoherCntl non-zero to implement a range-checked target stall.
    if ((syncReqs.cacheFlags != 0) || (syncReqs.cpMeCoherCntl.u32All != 0))
    {
        do
        {
            AcquireMemInfo acquireInfo = {};
            acquireInfo.flags.usePfp         = (waitPoint == HwPipeTop);
            acquireInfo.flags.invSqI$        = TestAnyFlagSet(syncReqs.cacheFlags, CacheSyncInvSqI$);
            acquireInfo.flags.invSqK$        = TestAnyFlagSet(syncReqs.cacheFlags, CacheSyncInvSqK$);
            acquireInfo.flags.flushSqK$      = TestAnyFlagSet(syncReqs.cacheFlags, CacheSyncFlushSqK$);
            acquireInfo.flags.wbInvCbData    = TestAnyFlagSet(syncReqs.cacheFlags, CacheSyncFlushAndInvCbData);
            acquireInfo.flags.wbInvDb        = TestAnyFlagSet(syncReqs.cacheFlags, CacheSyncFlushAndInvDb);

            syncReqs.cacheFlags &= ~(CacheSyncInvSqI$ | CacheSyncInvSqK$ | CacheSyncFlushSqK$ |
                                     CacheSyncFlushAndInvCbData | CacheSyncFlushAndInvDb);

            acquireInfo.engineType           = engineType;
            acquireInfo.cpMeCoherCntl.u32All = syncReqs.cpMeCoherCntl.u32All;
            acquireInfo.tcCacheOp            = SelectTcCacheOp(&syncReqs.cacheFlags);
            acquireInfo.baseAddress          = rangeStartAddr;
            acquireInfo.sizeBytes            = rangeSize;

            pCmdSpace += m_cmdUtil.BuildAcquireMem(acquireInfo, pCmdSpace);

            // If we didn't pick a cache op but there are still valid cache flags we will never clear them and this loop
            // will never terminate. In practice this should never happen because this function handles all flags that
            // can't be cleared by an ACQUIRE_MEM before this loop.
            PAL_ASSERT((acquireInfo.tcCacheOp != TcCacheOp::Nop) || (syncReqs.cacheFlags == 0));
        }
        while (syncReqs.cacheFlags != 0);

        if (isGfxSupported)
        {
            pCmdStream->SetContextRollDetected<false>();
        }
    }

    if (syncReqs.pfpSyncMe && isGfxSupported)
    {
        // Stalls the CP PFP until the ME has processed all previous commands.  Useful in cases where the ME is waiting
        // on some condition, but the PFP needs to stall execution until the condition is satisfied.  This must go last
        // otherwise the PFP could resume execution before the ME is done with all of its waits.
        pCmdSpace += m_cmdUtil.BuildPfpSyncMe(pCmdSpace);
        pOperations->pipelineStalls.pfpSyncMe = 1;
    }

    pCmdStream->CommitCommands(pCmdSpace);

    // Clear up xxxBltActive flags
    if (syncReqs.waitOnEopTs || TestAnyFlagSet(syncReqs.cpMeCoherCntl.u32All, CpMeCoherCntlStallMask))
    {
        pCmdBuf->SetGfxCmdBufGfxBltState(false);
    }
    if ((pCmdBuf->GetGfxCmdBufState().gfxBltActive == false) &&
        (TestAnyFlagSet(origCacheFlags, CacheSyncFlushAndInvRb) && (syncReqs.waitOnEopTs != 0)))
    {
        pCmdBuf->SetGfxCmdBufGfxBltWriteCacheState(false);
    }

    if (syncReqs.waitOnEopTs || syncReqs.csPartialFlush)
    {
        pCmdBuf->SetGfxCmdBufCsBltState(false);
    }
    if ((pCmdBuf->GetGfxCmdBufState().csBltActive == false) && TestAnyFlagSet(origCacheFlags, CacheSyncFlushTcc))
    {
        pCmdBuf->SetGfxCmdBufCsBltWriteCacheState(false);
    }

    if (syncReqs.syncCpDma)
    {
        pCmdBuf->SetGfxCmdBufCpBltState(false);
    }
    if (pCmdBuf->GetGfxCmdBufState().cpBltActive == false)
    {
        if (TestAnyFlagSet(origCacheFlags, CacheSyncFlushTcc))
        {
            pCmdBuf->SetGfxCmdBufCpBltWriteCacheState(false);
        }
        if (TestAnyFlagSet(origCacheFlags, CacheSyncInvTcc))
        {
            pCmdBuf->SetGfxCmdBufCpMemoryWriteL2CacheStaleState(false);
        }
    }
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
    GfxCmdBufferState cmdBufState = pCmdBuf->GetGfxCmdBufState();

    // -----------------------------------------------------------------------------------------------------------------
    // -- Early image layout transitions.
    // -----------------------------------------------------------------------------------------------------------------
    if (barrier.flags.splitBarrierLatePhase == 0)
    {
        DescribeBarrierStart(pCmdBuf, barrier.reason);

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
                        TransitionDepthStencil(pCmdBuf,
                                               cmdBufState,
                                               barrier.pTransitions[i],
                                               true,
                                               &globalSyncReqs,
                                               &barrierOps);
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
                globalSyncReqs.pfpSyncMe      = 1;
                break;
            case HwPipePreRasterization:
                globalSyncReqs.vsPartialFlush = 1;
                globalSyncReqs.pfpSyncMe      = (barrier.waitPoint == HwPipeTop);
                break;
            case HwPipePostPs:
                globalSyncReqs.vsPartialFlush = 1;
                globalSyncReqs.psPartialFlush = 1;
                globalSyncReqs.pfpSyncMe      = (barrier.waitPoint == HwPipeTop);
                break;
            case HwPipePostCs:
                globalSyncReqs.csPartialFlush = 1;
                globalSyncReqs.pfpSyncMe      = (barrier.waitPoint == HwPipeTop);
                break;
            case HwPipeBottom:
                globalSyncReqs.waitOnEopTs    = 1;
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
        // - If a CP L2 BLT occured, alias these srcCaches to CoherTimestamp (this isn't good but we have no CoherL2).
        // - If a CP direct-to-memory write occured, alias these srcCaches to CoherMemory.
        // Clear the original srcCaches from the srcCache mask for the rest of this scope.
        if (TestAnyFlagSet(srcCacheMask, CoherCopy | CoherClear | CoherResolve))
        {
            srcCacheMask &= ~(CoherCopy | CoherClear | CoherResolve);

            srcCacheMask |= cmdBufState.gfxWriteCachesDirty       ? CoherColorTarget : 0;
            srcCacheMask |= cmdBufState.csWriteCachesDirty        ? CoherShader      : 0;
            srcCacheMask |= cmdBufState.cpWriteCachesDirty        ? CoherTimestamp   : 0;
            srcCacheMask |= cmdBufState.cpMemoryWriteL2CacheStale ? CoherMemory      : 0;
        }

        // alwaysL2Mask is a mask of usages that always read/write through the L2 cache.
        const uint32 alwaysL2Mask = (CoherShader             |
                                     CoherCopy               |
                                     CoherColorTarget        |
                                     CoherDepthStencilTarget |
                                     CoherResolve            |
                                     CoherClear              |
                                     CoherIndirectArgs       |
                                     CoherIndexData          |
                                     CoherQueueAtomic        |
                                     CoherTimestamp          |
                                     CoherCeLoad             |
                                     CoherCeDump             |
                                     CoherStreamOut);

        // MaybeL2Mask is a mask of usages that may or may not read/write through the L2 cache.
        const uint32 maybeL2Mask = alwaysL2Mask;

        // Flush L2 if prior output might have been through L2 and upcoming reads/writes might not be through L2.
        if (TestAnyFlagSet(srcCacheMask, maybeL2Mask) && TestAnyFlagSet(transition.dstCacheMask, ~alwaysL2Mask))
        {
            globalSyncReqs.cacheFlags |= CacheSyncInvTcc | CacheSyncFlushTcc;
        }

        // Invalidate L2 if prior output might not have been through L2 and upcoming reads/writes might be through L2.
        if (TestAnyFlagSet(srcCacheMask, ~alwaysL2Mask) && TestAnyFlagSet(transition.dstCacheMask, maybeL2Mask))
        {
            globalSyncReqs.cacheFlags |= CacheSyncInvTcc | CacheSyncFlushTcc;
        }

        constexpr uint32 MaybeL1ShaderMask = CoherShader | CoherStreamOut | CoherCopy | CoherResolve | CoherClear;

        // Invalidate L1 shader caches if the previous output may have done shader writes, since there is no coherence
        // between different CUs' TCP (vector L1) caches.  Invalidate TCP and flush and invalidate SQ-K cache
        // (scalar cache) if this barrier is forcing shader read coherency.
        if (TestAnyFlagSet(srcCacheMask, MaybeL1ShaderMask) ||
            TestAnyFlagSet(transition.dstCacheMask, MaybeL1ShaderMask))
        {
            globalSyncReqs.cacheFlags |= CacheSyncInvTcp;
            globalSyncReqs.cacheFlags |= CacheSyncInvSqK$;
        }

        if (TestAnyFlagSet(srcCacheMask, CoherColorTarget) &&
            (TestAnyFlagSet(srcCacheMask, ~CoherColorTarget) ||
             TestAnyFlagSet(transition.dstCacheMask, ~CoherColorTarget)))
        {
            // CB metadata caches can only be flushed with a pipelined VGT event, like CACHE_FLUSH_AND_INV.  In order to
            // ensure the cache flush finishes before continuing, we must wait on a timestamp.  Catch those cases early
            // here so that we can perform it along with the rest of the stalls so that we might hide the bubble this
            // will introduce.
            globalSyncReqs.waitOnEopTs = 1;
            globalSyncReqs.cacheFlags |= CacheSyncFlushAndInvRb;
        }

        constexpr uint32 MaybeTccMdShaderMask = CoherShader | CoherCopy | CoherResolve | CoherClear;

        // Invalidate TCC's meta data cache to prevent future threads from reading stale data, since TCC's meta data
        // cache is non-coherent and read-only.
        if (TestAnyFlagSet(srcCacheMask, MaybeTccMdShaderMask) ||
            TestAnyFlagSet(transition.dstCacheMask, MaybeTccMdShaderMask))
        {
            globalSyncReqs.cacheFlags |= CacheSyncInvTccMd;
        }

        // Check if the currently bound depth/stencil target requires TCC flush. This may be needed before a shader
        // reads D/S metadata.
        if ((transition.imageInfo.pImage == nullptr) &&
            (TestAnyFlagSet(globalSyncReqs.cacheFlags, CacheSyncInvTcc | CacheSyncFlushTcc) == false))
        {
            if (cmdBufState.depthMdNeedsTccFlush)
            {
                globalSyncReqs.cacheFlags |= CacheSyncInvTcc | CacheSyncFlushTcc;
            }
        }
    }

    // Check conditions that end up requiring a stall for all GPU work to complete.  The cases are:
    //     - A pipelined wait has been requested.
    //     - Any DEST_BASE_ENA bit is set in the global ACQUIRE_MEM request, waiting for all gfx contexts to be idle.
    //     - If a CS_PARTIAL_FLUSH AND either VS/PS_PARTIAL_FLUSH are requested, we have to idle the whole pipe to
    //       ensure both sets of potentially parallel work have completed.
    const bool bottomOfPipeStall = (globalSyncReqs.waitOnEopTs ||
                                    (globalSyncReqs.cpMeCoherCntl.u32All != 0) ||
                                    (globalSyncReqs.csPartialFlush &&
                                     (globalSyncReqs.vsPartialFlush || globalSyncReqs.psPartialFlush)));

    const uint32 numEventSlots = Parent()->ChipProperties().gfxip.numSlotsPerEvent;

    if (barrier.pSplitBarrierGpuEvent != nullptr)
    {
        if (barrier.flags.splitBarrierEarlyPhase)
        {
            // This is the early phase of a split barrier.  We've already performed any early phase decompresses, etc.
            // that were possible.

            // Reset the split barrier event to get it in a known state.
            pCmdBuf->CmdResetEvent(*barrier.pSplitBarrierGpuEvent, HwPipeTop);

            // If this barrier requires CB/DB caches to be flushed, enqueue a pipeline event to do that now.  In
            // particular, note that CB/DB flushes performed by an ACQUIRE_MEM with a regular barrier is converted to
            // a pipelined event in a split barrier.
            if (TestAnyFlagSet(globalSyncReqs.cacheFlags, CacheSyncFlushAndInvRb))
            {
                uint32* pCmdSpace = pCmdStream->ReserveCommands();
                pCmdSpace += m_cmdUtil.BuildNonSampleEventWrite(CACHE_FLUSH_AND_INV_EVENT,
                                                                pCmdBuf->GetEngineType(),
                                                                pCmdSpace);
                pCmdStream->CommitCommands(pCmdSpace);
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
            const GpuEvent* pGpuEvent       = static_cast<const GpuEvent*>(barrier.pSplitBarrierGpuEvent);
            const gpusize   gpuEventStartVa = pGpuEvent->GetBoundGpuMemory().GpuVirtAddr();

            uint32* pCmdSpace = pCmdStream->ReserveCommands();
            for (uint32 slotIdx = 0; slotIdx < numEventSlots; slotIdx++)
            {
                pCmdSpace += m_cmdUtil.BuildWaitRegMem(mem_space__me_wait_reg_mem__memory_space,
                                                       function__me_wait_reg_mem__equal_to_the_reference_value,
                                                       engine_sel__pfp_wait_reg_mem__prefetch_parser,
                                                       gpuEventStartVa + (sizeof(uint32) * slotIdx),
                                                       GpuEvent::SetValue,
                                                       0xFFFFFFFF,
                                                       pCmdSpace);
            }
            pCmdStream->CommitCommands(pCmdSpace);

            if (globalSyncReqs.waitOnEopTs)
            {
                pCmdBuf->SetPrevCmdBufInactive();
            }

            // Clear any global sync requirements that we know have been satisfied by the wait on pSplitBarrierGpuEvent.
            globalSyncReqs.waitOnEopTs           = 0;
            globalSyncReqs.vsPartialFlush        = 0;
            globalSyncReqs.psPartialFlush        = 0;
            globalSyncReqs.csPartialFlush        = 0;
            globalSyncReqs.pfpSyncMe             = 0;
            globalSyncReqs.cacheFlags           &= ~CacheSyncFlushAndInvRb;
            globalSyncReqs.cpMeCoherCntl.u32All &= ~CpMeCoherCntlStallMask;

            // Some globalSyncReqs bits may still be set.  These will allow any late cache flush/invalidations that have
            // to be performed with ACQUIRE_MEM to be executed during the IssueSyncs() call, below.
        }
    }

    if (barrier.flags.splitBarrierEarlyPhase == 0)
    {
        // Skip the range-checked stalls if we know a global stall will ensure all graphics contexts are idle.
        if (bottomOfPipeStall == false)
        {
            // Issue any range-checked target stalls.  This will wait for any active graphics contexts that reference
            // the VA range of the specified image to be idle.
            for (uint32 i = 0; i < barrier.rangeCheckedTargetWaitCount; i++)
            {
                const Pal::Image* pImage     = static_cast<const Pal::Image*>(barrier.ppTargets[i]);
                const Image*      pGfx9Image = static_cast<const Image*>(pImage->GetGfxImage());

                SyncReqs targetStallSyncReqs = { };
                targetStallSyncReqs.cpMeCoherCntl.u32All = CpMeCoherCntlStallMask;

                if (pImage != nullptr)
                {
                    IssueSyncs(pCmdBuf,
                               pCmdStream,
                               targetStallSyncReqs,
                               barrier.waitPoint,
                               pImage->GetGpuVirtualAddr(),
                               pGfx9Image->GetGpuMemSyncSize(),
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
            const GpuEvent* pGpuEvent       = static_cast<const GpuEvent*>(barrier.ppGpuEvents[i]);
            const gpusize   gpuEventStartVa = pGpuEvent->GetBoundGpuMemory().GpuVirtAddr();
            const uint32    waitEngine      = (barrier.waitPoint == HwPipeTop) ?
                                              static_cast<uint32>(engine_sel__pfp_wait_reg_mem__prefetch_parser) :
                                              static_cast<uint32>(engine_sel__me_wait_reg_mem__micro_engine);

            uint32* pCmdSpace = pCmdStream->ReserveCommands();
            for (uint32 slotIdx = 0; slotIdx < numEventSlots; slotIdx++)
            {
                pCmdSpace += m_cmdUtil.BuildWaitRegMem(mem_space__me_wait_reg_mem__memory_space,
                                                       function__me_wait_reg_mem__equal_to_the_reference_value,
                                                       waitEngine,
                                                       gpuEventStartVa + (sizeof(uint32) * slotIdx),
                                                       GpuEvent::SetValue,
                                                       0xFFFFFFFF,
                                                       pCmdSpace);
            }

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
                    const auto& gfx9Image   = static_cast<const Gfx9::Image&>(*image.GetGfxImage());
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

                    if (gfx9Image.HasColorMetaData() || gfx9Image.HasHtileData())
                    {
                        barrierOps.layoutTransitions.initMaskRam = 1;
                        DescribeBarrier(pCmdBuf,
                                        &barrier.pTransitions[i],
                                        &barrierOps);

                        bool usedCompute = RsrcProcMgr().InitMaskRam(pCmdBuf, pCmdStream, gfx9Image, subresRange);

                        // After initializing Mask RAM, we need some syncs to guarantee the initialization blts have
                        // finished, even if other Blts caused these operations to occur before any Blts were performed.
                        // Using our knowledge of the code above (and praying it never changes) we need:
                        // - A CS_PARTIAL_FLUSH, L1 invalidation and TCC's meta cache invalidation if a compute shader
                        //   was used.
                        // - A CP DMA sync to wait for all asynchronous CP DMAs which are used to upload our
                        //   meta-equation. (GFX9 only)
                        if (usedCompute)
                        {
                            initSyncReqs.csPartialFlush = 1;
                            initSyncReqs.cacheFlags    |= CacheSyncInvTcp;
                            initSyncReqs.cacheFlags    |= CacheSyncInvTccMd;
                        }

                        if (m_gfxIpLevel == GfxIpLevel::GfxIp9)
                        {
                            initSyncReqs.syncCpDma = 1;
                        }
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

            if (transition.imageInfo.pImage != nullptr)
            {
                if ((TestAnyFlagSet(transition.imageInfo.oldLayout.usages, LayoutUninitializedTarget) == false) &&
                    (TestAnyFlagSet(transition.imageInfo.newLayout.usages, LayoutUninitializedTarget) == false))
                {
                    const auto& image     = static_cast<const Pal::Image&>(*transition.imageInfo.pImage);
                    const auto& gfx9Image = static_cast<const Gfx9::Image&>(*image.GetGfxImage());

                    SyncReqs imageSyncReqs = { };

                    if (image.IsDepthStencil())
                    {
                        // Issue a late-phase DB decompress, if necessary.
                        TransitionDepthStencil(pCmdBuf, cmdBufState, transition, false, &imageSyncReqs, &barrierOps);
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
                               gfx9Image.GetGpuMemSyncSize(),
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
    GfxCmdBuffer* pCmdBuf,
    uint32        reason
    ) const
{
    Developer::BarrierData barrierData = {};

    barrierData.pCmdBuffer = pCmdBuf;

    // Make sure we have an acceptable barrier reason.
    PAL_ALERT_MSG((GetPlatform()->IsDevDriverProfilingEnabled() && (reason == Developer::BarrierReasonInvalid)),
                  "Invalid barrier reason codes are not allowed!");

    barrierData.reason = reason;

    m_pParent->DeveloperCb(Developer::CallbackType::BarrierBegin, &barrierData);
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
    const BarrierTransition*      pTransition,
    Developer::BarrierOperations* pOperations
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

} // Gfx9
} // Pal
