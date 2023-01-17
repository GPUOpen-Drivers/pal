/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2022-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
