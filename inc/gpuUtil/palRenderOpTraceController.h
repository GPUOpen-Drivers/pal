/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "palTraceSession.h"

namespace Pal
{
class IPlatform;
class IQueue;
class ICmdBuffer;
class Device;
}

namespace GpuUtil
{

constexpr Pal::uint32 RenderOpTraceControllerVersion = 2;
constexpr char        RenderOpTraceControllerName[]  = "renderop";

// =====================================================================================================================
class RenderOpTraceController : public ITraceController
{
public:
    enum RenderOp : Pal::uint8
    {
        RenderOpDraw     = (1u << 0),
        RenderOpDispatch = (1u << 1)
    };

    RenderOpTraceController(Pal::IPlatform* pPlatform, Pal::IDevice* pDevice);
    virtual ~RenderOpTraceController();

    virtual const char* GetName() const override { return RenderOpTraceControllerName; }
    virtual Pal::uint32 GetVersion() const override { return RenderOpTraceControllerVersion; }

    virtual void OnConfigUpdated(DevDriver::StructuredValue* pJsonConfig) override;
    virtual Pal::Result OnTraceRequested() override { return Pal::Result::Success; }

    virtual Pal::Result OnBeginGpuWork(Pal::uint32 gpuIndex, Pal::ICmdBuffer** ppCmdBuffer) override;
    virtual Pal::Result OnEndGpuWork(Pal::uint32 gpuIndex, Pal::ICmdBuffer** ppCmdBuffer) override;

    void RecordRenderOp(Pal::IQueue* pQueue, RenderOp renderOp);

    void FinishTrace();

private:
    Pal::Result SubmitGpuWork(Pal::ICmdBuffer* pCmdBuf, Pal::IFence* pFence) const;
    Pal::Result WaitForTraceEndGpuWorkCompletion() const;
    Pal::Result CreateFence(Pal::IFence** ppFence) const;
    Pal::Result CreateCommandBuffer(bool traceEnd, Pal::ICmdBuffer** ppCmdBuf) const;
    Pal::Result CreateCmdAllocator();

    void OnRenderOpUpdated();
    void FreeResources();
    void AbortTrace();

    Pal::IPlatform* const m_pPlatform;             // Platform associated with this TraceController
    Pal::IDevice*         m_pDevice;               // Device associated with this TraceController
    Pal::ICmdAllocator*   m_pCmdAllocator;         // Command allocator for the TraceController

    TraceSession*         m_pTraceSession;         // TraceSession owning this TraceController
    Pal::uint64           m_supportedGpuMask;      // Bit mask of GPU indices that are capable of participating in the trace
    Pal::uint8            m_renderOpMask;          // Bitmask of RenderOp modes, indicating which are accepted
    Pal::uint64           m_renderOpCount;         // The "global" count, incremented on every render op
    Pal::uint64           m_numPrepRenderOps;      // Number of "warm-up" frames before the start frame
    Pal::uint64           m_captureRenderOpCount;  // Number of frames to wait before ending the trace
    Pal::uint64           m_renderOpTraceAccepted; // The frame number when the trace was accepted

    Util::Mutex           m_renderOpLock;          // Lock over UpdateFrame/OnFrameUpdated
    Pal::IQueue*          m_pQueue;                // The queue being used to submit Begin/End GPU trace command buffers
    Pal::ICmdBuffer*      m_pCmdBufTraceBegin;     // Command buffer to submit Trace Begin
    Pal::ICmdBuffer*      m_pCmdBufTraceEnd;       // Command buffer to submit Trace End
    Pal::IFence*          m_pFenceTraceEnd;        // Fence to wait for Trace End command buffer completion
};

} // namespace GpuUtil
