/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "palFile.h"
#include "palGpuMemoryBindable.h"
#include "palJsonWriter.h"
#include "palMutex.h"
#include "palPlatform.h"

#include "core/eventDefs.h"

#include "protocols/ddEventServer.h"
#include "protocols/ddEventProvider.h"

#include "util/ddEventTimer.h"
#include "util/rmtTokens.h"

namespace Pal
{

class Device;
class GpuMemory;
class Platform;
class Queue;

// =====================================================================================================================
// The GpuMemoryEventProvider class is a class derived from DevDriver EventProvider that is be responsible for
// logging developer mode events in PAL.
class GpuMemoryEventProvider final : public DevDriver::EventProtocol::BaseEventProvider
{
public:
    explicit GpuMemoryEventProvider(Platform* pPlatform);
    ~GpuMemoryEventProvider() override {}

    Result Init();
    void Destroy();

    bool IsMemoryProfilingEnabled() const
    {
        return (IsProviderEnabled());
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Event Log Functions
    // These functions will result in an event being sent through the DevDriver EventProtocol or to the event log file
    // if the provider and event are enabled.

    void LogCreateGpuMemoryEvent(const GpuMemory* pGpuMemory);

    void LogDestroyGpuMemoryEvent(const GpuMemory* pGpuMemory);

    void LogGpuMemoryResourceBindEvent(const GpuMemoryResourceBindEventData& eventData);

    void LogGpuMemoryCpuMapEvent(const GpuMemory* pGpuMemory);

    void LogGpuMemoryCpuUnmapEvent(const GpuMemory* pGpuMemory);

    void LogGpuMemoryAddReferencesEvent(
        uint32              gpuMemRefCount,
        const GpuMemoryRef* pGpuMemoryRefs,
        IQueue*             pQueue,
        uint32              flags);

    void LogGpuMemoryRemoveReferencesEvent(
        uint32            gpuMemoryCount,
        IGpuMemory*const* ppGpuMemory,
        IQueue*           pQueue);

    void LogGpuMemoryResourceCreateEvent(const ResourceCreateEventData& eventData);

    void LogGpuMemoryResourceDestroyEvent(const ResourceDestroyEventData& eventData);

    void LogDebugNameEvent(const DebugNameEventData& eventData);

    void LogGpuMemoryMiscEvent(const MiscEventData& eventData);

    void LogGpuMemorySnapshotEvent(const GpuMemorySnapshotEventData& eventData);

    void LogResourceCorrelationEvent(const ResourceCorrelationEventData& eventData);

    void LogResourceUpdateEvent(const ResourceUpdateEventData& eventData);

    // End of Event Log Functions
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // BaseEventProvider overrides

    static constexpr DevDriver::EventProtocol::EventProviderId kProviderId = 0x50616C45; // 'PalE'
    DevDriver::EventProtocol::EventProviderId GetId() const override { return kProviderId; }

    const char* GetName() const override { return "PalGpuMemoryEventProvider"; }
    const void* GetEventDescriptionData()     const override;
    uint32      GetEventDescriptionDataSize() const override;

    virtual void OnEnable() override;

    // End of BaseEventProvider overrides
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

private:
    bool ShouldLog(PalEvent eventId) const;

    // Logs a PalEvent by translating it into one or more RMT Tokens and passing it into WriteTokenData
    void LogEvent(PalEvent eventId, const void* pEventData, size_t eventDataSize);

    // Hepler method for LogEvent
    void LogResourceCreateEvent(uint8 delta, const void* pEventData, size_t eventDataSize);

    // Write an RMT token to both the service and event protocol
    void WriteTokenData(const DevDriver::RMT_TOKEN_DATA& token)
    {
        WriteEvent(
            static_cast<uint32>(PalEvent::RmtToken),
            token.Data(),
            token.Size()
        );
    }

    Platform*                  m_pPlatform;
    DevDriver::EventTimer      m_eventTimer;
    DevDriver::Platform::Mutex m_providerLock;
    bool                       m_logRmtVersion;

    PAL_DISALLOW_COPY_AND_ASSIGN(GpuMemoryEventProvider);
};

} // Pal
