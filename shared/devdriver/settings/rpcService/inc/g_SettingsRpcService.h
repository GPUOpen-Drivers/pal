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

namespace SettingsRpc
{

class ISettingsRpcService
{
public:
    virtual ~ISettingsRpcService() {}

    static const DDRpcServerRegisterServiceInfo kServiceInfo;

    // Queries the settings components
    virtual DD_RESULT GetComponents(
        const DDByteWriter& writer
    ) = 0;

    // Queries the settings for a component
    virtual DD_RESULT QueryComponentSettings(
        const void*         pParamBuffer,
        size_t              paramBufferSize,
        const DDByteWriter& writer
    ) = 0;

    // Queries for the current settings values for a compent
    virtual DD_RESULT QueryCurrentValues(
        const void*         pParamBuffer,
        size_t              paramBufferSize,
        const DDByteWriter& writer
    ) = 0;

    // Getts the setting data hash of the component
    virtual DD_RESULT QuerySettingsDataHash(
        const void*         pParamBuffer,
        size_t              paramBufferSize,
        const DDByteWriter& writer
    ) = 0;

    // Sends a setting to the driver
    virtual DD_RESULT SetData(
        const void* pParamBuffer,
        size_t      paramBufferSize
    ) = 0;

protected:
    ISettingsRpcService() {}
};

DD_RESULT RegisterService(DDRpcServer hServer, ISettingsRpcService* pService);

} // namespace SettingsRpc
