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

#pragma once

#include <ddRpcClient.h>

namespace SettingsRpc
{

class SettingsRpcClient
{
public:
    SettingsRpcClient();
    ~SettingsRpcClient();

    DD_RESULT Connect(const DDRpcClientCreateInfo& info);
    DD_RESULT IsServiceAvailable();
    DD_RESULT GetServiceInfo(DDApiVersion* pVersion);

    /// Queries the settings components
    DD_RESULT GetComponents(
        const DDByteWriter& writer
    );

    /// Queries the settings for a component
    DD_RESULT QueryComponentSettings(
        const void*         pParamBuffer,
        size_t              paramBufferSize,
        const DDByteWriter& writer
    );

    /// Queries for the current settings values for a compent
    DD_RESULT QueryCurrentValues(
        const void*         pParamBuffer,
        size_t              paramBufferSize,
        const DDByteWriter& writer
    );

    /// Getts the setting data hash of the component
    DD_RESULT QuerySettingsDataHash(
        const void*         pParamBuffer,
        size_t              paramBufferSize,
        const DDByteWriter& writer
    );

    /// Sends a setting to the driver
    DD_RESULT SetData(
        const void* pParamBuffer,
        size_t      paramBufferSize
    );

    /// Queries the current Settings values for all components.
    DD_RESULT GetCurrentValues(
        const DDByteWriter& writer
    );

    /// Set a setting's value.
    DD_RESULT SetValue(
        const void* pParamBuffer,
        size_t      paramBufferSize
    );

private:
    DDRpcClient m_hClient = DD_API_INVALID_HANDLE;
};

} // namespace SettingsRpc
