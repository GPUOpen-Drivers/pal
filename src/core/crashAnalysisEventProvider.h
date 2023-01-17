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

#pragma once

#include "palPlatform.h"
#include "palMutex.h"

#include "core/eventDefs.h"
#include "core/devDriverEventService.h"

#include "protocols/ddEventProvider.h"

namespace Pal
{
class Platform;

// =====================================================================================================================
// The CrashAnalysisEventProvider class is a class derived from DevDriver::EventProvider::BaseEventProvider
// that is responsible for logging markers as events in PAL.
class CrashAnalysisEventProvider final : public DevDriver::EventProtocol::BaseEventProvider
{
public:
    explicit CrashAnalysisEventProvider(Platform* pPlatform);
    ~CrashAnalysisEventProvider() override {}

    Result Init();
    void Destroy();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Event Log Functions
    // These functions will result in an event being sent through the DevDriver EventProtocol or to the event log file
    // if the provider and event are enabled.

    void LogCreateCrashAnalysisEvent(
        const CrashAnalysisExecutionMarker& eventData);

    // End of Event Log Functions
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // BaseEventProvider overrides

    static constexpr char ProviderName[] = "PalCrashAnalysisEventProvider";
    static constexpr DevDriver::EventProtocol::EventProviderId ProviderId = 0x50434145; // 'PCAE'

    DevDriver::EventProtocol::EventProviderId GetId() const override { return ProviderId; }

    const char* GetName() const override { return ProviderName; }
    const void* GetEventDescriptionData()     const override;
    uint32      GetEventDescriptionDataSize() const override;

    // End of BaseEventProvider overrides
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

private:
    bool ShouldLog() const;

    void LogEvent(
        PalEvent    eventId,
        const void* pEventData,
        size_t      eventDataSize);

    Platform*                  m_pPlatform;
    EventService               m_eventService;
    DevDriver::EventTimer      m_eventTimer;

    PAL_DISALLOW_COPY_AND_ASSIGN(CrashAnalysisEventProvider);
};

} // Pal
