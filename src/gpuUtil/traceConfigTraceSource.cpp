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

#if PAL_BUILD_RDF

#include "traceConfigTraceSource.h"
#include "core/platform.h"

using namespace Pal;

namespace GpuUtil
{

// =====================================================================================================================
TraceConfigTraceSource::TraceConfigTraceSource(
    Platform* pPlatform)
    :
    ITraceSource(),
    m_pPlatform(pPlatform)
{
}

// =====================================================================================================================
void TraceConfigTraceSource::OnTraceFinished()
{
    size_t      dataSize = 0;
    const void* pData    = m_pPlatform->GetTraceSession()->GetTraceConfig(&dataSize);
    PAL_ASSERT(dataSize > 0);

    TraceChunkInfo chunkInfo = {
        .version             = TraceConfigChunkVersion,
        .pHeader             = nullptr,
        .headerSize          = 0,
        .pData               = pData,
        .dataSize            = static_cast<int64>(dataSize),
        .enableCompression   = false
    };
    memcpy(&chunkInfo.id, &TraceConfigChunkId, TextIdentifierSize);

    Result result = m_pPlatform->GetTraceSession()->WriteDataChunk(this, chunkInfo);
    PAL_ASSERT(result == Result::Success);
}

} // namespace GpuUtil
#endif

