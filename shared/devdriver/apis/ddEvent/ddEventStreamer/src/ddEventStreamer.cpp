/* Copyright (c) 2022 Advanced Micro Devices, Inc. All rights reserved. */

#include <ddEventStreamer.h>
#include <ddCommon.h>

#include <eventStreamer.h>

using namespace DevDriver;
using namespace Event;

/// Define DDEventStreamer as an alias for EventStreamer
DD_DEFINE_HANDLE(DDEventStreamer, EventStreamer*);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DDApiVersion ddEventStreamerQueryVersion()
{
    DDApiVersion version = {};

    version.major = DD_EVENT_STREAMER_API_MAJOR_VERSION;
    version.minor = DD_EVENT_STREAMER_API_MINOR_VERSION;
    version.patch = DD_EVENT_STREAMER_API_PATCH_VERSION;

    return version;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const char* ddEventStreamerQueryVersionString()
{
    return DD_EVENT_STREAMER_API_VERSION_STRING;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT ddEventStreamerCreate(
    const DDEventStreamerCreateInfo* pInfo,
    DDEventStreamer*                 phStreamer)
{
    DD_RESULT result = DD_RESULT_COMMON_INVALID_PARAMETER;

    if ((phStreamer                   != nullptr)                  &&
        (pInfo                        != nullptr)                  &&
        (pInfo->hConnection           != DD_API_INVALID_HANDLE)    &&
        (pInfo->clientId              != DD_API_INVALID_CLIENT_ID) &&
        (pInfo->providerId            != DD_API_INVALID_CLIENT_ID) &&
        (pInfo->onEventCb.pfnCallback != nullptr))
    {
        EventStreamer* pStreamer = DD_NEW(EventStreamer, Platform::GenericAllocCb)();
        if (pStreamer != nullptr)
        {
            result = pStreamer->BeginStreaming(*pInfo);

            if (result != DD_RESULT_SUCCESS)
            {
                DD_DELETE(pStreamer, Platform::GenericAllocCb);
            }
        }
        else
        {
            result = DD_RESULT_COMMON_OUT_OF_HEAP_MEMORY;
        }

        if (result == DD_RESULT_SUCCESS)
        {
            *phStreamer = ToHandle(pStreamer);
        }
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ddEventStreamerDestroy(
    DDEventStreamer hStreamer)
{
    if (hStreamer != nullptr)
    {
        EventStreamer* pStreamer = FromHandle(hStreamer);
        DD_DELETE(pStreamer, Platform::GenericAllocCb);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT ddEventStreamerSetEventCallback(
    DDEventStreamer                hStreamer,
    const DDEventStreamerCallback* pCallback)
{
    DD_RESULT result = DD_RESULT_COMMON_INVALID_PARAMETER;

    if ((hStreamer != DD_API_INVALID_HANDLE) &&
        (pCallback != nullptr))
    {
        EventStreamer* pStreamer = FromHandle(hStreamer);
        pStreamer->SetEventCallback(*pCallback);
        result = DD_RESULT_SUCCESS;
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool ddEventStreamerIsStreaming(
    DDEventStreamer hStreamer)
{
    bool isStreaming = false;

    if (hStreamer != DD_API_INVALID_HANDLE)
    {
        EventStreamer* pStreamer = FromHandle(hStreamer);
        isStreaming = pStreamer->IsStreaming();
    }

    return isStreaming;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT ddEventStreamerEndStreaming(
    DDEventStreamer hStreamer)
{
    DD_RESULT result = DD_RESULT_COMMON_INVALID_PARAMETER;

    if (hStreamer != DD_API_INVALID_HANDLE)
    {
        EventStreamer* pStreamer = FromHandle(hStreamer);
        result = pStreamer->EndStreaming();
    }

    return result;
}
