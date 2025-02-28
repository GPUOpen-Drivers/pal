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

#pragma once

#include "core/hw/gfxip/gfxBarrier.h"
#include "core/hw/gfxip/gfxCmdBuffer.h"
#include "core/hw/gfxip/gfx9/gfx9Chip.h"
#include "palAutoBuffer.h"

namespace Pal
{
namespace Gfx9
{

class CmdStream;
class CmdUtil;
class Device;
class Image;
class RsrcProcMgr;

struct SyncReqs
{
    // These flags describe which caches must be flushed and/or invalidated.
    SyncGlxFlags glxCaches;
    SyncRbFlags  rbCaches;

    // These flags describe which syncronization operations are required.
    struct
    {
        uint8 waitOnEopTs    : 1;
        uint8 cbTargetStall  : 1; // Gfx-only: Do a range-checked stall on the active color targets.
        uint8 dbTargetStall  : 1; // Gfx-only: Do a range-checked stall on the active depth and stencil targets.
        uint8 vsPartialFlush : 1; // Gfx-only
        uint8 psPartialFlush : 1; // Gfx-only
        uint8 csPartialFlush : 1;
        uint8 pfpSyncMe      : 1; // Gfx-only
        uint8 syncCpDma      : 1;
    };
};

// HW layout transition types.
enum HwLayoutTransition : uint8
{
    None                         = 0x0,

    // Depth/Stencil
    ExpandDepthStencil           = 0x1,
    ExpandHtileHiZRange          = 0x2,
    ResummarizeDepthStencil      = 0x3,

    // Color
    FastClearEliminate           = 0x4,
    FmaskDecompress              = 0x5,
    DccDecompress                = 0x6,
    MsaaColorDecompress          = 0x7,

    // Initialize Color metadata or Depth/stencil Htile.
    InitMaskRam                  = 0x8,
};

// Information for layout transition BLT
struct LayoutTransitionInfo
{
    union
    {
        struct
        {
            uint32 useComputePath   : 1;  // For those transition BLTs that could do either graphics or compute path,
                                          // figure out what path the BLT will use and cache it here.
            uint32 reserved         : 7;  // Reserved for future usage.
        };
        uint8 u8All;                      // Flags packed as uint32.
    } flags;

    HwLayoutTransition blt[2];            // Color target may need a second decompress pass to do MSAA color decompress.
};

// Required cache sync operations for the transition
struct CacheSyncOps
{
    SyncGlxFlags glxFlags; // Required GLx flags to sync
    bool         rbCache;  // If need sync RB cache.
};

constexpr bool operator==(CacheSyncOps lhs, CacheSyncOps rhs)
{
    return (lhs.glxFlags == rhs.glxFlags) && (lhs.rbCache == rhs.rbCache);
}

constexpr CacheSyncOps& operator|=(CacheSyncOps& lhs, CacheSyncOps rhs)
{
    lhs.glxFlags |= rhs.glxFlags;
    lhs.rbCache  |= rhs.rbCache;
    return lhs;
}

// This family of constexpr bitmasks defines which source/prior stages require EOP or EOS events to wait for idle.
// They're mainly used to pick our Release barrier event but are also reused in other places in PAL.
constexpr uint32 EopWaitStageMask = (PipelineStageSampleRate  | PipelineStageDsTarget    |
                                     PipelineStageColorTarget | PipelineStageBottomOfPipe);

// PFP sets IB base and size to register VGT_DMA_BASE & VGT_DMA_SIZE and send request to VGT for indices fetch,
// which is done in GE. So need VsDone to make sure indices fetch done.
constexpr uint32 VsWaitStageMask  = (PipelineStageFetchIndices | PipelineStageStreamOut |
                                     PipelineStageVs | PipelineStageHs | PipelineStageDs | PipelineStageGs);
constexpr uint32 PsWaitStageMask  = PipelineStagePs;
constexpr uint32 CsWaitStageMask  = PipelineStageCs;

// =====================================================================================================================
// HWL Barrier Processing Manager: contain layout transition BLT and pre/post-BLT execution and memory dependencies.
class BarrierMgr final : public Pal::GfxBarrierMgr
{
public:
    BarrierMgr(GfxDevice* pGfxDevice, const CmdUtil& cmdUtil);

    virtual ~BarrierMgr() {}

