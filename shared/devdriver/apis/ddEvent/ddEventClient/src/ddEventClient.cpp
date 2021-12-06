/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */

#include <ddEventClient.h>
#include <ddCommon.h>

#include <eventClient.h>

using namespace DevDriver;
using namespace Event;

/// Define DDEventClient as an alias for EventClient
DD_DEFINE_HANDLE(DDEventClient, EventClient*);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Helper function used to verify if an event provider visitor contains all necessary fields
bool ValidateVisitor(const DDEventProviderVisitor& visitor)
{
    return (visitor.pfnVisit != nullptr);
}

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
        EventClient* pClient = DD_NEW(EventClient, Platform::GenericAllocCb)(pInfo->hConnection, pInfo->dataCb);
        if (pClient != nullptr)
        {
            result = pClient->Connect(pInfo->clientId, pInfo->timeoutInMs);

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
DD_RESULT ddEventClientQueryProviders(
    DDEventClient                 hClient,
    const DDEventProviderVisitor* pVisitor)
{
    DD_RESULT result = DD_RESULT_COMMON_INVALID_PARAMETER;

    if ((hClient != DD_API_INVALID_HANDLE) &&
        (pVisitor != nullptr)              &&
        ValidateVisitor(*pVisitor))
    {
        EventClient* pClient = FromHandle(hClient);
        result = pClient->QueryProviders(pVisitor);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT ddEventClientConfigureProviders(
    DDEventClient              hClient,
    size_t                     numProviders,
    const DDEventProviderDesc* pProviders)
{
    DD_RESULT result = DD_RESULT_COMMON_INVALID_PARAMETER;

    if ((hClient != DD_API_INVALID_HANDLE) &&
        (numProviders > 0)                 &&
        (pProviders != nullptr))
    {
        EventClient* pClient = FromHandle(hClient);
        result = pClient->ConfigureProviders(numProviders, pProviders);
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
