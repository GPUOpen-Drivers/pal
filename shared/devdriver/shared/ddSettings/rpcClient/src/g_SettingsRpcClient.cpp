/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include <g_SettingsRpcClient.h>
#include <ddCommon.h>

using namespace SettingsRpc;

SettingsRpcClient::SettingsRpcClient()
{
}

DD_RESULT SettingsRpcClient::Connect(const DDRpcClientCreateInfo& info)
{
    return ddRpcClientCreate(&info, &m_hClient);
}

DD_RESULT SettingsRpcClient::IsServiceAvailable()
{
    DDApiVersion version = {};
    DD_RESULT result = ddRpcClientGetServiceInfo(m_hClient, 0x15375127, &version);

    if (result == DD_RESULT_SUCCESS)
    {
        DDApiVersion serviceVersion = {};
        serviceVersion.major        = 1;
        serviceVersion.minor        = 1;
        serviceVersion.patch        = 0;

        result = ddIsVersionCompatible(version, serviceVersion) ?
            DD_RESULT_SUCCESS : DD_RESULT_COMMON_VERSION_MISMATCH;
    }

    return result;
}

DD_RESULT SettingsRpcClient::GetServiceInfo(DDApiVersion* pVersion)
{
    return ddRpcClientGetServiceInfo(m_hClient, 0x15375127, pVersion);
}

SettingsRpcClient::~SettingsRpcClient() { ddRpcClientDestroy(m_hClient); }

////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT SettingsRpcClient::GetComponents(
    const DDByteWriter& writer
)
{
    // No parameter
    const void* pParamBuffer     = nullptr;
    const size_t paramBufferSize = 0;

    const DDByteWriter* pResponseWriter = &writer;

    DDRpcClientCallInfo info  = {};
    info.service              = 0x15375127;
    info.serviceVersion.major = 1;
    info.serviceVersion.minor = 1;
    info.serviceVersion.patch = 0;
    info.function             = 0x1;
    info.pParamBuffer         = pParamBuffer;
    info.paramBufferSize      = paramBufferSize;
    info.pResponseWriter      = pResponseWriter;

    const DD_RESULT result = ddRpcClientCall(m_hClient, &info);

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT SettingsRpcClient::QueryComponentSettings(
    const void*         pParamBuffer,
    size_t              paramBufferSize,
    const DDByteWriter& writer
)
{
    const DDByteWriter* pResponseWriter = &writer;

    DDRpcClientCallInfo info  = {};
    info.service              = 0x15375127;
    info.serviceVersion.major = 1;
    info.serviceVersion.minor = 1;
    info.serviceVersion.patch = 0;
    info.function             = 0x2;
    info.pParamBuffer         = pParamBuffer;
    info.paramBufferSize      = paramBufferSize;
    info.pResponseWriter      = pResponseWriter;

    const DD_RESULT result = ddRpcClientCall(m_hClient, &info);

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT SettingsRpcClient::QueryCurrentValues(
    const void*         pParamBuffer,
    size_t              paramBufferSize,
    const DDByteWriter& writer
)
{
    const DDByteWriter* pResponseWriter = &writer;

    DDRpcClientCallInfo info  = {};
    info.service              = 0x15375127;
    info.serviceVersion.major = 1;
    info.serviceVersion.minor = 1;
    info.serviceVersion.patch = 0;
    info.function             = 0x3;
    info.pParamBuffer         = pParamBuffer;
    info.paramBufferSize      = paramBufferSize;
    info.pResponseWriter      = pResponseWriter;

    const DD_RESULT result = ddRpcClientCall(m_hClient, &info);

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT SettingsRpcClient::QuerySettingsDataHash(
    const void*         pParamBuffer,
    size_t              paramBufferSize,
    const DDByteWriter& writer
)
{
    const DDByteWriter* pResponseWriter = &writer;

    DDRpcClientCallInfo info  = {};
    info.service              = 0x15375127;
    info.serviceVersion.major = 1;
    info.serviceVersion.minor = 1;
    info.serviceVersion.patch = 0;
    info.function             = 0x4;
    info.pParamBuffer         = pParamBuffer;
    info.paramBufferSize      = paramBufferSize;
    info.pResponseWriter      = pResponseWriter;

    const DD_RESULT result = ddRpcClientCall(m_hClient, &info);

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT SettingsRpcClient::SetData(
    const void* pParamBuffer,
    size_t      paramBufferSize
)
{
    // No return
    EmptyByteWriter<DD_RESULT_DD_RPC_FUNC_RESPONSE_REJECTED> writer;
    const DDByteWriter* pResponseWriter = writer.Writer();

    DDRpcClientCallInfo info  = {};
    info.service              = 0x15375127;
    info.serviceVersion.major = 1;
    info.serviceVersion.minor = 1;
    info.serviceVersion.patch = 0;
    info.function             = 0x5;
    info.pParamBuffer         = pParamBuffer;
    info.paramBufferSize      = paramBufferSize;
    info.pResponseWriter      = pResponseWriter;

    const DD_RESULT result = ddRpcClientCall(m_hClient, &info);

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT SettingsRpcClient::GetCurrentValues(
    const DDByteWriter& writer
)
{
    // No parameter
    const void* pParamBuffer     = nullptr;
    const size_t paramBufferSize = 0;

    const DDByteWriter* pResponseWriter = &writer;

    DDRpcClientCallInfo info  = {};
    info.service              = 0x15375127;
    info.serviceVersion.major = 1;
    info.serviceVersion.minor = 1;
    info.serviceVersion.patch = 0;
    info.function             = 0x6;
    info.pParamBuffer         = pParamBuffer;
    info.paramBufferSize      = paramBufferSize;
    info.pResponseWriter      = pResponseWriter;

    const DD_RESULT result = ddRpcClientCall(m_hClient, &info);

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT SettingsRpcClient::SetValue(
    const void* pParamBuffer,
    size_t      paramBufferSize
)
{
    // No return
    EmptyByteWriter<DD_RESULT_DD_RPC_FUNC_RESPONSE_REJECTED> writer;
    const DDByteWriter* pResponseWriter = writer.Writer();

    DDRpcClientCallInfo info  = {};
    info.service              = 0x15375127;
    info.serviceVersion.major = 1;
    info.serviceVersion.minor = 1;
    info.serviceVersion.patch = 0;
    info.function             = 0x7;
    info.pParamBuffer         = pParamBuffer;
    info.paramBufferSize      = paramBufferSize;
    info.pResponseWriter      = pResponseWriter;

    const DD_RESULT result = ddRpcClientCall(m_hClient, &info);

    return result;
}
