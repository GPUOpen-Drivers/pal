/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */

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
