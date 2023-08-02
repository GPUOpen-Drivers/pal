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
#include "palDeveloperHooks.h"

namespace Pal
{

class  Device;
class  GfxCmdBuffer;
class  GfxDevice;
class  IGpuEvent;
class  Platform;
struct AcquireReleaseInfo;
struct BarrierInfo;

namespace Developer
{
struct BarrierOperations;
}

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

protected:
    GfxDevice*const   m_pGfxDevice;
    Pal::Device*const m_pDevice;
    Platform*const    m_pPlatform;

private:
    PAL_DISALLOW_DEFAULT_CTOR(GfxBarrierMgr);
    PAL_DISALLOW_COPY_AND_ASSIGN(GfxBarrierMgr);
};

}
