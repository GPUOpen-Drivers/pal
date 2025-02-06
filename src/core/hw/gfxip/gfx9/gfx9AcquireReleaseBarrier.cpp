/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9Image.h"
#include "core/hw/gfxip/gfx9/gfx9UniversalCmdBuffer.h"
#include "palVectorImpl.h"

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

// Cache coherency masks that may read data through L0 (V$ and K$)/L1 caches.
static constexpr uint32 CacheCoherShaderReadMask = CoherShaderRead | CoherSampleRate | CacheCoherencyBltSrc;

// Cache coherency masks that may read or write data through L0 (V$ and K$)/L1 caches.
static constexpr uint32 CacheCoherShaderAccessMask = CacheCoherencyBlt | CoherShader | CoherSampleRate | CoherStreamOut;

// None cache sync operations
static constexpr CacheSyncOps NullCacheSyncOps = {};

// =====================================================================================================================
BarrierMgr::BarrierMgr(
    GfxDevice*     pGfxDevice,
    const CmdUtil& cmdUtil)
    :
    GfxBarrierMgr(pGfxDevice),
    m_cmdUtil(cmdUtil),
    m_gfxIpLevel(pGfxDevice->Parent()->ChipProperties().gfxLevel)
{
}

// =====================================================================================================================
bool BarrierMgr::EnableReleaseMemWaitCpDma() const
{
    return static_cast<Device*>(m_pGfxDevice)->EnableReleaseMemWaitCpDma();
}

// =====================================================================================================================
const RsrcProcMgr& BarrierMgr::RsrcProcMgr() const
{
    return static_cast<const Gfx9::RsrcProcMgr&>(m_pGfxDevice->RsrcProcMgr());
}

// =====================================================================================================================
CmdStream* BarrierMgr::GetCmdStream(
    GfxCmdBuffer* pCmdBuf)
{
    const auto cmdStreamEngine = pCmdBuf->IsGraphicsSupported() ? CmdBufferEngineSupport::Graphics
                                                                : CmdBufferEngineSupport::Compute;

    return static_cast<CmdStream*>(pCmdBuf->GetCmdStreamByEngine(cmdStreamEngine));
}

