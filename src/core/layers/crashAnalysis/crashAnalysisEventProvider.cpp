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

#include "crashAnalysis.h"
#include "crashAnalysisEventProvider.h"
#include "core/devDriverUtil.h"
#include "devDriverServer.h"
#include <dd_events/gpu_detective/umd_crash_analysis.h>

using namespace Util;
using namespace DevDriver;

namespace Pal
{

constexpr uint32     EventFlushFrequencyInMs = 10;
constexpr uint32     NumValidEvents          = 3; /// Number of distinct event types emitted by this provider
constexpr const char EventDescription[]      = "All available events are used as Radeon GPU Detective breadcrumbs.";
constexpr const char DefaultMarkerName[]     = "Unnamed Marker"; /// Default marker annotation if none is provided

// =====================================================================================================================
CrashAnalysisEventProvider::CrashAnalysisEventProvider(
    IPlatform* pPlatform)
    :
    DevDriver::EventProtocol::BaseEventProvider(
        { pPlatform, DevDriverAlloc, DevDriverFree },
        NumValidEvents,
        EventFlushFrequencyInMs
    ),
    m_pPlatform(pPlatform),
    m_eventTimer()
{
}

// =====================================================================================================================
EventProtocol::EventProviderId CrashAnalysisEventProvider::GetId() const
{
    return UmdCrashAnalysisEvents::ProviderId;
}

// =====================================================================================================================
const void* CrashAnalysisEventProvider::GetEventDescriptionData() const
{
    return EventDescription;
}

// =====================================================================================================================
uint32 CrashAnalysisEventProvider::GetEventDescriptionDataSize() const
{
    return sizeof(EventDescription);
}

// =====================================================================================================================
Result CrashAnalysisEventProvider::Init()
{
    Result result = Result::ErrorInitializationFailed;

    DevDriverServer* pServer = m_pPlatform->GetDevDriverServer();

    if (pServer != nullptr)
    {
        EventProtocol::EventServer* pEventServer = pServer->GetEventServer();

        if (pEventServer != nullptr)
        {
            DevDriver::Result ddRes = pEventServer->RegisterProvider(this);

            // The DevDriver Result code will be lost in a conversion to PAL,
            // so stringify and log the failing error code for posterity.
            PAL_ASSERT_MSG(ddRes == DevDriver::Result::Success,
                "Failed to register event provider with DevDriver: %s",
                ResultToString(ddRes));

            result = (ddRes == DevDriver::Result::Success)
                   ? Pal::Result::Success
                   : Pal::Result::ErrorInitializationFailed;
        }
    }

    return result;
}

// =====================================================================================================================
// Shuts down the CrashAnalysis event provider and disconnects from the DevDriver event server.
void CrashAnalysisEventProvider::Destroy()
{
    DevDriverServer* pServer = m_pPlatform->GetDevDriverServer();

    if (pServer != nullptr)
    {
        EventProtocol::EventServer* pEventServer = pServer->GetEventServer();

        if (pEventServer != nullptr)
        {
            pEventServer->UnregisterProvider(this);
        }
    }
}

// =====================================================================================================================
// Determines if relevant PAL and DevDriver infrastructure has been properly configured, such that we don't waste time
// constructing and emitting an event that cannot be sent.
bool CrashAnalysisEventProvider::ShouldLog() const
{
    return IsProviderEnabled();
}

// =====================================================================================================================
void CrashAnalysisEventProvider::LogExecutionMarkerBegin(
    uint32      cmdBufferId,
    uint32      markerValue,
    const char* pMarkerName,
    uint32      markerNameSize)
{
    using namespace UmdCrashAnalysisEvents;

    const uint32 eventId = static_cast<uint32>(EventId::ExecutionMarkerTop);

    if (ShouldLog())
    {
        // Give default annotation if none is present
        if ((pMarkerName == nullptr) || (markerNameSize == 0))
        {
            pMarkerName    = DefaultMarkerName;
            markerNameSize = StringLength(DefaultMarkerName);
        }

        // There is a fixed-size buffer in the event structure, so we need to truncate the name if it is too long.
        markerNameSize = Min(markerNameSize, static_cast<uint32>(sizeof(ExecutionMarkerTop::markerName)));

        ExecutionMarkerTop eventInfo = { };
        eventInfo.cmdBufferId        = cmdBufferId;
        eventInfo.marker             = markerValue;
        eventInfo.markerNameSize     = markerNameSize;
        memcpy(eventInfo.markerName, pMarkerName, markerNameSize);

        uint8 eventData[sizeof(ExecutionMarkerTop)];

        const uint32 eventSize = eventInfo.ToBuffer(eventData);

        WriteEvent(eventId, eventData, eventSize);
    }
}

// =====================================================================================================================
void CrashAnalysisEventProvider::LogExecutionMarkerEnd(
    uint32 cmdBufferId,
    uint32 markerValue)
{
    using namespace UmdCrashAnalysisEvents;

    const uint32 eventId = static_cast<uint32>(EventId::ExecutionMarkerBottom);

    if (ShouldLog())
    {
        ExecutionMarkerBottom eventInfo = { };
        eventInfo.cmdBufferId           = cmdBufferId;
        eventInfo.marker                = markerValue;

        uint8 eventData[sizeof(ExecutionMarkerBottom)];

        const uint32 eventSize = eventInfo.ToBuffer(eventData);

        WriteEvent(eventId, eventData, eventSize);
    }
}

// =====================================================================================================================
// This function deserializes an EventCache and replays the events contained within it.
void CrashAnalysisEventProvider::ReplayEventCache(
    CrashAnalysis::EventCache* pEventCache)
{
    using namespace UmdCrashAnalysisEvents;

    PAL_ALERT(pEventCache == nullptr);

    EventId     eventId;
    uint32      cmdBufferId;
    uint32      markerValue;
    const char* pMarkerName;
    uint32      markerNameSize;

    if (pEventCache != nullptr)
    {
        for (uint32 i = 0; i < pEventCache->Count(); i++)
        {
            Result result = pEventCache->GetEventAt(i,
                &eventId,
                &cmdBufferId,
                &markerValue,
                &pMarkerName,
                &markerNameSize);

            if (result != Result::Success)
            {
                PAL_ASSERT_ALWAYS();
                continue;
            }

            switch (eventId)
            {
            case EventId::ExecutionMarkerTop:
            {
                LogExecutionMarkerBegin(cmdBufferId, markerValue, pMarkerName, markerNameSize);
                break;
            }
            case EventId::ExecutionMarkerBottom:
            {
                LogExecutionMarkerEnd(cmdBufferId, markerValue);
                break;
            }
            case EventId::CmdBufferReset:
            {
                LogCmdBufferReset(cmdBufferId);
                break;
            }
            default:
            {
                PAL_ASSERT_ALWAYS();
                break;
            }
            }
        }
    }
}

// =====================================================================================================================
void CrashAnalysisEventProvider::LogCrashDebugMarkerData(
    const CrashAnalysis::MarkerState* pMarkerHeader)
{
    using namespace UmdCrashAnalysisEvents;

    const uint32 eventId = static_cast<uint32>(EventId::CrashDebugMarkerValue);

    PAL_ALERT(pMarkerHeader == nullptr);

    if (ShouldLog() && (pMarkerHeader != nullptr))
    {
        CrashDebugMarkerValue eventInfo = { };
        eventInfo.cmdBufferId           = pMarkerHeader->cmdBufferId;
        eventInfo.topMarkerValue        = pMarkerHeader->markerBegin;
        eventInfo.bottomMarkerValue     = pMarkerHeader->markerEnd;

        uint8 eventData[sizeof(CrashDebugMarkerValue)];
        const uint32 eventSize = eventInfo.ToBuffer(eventData);
        WriteEvent(eventId, &eventData, eventSize);
    }
}

// =====================================================================================================================
void CrashAnalysisEventProvider::LogCmdBufferReset(
    uint32 cmdBufferId)
{
    using namespace UmdCrashAnalysisEvents;

    const uint32 eventId = static_cast<uint32>(EventId::CmdBufferReset);

    if (ShouldLog())
    {
        CmdBufferReset eventInfo = { };
        eventInfo.cmdBufferId    = cmdBufferId;

        uint8 eventData[sizeof(CmdBufferReset)];
        const uint32 eventSize = eventInfo.ToBuffer(eventData);

        WriteEvent(eventId, eventData, eventSize);
    }
}

} // Pal
