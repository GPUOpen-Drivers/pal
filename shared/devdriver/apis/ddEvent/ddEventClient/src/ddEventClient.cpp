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

#include <ddEventClient.h>
#include <ddCommon.h>
#include <dd_timeout_constants.h>

#include <eventClient.h>

using namespace DevDriver;
using namespace Event;

/// Define DDEventClient as an alias for EventClient
DD_DEFINE_HANDLE(DDEventClient, EventClient*);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DDApiVersion ddEventClientQueryVersion()
{
    DDApiVersion version = {};

    version.major = DD_EVENT_CLIENT_API_MAJOR_VERSION;
    version.minor = DD_EVENT_CLIENT_API_MINOR_VERSION;
    version.patch = DD_EVENT_CLIENT_API_PATCH_VERSION;

    return version;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const char* ddEventClientQueryVersionString()
{
    return DD_EVENT_CLIENT_API_VERSION_STRING;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT ddEventClientCreate(
    const DDEventClientCreateInfo* pInfo,
    DDEventClient*                 phClient)
{
    DD_RESULT result = DD_RESULT_COMMON_INVALID_PARAMETER;

    if ((pInfo != nullptr)                                 &&
        (pInfo->hConnection != DD_API_INVALID_HANDLE)      &&
        (pInfo->clientId    != DD_API_INVALID_CLIENT_ID)   &&
        (pInfo->dataCb.pfnCallback != nullptr)             &&
        (phClient != nullptr))
    {
        TimeoutConstants timeouts         = {};
        timeouts.retryTimeoutInMs         = pInfo->retryTimeoutInMs;
        timeouts.communicationTimeoutInMs = pInfo->communicationTimeoutInMs;
        timeouts.connectionTimeoutInMs    = pInfo->connectionTimeoutInMs;

        TimeoutConstantsInitialize(&timeouts);

        EventClient* pClient = DD_NEW(EventClient, Platform::GenericAllocCb)(pInfo->hConnection, pInfo->dataCb);
        if (pClient != nullptr)
        {
            result = pClient->Connect(pInfo->clientId);

            if (result == DD_RESULT_SUCCESS)
            {
                result = pClient->SubscribeToProvider(pInfo->providerId);
            }

            if (result != DD_RESULT_SUCCESS)
            {
                DD_DELETE(pClient, Platform::GenericAllocCb);
            }
        }
        else
        {
            result = DD_RESULT_COMMON_OUT_OF_HEAP_MEMORY;
        }

        if (result == DD_RESULT_SUCCESS)
        {
            *phClient = ToHandle(pClient);
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ddEventClientDestroy(
    DDEventClient hClient)
{
    if (hClient != nullptr)
    {
        EventClient* pClient = FromHandle(hClient);

        DD_DELETE(pClient, Platform::GenericAllocCb);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT ddEventClientReadEventData(
    DDEventClient hClient,
    uint32_t      timeoutInMs)
{
    DD_RESULT result = DD_RESULT_COMMON_INVALID_PARAMETER;

    if (hClient != DD_API_INVALID_HANDLE)
    {
        EventClient* pClient = FromHandle(hClient);
        result = pClient->ReadEventData(timeoutInMs);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT ddEventClientEnableProviders(
    DDEventClient   hClient,
    size_t          numProviderIds,
    const uint32_t* pProviderIds)
{
    DD_RESULT result = DD_RESULT_COMMON_INVALID_PARAMETER;

    if ((hClient != DD_API_INVALID_HANDLE) &&
        (numProviderIds > 0)               &&
        (pProviderIds != nullptr))
    {
        EventClient* pClient = FromHandle(hClient);
        result = pClient->EnableProviders(numProviderIds, pProviderIds);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT ddEventClientDisableProviders(
    DDEventClient   hClient,
    size_t          numProviderIds,
    const uint32_t* pProviderIds)
{
    DD_RESULT result = DD_RESULT_COMMON_INVALID_PARAMETER;

    if ((hClient != DD_API_INVALID_HANDLE) &&
        (numProviderIds > 0)               &&
        (pProviderIds != nullptr))
    {
        EventClient* pClient = FromHandle(hClient);
        result = pClient->DisableProviders(numProviderIds, pProviderIds);
    }

    return result;
}