    virtual void Barrier(
        GfxCmdBuffer*                 pGfxCmdBuf,
        const BarrierInfo&            barrierInfo,
        Developer::BarrierOperations* pBarrierOps) const override;

    virtual ReleaseToken Release(
        GfxCmdBuffer*                 pGfxCmdBuf,
        const AcquireReleaseInfo&     releaseInfo,
        Developer::BarrierOperations* pBarrierOps) const override;

    virtual void Acquire(
        GfxCmdBuffer*                 pGfxCmdBuf,
        const AcquireReleaseInfo&     acquireInfo,
        uint32                        syncTokenCount,
        const ReleaseToken*           pSyncTokens,
        Developer::BarrierOperations* pBarrierOps) const override;

    virtual void ReleaseEvent(
        GfxCmdBuffer*                 pGfxCmdBuf,
        const AcquireReleaseInfo&     releaseInfo,
        const IGpuEvent*              pClientEvent,
        Developer::BarrierOperations* pBarrierOps) const override;

    virtual void AcquireEvent(
        GfxCmdBuffer*                 pGfxCmdBuf,
        const AcquireReleaseInfo&     acquireInfo,
        uint32                        gpuEventCount,
        const IGpuEvent* const*       ppGpuEvents,
        Developer::BarrierOperations* pBarrierOps) const override;

    virtual void ReleaseThenAcquire(
        GfxCmdBuffer*                 pGfxCmdBuf,
        const AcquireReleaseInfo&     barrierInfo,
        Developer::BarrierOperations* pBarrierOps) const override;

    virtual void OptimizeStageMask(
        const GfxCmdBuffer* pCmdBuf,
        BarrierType         barrierType,
        uint32*             pSrcStageMask,
        uint32*             pDstStageMask,
        bool                isClearToTarget = false) const override;

    virtual bool OptimizeAccessMask(
        const GfxCmdBuffer* pCmdBuf,
        BarrierType         barrierType,
        const Pal::Image*   pImage,
        uint32*             pSrcAccessMask,
        uint32*             pDstAccessMask,
        bool                shaderMdAccessIndirectOnly) const override;

    void IssueSyncs(
        GfxCmdBuffer*                 pCmdBuf,
        CmdStream*                    pCmdStream,
        SyncReqs                      syncReqs,
        HwPipePoint                   waitPoint,
        gpusize                       rangeBase,
        gpusize                       rangeSize,
        Developer::BarrierOperations* pOperations) const;

private:
    // A structure that helps cache below info for an image barrier in acquire-release based barrier.
    //   1). Calculated layout transition BLT and sync requests
    //   2). Image barrier that needs UpdateDccStateMetaData.
    // Note that these two kinds of information are not related to each other! Effectively each AcqRelAutoBuffer stores
    // two unrelated arrays which have been interleaved in memory (to avoid allocating two separate AutoBuffers). Take
    // care to only inspect the BLT state when looping over bltCount and to only inspect the DCC metadata state when
    // looping over updateDccStateCount.
    struct AcqRelImgTransitionInfo
    {
        // Image barrier that needs layout transition BLT.
        ImgBarrier           bltBarrier;
        LayoutTransitionInfo layoutTransInfo;
        uint32               stageMask;     // Pipeline stage mask of layoutTransInfo.blt[0]
        uint32               accessMask;    // Coherency access mask of layoutTransInfo.blt[0]

        // Image barrier that needs UpdateDccStateMetaData
        const ImgBarrier*    pUpdateDccStateBarrier;
    };

    using AcqRelAutoBuffer = Util::AutoBuffer<AcqRelImgTransitionInfo, 8, Platform>;

    // Acquire release transition info gathered from all image transitions.
    struct AcqRelTransitionInfo
    {
        AcqRelAutoBuffer* pList;         // List of AcqRelImgTransitionInfo that need layout transition BLT and
                                         // UpdateDccStateMetaData.
        uint32            bltCount;      // Number of entries that needs layout transition blt in pBltList.
        uint32            bltStageMask;  // Pipeline stage mask for all layout transition BLTs in pBltList.

        uint32            updateDccStateCount; // Number of entries that needs UpdateDccStateMetaData in pBltList.
    };

    // Layout transition blt states in image barrier.
    enum LayoutTransBltState : uint8
    {
        WithLayoutTransBlt,
        WithoutLayoutTransBlt,
        LayoutTransBltStateCount
    };

