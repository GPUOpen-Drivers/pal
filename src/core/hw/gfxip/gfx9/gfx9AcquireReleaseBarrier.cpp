/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

// Cache coherency masks that are writable.
static constexpr uint32 CacheCoherWriteMask  = CoherCpu         | CoherShaderWrite        | CoherStreamOut |
                                               CoherColorTarget | CoherClear              | CoherCopyDst   |
                                               CoherResolveDst  | CoherDepthStencilTarget | CoherCeDump    |
                                               CoherQueueAtomic | CoherTimestamp          | CoherMemory;

// Cache coherency masks that may read data through L0 (V$ and K$)/L1 caches.
static constexpr uint32 CacheCoherShaderReadMask = CoherShaderRead | CoherCopySrc | CoherResolveSrc | CoherSampleRate;

// Cache coherency masks that may read or write data through L0 (V$ and K$)/L1 caches.
static constexpr uint32 CacheCoherShaderAccessMask = CacheCoherencyBlt | CoherShader | CoherSampleRate | CoherStreamOut;

// =====================================================================================================================
// Translate accessMask to ReleaseMemCaches.
static ReleaseMemCaches GetReleaseCacheFlags(
    uint32                        accessMask,  // Bitmask of CacheCoherencyUsageFlags.
    bool                          refreshTcc,
    Developer::BarrierOperations* pBarrierOps)
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
    else if (TestAnyFlagSet(accessMask, CoherCpu | CoherMemory))
    {
        // Split Release/Acquire is built around GL2 being the LLC where data is either released to or acquired from.
        // The Cpu and Memory usages are special because they do not go through GL2. Therefore, when releasing the
        // Cpu or Memory usage, where memory may have been updated directly, we must INV GL2 to ensure those updates
        // will be properly fetched into GL2 for the next GPU usage that is acquired.
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
    bool                          mayHaveMetadata,
    Developer::BarrierOperations* pBarrierOps
    ) const
{
    PAL_ASSERT(pBarrierOps != nullptr);

    SyncGlxFlags flags = SyncGlxNone;

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

    if (TestAnyFlagSet(accessMask, dstCacheInvMask))
    {
        flags |= SyncGlkInv | SyncGlvInv | SyncGlmInv;
        pBarrierOps->caches.invalSqK$        = 1;
        pBarrierOps->caches.invalTcp         = 1;
        pBarrierOps->caches.invalTccMetadata = 1;

        if (IsGfx10Plus(m_gfxIpLevel))
        {
            flags |= SyncGl1Inv;
            pBarrierOps->caches.invalGl1 = 1;
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
        // Split Release/Acquire is built around GL2 being the LLC where data is either released to or acquired from.
        // The Cpu, Memory, and Present usages are special because they do not go through GL2. Therefore, when acquiring
        // the Cpu, Memory, or Present usages we must WB GL2 so all prior writes are visible to those usages that will
        // read directly from memory.
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
    bool                          mayHaveMetadata,
    Developer::BarrierOperations* pBarrierOps
    ) const
{
    SyncGlxFlags acquireFlags = SyncGlxNone;

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
    const bool   canSkipCacheInv = (TestAnyFlagSet(srcAccessMask, CacheCoherWriteMask) == false) &&
                                   TestAnyFlagSet(srcAccessMask, CacheCoherShaderReadMask);

    if (TestAnyFlagSet(dstAccessMask, dstCacheInvMask) && (canSkipCacheInv == false))
    {
        acquireFlags |= SyncGlkInv | SyncGlvInv | SyncGlmInv;
        pBarrierOps->caches.invalSqK$        = 1;
        pBarrierOps->caches.invalTcp         = 1;
        pBarrierOps->caches.invalTccMetadata = 1;

        if (IsGfx10Plus(m_gfxIpLevel))
        {
            acquireFlags |= SyncGl1Inv;
            pBarrierOps->caches.invalGl1 = 1;
        }
    }

    if (refreshTcc)
    {
        acquireFlags |= SyncGl2WbInv;
        pBarrierOps->caches.flushTcc = 1;
        pBarrierOps->caches.invalTcc = 1;
    }
    else
    {
        if (TestAnyFlagSet(srcAccessMask, CoherCpu | CoherMemory))
        {
            // Split Release/Acquire is built around GL2 being the LLC where data is either released to or acquired from.
            // The Cpu and Memory usages are special because they do not go through GL2. Therefore, when releasing the
            // Cpu or Memory usage, where memory may have been updated directly, we must INV GL2 to ensure those updates
            // will be properly fetched into GL2 for the next GPU usage that is acquired.
            acquireFlags |= SyncGl2Inv;
            pBarrierOps->caches.invalTcc = 1;
        }

        if (TestAnyFlagSet(dstAccessMask, CoherCpu | CoherMemory | CoherPresent))
        {
            // Split Release/Acquire is built around GL2 being the LLC where data is either released to or acquired from.
            // The Cpu, Memory, and Present usages are special because they do not go through GL2. Therefore, when acquiring
            // the Cpu, Memory, or Present usages we must WB GL2 so all prior writes are visible to those usages that will
            // read directly from memory.
            acquireFlags |= SyncGl2Wb;
            pBarrierOps->caches.flushTcc = 1;
        }
    }

    return acquireFlags;
}

// =====================================================================================================================
//  We will need flush & inv L2 on MSAA Z, MSAA color, mips in the metadata tail, or any stencil.
//
// The driver assumes that all meta-data surfaces are channel-aligned, but there are cases where the HW does not
// actually channel-align the data.  In these cases, the L2 cache needs to be flushed and invalidated prior to the
// metadata being read by a shader.
// Note that don't call this function for buffer barrier as it's not needed.
static bool WaRefreshTccOnMetadataMisalignment(
    const IImage*      pImage, // nullptr indicates global barrier
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
    constexpr uint32 WaRefreshTccCoherMask = CoherColorTarget | CoherShaderWrite | CoherDepthStencilTarget |
                                             CoherCopyDst     | CoherResolveDst  | CoherClear | CoherPresent;
    bool needRefreshL2 = false;

    if (TestAnyFlagSet(srcAccessMask, WaRefreshTccCoherMask))
    {
        const bool backToBackDirectWrite =
            ((srcAccessMask == CoherColorTarget) && (dstAccessMask == CoherColorTarget)) ||
            ((srcAccessMask == CoherDepthStencilTarget) && (dstAccessMask == CoherDepthStencilTarget));

        // For CoherShaderWrite from image layout transition blt, it doesn't exactly indicate an indirect write
        // mode as image layout transition blt may direct write to fix up metadata. optimizeBacktoBackShaderWrite
        // makes sure when it's safe to optimize it.
        const bool backToBackIndirectWrite =
            shaderMdAccessIndirectOnly                      &&
            TestAnyFlagSet(srcAccessMask, CoherShaderWrite) &&
            TestAnyFlagSet(dstAccessMask, CoherShaderWrite) &&
            (TestAnyFlagSet(srcAccessMask | dstAccessMask, ~CoherShader) == false);

        const auto* pPalImage  = static_cast<const Pal::Image*>(pImage);
        const auto* pGfx9Image = (pImage != nullptr) ? static_cast<const Image*>(pPalImage->GetGfxImage()) : nullptr;

        // Can optimize to skip L2 refresh for back to back write with same access mode
        needRefreshL2 = (backToBackDirectWrite == false)   &&
                        (backToBackIndirectWrite == false) &&
                        // Either global barrier without image pointer or image barrier with metadata.
                        ((pImage == nullptr) || pGfx9Image->NeedFlushForMetadataPipeMisalignment(subresRange));
    }

    return needRefreshL2;
}

// =====================================================================================================================
static ReleaseEvents GetReleaseEvents(
    Pm4CmdBuffer* pCmdBuf,
    const Device& device,
    uint32        srcStageMask,
    uint32        srcAccessMask,
    bool          splitBarrier,
    SyncGlxFlags  acquireCaches = SyncGlxNone,       // Assume SyncGlxNone in split barrier
    AcquirePoint  acquirePoint  = AcquirePoint::Pfp) // Assume worst stall if info not available in split barrier
{
    constexpr uint32 EopWaitStageMask = PipelineStageEarlyDsTarget | PipelineStageLateDsTarget |
                                        PipelineStageColorTarget   | PipelineStageBottomOfPipe;
    // PFP sets IB base and size to register VGT_DMA_BASE & VGT_DMA_SIZE and send request to VGT for indices fetch,
    // which is done in GE. So need VsDone to make sure indices fetch done.
    constexpr uint32 VsWaitStageMask  = PipelineStageVs | PipelineStageHs | PipelineStageDs |
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 770
                                        PipelineStageGs | PipelineStageFetchIndices | PipelineStageStreamOut;
#else
                                        PipelineStageGs | PipelineStageFetchIndices;
#endif
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
            release.cs = TestAnyFlagSet(srcStageMask, CsWaitStageMask);
            release.vs = TestAnyFlagSet(srcStageMask, VsWaitStageMask);
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

#if PAL_BUILD_GFX11
    // Optimization: for stageMask transition Ps|Cs->Rt/Ps/Ds with GCR operation or Vs|Ps|Cs->Rt/Ps/Ds, convert
    //               release events to Eop so can wait at a later PreColor/PrePs/PreDepth point instead of ME.
    if (device.Parent()->UsePws(pCmdBuf->GetEngineType()) &&
        (release.ps && release.cs) &&
        (release.vs || (acquireCaches != SyncGlxNone)) &&
        (acquirePoint >= AcquirePoint::PreDepth))
    {
        release.eop = 1;
    }
#endif

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
        if ((acquirePoint == AcquirePoint::Eop) && (release.rbCache == 0) && (acquireCaches == SyncGlxNone))
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
AcquirePoint Device::GetAcquirePoint(
    uint32     dstStageMask,
    EngineType engineType
    ) const
{
    // Constants to map PAL interface pipe stage masks to HW acquire points.
    constexpr uint32 AcqPfpStages       = PipelineStageTopOfPipe | PipelineStageFetchIndirectArgs |
                                          PipelineStageFetchIndices;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 770
    // In DX12 conformance test, a buffer is bound as color target, cleared, and then bound as stream out
    // bufferFilledSizeLocation, where CmdLoadBufferFilledSizes() will be called to set this buffer with
    // STRMOUT_BUFFER_FILLED_SIZE (e.g. from control buffer for NGG-SO) via CP ME.
    // In CmdDrawOpaque(), bufferFilleSize allocation will be loaded by LOAD_CONTEXT_REG_INDEX packet via PFP to
    // initialize register VGT_STRMOUT_DRAW_OPAQUE_BUFFER_FILLED_SIZE. PFP_SYNC_ME is issued before load packet so
    // we're safe to acquire at ME stage here.
    constexpr uint32 AcqMeStages        = PipelineStageBlt | PipelineStageStreamOut;
#else
    constexpr uint32 AcqMeStages        = PipelineStageBlt;
#endif
    constexpr uint32 AcqPreShaderStages = PipelineStageVs | PipelineStageHs | PipelineStageDs |
                                          PipelineStageGs | PipelineStageCs;
    constexpr uint32 AcqPreDepthStages  = PipelineStageEarlyDsTarget | PipelineStageLateDsTarget;
    constexpr uint32 AcqPrePsStages     = PipelineStagePs;
    constexpr uint32 AcqPreColorStages  = PipelineStageColorTarget;

    // Convert global dstStageMask to HW acquire point.
    AcquirePoint acqPoint = TestAnyFlagSet(dstStageMask, AcqPfpStages)       ? AcquirePoint::Pfp       :
                            TestAnyFlagSet(dstStageMask, AcqMeStages)        ? AcquirePoint::Me        :
                            TestAnyFlagSet(dstStageMask, AcqPreShaderStages) ? AcquirePoint::PreShader :
                            TestAnyFlagSet(dstStageMask, AcqPreDepthStages)  ? AcquirePoint::PreDepth  :
                            TestAnyFlagSet(dstStageMask, AcqPrePsStages)     ? AcquirePoint::PrePs     :
                            TestAnyFlagSet(dstStageMask, AcqPreColorStages)  ? AcquirePoint::PreColor  :
                                                                               AcquirePoint::Eop;

    // Disable PWS late acquire point if PWS is not disabled or late acquire point is disallowed.
    if ((acqPoint > AcquirePoint::Me) &&
        (acqPoint != AcquirePoint::Eop)
#if PAL_BUILD_GFX11
        && (Parent()->UsePwsLateAcquirePoint(engineType) == false)
#endif
        )
    {
        acqPoint = AcquirePoint::Me;
    }

    return acqPoint;
}

#if PAL_BUILD_GFX11
// =====================================================================================================================
static ME_ACQUIRE_MEM_pws_stage_sel_enum GetPwsStageSel(
    AcquirePoint acquirePoint)
{
    static constexpr ME_ACQUIRE_MEM_pws_stage_sel_enum PwsStageSelMapTable[] =
    {
        pws_stage_sel__me_acquire_mem__cp_pfp__HASPWS,         // Pfp       = 0
        pws_stage_sel__me_acquire_mem__cp_me__HASPWS,          // Me        = 1
        pws_stage_sel__me_acquire_mem__pre_shader__HASPWS,     // PreShader = 2
        pws_stage_sel__me_acquire_mem__pre_depth__HASPWS,      // PreDepth  = 3
        pws_stage_sel__me_acquire_mem__pre_pix_shader__HASPWS, // PrePs     = 4
        pws_stage_sel__me_acquire_mem__pre_color__HASPWS,      // PreColor  = 5
        pws_stage_sel__me_acquire_mem__pre_color__HASPWS,      // Eop       = 6 (Invalid)
    };

    PAL_ASSERT(acquirePoint < AcquirePoint::Count);

    return PwsStageSelMapTable[uint32(acquirePoint)];
}
#endif

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

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 767
// =====================================================================================================================
static bool IsReadOnlyTransition(
    uint32 srcAccessMask,
    uint32 dstAccessMask)
{
    return (srcAccessMask != 0) &&
           (dstAccessMask != 0) &&
           (TestAnyFlagSet(srcAccessMask | dstAccessMask, CacheCoherWriteMask) == false);
}

// =====================================================================================================================
// e.g. PS|CS ShaderRead -> CS ShaderRead -> RT, can optimize to only release srcStageMask= PS as CS will be released
// in the second transition.
static uint32 GetOptimizedSrcStagesForReadOnlyBarrier(
    Pm4CmdBuffer* pCmdBuf,
    uint32        srcStageMask,
    uint32        dstStageMask)
{
    constexpr uint32 ReleaseVsStages = PipelineStageVs | PipelineStageHs |PipelineStageDs | PipelineStageGs |
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 770
                                       PipelineStageFetchIndices | PipelineStageStreamOut;
#else
                                       PipelineStageFetchIndices;
#endif

    uint32 optSrcStageMask = (srcStageMask & ~dstStageMask);

    // To handle case like, srcStageMask has PipelineStageVs but dstStageMask has PipelineStageGs set only.
    // Should be safe to remove PipelineStageVs from srcStageMask in this case.
    optSrcStageMask &= ~(TestAnyFlagSet(dstStageMask, ReleaseVsStages) ? ReleaseVsStages : 0UL);

    optSrcStageMask &= ~(pCmdBuf->AnyBltActive() ? 0UL : PipelineStageBlt);

    // Remove TopOfPipe and FetchIndirectArgs as they don't cause stall. PipelineStageFetchIndices needs stall VS.
    optSrcStageMask &= ~(PipelineStageTopOfPipe | PipelineStageFetchIndirectArgs);

    return optSrcStageMask;
}

// =====================================================================================================================
// Optimize MemBarrier by modify its srcStageMask/dstStageMask to reduce stall operations.
void Device::OptimizeReadOnlyMemBarrier(
    Pm4CmdBuffer* pCmdBuf,
    MemBarrier*   pTransition
    ) const
{
    const uint32 srcStageMask = pTransition->srcStageMask;
    const uint32 dstStageMask = pTransition->dstStageMask;

    // Don't skip if may miss shader caches invalidation for DST.
    bool canSkip = (TestAnyFlagSet(pTransition->srcAccessMask, CacheCoherShaderReadMask) ||
                   (TestAnyFlagSet(pTransition->dstAccessMask, CacheCoherShaderReadMask) == false));

#if PAL_BUILD_GFX11
    if (Parent()->UsePws(pCmdBuf->GetEngineType()))
    {
        // Don't skip if last barrier acquires later stage than current barrier acquires.
        const EngineType engineType = pCmdBuf->GetEngineType();
        canSkip &= (GetAcquirePoint(srcStageMask, engineType) <= GetAcquirePoint(dstStageMask, engineType));
    }
    else
#endif
    {
        // Don't skip if may miss PFP_SYNC_ME in non-PWS case.
        canSkip &= (TestAnyFlagSet(srcStageMask, PipelineStagePfpMask) ||
                    (TestAnyFlagSet(dstStageMask, PipelineStagePfpMask) == false));
    }

    if (canSkip)
    {
        const uint32 optSrcStageMask = GetOptimizedSrcStagesForReadOnlyBarrier(pCmdBuf, srcStageMask, dstStageMask);

        // Completely remove all of the barrier operations if optSrcStageMask requires releasing nothing.
        if (optSrcStageMask == 0)
        {
            pTransition->srcStageMask  = 0;
            pTransition->dstStageMask  = 0;
            pTransition->srcAccessMask = 0;
            pTransition->dstAccessMask = 0;
        }
        else
        {
            // Replace original srcStageMask with optimizedSrcStageMask to reduce potential stalls.
            pTransition->srcStageMask  = optSrcStageMask;
            // Buffer only: add TopOfPipe and FetchIndirectArgs back if orginal srcStageMask has them, to avoid
            // unnecesary PFP_SYNC_ME if dstStageMask acquires any stage of PipelineStagePfpMask.
            pTransition->srcStageMask |= (srcStageMask & (PipelineStageTopOfPipe | PipelineStageFetchIndirectArgs));
        }
    }
}

// =====================================================================================================================
// Check if can skip this image barrier or try to reduce stall operations by modify its srcStageMask/dstStageMask.
// Caller should make sure there is no layout transition BLT outside.
bool Device::OptimizeReadOnlyImgBarrier(
    Pm4CmdBuffer* pCmdBuf,
    ImgBarrier*   pTransition
    ) const
{
    const uint32 srcStageMask = pTransition->srcStageMask;
    const uint32 dstStageMask = pTransition->dstStageMask;

    // Don't skip if may miss shader caches invalidation for DST.
    bool canSkip = (TestAnyFlagSet(pTransition->srcAccessMask, CacheCoherShaderReadMask) ||
                   (TestAnyFlagSet(pTransition->dstAccessMask, CacheCoherShaderReadMask) == false));

#if PAL_BUILD_GFX11
    if (Parent()->UsePws(pCmdBuf->GetEngineType()))
    {
        const Pal::Image* pImage = static_cast<const Pal::Image*>(pTransition->pImage);

        // Image without metadata never uses CP DMA except CmdCloneImageData(). Safe to replace acquire BLT stage
        // with VS/CS stage on non-cloneable image. This is to skip (NonPsRead|PsRead->CopySrc) in DX12 Control.
        uint32 optDstStageMask = dstStageMask;
        if (TestAnyFlagSet(optDstStageMask, PipelineStageBlt) &&
            (pImage->GetMemoryLayout().metadataSize == 0)     && // No metadata
            (pImage->IsCloneable() == false))
        {
            optDstStageMask &= ~PipelineStageBlt;
            optDstStageMask |= (PipelineStageVs | PipelineStageCs);
        }

        // Don't skip if last barrier acquires later stage than current barrier acquires.
        const EngineType engineType = pCmdBuf->GetEngineType();
        canSkip &= (GetAcquirePoint(srcStageMask, engineType) <= GetAcquirePoint(optDstStageMask, engineType));
    }
    else
#endif
    {
        // Don't skip if may miss PFP_SYNC_ME in non-PWS case.
        canSkip &= (TestAnyFlagSet(srcStageMask, PipelineStagePfpMask) ||
                    (TestAnyFlagSet(dstStageMask, PipelineStagePfpMask) == false));
    }

    if (canSkip)
    {
        // Optimize srcStageMask to reduce potential stalls.
        const uint32 optSrcStageMask = GetOptimizedSrcStagesForReadOnlyBarrier(pCmdBuf, srcStageMask, dstStageMask);

        canSkip                   = (optSrcStageMask == 0);
        pTransition->srcStageMask = optSrcStageMask;
    }

    return canSkip;
}
#endif

// =====================================================================================================================
// Prepare and get all image layout transition info
bool Device::GetAcqRelLayoutTransitionBltInfo(
    Pm4CmdBuffer*                 pCmdBuf,
    CmdStream*                    pCmdStream,
    const AcquireReleaseInfo&     barrierInfo,
    AcqRelTransitionInfo*         pTransitionInfo,
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 767
    uint32*                       pSrcStageMask, // OR with all image srcStageMask as output
    uint32*                       pDstStageMask, // OR with all image dstStageMask as output
#endif
    uint32*                       pSrcAccessMask, // OR with all image srcAccessMask as output
    uint32*                       pDstAccessMask, // OR with all image dstAccessMask as output
    Developer::BarrierOperations* pBarrierOps
    ) const
{
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 767
    PAL_ASSERT((pSrcStageMask != nullptr) && (pDstStageMask != nullptr));
#endif
    PAL_ASSERT((pSrcAccessMask != nullptr) && (pDstAccessMask != nullptr));

    // Assert caller has initialized all members of pTransitonInfo.
    PAL_ASSERT((pTransitionInfo != nullptr) && (pTransitionInfo->pBltList != nullptr) &&
               (pTransitionInfo->bltCount == 0) && (pTransitionInfo->bltStageMask == 0) &&
               (pTransitionInfo->bltAccessMask == 0) && (pTransitionInfo->hasMetadata == false));

    bool refreshTcc = false;

    // Loop through image transitions to update client requested access.
    for (uint32 i = 0; i < barrierInfo.imageBarrierCount; i++)
    {
        ImgBarrier        imageBarrier = barrierInfo.pImageBarriers[i];
        const Pal::Image* pImage       = static_cast<const Pal::Image*>(imageBarrier.pImage);

        PAL_ASSERT(imageBarrier.subresRange.numPlanes == 1);

        // Prepare a layout transition BLT info and do pre-BLT preparation work.
        const LayoutTransitionInfo layoutTransInfo = PrepareBltInfo(pCmdBuf, imageBarrier);

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 767
        // Can safely skip transition between depth read and depth write.
        bool skipTransition = (imageBarrier.srcAccessMask == CoherDepthStencilTarget) &&
                              (imageBarrier.dstAccessMask == CoherDepthStencilTarget);

        // No need acquire at PFP for image barriers. Image may have metadata that's accessed by PFP by
        // it's handled properly internally and no need concern here.
        if (TestAnyFlagSet(imageBarrier.dstStageMask, PipelineStagePfpMask))
        {
            imageBarrier.dstStageMask &= ~PipelineStagePfpMask;
            // If no dstStageMask flag after removing PFP flags, force waiting at ME.
            if (imageBarrier.dstStageMask == 0)
            {
                imageBarrier.dstStageMask = PipelineStageBlt;
            }
        }

        if (IsReadOnlyTransition(imageBarrier.srcAccessMask, imageBarrier.dstAccessMask) &&
            (layoutTransInfo.blt[0] == HwLayoutTransition::None) &&
            // Only make sense to optimize if clients provide src/dstStageMask.
            ((imageBarrier.srcStageMask | imageBarrier.dstStageMask) != 0))
        {
            skipTransition |= OptimizeReadOnlyImgBarrier(pCmdBuf, &imageBarrier);
        }

        if (skipTransition == false)
#endif
        {
            *pSrcAccessMask |= imageBarrier.srcAccessMask;
            *pDstAccessMask |= imageBarrier.dstAccessMask;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 767
            *pSrcStageMask  |= imageBarrier.srcStageMask;
            *pDstStageMask  |= imageBarrier.dstStageMask;
#endif
            uint32 stageMask  = 0;
            uint32 accessMask = 0;
            if (layoutTransInfo.blt[0] != HwLayoutTransition::None)
            {
                GetBltStageAccessInfo(layoutTransInfo, &stageMask, &accessMask);

                (*pTransitionInfo->pBltList)[pTransitionInfo->bltCount].imgBarrier      = imageBarrier;
                (*pTransitionInfo->pBltList)[pTransitionInfo->bltCount].layoutTransInfo = layoutTransInfo;
                (*pTransitionInfo->pBltList)[pTransitionInfo->bltCount].stageMask       = stageMask;
                (*pTransitionInfo->pBltList)[pTransitionInfo->bltCount].accessMask      = accessMask;
                pTransitionInfo->bltCount++;

                // OR current BLT's stageMask/accessMask into a global mask used for an all-in-one pre-BLT acquire.
                pTransitionInfo->bltStageMask  |= stageMask;
                // Optimization: set preBltAccessMask=0 for transition to InitMaskRam since no need cache sync.
                pTransitionInfo->bltAccessMask |= (layoutTransInfo.blt[0] == HwLayoutTransition::InitMaskRam)
                                                  ? 0 : accessMask;
            }

            pTransitionInfo->hasMetadata |= (pImage->GetMemoryLayout().metadataSize != 0);

            // Check refresh L2 WA at Release() call and skip for Acquire() to save CPU overhead.
            if (imageBarrier.srcAccessMask != 0)
            {
                // (accessMask != 0) indicates a layout transition BLT. If don't need a BLT then assume CoherShader
                // in imageBarrier.dstAccessMask is indirect access only.
                const bool   shaderMdAccessIndirectOnly = (accessMask == 0);
                const uint32 dstAccessMaskForWaCall     = (accessMask != 0) ? accessMask : imageBarrier.dstAccessMask;

                refreshTcc |= WaRefreshTccOnMetadataMisalignment(imageBarrier.pImage,
                                                                 imageBarrier.subresRange,
                                                                 imageBarrier.srcAccessMask,
                                                                 dstAccessMaskForWaCall,
                                                                 shaderMdAccessIndirectOnly);
            }

            // For InitMaskRam case, call UpdateDccStateMetaDataIfNeeded after AcqRelInitMaskRam to avoid racing issue.
            if (layoutTransInfo.blt[0] != HwLayoutTransition::InitMaskRam)
            {
                UpdateDccStateMetaDataIfNeeded(pCmdBuf, pCmdStream, &imageBarrier, pBarrierOps);
            }
        }
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
    uint32 postBltStageMask      = 0;
    uint32 postBltAccessMask     = 0;
    bool   postBltRefreshTcc     = false;

    bool   preInitHtileSynced    = false;
    bool   postSyncInitMaskRam   = false;
    bool   syncTccForInitMaskRam = false;

    PAL_ASSERT(pTransitonInfo != nullptr);

    // Issue BLTs.
    for (uint32 i = 0; i < pTransitonInfo->bltCount; i++)
    {
        const AcqRelImgTransitionInfo& transition = (*pTransitonInfo->pBltList)[i];
        const ImgBarrier&              imgBarrier = transition.imgBarrier;

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

            const bool refrechTcc = WaRefreshTccOnMetadataMisalignment(imgBarrier.pImage,
                                                                       imgBarrier.subresRange,
                                                                       accessMask,
                                                                       imgBarrier.dstAccessMask,
                                                                       false); // shaderMdAccessIndirectOnly

            if (csInitMaskRam)
            {
                // Post-sync CsInitMaskRam is handled at end of this function specially. No need update
                // postBltStageMask/postBltAccessMask/postBltRefreshTcc to avoid PostBlt sync outside again.
                postSyncInitMaskRam    = true;
                syncTccForInitMaskRam |= refrechTcc;
            }
            else
            {
                // Add current BLT's stageMask/accessMask into a stageMask/accessMask used for an all-in-one
                // post-BLT release.
                postBltStageMask  |= stageMask;
                postBltAccessMask |= accessMask;
                postBltRefreshTcc |= refrechTcc;
            }
        }
    }

    if (postSyncInitMaskRam)
    {
        uint32* pCmdSpace = pCmdStream->ReserveCommands();

        pCmdSpace = pCmdBuf->WriteWaitCsIdle(pCmdSpace);

        AcquireMemGfxSurfSync acquireInfo = {};
        acquireInfo.cacheSync  = SyncGlvInv | SyncGl1Inv | SyncGlmInv;
        acquireInfo.cacheSync |= syncTccForInitMaskRam ? SyncGl2WbInv : SyncGlxNone;
        pCmdSpace += m_cmdUtil.BuildAcquireMemGfxSurfSync(acquireInfo, pCmdSpace);

        pCmdStream->CommitCommands(pCmdSpace);
        pCmdStream->SetContextRollDetected<false>(); // // acquire_mem packets can cause a context roll.

        pBarrierOps->pipelineStalls.csPartialFlush = 1;
        pBarrierOps->caches.invalTcp               = 1;
        pBarrierOps->caches.invalGl1               = 1;
        pBarrierOps->caches.invalTccMetadata       = 1;
        pBarrierOps->caches.flushTcc               = syncTccForInitMaskRam;
        pBarrierOps->caches.invalTcc               = syncTccForInitMaskRam;
    }

    // Output bltStageMask and bltAccessMask for release BLTs. Generally they're the same as the input values in
    // pTransitonInfo, but if (layoutTransInfo.blt[1] != HwLayoutTransition::None), they may be different and we
    // should update the values to release blt[1] correctly.
    pTransitonInfo->bltStageMask  = postBltStageMask;
    pTransitonInfo->bltAccessMask = postBltAccessMask;

    return postBltRefreshTcc;
}

// =====================================================================================================================
// Wrapper to call RPM's InitMaskRam to issues a compute shader blt to initialize the Mask RAM allocations for an Image.
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

    // This queue must support this barrier transition.
    PAL_ASSERT(engineProps.flags.supportsImageInitBarrier == 1);
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

            const bool refreshTcc = WaRefreshTccOnMetadataMisalignment(&image, imgBarrier.subresRange,
                                                                       srcAccessMask, dstAccessMask, false);

            IssueReleaseThenAcquireSync(pCmdBuf, pCmdStream, srcStageMask, dstStageMask,
                                        srcAccessMask, dstAccessMask, refreshTcc, true, pBarrierOps);

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

#if PAL_BUILD_GFX11
            if (Parent()->UsePws(engineType))
            {
                releaseMem.usePws = 1;

                if (syncToken.type == uint32(AcqRelEventType::Eop))
                {
                    releaseMem.dataSel = data_sel__me_release_mem__none;
                }
                else
                {
                    // Note: PWS+ doesn't need timestamp write, we pass in a dummy write just to meet RELEASE_MEM
                    //       packet programming requirement for DATA_SEL field, where 0=none (Discard data) is not
                    //       a valid option when EVENT_INDEX=shader_done (PS_DONE/CS_DONE).
                    releaseMem.dataSel = data_sel__me_release_mem__send_32_bit_low;
                    releaseMem.dstAddr = pCmdBuf->TimestampGpuVirtAddr();
                }
            }
            else
#endif
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
    bool                          mayHaveMetadata,
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

    const SyncGlxFlags acquireCaches = GetAcquireCacheFlags(accessMask, refreshTcc, mayHaveMetadata, pBarrierOps);
    AcquirePoint       acquirePoint  = GetAcquirePoint(stageMask, engineType);

    // Must acquire at PFP/ME if cache syncs are required.
    if ((acquireCaches != SyncGlxNone) && (acquirePoint > AcquirePoint::Me))
    {
        acquirePoint = AcquirePoint::Me;
    }

    uint32* pCmdSpace = pCmdStream->ReserveCommands();

    uint32  syncTokenToWait[uint32(AcqRelEventType::Count)] = {};
    bool    hasValidSyncToken = false;

    // Merge synchronization timestamp entries in the list.
    // Can safely skip Acquire if acquire point is EOP and no cache sync. If there is cache sync, acquire point has
    // been forced to ME by above codes.
    if (acquirePoint != AcquirePoint::Eop)
    {
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
    }

#if PAL_BUILD_GFX11
    // If no sync token is specified to be waited for completion, there is no need for a PWS-version of ACQUIRE_MEM.
    if (hasValidSyncToken && Parent()->UsePws(engineType))
    {
        // Wait on the PWS+ event via ACQUIRE_MEM.
        AcquireMemGfxPws acquireMem = {};
        acquireMem.cacheSync = acquireCaches;
        acquireMem.stageSel  = GetPwsStageSel(acquirePoint);

        static_assert((uint32(AcqRelEventType::Eop)    == uint32(pws_counter_sel__me_acquire_mem__ts_select__HASPWS)) &&
                      (uint32(AcqRelEventType::PsDone) == uint32(pws_counter_sel__me_acquire_mem__ps_select__HASPWS)) &&
                      (uint32(AcqRelEventType::CsDone) == uint32(pws_counter_sel__me_acquire_mem__cs_select__HASPWS)),
                      "Enum orders mismatch! Fix the ordering so the following for-loop runs correctly.");

        for (uint32 i = 0; i < uint32(AcqRelEventType::Count); i++)
        {
            if (syncTokenToWait[i] != 0)
            {
                const uint32 curSyncToken = pCmdBuf->GetCurAcqRelFenceVal(AcqRelEventType(i));
                const uint32 numEventsAgo = curSyncToken - syncTokenToWait[i];

                PAL_ASSERT(syncTokenToWait[i] <= curSyncToken);

                acquireMem.counterSel = ME_ACQUIRE_MEM_pws_counter_sel_enum(i);
                acquireMem.syncCount  = Util::Clamp(numEventsAgo, 0U, MaxNumPwsSyncEvents - 1U);

                pCmdSpace += m_cmdUtil.BuildAcquireMemGfxPws(acquireMem, pCmdSpace);
            }
        }

        pBarrierOps->pipelineStalls.waitOnTs   = 1;
        pBarrierOps->pipelineStalls.pfpSyncMe |= (acquirePoint == AcquirePoint::Pfp);
        needPfpSyncMe = false;
    }
    else
#endif
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
    if ((waitedEopFenceVal != 0) && (acquirePoint <= AcquirePoint::Me))
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
    }

    const uint32 waitedCsDoneFenceVal = syncTokenToWait[uint32(AcqRelEventType::CsDone)];

    if ((waitedCsDoneFenceVal != 0) &&
        (acquirePoint <= AcquirePoint::Me) &&
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
    bool                          mayHaveMetadata,
    Developer::BarrierOperations* pBarrierOps
    ) const
{
    const EngineType engineType     = pCmdBuf->GetEngineType();
    // In theory, no need PfpSyncMe if both srcStageMask and dstStageMask access stage flag in PipelineStagePfpMask.
    // But it's unsafe to optimize it here as srcStageMask and dstStageMask are combination of multiple transitions.
    // e.g. CmdReleaseThenAcquire() is called with two buffer transitions: one is from Cs(Uav)->CsIndirectArgs and
    // the other is from CsIndirectArgs->Cs(ShaderRead); we should NOT skip PFP_SYNC_ME in this case although we see
    // srcStageMask -> dstSrcStageMask = Cs|IndirectArgs -> Cs|IndirectArgs.
    bool             needPfpSyncMe  = TestAnyFlagSet(dstStageMask, PipelineStagePfpMask) &&
                                      pCmdBuf->IsGraphicsSupported();
    uint32*          pCmdSpace      = pCmdStream->ReserveCommands();

    if (pCmdBuf->IsGraphicsSupported() == false)
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

    AcquirePoint     acquirePoint  = GetAcquirePoint(dstStageMask, engineType);
    SyncGlxFlags     acquireCaches = GetReleaseThenAcquireCacheFlags(srcAccessMask, dstAccessMask,
                                                                     refreshTcc, mayHaveMetadata, pBarrierOps);
    ReleaseEvents    releaseEvents = GetReleaseEvents(pCmdBuf, *this, srcStageMask, srcAccessMask,
                                                      false, acquireCaches, acquirePoint);
    ReleaseMemCaches releaseCaches = {};

    const Pm4CmdBufferStateFlags cmdBufStateFlags = pCmdBuf->GetPm4CmdBufState().flags;

    // Preprocess release events, acquire caches and point for optimization or HW limitation before issue real packet.
    if (acquirePoint > AcquirePoint::Me)
    {
        // Optimization: convert acquireCaches to releaseCaches and do GCR op at release Eop instead of acquire
        //               at ME stage so acquire can wait at a later point.
        // Note that converting acquireCaches to releaseCaches is a correctness requirement when skip the acquire
        // at EOP wait.
        if (releaseEvents.eop && (acquireCaches != SyncGlxNone))
        {
            releaseCaches = m_cmdUtil.SelectReleaseMemCaches(&acquireCaches);
            PAL_ASSERT(acquireCaches == SyncGlxNone);
        }

        // HW limitation: Can only do GCR op at ME stage for Acquire.
        // Optimization : issue lighter VS_PARTIAL_FLUSH instead of PWS+ packet which needs bump VsDone to
        //                heavier PsDone or Eop.
        if ((acquireCaches != SyncGlxNone) || releaseEvents.vs)
        {
            acquirePoint = AcquirePoint::Me;
        }

        // Optimization: we want to acquire at a later stage and we also expect cmdBufStateFlags.gfxBltActive/
        //               gfxWriteCachesDirty can be optimized to 0 so the following release of BLT stage can be
        //               skipped. It's really a balance here, if acquire stage is PreShader which is near ME stage,
        //               let's force waiting at ME stage so cmdBufStateFlags can be optimized to 0.
        if ((acquirePoint == AcquirePoint::PreShader) &&
            (releaseEvents.eop != 0) &&
            (cmdBufStateFlags.gfxBltActive || cmdBufStateFlags.gfxWriteCachesDirty))
        {
            acquirePoint = AcquirePoint::Me;
        }
    }

    // Optimization: If this is an Eop release acquiring at PFP/ME stage and cmdBufStateFlags.gfxWriteCachesDirty
    //               is active, let's force releaseEvents.rbCache = 1 so cmdBufStateFlags.gfxWriteCachesDirty can
    //               be optimized to 0 at end of this function.
    if (releaseEvents.eop && (acquirePoint <= AcquirePoint::Me))
    {
        releaseEvents.rbCache |= cmdBufStateFlags.gfxWriteCachesDirty;
    }

    if (releaseEvents.rbCache)
    {
        PAL_ASSERT(releaseEvents.eop); // rbCache sync must have Eop event set.
        Pm4CmdBuffer::SetBarrierOperationsRbCacheSynced(pBarrierOps);
    }

    if (acquirePoint != AcquirePoint::Eop)
    {
#if PAL_BUILD_GFX11
        // Go PWS+ wait if PWS+ is supported and
        //     (1) If release a Eop event (PWS+ path performs better than legacy path)
        //     (2) Or if use a later PWS-only acquire point and need release either PsDone or CsDone.
        if ((releaseEvents.eop && Parent()->UsePws(engineType)) ||
            ((acquirePoint > AcquirePoint::Me) && (releaseEvents.ps || releaseEvents.cs)))
        {
            // No VsDone as it should go through non-PWS path.
            PAL_ASSERT(releaseEvents.vs == 0);

            ReleaseMemGfx releaseMem = {};
            releaseMem.usePws    = 1;
            releaseMem.cacheSync = releaseCaches;

            ME_ACQUIRE_MEM_pws_counter_sel_enum pwsCounterSel;
            if (releaseEvents.eop)
            {
                releaseMem.dataSel  = data_sel__me_release_mem__none;
                releaseMem.vgtEvent = releaseEvents.rbCache ? CACHE_FLUSH_AND_INV_TS_EVENT : BOTTOM_OF_PIPE_TS;
                pwsCounterSel       = pws_counter_sel__me_acquire_mem__ts_select__HASPWS;
                pBarrierOps->pipelineStalls.eopTsBottomOfPipe = 1;

                // Handle cases where a stall is needed as a workaround before EOP with CB Flush event
                if (releaseEvents.rbCache && TestAnyFlagSet(Settings().waitOnFlush, WaitBeforeBarrierEopWithCbFlush))
                {
                    pCmdSpace = pCmdBuf->WriteWaitEop(HwPipePreColorTarget, SyncGlxNone, SyncRbNone, pCmdSpace);
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
                    pwsCounterSel       = pws_counter_sel__me_acquire_mem__ps_select__HASPWS;
                    pBarrierOps->pipelineStalls.eosTsPsDone = 1;
                }
                else
                {
                    PAL_ASSERT(releaseEvents.cs);
                    releaseMem.vgtEvent = CS_DONE;
                    pwsCounterSel       = pws_counter_sel__me_acquire_mem__cs_select__HASPWS;
                    pBarrierOps->pipelineStalls.eosTsCsDone = 1;
                }
            }

            pCmdSpace += m_cmdUtil.BuildReleaseMemGfx(releaseMem, pCmdSpace);

            // If need issue CsDone RELEASE_MEM/ACQUIRE_MEM event when both PsDone and CsDone are active.
            if (releaseEvents.ps && releaseEvents.cs)
            {
                PAL_ASSERT(releaseEvents.eop == 0);
                releaseMem.vgtEvent = CS_DONE;
                pCmdSpace += m_cmdUtil.BuildReleaseMemGfx(releaseMem, pCmdSpace);
                pBarrierOps->pipelineStalls.eosTsCsDone = 1;
            }

            // Wait on the PWS+ event via ACQUIRE_MEM
            AcquireMemGfxPws acquireMem = {};
            acquireMem.cacheSync  = acquireCaches;
            acquireMem.stageSel   = GetPwsStageSel(acquirePoint);
            acquireMem.counterSel = pwsCounterSel;
            acquireMem.syncCount  = 0;

            pCmdSpace += m_cmdUtil.BuildAcquireMemGfxPws(acquireMem, pCmdSpace);

            if (releaseEvents.ps && releaseEvents.cs)
            {
                acquireMem.counterSel = pws_counter_sel__me_acquire_mem__cs_select__HASPWS;
                pCmdSpace += m_cmdUtil.BuildAcquireMemGfxPws(acquireMem, pCmdSpace);
            }

            acquireCaches = SyncGlxNone; // Clear all SyncGlxFlags as done in BuildAcquireMemGfxPws().
            needPfpSyncMe = false;

            pBarrierOps->pipelineStalls.waitOnTs   = 1;
            pBarrierOps->pipelineStalls.pfpSyncMe |= (acquirePoint == AcquirePoint::Pfp);
        }
        else
#endif
        {
            if (releaseEvents.eop)
            {
                const SyncRbFlags rbSync = releaseEvents.rbCache ? SyncRbWbInv : SyncRbNone;
                pCmdSpace = pCmdBuf->WriteWaitEop(HwPipePostPrefetch, SyncGlxNone, rbSync, pCmdSpace);

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
        }
    }
    else // acquirePoint == AcquirePoint::Eop
    {
        if (releaseEvents.rbCache || (releaseCaches.u8All != 0))
        {
            PAL_ASSERT(releaseEvents.eop); // Must be Eop event to sync RB or GCR cache.

            // Need issue GCR.gl2Inv/gl2Wb and RB cache sync in single ReleaseMem packet to avoid racing issue.
            // Note that it's possible GCR.glkInv is left in acquireCaches for cases glkInv isn't supported in
            // ReleaseMem packet. This case has been handled in if path since acquirePoint has been changed to Me
            // in optimization codes above the if branch.
            ReleaseMemGfx releaseMem = {};
            releaseMem.cacheSync = releaseCaches;
            releaseMem.vgtEvent  = releaseEvents.rbCache ? CACHE_FLUSH_AND_INV_TS_EVENT : BOTTOM_OF_PIPE_TS;
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

    // If we have stalled at Eop or Cs, update some CmdBufState (e.g. xxxBltActive) flags.
    if (acquirePoint <= AcquirePoint::Me)
    {
        if (releaseEvents.eop)
        {
            pCmdBuf->SetPrevCmdBufInactive();
            pCmdBuf->SetPm4CmdBufGfxBltState(false);

            if (releaseEvents.rbCache)
            {
                pCmdBuf->SetPm4CmdBufGfxBltWriteCacheState(false);
            }
        }

        if (releaseEvents.eop || releaseEvents.cs)
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
        const auto* pPublicSettings = m_pParent->GetPublicSettings();

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
                                   (pPublicSettings->expandHiZRangeForResummarize ||
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

        // This queue must support this barrier transition.
        PAL_ASSERT(engineProps.flags.supportsImageInitBarrier == 1);
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
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 767
    uint32 srcGlobalStageMask  = releaseInfo.srcGlobalStageMask;
#else
    uint32 srcGlobalStageMask  = releaseInfo.srcStageMask;
#endif
    uint32 srcGlobalAccessMask = releaseInfo.srcGlobalAccessMask;

    // Check if global barrier needs refresh L2
    bool globalRefreshTcc = WaRefreshTccOnMetadataMisalignment(nullptr, {}, srcGlobalAccessMask, 0, true);

    // Always do full-range flush sync.
    for (uint32 i = 0; i < releaseInfo.memoryBarrierCount; i++)
    {
        const MemBarrier& barrier = releaseInfo.pMemoryBarriers[i];

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 767
        srcGlobalStageMask  |= barrier.srcStageMask;
#endif
        // globallyAvailable is processed in Acquire().
        srcGlobalAccessMask |= barrier.srcAccessMask;
    }

    // A container to cache the calculated BLT transitions and some cache info for reuse.
    AcqRelAutoBuffer transitionList(releaseInfo.imageBarrierCount, GetPlatform());
    AcqRelSyncToken  syncToken = {};

    if (transitionList.Capacity() >= releaseInfo.imageBarrierCount)
    {
        // If BLTs will be issued, we need to know how to acquire for them.
        AcqRelTransitionInfo transInfo        = { &transitionList, 0, 0, 0, false };
        uint32               unusedStageMask  = 0;
        uint32               unusedAccessMask = 0;

        globalRefreshTcc |= GetAcqRelLayoutTransitionBltInfo(pCmdBuf,
                                                             pCmdStream,
                                                             releaseInfo,
                                                             &transInfo,
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 767
                                                             &srcGlobalStageMask,
                                                             &unusedStageMask,
#endif
                                                             &srcGlobalAccessMask,
                                                             &unusedAccessMask,
                                                             pBarrierOps);

        // Issue BLTs if there exists transitions that require one.
        if (transInfo.bltCount > 0)
        {
            // Issue all-in-one ReleaseThenAcquire prior to the potential BLTs.
            IssueReleaseThenAcquireSync(pCmdBuf,
                                        pCmdStream,
                                        srcGlobalStageMask,
                                        transInfo.bltStageMask,
                                        srcGlobalAccessMask,
                                        transInfo.bltAccessMask,
                                        globalRefreshTcc,
                                        true, // Image with Layout BLT should have metadata
                                        pBarrierOps);

            globalRefreshTcc = IssueAcqRelLayoutTransitionBlt(pCmdBuf, pCmdStream, &transInfo, pBarrierOps);

            // Override srcGlobalStageMask and srcGlobalAccessMask to release from BLTs.
            srcGlobalStageMask  = transInfo.bltStageMask;
            srcGlobalAccessMask = transInfo.bltAccessMask;
        }

        syncToken = IssueReleaseSync(pCmdBuf,
                                     pCmdStream,
                                     srcGlobalStageMask,
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
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 767
    uint32 dstGlobalStageMask  = acquireInfo.dstGlobalStageMask;
#else
    uint32 dstGlobalStageMask  = acquireInfo.dstStageMask;
#endif
    uint32 dstGlobalAccessMask = acquireInfo.dstGlobalAccessMask;
    bool   globalRefreshTcc    = false;

    // Assume worst case that global barrier may contain images with metadata.
    bool mayHaveMatadata = (dstGlobalAccessMask != 0);

    // Always do full-range flush sync.
    for (uint32 i = 0; i < acquireInfo.memoryBarrierCount; i++)
    {
        const MemBarrier& barrier = acquireInfo.pMemoryBarriers[i];

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 767
        dstGlobalStageMask  |= barrier.dstStageMask;
#endif
        dstGlobalAccessMask |= barrier.dstAccessMask;
        dstGlobalAccessMask |= barrier.flags.globallyAvailable ? CoherMemory : 0;
    }

    // A container to cache the calculated BLT transitions and some cache info for reuse.
    AcqRelAutoBuffer transitionList(acquireInfo.imageBarrierCount, GetPlatform());

    if (transitionList.Capacity() >= acquireInfo.imageBarrierCount)
    {
        // If BLTs will be issued, we need to know how to acquire for them.
        AcqRelTransitionInfo transInfo        = { &transitionList, 0, 0, 0, false };
        uint32               unusedStageMask  = 0;
        uint32               unusedAccessMask = 0;

        const uint32 preBltRefreshTcc = GetAcqRelLayoutTransitionBltInfo(pCmdBuf,
                                                                         pCmdStream,
                                                                         acquireInfo,
                                                                         &transInfo,
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 767
                                                                         &unusedStageMask,
                                                                         &dstGlobalStageMask,
#endif
                                                                         &unusedAccessMask,
                                                                         &dstGlobalAccessMask,
                                                                         pBarrierOps);
        mayHaveMatadata |= transInfo.hasMetadata;

        // Should have no L2 refresh for image barrier as this is done at Release().
        PAL_ASSERT(preBltRefreshTcc == false);

        // Issue acquire for global or pre-BLT sync. No need stall if wait at bottom of pipe
        const uint32 acquireDstStageMask  = (transInfo.bltCount > 0) ? transInfo.bltStageMask : dstGlobalStageMask;
        const uint32 acquireDstAccessMask = (transInfo.bltCount > 0) ? transInfo.bltAccessMask : dstGlobalAccessMask;

        IssueAcquireSync(pCmdBuf,
                         pCmdStream,
                         acquireDstStageMask,
                         acquireDstAccessMask,
                         globalRefreshTcc,
                         mayHaveMatadata,
                         syncTokenCount,
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
                                        dstGlobalStageMask,
                                        transInfo.bltAccessMask,
                                        dstGlobalAccessMask,
                                        globalRefreshTcc,
                                        true, // Image with Layout BLT should have metadata
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
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 767
    uint32 srcGlobalStageMask  = barrierInfo.srcGlobalStageMask;
    uint32 dstGlobalStageMask  = barrierInfo.dstGlobalStageMask;
#else
    uint32 srcGlobalStageMask  = barrierInfo.srcStageMask;
    uint32 dstGlobalStageMask  = barrierInfo.dstStageMask;
#endif
    uint32 srcGlobalAccessMask = barrierInfo.srcGlobalAccessMask;
    uint32 dstGlobalAccessMask = barrierInfo.dstGlobalAccessMask;

    // Check if global barrier needs refresh L2
    bool globalRefreshTcc = WaRefreshTccOnMetadataMisalignment(nullptr,
                                                               {},
                                                               srcGlobalAccessMask,
                                                               dstGlobalAccessMask,
                                                               true);

    // Assume worst case that global barrier may contain images with metadata.
    bool mayHaveMatadata = ((srcGlobalAccessMask | dstGlobalAccessMask) != 0);

    // Always do full-range flush sync.
    for (uint32 i = 0; i < barrierInfo.memoryBarrierCount; i++)
    {
        MemBarrier barrier = barrierInfo.pMemoryBarriers[i];

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 767
        if (IsReadOnlyTransition(barrier.srcAccessMask, barrier.dstAccessMask) &&
            // Only make sense to optimize if clients provide src/dstStageMask.
            ((barrier.srcStageMask | barrier.dstStageMask) != 0))
        {
            OptimizeReadOnlyMemBarrier(pCmdBuf, &barrier);
        }

        srcGlobalStageMask  |= barrier.srcStageMask;
        dstGlobalStageMask  |= barrier.dstStageMask;
#endif
        srcGlobalAccessMask |= barrier.srcAccessMask;
        dstGlobalAccessMask |= barrier.dstAccessMask;
        dstGlobalAccessMask |= barrier.flags.globallyAvailable ? CoherMemory : 0;
    }

    // A container to cache the calculated BLT transitions and some cache info for reuse.
    AcqRelAutoBuffer transitionList(barrierInfo.imageBarrierCount, GetPlatform());

    if (transitionList.Capacity() >= barrierInfo.imageBarrierCount)
    {
        // If BLTs will be issued, we need to know how to acquire for them.
        AcqRelTransitionInfo transInfo = { &transitionList, 0, 0, 0, false };

        globalRefreshTcc |= GetAcqRelLayoutTransitionBltInfo(pCmdBuf,
                                                             pCmdStream,
                                                             barrierInfo,
                                                             &transInfo,
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 767
                                                             &srcGlobalStageMask,
                                                             &dstGlobalStageMask,
#endif
                                                             &srcGlobalAccessMask,
                                                             &dstGlobalAccessMask,
                                                             pBarrierOps);
        mayHaveMatadata |= transInfo.hasMetadata;

        // Issue BLTs if there exists transitions that require one.
        if (transInfo.bltCount > 0)
        {
            IssueReleaseThenAcquireSync(pCmdBuf,
                                        pCmdStream,
                                        srcGlobalStageMask,
                                        transInfo.bltStageMask,
                                        srcGlobalAccessMask,
                                        transInfo.bltAccessMask,
                                        globalRefreshTcc,
                                        true, // Image with Layout BLT should have metadata
                                        pBarrierOps);

            globalRefreshTcc = IssueAcqRelLayoutTransitionBlt(pCmdBuf, pCmdStream, &transInfo, pBarrierOps);

            // Override srcGlobalStageMask and srcGlobalAccessMask to release from BLTs.
            srcGlobalStageMask  = transInfo.bltStageMask;
            srcGlobalAccessMask = transInfo.bltAccessMask;
        }

        // Issue acquire for global sync.
        IssueReleaseThenAcquireSync(pCmdBuf,
                                    pCmdStream,
                                    srcGlobalStageMask,
                                    dstGlobalStageMask,
                                    srcGlobalAccessMask,
                                    dstGlobalAccessMask,
                                    globalRefreshTcc,
                                    mayHaveMatadata,
                                    pBarrierOps);
    }
    else
    {
        pCmdBuf->NotifyAllocFailure();
    }
}

// =====================================================================================================================
// Helper function that issues requested transition for the image barrier.
// Returns "true" if there is InitMaskRam operation and it's done by compute engine.
bool Device::IssueBlt(
    Pm4CmdBuffer*                 pCmdBuf,
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
    const BarrierTransition rgpTransition = AcqRelBuildTransition(pImgBarrier, layoutTransInfo, pBarrierOps);
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
#if PAL_BUILD_GFX11
                // The most efficient way to wait for DB-idle and flush and invalidate the DB caches on pre-gfx11 HW
                // is an acquire_mem. Gfx11 can't touch the DB caches using an acquire_mem but that's OK because we
                // expect WriteWaitEop to do a PWS EOP wait which should be fast.
                if (IsGfx11(*Parent()))
                {
                    pCmdSpace = pCmdBuf->WriteWaitEop(HwPipePostPrefetch, SyncGlvInv|SyncGl1Inv, SyncDbWbInv, pCmdSpace);
                    waitEop = true;
                }
                else
#endif
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

                pBarrierOps->caches.flushDb         = 1;
                pBarrierOps->caches.invalDb         = 1;
                pBarrierOps->caches.flushDbMetadata = 1;
                pBarrierOps->caches.invalDbMetadata = 1;
            }
            else
            {
                pCmdSpace = pCmdBuf->WriteWaitEop(HwPipePostPrefetch, SyncGlvInv|SyncGl1Inv, SyncRbNone, pCmdSpace);
                waitEop = true;
            }

            pCmdStream->CommitCommands(pCmdSpace);

            pBarrierOps->caches.invalTcp = 1;
            pBarrierOps->caches.invalGl1 = 1;
            if (waitEop)
            {
                pBarrierOps->pipelineStalls.eopTsBottomOfPipe = 1;
                pBarrierOps->pipelineStalls.waitOnTs          = 1;
                *pPreInitHtileSynced                          = true;
            }
        }

        // RGP expects DescribeBarrier() call occurs before layout transition BLT.
        DescribeBarrier(pCmdBuf, &rgpTransition, pBarrierOps);

        // Transition out of LayoutUninitializedTarget needs to initialize metadata memories.
        csInitMaskRam = AcqRelInitMaskRam(pCmdBuf, pCmdStream, *pImgBarrier);

        // DXC only waits resource alias barrier on ME stage. Need UpdateDccStateMetaData (via PFP) after InitMaskRam
        // which contains an inside PfpSyncMe before initializing MaskRam. So the PfpSyncMe can prevent a racing issue
        // that DccStateMetadata update (by PFP) is done while resource memory to be aliased is stilled being used.
        UpdateDccStateMetaDataIfNeeded(pCmdBuf, pCmdStream, pImgBarrier, pBarrierOps);
    }
    else
    {
        // RGP expects DescribeBarrier() call occurs before layout transition BLT.
        DescribeBarrier(pCmdBuf, &rgpTransition, pBarrierOps);

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
            IssueAcquireSyncEvent(pCmdBuf, pCmdStream, stageMask, accessMask, false, true, 1, &pEvent, pBarrierOps);

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
    bool                          mayHaveMetadata,
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

    const SyncGlxFlags acquireCaches = GetAcquireCacheFlags(accessMask, refreshTcc, mayHaveMetadata, pBarrierOps);

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
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 767
    uint32 srcGlobalStageMask  = releaseInfo.srcGlobalStageMask;
#else
    uint32 srcGlobalStageMask  = releaseInfo.srcStageMask;
#endif
    uint32 srcGlobalAccessMask = releaseInfo.srcGlobalAccessMask;

    // Check if global barrier needs refresh L2
    bool globalRefreshTcc = WaRefreshTccOnMetadataMisalignment(nullptr, {}, srcGlobalAccessMask, 0, true);

    // Always do full-range flush sync.
    for (uint32 i = 0; i < releaseInfo.memoryBarrierCount; i++)
    {
        const MemBarrier& barrier = releaseInfo.pMemoryBarriers[i];

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 767
        srcGlobalStageMask  |= barrier.srcStageMask;
#endif
        // globallyAvailable is processed in AcquireEvent().
        srcGlobalAccessMask |= barrier.srcAccessMask;
    }

    // A container to cache the calculated BLT transitions and some cache info for reuse.
    AcqRelAutoBuffer transitionList(releaseInfo.imageBarrierCount, GetPlatform());

    if (transitionList.Capacity() >= releaseInfo.imageBarrierCount)
    {
        // If BLTs will be issued, we need to know how to acquire for them.
        AcqRelTransitionInfo transInfo        = { &transitionList, 0, 0, 0, false };
        uint32               unusedStageMask  = 0;
        uint32               unusedAccessMask = 0;

        globalRefreshTcc |= GetAcqRelLayoutTransitionBltInfo(pCmdBuf,
                                                             pCmdStream,
                                                             releaseInfo,
                                                             &transInfo,
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 767
                                                             &srcGlobalStageMask,
                                                             &unusedStageMask,
#endif
                                                             &srcGlobalAccessMask,
                                                             &unusedAccessMask,
                                                             pBarrierOps);

        // Initialize an IGpuEvent* pEvent pointing at the client provided event.
        // If we have internal BLTs, use internal event to signal/wait.
        const IGpuEvent* pActiveEvent = (transInfo.bltCount > 0) ? pCmdBuf->GetInternalEvent() : pClientEvent;

        // Perform an all-in-one release prior to the potential BLTs: IssueReleaseSyncEvent() on pActiveEvent.
        // Defer L2 refresh at acquire if has BLT.
        IssueReleaseSyncEvent(pCmdBuf,
                              pCmdStream,
                              srcGlobalStageMask,
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
                                  true,  // mayHaveMatadata. Layout BLT images should have metadata.
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
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 767
    uint32 dstGlobalStageMask  = acquireInfo.dstGlobalStageMask;
#else
    uint32 dstGlobalStageMask  = acquireInfo.dstStageMask;
#endif
    uint32 dstGlobalAccessMask = acquireInfo.dstGlobalAccessMask;
    bool   globalRefreshTcc    = false;

    // Assume worst case that global barrier may contain images with metadata.
    bool mayHaveMatadata = (dstGlobalAccessMask != 0);

    // Always do full-range flush sync.
    for (uint32 i = 0; i < acquireInfo.memoryBarrierCount; i++)
    {
        const MemBarrier& barrier = acquireInfo.pMemoryBarriers[i];

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 767
        dstGlobalStageMask  |= barrier.dstStageMask;
#endif
        dstGlobalAccessMask |= barrier.dstAccessMask;
        dstGlobalAccessMask |= barrier.flags.globallyAvailable ? CoherMemory : 0;
    }

    // A container to cache the calculated BLT transitions and some cache info for reuse.
    AcqRelAutoBuffer transitionList(acquireInfo.imageBarrierCount, GetPlatform());

    if (transitionList.Capacity() >= acquireInfo.imageBarrierCount)
    {
        // If BLTs will be issued, we need to know how to acquire for them.
        AcqRelTransitionInfo transInfo        = { &transitionList, 0, 0, 0, false };
        uint32               unusedStageMask  = 0;
        uint32               unusedAccessMask = 0;

        const uint32 preBltRefreshTcc = GetAcqRelLayoutTransitionBltInfo(pCmdBuf,
                                                                         pCmdStream,
                                                                         acquireInfo,
                                                                         &transInfo,
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 767
                                                                         &unusedStageMask,
                                                                         &dstGlobalStageMask,
#endif
                                                                         &unusedAccessMask,
                                                                         &dstGlobalAccessMask,
                                                                         pBarrierOps);
        mayHaveMatadata |= transInfo.hasMetadata;

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
                                  true,  // mayHaveMatadata. Layout BLT images should have metadata.
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

        IssueAcquireSyncEvent(pCmdBuf,
                              pCmdStream,
                              dstGlobalStageMask,
                              dstGlobalAccessMask,
                              globalRefreshTcc,
                              mayHaveMatadata,
                              activeEventCount,
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
