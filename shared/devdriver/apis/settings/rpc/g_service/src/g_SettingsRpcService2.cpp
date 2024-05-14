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

#include <g_SettingsRpcService2.h>

namespace SettingsRpc
{

static DD_RESULT RegisterFunctions(
    DDRpcServer hServer,
    ISettingsRpcService* pService)
{
    DD_RESULT result = DD_RESULT_SUCCESS;

    // Register "SendAllUserOverrides"
    if (result == DD_RESULT_SUCCESS)
    {
        DDRpcServerRegisterFunctionInfo info = {};
        info.serviceId                       = 0x15375127;
        info.id                              = 0x1;
        info.pName                           = "SendAllUserOverrides";
        info.pDescription                    = "Send user overrides of all components to the driver.";
        info.pFuncUserdata                   = pService;
        info.pfnFuncCb                       = [](
            const DDRpcServerCallInfo* pCall) -> DD_RESULT
        {
            auto* pService = reinterpret_cast<ISettingsRpcService*>(pCall->pUserdata);

            // Execute the service implementation
            return pService->SendAllUserOverrides(pCall->pParameterData, pCall->parameterDataSize);
        };

        result = ddRpcServerRegisterFunction(hServer, &info);
    }

    // Register "QueryAllCurrentValues"
    if (result == DD_RESULT_SUCCESS)
    {
        DDRpcServerRegisterFunctionInfo info = {};
        info.serviceId                       = 0x15375127;
        info.id                              = 0x2;
        info.pName                           = "QueryAllCurrentValues";
        info.pDescription                    = "Query current setting values of all components from the driver.";
        info.pFuncUserdata                   = pService;
        info.pfnFuncCb                       = [](
            const DDRpcServerCallInfo* pCall) -> DD_RESULT
        {
            auto* pService = reinterpret_cast<ISettingsRpcService*>(pCall->pUserdata);

            // Execute the service implementation
            return pService->QueryAllCurrentValues(*pCall->pWriter);
        };

        result = ddRpcServerRegisterFunction(hServer, &info);
    }

    // Register "GetUnsupportedExperiments"
    if (result == DD_RESULT_SUCCESS)
    {
        DDRpcServerRegisterFunctionInfo info = {};
        info.serviceId                       = 0x15375127;
        info.id                              = 0x3;
        info.pName                           = "GetUnsupportedExperiments";
        info.pDescription                    = "Query currently unsupported experiments for all components from the driver.";
        info.pFuncUserdata                   = pService;
        info.pfnFuncCb                       = [](
            const DDRpcServerCallInfo* pCall) -> DD_RESULT
        {
            auto* pService = reinterpret_cast<ISettingsRpcService*>(pCall->pUserdata);

            // Execute the service implementation
            return pService->GetUnsupportedExperiments(*pCall->pWriter);
        };

        result = ddRpcServerRegisterFunction(hServer, &info);
    }

    return result;
}

DD_RESULT RegisterService(
    DDRpcServer hServer,
    ISettingsRpcService* pService
)
{
    DDRpcServerRegisterServiceInfo info = {};
    info.id                             = 0x15375127;
    info.version.major                  = 2;
    info.version.minor                  = 1;
    info.version.patch                  = 0;
    info.pName                          = "SettingsRpc";
    info.pDescription                   = "A service that queries/modifies driver settings.";

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
    info.id                             = 0x15375127;
    info.version.major                  = 2;
    info.version.minor                  = 1;
    info.version.patch                  = 0;
    info.pName                          = "SettingsRpc";
    info.pDescription                   = "A service that queries/modifies driver settings.";

    // Unregister the service if registering functions fails
    ddRpcServerUnregisterService(hServer, info.id);
}

} // namespace SettingsRpc
