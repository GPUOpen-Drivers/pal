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
#include "core/platform.h"
#include "uberTraceService.h"
#include <util/ddStructuredReader.h>

using namespace Pal;

namespace GpuUtil
{

// =====================================================================================================================
// Translates a Pal::Result to a DD_RESULT
static DD_RESULT PalResultToDdResult(
    Result result)
{
    DD_RESULT devDriverResult;

    switch (result)
    {
    case Result::Success:
        devDriverResult = DD_RESULT_SUCCESS;
        break;
    case Result::ErrorInvalidPointer:
    case Result::ErrorInvalidValue:
        devDriverResult = DD_RESULT_DD_GENERIC_INVALID_PARAMETER;
        break;
    case Result::ErrorUnavailable:
        devDriverResult = DD_RESULT_DD_GENERIC_UNAVAILABLE;
        break;
    case Result::NotReady:
        devDriverResult = DD_RESULT_DD_GENERIC_NOT_READY;
        break;
    case Result::ErrorInvalidMemorySize:
        devDriverResult = DD_RESULT_DD_GENERIC_INSUFFICIENT_MEMORY;
        break;
    case Result::ErrorUnknown:
    default:
        devDriverResult = DD_RESULT_UNKNOWN;
        break;
    }

    return devDriverResult;
}

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
DD_RESULT UberTraceService::EnableTracing()
{
    m_pPlatform->GetTraceSession()->EnableTracing();
    return DD_RESULT_SUCCESS;
}

// =====================================================================================================================
DD_RESULT UberTraceService::ConfigureTraceParams(
    const void* pParamBuffer,
    size_t      paramBufferSize)
{
    Result    result          = m_pPlatform->GetTraceSession()->UpdateTraceConfig(pParamBuffer, paramBufferSize);
    DD_RESULT devDriverResult = PalResultToDdResult(result);
    return devDriverResult;
}

// =====================================================================================================================
DD_RESULT UberTraceService::RequestTrace()
{
    DD_RESULT devDriverResult = (m_pPlatform->GetTraceSession()->RequestTrace() == Result::Success) ?
                                DD_RESULT_SUCCESS : DD_RESULT_DD_GENERIC_UNAVAILABLE;
    return devDriverResult;
}

// =====================================================================================================================
DD_RESULT UberTraceService::CollectTrace(
    const DDByteWriter& writer)
{
    size_t pDataSize = 0;
    char*  pData     = nullptr;

    // CollectTrace needs to be called twice: (1) to retrieve the correct trace data size for buffer allocation and
    // (2) to consume any trace data stored within TraceSession. When the buffer pointer is null, CollectTrace returns
    // only the data size.
    DD_RESULT result = PalResultToDdResult(m_pPlatform->GetTraceSession()->CollectTrace(pData, &pDataSize));

    if (result == DD_RESULT_SUCCESS)
    {
        pData  = PAL_NEW_ARRAY(char, pDataSize, m_pPlatform, Util::AllocInternalTemp);
        result = PalResultToDdResult(m_pPlatform->GetTraceSession()->CollectTrace(pData, &pDataSize));
    }

    if (result == DD_RESULT_SUCCESS)
    {
        result = writer.pfnBegin(writer.pUserdata, &pDataSize);

        if (result == DD_RESULT_SUCCESS)
        {
            result = writer.pfnWriteBytes(writer.pUserdata, pData, pDataSize);
        }

        writer.pfnEnd(writer.pUserdata, result);

        PAL_SAFE_DELETE_ARRAY(pData, m_pPlatform);
    }

    return result;
}
}
#endif
