/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2022-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include <g_DriverUtilsService.h>

namespace DriverUtils
{

static DD_RESULT RegisterFunctions(
    DDRpcServer hServer,
    IDriverUtilsService* pService)
{
    DD_RESULT result = DD_RESULT_SUCCESS;

    // Register "EnableTracing"
    if (result == DD_RESULT_SUCCESS)
    {
        DDRpcServerRegisterFunctionInfo info = {};
        info.serviceId                       = 0x24815012;
        info.id                              = 0x1;
        info.pName                           = "EnableTracing";
        info.pDescription                    = "Informs driver we are collecting trace data";
        info.pFuncUserdata                   = pService;
        info.pfnFuncCb                       = [](
            const DDRpcServerCallInfo* pCall) -> DD_RESULT
        {
            auto* pService = reinterpret_cast<IDriverUtilsService*>(pCall->pUserdata);

            // Execute the service implementation
            return pService->EnableTracing();
        };

        result = ddRpcServerRegisterFunction(hServer, &info);
    }

    // Register "EnableCrashAnalysisMode"
    if (result == DD_RESULT_SUCCESS)
    {
        DDRpcServerRegisterFunctionInfo info = {};
        info.serviceId                       = 0x24815012;
        info.id                              = 0x2;
        info.pName                           = "EnableCrashAnalysisMode";
        info.pDescription                    = "Informs driver to enable crash analysis mode";
        info.pFuncUserdata                   = pService;
        info.pfnFuncCb                       = [](
            const DDRpcServerCallInfo* pCall) -> DD_RESULT
        {
            auto* pService = reinterpret_cast<IDriverUtilsService*>(pCall->pUserdata);

            // Execute the service implementation
            return pService->EnableCrashAnalysisMode();
        };

        result = ddRpcServerRegisterFunction(hServer, &info);
    }

    // Register "QueryPalDriverInfo"
    if (result == DD_RESULT_SUCCESS)
    {
        DDRpcServerRegisterFunctionInfo info = {};
        info.serviceId                       = 0x24815012;
        info.id                              = 0x3;
        info.pName                           = "QueryPalDriverInfo";
        info.pDescription                    = "Queries the driver for extended client info";
        info.pFuncUserdata                   = pService;
        info.pfnFuncCb                       = [](
            const DDRpcServerCallInfo* pCall) -> DD_RESULT
        {
            auto* pService = reinterpret_cast<IDriverUtilsService*>(pCall->pUserdata);

            // Execute the service implementation
            return pService->QueryPalDriverInfo(*pCall->pWriter);
        };

        result = ddRpcServerRegisterFunction(hServer, &info);
    }

    // Register "EnableDriverFeatures"
    if (result == DD_RESULT_SUCCESS)
    {
        DDRpcServerRegisterFunctionInfo info = {};
        info.serviceId                       = 0x24815012;
        info.id                              = 0x4;
        info.pName                           = "EnableDriverFeatures";
        info.pDescription                    = "Informs driver to enable different features: Tracing, CrashAnalysis, RT Shader Data Tokens, Debug Vmid";
        info.pFuncUserdata                   = pService;
        info.pfnFuncCb                       = [](
            const DDRpcServerCallInfo* pCall) -> DD_RESULT
        {
            auto* pService = reinterpret_cast<IDriverUtilsService*>(pCall->pUserdata);

            // Execute the service implementation
            return pService->EnableDriverFeatures(pCall->pParameterData, pCall->parameterDataSize);
        };

        result = ddRpcServerRegisterFunction(hServer, &info);
    }

    // Register "SetOverlayString"
    if (result == DD_RESULT_SUCCESS)
    {
        DDRpcServerRegisterFunctionInfo info = {};
        info.serviceId                       = 0x24815012;
        info.id                              = 0x5;
        info.pName                           = "SetOverlayString";
        info.pDescription                    = "Sends a string to PAL to display in the driver overlay";
        info.pFuncUserdata                   = pService;
        info.pfnFuncCb                       = [](
            const DDRpcServerCallInfo* pCall) -> DD_RESULT
        {
            auto* pService = reinterpret_cast<IDriverUtilsService*>(pCall->pUserdata);

            // Execute the service implementation
            return pService->SetOverlayString(pCall->pParameterData, pCall->parameterDataSize);
        };

        result = ddRpcServerRegisterFunction(hServer, &info);
    }

    // Register "SetDbgLogSeverityLevel"
    if (result == DD_RESULT_SUCCESS)
    {
        DDRpcServerRegisterFunctionInfo info = {};
        info.serviceId                       = 0x24815012;
        info.id                              = 0x6;
        info.pName                           = "SetDbgLogSeverityLevel";
        info.pDescription                    = "Set driver DbgLog's severity level";
        info.pFuncUserdata                   = pService;
        info.pfnFuncCb                       = [](
            const DDRpcServerCallInfo* pCall) -> DD_RESULT
        {
            auto* pService = reinterpret_cast<IDriverUtilsService*>(pCall->pUserdata);

            // Execute the service implementation
            return pService->SetDbgLogSeverityLevel(pCall->pParameterData, pCall->parameterDataSize);
        };

        result = ddRpcServerRegisterFunction(hServer, &info);
    }

    // Register "SetDbgLogOriginationMask"
    if (result == DD_RESULT_SUCCESS)
    {
        DDRpcServerRegisterFunctionInfo info = {};
        info.serviceId                       = 0x24815012;
        info.id                              = 0x7;
        info.pName                           = "SetDbgLogOriginationMask";
        info.pDescription                    = "Set driver DbgLog's origination mask";
        info.pFuncUserdata                   = pService;
        info.pfnFuncCb                       = [](
            const DDRpcServerCallInfo* pCall) -> DD_RESULT
        {
            auto* pService = reinterpret_cast<IDriverUtilsService*>(pCall->pUserdata);

            // Execute the service implementation
            return pService->SetDbgLogOriginationMask(pCall->pParameterData, pCall->parameterDataSize);
        };

        result = ddRpcServerRegisterFunction(hServer, &info);
    }

    // Register "ModifyDbgLogOriginationMask"
    if (result == DD_RESULT_SUCCESS)
    {
        DDRpcServerRegisterFunctionInfo info = {};
        info.serviceId                       = 0x24815012;
        info.id                              = 0x8;
        info.pName                           = "ModifyDbgLogOriginationMask";
        info.pDescription                    = "Modify driver DbgLog's origination mask";
        info.pFuncUserdata                   = pService;
        info.pfnFuncCb                       = [](
            const DDRpcServerCallInfo* pCall) -> DD_RESULT
        {
            auto* pService = reinterpret_cast<IDriverUtilsService*>(pCall->pUserdata);

            // Execute the service implementation
            return pService->ModifyDbgLogOriginationMask(pCall->pParameterData, pCall->parameterDataSize);
        };

        result = ddRpcServerRegisterFunction(hServer, &info);
    }

    return result;
}

DD_RESULT RegisterService(
    DDRpcServer hServer,
    IDriverUtilsService* pService
)
{
    DDRpcServerRegisterServiceInfo info = {};
    info.id                             = 0x24815012;
    info.version.major                  = 1;
    info.version.minor                  = 3;
    info.version.patch                  = 0;
    info.pName                          = "DriverUtils";
    info.pDescription                   = "A utilities service for modifying the driver.";

    // Register the service
    DD_RESULT result = ddRpcServerRegisterService(hServer, &info);

    // Register individual functions
    if (result == DD_RESULT_SUCCESS)
    {
        result = RegisterFunctions(hServer, pService);

        if (result != DD_RESULT_SUCCESS)
        {
            // Unregister the service if registering functions fails
            ddRpcServerUnregisterService(hServer, info.id);
        }
    }

    return result;
}

void UnRegisterService(DDRpcServer hServer)
{
    DDRpcServerRegisterServiceInfo info = {};
    info.id                             = 0x24815012;
    info.version.major                  = 1;
    info.version.minor                  = 3;
    info.version.patch                  = 0;
    info.pName                          = "DriverUtils";
    info.pDescription                   = "A utilities service for modifying the driver.";

    // Unregister the service if registering functions fails
    ddRpcServerUnregisterService(hServer, info.id);
}

} // namespace DriverUtils
