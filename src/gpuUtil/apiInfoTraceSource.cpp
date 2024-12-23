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

#if PAL_BUILD_RDF
#include "palAutoBuffer.h"
#include "core/platform.h"
#include "core/device.h"
#include "apiInfoTraceSource.h"

using namespace Pal;

namespace GpuUtil
{

// =====================================================================================================================
ApiInfoTraceSource::ApiInfoTraceSource(
    Platform* pPlatform)
    :
    m_pPlatform(pPlatform)
{
}

// =====================================================================================================================
ApiInfoTraceSource::~ApiInfoTraceSource()
{
}

// =====================================================================================================================
void ApiInfoTraceSource::FillTraceChunkApiInfo(
    TraceChunk::ApiInfo* pApiInfo)
{
    switch (m_pPlatform->GetClientApiId())
    {
    case ClientApi::Vulkan:
        pApiInfo->apiType = TraceChunk::ApiType::Vulkan;
        break;
    case ClientApi::Pal:
    default:
        pApiInfo->apiType = TraceChunk::ApiType::Generic;
        break;
    }

    pApiInfo->apiVersionMajor = m_pPlatform->GetClientApiMajorVer();
    pApiInfo->apiVersionMinor = m_pPlatform->GetClientApiMinorVer();
}

// =====================================================================================================================
void ApiInfoTraceSource::OnTraceFinished()
{
    TraceChunk::ApiInfo apiInfo = { };
    FillTraceChunkApiInfo(&apiInfo);

    TraceChunkInfo info = {
        .version           = TraceChunk::ApiChunkVersion,
        .pHeader           = nullptr,
        .headerSize        = 0,
        .pData             = &apiInfo,
        .dataSize          = sizeof(TraceChunk::ApiInfo),
        .enableCompression = false
    };
    memcpy(info.id, TraceChunk::ApiChunkTextIdentifier, TextIdentifierSize);

    Result result = m_pPlatform->GetTraceSession()->WriteDataChunk(this, info);
    PAL_ASSERT(result == Result::Success);
}

} // namespace GpuUtil
#endif