    // Acquire release sync info gathered from all image transitions.
    struct AcqRelImageSyncInfo
    {
        // For below arrays:
        //  - index WithoutLayoutTransBlt indicates OR'ed sync info from image barriers without layout trans blt.
        //  - index WithLayoutTransBlt indicates OR'ed sync info from image barriers with layout trans blt.
        CacheSyncOps cacheOps[LayoutTransBltStateCount];     // Required cache sync operations.
        uint32       srcStageMask[LayoutTransBltStateCount]; // OR'ed srcStageMask from image barriers.

        uint32       dstStageMask;
    };

    static void PostSyncUpdateDccStateMetaData(
        GfxCmdBuffer*                 pCmdBuf,
        CmdStream*                    pCmdStream,
        const AcqRelTransitionInfo&   transitonInfo,
        Developer::BarrierOperations* pBarrierOps);

    static void OptimizeSrcCacheMask(
        const GfxCmdBuffer* pCmdBuf,
        uint32*             pCacheMask);

    const RsrcProcMgr& RsrcProcMgr() const;

    bool NeedGlobalFlushAndInvL2(
        uint32                        srcCacheMask,
        uint32                        dstCacheMask,
        const IImage*                 pImage) const;
    void FlushAndInvL2IfNeeded(
        GfxCmdBuffer*                 pCmdBuf,
        CmdStream*                    pCmdStream,
        const BarrierInfo&            barrier,
        uint32                        transitionId,
        Developer::BarrierOperations* pOperations) const;
    void ExpandColor(
        GfxCmdBuffer*                 pCmdBuf,
        CmdStream*                    pCmdStream,
        const BarrierInfo&            barrier,
        uint32                        transitionId,
        bool                          earlyPhase,
        SyncReqs*                     pSyncReqs,
        Developer::BarrierOperations* pOperations) const;
    void TransitionDepthStencil(
        GfxCmdBuffer*                 pCmdBuf,
        CmdStream*                    pCmdStream,
        GfxCmdBufferStateFlags        cmdBufStateFlags,
        const BarrierInfo&            barrier,
        uint32                        transitionId,
        bool                          earlyPhase,
        SyncReqs*                     pSyncReqs,
        Developer::BarrierOperations* pOperations) const;
    void AcqRelColorTransition(
        GfxCmdBuffer*                 pCmdBuf,
        CmdStream*                    pCmdStream,
        const ImgBarrier&             imgBarrier,
        LayoutTransitionInfo          layoutTransInfo,
        Developer::BarrierOperations* pBarrierOps) const;

    void AcqRelDepthStencilTransition(
        GfxCmdBuffer*                 pCmdBuf,
        const ImgBarrier&             imgBarrier,
        LayoutTransitionInfo          layoutTransInfo) const;

    LayoutTransitionInfo PrepareColorBlt(
        GfxCmdBuffer*       pCmdBuf,
        const Pal::Image&   image,
        const SubresRange&  subresRange,
        ImageLayout         oldLayout,
        ImageLayout         newLayout) const;
    LayoutTransitionInfo PrepareDepthStencilBlt(
        const GfxCmdBuffer* pCmdBuf,
        const Pal::Image&   image,
        const SubresRange&  subresRange,
        ImageLayout         oldLayout,
        ImageLayout         newLayout) const;
    LayoutTransitionInfo PrepareBltInfo(
        GfxCmdBuffer*       pCmdBuf,
        const ImgBarrier&   imageBarrier) const;

    CacheSyncOps GetCacheSyncOps(
        GfxCmdBuffer*       pCmdBuf,
        BarrierType         barrierType,
        const ImgBarrier*   pImgBarrier,
        uint32              srcAccessMask,
        uint32              dstAccessMask,
        bool                shaderMdAccessIndirectOnly) const;

    CacheSyncOps GetGlobalCacheSyncOps(
        GfxCmdBuffer*       pCmdBuf,
        uint32              srcAccessMask,
        uint32              dstAccessMask) const
    {
        return GetCacheSyncOps(pCmdBuf, BarrierType::Global, nullptr, srcAccessMask, dstAccessMask, true);
    }

    CacheSyncOps GetBufferCacheSyncOps(
        GfxCmdBuffer*       pCmdBuf,
        uint32              srcAccessMask,
        uint32              dstAccessMask) const
    {
        return GetCacheSyncOps(pCmdBuf, BarrierType::Buffer, nullptr, srcAccessMask, dstAccessMask, true);
    }

