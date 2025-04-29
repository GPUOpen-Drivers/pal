/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include <ddEventServer.h>

#include <eventServer.h>
#include <eventProvider.h>
#include <eventShared.h>

#include <ddCommon.h>

using namespace DevDriver;
using namespace Event;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DDApiVersion ddEventServerQueryVersion()
{
    DDApiVersion version = {};

    version.major = DD_EVENT_SERVER_API_MAJOR_VERSION;
    version.minor = DD_EVENT_SERVER_API_MINOR_VERSION;
    version.patch = DD_EVENT_SERVER_API_PATCH_VERSION;

    return version;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const char* ddEventServerQueryVersionString()
{
    return DD_EVENT_SERVER_API_VERSION_STRING;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT ddEventServerCreate(
    const DDEventServerCreateInfo* pInfo,
    DDEventServer*                 phServer)
{
    DD_RESULT result = DD_RESULT_COMMON_INVALID_PARAMETER;

    if ((pInfo != nullptr)                            &&
        (pInfo->hConnection != DD_API_INVALID_HANDLE) &&
        (phServer != nullptr))
    {
        EventServer* pServer = DD_NEW(EventServer, Platform::GenericAllocCb)(pInfo->hConnection);
        if (pServer != nullptr)
        {
            result = pServer->Initialize();

            if (result != DD_RESULT_SUCCESS)
            {
                DD_DELETE(pServer, Platform::GenericAllocCb);
            }
        }
        else
        {
            result = DD_RESULT_COMMON_OUT_OF_HEAP_MEMORY;
        }

        if (result == DD_RESULT_SUCCESS)
        {
            *phServer = ToHandle(pServer);
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ddEventServerDestroy(
    DDEventServer hServer)
{
    if (hServer != nullptr)
    {
        EventServer* pServer = FromHandle(hServer);

        DD_DELETE(pServer, Platform::GenericAllocCb);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT ddEventServerCreateProvider(
    const DDEventProviderCreateInfo* pInfo,
    DDEventProvider*                 phProvider)
{
    DD_RESULT result = DD_RESULT_COMMON_INVALID_PARAMETER;

    if ((pInfo != nullptr)                        &&
        (pInfo->hServer != DD_API_INVALID_HANDLE) &&
        (pInfo->id != 0)                          &&
        (pInfo->numEvents > 0)                    &&
        (phProvider != nullptr))
    {
        EventServer* pServer = FromHandle(pInfo->hServer);

        EventProvider* pProvider =
            DD_NEW(EventProvider, Platform::GenericAllocCb)(*pInfo);

        if (pProvider != nullptr)
        {
            result = pServer->RegisterProvider(pProvider);

            if (result != DD_RESULT_SUCCESS)
            {
                DD_DELETE(pProvider, Platform::GenericAllocCb);
            }
        }
        else
        {
            result = DD_RESULT_COMMON_OUT_OF_HEAP_MEMORY;
        }

        if (result == DD_RESULT_SUCCESS)
        {
            *phProvider = ToHandle(pProvider);
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ddEventServerDestroyProvider(
    DDEventProvider hProvider)
{
    if (hProvider != nullptr)
    {
        EventProvider* pProvider = FromHandle(hProvider);

        EventServer* pServer = pProvider->GetServer();
        pServer->UnregisterProvider(pProvider);

        DD_DELETE(pProvider, Platform::GenericAllocCb);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT ddEventServerEmitWithHeader(
    DDEventProvider hProvider,
    uint32_t        eventId,
    size_t          headerSize,
    const void*     pHeader,
    size_t          payloadSize,
    const void*     pPayload)
{
    DD_RESULT result = DD_RESULT_COMMON_INVALID_PARAMETER;

    if ((hProvider != DD_API_INVALID_HANDLE)        &&
        ValidateOptionalBuffer(pHeader, headerSize) &&
        ValidateOptionalBuffer(pPayload, payloadSize))
    {
        EventProvider* pProvider = FromHandle(hProvider);

        result = pProvider->EmitWithHeader(eventId, headerSize, pHeader, payloadSize, pPayload);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT ddEventServerEmit(
    DDEventProvider hProvider,
    uint32_t        eventId,
    size_t          payloadSize,
    const void*     pPayload)
{
    // Just call the main emit function, but pass a null header parameter
    return ddEventServerEmitWithHeader(
        hProvider,
        eventId,
        0,
        nullptr,
        payloadSize,
        pPayload);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT ddEventServerTestEmit(
    DDEventProvider hProvider,
    uint32_t        eventId)
{
    DD_RESULT result = DD_RESULT_COMMON_INVALID_PARAMETER;

    if (hProvider != DD_API_INVALID_HANDLE)
    {
        EventProvider* pProvider = FromHandle(hProvider);

        result = pProvider->TestEmit(eventId);
    }

    return result;
}

