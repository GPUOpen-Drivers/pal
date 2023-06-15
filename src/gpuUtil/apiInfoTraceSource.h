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

#include "palPlatform.h"
#include "palDevice.h"
#include "palGpuUtil.h"
#include "palTraceSession.h"

namespace Pal
{
class Platform;
}

enum class TraceApiType : Pal::uint32
{
    DIRECTX_9  = 0,
    DIRECTX_11 = 1,
    DIRECTX_12 = 2,
    VULKAN     = 3,
    OPENGL     = 4,
    OPENCL     = 5,
    MANTLE     = 6,
    GENERIC    = 7
};

/// Api Info struct, based off of SqttFileChunkApiInfo. This is to be mapped to the RDF-based TraceChunkInfo
/// in TraceSession.
struct TraceChunkApiInfo
{
    TraceApiType apiType;
    uint16_t     apiVersionMajor; // Major client API version
    uint16_t     apiVersionMinor; // Minor client API version
};

namespace GpuUtil
{

constexpr char ApiInfoTraceSourceName[] = "apiinfo";
constexpr Pal::uint32 ApiInfoTraceSourceVersion = 1;

const char apiChunkTextIdentifier[GpuUtil::TextIdentifierSize] = "ApiInfo";

// =====================================================================================================================
// A trace source that sends ASIC information to the trace session. This is one of the "default" trace sources that are
// registered with the current PAL-owned trace session on start-up.
class ApiInfoTraceSource : public ITraceSource
{
public:
    ApiInfoTraceSource(Pal::Platform* pPlatform);
    virtual ~ApiInfoTraceSource();

    virtual void OnConfigUpdated(DevDriver::StructuredValue* pJsonConfig) override {}

    virtual Pal::uint64 QueryGpuWorkMask() const override { return 0; }

    virtual void OnTraceAccepted() override {}
    virtual void OnTraceBegin(Pal::uint32 gpuIndex, Pal::ICmdBuffer* pCmdBuf) override {}
    virtual void OnTraceEnd(Pal::uint32 gpuIndex, Pal::ICmdBuffer* pCmdBuf) override {}
    virtual void OnTraceFinished() override;

    virtual const char* GetName() const override { return ApiInfoTraceSourceName; }

    virtual Pal::uint32 GetVersion() const override { return ApiInfoTraceSourceVersion; }

    void FillTraceChunkApiInfo(TraceChunkApiInfo* pApiInfo);

    // Translate TraceChunkApiInfo to TraceChunkInfo and write it into TraceSession
    void WriteApiInfoTraceChunk();

private:
    Pal::Platform* const m_pPlatform; // Platform associated with this TraceSource
};

}
