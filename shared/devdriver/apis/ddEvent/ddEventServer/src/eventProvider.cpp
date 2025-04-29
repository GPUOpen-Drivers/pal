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

#include <eventProvider.h>
#include <eventServer.h>
#include <eventShared.h>

#include <ddCommon.h>

using namespace DevDriver;

namespace Event
{

/// Event provider flush frequency
///
/// This value is currently hardcoded in the implementation, but it may be exposed in later versions of the interface.
constexpr uint32_t kFlushFrequencyInMs = 100;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
EventProvider::EventProvider(
    const DDEventProviderCreateInfo& createInfo)
    : BaseEventProvider(Platform::GenericAllocCb, createInfo.numEvents, kFlushFrequencyInMs)
    , m_createInfo(createInfo)
    , m_pServer(FromHandle(createInfo.hServer))
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
EventProvider::~EventProvider()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT EventProvider::EmitWithHeader(
    uint32_t    eventId,
    size_t      headerSize,
    const void* pHeader,
    size_t      payloadSize,
    const void* pPayload)
{
    return EmitInternal(eventId, headerSize, pHeader, payloadSize, pPayload, false);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT EventProvider::TestEmit(
    uint32_t eventId)
{
    return EmitInternal(eventId, 0, nullptr, 0, nullptr, true);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void EventProvider::OnEnable()
{
    if (m_createInfo.stateChangeCb.pfnEnabled != nullptr)
    {
        m_createInfo.stateChangeCb.pfnEnabled(m_createInfo.stateChangeCb.pUserdata);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void EventProvider::OnDisable()
{
    if (m_createInfo.stateChangeCb.pfnDisabled != nullptr)
    {
        m_createInfo.stateChangeCb.pfnDisabled(m_createInfo.stateChangeCb.pUserdata);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DD_RESULT EventProvider::EmitInternal(
    uint32_t    eventId,
    size_t      headerSize,
    const void* pHeader,
    size_t      payloadSize,
    const void* pPayload,
    bool        isQuery)
{
    DD_RESULT result = DD_RESULT_DD_EVENT_EMIT_PROVIDER_DISABLED;

    if (IsProviderEnabled())
    {
        if (eventId < GetNumEvents())
        {
            const Result writeResult =
                isQuery ? QueryEventWriteStatus(eventId) :
                          WriteEventWithHeader(eventId, pHeader, headerSize, pPayload, payloadSize);
            if (writeResult == Result::Rejected)
            {
                // Manually translate rejected to event disabled
                result = DD_RESULT_DD_EVENT_EMIT_EVENT_DISABLED;
            }
            else
            {
                result = DevDriverToDDResult(writeResult);
            }
        }
        else
        {
            result = DD_RESULT_DD_EVENT_EMIT_INVALID_EVENT_ID;
        }
    }

    return result;
}

} // namespace Event
