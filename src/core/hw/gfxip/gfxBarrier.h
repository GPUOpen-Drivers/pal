/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "pal.h"
#include "palCmdBuffer.h"
#include "palDeveloperHooks.h"

namespace Pal
{

class  Device;
class  GfxCmdBuffer;
class  GfxDevice;
class  IGpuEvent;
class  Platform;
class  Pm4CmdBuffer;
struct AcquireReleaseInfo;
struct BarrierInfo;

namespace Developer
{
struct BarrierOperations;
}

// Acquire/release synchronization event types for supported pipeline event.
enum class AcqRelEventType : uint32
{
    Eop    = 0x0,
    PsDone = 0x1,
    CsDone = 0x2,
    Count,

    Invalid = Count
};

// Bit mask value of AcqRelEventType
enum AcqRelEventTypeMask : uint32
{
    AcqRelEventMaskEop      = 1u << uint32(AcqRelEventType::Eop),
    AcqRelEventMaskPsDone   = 1u << uint32(AcqRelEventType::PsDone),
    AcqRelEventMaskCsDone   = 1u << uint32(AcqRelEventType::CsDone),

    AcqRelEventMaskPsCsDone = AcqRelEventMaskPsDone | AcqRelEventMaskCsDone,
    AcqRelEventMaskAll      = (1u << uint32(AcqRelEventType::Count)) - 1
};

// Acquire/release synchronization token structure.
union AcqRelSyncToken
{
    struct
    {
        uint32 fenceVal : 30;
        uint32 type     :  2; // AcqRelEventType
    };

    uint32 u32All;
};

constexpr uint32 PipelineStagesGraphicsOnly = PipelineStageFetchIndices  |
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 770
                                              PipelineStageStreamOut     |
#endif
                                              PipelineStageVs            |
                                              PipelineStageHs            |
                                              PipelineStageDs            |
                                              PipelineStageGs            |
                                              PipelineStagePs            |
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 835
                                              PipelineStageSampleRate    |
#endif
                                              PipelineStageEarlyDsTarget |
                                              PipelineStageLateDsTarget  |
                                              PipelineStageColorTarget;

constexpr uint32 CacheCoherencyGraphicsOnly = CoherColorTarget        |
                                              CoherDepthStencilTarget |
                                              CoherSampleRate         |
                                              CoherCeLoad             |
                                              CoherCeDump             |
                                              CoherStreamOut          |
                                              CoherIndexData;

// There are various BLTs(Copy, Clear, and Resolve) that can involve different caches based on what engine
// does the BLT. Note that the compute shader support for masked clears requires an implicit read-modify-write.
constexpr uint32 CacheCoherencyBltSrc = CoherCopySrc | CoherResolveSrc | CoherClear;
constexpr uint32 CacheCoherencyBltDst = CoherCopyDst | CoherResolveDst | CoherClear;
constexpr uint32 CacheCoherencyBlt    = CacheCoherencyBltSrc | CacheCoherencyBltDst;

// =====================================================================================================================
// BASE barrier Processing Manager: only contain execution and memory dependencies.
class GfxBarrierMgr
{
public:
    explicit GfxBarrierMgr(GfxDevice* pGfxDevice);
    virtual ~GfxBarrierMgr() {}

    virtual void Barrier(
        GfxCmdBuffer*                 pGfxCmdBuf,
        const BarrierInfo&            barrierInfo,
        Developer::BarrierOperations* pBarrierOps) const { PAL_NEVER_CALLED(); }

    virtual uint32 Release(
        GfxCmdBuffer*                 pGfxCmdBuf,
        const AcquireReleaseInfo&     releaseInfo,
        Developer::BarrierOperations* pBarrierOps) const { PAL_NEVER_CALLED(); return 0; }

    virtual void Acquire(
        GfxCmdBuffer*                 pGfxCmdBuf,
        const AcquireReleaseInfo&     acquireInfo,
        uint32                        syncTokenCount,
        const uint32*                 pSyncTokens,
        Developer::BarrierOperations* pBarrierOps) const { PAL_NEVER_CALLED(); }

