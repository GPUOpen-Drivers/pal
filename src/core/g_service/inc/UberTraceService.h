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

#pragma once

#include <ddRpcServer.h>

namespace UberTrace
{

class IService
{
public:
    virtual ~IService() {}

    static const DDRpcServerRegisterServiceInfo kServiceInfo;

    // Attempts to enable tracing
    virtual DD_RESULT EnableTracing() = 0;

    // Queries the current set of trace parameters
    virtual DD_RESULT QueryTraceParams(
        const DDByteWriter& writer
    ) = 0;

    // Configures the current set of trace parameters
    virtual DD_RESULT ConfigureTraceParams(
        const void* pParamBuffer,
        size_t      paramBufferSize
    ) = 0;

    // Requests execution of a trace
    virtual DD_RESULT RequestTrace() = 0;

    // Cancels a previously requested trace before it starts or after it completes
    virtual DD_RESULT CancelTrace() = 0;

    // Collects the data created by a previously executed trace
    virtual DD_RESULT CollectTrace(
        const DDByteWriter& writer
    ) = 0;

protected:
    IService() {}
};

DD_RESULT RegisterService(DDRpcServer hServer, IService* pService);

} // namespace UberTrace
