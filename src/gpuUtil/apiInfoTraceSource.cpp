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
void ApiInfoTraceSource::FillTraceChunkApiInfo(TraceChunkApiInfo* pApiInfo)
{
    pApiInfo->apiType         = TraceApiType::VULKAN;
    pApiInfo->apiVersionMajor = m_pPlatform->GetClientApiMajorVer();
    pApiInfo->apiVersionMinor = m_pPlatform->GetClientApiMinorVer();
}

// =====================================================================================================================
// Translate TraceChunkApiInfo to TraceChunkInfo and write it into TraceSession
void ApiInfoTraceSource::WriteApiInfoTraceChunk()
{
    Result result = Result::Success;
    // Populate the TraceApiChunk with the Api details
    TraceChunkApiInfo traceChunkApiInfo = {};
    memset(&traceChunkApiInfo, 0, sizeof(TraceChunkApiInfo));
    FillTraceChunkApiInfo(&traceChunkApiInfo);

    TraceChunkInfo info;
    memcpy(info.id, apiChunkTextIdentifier, GpuUtil::TextIdentifierSize);
    info.pHeader           = nullptr;
    info.headerSize        = 0;
    info.version           = 1;
    info.pData             = &traceChunkApiInfo;
    info.dataSize          = sizeof(TraceChunkApiInfo);
    info.enableCompression = false;

    result = m_pPlatform->GetTraceSession()->WriteDataChunk(this, info);

    if (result != Result::Success)
    {
        const char errorMessage[] = "[ApiInfoChunk] Error Writing Chunk Data";

        m_pPlatform->GetTraceSession()->ReportError(
            info.id,
            errorMessage,
            sizeof(errorMessage),
            TraceErrorPayload::ErrorString,
            result);
    }

}

// =====================================================================================================================
void ApiInfoTraceSource::OnTraceFinished()
{
    WriteApiInfoTraceChunk();
}

}
#endif
