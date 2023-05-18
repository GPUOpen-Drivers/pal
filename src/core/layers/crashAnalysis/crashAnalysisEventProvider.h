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
#include "core/eventDefs.h"
#include "protocols/ddEventProvider.h"

namespace Pal
{
class IPlatform;

// Forward declarations
namespace CrashAnalysis
{
struct MarkerState;
class EventCache;
}

using EventProviderId = DevDriver::EventProtocol::EventProviderId;

// =====================================================================================================================
// The CrashAnalysisEventProvider class is a class derived from DevDriver::EventProvider::BaseEventProvider
// that is responsible for logging markers as events in PAL.
class CrashAnalysisEventProvider final : public DevDriver::EventProtocol::BaseEventProvider
{
public:
    explicit CrashAnalysisEventProvider(IPlatform* pPlatform);
    ~CrashAnalysisEventProvider() override { }

    Result Init();
    void Destroy();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Event Log Functions
    // These functions will result in an event being sent through the DevDriver EventProtocol or to the event log file
    // if the provider and event are enabled.

    void LogExecutionMarkerBegin(
        uint32      cmdBufferId,
        uint32      markerValue,
        const char* pMarkerName,
        uint32      markerNameSize);

    void LogExecutionMarkerEnd(
        uint32 cmdBufferId,
        uint32 markerValue);

    void LogCrashDebugMarkerData(
        const CrashAnalysis::MarkerState* pMarkerHeader);

    void ReplayEventCache(
        CrashAnalysis::EventCache* pEventCache);

    // End of Event Log Functions
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // BaseEventProvider overrides

    EventProviderId GetId()   const override;
    const char*     GetName() const override { return "PalCrashAnalysisEventProvider"; }

    const void*     GetEventDescriptionData()     const override;
    uint32          GetEventDescriptionDataSize() const override;

    // End of BaseEventProvider overrides
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

private:
    bool ShouldLog() const;

    IPlatform*            m_pPlatform;
    DevDriver::EventTimer m_eventTimer;

    PAL_DISALLOW_COPY_AND_ASSIGN(CrashAnalysisEventProvider);
};

} // Pal
