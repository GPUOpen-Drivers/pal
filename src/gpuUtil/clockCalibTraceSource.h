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

#include "palPlatform.h"
#include "palGpuUtil.h"
#include "palTraceSession.h"

namespace Pal
{
class Platform;
}

struct TraceChunkClockCalibration
{
    Pal::uint32 pciId;        // The ID of the GPU sampled
    Pal::uint64 cpuTimestamp; // CPU timestamp counter
    Pal::uint64 gpuTimestamp; // GPU timestamp counter
};

namespace GpuUtil
{

constexpr char        ClockCalibTraceSourceName[]  = "clockcalibration";
constexpr Pal::uint32 ClockCalibTraceSourceVersion = 2;

const char ClockCalibTextId[TextIdentifierSize]    = /* "ClockCalibration" */
    { 'C','l','o','c','k','C','a','l','i','b','r','a','t','i','o','n' }; // Using array form, since the null-terminator
                                                                         // from a string literal would put us over
                                                                         // 16 chars

constexpr Pal::uint32 ClockCalibChunkVersion       = 2;

// =====================================================================================================================
class ClockCalibrationTraceSource : public ITraceSource
{
public:
    ClockCalibrationTraceSource(Pal::Platform* pPlatform) : m_pPlatform(pPlatform) { }
    virtual ~ClockCalibrationTraceSource() { }

    virtual void OnConfigUpdated(DevDriver::StructuredValue* pJsonConfig) override { }

    virtual Pal::uint64 QueryGpuWorkMask() const override { return 0; }

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 908
    virtual void OnTraceAccepted(Pal::uint32 gpuIndex, Pal::ICmdBuffer* pCmdBuf) override { }
#else
    virtual void OnTraceAccepted() override { }
#endif
    virtual void OnTraceBegin(Pal::uint32 gpuIndex, Pal::ICmdBuffer* pCmdBuf) override { }
    virtual void OnTraceEnd(Pal::uint32 gpuIndex, Pal::ICmdBuffer* pCmdBuf) override { }
    virtual void OnTraceFinished() override;

    virtual const char* GetName() const override { return ClockCalibTraceSourceName; }

    virtual Pal::uint32 GetVersion() const override { return ClockCalibTraceSourceVersion; }

private:
    Pal::Platform* const m_pPlatform;
};

} // namespace GpuUtil
