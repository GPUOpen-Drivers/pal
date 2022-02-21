/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/platform.h"
#include "uberTraceService.h"
#include <util/ddStructuredReader.h>

using namespace Pal;

namespace GpuUtil
{

// =====================================================================================================================
UberTraceService::UberTraceService(
    Platform* pPlatform)
    :
    m_pPlatform(pPlatform)
{
}

// =====================================================================================================================
UberTraceService::~UberTraceService()
{
}

// =====================================================================================================================
DD_RESULT UberTraceService::RequestTrace()
{
    return DD_RESULT_SUCCESS;
}

// =====================================================================================================================
DD_RESULT UberTraceService::CollectTrace(
    const DDByteWriter& writer)
{
    DD_RESULT devDriverResult = DD_RESULT_COMMON_UNSUPPORTED;
    size_t    pDataSize       = 0;
    char*     pData           = nullptr;

    // CollectTrace needs to be called twice: (1) to retrieve the correct trace data size for buffer allocation and
    // (2) to consume any trace data stored within TraceSession. When the buffer pointer is null, CollectTrace returns
    // only the data size.
    Result result = m_pPlatform->GetTraceSession()->CollectTrace(pData, &pDataSize);
    if (result == Result::Success)
    {
        pData  = PAL_NEW_ARRAY(char, pDataSize, m_pPlatform, Util::AllocInternalTemp);
        result = m_pPlatform->GetTraceSession()->CollectTrace(pData, &pDataSize);
    }

    if (result == Result::Success)
    {
        devDriverResult = writer.pfnBegin(writer.pUserdata, &pDataSize);

        if (devDriverResult == DD_RESULT_SUCCESS)
        {
            devDriverResult = writer.pfnWriteBytes(writer.pUserdata, pData, pDataSize);
        }

        writer.pfnEnd(writer.pUserdata, devDriverResult);

        PAL_SAFE_DELETE_ARRAY(pData, m_pPlatform);
    }

    return devDriverResult;
}
}
#endif
