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

#include <ddRpcClient.h>

namespace AmdLogUtils
{

class AmdLogUtilsClient
{
public:
    AmdLogUtilsClient();
    ~AmdLogUtilsClient();

    DD_RESULT Connect(const DDRpcClientCreateInfo& info);
    DD_RESULT IsServiceAvailable();
    DD_RESULT GetServiceInfo(DDApiVersion* pVersion);

    /// Queries the list of supported clock modes
    DD_RESULT QueryDeviceClocks(
        const void*         pParamBuffer,
        size_t              paramBufferSize,
        const DDByteWriter& writer
    );

    /// Queries which clock mode is currently active
    DD_RESULT QueryCurrentClockMode(
        const void*         pParamBuffer,
        size_t              paramBufferSize,
        const DDByteWriter& writer
    );

    /// Requests that the current clock mode be changed to the provided one
    DD_RESULT SetClockMode(
        const void* pParamBuffer,
        size_t      paramBufferSize
    );

private:
    DDRpcClient m_hClient = DD_API_INVALID_HANDLE;
};

} // namespace AmdLogUtils