// =====================================================================================================================
// Helper function to optimize pipeline access masks for BLTs. This is for acquire/release interface.
// This function also mask off all graphics path specific stage mask flags for non-universal command buffer as well as
// remove some invalid pfp stage mask on dstStageMask to avoid unnecessary PFP_SYNC_ME stall.
void BarrierMgr::OptimizeStageMask(
    const GfxCmdBuffer* pCmdBuf,
    BarrierType         barrierType,
    uint32*             pSrcStageMask,
    uint32*             pDstStageMask,
    bool                isClearToTarget // Optimization hint.
    ) const
{
    PAL_ASSERT((pSrcStageMask != nullptr) && (pDstStageMask != nullptr));

    const GfxCmdBufferStateFlags stateFlags = pCmdBuf->GetCmdBufState().flags;

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
bool BarrierMgr::OptimizeAccessMask(
    const GfxCmdBuffer* pCmdBuf,
    BarrierType         barrierType,
    const Pal::Image*   pImage,        // Optimization hint
    uint32*             pSrcAccessMask,
    uint32*             pDstAccessMask,
    bool                shaderMdAccessIndirectOnly
    ) const
{
    PAL_ASSERT((pSrcAccessMask != nullptr) && (pDstAccessMask != nullptr));

    const GfxCmdBufferStateFlags stateFlags       = pCmdBuf->GetCmdBufState().flags;
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

// =====================================================================================================================
static void ConvertSyncGlxFlagsToBarrierOps(
    SyncGlxFlags                  acquireCaches,
    Developer::BarrierOperations* pBarrierOps)
{
    pBarrierOps->caches.invalTcp         |= TestAnyFlagSet(acquireCaches, SyncGlvInv);
    pBarrierOps->caches.invalSqI$        |= TestAnyFlagSet(acquireCaches, SyncGliInv);
    pBarrierOps->caches.invalSqK$        |= TestAnyFlagSet(acquireCaches, SyncGlkInv);
    pBarrierOps->caches.invalGl1         |= TestAnyFlagSet(acquireCaches, SyncGl1Inv);
    pBarrierOps->caches.flushTcc         |= TestAnyFlagSet(acquireCaches, SyncGl2Wb);
    pBarrierOps->caches.invalTcc         |= TestAnyFlagSet(acquireCaches, SyncGl2Inv);
    pBarrierOps->caches.invalTccMetadata |= TestAnyFlagSet(acquireCaches, SyncGlmInv);
}

// =====================================================================================================================
// Generate required cache ops for single global/buffer/image transition on Release, Acquire and ReleaseThenAcquire call.
// Return required cache operations (SyncGlxFlags and if need sync RB cache).
CacheSyncOps BarrierMgr::GetCacheSyncOps(
    GfxCmdBuffer*     pCmdBuf,
    BarrierType       barrierType,
    const ImgBarrier* pImgBarrier,
    uint32            srcAccessMask, // Bitmask of CacheCoherencyUsageFlags
    uint32            dstAccessMask, // Bitmask of CacheCoherencyUsageFlags
    bool              shaderMdAccessIndirectOnly
    ) const
{
    const auto*  pImage           = static_cast<const Pal::Image*>((pImgBarrier != nullptr) ? pImgBarrier->pImage
                                                                                            : nullptr);
    const uint32 orgSrcAccessMask = srcAccessMask;
    CacheSyncOps cacheOps         = {};

    // Optimize BLT coherency flags into explicit flags.
    if (OptimizeAccessMask(pCmdBuf, barrierType, pImage, &srcAccessMask, &dstAccessMask, shaderMdAccessIndirectOnly))
    {
        cacheOps.glxFlags |= SyncGl2WbInv;
    }

    // Assume global transition resources have metadata in worst case for safe.
    const bool mayHaveMetadata = (barrierType == BarrierType::Global) || ((pImage != nullptr) && pImage->HasMetadata());

    // V$ and GL1 partial cache line writes to GL2 via byte enables/mask.
    // - If this is an image with DCC, GL2 does the RMW: decompress, update with new data, and then recompress.
    // - If this is an image with HTILE, GL2 does the RMW: decompress, update with new data, no recompress since GL2
    // doesn't support Z plane compression and driver needs explicit resummarization (due to HiZ/HiS is not updated).
    //
    // So it's safe to skip L0/L1 invalidation when transition to shader write as newly written data will override
    // old data in cache line (it doesn't matter even if old data in cache line is stale). However, if the transition
    // has metadata, decompress before GL2 RMW may read data from M$, so still need invalidate metadata cache.
    //
    // Note that CoherClear is a special case as it may contain implicit CoherShaderRead if this is a tile stencil
    // enabled image and only clear depth or stencil plane; in which case RPM compute shader will read HTILE values,
    // modify cleared plane related HTILE values (e.g. if clear stencil, keep ZMask/ZRange/ZPrecision unchanged and only
    // change SMEM) and then write out. This is already handled in FastDepthStencilClearComputeCommon(), no worry here.

    // For transition without metadata, need invalidate L0/L1 caches when about to be shader read. For transition with
    // metadata, need invalidate L0/L1/M$ caches when about to be shader accessed. Strictly speaking, no need invalidate
    // L0/L1 for transition to shader write only case; for simple, make the same logic as transition to shader read case
    // as it doesn't increase packet and cost of inv L0/L1 is light.
    const uint32 dstCacheInvMask = mayHaveMetadata ? CacheCoherShaderAccessMask : CacheCoherShaderReadMask;

    // Optimization: can skip shader source caches (L0/L1/M$) invalidation if previously read through shader source
    //               caches and about to access through shader source caches again. Don't apply the optimization on
    //               global transition.
    // Note that use orgSrcAccessMask instead of srcAccessMask to check if can skip shader source cache invalidation
    // since it can skip more cases safely. srcAccessMask from OptimizeAccessMask() may convert CoherCopySrc to CoherCp
    // and can't skip here but it's safe to skip here.
    //
    // GL0/GL1 cache is tied to view format and view type. When memory is accessed through GL0/GL1 with different
    // view types (e.g. image vs buffer) or two image views with different bits-per-element, the GL0/GL1 layout will
    // be inconsistent and cache invalidation is required in-between.
    //
    //  - For clone CopySrc <-> ShaderRead, need inv GL0/GL1.
    //  - For clone CopySrc -> ShaderWrite, no need cache inv as V$ and GL1 partial cache line writes to GL2 via byte
    //    enables/mask. Similarly transition to clone CopyDst doesn't read and inv GL0/GL1.
    //  - For ShaderWrite -> clone CopySrc, will always inv GL0/GL1 regardless of clone CopySrc or common CopySrc.
    //  - For clone CopyDst -> ShadeRead, always inv GL0/GL1 regardless of clone CopyDst or common CopyDst.
    const bool noSkipCacheInv = (pImage != nullptr) && pImage->IsCloneable()                 &&
                                ((TestAnyFlagSet(orgSrcAccessMask, CoherCopySrc)             &&
                                  TestAnyFlagSet(dstAccessMask, CacheCoherShaderReadMask))   ||
                                 (TestAnyFlagSet(orgSrcAccessMask, CacheCoherShaderReadMask) &&
                                  TestAnyFlagSet(dstAccessMask, CoherCopySrc)));
    const bool canSkipCacheInv = (barrierType != BarrierType::Global)                             &&
                                 (noSkipCacheInv == false)                                        &&
                                 (TestAnyFlagSet(orgSrcAccessMask, CacheCoherWriteMask) == false) &&
                                 TestAnyFlagSet(orgSrcAccessMask, CacheCoherShaderReadMask);

    if (TestAnyFlagSet(dstAccessMask, dstCacheInvMask) && (canSkipCacheInv == false))
    {
        cacheOps.glxFlags |= SyncGlkInv | SyncGlvInv | SyncGl1Inv | SyncGlmInv;
    }

    if (TestAnyFlagSet(dstAccessMask, CoherCpu | CoherMemory | CoherPresent))
    {
        // Split Release/Acquire is built around GL2 being the LLC where data is either released to or acquired from.
        // The Cpu, Memory, and Present usages are special because they do not go through GL2. Therefore, when acquiring
        // the Cpu, Memory, or Present usages we must WB GL2 so all prior writes are visible to those usages that will
        // read directly from memory.
        cacheOps.glxFlags |= SyncGl2Wb;
    }

    if (TestAnyFlagSet(srcAccessMask, CoherCpu | CoherMemory))
    {
        // Split Release/Acquire is built around GL2 being the LLC where data is either released to or acquired from.
        // The Cpu and Memory usages are special because they do not go through GL2. Therefore, when releasing the
        // Cpu or Memory usage, where memory may have been updated directly, we must INV GL2 to ensure those updates
        // will be properly fetched into GL2 for the next GPU usage that is acquired.
        cacheOps.glxFlags |= SyncGl2Inv;
    }

    // Use CACHE_FLUSH_AND_INV_TS to sync RB cache. There is no way to INV the CB metadata caches during acquire.
    // So at release always also invalidate if we are to flush CB metadata. Furthermore, CACHE_FLUSH_AND_INV_TS_EVENT
    // always flush and invalidate RB cache, so there is no need to invalidate RB at acquire again.
    //
    // dstAccessMask == 0 is for split barrier, assume the worst case.
    // Skip RB cache sync for back to back color or depth stencil write.
    if (TestAnyFlagSet(srcAccessMask, CacheCoherRbAccessMask) &&
        ((dstAccessMask == 0) || TestAnyFlagSet(srcAccessMask | dstAccessMask, ~CacheCoherRbAccessMask)))
    {
        cacheOps.rbCache = true;
    }

    // The driver assumes that all meta-data surfaces are channel-aligned, but there are cases where the HW does not
    // actually channel-align the data.  In these cases, the L2 cache needs to be flushed and invalidated prior to the
    // metadata being read by a shader.
    //
    // 1). Direct metadata access mode:
    //   - Accessed as metadata for color target and depth stencil target.
    //   - Accessed directly by shader read or write, like Cs fast clear metadata, copy or fixup metadata.
    // 2). Indirect metadata access mode:
    //   - Accessed as metadata for shader resource or UAV resource.
    //
    // The workaround requires inserting L2 flush and invalidation when transition between direct mode and indirect
    // mode. For split barrier, unfortunately not both srcAccessMask and dstAccessMask are available in either
    // CmdRelease() or CmdAcquire() call. A different solution is to refresh L2 at any cache write in CmdRelease().
    //
    // L2 refresh can be optimized to be skipped for back to back same access mode.
    constexpr uint32 WaRefreshTccCoherMask = CoherColorTarget | CoherShaderWrite | CoherDepthStencilTarget |
                                             CoherCopyDst     | CoherResolveDst  | CoherClear | CoherPresent;

    // Note that use orgSrcAccessMask instead of srcAccessMask for safe here since when clearing blt write cache flags
    // like gfxWriteCachesDirty/csWriteCachesDirty/cpWriteCachesDirty, we don't check if issued a GL2 sync there. That
    // indicates even if blt write cache dirty flags are cleared, we may still need sync GL2 for the workaround.
    // Misaligned metadata workaround for orgSrcAccessMask's BltDst flags have been handled in OptimizeAccessMask().

    // Mask off buffer only coherency flags that never applied to image.
    // Mask off BltDst flags that are already processed in OptimizeAccessMask().
    const auto*  pGfx9Image      = (pImage != nullptr) ? static_cast<const Image*>(pImage->GetGfxImage()) : nullptr;
    const uint32 waSrcAccessMask = orgSrcAccessMask & ~(CoherBufferOnlyMask | CacheCoherencyBltDst);

    if (TestAnyFlagSet(waSrcAccessMask, WaRefreshTccCoherMask) &&
        ((barrierType == BarrierType::Global) ||
         ((pImage != nullptr) && pGfx9Image->NeedFlushForMetadataPipeMisalignment(pImgBarrier->subresRange))))
    {
        const uint32 waDstAccessMask = dstAccessMask & ~CoherBufferOnlyMask;

        const bool backToBackDirectWrite =
            (waSrcAccessMask == waDstAccessMask) &&
            ((waDstAccessMask == CoherColorTarget) || (waDstAccessMask == CoherDepthStencilTarget));

        // For CoherShaderWrite from image layout transition blt, it doesn't exactly indicate an indirect write
        // mode as image layout transition blt may direct write to fix up metadata. optimizeBacktoBackShaderWrite
        // makes sure when it's safe to optimize it.
        const bool backToBackIndirectWrite =
            shaderMdAccessIndirectOnly                        &&
            TestAnyFlagSet(waSrcAccessMask, CoherShaderWrite) &&
            TestAnyFlagSet(waDstAccessMask, CoherShaderWrite) &&
            (TestAnyFlagSet(waSrcAccessMask | waDstAccessMask, ~CoherShader) == false);

        // Can optimize to skip L2 refresh for back to back write with same access mode
        if ((backToBackDirectWrite == false) && (backToBackIndirectWrite == false))
        {
            cacheOps.glxFlags |= SyncGl2WbInv;
        }
    }

    return cacheOps;
}

// =====================================================================================================================
static ReleaseEvents GetReleaseEvents(
    GfxCmdBuffer*    pCmdBuf,
    uint32           srcStageMask,
    CacheSyncOps     cacheOps,
    bool             splitBarrier,
    AcquirePoint     acquirePoint  = AcquirePointPfp) // Assume worst stall if info not available in split barrier
{
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

    if (TestAnyFlagSet(srcStageMask, StallReqStageMask[acquirePoint]))
    {
        if (TestAnyFlagSet(srcStageMask, EopWaitStageMask))
        {
            release.eop = 1;
        }
        else
        {
            release.ps = TestAnyFlagSet(srcStageMask, PsWaitStageMask);
            release.cs = TestAnyFlagSet(srcStageMask, CsWaitStageMask);
            release.vs = TestAnyFlagSet(srcStageMask, VsWaitStageMask);
        }
    }

    if (cacheOps.rbCache)
    {
        release.eop     = 1;
        release.rbCache = 1;
    }

    // Optimization: for stageMask transition Ps|Cs->Rt/Ps/Ds with GCR operation or Vs|Ps|Cs->Rt/Ps/Ds, convert
    //               release events to Eop so can wait at a later PreColor/PrePs/PreDepth point instead of ME.
    if (pCmdBuf->GetDevice().UsePws(pCmdBuf->GetEngineType()) &&
        (release.ps && release.cs)                            &&
        (release.vs || (cacheOps.glxFlags != SyncGlxNone))    &&
        (acquirePoint >= AcquirePointPreDepth))
    {
        release.eop = 1;
    }

    if (splitBarrier)
    {
        // No VS_DONE event support from HW yet. For ReleaseThenAcquire, can issue VS_PARTIAL_FLUSH instead and for
        // split barrier, need bump to Eop instead.
        release.eop |= release.vs;

        // Combine two events to single event
        release.eop |= (release.ps & release.cs);
    }
    else
    {
        // If acquire at bottom of pipe and no cache sync, can skip the release/acquire.
        if ((acquirePoint == AcquirePointEop) && (release.rbCache == 0) && (cacheOps.glxFlags == SyncGlxNone))
        {
            release.eop = 0;
            release.ps  = 0;
            release.vs  = 0;
            release.cs  = 0;
        }
    }

    if (release.eop)
    {
        release.ps = 0;
        release.vs = 0;
        release.cs = 0;
    }

    return release;
}

// =====================================================================================================================
AcquirePoint BarrierMgr::GetAcquirePoint(
    uint32     dstStageMask,
    EngineType engineType
    ) const
{
    // Constants to map PAL interface pipe stage masks to HW acquire points.
    // In theory, no need PfpSyncMe if both srcStageMask and dstStageMask access stage flag in PipelineStagePfpMask.
    // But it's unsafe to optimize it here as srcStageMask and dstStageMask are combination of multiple transitions.
    // e.g. CmdReleaseThenAcquire() is called with two buffer transitions: one is from Cs(Uav)->CsIndirectArgs and
    // the other is from CsIndirectArgs->Cs(ShaderRead); we should NOT skip PFP_SYNC_ME in this case although we see
    // srcStageMask -> dstSrcStageMask = Cs|IndirectArgs -> Cs|IndirectArgs.
    constexpr uint32 AcqPfpStages       = PipelineStagePfpMask;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 835
    // In DX12 conformance test, a buffer is bound as color target, cleared, and then bound as stream out
    // bufferFilledSizeLocation, where CmdLoadBufferFilledSizes() will be called to set this buffer with
    // STRMOUT_BUFFER_FILLED_SIZE (e.g. from control buffer for NGG-SO) via CP ME.
    // In CmdDrawOpaque(), bufferFilleSize allocation will be loaded by LOAD_CONTEXT_REG_INDEX packet via PFP to
    // initialize register VGT_STRMOUT_DRAW_OPAQUE_BUFFER_FILLED_SIZE. PFP_SYNC_ME is issued before load packet so
    // we're safe to acquire at ME stage here.
    constexpr uint32 AcqMeStages        = PipelineStagePostPrefetch | PipelineStageBlt | PipelineStageStreamOut |
                                          PipelineStageVs           | PipelineStageHs  | PipelineStageDs        |
                                          PipelineStageGs           | PipelineStageCs;
    constexpr uint32 AcqPreDepthStages  = PipelineStageSampleRate   | PipelineStageDsTarget | PipelineStagePs |
                                          PipelineStageColorTarget;
#else
    constexpr uint32 AcqMeStages        = PipelineStageBlt | PipelineStageStreamOut | PipelineStageVs |
                                          PipelineStageHs  | PipelineStageDs        | PipelineStageGs |
                                          PipelineStageCs;
    constexpr uint32 AcqPreDepthStages  = PipelineStageDsTarget | PipelineStagePs | PipelineStageColorTarget;
#endif

    // Convert global dstStageMask to HW acquire point.
    AcquirePoint acqPoint = (dstStageMask & AcqPfpStages)      ? AcquirePointPfp      :
                            (dstStageMask & AcqMeStages)       ? AcquirePointMe       :
                            (dstStageMask & AcqPreDepthStages) ? AcquirePointPreDepth :
                                                                 AcquirePointEop;

    // Disable PWS late acquire point if PWS is not disabled or late acquire point is disallowed.
    if (((acqPoint == AcquirePointPreDepth) && (m_pDevice->UsePwsLateAcquirePoint(engineType) == false)) ||
        ((acqPoint == AcquirePointPfp) && (engineType != EngineTypeUniversal))) // No Pfp on non-universal engine.
    {
        acqPoint = AcquirePointMe;
    }

    return acqPoint;
}

// =====================================================================================================================
static ME_ACQUIRE_MEM_pws_stage_sel_enum GetPwsStageSel(
    AcquirePoint acquirePoint)
{
    static constexpr ME_ACQUIRE_MEM_pws_stage_sel_enum PwsStageSelMapTable[] =
    {
        pws_stage_sel__me_acquire_mem__cp_pfp__GFX11,    // AcquirePointPfp      = 0
        pws_stage_sel__me_acquire_mem__cp_me__GFX11,     // AcquirePointMe       = 1
        pws_stage_sel__me_acquire_mem__pre_depth__GFX11, // AcquirePointPreDepth = 2
        pws_stage_sel__me_acquire_mem__pre_depth__GFX11  // AcquirePointEop      = 3
    };

    static_assert((ArrayLen32(PwsStageSelMapTable) == AcquirePointCount), "Need update above mapping table!");

    PAL_ASSERT(acquirePoint < AcquirePointCount);

    return PwsStageSelMapTable[acquirePoint];
}

// =====================================================================================================================
// Fill in a given BarrierOperations struct with info about a layout transition
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 902
static void AcqRelBuildTransition(
#else
static BarrierTransition AcqRelBuildTransition(
#endif
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
    case ExpandHtileHiZRange:
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

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 902
    BarrierTransition out = {};
    out.srcCacheMask                 = pBarrier->srcAccessMask;
    out.dstCacheMask                 = pBarrier->dstAccessMask;
    out.imageInfo.pImage             = pBarrier->pImage;
    out.imageInfo.subresRange        = pBarrier->subresRange;
    out.imageInfo.oldLayout          = pBarrier->oldLayout;
    out.imageInfo.newLayout          = pBarrier->newLayout;
    out.imageInfo.pQuadSamplePattern = pBarrier->pQuadSamplePattern;

    return out;
#endif
}

// =====================================================================================================================
// Update DCC state metadata, which must be done after barrier sync to avoid racing issue (e.g. alias barrier).
// For split barrier, only need update at either Release or Acquire side, preferred Acquire side. Note that alias
// barrier should not be a split barrier.
void BarrierMgr::PostSyncUpdateDccStateMetaData(
    GfxCmdBuffer*                 pCmdBuf,
    CmdStream*                    pCmdStream,
    const AcqRelTransitionInfo&   transitonInfo,
    Developer::BarrierOperations* pBarrierOps)
{
    for (uint32 i = 0; i < transitonInfo.updateDccStateCount; i++)
    {
        const ImgBarrier& imgBarrier = *(*transitonInfo.pList)[i].pUpdateDccStateBarrier;
        const auto&       palImage   = static_cast<const Pal::Image&>(*imgBarrier.pImage);
        const auto&       gfx9Image  = static_cast<const Image&>(*palImage.GetGfxImage());

        gfx9Image.UpdateDccStateMetaData(pCmdStream,
                                         imgBarrier.subresRange,
                                         true,
                                         pCmdBuf->GetEngineType(),
                                         Pm4Predicate::PredDisable);
    }

    if (transitonInfo.updateDccStateCount > 0)
    {
        pBarrierOps->layoutTransitions.updateDccStateMetadata = 1;
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
            *pStageMask  = PipelineStageDsTarget;
            *pAccessMask = CoherDepthStencilTarget;
        }
        break;

    case HwLayoutTransition::ExpandHtileHiZRange:
    case HwLayoutTransition::MsaaColorDecompress:
        *pStageMask  = PipelineStageCs;
        *pAccessMask = CoherShader;
        break;

    case HwLayoutTransition::InitMaskRam:
        // DX12's implicit alias barrier in DiscardResource() will trigger InitMaskRam and fall into here.
        // InitMaskRam contains an internal dispatch call to clear MaskRam to uncompressed state and PFP
        // DMA update to addressing equation and DCC state metadata memory. Returning *pStageMask=PipelineStageCs
        // will result in potential racing issue when PWS is enabled, since pre-InitMaskRam-barrier
        // (Release(Eop_TS)->Acquire(PreShader) and the following PFP_SYNC_ME inside InitMaskRam() can't guarantee
        // previous draw finish using the aliased memory before PFP DMA updates new content to the same memory.
        // We need pre-InitMaskRam-barrier wait at the ME (PostPrefetch) to avoid the racing issue here.
        //
        // No need worry additional overhead in post-InitMaskRam-barrier since pre-InitMaskRam-barrier can clear
        // all outstanding blt active flags before InitMaskRam.
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 835
        *pStageMask  = PipelineStagePostPrefetch;
#else
        *pStageMask  = PipelineStageBlt;
#endif
        *pAccessMask = CoherShader;
        break;

    case HwLayoutTransition::ResummarizeDepthStencil:
        *pStageMask  = PipelineStageDsTarget;
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
// Optimize read only barrier by modify its srcStageMask/dstStageMask to reduce stall operations.
// If cache operations are not required, set srcAccessMask/dstAccessMask to 0 as well.
void BarrierMgr::OptimizeReadOnlyBarrier(
    GfxCmdBuffer*     pCmdBuf,
    const ImgBarrier* pImgBarrier,
    uint32*           pSrcStageMask,
    uint32*           pDstStageMask,
    uint32*           pSrcAccessMask,
    uint32*           pDstAccessMask
    ) const
{
    // Don't skip if may miss shader cache invalidation for DST.
    const CacheSyncOps cacheOps = GetCacheSyncOps(pCmdBuf,
                                                  ((pImgBarrier != nullptr) ? BarrierType::Image : BarrierType::Buffer),
                                                  pImgBarrier,
                                                  *pSrcAccessMask,
                                                  *pDstAccessMask,
                                                  true); // shaderMdAccessIndirectOnly
    bool canSkip = (cacheOps == NullCacheSyncOps);

    // Don't skip if last barrier acquires later stage than current barrier acquires.
    const EngineType engineType = pCmdBuf->GetEngineType();
    canSkip &= (GetAcquirePoint(*pSrcStageMask, engineType) <= GetAcquirePoint(*pDstStageMask, engineType));

    if (canSkip)
    {
        // Compute optimized srcStageMask to release.
        //
        // e.g. PS|CS ShaderRead -> CS ShaderRead -> ColorTarget, can optimize to only release srcStageMask= PS as
        // CS will be released in the second transition.
        constexpr uint32 ReleaseVsStages = PipelineStageVs | PipelineStageHs | PipelineStageDs | PipelineStageGs |
                                           PipelineStageFetchIndices | PipelineStageStreamOut;

        uint32 optSrcStageMask = (*pSrcStageMask & ~*pDstStageMask);

        // To handle case like, srcStageMask has PipelineStageVs but dstStageMask has PipelineStageGs set only.
        // Should be safe to remove PipelineStageVs from srcStageMask in this case.
        optSrcStageMask &= ~(TestAnyFlagSet(*pDstStageMask, ReleaseVsStages) ? ReleaseVsStages : 0UL);

        optSrcStageMask &= ~(pCmdBuf->AnyBltActive() ? 0UL : PipelineStageBlt);

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 835
        // Remove TopOfPipe, FetchIndirectArgs and PipelineStagePostPrefetch as they don't cause stall.
        // PipelineStageFetchIndices needs stall VS.
        optSrcStageMask &= ~(PipelineStageTopOfPipe | PipelineStageFetchIndirectArgs | PipelineStagePostPrefetch);
#else
        // Remove TopOfPipe and FetchIndirectArgs as they don't cause stall. PipelineStageFetchIndices needs stall VS.
        optSrcStageMask &= ~(PipelineStageTopOfPipe | PipelineStageFetchIndirectArgs);
#endif

        *pSrcStageMask = optSrcStageMask;

        // Completely remove all of the barrier operations if optSrcStageMask doesn't need release anything.
        if (*pSrcStageMask == 0)
        {
            *pDstStageMask  = 0;
            *pSrcAccessMask = 0;
            *pDstAccessMask = 0;
        }
    }
}

// =====================================================================================================================
// Optimize image barrier by modify its srcStageMask/dstStageMask/srcAccessMask/dstAccessMask to reduce stall and
// cache operations.
// Return SyncRbFlags to be async released pre blt.
SyncRbFlags BarrierMgr::OptimizeImageBarrier(
    GfxCmdBuffer*               pCmdBuf,
    ImgBarrier*                 pImgBarrier,  // [In/Out]: return optimized stage/access masks.
    const LayoutTransitionInfo& layoutTransInfo,
    uint32                      bltStageMask,
    uint32                      bltAccessMask
    ) const
{
    SyncRbFlags releaseRbFlags = SyncRbNone;

    if (layoutTransInfo.blt[0] == HwLayoutTransition::None)
    {
        // Can safely skip transition between depth read and depth write.
        if ((pImgBarrier->srcAccessMask == CoherDepthStencilTarget) &&
            (pImgBarrier->dstAccessMask == CoherDepthStencilTarget))
        {
            pImgBarrier->srcStageMask  = 0;
            pImgBarrier->dstStageMask  = 0;
            pImgBarrier->srcAccessMask = 0;
            pImgBarrier->dstAccessMask = 0;
        }
        else if (IsReadOnlyTransition(pImgBarrier->srcAccessMask, pImgBarrier->dstAccessMask))
        {
            OptimizeReadOnlyBarrier(pCmdBuf,
                                    pImgBarrier,
                                    &pImgBarrier->srcStageMask,
                                    &pImgBarrier->dstStageMask,
                                    &pImgBarrier->srcAccessMask,
                                    &pImgBarrier->dstAccessMask);
        }
    }
    else
    {
        if (bltAccessMask == CoherColorTarget)
        {
            const auto& gfx9Device = *static_cast<Device*>(m_pGfxDevice);
            const auto* pImage     = static_cast<const Pal::Image*>(pImgBarrier->pImage);
            const auto& gfx9Image  = static_cast<Image&>(*pImage->GetGfxImage());

            // If the image was previously in RB cache, and an incoming decompress operation can be pipelined.
            if (layoutTransInfo.blt[0] == HwLayoutTransition::DccDecompress)
            {
                releaseRbFlags = gfx9Device.Settings().waDccCacheFlushAndInv ? SyncCbWbInv : SyncRbNone;
            }
            else if (layoutTransInfo.blt[0] == HwLayoutTransition::FastClearEliminate)
            {
                releaseRbFlags = (gfx9Device.Settings().waDccCacheFlushAndInv &&
                                  gfx9Image.HasDccData()) ? SyncCbWbInv : SyncRbNone;
            }
            else if (layoutTransInfo.blt[0] == HwLayoutTransition::FmaskDecompress)
            {
                releaseRbFlags = SyncCbWbInv;
            }
        }
        else if (bltAccessMask == CoherDepthStencilTarget)
        {
            if (IsGfx10(*m_pDevice))
            {
                releaseRbFlags = SyncDbWbInv;
            }
        }

        // Can safely skip pre-blt sync for pipelined blt, avoid sync by setting srcStageMask/srcAccessMask to 0.
        if ((pImgBarrier->srcStageMask == bltStageMask)   &&
            (pImgBarrier->srcAccessMask == bltAccessMask) &&
            ((bltStageMask  == PipelineStageColorTarget)  ||
             (bltStageMask  == PipelineStageDsTarget))    &&
            ((bltAccessMask == CoherColorTarget) ||
             (bltAccessMask == CoherDepthStencilTarget)))
        {
            pImgBarrier->srcStageMask  = 0;
            pImgBarrier->srcAccessMask = 0;
        }
    }

    return releaseRbFlags;
}

// =====================================================================================================================
// Prepare and get all image layout transition info.
// Return collected OR'ed SRC/DST stage masks and required cache operations (SyncGlxFlags and if need sync RB cache).
BarrierMgr::AcqRelImageSyncInfo BarrierMgr::GetAcqRelLayoutTransitionBltInfo(
    GfxCmdBuffer*                 pCmdBuf,
    CmdStream*                    pCmdStream,
    const AcquireReleaseInfo&     barrierInfo,
    AcqRelTransitionInfo*         pTransInfo,   // (out) Layout transition barrier info
    Developer::BarrierOperations* pBarrierOps   // (out) Developer barrier operations
    ) const
{
    // Assert caller has initialized all members of pTransInfo.
    PAL_ASSERT((pTransInfo != nullptr) && (pTransInfo->pList != nullptr) &&
               (pTransInfo->bltCount == 0) && (pTransInfo->bltStageMask == 0) &&
               (pTransInfo->updateDccStateCount == 0));

    SyncRbFlags         releaseRbFlags = SyncRbNone; // RB cache flags to be async released pre blt.
    AcqRelImageSyncInfo syncInfo       = {};

    // Loop through image transitions to update client requested access.
    for (uint32 i = 0; i < barrierInfo.imageBarrierCount; i++)
    {
        ImgBarrier imageBarrier = barrierInfo.pImageBarriers[i];

        PAL_ASSERT(imageBarrier.subresRange.numPlanes == 1);

        // Prepare a layout transition BLT info and do pre-BLT preparation work.
        const LayoutTransitionInfo layoutTransInfo = PrepareBltInfo(pCmdBuf, imageBarrier);
        const bool                 hasBlt          = (layoutTransInfo.blt[0] != HwLayoutTransition::None);
        uint32                     bltStageMask    = 0;
        uint32                     bltAccessMask   = 0;

        if (hasBlt)
        {
            GetBltStageAccessInfo(layoutTransInfo, &bltStageMask, &bltAccessMask);

            // Pass imageBarrier before optimized.
            (*pTransInfo->pList)[pTransInfo->bltCount].bltBarrier      = imageBarrier;
            (*pTransInfo->pList)[pTransInfo->bltCount].layoutTransInfo = layoutTransInfo;
            (*pTransInfo->pList)[pTransInfo->bltCount].stageMask       = bltStageMask;
            (*pTransInfo->pList)[pTransInfo->bltCount].accessMask      = bltAccessMask;
            pTransInfo->bltCount++;

            // OR current BLT's stageMask/accessMask into a global mask used for an all-in-one pre-BLT acquire.
            pTransInfo->bltStageMask |= bltStageMask;
        }

        // Optimize transition stageMask/accessMask to reduce potential stall and cache operations.
        releaseRbFlags |= OptimizeImageBarrier(pCmdBuf, &imageBarrier, layoutTransInfo, bltStageMask, bltAccessMask);

        // Minor optimization: set transition datAccessMask to 0 for InitMaskRam to avoid unneeded cache sync.
        const bool   initMaskRam   = (layoutTransInfo.blt[0] == HwLayoutTransition::InitMaskRam);
        const uint32 dstAccessMask = (hasBlt ? (initMaskRam ? 0 : bltAccessMask) : imageBarrier.dstAccessMask);
        const uint32 syncIdx       = hasBlt ? WithLayoutTransBlt : WithoutLayoutTransBlt;

        syncInfo.cacheOps[syncIdx] |=
            GetImageCacheSyncOps(pCmdBuf,
                                 &imageBarrier,
                                 imageBarrier.srcAccessMask,
                                 dstAccessMask,
                                 (hasBlt ? false : true)); // shaderMdAccessIndirectOnly, assume potential metadata
                                                           // direct access in blt.

        // Optimize image stage masks before OR to global stage mask.
        const bool isClearToTarget = IsClearToTargetTransition(imageBarrier.srcAccessMask, imageBarrier.dstAccessMask);
        OptimizeStageMask(pCmdBuf,
                          BarrierType::Image,
                          &imageBarrier.srcStageMask,
                          &imageBarrier.dstStageMask,
                          isClearToTarget);

        syncInfo.srcStageMask[syncIdx] |= imageBarrier.srcStageMask;
        syncInfo.dstStageMask          |= imageBarrier.dstStageMask;

        // Check if need UpdateDccStateMetaData; if yes, insert to tracked list and defer to update after barrier sync
        // is done. InitMaskRam has UpdateDccStateMetaData inside so no need check here again.
        //
        // In DX12, hit case that one barrier call may have one alias barrier on one resource and followed a transition
        // barrier on the same resource from one state to another state (e.g. ShaderRead->ColorTarget), which needs
        // UpdateDccStateMetaData. Need defer UpdateDccStateMetaData after alias barrier sync is done to avoid racing
        // issue.
        if (initMaskRam == false)
        {
            const auto&               palImage      = static_cast<const Pal::Image&>(*imageBarrier.pImage);
            const auto&               gfx9Image     = static_cast<const Image&>(*palImage.GetGfxImage());
            const ColorLayoutToState& layoutToState = gfx9Image.LayoutToColorCompressionState();

            if (gfx9Image.HasDccStateMetaData(imageBarrier.subresRange) &&
                (ImageLayoutCanCompressColorData(layoutToState, imageBarrier.oldLayout) == false) &&
                (ImageLayoutCanCompressColorData(layoutToState, imageBarrier.newLayout)))
            {
                (*pTransInfo->pList)[pTransInfo->updateDccStateCount++].pUpdateDccStateBarrier =
                    &barrierInfo.pImageBarriers[i];
            }
        }
    }

    if (releaseRbFlags != SyncRbNone)
    {
        VGT_EVENT_TYPE vgtEvent;

        if (TestAnyFlagSet(releaseRbFlags, SyncCbWbInv))
        {
            SetBarrierOperationsRbCacheSynced(pBarrierOps);
            vgtEvent = CACHE_FLUSH_AND_INV_EVENT;
        }
        else
        {
            pBarrierOps->caches.flushDb = 1;
            pBarrierOps->caches.invalDb = 1;
            pBarrierOps->caches.flushDbMetadata = 1;
            pBarrierOps->caches.invalDbMetadata = 1;
            vgtEvent = DB_CACHE_FLUSH_AND_INV;
        }

        uint32* pCmdSpace = pCmdStream->ReserveCommands();
        pCmdSpace += m_cmdUtil.BuildNonSampleEventWrite(vgtEvent, pCmdBuf->GetEngineType(), pCmdSpace);
        pCmdStream->CommitCommands(pCmdSpace);
    }

    return syncInfo;
}

// =====================================================================================================================
// Issue all image layout transition BLTs and compute info for release the BLTs.
// Return required cache operations.
CacheSyncOps BarrierMgr::IssueAcqRelLayoutTransitionBlt(
    GfxCmdBuffer*                 pCmdBuf,
    CmdStream*                    pCmdStream,
    AcqRelTransitionInfo*         pTransitonInfo,
    Developer::BarrierOperations* pBarrierOps
    ) const
{
    // If BLTs will be issued, we need to know how to release from it/them.
    uint32 postBltStageMask    = 0;
    bool   preInitHtileSynced  = false;
    bool   postSyncInitMaskRam = false;

    PAL_ASSERT(pTransitonInfo != nullptr);

    CacheSyncOps cacheOps = {};

    // Issue BLTs.
    for (uint32 i = 0; i < pTransitonInfo->bltCount; i++)
    {
        const AcqRelImgTransitionInfo& transition = (*pTransitonInfo->pList)[i];
        const ImgBarrier&              imgBarrier = transition.bltBarrier;

        if (transition.layoutTransInfo.blt[0] != HwLayoutTransition::None)
        {
            const bool csInitMaskRam = IssueBlt(pCmdBuf, pCmdStream, &imgBarrier, transition.layoutTransInfo,
                                                &preInitHtileSynced, pBarrierOps);

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

            // Explicitly requires RB cache sync post blt if this is a graphics layout blt. The stall acquire point
            // will be based on value of dstStageMask. e.g. if dstStageMask is 0 or PipelineStageBottomOfPipe, this
            // will be an async RB cache event (ReleseMem only).
            cacheOps.rbCache |= TestAnyFlagSet(accessMask, CacheCoherRbAccessMask);

            cacheOps |= GetImageCacheSyncOps(pCmdBuf,
                                             &imgBarrier,
                                             accessMask,
                                             imgBarrier.dstAccessMask,
                                             false); // shaderMdAccessIndirectOnly, assume potential metadata
                                                     // direct access in blt.

            if (csInitMaskRam)
            {
                // Post-stall for CsInitMaskRam is handled at end of this function specially, so no need update
                // postBltStageMask to avoid PostBlt sync outside again. Set required cache op in case clients doesn't
                // provide dstAccessMask; defer cache op with syncGlxFlags to be issued at a later time.
                cacheOps.glxFlags  |= SyncGlvInv | SyncGl1Inv | SyncGlmInv;
                postSyncInitMaskRam = true;
            }
            else
            {
                // Add current BLT's stageMask into a stageMask used for an all-in-one post-BLT release.
                postBltStageMask |= stageMask;
            }
        }
    }

    // If clients pass with dstStageMask = PipelineStageBottomOfPipe (may be not aware yet that how this resource will
    // be used in the next access), then the sync of InitMaskRam will not be done. So stall it here immediately.
    // Note that defer the cache operation with syncGlxFlags at a later time.
    if (postSyncInitMaskRam)
    {
        uint32* pCmdSpace = pCmdStream->ReserveCommands();
        pCmdSpace = pCmdBuf->WriteWaitCsIdle(pCmdSpace);
        pCmdStream->CommitCommands(pCmdSpace);

        pBarrierOps->pipelineStalls.csPartialFlush = 1;
    }

    // Output bltStageMask for release BLTs. Generally they're the same as the input values in pTransitonInfo, but if
    // (layoutTransInfo.blt[1] != HwLayoutTransition::None), they may be different and we should update the values to
    // release blt[1] correctly.
    pTransitonInfo->bltStageMask = postBltStageMask;

    return cacheOps;
}

// =====================================================================================================================
// Wrapper to call RPM's InitMaskRam to issues a compute shader blt to initialize the Mask RAM allocations for an Image.
// Returns "true" if the compute engine was last used in this function.
bool BarrierMgr::AcqRelInitMaskRam(
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
    const auto& engineProps = m_pDevice->EngineProperties().perEngine[engineType];

    // This queue must support this barrier transition.
    PAL_ASSERT(engineProps.flags.supportsImageInitBarrier == 1);
#endif

    PAL_ASSERT(gfx9Image.HasColorMetaData() || gfx9Image.HasHtileData());

    bool usedCompute = RsrcProcMgr().InitMaskRam(pCmdBuf,
                                                 pCmdStream,
                                                 gfx9Image,
                                                 subresRange,
                                                 imgBarrier.newLayout);

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 836
    const ColorLayoutToState layoutToState = gfx9Image.LayoutToColorCompressionState();

    if (gfx9Image.HasDisplayDccData() &&
        (ImageLayoutToColorCompressionState(layoutToState, imgBarrier.newLayout) == ColorDecompressed))
    {
        RsrcProcMgr().CmdDisplayDccFixUp(pCmdBuf, image);
        usedCompute = true;
    }
#endif

    return usedCompute;
}

// =====================================================================================================================
// Issue the specified BLT operation(s) (i.e., decompress, resummarize) necessary to convert a depth/stencil image from
// one ImageLayout to another.
void BarrierMgr::AcqRelDepthStencilTransition(
    GfxCmdBuffer*        pCmdBuf,
    const ImgBarrier&    imgBarrier,
    LayoutTransitionInfo layoutTransInfo
    ) const
{
    PAL_ASSERT(imgBarrier.subresRange.numPlanes == 1);
    PAL_ASSERT(imgBarrier.pImage != nullptr);

    const auto& image = static_cast<const Pal::Image&>(*imgBarrier.pImage);

    if (layoutTransInfo.blt[0] == HwLayoutTransition::ExpandHtileHiZRange)
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
void BarrierMgr::AcqRelColorTransition(
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

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 836
            if (gfx9Image.HasDisplayDccData())
            {
                RsrcProcMgr().CmdDisplayDccFixUp(pCmdBuf, image);
            }
#endif
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

            CacheSyncOps cacheOps = GetImageCacheSyncOps(pCmdBuf, &imgBarrier, srcAccessMask, dstAccessMask, false);

            // Explicitly requires RB cache sync post blt[0] if this is a graphics layout blt.
            cacheOps.rbCache |= TestAnyFlagSet(srcAccessMask, CacheCoherRbAccessMask);

            IssueReleaseThenAcquireSync(pCmdBuf, pCmdStream, srcStageMask, dstStageMask, cacheOps, pBarrierOps);

            // Tell RGP about this transition
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 902
            AcqRelBuildTransition(&imgBarrier, MsaaBltInfo, pBarrierOps);
            DescribeBarrier(pCmdBuf, &imgBarrier, pBarrierOps);
#else
            BarrierTransition rgpTransition = AcqRelBuildTransition(&imgBarrier, MsaaBltInfo, pBarrierOps);
            DescribeBarrier(pCmdBuf, &rgpTransition, pBarrierOps);
#endif

            RsrcProcMgr().FmaskColorExpand(pCmdBuf, gfx9Image, imgBarrier.subresRange);
        }
    }
}

// =====================================================================================================================
// Issue appropriate cache sync hardware commands to satisfy the cache release requirements.
ReleaseToken BarrierMgr::IssueReleaseSync(
    GfxCmdBuffer*                 pCmdBuf,
    uint32                        srcStageMask,  // Bitmask of PipelineStageFlag.
    bool                          releaseBufferCopyOnly,
    CacheSyncOps                  cacheOps,
    const GpuEvent*               pClientEvent,
    Developer::BarrierOperations* pBarrierOps
    ) const
{
    const bool          isReleaseTokenPath = (pClientEvent == nullptr);
    const ReleaseEvents releaseEvents      = GetReleaseEvents(pCmdBuf, srcStageMask, cacheOps, true);
    const bool          waitCpDma          = NeedWaitCpDma(pCmdBuf, srcStageMask);
    ReleaseToken        syncToken          = {};

    PAL_ASSERT((pClientEvent == nullptr) || pClientEvent->GetBoundGpuMemory().IsBound());

    // For release sync token path, optimize to defer wait and cache op to acquire side if there is only CpDma wait and
    // this is release buffer copy only.
    // Cache op must happen after stall complete (wait CpDma idle) otherwise cache may be dirty again due to running
    // CpDma blt. Since CpDma wait is deferred, cache op must be deferred to acquire time as well.
    if (isReleaseTokenPath && waitCpDma && releaseBufferCopyOnly && (releaseEvents.u8All == 0))
    {
        syncToken.type       = ReleaseTokenCpDma;
        syncToken.fenceValue = pCmdBuf->GetNextAcqRelFenceVal(ReleaseTokenCpDma);
    }
    else
    {
        const EngineType engineType   = pCmdBuf->GetEngineType();
        CmdStream* const pCmdStream   = GetCmdStream(pCmdBuf);
        uint32*          pCmdSpace    = pCmdStream->ReserveCommands();
        SyncGlxFlags     syncGlxFlags = cacheOps.glxFlags;

        ConvertSyncGlxFlagsToBarrierOps(syncGlxFlags, pBarrierOps);

        const ReleaseMemCaches releaseCaches = m_cmdUtil.SelectReleaseMemCaches(&syncGlxFlags);

        // Make sure all SyncGlxFlags have been converted to ReleaseMemCaches.
        PAL_ASSERT(syncGlxFlags == SyncGlxNone);

        // Pick EOP event if a cache sync is requested because EOS events do not support cache syncs.
        const bool bumpToEop = (releaseCaches.u8All != 0) && (releaseEvents.u8All != 0);

        // Note that release event flags for split barrier should meet below conditions,
        //    1). No VsDone as it should be converted to PsDone or Eop.
        //    2). PsDone and CsDone should have been already converted to Eop.
        //    3). rbCache sync must have Eop event set.
        PAL_ASSERT(releaseEvents.vs == 0);
        PAL_ASSERT((releaseEvents.ps & releaseEvents.cs)== 0);
        PAL_ASSERT((releaseEvents.rbCache == 0) || releaseEvents.eop);

        const ReleaseTokenType eventType = (releaseEvents.eop || bumpToEop) ? ReleaseTokenEop    :
                                           releaseEvents.ps                 ? ReleaseTokenPsDone :
                                           releaseEvents.cs                 ? ReleaseTokenCsDone :
                                                                              ReleaseTokenInvalid;

        bool releaseMemWaitCpDma = false;

        if (waitCpDma)
        {
            // We must guarantee that all prior CP DMA accelerated blts have completed before we write this event because
            // the CmdSetEvent and CmdResetEvent functions expect that the prior blts have reached the post-blt stage by
            // the time the event is written to memory. Given that our CP DMA blts are asynchronous to the pipeline stages
            // the only way to satisfy this requirement is to force the MEC to stall until the CP DMAs are completed.
            if (EnableReleaseMemWaitCpDma() && (eventType != ReleaseTokenInvalid))
            {
                releaseMemWaitCpDma = true;
            }
            else
            {
                pCmdSpace += m_cmdUtil.BuildWaitDmaData(pCmdSpace);
            }

            pBarrierOps->pipelineStalls.syncCpDma = 1;
            pCmdBuf->SetCpBltState(false);
        }

        // Issue RELEASE_MEM packet.
        if (eventType != ReleaseTokenInvalid)
        {
            const VGT_EVENT_TYPE eopEvent = releaseEvents.rbCache ? CACHE_FLUSH_AND_INV_TS_EVENT
                                                                  : BOTTOM_OF_PIPE_TS;

            switch (eventType)
            {
            case ReleaseTokenEop:
                pBarrierOps->pipelineStalls.eopTsBottomOfPipe = 1;
                if (releaseEvents.rbCache)
                {
                    PAL_ASSERT(Pal::Device::EngineSupportsGraphics(engineType));
                    SetBarrierOperationsRbCacheSynced(pBarrierOps);
                }
                break;
            case ReleaseTokenPsDone:
                PAL_ASSERT(Pal::Device::EngineSupportsGraphics(engineType));
                pBarrierOps->pipelineStalls.eosTsPsDone = 1;
                break;
            case ReleaseTokenCsDone:
                pBarrierOps->pipelineStalls.eosTsCsDone = 1;
                break;
            default:
                PAL_ASSERT_ALWAYS();
                break;
            }

            if (isReleaseTokenPath)
            {
                // Request sync fence value after VGT event type is finalized.
                syncToken.type       = eventType;
                syncToken.fenceValue = pCmdBuf->GetNextAcqRelFenceVal(eventType);

                if (Pal::Device::EngineSupportsGraphics(engineType))
                {
                    ReleaseMemGfx releaseMem = {};
                    releaseMem.cacheSync      = releaseCaches;
                    releaseMem.gfx11WaitCpDma = releaseMemWaitCpDma;
                    releaseMem.vgtEvent       = (eventType == ReleaseTokenEop)    ? eopEvent :
                                                (eventType == ReleaseTokenPsDone) ? PS_DONE  : CS_DONE;

                    if (m_pDevice->UsePws(engineType))
                    {
                        releaseMem.usePws = true;
                        if (eventType == ReleaseTokenEop)
                        {
                            releaseMem.dataSel = data_sel__me_release_mem__none;
                        }
                        else
                        {
                            // Note: PWS+ doesn't need timestamp write, we pass in a dummy write just to meet
                            //       RELEASE_MEM packet programming requirement for DATA_SEL field, where 0=none
                            //       (Discard data) is not a valid option when EVENT_INDEX=shader_done(PS_DONE/CS_DONE).
                            releaseMem.dataSel = data_sel__me_release_mem__send_32_bit_low;
                            releaseMem.dstAddr = pCmdBuf->TimestampGpuVirtAddr();
                        }
                    }
                    else
                    {
                        releaseMem.dataSel = data_sel__me_release_mem__send_32_bit_low;
                        releaseMem.dstAddr = pCmdBuf->AcqRelFenceValGpuVa(eventType);
                        releaseMem.data    = syncToken.fenceValue;
                    }

                    pCmdSpace += m_cmdUtil.BuildReleaseMemGfx(releaseMem, pCmdSpace);
                }
                else
                {
                    ReleaseMemGeneric releaseMem = {};
                    releaseMem.cacheSync      = releaseCaches;
                    releaseMem.dstAddr        = pCmdBuf->AcqRelFenceValGpuVa(eventType);
                    releaseMem.dataSel        = data_sel__me_release_mem__send_32_bit_low;
                    releaseMem.data           = syncToken.fenceValue;
                    releaseMem.gfx11WaitCpDma = releaseMemWaitCpDma;

                    pCmdSpace += m_cmdUtil.BuildReleaseMemGeneric(releaseMem, pCmdSpace);
                }
            }
            else // For ReleaseEvent() path
            {
                // Build a WRITE_DATA command to first RESET event slots that will be set by event later on.
                WriteDataInfo writeData = {};
                writeData.engineType    = engineType;
                writeData.engineSel     = engine_sel__me_write_data__micro_engine;
                writeData.dstSel        = dst_sel__me_write_data__memory;
                writeData.dstAddr       = pClientEvent->GetBoundGpuMemory().GpuVirtAddr();
                pCmdSpace += CmdUtil::BuildWriteData(writeData, GpuEvent::ResetValue, pCmdSpace);

                if (Pal::Device::EngineSupportsGraphics(engineType))
                {
                    ReleaseMemGfx releaseMem  = {};
                    releaseMem.cacheSync      = releaseCaches;
                    releaseMem.vgtEvent       = (eventType == ReleaseTokenEop)    ? eopEvent :
                                                (eventType == ReleaseTokenPsDone) ? PS_DONE  : CS_DONE;
                    releaseMem.dstAddr        = writeData.dstAddr;
                    releaseMem.dataSel        = data_sel__me_release_mem__send_32_bit_low;
                    releaseMem.data           = GpuEvent::SetValue;
                    releaseMem.gfx11WaitCpDma = releaseMemWaitCpDma;

                    pCmdSpace += m_cmdUtil.BuildReleaseMemGfx(releaseMem, pCmdSpace);
                }
                else
                {
                    ReleaseMemGeneric releaseMem = {};
                    releaseMem.cacheSync      = releaseCaches;
                    releaseMem.dstAddr        = writeData.dstAddr;
                    releaseMem.dataSel        = data_sel__me_release_mem__send_32_bit_low;
                    releaseMem.data           = GpuEvent::SetValue;
                    releaseMem.gfx11WaitCpDma = releaseMemWaitCpDma;
                    pCmdSpace += m_cmdUtil.BuildReleaseMemGeneric(releaseMem, pCmdSpace);
                }
            }
        }
        else // (eventType == ReleaseTokenInvalid)
        {
            // For ReleaseEvent() path, set event value directly if no valid release events.
            if (isReleaseTokenPath == false)
            {
                WriteDataInfo writeData = {};
                writeData.engineType    = engineType;
                writeData.engineSel     = engine_sel__me_write_data__micro_engine;
                writeData.dstSel        = dst_sel__me_write_data__memory;
                writeData.dstAddr       = pClientEvent->GetBoundGpuMemory().GpuVirtAddr();

                pCmdSpace += CmdUtil::BuildWriteData(writeData, GpuEvent::SetValue, pCmdSpace);
            }

            // No release case (if valid release event and releaseCaches != 0, should be already bumped to EOP and
            // handled in if path).
            if (releaseCaches.u8All != 0)
            {
                // This is an optimization path to use AcquireMem for cache syncs only (no release event case) case as
                // ReleaseMem requires an EOP or EOS event.
                AcquireMemGeneric acqMem = {};
                acqMem.engineType = engineType;
                acqMem.cacheSync  = cacheOps.glxFlags; // Use original glxFlags not processed by SelectReleaseMemCaches.

                pCmdSpace += m_cmdUtil.BuildAcquireMemGeneric(acqMem, pCmdSpace);
            }
        }

        if (isReleaseTokenPath && releaseEvents.rbCache)
        {
            pCmdBuf->UpdateGfxBltWbEopFence(syncToken.fenceValue);
        }

        pCmdStream->CommitCommands(pCmdSpace);
    }

    return syncToken;
}

#if PAL_DEVELOPER_BUILD
// =====================================================================================================================
static Developer::AcquirePoint ConvertToDeveloperAcquirePoint(
    AcquirePoint acqPoint)
{
    Developer::AcquirePoint retPoint;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 901
    switch (acqPoint)
    {
    case AcquirePointPfp:
        retPoint = Developer::AcquirePointPfp;
        break;
    case AcquirePointMe:
        retPoint = Developer::AcquirePointMe;
        break;
    case AcquirePointPreDepth:
        retPoint = Developer::AcquirePointPreDepth;
        break;
    case AcquirePointEop:
        retPoint = Developer::AcquirePointEop;
        break;
    default:
        PAL_ASSERT_ALWAYS(); // Should not hit here.
        break;
    }
#else
    switch (acqPoint)
    {
    case AcquirePointPfp:
        retPoint = Developer::AcquirePoint::Pfp;
        break;
    case AcquirePointMe:
        retPoint = Developer::AcquirePoint::Me;
        break;
    case AcquirePointPreDepth:
        retPoint = Developer::AcquirePoint::PreDepth;
        break;
    case AcquirePointEop:
        retPoint = Developer::AcquirePoint::Eop;
        break;
    default:
        PAL_ASSERT_ALWAYS(); // Should not hit here.
        break;
    }
#endif

    return retPoint;
}
#endif

// =====================================================================================================================
// Issue appropriate cache sync hardware commands to satisfy the cache acquire requirements.
// Note: The list of sync tokens cannot be cleared by this function and make it const. If multiple IssueAcquireSync are
//       called in the for loop, typically we'd think only the first call needs to effectively wait on the token(s).
//       After the first call the tokens have been waited and can be cleared.That's under wrong assumption that all the
//       IssueAcquireSync calls wait at same pipeline point, like PFP / ME. But in reality these syncs can end up wait
//       at different pipeline points. For example one wait at PFP while another wait at ME, or even later in the
//       pipeline. Every IssueAcquireSync needs to effectively wait on the list of release tokens.
void BarrierMgr::IssueAcquireSync(
    GfxCmdBuffer*                 pCmdBuf,
    uint32                        dstStageMask,  // Bitmask of PipelineStageFlag.
    CacheSyncOps                  cacheOps,
    uint32                        syncTokenCount,
    const ReleaseToken*           pSyncTokens,
    Developer::BarrierOperations* pBarrierOps
    ) const
{
    const EngineType engineType   = pCmdBuf->GetEngineType();
    CmdStream* const pCmdStream   = GetCmdStream(pCmdBuf);
    uint32*          pCmdSpace    = pCmdStream->ReserveCommands();
    AcquirePoint     acquirePoint = GetAcquirePoint(dstStageMask, engineType);

    // Indicate if required cacheOps and PFP_SYNC_ME are already issued.
    bool syncCacheAndWaitPfp = false;

    // Handle case with syncTokens. e.g. AccquireEvent won't go into if path.
    if (syncTokenCount > 0)
    {
        for (uint32 i = 0; i < syncTokenCount; i++)
        {
            if (pSyncTokens[i].type == ReleaseTokenCpDma)
            {
                // Append deferred cache op for special CpDma wait case; OR into cacheOps.
                // Only for release buffer copy case so compute required cache operations from releasing buffer copy.
                cacheOps |= GetCacheSyncOps(pCmdBuf, BarrierType::Buffer, nullptr, CoherCopy, 0, false);

                // Wait CpDma only if it's still active.
                if ((pCmdBuf->GetCmdBufState().flags.cpBltActive != 0) &&
                    (pSyncTokens[i].fenceValue > pCmdBuf->GetRetiredAcqRelFenceVal(ReleaseTokenCpDma)))
                {
                    pCmdSpace += m_cmdUtil.BuildWaitDmaData(pCmdSpace);

                    pCmdBuf->SetCpBltState(false);
                    pBarrierOps->pipelineStalls.syncCpDma = 1;
                }
                break;
            }
        }

        // Must acquire at PFP/ME if cache syncs are required.
        if ((cacheOps.glxFlags != SyncGlxNone) && (acquirePoint > AcquirePointMe) && (acquirePoint != AcquirePointEop))
        {
            acquirePoint = AcquirePointMe;
        }

        uint32 syncTokenToWait[ReleaseTokenCount] = {};
        bool   hasValidSyncToken = false;

        // Merge synchronization timestamp entries in the list. Can safely skip Acquire if acquire point is EOP and
        // no cache sync. If there is cache sync, acquire point has been forced to ME by above codes.
        if (acquirePoint != AcquirePointEop)
        {
            for (uint32 i = 0; i < syncTokenCount; i++)
            {
                const ReleaseToken token = pSyncTokens[i];

                if ((token.type < ReleaseTokenCpDma) &&
                    (token.fenceValue > pCmdBuf->GetRetiredAcqRelFenceVal(ReleaseTokenType(token.type))))
                {
                    syncTokenToWait[token.type] = Max(token.fenceValue, syncTokenToWait[token.type]);
                    hasValidSyncToken = true;
                }
            }
        }

        if (hasValidSyncToken)
        {
            if (pCmdBuf->GetDevice().UsePws(engineType))
            {
                // Maximum number of PWS-enabled pipeline events that PWS+ supported engine can track.
                constexpr uint32 MaxNumPwsSyncEvents = 64;

                // Wait on the PWS+ event via ACQUIRE_MEM.
                AcquireMemGfxPws acquireMem = {};
                acquireMem.cacheSync = cacheOps.glxFlags;
                acquireMem.stageSel  = GetPwsStageSel(acquirePoint);

                static_assert((ReleaseTokenEop    == uint32(pws_counter_sel__me_acquire_mem__ts_select__GFX11)) &&
                              (ReleaseTokenPsDone == uint32(pws_counter_sel__me_acquire_mem__ps_select__GFX11)) &&
                              (ReleaseTokenCsDone == uint32(pws_counter_sel__me_acquire_mem__cs_select__GFX11)),
                              "Enum orders mismatch! Fix the ordering so the following for-loop runs correctly.");

                for (uint32 i = 0; i < ReleaseTokenCpDma; i++)
                {
                    if (syncTokenToWait[i] != 0)
                    {
                        const uint32 curSyncToken = pCmdBuf->GetCurAcqRelFenceVal(ReleaseTokenType(i));
                        const uint32 numEventsAgo = curSyncToken - syncTokenToWait[i];

                        PAL_ASSERT(syncTokenToWait[i] <= curSyncToken);

                        acquireMem.counterSel = ME_ACQUIRE_MEM_pws_counter_sel_enum(i);
                        acquireMem.syncCount  = Clamp(numEventsAgo, 0U, MaxNumPwsSyncEvents - 1U);

                        pCmdSpace += m_cmdUtil.BuildAcquireMemGfxPws(acquireMem, pCmdSpace);
                    }
                }

                syncCacheAndWaitPfp = true; // PWS ACQUIRE_MEM packet can sync cache and wait at PFP.
            }
            else
            {
                for (uint32 i = 0; i < ReleaseTokenCpDma; i++)
                {
                    if (syncTokenToWait[i] != 0)
                    {
                        pCmdSpace +=
                            CmdUtil::BuildWaitRegMem(engineType,
                                                     mem_space__me_wait_reg_mem__memory_space,
                                                     function__me_wait_reg_mem__greater_than_or_equal_reference_value,
                                                     engine_sel__me_wait_reg_mem__micro_engine,
                                                     pCmdBuf->AcqRelFenceValGpuVa(ReleaseTokenType(i)),
                                                     syncTokenToWait[i],
                                                     UINT32_MAX,
                                                     pCmdSpace);
                    }
                }
            }

            pBarrierOps->pipelineStalls.waitOnTs = 1;

            if (acquirePoint <= AcquirePointMe)
            {
                // Update retired acquire release fence values
                for (uint32 i = 0; i < ReleaseTokenCpDma; i++)
                {
                    pCmdBuf->UpdateRetiredAcqRelFenceVal(ReleaseTokenType(i), syncTokenToWait[i]);
                }

                const GfxCmdBufferState& cmdBufState       = pCmdBuf->GetCmdBufState();
                const uint32             waitedEopFenceVal = syncTokenToWait[ReleaseTokenEop];

                // If we have waited on a valid EOP fence value, update some CmdBufState (e.g. xxxBltActive) flags.
                if (waitedEopFenceVal != 0)
                {
                    pCmdBuf->SetPrevCmdBufInactive();

                    // An EOP release sync that is issued after the latest GFX BLT must have completed, so mark
                    // GFX BLT idle.
                    if (waitedEopFenceVal >= cmdBufState.fences.gfxBltExecEopFenceVal)
                    {
                        pCmdBuf->SetGfxBltState(false);
                    }

                    // An EOP release sync that issued GFX BLT cache flush must have completed, so mark GFX BLT
                    // cache clean.
                    if (waitedEopFenceVal >= cmdBufState.fences.gfxBltWbEopFenceVal)
                    {
                        pCmdBuf->SetGfxBltWriteCacheState(false);
                    }
                }

                // An EOP or CS_DONE release sync that is issued after the latest CS BLT must have completed, so mark
                // CS BLT idle.
                if ((waitedEopFenceVal >= cmdBufState.fences.csBltExecEopFenceVal) ||
                    (syncTokenToWait[ReleaseTokenCsDone] >= cmdBufState.fences.csBltExecCsDoneFenceVal))
                {
                    pCmdBuf->SetCsBltState(false);
                }
            }
        }
    }

    // Sync RB cache should be only for Release side.
    PAL_ASSERT(cacheOps.rbCache == false);

    if (syncCacheAndWaitPfp == false)
    {
        if (cacheOps.glxFlags != SyncGlxNone)
        {
            // We need a trailing acquire_mem to handle any cache sync requests.
            AcquireMemGeneric acquireMem = {};
            acquireMem.engineType = engineType;
            acquireMem.cacheSync  = cacheOps.glxFlags;

            pCmdSpace += m_cmdUtil.BuildAcquireMemGeneric(acquireMem, pCmdSpace);
        }

        if (acquirePoint == AcquirePointPfp)
        {
            pCmdSpace += CmdUtil::BuildPfpSyncMe(pCmdSpace);
        }
    }

    if (TestAllFlagsSet(cacheOps.glxFlags, SyncGl2WbInv))
    {
        pCmdBuf->ClearBltWriteMisalignMdState();
    }

    ConvertSyncGlxFlagsToBarrierOps(cacheOps.glxFlags, pBarrierOps);

    pBarrierOps->pipelineStalls.pfpSyncMe |= (acquirePoint == AcquirePointPfp);

#if PAL_DEVELOPER_BUILD
    pBarrierOps->acquirePoint = ConvertToDeveloperAcquirePoint(acquirePoint);
#endif

    pCmdStream->CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// Issue appropriate cache sync hardware commands to satisfy the cache release requirements.
void BarrierMgr::IssueReleaseThenAcquireSync(
    GfxCmdBuffer*                 pCmdBuf,
    CmdStream*                    pCmdStream,
    uint32                        srcStageMask,
    uint32                        dstStageMask,
    CacheSyncOps                  cacheOps,
    Developer::BarrierOperations* pBarrierOps
    ) const
{
    const EngineType engineType    = pCmdBuf->GetEngineType();
    uint32*          pCmdSpace     = pCmdStream->ReserveCommands();
    SyncGlxFlags     syncGlxFlags  = cacheOps.glxFlags;
    const bool       syncGl2Cache  = TestAllFlagsSet(syncGlxFlags, SyncGl2WbInv);
    AcquirePoint     acquirePoint  = GetAcquirePoint(dstStageMask, engineType);
    ReleaseEvents    releaseEvents = GetReleaseEvents(pCmdBuf, srcStageMask, cacheOps, false, acquirePoint);
    ReleaseMemCaches releaseCaches = {};

    ConvertSyncGlxFlagsToBarrierOps(syncGlxFlags, pBarrierOps);

    // Preprocess release events, acquire caches and point for optimization or HW limitation before issue real packet.
    if (acquirePoint > AcquirePointMe)
    {
        // Optimization: convert acquireCaches to releaseCaches and do GCR op at release Eop instead of acquire
        //               at ME stage so acquire can wait at a later point.
        // Note that converting acquireCaches to releaseCaches is a correctness requirement when skip the acquire
        // at EOP wait.
        if (releaseEvents.eop && (syncGlxFlags != SyncGlxNone))
        {
            const SyncGlxFlags orgSyncGlxFlags = syncGlxFlags;

            releaseCaches = m_cmdUtil.SelectReleaseMemCaches(&syncGlxFlags);

            // If RELEASE_MEM can't handle all GCR bits in ACQUIRE_MEM, convert back and let ACQUIRE_MEM handle it.
            if (syncGlxFlags != SyncGlxNone)
            {
                // Only non-PWS path with acquirePoint= AcquirePointEop can hit here since in PWS path, RELEASE_MEM
                // GCR support all bits in ACQUIRE_MEM GCR except I$ inv.
                PAL_ASSERT(acquirePoint == AcquirePointEop);
                syncGlxFlags = orgSyncGlxFlags;
                releaseCaches.u8All = 0;
            }
        }

        // HW limitation: Can only do GCR op at ME stage for Acquire.
        // Optimization : issue lighter VS_PARTIAL_FLUSH instead of PWS+ packet which needs bump VsDone to
        //                heavier PsDone or Eop.
        //
        // If no release event but with late acquire point, force it to be ME so it can go through the right path to
        // handle cache operation correctly.
        if ((syncGlxFlags != SyncGlxNone) || releaseEvents.vs || (releaseEvents.u8All == 0))
        {
            acquirePoint = AcquirePointMe;
        }
    }

#if PAL_DEVELOPER_BUILD
    pBarrierOps->acquirePoint = ConvertToDeveloperAcquirePoint(acquirePoint);
#endif

    const GfxCmdBufferStateFlags cmdBufStateFlags = pCmdBuf->GetCmdBufState().flags;

    // Optimization: If this is an Eop release acquiring at PFP/ME stage and cmdBufStateFlags.gfxWriteCachesDirty
    //               is active, let's force releaseEvents.rbCache = 1 so cmdBufStateFlags.gfxWriteCachesDirty can
    //               be optimized to 0 at end of this function.
    if (releaseEvents.eop && (acquirePoint <= AcquirePointMe))
    {
        releaseEvents.rbCache |= cmdBufStateFlags.gfxWriteCachesDirty;
    }

    if (releaseEvents.rbCache)
    {
        PAL_ASSERT(releaseEvents.eop); // rbCache sync must have Eop event set.
        SetBarrierOperationsRbCacheSynced(pBarrierOps);
    }

    // Go PWS+ wait if PWS+ is supported, acquire point isn't Eop and
    //     (1) If release a Eop event (PWS+ path performs better than legacy path)
    //     (2) Or if use a later PWS-only acquire point and need release either PsDone or CsDone.
    const bool usePws = m_pDevice->UsePws(engineType)     &&
                        (acquirePoint != AcquirePointEop) &&
                        (releaseEvents.eop ||
                         ((acquirePoint > AcquirePointMe) && (releaseEvents.ps || releaseEvents.cs)));

    bool releaseMemWaitCpDma = false;

    if (NeedWaitCpDma(pCmdBuf, srcStageMask))
    {
        if (EnableReleaseMemWaitCpDma() &&
            (usePws ||
             ((acquirePoint <= AcquirePointMe) && releaseEvents.eop) ||
             ((acquirePoint == AcquirePointEop) && (releaseEvents.rbCache || (releaseCaches.u8All != 0)))))
        {
            releaseMemWaitCpDma = true;
        }
        else
        {
            pCmdSpace += m_cmdUtil.BuildWaitDmaData(pCmdSpace);
        }

        pBarrierOps->pipelineStalls.syncCpDma = 1;
        pCmdBuf->SetCpBltState(false);
    }

    if (usePws)
    {
        // No VsDone as it should go through non-PWS path.
        PAL_ASSERT(releaseEvents.vs == 0);

        ReleaseMemGfx releaseMem = {};
        releaseMem.usePws         = 1;
        releaseMem.gfx11WaitCpDma = releaseMemWaitCpDma;
        releaseMem.cacheSync      = releaseCaches;

        ME_ACQUIRE_MEM_pws_counter_sel_enum pwsCounterSel;
        if (releaseEvents.eop)
        {
            releaseMem.dataSel  = data_sel__me_release_mem__none;
            releaseMem.vgtEvent = releaseEvents.rbCache ? CACHE_FLUSH_AND_INV_TS_EVENT : BOTTOM_OF_PIPE_TS;
            pwsCounterSel       = pws_counter_sel__me_acquire_mem__ts_select__GFX11;
            pBarrierOps->pipelineStalls.eopTsBottomOfPipe = 1;

            // Handle cases where a stall is needed as a workaround before EOP with CB Flush event
            const auto& gfx9Device = *static_cast<Device*>(m_pGfxDevice);
            if (releaseEvents.rbCache &&
                TestAnyFlagSet(gfx9Device.Settings().waitOnFlush, WaitBeforeBarrierEopWithCbFlush))
            {
                constexpr WriteWaitEopInfo WaitEopInfo = { .hwAcqPoint = AcquirePointPreDepth };

                pCmdSpace = pCmdBuf->WriteWaitEop(WaitEopInfo, pCmdSpace);
            }
        }
        else
        {
            // Note: PWS+ doesn't need timestamp write, we pass in a dummy write just to meet RELEASE_MEM packet
            //       programming requirement for DATA_SEL field, where 0=none (Discard data) is not a valid option
            //       when EVENT_INDEX=shader_done (PS_DONE/CS_DONE).
            releaseMem.dataSel = data_sel__me_release_mem__send_32_bit_low;
            releaseMem.dstAddr = pCmdBuf->TimestampGpuVirtAddr();

            if (releaseEvents.ps)
            {
                releaseMem.vgtEvent = PS_DONE;
                pwsCounterSel       = pws_counter_sel__me_acquire_mem__ps_select__GFX11;
                pBarrierOps->pipelineStalls.eosTsPsDone = 1;
            }
            else
            {
                PAL_ASSERT(releaseEvents.cs);
                releaseMem.vgtEvent = CS_DONE;
                pwsCounterSel       = pws_counter_sel__me_acquire_mem__cs_select__GFX11;
                pBarrierOps->pipelineStalls.eosTsCsDone = 1;
            }
        }

        pCmdSpace += m_cmdUtil.BuildReleaseMemGfx(releaseMem, pCmdSpace);

        // If need issue CsDone RELEASE_MEM/ACQUIRE_MEM event when both PsDone and CsDone are active.
        if (releaseEvents.ps && releaseEvents.cs)
        {
            PAL_ASSERT(releaseEvents.eop == 0);
            releaseMem.vgtEvent       = CS_DONE;
            releaseMem.gfx11WaitCpDma = false; // PS_DONE has waited CpDma, no need wait again.
            pCmdSpace += m_cmdUtil.BuildReleaseMemGfx(releaseMem, pCmdSpace);
            pBarrierOps->pipelineStalls.eosTsCsDone = 1;
        }

        // Wait on the PWS+ event via ACQUIRE_MEM
        AcquireMemGfxPws acquireMem = {};
        acquireMem.cacheSync  = syncGlxFlags;
        acquireMem.stageSel   = GetPwsStageSel(acquirePoint);
        acquireMem.counterSel = pwsCounterSel;
        acquireMem.syncCount  = 0;

        pCmdSpace += m_cmdUtil.BuildAcquireMemGfxPws(acquireMem, pCmdSpace);

        if (releaseEvents.ps && releaseEvents.cs)
        {
            acquireMem.counterSel = pws_counter_sel__me_acquire_mem__cs_select__GFX11;
            pCmdSpace += m_cmdUtil.BuildAcquireMemGfxPws(acquireMem, pCmdSpace);
        }

        pBarrierOps->pipelineStalls.waitOnTs   = 1;
        pBarrierOps->pipelineStalls.pfpSyncMe |= (acquirePoint == AcquirePointPfp);
    }
    else if (acquirePoint <= AcquirePointMe) // Non-PWS path
    {
        if (releaseEvents.eop)
        {
            WriteWaitEopInfo waitEopInfo = {};
            waitEopInfo.hwRbSync   = releaseEvents.rbCache ? SyncRbWbInv : SyncRbNone;
            waitEopInfo.hwAcqPoint = AcquirePointMe;
            waitEopInfo.waitCpDma  = releaseMemWaitCpDma;
            waitEopInfo.disablePws = true;

            pCmdSpace = pCmdBuf->WriteWaitEop(waitEopInfo, pCmdSpace);

            pBarrierOps->pipelineStalls.eopTsBottomOfPipe = 1;
            pBarrierOps->pipelineStalls.waitOnTs = 1;
        }
        else
        {
            if (releaseEvents.ps)
            {
                pCmdSpace += m_cmdUtil.BuildNonSampleEventWrite(PS_PARTIAL_FLUSH, engineType, pCmdSpace);
                pBarrierOps->pipelineStalls.psPartialFlush = 1;
            }

            if (releaseEvents.vs)
            {
                pCmdSpace += m_cmdUtil.BuildNonSampleEventWrite(VS_PARTIAL_FLUSH, engineType, pCmdSpace);
                pBarrierOps->pipelineStalls.vsPartialFlush = 1;
            }

            if (releaseEvents.cs)
            {
                pCmdSpace = pCmdBuf->WriteWaitCsIdle(pCmdSpace);
                pBarrierOps->pipelineStalls.csPartialFlush = 1;
            }
        }

        if (syncGlxFlags != SyncGlxNone)
        {
            // We need a trailing acquire_mem to handle any cache sync requests.
            AcquireMemGeneric acquireMem = {};
            acquireMem.engineType = engineType;
            acquireMem.cacheSync  = syncGlxFlags;

            pCmdSpace += m_cmdUtil.BuildAcquireMemGeneric(acquireMem, pCmdSpace);
        }

        // BuildWaitRegMem waits in the ME, if the waitPoint needs to stall at the PFP request a PFP/ME sync.
        if (acquirePoint == AcquirePointPfp)
        {
            // Stalls the CP PFP until the ME has processed all previous commands.  Useful in cases where the ME
            // is waiting on some condition, but the PFP needs to stall execution until the condition is satisfied.
            // This must go last otherwise the PFP could resume execution before the ME is done with all of its
            // waits.
            pCmdSpace += m_cmdUtil.BuildPfpSyncMe(pCmdSpace);
            pBarrierOps->pipelineStalls.pfpSyncMe = 1;
        }
    }
    else // acquirePoint == AcquirePointEop
    {
        PAL_ASSERT(acquirePoint == AcquirePointEop);
        PAL_ASSERT(syncGlxFlags == SyncGlxNone); // syncGlxFlags should be all converted to releaseCaches.

        if (releaseEvents.rbCache || (releaseCaches.u8All != 0))
        {
            PAL_ASSERT(releaseEvents.eop); // Must be Eop event to sync RB or GCR cache.

            // Need issue GCR.gl2Inv/gl2Wb and RB cache sync in single ReleaseMem packet to avoid racing issue.
            // Note that it's possible GCR.glkInv is left in acquireCaches for cases glkInv isn't supported in
            // ReleaseMem packet. This case has been handled in if path since acquirePoint has been changed to Me
            // in optimization codes above the if branch.
            ReleaseMemGfx releaseMem = {};
            releaseMem.cacheSync      = releaseCaches;
            releaseMem.vgtEvent       = releaseEvents.rbCache ? CACHE_FLUSH_AND_INV_TS_EVENT : BOTTOM_OF_PIPE_TS;
            releaseMem.dataSel        = data_sel__me_release_mem__none;
            releaseMem.gfx11WaitCpDma = releaseMemWaitCpDma;

            pCmdSpace += m_cmdUtil.BuildReleaseMemGfx(releaseMem, pCmdSpace);
            pBarrierOps->pipelineStalls.eopTsBottomOfPipe = 1;
        }
    }

    // If we have stalled at Eop or Cs, update some CmdBufState (e.g. xxxBltActive) flags.
    if (acquirePoint <= AcquirePointMe)
    {
        if (releaseEvents.eop)
        {
            pCmdBuf->SetPrevCmdBufInactive();
            pCmdBuf->SetGfxBltState(false);
            pCmdBuf->UpdateRetiredAcqRelFenceVal(ReleaseTokenEop, pCmdBuf->GetCurAcqRelFenceVal(ReleaseTokenEop));

            if (releaseEvents.rbCache)
            {
                pCmdBuf->SetGfxBltWriteCacheState(false);
            }
        }

        if (releaseEvents.eop || releaseEvents.ps)
        {
            pCmdBuf->UpdateRetiredAcqRelFenceVal(ReleaseTokenPsDone, pCmdBuf->GetCurAcqRelFenceVal(ReleaseTokenPsDone));
        }

        if (releaseEvents.eop || releaseEvents.cs)
        {
            pCmdBuf->SetCsBltState(false);
            pCmdBuf->UpdateRetiredAcqRelFenceVal(ReleaseTokenCsDone, pCmdBuf->GetCurAcqRelFenceVal(ReleaseTokenCsDone));
        }
    }

    if (syncGl2Cache)
    {
        pCmdBuf->ClearBltWriteMisalignMdState();
    }

    pCmdStream->CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// Figure out the specific BLT operation(s) necessary to convert a color image from one ImageLayout to another.
LayoutTransitionInfo BarrierMgr::PrepareColorBlt(
    GfxCmdBuffer*      pCmdBuf,
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

                if (RsrcProcMgr().WillDecompressColorWithCompute(pCmdBuf, gfx9ImageConst, subresRange))
                {
                    transitionInfo.flags.useComputePath = 1;
                }
            }
        }
        else if (isMsaaImage)
        {
            if (oldState == ColorCompressed)
            {
                // Need FmaskDecompress in preparation for the following full MSAA color decompress.
                transitionInfo.blt[bltIndex++] = HwLayoutTransition::FmaskDecompress;
            }
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

                if (RsrcProcMgr().WillDecompressColorWithCompute(pCmdBuf, gfx9ImageConst, subresRange))
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
LayoutTransitionInfo BarrierMgr::PrepareDepthStencilBlt(
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

        if (RsrcProcMgr().WillDecompressDepthStencilWithCompute(pCmdBuf, gfx9Image, subresRange))
        {
            transitionInfo.flags.useComputePath = 1;
        }
    }
    // Resummarize the htile values from the depth-stencil surface contents when transitioning from "HiZ invalid"
    // state to something that uses HiZ.
    else if ((oldState == DepthStencilDecomprNoHiZ) && (newState != DepthStencilDecomprNoHiZ))
    {
        if (RsrcProcMgr().WillResummarizeWithCompute(pCmdBuf, image))
        {
            // CS blit to open-up the HiZ range.
            transitionInfo.blt[0] = HwLayoutTransition::ExpandHtileHiZRange;
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
LayoutTransitionInfo BarrierMgr::PrepareBltInfo(
    GfxCmdBuffer*     pCmdBuf,
    const ImgBarrier& imgBarrier
    ) const
{
    PAL_ASSERT(imgBarrier.pImage != nullptr);
    PAL_ASSERT(imgBarrier.subresRange.numPlanes == 1);

#if PAL_ENABLE_PRINTS_ASSERTS || PAL_ENABLE_LOGGING
    // With the exception of a transition uninitialized state, at least one queue type must be valid for every layout.
    if (imgBarrier.oldLayout.usages != 0)
    {
        PAL_ASSERT((imgBarrier.oldLayout.usages == LayoutUninitializedTarget) ||
                   (imgBarrier.oldLayout.engines != 0));
    }
    if (imgBarrier.newLayout.usages != 0)
    {
        PAL_ASSERT((imgBarrier.newLayout.usages == LayoutUninitializedTarget) ||
                   (imgBarrier.newLayout.engines != 0));
    }
#endif

    const ImageLayout oldLayout   = imgBarrier.oldLayout;
    const ImageLayout newLayout   = imgBarrier.newLayout;
    const auto&       image       = static_cast<const Pal::Image&>(*imgBarrier.pImage);
    const auto&       subresRange = imgBarrier.subresRange;

    LayoutTransitionInfo layoutTransInfo = {};

    if ((oldLayout.usages == 0) && (newLayout.usages == 0))
    {
        // Default no layout transition if zero usages are provided.
    }
    else if (TestAnyFlagSet(newLayout.usages, LayoutUninitializedTarget))
    {
        // If the LayoutUninitializedTarget usage is set, no other usages should be set.
        PAL_ASSERT(TestAnyFlagSet(newLayout.usages, ~LayoutUninitializedTarget) == false);

        // We do no decompresses, expands, or any other kind of blt in this case.
    }
    else if (TestAnyFlagSet(oldLayout.usages, LayoutUninitializedTarget))
    {
        // If the LayoutUninitializedTarget usage is set, no other usages should be set.
        PAL_ASSERT(TestAnyFlagSet(oldLayout.usages, ~LayoutUninitializedTarget) == false);

        const auto& gfx9Image   = static_cast<const Gfx9::Image&>(*image.GetGfxImage());

#if PAL_ENABLE_PRINTS_ASSERTS || PAL_ENABLE_LOGGING
        const auto& engineProps = m_pDevice->EngineProperties().perEngine[pCmdBuf->GetEngineType()];

        // This queue must support this barrier transition.
        PAL_ASSERT(engineProps.flags.supportsImageInitBarrier == 1);
#endif

        if (gfx9Image.HasColorMetaData() || gfx9Image.HasHtileData())
        {
            layoutTransInfo.blt[0] = HwLayoutTransition::InitMaskRam;
        }
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
static bool HasGlobalBarrier(
    const AcquireReleaseInfo& barrierInfo)
{
    return (barrierInfo.srcGlobalStageMask  |
            barrierInfo.dstGlobalStageMask  |
            barrierInfo.srcGlobalAccessMask |
            barrierInfo.dstGlobalAccessMask) != 0;
}

// =====================================================================================================================
ReleaseToken BarrierMgr::ReleaseInternal(
    GfxCmdBuffer*                 pCmdBuf,
    const AcquireReleaseInfo&     releaseInfo,
    const GpuEvent*               pClientEvent,
    Developer::BarrierOperations* pBarrierOps
    ) const
{
    uint32     srcGlobalStageMask    = releaseInfo.srcGlobalStageMask;
    const bool hasGlobalBarrier      = HasGlobalBarrier(releaseInfo);
    bool       releaseBufferCopyOnly = (hasGlobalBarrier == false) && (releaseInfo.imageBarrierCount == 0);
    uint32     unusedStageMask       = 0;

    // Optimize global stage masks.
    OptimizeStageMask(pCmdBuf, BarrierType::Global, &srcGlobalStageMask, &unusedStageMask);

    CacheSyncOps globalCacheOps = GetGlobalCacheSyncOps(pCmdBuf, releaseInfo.srcGlobalAccessMask, 0);

    // Always do full-range flush sync.
    uint32 srcStageMask = 0;
    for (uint32 i = 0; i < releaseInfo.memoryBarrierCount; i++)
    {
        const MemBarrier& barrier           = releaseInfo.pMemoryBarriers[i];
        const bool        releaseBufferCopy = (barrier.srcStageMask == PipelineStageBlt) &&
                                              (barrier.srcAccessMask != 0)               &&
                                              TestAllFlagsSet(CoherCopy, barrier.srcAccessMask);

        globalCacheOps        |= GetBufferCacheSyncOps(pCmdBuf, barrier.srcAccessMask, 0);
        srcStageMask          |= barrier.srcStageMask;
        releaseBufferCopyOnly &= releaseBufferCopy;
    }

    // Optimize buffer stage masks before OR together.
    OptimizeStageMask(pCmdBuf, BarrierType::Buffer, &srcStageMask, &unusedStageMask);
    srcGlobalStageMask |= srcStageMask;

    // A container to cache the calculated BLT transitions and some cache info for reuse.
    AcqRelAutoBuffer transitionList(releaseInfo.imageBarrierCount, m_pDevice->GetPlatform());
    ReleaseToken     syncToken = {};

    if (transitionList.Capacity() >= releaseInfo.imageBarrierCount)
    {
        // If BLTs will be issued, we need to know how to acquire for them.
        CmdStream*           pCmdStream = GetCmdStream(pCmdBuf);
        AcqRelTransitionInfo transInfo  = { .pList = &transitionList };
        AcqRelImageSyncInfo  syncInfo   = GetAcqRelLayoutTransitionBltInfo(pCmdBuf, pCmdStream, releaseInfo,
                                                                           &transInfo, pBarrierOps);

        // Merge sync info for image barriers without layout transition blt.
        globalCacheOps     |= syncInfo.cacheOps[WithoutLayoutTransBlt];
        srcGlobalStageMask |= syncInfo.srcStageMask[WithoutLayoutTransBlt];

        // Release should not have dstStageMask info.
        PAL_ASSERT(syncInfo.dstStageMask == 0);

        // Issue BLTs if there exists transitions that require one.
        if (transInfo.bltCount > 0)
        {
            // If there is global barrier, it's not clear if it's applicable to the image barrier with layout blt or
            // not. In theory, it's possible. For safe, handle all sync operations in pre-blt barrier in this case.
            if (hasGlobalBarrier)
            {
                syncInfo.cacheOps[WithLayoutTransBlt]     |= globalCacheOps;
                syncInfo.srcStageMask[WithLayoutTransBlt] |= srcGlobalStageMask;

                globalCacheOps     = {};
                srcGlobalStageMask = 0;
            }

            // Issue all-in-one ReleaseThenAcquire prior to the potential BLTs.
            IssueReleaseThenAcquireSync(pCmdBuf,
                                        pCmdStream,
                                        syncInfo.srcStageMask[WithLayoutTransBlt],
                                        transInfo.bltStageMask,
                                        syncInfo.cacheOps[WithLayoutTransBlt],
                                        pBarrierOps);

            // Issue BLTs and get required SyncGlxFlags/syncRbCache for post blt sync.
            globalCacheOps |= IssueAcqRelLayoutTransitionBlt(pCmdBuf, pCmdStream, &transInfo, pBarrierOps);

            // Or srcGlobalStageMask with release BLTs, no need call OptimizeStageMask() here.
            srcGlobalStageMask |= transInfo.bltStageMask;
        }

        syncToken = IssueReleaseSync(pCmdBuf, srcGlobalStageMask, releaseBufferCopyOnly, globalCacheOps,
                                     pClientEvent, pBarrierOps);
    }
    else
    {
        pCmdBuf->NotifyAllocFailure();
    }

    return syncToken;
}

// =====================================================================================================================
void BarrierMgr::AcquireInternal(
    GfxCmdBuffer*                 pCmdBuf,
    const AcquireReleaseInfo&     acquireInfo,
    uint32                        syncTokenCount,
    const ReleaseToken*           pSyncTokens,
    Developer::BarrierOperations* pBarrierOps
    ) const
{
    uint32 dstGlobalStageMask = acquireInfo.dstGlobalStageMask;
    uint32 unusedStageMask    = 0;

    // Optimize global stage masks.
    OptimizeStageMask(pCmdBuf, BarrierType::Global, &unusedStageMask, &dstGlobalStageMask);

    CacheSyncOps globalCacheOps = GetGlobalCacheSyncOps(pCmdBuf, 0, acquireInfo.dstGlobalAccessMask);

    // Always do full-range flush sync.
    uint32 dstStageMask = 0;
    for (uint32 i = 0; i < acquireInfo.memoryBarrierCount; i++)
    {
        const MemBarrier& barrier = acquireInfo.pMemoryBarriers[i];

        globalCacheOps |= GetBufferCacheSyncOps(pCmdBuf, 0, barrier.dstAccessMask);
        dstStageMask   |= barrier.dstStageMask;
    }

    // Optimize buffer stage masks before OR together.
    OptimizeStageMask(pCmdBuf, BarrierType::Buffer, &unusedStageMask, &dstStageMask);
    dstGlobalStageMask |= dstStageMask;

    // A container to cache the calculated BLT transitions and some cache info for reuse.
    AcqRelAutoBuffer transitionList(acquireInfo.imageBarrierCount, m_pDevice->GetPlatform());

    if (transitionList.Capacity() >= acquireInfo.imageBarrierCount)
    {
        // If BLTs will be issued, we need to know how to acquire for them.
        CmdStream*           pCmdStream = GetCmdStream(pCmdBuf);
        AcqRelTransitionInfo transInfo  = { .pList = &transitionList };
        AcqRelImageSyncInfo  syncInfo   = GetAcqRelLayoutTransitionBltInfo(pCmdBuf, pCmdStream, acquireInfo,
                                                                           &transInfo, pBarrierOps);

        // Merge sync info for image barriers without layout transition blt.
        globalCacheOps     |= syncInfo.cacheOps[WithoutLayoutTransBlt];
        dstGlobalStageMask |= syncInfo.dstStageMask;

        // Should no syncRbCache and srcStageMask case at Acquire.
        PAL_ASSERT((syncInfo.srcStageMask[WithLayoutTransBlt]    == 0) &&
                   (syncInfo.srcStageMask[WithoutLayoutTransBlt] == 0));

        // Issue acquire for global or pre-BLT sync.
        if (transInfo.bltCount > 0)
        {
            // If there is global barrier, it's not clear if it's applicable to the image barrier with layout blt or
            // not. In theory, it's possible. For safe, handle all sync operations in pre-blt barrier in this case.
            if (HasGlobalBarrier(acquireInfo))
            {
                syncInfo.cacheOps[WithLayoutTransBlt] |= globalCacheOps;

                globalCacheOps = {};
            }

            IssueAcquireSync(pCmdBuf, transInfo.bltStageMask, syncInfo.cacheOps[WithLayoutTransBlt],
                             syncTokenCount, pSyncTokens, pBarrierOps);

            // Issue BLTs and get required SyncGlxFlags/syncRbCache for post blt sync.
            globalCacheOps |= IssueAcqRelLayoutTransitionBlt(pCmdBuf, pCmdStream, &transInfo, pBarrierOps);

            // Issue all-in-one ReleaseThenAcquire prior to the potential BLTs.
            IssueReleaseThenAcquireSync(pCmdBuf, pCmdStream, transInfo.bltStageMask, dstGlobalStageMask, globalCacheOps,
                                        pBarrierOps);
        }
        else
        {
            IssueAcquireSync(pCmdBuf, dstGlobalStageMask, globalCacheOps, syncTokenCount, pSyncTokens, pBarrierOps);
        }

        // Must be called after all sync done in case racing issue due to alias barrier.
        PostSyncUpdateDccStateMetaData(pCmdBuf, pCmdStream, transInfo, pBarrierOps);
    }
    else
    {
        pCmdBuf->NotifyAllocFailure();
    }
}

// =====================================================================================================================
// Release perform any necessary layout transition, availability operation, and enqueue command(s) to set a given
// IGpuEvent object once the prior operations' intersection with the given synchronization scope is confirmed complete.
// The availability operation will flush the requested local caches.
ReleaseToken BarrierMgr::Release(
    GfxCmdBuffer*                 pGfxCmdBuf,
    const AcquireReleaseInfo&     releaseInfo,
    Developer::BarrierOperations* pBarrierOps
    ) const
{
    const EngineType engineType = pGfxCmdBuf->GetEngineType();

    // PWS enabled case doesn't need fence GPU memory since internal PWS counter is used instead.
    if ((pGfxCmdBuf->GetAcqRelFenceValBaseGpuVa() == 0) && (m_pDevice->UsePws(engineType) == false))
    {
        // Allocate acquire/release synchronization fence value GPU memory from the command allocator.
        // AllocateGpuScratchMem() always returns a valid GPU address, even if we fail to obtain memory from the
        // allocator.  In that scenario, the allocator returns a dummy chunk so we can always have a valid object
        // to access, and sets m_status to a failure code.
        const gpusize fenceAddr = pGfxCmdBuf->AllocateGpuScratchMem(ReleaseTokenCount, 1);

        const uint32  data[ReleaseTokenCount] = {};
        WriteDataInfo writeDataInfo           = {};
        writeDataInfo.engineType = engineType;
        writeDataInfo.engineSel  = engine_sel__pfp_write_data__prefetch_parser;
        writeDataInfo.dstSel     = dst_sel__pfp_write_data__memory;
        writeDataInfo.dstAddr    = fenceAddr;

        CmdStream* const pCmdStream = GetCmdStream(pGfxCmdBuf);
        uint32*          pCmdSpace  = pCmdStream->ReserveCommands();
        pCmdSpace += CmdUtil::BuildWriteData(writeDataInfo, (sizeof(data) / sizeof(uint32)), &data[0], pCmdSpace);
        pCmdStream->CommitCommands(pCmdSpace);

        pGfxCmdBuf->SetAcqRelFenceValBaseGpuVa(fenceAddr);
    }

    return ReleaseInternal(pGfxCmdBuf, releaseInfo, nullptr, pBarrierOps);
}

// =====================================================================================================================
// Acquire will wait on the specified IGpuEvent object to be signaled, perform any necessary layout transition, and
// issue the required visibility operations. The visibility operation will invalidate the required ranges in local
// caches.
void BarrierMgr::Acquire(
    GfxCmdBuffer*                 pGfxCmdBuf,
    const AcquireReleaseInfo&     acquireInfo,
    uint32                        syncTokenCount,
    const ReleaseToken*           pSyncTokens,
    Developer::BarrierOperations* pBarrierOps
    ) const
{
    AcquireInternal(pGfxCmdBuf, acquireInfo, syncTokenCount, pSyncTokens, pBarrierOps);
}

// =====================================================================================================================
// Release perform any necessary layout transition, availability operation, and enqueue command(s) to set a given
// IGpuEvent object once the prior operations' intersection with the given synchronization scope is confirmed complete.
// The availability operation will flush the requested local caches.
void BarrierMgr::ReleaseEvent(
    GfxCmdBuffer*                 pGfxCmdBuf,
    const AcquireReleaseInfo&     releaseInfo,
    const IGpuEvent*              pClientEvent,
    Developer::BarrierOperations* pBarrierOps
    ) const
{
    ReleaseInternal(pGfxCmdBuf, releaseInfo, static_cast<const GpuEvent*>(pClientEvent), pBarrierOps);
}

// =====================================================================================================================
// Acquire will wait on the specified IGpuEvent object to be signaled, perform any necessary layout transition,
// and issue the required visibility operations. The visibility operation will invalidate the required ranges in local
// caches.
void BarrierMgr::AcquireEvent(
    GfxCmdBuffer*                 pCmdBuf,
    const AcquireReleaseInfo&     acquireInfo,
    uint32                        gpuEventCount,
    const IGpuEvent* const*       ppGpuEvents,
    Developer::BarrierOperations* pBarrierOps
    ) const
{
    // Wait on the GPU memory slot(s) in all specified IGpuEvent objects.
    if (gpuEventCount > 0)
    {
        const EngineType engineType = pCmdBuf->GetEngineType();
        auto* const      pCmdStream = GetCmdStream(pCmdBuf);
        uint32*          pCmdSpace  = pCmdStream->ReserveCommands();

        for (uint32 i = 0; i < gpuEventCount; i++)
        {
            const GpuEvent* pGpuEvent       = static_cast<const GpuEvent*>(ppGpuEvents[i]);
            const gpusize   gpuEventStartVa = pGpuEvent->GetBoundGpuMemory().GpuVirtAddr();

            PAL_ASSERT(pGpuEvent->GetBoundGpuMemory().IsBound());

            pCmdSpace += CmdUtil::BuildWaitRegMem(engineType,
                                                  mem_space__me_wait_reg_mem__memory_space,
                                                  function__me_wait_reg_mem__equal_to_the_reference_value,
                                                  engine_sel__me_wait_reg_mem__micro_engine,
                                                  gpuEventStartVa,
                                                  GpuEvent::SetValue,
                                                  UINT32_MAX,
                                                  pCmdSpace);
        }
        pBarrierOps->pipelineStalls.waitOnTs = 1;

        pCmdStream->CommitCommands(pCmdSpace);
    }

    AcquireInternal(pCmdBuf, acquireInfo, 0, nullptr, pBarrierOps);
}

// =====================================================================================================================
// ReleaseThenAcquire is effectively the same as calling Release immediately by calling Acquire.
// This is a convenience method for clients implementing single point barriers. Since ReleaseThenAcquire knows
// both srcAccessMask and dstAccessMask info, instead of calling Release and Acquire directly, can
// implement independently to issue barrier optimally.
void BarrierMgr::ReleaseThenAcquire(
    GfxCmdBuffer*                 pCmdBuf,
    const AcquireReleaseInfo&     barrierInfo,
    Developer::BarrierOperations* pBarrierOps
    ) const
{
    uint32 srcGlobalStageMask = barrierInfo.srcGlobalStageMask;
    uint32 dstGlobalStageMask = barrierInfo.dstGlobalStageMask;

    // Optimize global stage masks.
    OptimizeStageMask(pCmdBuf, BarrierType::Global, &srcGlobalStageMask, &dstGlobalStageMask);

    CacheSyncOps globalCacheOps = GetGlobalCacheSyncOps(pCmdBuf,
                                                       barrierInfo.srcGlobalAccessMask,
                                                       barrierInfo.dstGlobalAccessMask);

    // Always do full-range flush sync.
    uint32 srcStageMask = 0;
    uint32 dstStageMask = 0;
    for (uint32 i = 0; i < barrierInfo.memoryBarrierCount; i++)
    {
        MemBarrier barrier = barrierInfo.pMemoryBarriers[i];

        if (IsReadOnlyTransition(barrier.srcAccessMask, barrier.dstAccessMask))
        {
            OptimizeReadOnlyBarrier(pCmdBuf,
                                    nullptr,
                                    &barrier.srcStageMask,
                                    &barrier.dstStageMask,
                                    &barrier.srcAccessMask,
                                    &barrier.dstAccessMask);
        }

        globalCacheOps |= GetBufferCacheSyncOps(pCmdBuf, barrier.srcAccessMask, barrier.dstAccessMask);
        srcStageMask   |= barrier.srcStageMask;
        dstStageMask   |= barrier.dstStageMask;
    }

    // Optimize buffer stage masks before OR together.
    OptimizeStageMask(pCmdBuf, BarrierType::Buffer, &srcStageMask, &dstStageMask);
    srcGlobalStageMask |= srcStageMask;
    dstGlobalStageMask |= dstStageMask;

    // A container to cache the calculated BLT transitions and some cache info for reuse.
    AcqRelAutoBuffer transitionList(barrierInfo.imageBarrierCount, m_pDevice->GetPlatform());

    if (transitionList.Capacity() >= barrierInfo.imageBarrierCount)
    {
        // If BLTs will be issued, we need to know how to acquire for them.
        CmdStream*           pCmdStream = GetCmdStream(pCmdBuf);
        AcqRelTransitionInfo transInfo  = { .pList = &transitionList };
        AcqRelImageSyncInfo  syncInfo   = GetAcqRelLayoutTransitionBltInfo(pCmdBuf, pCmdStream, barrierInfo,
                                                                           &transInfo, pBarrierOps);

        // Merge sync info for image barriers without layout transition blt.
        globalCacheOps     |= syncInfo.cacheOps[WithoutLayoutTransBlt];
        srcGlobalStageMask |= syncInfo.srcStageMask[WithoutLayoutTransBlt];
        dstGlobalStageMask |= syncInfo.dstStageMask;

        // Issue BLTs if there exists transitions that require one.
        if (transInfo.bltCount > 0)
        {
            // If there is global barrier, it's not clear if it's applicable to the image barrier with layout blt or
            // not. In theory, it's possible. For safe, handle all sync operations in pre-blt barrier in this case.
            if (HasGlobalBarrier(barrierInfo))
            {
                syncInfo.cacheOps[WithLayoutTransBlt]     |= globalCacheOps;
                syncInfo.srcStageMask[WithLayoutTransBlt] |= srcGlobalStageMask;

                globalCacheOps     = {};
                srcGlobalStageMask = 0;
            }

            IssueReleaseThenAcquireSync(pCmdBuf,
                                        pCmdStream,
                                        syncInfo.srcStageMask[WithLayoutTransBlt],
                                        transInfo.bltStageMask,
                                        syncInfo.cacheOps[WithLayoutTransBlt],
                                        pBarrierOps);

            // Issue BLTs and get required SyncGlxFlags/syncRbCache for post blt sync.
            globalCacheOps |= IssueAcqRelLayoutTransitionBlt(pCmdBuf, pCmdStream, &transInfo, pBarrierOps);

            // Or srcGlobalStageMask with release BLTs, no need call OptimizeStageMask() here.
            srcGlobalStageMask |= transInfo.bltStageMask;
        }

        // Issue acquire for global sync.
        IssueReleaseThenAcquireSync(pCmdBuf, pCmdStream, srcGlobalStageMask, dstGlobalStageMask, globalCacheOps,
                                    pBarrierOps);

        // Must be called after all sync done in case racing issue due to alias barrier.
        PostSyncUpdateDccStateMetaData(pCmdBuf, pCmdStream, transInfo, pBarrierOps);
    }
    else
    {
        pCmdBuf->NotifyAllocFailure();
    }
}

// =====================================================================================================================
// Helper function that issues requested transition for the image barrier.
// Returns "true" if there is InitMaskRam operation and it's done by compute engine.
bool BarrierMgr::IssueBlt(
    GfxCmdBuffer*                 pCmdBuf,
    CmdStream*                    pCmdStream,
    const ImgBarrier*             pImgBarrier,
    LayoutTransitionInfo          layoutTransInfo,
    bool*                         pPreInitHtileSynced, // In/out if pre-InitHtile sync has been done.
    Developer::BarrierOperations* pBarrierOps
    ) const
{
    PAL_ASSERT(pImgBarrier->subresRange.numPlanes == 1);
    PAL_ASSERT(pImgBarrier != nullptr);
    PAL_ASSERT(layoutTransInfo.blt[0] != HwLayoutTransition::None);
    PAL_ASSERT(pBarrierOps != nullptr);
    PAL_ASSERT(pPreInitHtileSynced != nullptr);

    // Tell RGP about this transition
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 902
    AcqRelBuildTransition(pImgBarrier, layoutTransInfo, pBarrierOps);
#else
    const BarrierTransition rgpTransition = AcqRelBuildTransition(pImgBarrier, layoutTransInfo, pBarrierOps);
#endif
    const auto&             image         = static_cast<const Pal::Image&>(*pImgBarrier->pImage);
    bool                    csInitMaskRam = false;

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
        if ((*pPreInitHtileSynced == false)       &&
            (gfx9Image.HasHtileData() == true)    &&
            (image.GetImageInfo().numPlanes == 2) &&
            (gfx9Image.GetHtile()->TileStencilDisabled() == false))
        {
            bool waitEop = false;

            uint32* pCmdSpace = pCmdStream->ReserveCommands();

            if (pCmdBuf->IsGraphicsSupported())
            {
                // The most efficient way to wait for DB-idle and flush and invalidate the DB caches on pre-gfx11 HW
                // is an acquire_mem. Gfx11 can't touch the DB caches using an acquire_mem but that's OK because we
                // expect WriteWaitEop to do a PWS EOP wait which should be fast.
                if (IsGfx11(*m_pDevice))
                {
                    constexpr WriteWaitEopInfo WaitEopInfo = { .hwGlxSync  = SyncGlvInv | SyncGl1Inv,
                                                               .hwRbSync   = SyncDbWbInv,
                                                               .hwAcqPoint = AcquirePointMe };

                    pCmdSpace = pCmdBuf->WriteWaitEop(WaitEopInfo, pCmdSpace);
                    waitEop = true;
                }
                else
                {
                    AcquireMemGfxSurfSync acquireInfo = {};
                    acquireInfo.cacheSync           = SyncGlvInv | SyncGl1Inv;
                    acquireInfo.rangeBase           = image.GetGpuVirtualAddr();
                    acquireInfo.rangeSize           = gfx9Image.GetGpuMemSyncSize();
                    acquireInfo.flags.dbTargetStall = 1;
                    acquireInfo.flags.gfx10DbWbInv  = 1;

                    pCmdSpace += m_cmdUtil.BuildAcquireMemGfxSurfSync(acquireInfo, pCmdSpace);
                }

                pBarrierOps->caches.flushDb         = 1;
                pBarrierOps->caches.invalDb         = 1;
                pBarrierOps->caches.flushDbMetadata = 1;
                pBarrierOps->caches.invalDbMetadata = 1;
            }
            else
            {
                constexpr WriteWaitEopInfo WaitEopInfo = { .hwGlxSync  = SyncGlvInv | SyncGl1Inv,
                                                           .hwAcqPoint = AcquirePointMe };

                pCmdSpace = pCmdBuf->WriteWaitEop(WaitEopInfo, pCmdSpace);
                waitEop = true;
            }

            pCmdStream->CommitCommands(pCmdSpace);

            pBarrierOps->caches.invalTcp = 1;
            pBarrierOps->caches.invalGl1 = 1;
            if (waitEop)
            {
                pBarrierOps->pipelineStalls.eopTsBottomOfPipe = 1;
                pBarrierOps->pipelineStalls.waitOnTs          = 1;
#if PAL_DEVELOPER_BUILD
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 901
                pBarrierOps->acquirePoint                     = Developer::AcquirePointMe;
#else
                pBarrierOps->acquirePoint                     = Developer::AcquirePoint::Me;
#endif
#endif

                *pPreInitHtileSynced                          = true;
            }
        }

        // RGP expects DescribeBarrier() call occurs before layout transition BLT.
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 902
        DescribeBarrier(pCmdBuf, pImgBarrier, pBarrierOps);
#else
        DescribeBarrier(pCmdBuf, &rgpTransition, pBarrierOps);
#endif

        // Transition out of LayoutUninitializedTarget needs to initialize metadata memories.
        csInitMaskRam = AcqRelInitMaskRam(pCmdBuf, pCmdStream, *pImgBarrier);
    }
    else
    {
        // RGP expects DescribeBarrier() call occurs before layout transition BLT.
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 902
        DescribeBarrier(pCmdBuf, pImgBarrier, pBarrierOps);
#else
        DescribeBarrier(pCmdBuf, &rgpTransition, pBarrierOps);
#endif

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

    return csInitMaskRam;
}

} // Gfx9
} // Pal