    CacheSyncOps GetImageCacheSyncOps(
        GfxCmdBuffer*       pCmdBuf,
        const ImgBarrier*   pImgBarrier,
        uint32              srcAccessMask,
        uint32              dstAccessMask,
        bool                shaderMdAccessIndirectOnly) const
    {
        return GetCacheSyncOps(pCmdBuf, BarrierType::Image, pImgBarrier, srcAccessMask, dstAccessMask,
                               shaderMdAccessIndirectOnly);
    }

    bool IssueBlt(
        GfxCmdBuffer*                 pCmdBuf,
        CmdStream*                    pCmdStream,
        const ImgBarrier*             pImgBarrier,
        LayoutTransitionInfo          transition,
        bool*                         pPreInitHtileSynced,
        Developer::BarrierOperations* pBarrierOps) const;

    SyncRbFlags OptimizeImageBarrier(
        GfxCmdBuffer*               pCmdBuf,
        ImgBarrier*                 pImgBarrier,
        const LayoutTransitionInfo& layoutTransInfo,
        uint32                      bltStageMask,
        uint32                      bltAccessMask) const;

    AcqRelImageSyncInfo GetAcqRelLayoutTransitionBltInfo(
        GfxCmdBuffer*                 pCmdBuf,
        CmdStream*                    pCmdStream,
        const AcquireReleaseInfo&     barrierInfo,
        AcqRelTransitionInfo*         pTransitionInfo,
        Developer::BarrierOperations* pBarrierOps) const;

    CacheSyncOps IssueAcqRelLayoutTransitionBlt(
        GfxCmdBuffer*                 pCmdBuf,
        CmdStream*                    pCmdStream,
        AcqRelTransitionInfo*         pTransitonInfo,
        Developer::BarrierOperations* pBarrierOps) const;

    bool AcqRelInitMaskRam(
        GfxCmdBuffer*      pCmdBuf,
        CmdStream*         pCmdStream,
        const ImgBarrier&  imgBarrier) const;

    ReleaseToken ReleaseInternal(
        GfxCmdBuffer*                 pCmdBuf,
        const AcquireReleaseInfo&     releaseInfo,
        const GpuEvent*               pClientEvent,
        Developer::BarrierOperations* pBarrierOps) const;

    void AcquireInternal(
        GfxCmdBuffer*                 pCmdBuf,
        const AcquireReleaseInfo&     acquireInfo,
        uint32                        syncTokenCount,
        const ReleaseToken*           pSyncTokens,
        Developer::BarrierOperations* pBarrierOps) const;

    ReleaseToken IssueReleaseSync(
        GfxCmdBuffer*                 pCmdBuf,
        uint32                        srcStageMask,
        bool                          releaseBufferCopyOnly,
        CacheSyncOps                  cacheOps,
        const GpuEvent*               pClientEvent,
        Developer::BarrierOperations* pBarrierOps) const;

    void IssueAcquireSync(
        GfxCmdBuffer*                 pCmdBuf,
        uint32                        dstStageMask,
        CacheSyncOps                  cacheOps,
        uint32                        syncTokenCount,
        const ReleaseToken*           pSyncTokens,
        Developer::BarrierOperations* pBarrierOps) const;

    void IssueReleaseThenAcquireSync(
        GfxCmdBuffer*                 pCmdBuf,
        CmdStream*                    pCmdStream,
        uint32                        srcStageMask,
        uint32                        dstStageMask,
        CacheSyncOps                  cacheOps,
        Developer::BarrierOperations* pBarrierOps) const;

    void OptimizeReadOnlyBarrier(
        GfxCmdBuffer*     pCmdBuf,
        const ImgBarrier* pImgBarrier,
        uint32*           pSrcStageMask,
        uint32*           pDstStageMask,
        uint32*           pSrcAccessMask,
        uint32*           pDstAccessMask) const;

    AcquirePoint GetAcquirePoint(uint32 dstStageMask, EngineType engineType) const;

    void FillCacheOperations(const SyncReqs& syncReqs, Developer::BarrierOperations* pOperations) const;

    bool EnableReleaseMemWaitCpDma() const;

    const CmdUtil&   m_cmdUtil;
    const GfxIpLevel m_gfxIpLevel;

    PAL_DISALLOW_DEFAULT_CTOR(BarrierMgr);
    PAL_DISALLOW_COPY_AND_ASSIGN(BarrierMgr);
};

}
}
