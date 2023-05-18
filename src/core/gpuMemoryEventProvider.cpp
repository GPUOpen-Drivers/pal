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

#include "core/gpuMemoryEventProvider.h"
#include "core/queue.h"
#include "core/platform.h"
#include "core/gpuMemory.h"
#include "core/devDriverUtil.h"

#include "palSysUtil.h"
#include "devDriverServer.h"

#include "core/devDriverEventServiceConv.h"
#include "core/eventDefs.h"
#include "core/gpuMemory.h"

#include "palSysUtil.h"

#include "util/rmtFileFormat.h"
#include "util/rmtResourceDescriptions.h"
#include "util/rmtTokens.h"

using namespace Util;
using namespace DevDriver;

namespace Pal
{

constexpr uint32 kEventFlushTimeoutInMs = 10;

constexpr const char kEventDescription[] = "All available events are RmtTokens directly embedded.";

const void* GpuMemoryEventProvider::GetEventDescriptionData() const
{
    return kEventDescription;
}

uint32 GpuMemoryEventProvider::GetEventDescriptionDataSize() const
{
    return sizeof(kEventDescription);
}

GpuMemoryEventProvider::GpuMemoryEventProvider(Platform* pPlatform)
        :
        DevDriver::EventProtocol::BaseEventProvider(
            { pPlatform, DevDriverAlloc, DevDriverFree },
            static_cast<uint32>(PalEvent::Count),
            kEventFlushTimeoutInMs
        ),
        m_pPlatform(pPlatform),
        m_logRmtVersion(false)
        {}

// =====================================================================================================================
Result GpuMemoryEventProvider::Init()
{
    Result result = Result::Success;

    // The event provider runs in a no-op mode when developer mode is not enabled
    if (m_pPlatform->IsDeveloperModeEnabled())
    {
        DevDriverServer* pServer = m_pPlatform->GetDevDriverServer();
        PAL_ASSERT(pServer != nullptr);

        IMsgChannel* pMsgChannel = pServer->GetMessageChannel();
        PAL_ASSERT(pMsgChannel != nullptr);

        EventProtocol::EventServer* pEventServer = pServer->GetEventServer();
        PAL_ASSERT(pEventServer != nullptr);

        result = (pEventServer->RegisterProvider(this) == DevDriver::Result::Success) ? Result::Success
                                                                                      : Result::ErrorUnknown;
    }

    return result;
}

// =====================================================================================================================
void GpuMemoryEventProvider::Destroy()
{
    // The event provider runs in a no-op mode when developer mode is not enabled
    if (m_pPlatform->IsDeveloperModeEnabled())
    {
        DevDriverServer* pServer = m_pPlatform->GetDevDriverServer();
        PAL_ASSERT(pServer != nullptr);

        IMsgChannel* pMsgChannel = pServer->GetMessageChannel();
        PAL_ASSERT(pMsgChannel != nullptr);

        EventProtocol::EventServer* pEventServer = pServer->GetEventServer();
        PAL_ASSERT(pEventServer != nullptr);

        DD_UNHANDLED_RESULT(pEventServer->UnregisterProvider(this));
    }
}

// =====================================================================================================================
// Performs required actions in response to this event provider being enabled by a tool.
void GpuMemoryEventProvider::OnEnable()
{
    DevDriver::Platform::LockGuard<DevDriver::Platform::Mutex> providerLock(m_providerLock);

    m_logRmtVersion = true;
}

// =====================================================================================================================
// Determines if the event would be written to either the EventServer or to the log file, used to determine if a log
// event call should bother constructing the log event data structure.
bool GpuMemoryEventProvider::ShouldLog(
    PalEvent eventId
    ) const
{
    return QueryEventWriteStatus(static_cast<uint32>(eventId)) == DevDriver::Result::Success;
}

// =====================================================================================================================
// Logs an event on creation of a GPU Memory allocation (physical or virtual).
void GpuMemoryEventProvider::LogCreateGpuMemoryEvent(
    const GpuMemory* pGpuMemory)
{
    // We only want to log new allocations
    if ((pGpuMemory != nullptr) && (pGpuMemory->IsGpuVaPreReserved() == false))
    {
        static constexpr PalEvent eventId = PalEvent::CreateGpuMemory;
        if (ShouldLog(eventId))
        {
            const GpuMemoryDesc desc = pGpuMemory->Desc();
            CreateGpuMemoryData data = {};
            data.handle              = reinterpret_cast<GpuMemHandle>(pGpuMemory);
            data.size                = desc.size;
            data.alignment           = desc.alignment;
            data.heapCount           = desc.heapCount;
            for (uint32 i = 0; i < data.heapCount; i++)
            {
                data.heaps[i]        = desc.heaps[i];
            }
            data.isVirtual           = desc.flags.isVirtual;
            data.isInternal          = pGpuMemory->IsClient();
            data.isExternalShared    = desc.flags.isExternal;
            data.gpuVirtualAddr      = desc.gpuVirtAddr;

            LogEvent(eventId, &data, sizeof(data));
        }
    }
}

// =====================================================================================================================
// Logs an event when a GPU Memory allocation (physical or virtual) is destroyed.
void GpuMemoryEventProvider::LogDestroyGpuMemoryEvent(
    const GpuMemory* pGpuMemory)
{
    static constexpr PalEvent eventId = PalEvent::DestroyGpuMemory;
    if (ShouldLog(eventId))
    {
        DestroyGpuMemoryData data = {};
        data.handle               = reinterpret_cast<GpuMemHandle>(pGpuMemory);
        data.gpuVirtualAddr       = pGpuMemory->Desc().gpuVirtAddr;

        LogEvent(eventId, &data, sizeof(data));
    }
}

// =====================================================================================================================
// Logs an event when a resource has GPU memory bound to it.
void GpuMemoryEventProvider::LogGpuMemoryResourceBindEvent(
    const GpuMemoryResourceBindEventData& eventData)
{
    static constexpr PalEvent eventId = PalEvent::GpuMemoryResourceBind;
    if (ShouldLog(eventId))
    {
        PAL_ASSERT(eventData.pObj != nullptr);

        GpuMemoryResourceBindData data = {};
        data.handle                    = reinterpret_cast<GpuMemHandle>(eventData.pGpuMemory);
        data.gpuVirtualAddr            = (eventData.pGpuMemory != nullptr) ? eventData.pGpuMemory->Desc().gpuVirtAddr : 0;
        data.resourceHandle            = reinterpret_cast<ResourceHandle>(eventData.pObj);
        data.requiredSize              = eventData.requiredGpuMemSize;
        data.offset                    = eventData.offset;
        data.isSystemMemory            = eventData.isSystemMemory;

        LogEvent(eventId, &data, sizeof(data));
    }
}

// =====================================================================================================================
// Logs an event when a GPU memory allocation is mapped for CPU access.
void GpuMemoryEventProvider::LogGpuMemoryCpuMapEvent(
    const GpuMemory* pGpuMemory)
{
    static constexpr PalEvent eventId = PalEvent::GpuMemoryCpuMap;
    if (ShouldLog(eventId))
    {
        GpuMemoryCpuMapData data = {};
        data.handle              = reinterpret_cast<GpuMemHandle>(pGpuMemory);
        data.gpuVirtualAddr      = pGpuMemory->Desc().gpuVirtAddr;

        LogEvent(eventId, &data, sizeof(data));
    }
}

// =====================================================================================================================
// Logs an event when a GPU memory allocation is unmapped for CPU access.
void GpuMemoryEventProvider::LogGpuMemoryCpuUnmapEvent(
    const GpuMemory* pGpuMemory)
{
    static constexpr PalEvent eventId = PalEvent::GpuMemoryCpuUnmap;
    if (ShouldLog(eventId))
    {
        GpuMemoryCpuUnmapData data = {};
        data.handle                = reinterpret_cast<GpuMemHandle>(pGpuMemory);
        data.gpuVirtualAddr        = pGpuMemory->Desc().gpuVirtAddr;

        LogEvent(eventId, &data, sizeof(data));
    }
}

// =====================================================================================================================
// Logs an event when GPU memory allocations are added to a per-device or per-queue reference list. The flags field is
// a GpuMemoryRefFlags flags type.
// NOTE: It is expected that pQueue will always be null for WDDM.
void GpuMemoryEventProvider::LogGpuMemoryAddReferencesEvent(
    uint32              gpuMemRefCount,
    const GpuMemoryRef* pGpuMemoryRefs,
    IQueue*             pQueue,
    uint32              flags)
{
    static constexpr PalEvent eventId = PalEvent::GpuMemoryAddReference;
    if (ShouldLog(eventId))
    {
        for (uint32 i=0; i < gpuMemRefCount; i++)
        {
            GpuMemoryAddReferenceData data = {};
            data.handle                    = reinterpret_cast<GpuMemHandle>(pGpuMemoryRefs[i].pGpuMemory);
            data.gpuVirtualAddr            = pGpuMemoryRefs[i].pGpuMemory->Desc().gpuVirtAddr;
            data.queueHandle               = reinterpret_cast<QueueHandle>(pQueue);
            data.flags                     = flags;

            LogEvent(eventId, &data, sizeof(data));
        }
    }
}

// =====================================================================================================================
// Logs an event when GPU memory allocations are removed from a per-device or per-queue reference list.
// NOTE: It is expected that pQueue will always be null for WDDM.
void GpuMemoryEventProvider::LogGpuMemoryRemoveReferencesEvent(
    uint32            gpuMemoryCount,
    IGpuMemory*const* ppGpuMemory,
    IQueue*           pQueue)
{
    static constexpr PalEvent eventId = PalEvent::GpuMemoryRemoveReference;
    if (ShouldLog(eventId))
    {
        for (uint32 i = 0; i < gpuMemoryCount; i++)
        {
            GpuMemoryRemoveReferenceData data = {};
            data.handle                       = reinterpret_cast<GpuMemHandle>(ppGpuMemory[i]);
            data.gpuVirtualAddr               = ppGpuMemory[i]->Desc().gpuVirtAddr;
            data.queueHandle                  = reinterpret_cast<QueueHandle>(pQueue);

            LogEvent(eventId, &data, sizeof(data));
        }
    }
}

// =====================================================================================================================
// Logs an event when a resource that requires GPU memory is created.  See the ResourceType enum for the list of
// resources this applies to.
void GpuMemoryEventProvider::LogGpuMemoryResourceCreateEvent(
    const ResourceCreateEventData& eventData)
{
    static constexpr PalEvent eventId = PalEvent::GpuMemoryResourceCreate;

    if (ShouldLog(eventId))
    {
        PAL_ASSERT(eventData.pObj != nullptr);

        GpuMemoryResourceCreateData data = {};
        data.handle                      = reinterpret_cast<ResourceHandle>(eventData.pObj);
        data.type                        = eventData.type;
        data.descriptionSize             = eventData.resourceDescSize;
        data.pDescription                = eventData.pResourceDescData;

        LogEvent(eventId, &data, sizeof(data));
    }
}

// =====================================================================================================================
// Logs an event when a resource that requires GPU memory is destroyed.  See the ResourceType enum for the list of
// resources this applies to.
void GpuMemoryEventProvider::LogGpuMemoryResourceDestroyEvent(
    const ResourceDestroyEventData& eventData)
{
    static constexpr PalEvent eventId = PalEvent::GpuMemoryResourceDestroy;

    if (ShouldLog(eventId))
    {
        PAL_ASSERT(eventData.pObj != nullptr);

        GpuMemoryResourceDestroyData data = {};
        data.handle                       = reinterpret_cast<ResourceHandle>(eventData.pObj);

        LogEvent(eventId, &data, sizeof(data));
    }
}

// =====================================================================================================================
// Logs an event capturing the assignment of an app-specified name for an object.
void GpuMemoryEventProvider::LogDebugNameEvent(
    const DebugNameEventData& eventData)
{
    static constexpr PalEvent eventId = PalEvent::DebugName;

    if (ShouldLog(eventId))
    {
        PAL_ASSERT(eventData.pObj != nullptr);

        DebugNameData data = {};
        data.handle        = reinterpret_cast<ResourceHandle>(eventData.pObj);
        data.pDebugName    = eventData.pDebugName;
        data.nameSize      = static_cast<uint32>(strlen(eventData.pDebugName));

        LogEvent(eventId, &data, sizeof(data));
    }
}

// =====================================================================================================================
// Logs a miscellaneous event that requires no additional data.  See MiscEventType for the list of miscellaneous events.
void GpuMemoryEventProvider::LogGpuMemoryMiscEvent(
    const MiscEventData& eventData)
{
    static constexpr PalEvent eventId = PalEvent::GpuMemoryMisc;

    if (ShouldLog(eventId))
    {
        GpuMemoryMiscData data = {};
        data.type              = eventData.eventType;
        data.engine            = eventData.engine;

        LogEvent(eventId, &data, sizeof(data));
    }
}

// =====================================================================================================================
// Logs an event when an application/driver wants to insert a snapshot marker into the event data.  A snapshot is a
// named point in time that can give context to the surrounding event data.
void GpuMemoryEventProvider::LogGpuMemorySnapshotEvent(
    const GpuMemorySnapshotEventData& eventData)
{
    static constexpr PalEvent eventId = PalEvent::GpuMemorySnapshot;

    if (ShouldLog(eventId))
    {
        GpuMemorySnapshotData data = {};
        data.pSnapshotName         = eventData.pSnapshotName;

        LogEvent(eventId, &data, sizeof(data));
    }
}

// =====================================================================================================================
// Logs an event when a driver wants to correlate internal driver information with the equivalent resource ID.
// Allows the client to correlate PAL resources with arbitrary data, such as data provided by the
// driver, runtime, or application.
void GpuMemoryEventProvider::LogResourceCorrelationEvent(
    const ResourceCorrelationEventData& eventData)
{
    static constexpr PalEvent eventId = PalEvent::ResourceCorrelation;

    if (ShouldLog(eventId))
    {
        ResourceCorrelationData data = {};
        data.handle       = reinterpret_cast<ResourceHandle>(eventData.pObj);
        data.driverHandle = reinterpret_cast<ResourceHandle>(eventData.pDriverPrivate);

        LogEvent(eventId, &data, sizeof(data));
    }
}

// =====================================================================================================================
void GpuMemoryEventProvider::LogResourceUpdateEvent(
    const ResourceUpdateEventData& eventData)
{
    static constexpr PalEvent eventId = PalEvent::ResourceInfoUpdate;

    if (ShouldLog(eventId))
    {
        PAL_ASSERT(eventData.pObj != nullptr);

        ResourceUpdateInfoData data = {};
        data.handle                 = reinterpret_cast<ResourceHandle>(eventData.pObj);
        data.subresourceId          = eventData.subresourceId;
        data.type                   = eventData.type;
        data.before                 = eventData.beforeUsageFlags;
        data.after                  = eventData.afterUsageFlags;

        LogEvent(eventId, &data, sizeof(data));
    }
}

// =====================================================================================================================
void GpuMemoryEventProvider::LogEvent(
    PalEvent    eventId,
    const void* pEventData,
    size_t      eventDataSize)
{
    static_assert(static_cast<uint32>(PalEvent::Count) == 17, "Write support for new event!");

    if (ShouldLog(eventId))
    {
        // The RMT format requires that certain tokens strictly follow each other (e.g. resource create + description),
        // so we need to lock to ensure another event isn't inserted into the stream while writing dependent tokens.
        DevDriver::Platform::LockGuard<DevDriver::Platform::Mutex> providerLock(m_providerLock);

        // The first time we have something to log, we need to log the RmtVersion first
        if (m_logRmtVersion)
        {
            if (ShouldLog(PalEvent::RmtVersion))
            {
                // If RMT logging is enabled, the first token we emit should be the RmtVersion event
                static const RmtDataVersion kRmtVersionEvent = {
                    RMT_FILE_DATA_CHUNK_MAJOR_VERSION,
                    RMT_FILE_DATA_CHUNK_MINOR_VERSION };

                WriteEvent(static_cast<uint32>(PalEvent::RmtVersion), &kRmtVersionEvent, sizeof(RmtDataVersion));
                m_logRmtVersion = false;
            }
        }

        const EventTimestamp timestamp = m_eventTimer.CreateTimestamp();
        uint8 delta = 0;

        if (timestamp.type == EventTimestampType::Full)
        {
            RMT_MSG_TIMESTAMP tsToken(timestamp.full.timestamp, timestamp.full.frequency);
            WriteTokenData(tsToken);
        }
        else if (timestamp.type == EventTimestampType::LargeDelta)
        {
            RMT_MSG_TIME_DELTA tdToken(timestamp.largeDelta.delta, timestamp.largeDelta.numBytes);
            WriteTokenData(tdToken);
        }
        else
        {
            delta = timestamp.smallDelta.delta;
        }

        switch (eventId)
        {
            case PalEvent::ResourceCorrelation:
            {
                const ResourceCorrelationData* pData = reinterpret_cast<const ResourceCorrelationData*>(pEventData);

                const uint32 handle       = LowPart(pData->handle);
                const uint32 driverHandle = LowPart(pData->driverHandle);

                RMT_MSG_USERDATA_RSRC_CORRELATION eventToken(delta, handle, driverHandle);
                WriteTokenData(eventToken);
                break;
            }
            case PalEvent::Count:
            case PalEvent::Invalid:
            {
                PAL_ASSERT_ALWAYS();
                break;
            }
            case PalEvent::RmtToken:
            case PalEvent::RmtVersion:
            {
                // RmtToken and RmtVersion should not be logged through this function
                PAL_ASSERT_ALWAYS();
                break;
            }
            case PalEvent::CreateGpuMemory:
            {
                PAL_ASSERT(sizeof(CreateGpuMemoryData) == eventDataSize);

                const CreateGpuMemoryData* pData = reinterpret_cast<const CreateGpuMemoryData*>(pEventData);

                static_assert((GpuHeapCount >= 4),
                    "We store 4 heaps in the RMT_MSG_VIRTUAL_ALLOCATE message. Ensure we're not out of bounds.");

                RMT_MSG_VIRTUAL_ALLOCATE eventToken(
                    delta,
                    pData->size,
                    pData->isInternal ? RMT_OWNER_CLIENT_DRIVER : RMT_OWNER_APP, // For now we only distinguish between driver
                                                                                 // app ownership
                    pData->gpuVirtualAddr,
                    PalToRmtHeapType(pData->heaps[0]),
                    PalToRmtHeapType(pData->heaps[1]),
                    PalToRmtHeapType(pData->heaps[2]),
                    PalToRmtHeapType(pData->heaps[3]),
                    static_cast<DevDriver::uint8>(pData->heapCount),
                    pData->isExternalShared);

                WriteTokenData(eventToken);

                break;
            }
            case PalEvent::DestroyGpuMemory:
            {
                PAL_ASSERT(sizeof(DestroyGpuMemoryData) == eventDataSize);

                const DestroyGpuMemoryData* pData = reinterpret_cast<const DestroyGpuMemoryData*>(pEventData);

                RMT_MSG_FREE_VIRTUAL eventToken(delta, pData->gpuVirtualAddr);

                WriteTokenData(eventToken);

                break;
            }
            case PalEvent::GpuMemoryResourceCreate:
            {
                LogResourceCreateEvent(delta, pEventData, eventDataSize);
                break;
            }
            case PalEvent::GpuMemoryResourceDestroy:
            {
                PAL_ASSERT(sizeof(GpuMemoryResourceDestroyData) == eventDataSize);
                const GpuMemoryResourceDestroyData* pData =
                    reinterpret_cast<const GpuMemoryResourceDestroyData*>(pEventData);

                RMT_MSG_RESOURCE_DESTROY eventToken(delta, LowPart(pData->handle));

                WriteTokenData(eventToken);

                break;
            }
            case PalEvent::GpuMemoryMisc:
            {
                PAL_ASSERT(sizeof(GpuMemoryMiscData) == eventDataSize);
                const GpuMemoryMiscData* pData = reinterpret_cast<const GpuMemoryMiscData*>(pEventData);

                RMT_MSG_MISC eventToken(delta, PalToRmtMiscEventType(pData->type));

                WriteTokenData(eventToken);
                break;
            }
            case PalEvent::GpuMemorySnapshot:
            {
                PAL_ASSERT(sizeof(GpuMemorySnapshotData) == eventDataSize);
                const GpuMemorySnapshotData* pData = reinterpret_cast<const GpuMemorySnapshotData*>(pEventData);

                RMT_MSG_USERDATA_EMBEDDED_STRING eventToken(
                    delta,
                    RMT_USERDATA_EVENT_TYPE_SNAPSHOT,
                    pData->pSnapshotName);

                WriteTokenData(eventToken);
                break;
            }
            case PalEvent::DebugName:
            {
                PAL_ASSERT(sizeof(DebugNameData) == eventDataSize);
                const DebugNameData* pData = reinterpret_cast<const DebugNameData*>(pEventData);

                RMT_MSG_USERDATA_DEBUG_NAME eventToken(
                    delta,
                    pData->pDebugName,
                    LowPart(pData->handle));

                WriteTokenData(eventToken);
                break;
            }
            case PalEvent::GpuMemoryResourceBind:
            {
                PAL_ASSERT(sizeof(GpuMemoryResourceBindData) == eventDataSize);
                const GpuMemoryResourceBindData* pData =
                    reinterpret_cast<const GpuMemoryResourceBindData*>(pEventData);

                RMT_MSG_RESOURCE_BIND eventToken(
                    delta,
                    pData->gpuVirtualAddr + pData->offset,
                    pData->requiredSize,
                    LowPart(pData->resourceHandle),
                    pData->isSystemMemory);

                WriteTokenData(eventToken);

                GpuMemory* pGpuMemory = reinterpret_cast<GpuMemory*>(pData->handle);
                if (pGpuMemory != nullptr)
                {
                    if (pData->requiredSize > pGpuMemory->Desc().size)
                    {
                        // GPU memory smaller than resource size
                        DD_ASSERT_ALWAYS();
                    }
                }
                break;
            }
            case PalEvent::GpuMemoryCpuMap:
            {
                PAL_ASSERT(sizeof(GpuMemoryCpuMapData) == eventDataSize);
                const GpuMemoryCpuMapData* pData = reinterpret_cast<const GpuMemoryCpuMapData*>(pEventData);

                RMT_MSG_CPU_MAP eventToken(delta, pData->gpuVirtualAddr, false);

                WriteTokenData(eventToken);
                break;
            }
            case PalEvent::GpuMemoryCpuUnmap:
            {
                PAL_ASSERT(sizeof(GpuMemoryCpuUnmapData) == eventDataSize);
                const GpuMemoryCpuUnmapData* pData = reinterpret_cast<const GpuMemoryCpuUnmapData*>(pEventData);

                RMT_MSG_CPU_MAP eventToken(delta, pData->gpuVirtualAddr, true);

                WriteTokenData(eventToken);
                break;
            }
            case PalEvent::GpuMemoryAddReference:
            {
                PAL_ASSERT(sizeof(GpuMemoryAddReferenceData) == eventDataSize);
                const GpuMemoryAddReferenceData* pData = reinterpret_cast<const GpuMemoryAddReferenceData*>(pEventData);

                RMT_MSG_RESOURCE_REFERENCE eventToken(
                    delta,
                    false,   // isRemove
                    pData->gpuVirtualAddr,
                    static_cast<uint8>(pData->queueHandle) & 0x7f);

                WriteTokenData(eventToken);
                break;
            }
            case PalEvent::GpuMemoryRemoveReference:
            {
                PAL_ASSERT(sizeof(GpuMemoryRemoveReferenceData) == eventDataSize);
                const GpuMemoryRemoveReferenceData* pData = reinterpret_cast<const GpuMemoryRemoveReferenceData*>(pEventData);

                RMT_MSG_RESOURCE_REFERENCE eventToken(
                    delta,
                    true,   // isRemove
                    pData->gpuVirtualAddr,
                    static_cast<uint8>(pData->queueHandle) & 0x7f);

                WriteTokenData(eventToken);
                break;
            }
            case PalEvent::ResourceInfoUpdate:
            {
                PAL_ASSERT(sizeof(ResourceUpdateInfoData) == eventDataSize);

                const auto* pUpdateInfo = reinterpret_cast<const ResourceUpdateInfoData*>(pEventData);
                // We are only logging buffers to capture DX12 raytracing resources. Logging all resource transitions
                // will lead to a significant increase in the size of the log file, so we are only supporting
                // buffers at this point.
                // Additionally, conversion functions are needed to support other types.
                PAL_ALERT_MSG(pUpdateInfo->type != ResourceType::Buffer,
                    "We only support buffers. Add conversion functions to use new types");

                RMT_MSG_RESOURCE_UPDATE rsrcUpdateToken(delta,
                                                        LowPart(pUpdateInfo->handle),
                                                        pUpdateInfo->subresourceId,
                                                        PalToRmtResourceType(pUpdateInfo->type),
                                                        PalToRmtBufferUsageFlags(pUpdateInfo->before),
                                                        PalToRmtBufferUsageFlags(pUpdateInfo->after));

                WriteTokenData(rsrcUpdateToken);
                break;
            }
        }
    }
}

// =====================================================================================================================
void GpuMemoryEventProvider::LogResourceCreateEvent(
    uint8       delta,
    const void* pEventData,
    size_t      eventDataSize)
{
    PAL_ASSERT(eventDataSize == sizeof(GpuMemoryResourceCreateData));
    const auto* pRsrcCreateData = reinterpret_cast<const GpuMemoryResourceCreateData*>(pEventData);

    RMT_MSG_RESOURCE_CREATE rsrcCreateToken(
        delta,
        LowPart(pRsrcCreateData->handle),
        RMT_OWNER_KMD,
        0,
        RMT_COMMIT_TYPE_COMMITTED,
        PalToRmtResourceType(pRsrcCreateData->type));
    WriteTokenData(rsrcCreateToken);

    switch (pRsrcCreateData->type)
    {
    case ResourceType::Image:
    {
        PAL_ASSERT(pRsrcCreateData->descriptionSize == sizeof(ResourceDescriptionImage));
        const auto* pImageData = reinterpret_cast<const ResourceDescriptionImage*>(pRsrcCreateData->pDescription);
        RMT_IMAGE_DESC_CREATE_INFO imgCreateInfo = {};
        imgCreateInfo.createFlags = PalToRmtImgCreateFlags(pImageData->pCreateInfo->flags);
        imgCreateInfo.usageFlags = PalToRmtImgUsageFlags(pImageData->pCreateInfo->usageFlags);
        imgCreateInfo.imageType = PalToRmtImageType(pImageData->pCreateInfo->imageType);
        imgCreateInfo.dimensions.dimension_X = static_cast<uint16>(pImageData->pCreateInfo->extent.width);
        imgCreateInfo.dimensions.dimension_Y = static_cast<uint16>(pImageData->pCreateInfo->extent.height);
        imgCreateInfo.dimensions.dimension_Z = static_cast<uint16>(pImageData->pCreateInfo->extent.depth);
        imgCreateInfo.format = PalToRmtImageFormat(pImageData->pCreateInfo->swizzledFormat);
        imgCreateInfo.mips = static_cast<uint8>(pImageData->pCreateInfo->mipLevels);
        imgCreateInfo.slices = static_cast<uint16>(pImageData->pCreateInfo->arraySize);
        imgCreateInfo.samples = static_cast<uint8>(pImageData->pCreateInfo->samples);
        imgCreateInfo.fragments = static_cast<uint8>(pImageData->pCreateInfo->fragments);
        imgCreateInfo.tilingType = PalToRmtTilingType(pImageData->pCreateInfo->tiling);
        imgCreateInfo.tilingOptMode = PalToRmtTilingOptMode(pImageData->pCreateInfo->tilingOptMode);
        imgCreateInfo.metadataMode = PalToRmtMetadataMode(pImageData->pCreateInfo->metadataMode);
        imgCreateInfo.maxBaseAlignment = pImageData->pCreateInfo->maxBaseAlign;
        imgCreateInfo.isPresentable = pImageData->isPresentable;
        imgCreateInfo.imageSize = static_cast<uint32>(pImageData->pMemoryLayout->dataSize);
        imgCreateInfo.metadataOffset = static_cast<uint32>(pImageData->pMemoryLayout->metadataOffset);
        imgCreateInfo.metadataSize = static_cast<uint32>(pImageData->pMemoryLayout->metadataSize);
        imgCreateInfo.metadataHeaderOffset = static_cast<uint32>(pImageData->pMemoryLayout->metadataHeaderOffset);
        imgCreateInfo.metadataHeaderSize = static_cast<uint32>(pImageData->pMemoryLayout->metadataHeaderSize);
        imgCreateInfo.imageAlignment = pImageData->pMemoryLayout->dataAlignment;
        imgCreateInfo.metadataAlignment = pImageData->pMemoryLayout->metadataAlignment;
        imgCreateInfo.metadataHeaderAlignment = pImageData->pMemoryLayout->metadataHeaderAlignment;
        imgCreateInfo.isFullscreen = pImageData->isFullscreen;

        RMT_RESOURCE_TYPE_IMAGE_TOKEN imgDesc(imgCreateInfo);

        WriteTokenData(imgDesc);
        break;
    }

    case ResourceType::Buffer:
    {
        PAL_ASSERT(pRsrcCreateData->descriptionSize == sizeof(ResourceDescriptionBuffer));
        const auto* pBufferData = reinterpret_cast<const ResourceDescriptionBuffer*>(pRsrcCreateData->pDescription);

        RMT_RESOURCE_TYPE_BUFFER_TOKEN bufferDesc(
            PalToRmtBufferCreateFlags(pBufferData->createFlags),
            PalToRmtBufferUsageFlags(pBufferData->usageFlags),
            pBufferData->size);

        WriteTokenData(bufferDesc);
        break;
    }

    case ResourceType::Pipeline:
    {
        PAL_ASSERT(pRsrcCreateData->descriptionSize == sizeof(ResourceDescriptionPipeline));
        const auto* pPipelineData = reinterpret_cast<const ResourceDescriptionPipeline*>(pRsrcCreateData->pDescription);

        RMT_PIPELINE_CREATE_FLAGS flags = {};
        flags.CLIENT_INTERNAL   = pPipelineData->pCreateFlags->clientInternal;
        flags.OVERRIDE_GPU_HEAP = 0; // pipeline heap override has been removed in PAL version 631.0

        RMT_PIPELINE_HASH hash = {};
        hash.hashUpper = pPipelineData->pPipelineInfo->internalPipelineHash.unique;
        hash.hashLower = pPipelineData->pPipelineInfo->internalPipelineHash.stable;

        const auto& shaderHashes = pPipelineData->pPipelineInfo->shader;
        RMT_PIPELINE_STAGES stages;
        stages.PS_STAGE = ShaderHashIsNonzero(shaderHashes[static_cast<uint32>(ShaderType::Pixel)].hash);
        stages.HS_STAGE = ShaderHashIsNonzero(shaderHashes[static_cast<uint32>(ShaderType::Hull)].hash);
        stages.DS_STAGE = ShaderHashIsNonzero(shaderHashes[static_cast<uint32>(ShaderType::Domain)].hash);
        stages.VS_STAGE = ShaderHashIsNonzero(shaderHashes[static_cast<uint32>(ShaderType::Vertex)].hash);
        stages.GS_STAGE = ShaderHashIsNonzero(shaderHashes[static_cast<uint32>(ShaderType::Geometry)].hash);
        stages.CS_STAGE = ShaderHashIsNonzero(shaderHashes[static_cast<uint32>(ShaderType::Compute)].hash);
        stages.TS_STAGE = ShaderHashIsNonzero(shaderHashes[static_cast<uint32>(ShaderType::Task)].hash);
        stages.MS_STAGE = ShaderHashIsNonzero(shaderHashes[static_cast<uint32>(ShaderType::Mesh)].hash);

        RMT_RESOURCE_TYPE_PIPELINE_TOKEN pipelineDesc(flags, hash, stages, false);

        WriteTokenData(pipelineDesc);
        break;
    }

    case ResourceType::Heap:
    {
        PAL_ASSERT(pRsrcCreateData->descriptionSize == sizeof(ResourceDescriptionHeap));
        const auto* pHeapData = reinterpret_cast<const ResourceDescriptionHeap*>(pRsrcCreateData->pDescription);

        RMT_HEAP_FLAGS rmtFlags = {};
        if (Util::TestAnyFlagSet(pHeapData->flags,
            static_cast<uint32>(ResourceDescriptionHeapFlags::NonRenderTargetDepthStencilTextures)))
        {
            rmtFlags.NON_RT_DS_TEXTURES = 1;
        }

        if (Util::TestAnyFlagSet(pHeapData->flags,
            static_cast<uint32>(ResourceDescriptionHeapFlags::Buffers)))
        {
            rmtFlags.BUFFERS = 1;
        }

        if (Util::TestAnyFlagSet(pHeapData->flags,
            static_cast<uint32>(ResourceDescriptionHeapFlags::CoherentSystemWide)))
        {
            rmtFlags.COHERENT_SYSTEM_WIDE = 1;
        }
        if (Util::TestAnyFlagSet(pHeapData->flags,
            static_cast<uint32>(ResourceDescriptionHeapFlags::Primary)))
        {
            rmtFlags.PRIMARY = 1;
        }

        if (Util::TestAnyFlagSet(pHeapData->flags,
            static_cast<uint32>(ResourceDescriptionHeapFlags::RenderTargetDepthStencilTextures)))
        {
            rmtFlags.RT_DS_TEXTURES = 1;
        }

        if (Util::TestAnyFlagSet(pHeapData->flags,
            static_cast<uint32>(ResourceDescriptionHeapFlags::DenyL0Demotion)))
        {
            rmtFlags.DENY_L0_PROMOTION = 1;
        }

        RMT_RESOURCE_TYPE_HEAP_TOKEN heapDesc(
            rmtFlags,
            pHeapData->size,
            RMT_PAGE_SIZE_4KB,  //< @TODO - we don't currently have this info, so just set to 4KB
            static_cast<uint8>(pHeapData->preferredGpuHeap));

        WriteTokenData(heapDesc);
        break;
    }

    case ResourceType::GpuEvent:
    {
        PAL_ASSERT(pRsrcCreateData->descriptionSize == sizeof(ResourceDescriptionGpuEvent));
        const auto* pGpuEventData = reinterpret_cast<const ResourceDescriptionGpuEvent*>(pRsrcCreateData->pDescription);

        const bool isGpuOnly = (pGpuEventData->pCreateInfo->flags.gpuAccessOnly == 1);
        RMT_RESOURCE_TYPE_GPU_EVENT_TOKEN gpuEventDesc(isGpuOnly);

        WriteTokenData(gpuEventDesc);
        break;
    }

    case ResourceType::BorderColorPalette:
    {
        PAL_ASSERT(pRsrcCreateData->descriptionSize == sizeof(ResourceDescriptionBorderColorPalette));
        const auto* pBcpData = reinterpret_cast<const ResourceDescriptionBorderColorPalette*>(pRsrcCreateData->pDescription);

        RMT_RESOURCE_TYPE_BORDER_COLOR_PALETTE_TOKEN bcpDesc(static_cast<uint8>(pBcpData->pCreateInfo->paletteSize));

        WriteTokenData(bcpDesc);
        break;
    }

    case ResourceType::PerfExperiment:
    {
        PAL_ASSERT(pRsrcCreateData->descriptionSize == sizeof(ResourceDescriptionPerfExperiment));
        const auto* pPerfExperimentData =
            reinterpret_cast<const ResourceDescriptionPerfExperiment*>(pRsrcCreateData->pDescription);

        RMT_RESOURCE_TYPE_PERF_EXPERIMENT_TOKEN perfExperimentDesc(
            static_cast<uint32>(pPerfExperimentData->spmSize),
            static_cast<uint32>(pPerfExperimentData->sqttSize),
            static_cast<uint32>(pPerfExperimentData->perfCounterSize));

        WriteTokenData(perfExperimentDesc);
        break;
    }

    case ResourceType::QueryPool:
    {
        PAL_ASSERT(pRsrcCreateData->descriptionSize == sizeof(ResourceDescriptionQueryPool));
        const auto* pQueryPoolData = reinterpret_cast<const ResourceDescriptionQueryPool*>(pRsrcCreateData->pDescription);

        RMT_RESOURCE_TYPE_QUERY_HEAP_TOKEN queryHeapDesc(
            PalToRmtQueryHeapType(pQueryPoolData->pCreateInfo->queryPoolType),
            (pQueryPoolData->pCreateInfo->flags.enableCpuAccess == 1));

        WriteTokenData(queryHeapDesc);
        break;
    }

    case ResourceType::VideoEncoder:
    {
        break;
    }

    case ResourceType::VideoDecoder:
    {
        break;
    }

    case ResourceType::DescriptorHeap:
    {
        PAL_ASSERT(pRsrcCreateData->descriptionSize == sizeof(ResourceDescriptionDescriptorHeap));
        const auto* pDescriptorHeapData =
            reinterpret_cast<const ResourceDescriptionDescriptorHeap*>(pRsrcCreateData->pDescription);

        RMT_RESOURCE_TYPE_DESCRIPTOR_HEAP_TOKEN descriptorHeapDesc(
            PalToRmtDescriptorType(pDescriptorHeapData->type),
            pDescriptorHeapData->isShaderVisible,
            static_cast<uint8>(pDescriptorHeapData->nodeMask),
            static_cast<uint16>(pDescriptorHeapData->numDescriptors));

        WriteTokenData(descriptorHeapDesc);
        break;
    }

    case ResourceType::DescriptorPool:
    {
        PAL_ASSERT(pRsrcCreateData->descriptionSize == sizeof(ResourceDescriptionDescriptorPool));
        const auto* pDescriptorPoolData =
            reinterpret_cast<const ResourceDescriptionDescriptorPool*>(pRsrcCreateData->pDescription);

        RMT_RESOURCE_TYPE_POOL_SIZE_TOKEN poolSizeDesc(
            static_cast<uint16>(pDescriptorPoolData->maxSets),
            static_cast<uint8>(pDescriptorPoolData->numPoolSize));

        WriteTokenData(poolSizeDesc);

        // Then loop through writing RMT_POOL_SIZE_DESCs
        for (uint32 i = 0; i < pDescriptorPoolData->numPoolSize; ++i)
        {
            RMT_POOL_SIZE_DESC poolSize(
                PalToRmtDescriptorType(pDescriptorPoolData->pPoolSizes[i].type),
                static_cast<uint16>(pDescriptorPoolData->pPoolSizes[i].numDescriptors));

            WriteTokenData(poolSize);
        }
        break;
    }

    case ResourceType::CmdAllocator:
    {
        PAL_ASSERT(pRsrcCreateData->descriptionSize == sizeof(ResourceDescriptionCmdAllocator));
        const auto* pCmdAllocatorData =
            reinterpret_cast<const ResourceDescriptionCmdAllocator*>(pRsrcCreateData->pDescription);

        RMT_RESOURCE_TYPE_CMD_ALLOCATOR_TOKEN cmdAllocatorDesc(
            PalToRmtCmdAllocatorCreateFlags(pCmdAllocatorData->pCreateInfo->flags),
            PalToRmtHeapType(pCmdAllocatorData->pCreateInfo->allocInfo[CmdAllocType::CommandDataAlloc].allocHeap),
            pCmdAllocatorData->pCreateInfo->allocInfo[CmdAllocType::CommandDataAlloc].allocSize,
            pCmdAllocatorData->pCreateInfo->allocInfo[CmdAllocType::CommandDataAlloc].suballocSize,
            PalToRmtHeapType(pCmdAllocatorData->pCreateInfo->allocInfo[CmdAllocType::EmbeddedDataAlloc].allocHeap),
            pCmdAllocatorData->pCreateInfo->allocInfo[CmdAllocType::EmbeddedDataAlloc].allocSize,
            pCmdAllocatorData->pCreateInfo->allocInfo[CmdAllocType::EmbeddedDataAlloc].suballocSize,
            PalToRmtHeapType(pCmdAllocatorData->pCreateInfo->allocInfo[CmdAllocType::GpuScratchMemAlloc].allocHeap),
            pCmdAllocatorData->pCreateInfo->allocInfo[CmdAllocType::GpuScratchMemAlloc].allocSize,
            pCmdAllocatorData->pCreateInfo->allocInfo[CmdAllocType::GpuScratchMemAlloc].suballocSize);

        WriteTokenData(cmdAllocatorDesc);
        break;
    }

    case ResourceType::MiscInternal:
    {
        PAL_ASSERT(pRsrcCreateData->descriptionSize == sizeof(ResourceDescriptionMiscInternal));
        const auto* pMiscInternalData =
            reinterpret_cast<const ResourceDescriptionMiscInternal*>(pRsrcCreateData->pDescription);

        RMT_RESOURCE_TYPE_MISC_INTERNAL_TOKEN miscInternalDesc(PalToRmtMiscInternalType(pMiscInternalData->type));

        WriteTokenData(miscInternalDesc);
        break;
    }

    case ResourceType::IndirectCmdGenerator:
        // IndirectCmdGenerator has no description data
        PAL_ASSERT(pRsrcCreateData->descriptionSize == 0);
        break;

    case ResourceType::MotionEstimator:
        // MotionEstimator has no description data
        PAL_ASSERT(pRsrcCreateData->descriptionSize == 0);
        break;

    case ResourceType::Timestamp:
        // Timestamp has no description data
        PAL_ASSERT(pRsrcCreateData->descriptionSize == 0);
        break;

    default:
        PAL_ASSERT_ALWAYS();
        break;
    }
}

} // Pal
