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

#include "core/crashAnalysisEventProvider.h"

#include "core/devDriverUtil.h"

#include "devDriverServer.h"

using namespace Util;
using namespace DevDriver;

namespace Pal
{

constexpr uint32 EventFlushTimeoutInMs = 10;

constexpr const char EventDescription[] = "All available events are used as Radeon GPU Detective breadcrumbs.";

// Maximum allowed string length (in bytes) for a CrashAnalysisExecutionMarker marker string
constexpr uint32 MaxStringSize = 256;

// Maximum size of a single event (in bytes)
constexpr uint32 MaxEventSize = MaxStringSize + sizeof(CrashAnalysisExecutionMarker);

// =====================================================================================================================
CrashAnalysisEventProvider::CrashAnalysisEventProvider(
    Platform* pPlatform)
    :
    EventProtocol::BaseEventProvider(
        { pPlatform, DevDriverAlloc, DevDriverFree },
        static_cast<uint32>(PalEvent::Count),
        EventFlushTimeoutInMs
    ),
    m_pPlatform(pPlatform),
    m_eventService({ pPlatform, DevDriverAlloc, DevDriverFree }),
    m_eventTimer()
{
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
    Result result = Result::Success;

    // The event provider runs in a no-op mode when crash analysis mode is not enabled
    if (m_pPlatform->IsCrashAnalysisModeEnabled())
    {
        DevDriverServer* pServer = m_pPlatform->GetDevDriverServer();
        PAL_ASSERT(pServer != nullptr);

        IMsgChannel* pMsgChannel = pServer->GetMessageChannel();
        PAL_ASSERT(pMsgChannel != nullptr);

        EventProtocol::EventServer* pEventServer = pServer->GetEventServer();
        PAL_ASSERT(pEventServer != nullptr);

        result = (pMsgChannel->RegisterService(&m_eventService) == DevDriver::Result::Success)
                     ? Result::Success
                     : Result::ErrorUnknown;

        if (result == Result::Success)
        {
            result = (pEventServer->RegisterProvider(this) == DevDriver::Result::Success)
                        ? Result::Success
                        : Result::ErrorUnknown;

            if (result != Result::Success)
            {
                DD_UNHANDLED_RESULT(pMsgChannel->UnregisterService(&m_eventService));
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Shuts down the CrashAnalysis event provider and disconnects from the DevDriver event server.
void CrashAnalysisEventProvider::Destroy()
{
    // The event provider runs in a no-op mode when crash analysis mode is not enabled
    if (m_pPlatform->IsCrashAnalysisModeEnabled())
    {
        DevDriverServer* pServer = m_pPlatform->GetDevDriverServer();
        PAL_ASSERT(pServer != nullptr);

        IMsgChannel* pMsgChannel = pServer->GetMessageChannel();
        PAL_ASSERT(pMsgChannel != nullptr);

        EventProtocol::EventServer* pEventServer = pServer->GetEventServer();
        PAL_ASSERT(pEventServer != nullptr);

        DD_UNHANDLED_RESULT(pEventServer->UnregisterProvider(this));
        DD_UNHANDLED_RESULT(pMsgChannel->UnregisterService(&m_eventService));
    }
}

// =====================================================================================================================
// Determines if relevant PAL and DevDriver infrastructure has been properly configured, such that we don't waste time
// constructing and emitting an event that cannot be sent.
bool CrashAnalysisEventProvider::ShouldLog() const
{
    return (IsProviderEnabled() || m_pPlatform->IsCrashAnalysisModeEnabled());
}

// =====================================================================================================================
// Logs an event in response to a crash analysis marker insertion.
void CrashAnalysisEventProvider::LogCreateCrashAnalysisEvent(
    const CrashAnalysisExecutionMarker& eventData)
{
    // The toolside applications aren't currently configured to parse
    // event IDs - so, we set it to a NULL value until this changes.
    const uint32 eventId = 0;

    if (ShouldLog())
    {
        static_assert(sizeof(CrashAnalysisExecutionMarker) == 12 + sizeof(const char*),
            "Definition has changed unexpectedly");
        static_assert(offsetof(CrashAnalysisExecutionMarker, cmdBufferId) == 0,
            "invalid offset for 'cmdBufferId'");
        static_assert(offsetof(CrashAnalysisExecutionMarker, markerValue) == 4,
            "invalid offset for 'markerValue'");
        static_assert(offsetof(CrashAnalysisExecutionMarker, markerStringSize) == 8,
            "invalid offset for 'markerStringSize'");
        static_assert(offsetof(CrashAnalysisExecutionMarker, pMarkerString) == 12,
            "invalid offset for 'pMarkerString'");

        PAL_ASSERT(eventData.pMarkerString != nullptr);
        PAL_ASSERT(eventData.markerStringSize <= MaxStringSize);

        uint8 data[MaxEventSize];

        // Reinterpret byte array as uint32 for easier
        // data copying & addressing
        uint32* pDataU32 = (uint32*)data;

        // Copy struct metadata
        pDataU32[0] = eventData.cmdBufferId;
        pDataU32[1] = eventData.markerValue;
        pDataU32[2] = eventData.markerStringSize;

        // Copy struct payload
        memcpy(
            &pDataU32[3],
            eventData.pMarkerString,
            eventData.markerStringSize
        );

        size_t totalSize = eventData.markerStringSize + (3 * sizeof(uint32));

        WriteEvent(eventId, &data, totalSize);
    }
}

} // Pal