    virtual void ReleaseEvent(
        GfxCmdBuffer*                 pGfxCmdBuf,
        const AcquireReleaseInfo&     releaseInfo,
        const IGpuEvent*              pClientEvent,
        Developer::BarrierOperations* pBarrierOps) const { PAL_NEVER_CALLED(); }

    virtual void AcquireEvent(
        GfxCmdBuffer*                 pGfxCmdBuf,
        const AcquireReleaseInfo&     acquireInfo,
        uint32                        gpuEventCount,
        const IGpuEvent* const*       ppGpuEvents,
        Developer::BarrierOperations* pBarrierOps) const { PAL_NEVER_CALLED(); }

    virtual void ReleaseThenAcquire(
        GfxCmdBuffer*                 pGfxCmdBuf,
        const AcquireReleaseInfo&     barrierInfo,
        Developer::BarrierOperations* pBarrierOps) const { PAL_NEVER_CALLED(); }

    void DescribeBarrier(
        GfxCmdBuffer*                 pGfxCmdBuf,
        const BarrierTransition*      pTransition,
        Developer::BarrierOperations* pOperations) const;

    void DescribeBarrierStart(GfxCmdBuffer* pGfxCmdBuf, uint32 reason, Developer::BarrierType type) const;
    void DescribeBarrierEnd(GfxCmdBuffer* pGfxCmdBuf, Developer::BarrierOperations* pOperations) const;

    static Result SplitBarrierTransitions(
        Platform*    pPlatform,
        BarrierInfo* pBarrier,
        bool*        pMemAllocated);

    static Result SplitImgBarriers(
        Platform*           pPlatform,
        AcquireReleaseInfo* pBarrier,
        bool*               pMemAllocated);

    static void OptimizePipePoint(const Pm4CmdBuffer* pCmdBuf, HwPipePoint* pPipePoint);

    static void OptimizeSrcCacheMask(const Pm4CmdBuffer* pCmdBuf, uint32* pCacheMask);

    static void OptimizePipeStageAndCacheMask(
        const Pm4CmdBuffer* pCmdBuf,
        uint32*             pSrcStageMask,
        uint32*             pSrcAccessMask,
        uint32*             pDstStageMask,
        uint32*             pDstAccessMask);

    static void SetBarrierOperationsRbCacheSynced(Developer::BarrierOperations* pOperations)
    {
        pOperations->caches.flushCb = 1;
        pOperations->caches.invalCb = 1;
        pOperations->caches.flushDb = 1;
        pOperations->caches.invalDb = 1;
        pOperations->caches.flushCbMetadata = 1;
        pOperations->caches.invalCbMetadata = 1;
        pOperations->caches.flushDbMetadata = 1;
        pOperations->caches.invalDbMetadata = 1;
    }

protected:
    static uint32 GetPipelineStageMaskFromBarrierInfo(const BarrierInfo& barrierInfo, uint32* pSrcStageMask);

    static bool IsReadOnlyTransition(uint32 srcAccessMask, uint32 dstAccessMask);

    // Cache coherency masks that are writable.
    static constexpr uint32 CacheCoherWriteMask  = CoherCpu         | CoherShaderWrite        | CoherStreamOut |
                                                   CoherColorTarget | CoherClear              | CoherCopyDst   |
                                                   CoherResolveDst  | CoherDepthStencilTarget | CoherCeDump    |
                                                   CoherQueueAtomic | CoherTimestamp          | CoherMemory;

    GfxDevice*const   m_pGfxDevice;
    Pal::Device*const m_pDevice;
    Platform*const    m_pPlatform;

private:
    PAL_DISALLOW_DEFAULT_CTOR(GfxBarrierMgr);
    PAL_DISALLOW_COPY_AND_ASSIGN(GfxBarrierMgr);
};

}
