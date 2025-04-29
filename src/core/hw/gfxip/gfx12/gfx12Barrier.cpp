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

#include "palSysMemory.h"
#include "core/device.h"
#include "core/hw/gfxip/gfxCmdBuffer.h"
#include "core/hw/gfxip/gfx12/gfx12Barrier.h"
#include "core/hw/gfxip/gfx12/gfx12Device.h"
#include "core/hw/gfxip/gfx12/gfx12Image.h"
#include "core/hw/gfxip/gfx12/gfx12Metadata.h"
#include "core/hw/gfxip/rpm/gfx12/gfx12RsrcProcMgr.h"

using namespace Util;

namespace Pal
{
namespace Gfx12
{

// Define required HW release info: release events and RB cache flags.
struct ReleaseEvents
{
    uint32 eventTypeMask; // Bitmask combination of event type, defined in ReleaseTokenTypeMask.
    bool   waitVsDone;
    bool   syncRbCache;

    bool HasValidEvents() const { return (eventTypeMask != 0) || waitVsDone || syncRbCache; }
};

// Mask of all GPU memory read may go through GL0 cache (K$, V$).
static constexpr uint32 CacheCoherGl0ReadMask  = CoherShaderRead | CoherSampleRate | CacheCoherencyBltSrc;

// Mask of all GPU memory write may go through GL0 cache (V$).
static constexpr uint32 CacheCoherGl0WriteMask = CoherShaderWrite | CoherStreamOut | CacheCoherencyBltDst;

// Mask of all GPU memory access may go through GL2 cache.
static constexpr uint32 CacheCoherGl2AccessMask = CacheCoherGl0ReadMask | CacheCoherGl0WriteMask |
                                                  CacheCoherRbAccessMask;

// Mask of all GPU memory access bypasses GL2 cache, e.g. through mall directly.
// Note that on gfx12, clear and resolve are always done by dispatch.
//
// DX12 app creates streamout buffer filled size allocation as a separate resource; and may call CmdClearColorBuffer()
// to it. Client driver calls CmdLoadBufferFilledSizes() and CmdSaveBufferFilledSizes at streamout target bind/unbind
// time to load/save buffer filled size info to/from internal streamout control buffer, which is done by CP and GL2
// cache is bypassed. DX12 uses STREAM_OUT state (driver translates to CoherStreamOut) for both streamout target and
// buffer filled size resource.
// Note that CP PFP accesses buffer filled size allocation content directly when loading or saving it; CP FW adds
// PFP_SYNC_ME internally so driver doesn't need take care of this.
static constexpr uint32 CacheCoherBypassGl2AccessMask = CoherCpu          | CoherCopySrc   | CoherCopyDst     |
                                                        CoherIndirectArgs | CoherIndexData | CoherQueueAtomic |
                                                        CoherTimestamp    | CoherMemory    | CoherPresent     |
                                                        CoherCp           | CoherStreamOut;

// None cache sync operations
static constexpr CacheSyncOps NullCacheSyncOps = {};

// =====================================================================================================================
static void ConvertSyncGlxFlagsToBarrierOps(
    SyncGlxFlags                  syncGlxFlags,
    Developer::BarrierOperations* pBarrierOps)
{
    pBarrierOps->caches.invalTcp  |= TestAnyFlagSet(syncGlxFlags, SyncGlvInv);
    pBarrierOps->caches.invalSqI$ |= TestAnyFlagSet(syncGlxFlags, SyncGliInv);
    pBarrierOps->caches.invalSqK$ |= TestAnyFlagSet(syncGlxFlags, SyncGlkInv);
    pBarrierOps->caches.flushTcc  |= TestAnyFlagSet(syncGlxFlags, SyncGl2Wb);
    pBarrierOps->caches.invalTcc  |= TestAnyFlagSet(syncGlxFlags, SyncGl2Inv);
}

// =====================================================================================================================
// Helper to write commands to ensure timestamp writes are confirmed
static uint32* WriteTimestampSync(
    EngineType     engineType,
    gpusize        dstAddr,
    uint32*        pCmdSpace,
    const CmdUtil& cmdUtil)
{
    // Force a EOP ReleaseMem with confirmed writes to ensure the EOP write path is flushed because all writes
    // must be confirmed for the last here to be confirmed
    ReleaseMemGeneric releaseMem = {};
    releaseMem.vgtEvent    = BOTTOM_OF_PIPE_TS;
    releaseMem.dataSel     = data_sel__me_release_mem__send_gpu_clock_counter;
    releaseMem.dstAddr     = dstAddr;

    pCmdSpace += cmdUtil.BuildReleaseMemGeneric(releaseMem, pCmdSpace);

    // WriteData with dontWriteConfirm=false which ensures all prior writes are complete
    WriteDataInfo writeData = {};
    writeData.engineType    = engineType;
    writeData.engineSel     = engine_sel__me_write_data__micro_engine;
    writeData.dstSel        = dst_sel__me_write_data__memory;
    writeData.dstAddr       = dstAddr;

    pCmdSpace += CmdUtil::BuildWriteData(writeData, 0, pCmdSpace);

    return pCmdSpace;
}

// =====================================================================================================================
// Get required events (e.g. Eop/Vs/Ps/Cs) need to be released from srcStageMask. Also check if need sync RB cache.
static ReleaseEvents GetReleaseEvents(
    uint32                        srcStageMask,
    CacheSyncOps                  cacheOps,
    AcquirePoint                  acquirePoint,
    Developer::BarrierOperations* pBarrierOps)
{
    // Detect cases where no global execution barrier is required because the acquire point is later than the
    // pipeline stages being released.
    constexpr uint32 StallReqStageMask[] =
    {
        // Pfp       = 0
        VsPsCsWaitStageMask | EopWaitStageMask,
        // Me        = 1
        VsPsCsWaitStageMask | EopWaitStageMask,
        // PreShader = 2
        VsPsCsWaitStageMask | EopWaitStageMask,
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
        // Eop       = 6 (Invalid)
        0,
    };

    ReleaseEvents release = {};

    if (srcStageMask & StallReqStageMask[acquirePoint])
    {
        // Optimization: for stageMask transition Ps|Cs->Rt/Ps|Ds with GCR operation, convert PsDone+CsDone to Eop so
        //               can wait at a later PreColor/PrePs/PreDepth point; otherwise PS_PARTIAL_FLUSH/CS_PARTIAL_FLUSH
        //               waits at ME stage.
        if (TestAnyFlagSet(srcStageMask, EopWaitStageMask) ||
            ((acquirePoint >= AcquirePointPreDepth)        &&
             (cacheOps.glxFlags != SyncGlxNone)            &&
             TestAnyFlagSet(srcStageMask, CsWaitStageMask) &&
             TestAnyFlagSet(srcStageMask, PsWaitStageMask)))
        {
            release.eventTypeMask = ReleaseTokenMaskEop;
        }
        else
        {
            release.eventTypeMask  = (TestAnyFlagSet(srcStageMask, PsWaitStageMask) ? ReleaseTokenMaskPsDone : 0) |
                                     (TestAnyFlagSet(srcStageMask, CsWaitStageMask) ? ReleaseTokenMaskCsDone : 0);
            // PsDone event and PS_PARTIAL_FLUSH can make sure all VS waves done.
            release.waitVsDone     = TestAnyFlagSet(srcStageMask, VsWaitStageMask) &&
                                     (TestAnyFlagSet(srcStageMask, PsWaitStageMask) == false);
        }
    }

    if (cacheOps.rbCache)
    {
        release.eventTypeMask = ReleaseTokenMaskEop; // No need release other events if release Eop.
        release.syncRbCache   = true;
        release.waitVsDone    = false;

        GfxBarrierMgr::SetBarrierOperationsRbCacheSynced(pBarrierOps);
    }
    else if (cacheOps.timestamp &&
             (release.waitVsDone || TestAnyFlagSet(release.eventTypeMask, ~ReleaseTokenMaskEop)))
    {
        // We confirm prior timestamp writes using a pipelined EOP event. If this barrier uses any non-EOP stalls
        // we must force an EOP stall to ensure that the acquire is synchronous with the TS write confirm.
        release.eventTypeMask = ReleaseTokenMaskEop;
        release.waitVsDone    = false;
    }

    if (acquirePoint == AcquirePointEop)
    {
        // If acquire at bottom pipe but no any cache op, can safely skip the barrier.
        // Minor optimization: If need sync cache, bump to Eop event to sync all caches in Release (no need acquire)
        release.eventTypeMask = (cacheOps == NullCacheSyncOps) ? 0 : ReleaseTokenMaskEop;
        release.waitVsDone    = false;
    }

    return release;
}

// =====================================================================================================================
// Get a PWS+ acquire point from dstStageMask.
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

    // Convert global dstStageMask to HW acquire point.
    AcquirePoint acqPoint = (dstStageMask & AcqPfpStages)      ? AcquirePointPfp      :
                            (dstStageMask & AcqMeStages)       ? AcquirePointMe       :
                            (dstStageMask & AcqPreDepthStages) ? AcquirePointPreDepth :
                                                                 AcquirePointEop;

