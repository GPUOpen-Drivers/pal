/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "palHashMap.h"
#include "palTraceSession.h"

namespace Pal
{
class Platform;
class Device;
class CmdBuffer;
}

namespace GpuUtil
{

const Pal::uint32 FrameTraceControllerVersion = 3;
const char        FrameTraceControllerName[]  = "frame";

// =====================================================================================================================
// Responsible for driving the TraceSession from begin to end based on presentation logic triggers
class FrameTraceController : public ITraceController
{
public:
    FrameTraceController(Pal::Platform* pPlatform);
    virtual ~FrameTraceController();

    virtual const char* GetName() const override { return FrameTraceControllerName; }
    virtual Pal::uint32 GetVersion() const override { return FrameTraceControllerVersion; }

    virtual void OnConfigUpdated(DevDriver::StructuredValue* pJsonConfig) override;
    virtual Pal::Result OnTraceRequested() override;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 908
    virtual Pal::Result OnPreparationGpuWork(Pal::uint32 gpuIndex, Pal::ICmdBuffer** ppCmdBuf) override;
#endif
    virtual Pal::Result OnBeginGpuWork(Pal::uint32 gpuIndex, Pal::ICmdBuffer** ppCmdBuffer) override;
    virtual Pal::Result OnEndGpuWork(Pal::uint32 gpuIndex, Pal::ICmdBuffer** ppCmdBuffer) override;

    void FinishTrace();
    void UpdateFrame(Pal::IQueue *pQueue);
    void OnFrameUpdated();

    Pal::uint32 FrameCount() const { return m_frameCount; }

    virtual Pal::Result OnTraceCanceled() override;

private:
    /// Controls whether the trace proceeds on absolute frame indices or relative
    enum class CaptureMode : Pal::uint8
    {
        Relative = 0, ///< Relative to when the trace request is received
        Absolute      ///< Absolute frame index
    };

    Pal::Result AcceptTrace();
    Pal::Result BeginTrace();

    Pal::Result SubmitBeginTraceGpuWork() const;
    Pal::Result SubmitEndTraceGpuWork();

    Pal::Result CreateFence(Pal::Device*  pDevice,
                            Pal::IFence** ppFence) const;

    Pal::Result CreateCommandBuffer(Pal::Device*      pDevice,
                                    bool              traceEnd,
                                    Pal::ICmdBuffer** ppCmdBuf) const;

    Pal::Platform*const m_pPlatform;        // Platform associated with this TraceController

    Pal::uint64      m_supportedGpuMask;    // Bit mask of GPU indices that are capable of participating in the trace
    CaptureMode      m_captureMode;         // Modality for determining the start frame of a trace
    Pal::uint64      m_frameCount;          // The "global" frame count, incremented on every frame
    Pal::uint64      m_prepStartIndex;      // The relative or absolute frame index indicating trace begin
    Pal::uint64      m_frameTraceAccepted;  // The frame number when the trace was accepted
    Pal::uint32      m_numPrepFrames;       // Number of "warm-up" frames before the start frame
    Pal::uint32      m_captureFrameCount;   // Number of frames to wait before ending the trace
    Util::Mutex      m_framePresentLock;    // Lock over UpdateFrame/OnFrameUpdated
    TraceSession*    m_pTraceSession;       // TraceSession owning this TraceController
    Pal::IQueue*     m_pQueue;              // The queue being used to submit Begin/End GPU trace command buffers
    Pal::ICmdBuffer* m_pCmdBufTraceBegin;   // Command buffer to submit Trace Begin
    Pal::ICmdBuffer* m_pCmdBufTraceEnd;     // Command buffer to submit Trace End
    Pal::IFence*     m_pTraceEndFence;      // Fence to wait for Trace End command buffer completion
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 908
    Pal::ICmdBuffer* m_pCmdBufTracePrepare; // Command buffer for recording during the prep phase
#endif
};

} // namespace GpuUtil
