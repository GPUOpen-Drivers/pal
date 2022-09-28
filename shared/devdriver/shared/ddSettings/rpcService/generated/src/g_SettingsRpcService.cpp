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

#include <g_SettingsRpcService.h>

namespace SettingsRpc
{

const DDRpcServerRegisterServiceInfo ISettingsRpcService::kServiceInfo = []() -> DDRpcServerRegisterServiceInfo {
    DDRpcServerRegisterServiceInfo info = {};
    info.id                             = 0x15375127;
    info.version.major                  = 1;
    info.version.minor                  = 1;
    info.version.patch                  = 0;
    info.pName                          = "SettingsRpc";
    info.pDescription                   = "A service that queries/modifies driver settings.";

    return info;
}();

static DD_RESULT RegisterFunctions(
    DDRpcServer hServer,
    ISettingsRpcService* pService)
{
    DD_RESULT result = DD_RESULT_SUCCESS;

    // Register "GetComponents"
    if (result == DD_RESULT_SUCCESS)
    {
        DDRpcServerRegisterFunctionInfo info = {};
        info.serviceId                       = 0x15375127;
        info.id                              = 0x1;
        info.pName                           = "GetComponents";
        info.pDescription                    = "Queries the settings components";
        info.pFuncUserdata                   = pService;
        info.pfnFuncCb                       = [](
            const DDRpcServerCallInfo* pCall) -> DD_RESULT
        {
            auto* pService = reinterpret_cast<ISettingsRpcService*>(pCall->pUserdata);

            // Execute the service implementation
            return pService->GetComponents(*pCall->pWriter);
        };

        result = ddRpcServerRegisterFunction(hServer, &info);
    }

    // Register "QueryComponentSettings"
    if (result == DD_RESULT_SUCCESS)
    {
        DDRpcServerRegisterFunctionInfo info = {};
        info.serviceId                       = 0x15375127;
        info.id                              = 0x2;
        info.pName                           = "QueryComponentSettings";
        info.pDescription                    = "Queries the settings for a component";
        info.pFuncUserdata                   = pService;
        info.pfnFuncCb                       = [](
            const DDRpcServerCallInfo* pCall) -> DD_RESULT
        {
            auto* pService = reinterpret_cast<ISettingsRpcService*>(pCall->pUserdata);

            // Execute the service implementation
            return pService->QueryComponentSettings(pCall->pParameterData, pCall->parameterDataSize, *pCall->pWriter);
        };

        result = ddRpcServerRegisterFunction(hServer, &info);
    }

    // Register "QueryCurrentValues"
    if (result == DD_RESULT_SUCCESS)
    {
        DDRpcServerRegisterFunctionInfo info = {};
        info.serviceId                       = 0x15375127;
        info.id                              = 0x3;
        info.pName                           = "QueryCurrentValues";
        info.pDescription                    = "Queries for the current settings values for a compent";
        info.pFuncUserdata                   = pService;
        info.pfnFuncCb                       = [](
            const DDRpcServerCallInfo* pCall) -> DD_RESULT
        {
            auto* pService = reinterpret_cast<ISettingsRpcService*>(pCall->pUserdata);

            // Execute the service implementation
            return pService->QueryCurrentValues(pCall->pParameterData, pCall->parameterDataSize, *pCall->pWriter);
        };

        result = ddRpcServerRegisterFunction(hServer, &info);
    }

    // Register "QuerySettingsDataHash"
    if (result == DD_RESULT_SUCCESS)
    {
        DDRpcServerRegisterFunctionInfo info = {};
        info.serviceId                       = 0x15375127;
        info.id                              = 0x4;
        info.pName                           = "QuerySettingsDataHash";
        info.pDescription                    = "Getts the setting data hash of the component";
        info.pFuncUserdata                   = pService;
        info.pfnFuncCb                       = [](
            const DDRpcServerCallInfo* pCall) -> DD_RESULT
        {
            auto* pService = reinterpret_cast<ISettingsRpcService*>(pCall->pUserdata);

            // Execute the service implementation
            return pService->QuerySettingsDataHash(pCall->pParameterData, pCall->parameterDataSize, *pCall->pWriter);
        };

        result = ddRpcServerRegisterFunction(hServer, &info);
    }

    // Register "SetData"
    if (result == DD_RESULT_SUCCESS)
    {
        DDRpcServerRegisterFunctionInfo info = {};
        info.serviceId                       = 0x15375127;
        info.id                              = 0x5;
        info.pName                           = "SetData";
        info.pDescription                    = "Sends a setting to the driver";
        info.pFuncUserdata                   = pService;
        info.pfnFuncCb                       = [](
            const DDRpcServerCallInfo* pCall) -> DD_RESULT
        {
            auto* pService = reinterpret_cast<ISettingsRpcService*>(pCall->pUserdata);

            // Execute the service implementation
            return pService->SetData(pCall->pParameterData, pCall->parameterDataSize);
        };

        result = ddRpcServerRegisterFunction(hServer, &info);
    }

    // Register "GetCurrentValues"
    if (result == DD_RESULT_SUCCESS)
    {
        DDRpcServerRegisterFunctionInfo info = {};
        info.serviceId                       = 0x15375127;
        info.id                              = 0x6;
        info.pName                           = "GetCurrentValues";
        info.pDescription                    = "Queries the current Settings values for all components.";
        info.pFuncUserdata                   = pService;
        info.pfnFuncCb                       = [](
            const DDRpcServerCallInfo* pCall) -> DD_RESULT
        {
            auto* pService = reinterpret_cast<ISettingsRpcService*>(pCall->pUserdata);

            // Execute the service implementation
            return pService->GetCurrentValues(*pCall->pWriter);
        };

        result = ddRpcServerRegisterFunction(hServer, &info);
    }

    // Register "SetValue"
    if (result == DD_RESULT_SUCCESS)
    {
        DDRpcServerRegisterFunctionInfo info = {};
        info.serviceId                       = 0x15375127;
        info.id                              = 0x7;
        info.pName                           = "SetValue";
        info.pDescription                    = "Set a setting's value.";
        info.pFuncUserdata                   = pService;
        info.pfnFuncCb                       = [](
            const DDRpcServerCallInfo* pCall) -> DD_RESULT
        {
            auto* pService = reinterpret_cast<ISettingsRpcService*>(pCall->pUserdata);

            // Execute the service implementation
            return pService->SetValue(pCall->pParameterData, pCall->parameterDataSize);
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
    // Register the service
    DD_RESULT result = ddRpcServerRegisterService(hServer, &ISettingsRpcService::kServiceInfo);

    // Register individual functions
    if (result == DD_RESULT_SUCCESS)
    {
        result = RegisterFunctions(hServer, pService);

        if (result != DD_RESULT_SUCCESS)
        {
            // Unregister the service if registering functions fails
            ddRpcServerUnregisterService(hServer, ISettingsRpcService::kServiceInfo.id);
        }
    }

    return result;
}

} // namespace SettingsRpc
