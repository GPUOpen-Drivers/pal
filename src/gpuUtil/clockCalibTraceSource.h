/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2023-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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
    Pal::uint64 cpuTimestamp;
    Pal::uint64 gpuTimestamp;
};

namespace GpuUtil
{

constexpr char        ClockCalibTraceSourceName[]  = "clockcalibration";
constexpr Pal::uint32 ClockCalibTraceSourceVersion = 1;
const char ClockCalibTextId[TextIdentifierSize]    = /* "ClockCalibration" */
    { 'C','l','o','c','k','C','a','l','i','b','r','a','t','i','o','n' }; // Using array form, since the null-terminator
                                                                         // from a string literal would put us over
                                                                         // 16 chars

// =====================================================================================================================
class ClockCalibrationTraceSource : public ITraceSource
{
public:
    ClockCalibrationTraceSource(Pal::Platform* pPlatform) : m_pPlatform(pPlatform) { }
    virtual ~ClockCalibrationTraceSource() { }

    virtual void OnConfigUpdated(DevDriver::StructuredValue* pJsonConfig) override { }

    virtual Pal::uint64 QueryGpuWorkMask() const override { return 0; }

    virtual void OnTraceAccepted() override { }
    virtual void OnTraceBegin(Pal::uint32 gpuIndex, Pal::ICmdBuffer* pCmdBuf) override { }
    virtual void OnTraceEnd(Pal::uint32 gpuIndex, Pal::ICmdBuffer* pCmdBuf) override { }
    virtual void OnTraceFinished() override;

    virtual const char* GetName() const override { return ClockCalibTraceSourceName; }

    virtual Pal::uint32 GetVersion() const override { return ClockCalibTraceSourceVersion; }

private:
    // Writes a clock calibration chunk for each Platform-held device to the trace session.
    void WriteClockCalibrationChunks();

    Pal::Platform* const m_pPlatform;
};

} // namespace GpuUtil
