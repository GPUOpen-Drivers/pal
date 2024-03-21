/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

namespace GpuUtil
{

namespace TraceChunk
{

const Pal::uint32 ApiChunkVersion = 2;
const char        ApiChunkTextIdentifier[TextIdentifierSize] = "ApiInfo";

enum class ApiType : Pal::uint32
{
    Generic    = 0,
    DirectX9   = 1,
    DirectX11  = 2,
    DirectX12  = 3,
    Vulkan     = 4,
    OpenGl     = 5,
    OpenCl     = 6,
    Mantle     = 7,
    Hip        = 8,
};

/// API Info struct, based off of SqttFileChunkApiInfo.
struct ApiInfo
{
    ApiType     apiType;         // Client API type
    Pal::uint16 apiVersionMajor; // Major client API version
    Pal::uint16 apiVersionMinor; // Minor client API version
};

} // namespace TraceChunk

const Pal::uint32 ApiInfoTraceSourceVersion = 2;
const char        ApiInfoTraceSourceName[]  = "apiinfo";

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

private:
    void FillTraceChunkApiInfo(TraceChunk::ApiInfo* pApiInfo);

    Pal::Platform* const m_pPlatform;
};

} // namespace GpuUtil

