/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */

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

