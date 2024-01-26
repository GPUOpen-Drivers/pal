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

#include <UberTraceService.h>

namespace UberTrace
{

const DDRpcServerRegisterServiceInfo IService::kServiceInfo = []() -> DDRpcServerRegisterServiceInfo {
    DDRpcServerRegisterServiceInfo info = {};
    info.id                             = 0x63727461;
    info.version.major                  = 0;
    info.version.minor                  = 2;
    info.version.patch                  = 0;
    info.pName                          = "UberTrace";
    info.pDescription                   = "A service that provides generic trace functionality";

    return info;
}();

static DD_RESULT RegisterFunctions(
    DDRpcServer hServer,
    IService* pService)
{
    DD_RESULT result = DD_RESULT_SUCCESS;

    // Register "EnableTracing"
    if (result == DD_RESULT_SUCCESS)
    {
        DDRpcServerRegisterFunctionInfo info = {};
        info.serviceId                       = 0x63727461;
        info.id                              = 0x1;
        info.pName                           = "EnableTracing";
        info.pDescription                    = "Attempts to enable tracing";
        info.pFuncUserdata                   = pService;
        info.pfnFuncCb                       = [](
            const DDRpcServerCallInfo* pCall) -> DD_RESULT
        {
            auto* pService = reinterpret_cast<IService*>(pCall->pUserdata);

            // Execute the service implementation
            return pService->EnableTracing();
        };

        result = ddRpcServerRegisterFunction(hServer, &info);
    }

    // Register "QueryTraceParams"
    if (result == DD_RESULT_SUCCESS)
    {
        DDRpcServerRegisterFunctionInfo info = {};
        info.serviceId                       = 0x63727461;
        info.id                              = 0x2;
        info.pName                           = "QueryTraceParams";
        info.pDescription                    = "Queries the current set of trace parameters";
        info.pFuncUserdata                   = pService;
        info.pfnFuncCb                       = [](
            const DDRpcServerCallInfo* pCall) -> DD_RESULT
        {
            auto* pService = reinterpret_cast<IService*>(pCall->pUserdata);

            // Execute the service implementation
            return pService->QueryTraceParams(*pCall->pWriter);
        };

        result = ddRpcServerRegisterFunction(hServer, &info);
    }

    // Register "ConfigureTraceParams"
    if (result == DD_RESULT_SUCCESS)
    {
        DDRpcServerRegisterFunctionInfo info = {};
        info.serviceId                       = 0x63727461;
        info.id                              = 0x3;
        info.pName                           = "ConfigureTraceParams";
        info.pDescription                    = "Configures the current set of trace parameters";
        info.pFuncUserdata                   = pService;
        info.pfnFuncCb                       = [](
            const DDRpcServerCallInfo* pCall) -> DD_RESULT
        {
            auto* pService = reinterpret_cast<IService*>(pCall->pUserdata);

            // Execute the service implementation
            return pService->ConfigureTraceParams(pCall->pParameterData, pCall->parameterDataSize);
        };

        result = ddRpcServerRegisterFunction(hServer, &info);
    }

    // Register "RequestTrace"
    if (result == DD_RESULT_SUCCESS)
    {
        DDRpcServerRegisterFunctionInfo info = {};
        info.serviceId                       = 0x63727461;
        info.id                              = 0x4;
        info.pName                           = "RequestTrace";
        info.pDescription                    = "Requests execution of a trace";
        info.pFuncUserdata                   = pService;
        info.pfnFuncCb                       = [](
            const DDRpcServerCallInfo* pCall) -> DD_RESULT
        {
            auto* pService = reinterpret_cast<IService*>(pCall->pUserdata);

            // Execute the service implementation
            return pService->RequestTrace();
        };

        result = ddRpcServerRegisterFunction(hServer, &info);
    }

    // Register "CancelTrace"
    if (result == DD_RESULT_SUCCESS)
    {
        DDRpcServerRegisterFunctionInfo info = {};
        info.serviceId                       = 0x63727461;
        info.id                              = 0x5;
        info.pName                           = "CancelTrace";
        info.pDescription                    = "Cancels a previously requested trace before it starts or after it completes";
        info.pFuncUserdata                   = pService;
        info.pfnFuncCb                       = [](
            const DDRpcServerCallInfo* pCall) -> DD_RESULT
        {
            auto* pService = reinterpret_cast<IService*>(pCall->pUserdata);

            // Execute the service implementation
            return pService->CancelTrace();
        };

        result = ddRpcServerRegisterFunction(hServer, &info);
    }

    // Register "CollectTrace"
    if (result == DD_RESULT_SUCCESS)
    {
        DDRpcServerRegisterFunctionInfo info = {};
        info.serviceId                       = 0x63727461;
        info.id                              = 0x6;
        info.pName                           = "CollectTrace";
        info.pDescription                    = "Collects the data created by a previously executed trace";
        info.pFuncUserdata                   = pService;
        info.pfnFuncCb                       = [](
            const DDRpcServerCallInfo* pCall) -> DD_RESULT
        {
            auto* pService = reinterpret_cast<IService*>(pCall->pUserdata);

            // Execute the service implementation
            return pService->CollectTrace(*pCall->pWriter);
        };

        result = ddRpcServerRegisterFunction(hServer, &info);
    }

    return result;
}

DD_RESULT RegisterService(
    DDRpcServer hServer,
    IService* pService
)
{
    // Register the service
    DD_RESULT result = ddRpcServerRegisterService(hServer, &IService::kServiceInfo);

    // Register individual functions
    if (result == DD_RESULT_SUCCESS)
    {
        result = RegisterFunctions(hServer, pService);

        if (result != DD_RESULT_SUCCESS)
        {
            // Unregister the service if registering functions fails
            ddRpcServerUnregisterService(hServer, IService::kServiceInfo.id);
        }
    }

    return result;
}

} // namespace UberTrace
