/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

constexpr char FrameTraceControllerName[] = "framecontroller";
constexpr Pal::uint32 FrameTraceControllerVersion = 1;

// =====================================================================================================================
// Responsible for driving the TraceSession from begin to end based on presentation logic triggers
class FrameTraceController : public ITraceController
{
public:
    FrameTraceController(Pal::Platform* pPlatform);
    virtual ~FrameTraceController();

    Pal::Result Init();

    virtual const char* GetName() const override { return FrameTraceControllerName; }
    virtual Pal::uint32 GetVersion() const override { return FrameTraceControllerVersion; }

    virtual void OnConfigUpdated(DevDriver::StructuredValue* pJsonConfig) override {}

    virtual Pal::Result OnBeginGpuWork(Pal::uint32 gpuIndex, Pal::ICmdBuffer** ppCmdBuffer) override;
    virtual Pal::Result OnEndGpuWork(Pal::uint32 gpuIndex, Pal::ICmdBuffer** ppCmdBuffer) override;

    void FinishTrace();

    void UpdateFrame(Pal::CmdBuffer* pCmdBuffer);
    void OnFrameUpdated();

    Pal::uint32 FrameCount() const { return m_frameCount; }

private:
    Pal::Platform* const m_pPlatform;   // Platform associated with this TraceController
    Pal::Device* m_pCurrentDevice;

    Pal::uint64 m_supportedGpuMask;      // Bit mask of GPU indices that are capable of participating in the trace

    Pal::uint32 m_frameCount;
    Pal::uint32 m_captureStartIndex;      // Frame index where trace will be started, if accepted
    Pal::uint32 m_currentTraceStartIndex; // Starting frame index of current running trace
    Pal::uint32 m_captureFrameCount;      // Number of frames to wait before ending the trace

    Pal::CmdBuffer* m_pCurrentCmdBuffer;  // GPU CmdBuffers for TraceSources to submit gpu-work at trace start/end
    Util::Mutex m_framePresentLock;

    TraceSession* m_pTraceSession;
};

}
