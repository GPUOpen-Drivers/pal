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

#include "core/hw/gfxip/gfxCmdBuffer.h"
#include "core/hw/gfxip/gfx12/gfx12Chip.h"
#include "palAutoBuffer.h"

namespace Pal
{
namespace Gfx12
{

class CmdStream;
class CmdUtil;
class Device;

// This family of constexpr bitmasks defines which source/prior stages require EOP or EOS events to wait for idle.
// They're mainly used to pick our Release barrier event but are also reused in other places in PAL.
constexpr uint32 EopWaitStageMask = (PipelineStageSampleRate  | PipelineStageDsTarget |
                                     PipelineStageColorTarget | PipelineStageBottomOfPipe);

// PFP sets IB base and size to register VGT_DMA_BASE & VGT_DMA_SIZE and send request to VGT for indices fetch,
// which is done in GE. So need VsDone to make sure indices fetch done.
constexpr uint32 VsWaitStageMask  = (PipelineStageFetchIndices | PipelineStageStreamOut |
                                     PipelineStageVs | PipelineStageHs | PipelineStageDs | PipelineStageGs);
constexpr uint32 PsWaitStageMask  = PipelineStagePs;
constexpr uint32 CsWaitStageMask  = PipelineStageCs;

constexpr uint32 VsPsCsWaitStageMask = VsWaitStageMask | PsWaitStageMask | CsWaitStageMask;

// Required cache sync operations for the transition
struct CacheSyncOps
{
    SyncGlxFlags glxFlags;  // Required GLx flags to sync
    bool         rbCache;   // If need sync RB cache.
    bool         timestamp; // Ensure timestamp writes have completed
};

constexpr bool operator==(CacheSyncOps lhs, CacheSyncOps rhs)
{
    return (lhs.glxFlags == rhs.glxFlags) && (lhs.rbCache == rhs.rbCache) && (lhs.timestamp == rhs.timestamp);
}

constexpr CacheSyncOps& operator|=(CacheSyncOps& lhs, CacheSyncOps rhs)
{
    lhs.glxFlags  |= rhs.glxFlags;
    lhs.rbCache   |= rhs.rbCache;
    lhs.timestamp |= rhs.timestamp;
    return lhs;
}

// =====================================================================================================================
// HWL Barrier Processing Manager: contain layout transition BLT and pre/post-BLT execution and memory dependencies.
class BarrierMgr final : public GfxBarrierMgr
{
public:
    explicit BarrierMgr(GfxDevice* pGfxDevice);
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
        bool                isClearToTarget = false) const override; // isClearToTarget: Optimization hint,
                                                                     //                  no used in gfx12.

    virtual bool OptimizeAccessMask(
        const GfxCmdBuffer* pCmdBuf,
        BarrierType         barrierType,
        const Pal::Image*   pImage,
        uint32*             pSrcAccessMask,
        uint32*             pDstAccessMask,
        bool                shaderMdAccessIndirectOnly) const override;

private:
    // Image layout transition type
    enum class LayoutTransition : uint32
    {
        None = 0x0,

        InitMaskRam,            // Initialize HiZ/His including HiS Pretests metadata
        ExpandHiSZRange,        // Expand HiZ or HiS with full range.
    };

    // A structure that helps cache BLT transition requests for an image barrier.
    struct ImgTransitionInfo
    {
        ImgBarrier       imgBarrier;
        LayoutTransition type;
    };

    using ImgLayoutTransitionList = Util::AutoBuffer<ImgTransitionInfo, 8, Platform>;

    // The only image layout transition BLT is HiZ/HiS range fixup via compute.
    static constexpr uint32 BltStageMask  = Pal::PipelineStageCs;
    static constexpr uint32 BltAccessMask = Pal::CoherShader;

    static LayoutTransition GetLayoutTransitionType(
        const IImage*      pImage,
        const SubresRange& subresRange,
        ImageLayout        oldLayout,
        ImageLayout        newLayout);

    CacheSyncOps GetCacheSyncOps(
        GfxCmdBuffer* pCmdBuf,
        BarrierType   barrierType,
        const IImage* pImage,
        uint32        srcAccessMask,
        uint32        dstAccessMask) const;

    void OptimizeReadOnlyBarrier(
        GfxCmdBuffer* pCmdBuf,
        BarrierType   barrierType,
        const IImage* pImage,
        uint32*       pSrcStageMask,
        uint32*       pDstStageMask,
        uint32*       pSrcAccessMask,
        uint32*       pDstAccessMask) const;

    CacheSyncOps IssueLayoutTransitionBlt(
        GfxCmdBuffer*                  pCmdBuf,
        const ImgLayoutTransitionList& bltList,
        uint32                         bltCount,
        uint32*                        pPostBltStageMask,
        Developer::BarrierOperations*  pBarrierOps) const;

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
        uint32                        srcStageMask,
        uint32                        dstStageMask,
        CacheSyncOps                  cacheOps,
        Developer::BarrierOperations* pBarrierOps) const;

    AcquirePoint GetAcquirePoint(uint32 dstStageMask, EngineType engineType) const;

    const Gfx12::Device& m_gfxDevice;
    const CmdUtil&       m_cmdUtil;

    PAL_DISALLOW_DEFAULT_CTOR(BarrierMgr);
    PAL_DISALLOW_COPY_AND_ASSIGN(BarrierMgr);
};

}
}