    // If PwsLateAcquirePointEnabled == false, should clamp all (except Eop/Pfp) to Me.
    if (((acqPoint == AcquirePointPreDepth) && (m_pDevice->UsePwsLateAcquirePoint(engineType) == false)) ||
        ((acqPoint == AcquirePointPfp) && (engineType != EngineTypeUniversal))) // No Pfp on non-universal engine.
    {
        acqPoint = AcquirePointMe;
    }

    return acqPoint;
}

// =====================================================================================================================
// Helper function to optimize pipeline access masks for BLTs. This is for acquire/release interface.
// This function also mask off all graphics path specific stage mask flags for non-universal command buffer as well as
// remove some invalid pfp stage mask on dstStageMask to avoid unnecessary PFP_SYNC_ME stall.
void BarrierMgr::OptimizeStageMask(
    const GfxCmdBuffer* pCmdBuf,
    BarrierType         barrierType,
    uint32*             pSrcStageMask,  // Optional, can be nullptr
    uint32*             pDstStageMask,  // Optional, can be nullptr
    bool                isClearToTarget // Not used
    ) const
{
    const GfxCmdBufferStateFlags stateFlags = pCmdBuf->GetCmdBufState().flags;

    // Should be no GFX BLT except auto syn clear where this flag is not set.
    PAL_ASSERT(stateFlags.gfxBltActive == 0);

    if (pSrcStageMask != nullptr)
    {
        // Update pipeline stages if valid input stage mask is provided.
        if (TestAnyFlagSet(*pSrcStageMask, PipelineStageBlt))
        {
            *pSrcStageMask &= ~PipelineStageBlt;
            *pSrcStageMask |= stateFlags.csBltActive ? PipelineStageCs : 0;

            // Only buffer and global barrier use CP DMA blt potentially.
            if (barrierType != BarrierType::Image)
            {
                // Add back PipelineStageBlt because we cannot express it with a more accurate stage.
                *pSrcStageMask |= stateFlags.cpBltActive ? PipelineStageBlt : 0;
            }
        }

        // Mark off all graphics path specific stages and caches if command buffer doesn't support graphics.
        if (pCmdBuf->GetEngineType() != EngineTypeUniversal)
        {
            *pSrcStageMask &= ~PipelineStagesGraphicsOnly;
        }
    }

    if (pDstStageMask != nullptr)
    {
        // No need acquire at PFP for image barriers. Image may have metadata that's accessed by PFP by
        // it's handled properly internally and no need concern here.
        if ((barrierType == BarrierType::Image) &&
            TestAnyFlagSet(*pDstStageMask, PipelineStagePfpMask))
        {
            *pDstStageMask &= ~PipelineStagePfpMask;

            // If no dstStageMask flag after removing PFP flags, force waiting at ME.
            if (*pDstStageMask == 0)
            {
                *pDstStageMask = PipelineStagePostPrefetch;
            }
        }

        // Mark off all graphics path specific stages and caches if command buffer doesn't support graphics.
        if (pCmdBuf->GetEngineType() != EngineTypeUniversal)
        {
            *pDstStageMask &= ~PipelineStagesGraphicsOnly;
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
    const Pal::Image*   pImage,                    // Not used in gfx12.
    uint32*             pSrcAccessMask,
    uint32*             pDstAccessMask,
    bool                shaderMdAccessIndirectOnly // Not used in gfx12.
    ) const
{
    PAL_ASSERT((pSrcAccessMask != nullptr) && (pDstAccessMask != nullptr));

    const GfxCmdBufferStateFlags stateFlags = pCmdBuf->GetCmdBufState().flags;

    // Should be no GFX BLT except auto syn clear where this flag is not set.
    PAL_ASSERT(stateFlags.gfxWriteCachesDirty == 0);

    // Update cache access masks if valid input access mask is provided.
    if (TestAnyFlagSet(*pSrcAccessMask, CacheCoherencyBlt))
    {
        if (stateFlags.csWriteCachesDirty)
        {
            *pSrcAccessMask |= (TestAnyFlagSet(*pSrcAccessMask, CacheCoherencyBltSrc) ? CoherShaderRead  : 0) |
                               (TestAnyFlagSet(*pSrcAccessMask, CacheCoherencyBltDst) ? CoherShaderWrite : 0);
        }

        // Only buffer and global barrier (potentially contains buffer) CoherCopy case may use CP DMA blt.
        if ((barrierType != BarrierType::Image) && TestAnyFlagSet(*pSrcAccessMask, CoherCopy))
        {
            *pSrcAccessMask |= (stateFlags.cpMemoryWriteL2CacheStale  ? CoherMemory : 0);
        }

        // Must be here as above codes check with CacheCoherencyBlt.
        *pSrcAccessMask &= ~CacheCoherencyBlt;
    }

    // Can optimize dstAccessMask for image barrier specially as image RPM blts always go through compute except
    // auto sync clear which doesn't need barrier management here. This could potentially reduce GL2 sync.
    if ((barrierType == BarrierType::Image) &&
        TestAnyFlagSet(*pDstAccessMask, CacheCoherencyBlt))
    {
        *pDstAccessMask |= (TestAnyFlagSet(*pDstAccessMask, CacheCoherencyBltSrc) ? CoherShaderRead  : 0) |
                           (TestAnyFlagSet(*pDstAccessMask, CacheCoherencyBltDst) ? CoherShaderWrite : 0);

        // Must be here as above codes check with CacheCoherencyBlt.
        *pDstAccessMask &= ~CacheCoherencyBlt;
    }

    // Mark off all graphics path specific stages and caches if command buffer doesn't support graphics.
    if (pCmdBuf->GetEngineType() != EngineTypeUniversal)
    {
        *pSrcAccessMask &= ~CacheCoherencyGraphicsOnly;
        *pDstAccessMask &= ~CacheCoherencyGraphicsOnly;
    }

    return false;
}

// =====================================================================================================================
// Generate required cache ops for single global/buffer/image transition on Release, Acquire and ReleaseThenAcquire call.
// Return required cache sync operations.
CacheSyncOps BarrierMgr::GetCacheSyncOps(
    GfxCmdBuffer* pCmdBuf,
    BarrierType   barrierType,
    const IImage* pImage,
    uint32        srcAccessMask,
    uint32        dstAccessMask
    ) const
{
    const uint32 orgSrcAccessMask = srcAccessMask;

    // Optimize BLT coherency flags into explicit flags.
    OptimizeAccessMask(pCmdBuf, barrierType, nullptr, &srcAccessMask, &dstAccessMask, false);

    CacheSyncOps cacheOps = {};

    if (TestAnyFlagSet(srcAccessMask, CoherTimestamp))
    {
        cacheOps.timestamp = true;
    }

    // V$ and GL2 partial cache line writes to DF/MALL via byte enables/mask.
    // - If this is an image with compression enabled, DF/MALL does the RMW: decompress, update with new data, and
    //   then only do simple recompress (comp-to-single, clear).
    // - Shader write doesn't support Z plane compression and driver needs explicit resummarization.
    //
    // Can skip GL0/GL2 invalidation when transition to shader write as newly written data will override old data
    // in cache line. e.g. from ShaderRead -> ShaderWrite.

    // On gfx12, GL2 is coherent across SEs but CP is connected to mall directly (not through GL2). Generally, all
    // kinds of GPU access can be grouped into four: GL2Read, GL2Write, BypassGL2Read and BypassGL2Write.
    //
    // It's hard to track the accurate cache operation due to PAL only holds one-step transition access info.
    // For example of Gl2Read->BypassGL2Read, PAL doesn't know if really need GL2 flush here. If transition chain is
    // "BypassGL2Read->Gl2Read->BypassGL2Read", then it's safe to skip all cache op here since data was already coherent
    // in mall; however if it's "G2Write->Gl2Read->BypassGL2Read", need flush GL2 here. PAL should assume the worst case
    // for safe. For simple, only need consider transition between Gl2Access and BypasGl2Access.
    //
    // The most common transition is (Gl2Access<->Gl2Access) like between RB and TC for image and buffer access. For
    // less cache operation (better performance) in split barrier, the code logic below will try to keep GL2 coherent:
    //   - If previous access (srcAccessMask) bypasses GL2, invalidate GL2 to avoid GL2 to be stale.
    //   - If next access (dstAccessMask) bypasses GL2, flush GL2 to mall to make mall have fresh data.

    // dstAccessMask == 0 is for split barrier, assume the worst case.
    if (TestAnyFlagSet(srcAccessMask, CacheCoherBypassGl2AccessMask) &&
        ((dstAccessMask == 0) || TestAnyFlagSet(dstAccessMask, CacheCoherGl2AccessMask)))
    {
        cacheOps.glxFlags |= SyncGl2Inv;
        // Always flush GL2 cache in case invGl2 discards valid data in GL2 cache.
        cacheOps.glxFlags |= SyncGl2Wb;
    }
    // srcAccessMask == 0 is for split barrier, assume the worst case.
    else if (TestAnyFlagSet(dstAccessMask, CacheCoherBypassGl2AccessMask) &&
             ((srcAccessMask == 0) || TestAnyFlagSet(srcAccessMask, CacheCoherGl2AccessMask)))
    {
        cacheOps.glxFlags |= SyncGl2Wb;
    }

    // Optimization: can skip GL0 invalidation if previously read through GL0 caches and about to access through
    //               GL0 caches again. Don't apply the optimization on global transition.
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
    const auto* pPalImage     = static_cast<const Pal::Image*>(pImage);
    const bool noSkipCacheInv = (pPalImage != nullptr) && pPalImage->IsCloneable()        &&
                                ((TestAnyFlagSet(orgSrcAccessMask, CoherCopySrc)          &&
                                  TestAnyFlagSet(dstAccessMask, CacheCoherGl0ReadMask))   ||
                                 (TestAnyFlagSet(orgSrcAccessMask, CacheCoherGl0ReadMask) &&
                                  TestAnyFlagSet(dstAccessMask, CoherCopySrc)));
    const bool skipGL0Inv     = (barrierType != BarrierType::Global)                             &&
                                (noSkipCacheInv == false)                                        &&
                                (TestAnyFlagSet(orgSrcAccessMask, CacheCoherWriteMask) == false) &&
                                TestAnyFlagSet(orgSrcAccessMask, CacheCoherGl0ReadMask);

    if (TestAnyFlagSet(dstAccessMask, CacheCoherGl0ReadMask) && (skipGL0Inv == false))
    {
        cacheOps.glxFlags |= SyncGlvInv | SyncGlkInv;
    }

    // dstAccessMask == 0 is for split barrier, assume the worst case.
    // Skip RB cache sync for back to back color or depth stencil write.
    if (TestAnyFlagSet(srcAccessMask, CacheCoherRbAccessMask) &&
        ((dstAccessMask == 0) || TestAnyFlagSet(srcAccessMask | dstAccessMask, ~CacheCoherRbAccessMask)))
    {
        cacheOps.rbCache = true;
    }

    return cacheOps;
}

// =====================================================================================================================
// Maps AcquirePoint to PWS_STAGE_SEL value as defined in the PM4 spec.
static ME_ACQUIRE_MEM_pws_stage_sel_enum GetPwsStageSel(
    AcquirePoint acquirePoint)
{
    static constexpr ME_ACQUIRE_MEM_pws_stage_sel_enum PwsStageSelMapTable[] =
    {
        pws_stage_sel__me_acquire_mem__cp_pfp,    // AcquirePointPfp       = 0
        pws_stage_sel__me_acquire_mem__cp_me,     // AcquirePointMe        = 1
        pws_stage_sel__me_acquire_mem__pre_depth, // AcquirePointPreDepth  = 2
        pws_stage_sel__me_acquire_mem__pre_depth, // AcquirePointEop       = 3
    };

    static_assert((ArrayLen32(PwsStageSelMapTable) == AcquirePointCount), "Need update above mapping table!");

    PAL_ASSERT(acquirePoint < AcquirePointCount);

    return PwsStageSelMapTable[acquirePoint];
}

// =====================================================================================================================
// Check if the image needs layout transition BLT based on provided layout info.
BarrierMgr::LayoutTransition BarrierMgr::GetLayoutTransitionType(
    const IImage*      pImage,
    const SubresRange& subresRange,
    ImageLayout        oldLayout,
    ImageLayout        newLayout)
{
    const auto&       palImage        = static_cast<const Pal::Image&>(*pImage);
    const auto&       gfx12Image      = static_cast<const Image&>(*palImage.GetGfxImage());
    const ImageLayout hiSZValidLayout = gfx12Image.GetHiSZValidLayout(subresRange);

    LayoutTransition tranType = LayoutTransition::None;

    // Only check if the image has valid HiSZ layout usages.
    if (hiSZValidLayout.usages != 0)
    {
        if ((oldLayout.usages == 0) && (newLayout.usages == 0))
        {
            // Default no layout transition if zero usages are provided.
        }
        else if (TestAnyFlagSet(newLayout.usages, LayoutUninitializedTarget))
        {
            // If the LayoutUninitializedTarget usage is set, no other usages should be set.
            PAL_ASSERT(TestAnyFlagSet(newLayout.usages, ~LayoutUninitializedTarget) == false);

            // We do no blt in this case.
        }
        else if (TestAnyFlagSet(oldLayout.usages, LayoutUninitializedTarget))
        {
            tranType = LayoutTransition::InitMaskRam;
        }
        else
        {
            const DepthStencilHiSZState oldState = ImageLayoutToDepthStencilHiSZState(hiSZValidLayout, oldLayout);
            const DepthStencilHiSZState newState = ImageLayoutToDepthStencilHiSZState(hiSZValidLayout, newLayout);

            if ((oldState == DepthStencilNoHiSZ) && (newState == DepthStencilWithHiSZ))
            {
                tranType = LayoutTransition::ExpandHiSZRange;
            }
        }
    }

    return tranType;
}

// =====================================================================================================================
BarrierMgr::BarrierMgr(
    GfxDevice*     pGfxDevice)
    :
    GfxBarrierMgr(pGfxDevice),
    m_gfxDevice(*static_cast<Device*>(pGfxDevice)),
    m_cmdUtil(m_gfxDevice.CmdUtil())
{
}

// =====================================================================================================================
// Loop from the collection list to do all layout transition BLT.
CacheSyncOps BarrierMgr::IssueLayoutTransitionBlt(
    GfxCmdBuffer*                  pCmdBuf,
    const ImgLayoutTransitionList& bltList,
    uint32                         bltCount,
    uint32*                        pPostBltStageMask, // [Out]: Post blt stageMask need to be released.
    Developer::BarrierOperations*  pBarrierOps
    ) const
{
    const RsrcProcMgr& gfx12RsrcProcMgr     = static_cast<const RsrcProcMgr&>(pCmdBuf->GetGfxDevice().RsrcProcMgr());
    CacheSyncOps       cacheOps             = {};
    bool               postSyncInitMetadata = false;

    PAL_ASSERT(pPostBltStageMask != nullptr);
    *pPostBltStageMask = 0;

    for (uint32 i = 0; i < bltCount; i++)
    {
        const ImgTransitionInfo& imgTrans    = bltList[i];
        const ImgBarrier&        imgBarrier  = imgTrans.imgBarrier;
        const SubresRange&       subresRange = imgBarrier.subresRange;

        switch (imgTrans.type)
        {
        case LayoutTransition::InitMaskRam:
            pBarrierOps->layoutTransitions.initMaskRam = 1;
            break;
        case LayoutTransition::ExpandHiSZRange:
            pBarrierOps->layoutTransitions.htileHiZRangeExpand = 1;
            break;
        default:
            PAL_NEVER_CALLED();
            break;
        }

        // Tell RGP about this transition
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 902
        DescribeBarrier(pCmdBuf, &imgBarrier, pBarrierOps);
#else
        BarrierTransition rgpTrans = {};
        rgpTrans.srcCacheMask                 = imgBarrier.srcAccessMask;
        rgpTrans.dstCacheMask                 = imgBarrier.dstAccessMask;
        rgpTrans.imageInfo.pImage             = imgBarrier.pImage;
        rgpTrans.imageInfo.subresRange        = subresRange;
        rgpTrans.imageInfo.oldLayout          = imgBarrier.oldLayout;
        rgpTrans.imageInfo.newLayout          = imgBarrier.newLayout;
        rgpTrans.imageInfo.pQuadSamplePattern = imgBarrier.pQuadSamplePattern;
        DescribeBarrier(pCmdBuf, &rgpTrans, pBarrierOps);
#endif

        switch (imgTrans.type)
        {
        case LayoutTransition::InitMaskRam: // Fall through
        case LayoutTransition::ExpandHiSZRange:
        {
            const auto& palImage   = static_cast<const Pal::Image&>(*imgBarrier.pImage);
            const auto& gfx12Image = static_cast<const Image&>(*palImage.GetGfxImage());

            // Process one plane in each ExpandHiSZWithFullRange() call.
            SubresRange range = subresRange;
            range.numPlanes = 1;

            for (uint32 planeIndex = 0; planeIndex < subresRange.numPlanes; planeIndex++)
            {
                const uint32 plane = subresRange.startSubres.plane + planeIndex;

                range.startSubres.plane = plane;

                // Non zero HiSZ valid layout indicates HiZ or HiS there and requires an layout trans blt here.
                if (gfx12Image.GetHiSZValidLayout(plane).usages != 0)
                {
                    gfx12RsrcProcMgr.ExpandHiSZWithFullRange(pCmdBuf, *imgBarrier.pImage, range, true);
                }
            }

            // Only update GPU state metadata if both depth and stencil are handled in the barrier.
            if (gfx12Image.HasHiSZStateMetaData() && palImage.IsRangeFullSlices(subresRange))
            {
                // Expand the other plane so can safely re-enable HiSZ.
                if (subresRange.numPlanes == 1)
                {
                    const HiSZ* pHiSZ = gfx12Image.GetHiSZ();

                    range.startSubres.plane = (subresRange.startSubres.plane == 0) ? 1 : 0;

                    // Note: This is only necessary if both HiZ and HiS are enabled.
                    if (((range.startSubres.plane == 0) && pHiSZ->HiZEnabled()) ||
                        ((range.startSubres.plane == 1) && pHiSZ->HiSEnabled()))
                    {
                        gfx12RsrcProcMgr.ExpandHiSZWithFullRange(pCmdBuf, *imgBarrier.pImage, range, true);
                    }
                }

                const auto  pktPredicate = static_cast<Pm4Predicate>(pCmdBuf->GetPacketPredicate());
                auto* const pCmdStream   = static_cast<CmdStream*>(pCmdBuf->GetMainCmdStream());
                uint32*     pCmdSpace    = pCmdStream->ReserveCommands();

                pCmdSpace = gfx12Image.UpdateHiSZStateMetaData(subresRange, true, pktPredicate,
                                                               pCmdBuf->GetEngineType(), pCmdSpace);

                pCmdStream->CommitCommands(pCmdSpace);
            }
            break;
        }
        default:
            PAL_NEVER_CALLED();
            break;
        }

        cacheOps |= GetCacheSyncOps(pCmdBuf, BarrierType::Image, imgBarrier.pImage, BltAccessMask,
                                    imgBarrier.dstAccessMask);

        if (imgTrans.type == LayoutTransition::InitMaskRam)
        {
            // Post-stall for InitMaskRam is handled at end of this function specially, so no need update
            // postBltStageMask to avoid PostBlt sync outside again. Set required cache op in case clients doesn't
            // provide dstAccessMask; defer cache op with syncGlxFlags to be issued at a later time.
            cacheOps.glxFlags    |= SyncGlvInv;
            postSyncInitMetadata  = true;
        }
        else
        {
            // Add current BLT's stageMask into a stageMask used for an all-in-one post-BLT release.
            *pPostBltStageMask |= BltStageMask;
        }
    }

    // If clients pass with dstStageMask = PipelineStageBottomOfPipe (may be not aware yet that how this resource will
    // be used in the next access), then the sync of InitMaskRam will not be done. So stall it here immediately.
    // Note that defer the cache operation with syncGlxFlags at a later time.
    if (postSyncInitMetadata)
    {
        auto* const pCmdStream = static_cast<CmdStream*>(pCmdBuf->GetMainCmdStream());
        uint32*     pCmdSpace  = pCmdStream->ReserveCommands();

        pCmdSpace = pCmdBuf->WriteWaitCsIdle(pCmdSpace);

        pCmdStream->CommitCommands(pCmdSpace);

        pBarrierOps->pipelineStalls.csPartialFlush = 1;
    }

    return cacheOps;
}

// =====================================================================================================================
ReleaseToken BarrierMgr::IssueReleaseSync(
    GfxCmdBuffer*                 pCmdBuf,
    uint32                        srcStageMask,
    bool                          releaseBufferCopyOnly,
    CacheSyncOps                  cacheOps,
    const GpuEvent*               pClientEvent,
    Developer::BarrierOperations* pBarrierOps
    ) const
{
    const bool    isReleaseTokenPath = (pClientEvent == nullptr);
    ReleaseEvents releaseEvents      = GetReleaseEvents(srcStageMask, cacheOps, AcquirePointPfp, pBarrierOps);
    const bool    waitCpDma          = NeedWaitCpDma(pCmdBuf, srcStageMask);
    ReleaseToken  syncToken          = {};

    PAL_ASSERT((pClientEvent == nullptr) || pClientEvent->GetBoundGpuMemory().IsBound());

    // For release sync token path, optimize to defer wait and cache op to acquire side if there is only CpDma wait and
    // this is release buffer copy only.
    // Cache op must happen after stall complete (wait CpDma idle) otherwise cache may be dirty again due to running
    // CpDma blt. Since CpDma wait is deferred, cache op must be deferred to acquire time as well.
    if (isReleaseTokenPath && waitCpDma && releaseBufferCopyOnly && (releaseEvents.HasValidEvents() == false))
    {
        syncToken.type       = ReleaseTokenCpDma;
        syncToken.fenceValue = pCmdBuf->GetNextAcqRelFenceVal(ReleaseTokenCpDma);
    }
    else
    {
        const EngineType engineType   = pCmdBuf->GetEngineType();
        CmdStream* const pCmdStream   = static_cast<CmdStream*>(pCmdBuf->GetMainCmdStream());
        uint32*          pCmdSpace    = pCmdStream->ReserveCommands();
        SyncGlxFlags     syncGlxFlags = cacheOps.glxFlags;

        ConvertSyncGlxFlagsToBarrierOps(syncGlxFlags, pBarrierOps);

        const ReleaseMemCaches releaseCaches = CmdUtil::SelectReleaseMemCaches(&syncGlxFlags);

        // No VsDone event. PsDone event can make sure all VS waves done, convert VsDone to PsDone.
        if (releaseEvents.waitVsDone)
        {
            releaseEvents.eventTypeMask |= ReleaseTokenMaskPsDone;
            releaseEvents.waitVsDone     = false;
        }

        // HW limitation:
        // - Can only do GCR op at EOP for Release;
        // - Only support single event, need convert PsDone && CsDone to EOP.
        if (((releaseCaches.u8All != 0) && (releaseEvents.eventTypeMask != 0)) ||
            TestAllFlagsSet(releaseEvents.eventTypeMask, ReleaseTokenMaskPsCsDone))
        {
            releaseEvents.eventTypeMask = ReleaseTokenMaskEop;
        }

        // Note that release event flags for split barrier should meet below conditions,
        //    1). No VsDone as it should be converted to PsDone or Eop.
        //    2). PsDone and CsDone should have been already converted to Eop.
        //    3). rbCache sync must have Eop event set.
        PAL_ASSERT(releaseEvents.waitVsDone == false);
        PAL_ASSERT(TestAllFlagsSet(releaseEvents.eventTypeMask, ReleaseTokenMaskPsCsDone) == false);
        PAL_ASSERT((releaseEvents.syncRbCache == false) ||
                   TestAnyFlagSet(releaseEvents.eventTypeMask, ReleaseTokenMaskEop));

        const ReleaseTokenType eventType =
            TestAnyFlagSet(releaseEvents.eventTypeMask, ReleaseTokenMaskEop)    ? ReleaseTokenEop    :
            TestAnyFlagSet(releaseEvents.eventTypeMask, ReleaseTokenMaskPsDone) ? ReleaseTokenPsDone :
            TestAnyFlagSet(releaseEvents.eventTypeMask, ReleaseTokenMaskCsDone) ? ReleaseTokenCsDone :
                                                                                  ReleaseTokenInvalid;

        bool releaseMemWaitCpDma = false;

        if (waitCpDma)
        {
            if (m_gfxDevice.EnableReleaseMemWaitCpDma() && (eventType != ReleaseTokenInvalid))
            {
                releaseMemWaitCpDma = true;
            }
            else
            {
                pCmdSpace += CmdUtil::BuildWaitDmaData(pCmdSpace);
            }
            pBarrierOps->pipelineStalls.syncCpDma = 1;
            pCmdBuf->SetCpBltState(false);
        }

        if (cacheOps.timestamp)
        {
            pCmdSpace = WriteTimestampSync(engineType, pCmdBuf->GetReleaseMemTsGpuVa(), pCmdSpace, m_cmdUtil);
        }

        if (eventType != ReleaseTokenInvalid)
        {
            ReleaseMemGeneric releaseMem = {};
            releaseMem.cacheSync = releaseCaches;
            releaseMem.waitCpDma = releaseMemWaitCpDma;

            switch (eventType)
            {
            case ReleaseTokenEop:
                releaseMem.vgtEvent = releaseEvents.syncRbCache ? CACHE_FLUSH_AND_INV_TS_EVENT : BOTTOM_OF_PIPE_TS;
                pBarrierOps->pipelineStalls.eopTsBottomOfPipe = 1;
                break;
            case ReleaseTokenPsDone:
                releaseMem.vgtEvent = PS_DONE;
                pBarrierOps->pipelineStalls.eosTsPsDone = 1;
                break;
            case ReleaseTokenCsDone:
                releaseMem.vgtEvent = CS_DONE;
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

                if (pCmdBuf->GetDevice().UsePws(engineType))
                {
                    releaseMem.usePws = true;
                    if (eventType == ReleaseTokenEop)
                    {
                        releaseMem.dataSel = data_sel__me_release_mem__none;
                    }
                    else
                    {
                        // Note: PWS+ doesn't need timestamp write, we pass in a dummy write just to meet RELEASE_MEM
                        //       packet programming requirement for DATA_SEL field, where 0=none (Discard data) is not
                        //       a valid option when EVENT_INDEX=shader_done (PS_DONE/CS_DONE).
                        releaseMem.dataSel = data_sel__me_release_mem__send_32_bit_low;
                        releaseMem.dstAddr = pCmdBuf->GetReleaseMemTsGpuVa();
                    }
                }
                else
                {
                    releaseMem.dataSel = data_sel__me_release_mem__send_32_bit_low;
                    releaseMem.dstAddr = pCmdBuf->GetAcqRelFenceGpuVa(eventType, &pCmdSpace);
                    releaseMem.data    = syncToken.fenceValue;
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

                releaseMem.dataSel   = data_sel__me_release_mem__send_32_bit_low;
                releaseMem.dstAddr   = writeData.dstAddr;
                releaseMem.data      = GpuEvent::SetValue;
            }

            pCmdSpace += m_cmdUtil.BuildReleaseMemGeneric(releaseMem, pCmdSpace);
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

                pCmdSpace += CmdUtil::BuildAcquireMemGeneric(acqMem, pCmdSpace);
            }
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
void BarrierMgr::IssueAcquireSync(
    GfxCmdBuffer*                 pCmdBuf,
    uint32                        dstStageMask,
    CacheSyncOps                  cacheOps,
    uint32                        syncTokenCount,
    const ReleaseToken*           pSyncTokens,
    Developer::BarrierOperations* pBarrierOps
    ) const
{
    const EngineType engineType   = pCmdBuf->GetEngineType();
    CmdStream* const pCmdStream   = static_cast<CmdStream*>(pCmdBuf->GetMainCmdStream());
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
                cacheOps |= GetCacheSyncOps(pCmdBuf, BarrierType::Buffer, nullptr, CoherCopy, 0);

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

                static_assert((ReleaseTokenEop    == uint32(pws_counter_sel__me_acquire_mem__ts_select)) &&
                              (ReleaseTokenPsDone == uint32(pws_counter_sel__me_acquire_mem__ps_select)) &&
                              (ReleaseTokenCsDone == uint32(pws_counter_sel__me_acquire_mem__cs_select)),
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

                        pCmdSpace += CmdUtil::BuildAcquireMemGfxPws(acquireMem, pCmdSpace);
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
                        const gpusize fenceGpuVa = pCmdBuf->GetAcqRelFenceGpuVa(ReleaseTokenType(i), &pCmdSpace);

                        pCmdSpace +=
                            CmdUtil::BuildWaitRegMem(engineType,
                                                     mem_space__me_wait_reg_mem__memory_space,
                                                     function__me_wait_reg_mem__greater_than_or_equal_reference_value,
                                                     engine_sel__me_wait_reg_mem__micro_engine,
                                                     fenceGpuVa,
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

                // If we have waited on a valid EOP fence value, update some CmdBufState (e.g. xxxBltActive) flags.
                if (syncTokenToWait[ReleaseTokenEop] != 0)
                {
                    pCmdBuf->SetPrevCmdBufInactive();
                }

                // An EOP or CS_DONE release sync that is issued after the latest CS BLT must have completed, so mark
                // CS BLT idle.
                const GfxCmdBufferState& cmdBufState = pCmdBuf->GetCmdBufState();

                if ((syncTokenToWait[ReleaseTokenEop] >= cmdBufState.fences.csBltExecEopFenceVal) ||
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

            pCmdSpace += CmdUtil::BuildAcquireMemGeneric(acquireMem, pCmdSpace);
        }

        if (acquirePoint == AcquirePointPfp)
        {
            pCmdSpace += CmdUtil::BuildPfpSyncMe(pCmdSpace);
        }
    }

    ConvertSyncGlxFlagsToBarrierOps(cacheOps.glxFlags, pBarrierOps);

    pBarrierOps->pipelineStalls.pfpSyncMe |= (acquirePoint == AcquirePointPfp);

#if PAL_DEVELOPER_BUILD
    pBarrierOps->acquirePoint = ConvertToDeveloperAcquirePoint(acquirePoint);
#endif

    pCmdStream->CommitCommands(pCmdSpace);
}

// =====================================================================================================================
void BarrierMgr::IssueReleaseThenAcquireSync(
    GfxCmdBuffer*                 pCmdBuf,
    uint32                        srcStageMask,
    uint32                        dstStageMask,
    CacheSyncOps                  cacheOps,
    Developer::BarrierOperations* pBarrierOps
    ) const
{
    const EngineType    engineType    = pCmdBuf->GetEngineType();
    auto* const         pCmdStream    = static_cast<CmdStream*>(pCmdBuf->GetMainCmdStream());
    uint32*             pCmdSpace     = pCmdStream->ReserveCommands();
    AcquirePoint        acquirePoint  = GetAcquirePoint(dstStageMask, engineType);
    const ReleaseEvents releaseEvents = GetReleaseEvents(srcStageMask, cacheOps, acquirePoint, pBarrierOps);
    SyncGlxFlags        syncGlxFlags  = cacheOps.glxFlags;
    const bool          syncSrcCaches = TestAllFlagsSet(syncGlxFlags, SyncGl2WbInv | SyncGlkInv | SyncGlvInv);
    const bool          invSrcCaches  = TestAllFlagsSet(syncGlxFlags, SyncGl2Inv | SyncGlkInv | SyncGlvInv);
    bool                usePws        = false;

    ConvertSyncGlxFlagsToBarrierOps(syncGlxFlags, pBarrierOps);

    if (TestAnyFlagSet(releaseEvents.eventTypeMask, ReleaseTokenMaskEop))
    {
        usePws = (acquirePoint != AcquirePointEop) && pCmdBuf->GetDevice().UsePws(engineType);
    }
    else
    {
        // HW limitation: Can only do GCR op at ME stage for Acquire.
        // Optimization:  issue lighter VS_PARTIAL_FLUSH (which waits at ME) instead of PWS+ packet which needs
        //                bump VsDone to heavier PsDone or EOP.
        //
        // If no release event but with late acquire point, force it to be ME so it can go through the right path to
        // handle cache operation correctly.
        if ((acquirePoint > AcquirePointMe) &&
            ((syncGlxFlags != SyncGlxNone) || releaseEvents.waitVsDone || (releaseEvents.eventTypeMask == 0)))
        {
            acquirePoint = AcquirePointMe;
        }

        // No PsDone/CsDone->acquire_Eop case as if no cache op, GetReleaseEvents() will override eventTypeMask to 0;
        // otherwise if there is cache op, acquire point will be forced to ME with above codes.
        usePws = (releaseEvents.eventTypeMask != 0) &&
                 (acquirePoint > AcquirePointMe)    &&
                 pCmdBuf->GetDevice().UsePws(engineType);
    }

#if PAL_DEVELOPER_BUILD
    pBarrierOps->acquirePoint = ConvertToDeveloperAcquirePoint(acquirePoint);
#endif

    bool releaseMemWaitCpDma = false;

    if (NeedWaitCpDma(pCmdBuf, srcStageMask))
    {
        if (m_gfxDevice.EnableReleaseMemWaitCpDma() &&
            (usePws ||
             ((acquirePoint <= AcquirePointMe) && TestAnyFlagSet(releaseEvents.eventTypeMask, ReleaseTokenMaskEop)) ||
             ((acquirePoint == AcquirePointEop) && (releaseEvents.syncRbCache || (syncGlxFlags != SyncGlxNone)))))
        {
            releaseMemWaitCpDma = true;
        }
        else
        {
            pCmdSpace += CmdUtil::BuildWaitDmaData(pCmdSpace);
        }
        pBarrierOps->pipelineStalls.syncCpDma = 1;
        pCmdBuf->SetCpBltState(false);
    }

    if (cacheOps.timestamp)
    {
        pCmdSpace = WriteTimestampSync(engineType, pCmdBuf->GetReleaseMemTsGpuVa(), pCmdSpace, m_cmdUtil);
    }

    if (usePws)
    {
        ReleaseMemGeneric releaseMem = {};
        releaseMem.cacheSync = CmdUtil::SelectReleaseMemCaches(&syncGlxFlags);
        releaseMem.usePws    = true;
        releaseMem.waitCpDma = releaseMemWaitCpDma;

        // Note that when we computed usePws we forced all EOS releases with GCR ops down the non-PWS path.
        PAL_ASSERT(TestAnyFlagSet(releaseEvents.eventTypeMask, ReleaseTokenMaskEop) ||
                   (releaseMem.cacheSync.u8All == 0));

        ME_ACQUIRE_MEM_pws_counter_sel_enum pwsCounterSel;
        if (TestAnyFlagSet(releaseEvents.eventTypeMask, ReleaseTokenMaskEop))
        {
            releaseMem.dataSel  = data_sel__me_release_mem__none;
            releaseMem.vgtEvent = releaseEvents.syncRbCache ? CACHE_FLUSH_AND_INV_TS_EVENT : BOTTOM_OF_PIPE_TS;
            pwsCounterSel       = pws_counter_sel__me_acquire_mem__ts_select;
            pBarrierOps->pipelineStalls.eopTsBottomOfPipe = 1;
        }
        else
        {
            // Note: PWS+ doesn't need timestamp write, we pass in a dummy write just to meet RELEASE_MEM packet
            //       programming requirement for DATA_SEL field, where 0=none (Discard data) is not a valid option
            //       when EVENT_INDEX=shader_done (PS_DONE/CS_DONE).
            releaseMem.dataSel = data_sel__me_release_mem__send_32_bit_low;
            releaseMem.dstAddr = pCmdBuf->GetReleaseMemTsGpuVa();

            if (TestAnyFlagSet(releaseEvents.eventTypeMask, ReleaseTokenMaskPsDone))
            {
                releaseMem.vgtEvent = PS_DONE;
                pwsCounterSel       = pws_counter_sel__me_acquire_mem__ps_select;
                pBarrierOps->pipelineStalls.eosTsPsDone = 1;
            }
            else
            {
                PAL_ASSERT(TestAnyFlagSet(releaseEvents.eventTypeMask, ReleaseTokenMaskCsDone));
                PAL_ASSERT(releaseEvents.waitVsDone == false); // No VsDone as it should go through non-PWS path.

                releaseMem.vgtEvent = CS_DONE;
                pwsCounterSel       = pws_counter_sel__me_acquire_mem__cs_select;
                pBarrierOps->pipelineStalls.eosTsCsDone = 1;
            }
        }
        pCmdSpace += m_cmdUtil.BuildReleaseMemGeneric(releaseMem, pCmdSpace);

        const bool syncPsCsDone = TestAllFlagsSet(releaseEvents.eventTypeMask, ReleaseTokenMaskPsCsDone);
        if (syncPsCsDone)
        {
            releaseMem.vgtEvent  = CS_DONE;
            releaseMem.waitCpDma = false; // PS_DONE has waited CpDma, no need wait again.
            pCmdSpace += m_cmdUtil.BuildReleaseMemGeneric(releaseMem, pCmdSpace);
            pBarrierOps->pipelineStalls.eosTsCsDone = 1;
        }

        // Wait on the PWS+ event via ACQUIRE_MEM
        AcquireMemGfxPws acquireMem = {};
        acquireMem.stageSel   = GetPwsStageSel(acquirePoint);
        acquireMem.counterSel = pwsCounterSel;
        acquireMem.syncCount  = 0;
        pCmdSpace += CmdUtil::BuildAcquireMemGfxPws(acquireMem, pCmdSpace);

        if (syncPsCsDone)
        {
            acquireMem.counterSel = pws_counter_sel__me_acquire_mem__cs_select;
            pCmdSpace += CmdUtil::BuildAcquireMemGfxPws(acquireMem, pCmdSpace);
        }

        pBarrierOps->pipelineStalls.waitOnTs   = 1;
        pBarrierOps->pipelineStalls.pfpSyncMe |= (acquirePoint == AcquirePointPfp);
    }
    else if (acquirePoint != AcquirePointEop) // Non-PWS path
    {
        // GetAcquirePoint() should have clamped all later acquire points to AcquirePointMe except AcquirePointEop.
        PAL_ASSERT(acquirePoint <= AcquirePointMe);

        if (TestAnyFlagSet(releaseEvents.eventTypeMask, ReleaseTokenMaskEop))
        {
            WriteWaitEopInfo waitEopInfo = {};
            waitEopInfo.hwGlxSync  = syncGlxFlags;
            waitEopInfo.hwRbSync   = releaseEvents.syncRbCache ? SyncRbWbInv : SyncRbNone;
            waitEopInfo.hwAcqPoint = AcquirePointMe;
            waitEopInfo.waitCpDma  = releaseMemWaitCpDma;
            waitEopInfo.disablePws = true;

            pCmdSpace = pCmdBuf->WriteWaitEop(waitEopInfo, pCmdSpace);

            pBarrierOps->pipelineStalls.eopTsBottomOfPipe = 1;
            pBarrierOps->pipelineStalls.waitOnTs          = 1;
        }
        else
        {
            if (TestAnyFlagSet(releaseEvents.eventTypeMask, ReleaseTokenMaskPsDone))
            {
                pCmdSpace += CmdUtil::BuildNonSampleEventWrite(PS_PARTIAL_FLUSH, engineType, pCmdSpace);
                pBarrierOps->pipelineStalls.psPartialFlush = 1;
            }
            else if (releaseEvents.waitVsDone) // On gfx12, PsDone can guarantee VsDone.
            {
                pCmdSpace += CmdUtil::BuildNonSampleEventWrite(VS_PARTIAL_FLUSH, engineType, pCmdSpace);
                pBarrierOps->pipelineStalls.vsPartialFlush = 1;
            }

            if (TestAnyFlagSet(releaseEvents.eventTypeMask, ReleaseTokenMaskCsDone))
            {
                pCmdSpace += CmdUtil::BuildNonSampleEventWrite(CS_PARTIAL_FLUSH, engineType, pCmdSpace);
                pBarrierOps->pipelineStalls.csPartialFlush = 1;
            }

            if (syncGlxFlags != SyncGlxNone)
            {
                // We need a trailing acquire_mem to handle any cache sync requests.
                AcquireMemGeneric acquireMem = {};
                acquireMem.engineType = engineType;
                acquireMem.cacheSync  = syncGlxFlags;
                pCmdSpace += CmdUtil::BuildAcquireMemGeneric(acquireMem, pCmdSpace);
            }
        }

        if (acquirePoint == AcquirePointPfp)
        {
            pCmdSpace += CmdUtil::BuildPfpSyncMe(pCmdSpace);
            pBarrierOps->pipelineStalls.pfpSyncMe = 1;
        }
    }
    else // acquirePoint == AcquirePointEop, non-PWS path
    {
        if (releaseEvents.syncRbCache || (syncGlxFlags != SyncGlxNone))
        {
            // Must be Eop event to sync RB or GCR cache.
            PAL_ASSERT(TestAnyFlagSet(releaseEvents.eventTypeMask, ReleaseTokenMaskEop));

            // Need issue GCR.gl2Inv/gl2Wb and RB cache sync in single ReleaseMem packet to avoid racing issue.
            ReleaseMemGeneric releaseMem = {};
            releaseMem.cacheSync = CmdUtil::SelectReleaseMemCaches(&syncGlxFlags);
            releaseMem.dataSel   = data_sel__me_release_mem__none;
            releaseMem.vgtEvent  = releaseEvents.syncRbCache ? CACHE_FLUSH_AND_INV_TS_EVENT : BOTTOM_OF_PIPE_TS;
            releaseMem.waitCpDma = releaseMemWaitCpDma;

            pCmdSpace += m_cmdUtil.BuildReleaseMemGeneric(releaseMem, pCmdSpace);
            pBarrierOps->pipelineStalls.eopTsBottomOfPipe = 1;
        }
    }

    // If we have stalled at Eop or CsDone, update some CmdBufState (e.g. xxxBltActive) flags.
    if (acquirePoint <= AcquirePointMe)
    {
        if (TestAnyFlagSet(releaseEvents.eventTypeMask, ReleaseTokenMaskEop))
        {
            pCmdBuf->SetPrevCmdBufInactive();
            pCmdBuf->UpdateRetiredAcqRelFenceVal(ReleaseTokenEop, pCmdBuf->GetCurAcqRelFenceVal(ReleaseTokenEop));
        }

        if (TestAnyFlagSet(releaseEvents.eventTypeMask, ReleaseTokenMaskEop | ReleaseTokenMaskPsDone))
        {
            pCmdBuf->UpdateRetiredAcqRelFenceVal(ReleaseTokenPsDone, pCmdBuf->GetCurAcqRelFenceVal(ReleaseTokenPsDone));
        }

        if (TestAnyFlagSet(releaseEvents.eventTypeMask, ReleaseTokenMaskEop | ReleaseTokenMaskCsDone))
        {
            pCmdBuf->SetCsBltState(false);
            pCmdBuf->UpdateRetiredAcqRelFenceVal(ReleaseTokenCsDone, pCmdBuf->GetCurAcqRelFenceVal(ReleaseTokenCsDone));
        }

        if (syncSrcCaches && (pCmdBuf->GetCmdBufState().flags.csBltActive == 0))
        {
            pCmdBuf->SetCsBltWriteCacheState(false);
        }
    }

    if (invSrcCaches && (pCmdBuf->GetCmdBufState().flags.cpBltActive == 0))
    {
        pCmdBuf->SetCpMemoryWriteL2CacheStaleState(false);
    }

    pCmdStream->CommitCommands(pCmdSpace);
}

// =====================================================================================================================
void BarrierMgr::Barrier(
    GfxCmdBuffer*                 pCmdBuf,
    const BarrierInfo&            barrierInfo,
    Developer::BarrierOperations* pBarrierOps
    ) const
{
    // Wait on the GPU memory slot(s) in all specified IGpuEvent objects.
    if (barrierInfo.gpuEventWaitCount > 0)
    {
        const EngineType engineType = pCmdBuf->GetEngineType();
        auto* const      pCmdStream = static_cast<CmdStream*>(pCmdBuf->GetMainCmdStream());
        uint32*          pCmdSpace  = pCmdStream->ReserveCommands();

        for (uint32 i = 0; i < barrierInfo.gpuEventWaitCount; i++)
        {
            const GpuEvent* pGpuEvent       = static_cast<const GpuEvent*>(barrierInfo.ppGpuEvents[i]);
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

    // A container to cache the calculated BLT transitions and some cache info for reuse.
    ImgLayoutTransitionList bltList(barrierInfo.transitionCount, m_pPlatform);

    Result result = (bltList.Capacity() >= barrierInfo.transitionCount) ? Result::Success : Result::ErrorOutOfMemory;

    if (result == Result::Success)
    {
        uint32 srcStageMask;
        uint32 dstStageMask = GetPipelineStageMaskFromBarrierInfo(barrierInfo, &srcStageMask);

        // Optimize global stage masks.
        OptimizeStageMask(pCmdBuf, BarrierType::Global, &srcStageMask, &dstStageMask);

        CacheSyncOps cacheOps = GetCacheSyncOps(pCmdBuf, BarrierType::Global, nullptr,
                                                barrierInfo.globalSrcCacheMask, barrierInfo.globalDstCacheMask);
        uint32 bltCount = 0;
        for (uint32 i = 0; i < barrierInfo.transitionCount; i++)
        {
            const BarrierTransition& transition = barrierInfo.pTransitions[i];
            const auto&              imgInfo    = transition.imageInfo;
            LayoutTransition         tranType   = LayoutTransition::None;

            if (imgInfo.pImage != nullptr)
            {
                tranType = GetLayoutTransitionType(imgInfo.pImage, imgInfo.subresRange,
                                                   imgInfo.oldLayout, imgInfo.newLayout);

                if (tranType != LayoutTransition::None)
                {
                    // Zero below three unused members.
                    bltList[bltCount].imgBarrier.srcStageMask       = 0;
                    bltList[bltCount].imgBarrier.dstStageMask       = 0;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 880
                    bltList[bltCount].imgBarrier.box                = {};
#endif

                    bltList[bltCount].imgBarrier.srcAccessMask      = transition.srcCacheMask;
                    bltList[bltCount].imgBarrier.dstAccessMask      = transition.dstCacheMask;
                    bltList[bltCount].imgBarrier.pImage             = transition.imageInfo.pImage;
                    bltList[bltCount].imgBarrier.subresRange        = transition.imageInfo.subresRange;
                    bltList[bltCount].imgBarrier.oldLayout          = transition.imageInfo.oldLayout;
                    bltList[bltCount].imgBarrier.newLayout          = transition.imageInfo.newLayout;
                    bltList[bltCount].imgBarrier.pQuadSamplePattern = transition.imageInfo.pQuadSamplePattern;

                    bltList[bltCount].type = tranType;
                    bltCount++;
                }
            }

            const uint32 dstCacheMask = (tranType != LayoutTransition::None) ? BltAccessMask : transition.dstCacheMask;

            cacheOps |= GetCacheSyncOps(pCmdBuf,
                                        ((imgInfo.pImage != nullptr) ? BarrierType::Image : BarrierType::Buffer),
                                        imgInfo.pImage,
                                        transition.srcCacheMask,
                                        dstCacheMask);
        }

        if (bltCount > 0)
        {
            // Pre-BLT barrier
            IssueReleaseThenAcquireSync(pCmdBuf, srcStageMask, BltStageMask, cacheOps, pBarrierOps);

            // Override srcStageMask with PostBltStageMask to release.
            cacheOps = IssueLayoutTransitionBlt(pCmdBuf, bltList, bltCount, &srcStageMask, pBarrierOps);
        }

        IssueReleaseThenAcquireSync(pCmdBuf, srcStageMask, dstStageMask, cacheOps, pBarrierOps);
    }
    else
    {
        PAL_ASSERT(result == Result::ErrorOutOfMemory);
        pCmdBuf->NotifyAllocFailure();
    }
}

// =====================================================================================================================
ReleaseToken BarrierMgr::ReleaseInternal(
    GfxCmdBuffer*                 pCmdBuf,
    const AcquireReleaseInfo&     releaseInfo,
    const GpuEvent*               pClientEvent,
    Developer::BarrierOperations* pBarrierOps
    ) const
{
    // A container to cache the calculated BLT transitions and some cache info for reuse.
    ImgLayoutTransitionList bltList(releaseInfo.imageBarrierCount, m_pPlatform);

    Result result = (bltList.Capacity() >= releaseInfo.imageBarrierCount) ? Result::Success : Result::ErrorOutOfMemory;
    ReleaseToken syncToken = {};

    if (result == Result::Success)
    {
        uint32 srcGlobalStageMask    = releaseInfo.srcGlobalStageMask;
        bool   releaseBufferCopyOnly = (releaseInfo.srcGlobalStageMask == 0)  &&
                                       (releaseInfo.srcGlobalAccessMask == 0) &&
                                       (releaseInfo.imageBarrierCount == 0);

        // Optimize global stage masks.
        OptimizeStageMask(pCmdBuf, BarrierType::Global, &srcGlobalStageMask, nullptr);

        CacheSyncOps cacheOps = {};
        if (releaseInfo.srcGlobalAccessMask != 0)
        {
            cacheOps = GetCacheSyncOps(pCmdBuf, BarrierType::Global, nullptr, releaseInfo.srcGlobalAccessMask, 0);
        }

        // Always do full-range flush sync.
        uint32 srcStageMask = 0;
        for (uint32 i = 0; i < releaseInfo.memoryBarrierCount; i++)
        {
            const MemBarrier& barrier           = releaseInfo.pMemoryBarriers[i];
            const bool        releaseBufferCopy = (barrier.srcStageMask == PipelineStageBlt) &&
                                                  (barrier.srcAccessMask != 0)               &&
                                                  TestAllFlagsSet(CoherCopy, barrier.srcAccessMask);

            cacheOps              |= GetCacheSyncOps(pCmdBuf, BarrierType::Buffer, nullptr, barrier.srcAccessMask, 0);
            srcStageMask          |= barrier.srcStageMask;
            releaseBufferCopyOnly &= releaseBufferCopy;
        }

        // Optimize buffer stage masks before OR together.
        OptimizeStageMask(pCmdBuf, BarrierType::Buffer, &srcStageMask, nullptr);
        srcGlobalStageMask |= srcStageMask;

        uint32 bltCount = 0;
        srcStageMask = 0;
        for (uint32 i = 0; i < releaseInfo.imageBarrierCount; i++)
        {
            const ImgBarrier& barrier = releaseInfo.pImageBarriers[i];

            const LayoutTransition tranType = GetLayoutTransitionType(barrier.pImage, barrier.subresRange,
                                                                      barrier.oldLayout, barrier.newLayout);
            if (tranType != LayoutTransition::None)
            {
                bltList[bltCount].imgBarrier = barrier;
                bltList[bltCount].type       = tranType;
                bltCount++;
            }

            // Minor optimization: set transition datAccessMask to 0 for InitMaskRam to avoid unneeded cache sync.
            const uint32 dstAccessMask = (tranType == LayoutTransition::None)        ? barrier.dstAccessMask :
                                         (tranType != LayoutTransition::InitMaskRam) ? BltAccessMask         :
                                                                                       0;
            cacheOps     |= GetCacheSyncOps(pCmdBuf, BarrierType::Image, barrier.pImage,
                                            barrier.srcAccessMask, dstAccessMask);
            srcStageMask |= barrier.srcStageMask;
        }

        // Optimize image stage masks before OR together.
        OptimizeStageMask(pCmdBuf, BarrierType::Image, &srcStageMask, nullptr);
        srcGlobalStageMask |= srcStageMask;

        if (bltCount > 0)
        {
            // Issue all-in-one ReleaseThenAcquire prior to the potential BLTs.
            IssueReleaseThenAcquireSync(pCmdBuf, srcGlobalStageMask, BltStageMask, cacheOps, pBarrierOps);

            // Override srcGlobalStageMask with PostBltStageMask to release.
            cacheOps = IssueLayoutTransitionBlt(pCmdBuf, bltList, bltCount, &srcGlobalStageMask, pBarrierOps);
        }

        syncToken = IssueReleaseSync(pCmdBuf, srcGlobalStageMask, releaseBufferCopyOnly, cacheOps, pClientEvent,
                                     pBarrierOps);
    }
    else
    {
        PAL_ASSERT(result == Result::ErrorOutOfMemory);
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
    // A container to cache the calculated BLT transitions and some cache info for reuse.
    ImgLayoutTransitionList bltList(acquireInfo.imageBarrierCount, m_pPlatform);

    Result result = (bltList.Capacity() >= acquireInfo.imageBarrierCount) ? Result::Success : Result::ErrorOutOfMemory;

    if (result == Result::Success)
    {
        uint32 dstGlobalStageMask = acquireInfo.dstGlobalStageMask;

        // Optimize global stage masks.
        OptimizeStageMask(pCmdBuf, BarrierType::Global, nullptr, &dstGlobalStageMask);

        CacheSyncOps cacheOps = {};
        if (acquireInfo.dstGlobalAccessMask != 0)
        {
            cacheOps = GetCacheSyncOps(pCmdBuf, BarrierType::Global, nullptr, 0, acquireInfo.dstGlobalAccessMask);
        }

        // Always do full-range flush sync.
        uint32 dstStageMask = 0;
        for (uint32 i = 0; i < acquireInfo.memoryBarrierCount; i++)
        {
            const MemBarrier& barrier = acquireInfo.pMemoryBarriers[i];

            cacheOps     |= GetCacheSyncOps(pCmdBuf, BarrierType::Buffer, nullptr, 0, barrier.dstAccessMask);
            dstStageMask |= barrier.dstStageMask;
        }

        // Optimize buffer stage masks before OR together.
        OptimizeStageMask(pCmdBuf, BarrierType::Buffer, nullptr, &dstStageMask);
        dstGlobalStageMask |= dstStageMask;

        uint32 bltCount = 0;
        dstStageMask = 0;
        for (uint32 i = 0; i < acquireInfo.imageBarrierCount; i++)
        {
            const ImgBarrier& barrier = acquireInfo.pImageBarriers[i];

            const LayoutTransition tranType = GetLayoutTransitionType(barrier.pImage, barrier.subresRange,
                                                                      barrier.oldLayout, barrier.newLayout);
            if (tranType != LayoutTransition::None)
            {
                bltList[bltCount].imgBarrier = barrier;
                bltList[bltCount].type       = tranType;
                bltCount++;
            }

            // Minor optimization: no need pre-blt cache sync for transition with InitMaskRam case.
            if (tranType != LayoutTransition::InitMaskRam)
            {
                const uint32 dstAccessMask = (tranType == LayoutTransition::None) ? barrier.dstAccessMask :
                                                                                    BltAccessMask;
                cacheOps |= GetCacheSyncOps(pCmdBuf, BarrierType::Image, barrier.pImage, 0, dstAccessMask);
            }
            dstStageMask |= barrier.dstStageMask;
        }

        // Optimize image stage masks before OR together.
        OptimizeStageMask(pCmdBuf, BarrierType::Image, nullptr, &dstStageMask);
        dstGlobalStageMask |= dstStageMask;

        // Issue acquire for global or pre-BLT sync.
        IssueAcquireSync(pCmdBuf,
                         ((bltCount > 0) ? BltStageMask : dstGlobalStageMask),
                         cacheOps,
                         syncTokenCount,
                         pSyncTokens,
                         pBarrierOps);

        if (bltCount > 0)
        {
            uint32 postBltStageMask;
            cacheOps = IssueLayoutTransitionBlt(pCmdBuf, bltList, bltCount, &postBltStageMask, pBarrierOps);

            // Issue all-in-one ReleaseThenAcquire for post BLTs barrier.
            IssueReleaseThenAcquireSync(pCmdBuf, postBltStageMask, dstGlobalStageMask, cacheOps, pBarrierOps);
        }
    }
    else
    {
        PAL_ASSERT(result == Result::ErrorOutOfMemory);
        pCmdBuf->NotifyAllocFailure();
    }
}

// =====================================================================================================================
ReleaseToken BarrierMgr::Release(
    GfxCmdBuffer*                 pGfxCmdBuf,
    const AcquireReleaseInfo&     releaseInfo,
    Developer::BarrierOperations* pBarrierOps
    ) const
{
    return ReleaseInternal(pGfxCmdBuf, releaseInfo, nullptr, pBarrierOps);
}

// =====================================================================================================================
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
        auto* const      pCmdStream = static_cast<CmdStream*>(pCmdBuf->GetMainCmdStream());
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
// Optimize barrier transition by modify its srcStageMask/dstStageMask to reduce stall operations.
// e.g. (PS|CS, ShaderRead) -> (CS, ShaderRead) -> (ColorTarget, CoherColorTarget), can optimize to only release
// srcStageMask = PS as CS will be released in the second transition.
void BarrierMgr::OptimizeReadOnlyBarrier(
    GfxCmdBuffer* pCmdBuf,
    BarrierType   barrierType,
    const IImage* pImage,
    uint32*       pSrcStageMask,
    uint32*       pDstStageMask,
    uint32*       pSrcAccessMask,
    uint32*       pDstAccessMask
    ) const
{
    bool canSkip = (GetCacheSyncOps(pCmdBuf, barrierType, pImage, *pSrcAccessMask, *pDstAccessMask) == NullCacheSyncOps);

    // Can only skip if previous barrier acquires same or earlier stage than current barrier acquires.
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

        // Remove TopOfPipe, FetchIndirectArgs and PipelineStagePostPrefetch as they don't cause stall.
        // PipelineStageFetchIndices needs stall VS.
        optSrcStageMask &= ~(PipelineStageTopOfPipe | PipelineStageFetchIndirectArgs | PipelineStagePostPrefetch);

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
void BarrierMgr::ReleaseThenAcquire(
    GfxCmdBuffer*                 pCmdBuf,
    const AcquireReleaseInfo&     barrierInfo,
    Developer::BarrierOperations* pBarrierOps
    ) const
{
    // A container to cache the calculated BLT transitions and some cache info for reuse.
    ImgLayoutTransitionList bltList(barrierInfo.imageBarrierCount, m_pPlatform);

    Result result = (bltList.Capacity() >= barrierInfo.imageBarrierCount) ? Result::Success : Result::ErrorOutOfMemory;

    if (result == Result::Success)
    {
        uint32 srcGlobalStageMask = barrierInfo.srcGlobalStageMask;
        uint32 dstGlobalStageMask = barrierInfo.dstGlobalStageMask;

        // Optimize global stage masks.
        OptimizeStageMask(pCmdBuf, BarrierType::Global, &srcGlobalStageMask, &dstGlobalStageMask);

        CacheSyncOps cacheOps = {};
        if ((barrierInfo.srcGlobalAccessMask | barrierInfo.dstGlobalAccessMask) != 0)
        {
            cacheOps = GetCacheSyncOps(pCmdBuf, BarrierType::Global, nullptr,
                                       barrierInfo.srcGlobalAccessMask, barrierInfo.dstGlobalAccessMask);
        }

        // Always do full-range flush sync.
        uint32 srcStageMask = 0;
        uint32 dstStageMask = 0;
        for (uint32 i = 0; i < barrierInfo.memoryBarrierCount; i++)
        {
            MemBarrier barrier = barrierInfo.pMemoryBarriers[i];

            if (IsReadOnlyTransition(barrier.srcAccessMask, barrier.dstAccessMask))
            {
                OptimizeReadOnlyBarrier(pCmdBuf, BarrierType::Buffer, nullptr, &barrier.srcStageMask,
                                        &barrier.dstStageMask, &barrier.srcAccessMask, &barrier.dstAccessMask);
            }

            cacheOps     |= GetCacheSyncOps(pCmdBuf, BarrierType::Buffer, nullptr, barrier.srcAccessMask,
                                            barrier.dstAccessMask);
            srcStageMask |= barrier.srcStageMask;
            dstStageMask |= barrier.dstStageMask;
        }

        // Optimize buffer stage masks before OR together.
        OptimizeStageMask(pCmdBuf, BarrierType::Buffer, &srcStageMask, &dstStageMask);
        srcGlobalStageMask |= srcStageMask;
        dstGlobalStageMask |= dstStageMask;

        uint32 bltCount = 0;
        srcStageMask = 0;
        dstStageMask = 0;
        for (uint32 i = 0; i < barrierInfo.imageBarrierCount; i++)
        {
            ImgBarrier barrier = barrierInfo.pImageBarriers[i];

            const LayoutTransition tranType = GetLayoutTransitionType(barrier.pImage, barrier.subresRange,
                                                                      barrier.oldLayout, barrier.newLayout);
            if (tranType != LayoutTransition::None)
            {
                bltList[bltCount].imgBarrier = barrier; // Record non-optimized barrier info.
                bltList[bltCount].type       = tranType;
                bltCount++;
            }
            else // Try to optimize image barrier if possible.
            {
                // Can safely skip transition between depth read and depth write.
                if ((barrier.srcAccessMask == CoherDepthStencilTarget) &&
                    (barrier.dstAccessMask == CoherDepthStencilTarget))
                {
                    barrier.srcStageMask  = 0;
                    barrier.dstStageMask  = 0;
                    barrier.srcAccessMask = 0;
                    barrier.dstAccessMask = 0;
                }
                else if (IsReadOnlyTransition(barrier.srcAccessMask, barrier.dstAccessMask))
                {
                    OptimizeReadOnlyBarrier(pCmdBuf, BarrierType::Image, barrier.pImage,
                                            &barrier.srcStageMask, &barrier.dstStageMask,
                                            &barrier.srcAccessMask, &barrier.dstAccessMask);
                }
            }

            // Minor optimization: set transition dstAccessMask to 0 for InitMaskRam to avoid unneeded cache sync.
            const uint32 dstAccessMask = (tranType == LayoutTransition::None)        ? barrier.dstAccessMask :
                                         (tranType != LayoutTransition::InitMaskRam) ? BltAccessMask         :
                                                                                       0;
            cacheOps |= GetCacheSyncOps(pCmdBuf, BarrierType::Image, barrier.pImage, barrier.srcAccessMask,
                                        dstAccessMask);

            srcStageMask |= barrier.srcStageMask;
            dstStageMask |= barrier.dstStageMask;
        }

        // Optimize image stage masks before OR together.
        OptimizeStageMask(pCmdBuf, BarrierType::Image, &srcStageMask, &dstStageMask);
        srcGlobalStageMask |= srcStageMask;
        dstGlobalStageMask |= dstStageMask;

        if (bltCount > 0)
        {
            // Pre-BLT barrier
            IssueReleaseThenAcquireSync(pCmdBuf, srcGlobalStageMask, BltStageMask, cacheOps, pBarrierOps);

            // Override srcGlobalStageMask with PostBltStageMask to release.
            cacheOps = IssueLayoutTransitionBlt(pCmdBuf, bltList, bltCount, &srcGlobalStageMask, pBarrierOps);
        }

        IssueReleaseThenAcquireSync(pCmdBuf, srcGlobalStageMask, dstGlobalStageMask, cacheOps, pBarrierOps);
    }
    else
    {
        PAL_ASSERT(result == Result::ErrorOutOfMemory);
        pCmdBuf->NotifyAllocFailure();
    }
}

}
}
