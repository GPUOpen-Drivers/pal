/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/eventProvider.h"
#include "core/queue.h"
#include "core/platform.h"
#include "core/gpuMemory.h"
#include "palSysUtil.h"
#include "core/devDriverUtil.h"
#include "devDriverServer.h"

using namespace Util;
using namespace DevDriver;

namespace Pal
{

EventProvider::EventProvider(Platform* pPlatform)
        :
#if GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION >= GPUOPEN_EVENT_PROVIDER_VERSION
        DevDriver::EventProtocol::EventProvider(),
#endif
        m_pPlatform(pPlatform),
        m_isFileLoggingActive(false),
        m_eventStream(pPlatform),
        m_jsonWriter(&m_eventStream),
        m_eventService({ pPlatform, DevDriverAlloc, DevDriverFree })
        {}

// =====================================================================================================================
Result EventProvider::Init()
{
    Result result = m_jsonWriterMutex.Init();

    if (result == Result::Success)
    {
        result = m_eventStreamMutex.Init();
    }

    if ((result == Result::Success) && (m_pPlatform->GetDevDriverServer() != nullptr))
    {
        result = (m_pPlatform->GetDevDriverServer()->GetMessageChannel()->RegisterService(&m_eventService) ==
                  DevDriver::Result::Success) ? Result::Success : Result::ErrorUnknown;
    }

    return result;
}

// =====================================================================================================================
// Destroys this EventProvider, flushing and closing the event log file if necessary.
void EventProvider::Destroy()
{
    MutexAuto lock(&m_eventStreamMutex);
    if (m_isFileLoggingActive)
    {
        EndEventLogStream(&m_jsonWriter);
        m_eventStream.CloseFile();
    }
}

// =====================================================================================================================
// Enables logging of events to the specified file
Result EventProvider::EnableFileLogging(
    const char* pFilePath)
{
    MutexAuto lock(&m_eventStreamMutex);

    m_isFileLoggingActive = true;

    // Try to open the file
    Result result = Result::Success;
    if (pFilePath != nullptr)
    {
        result = m_eventStream.OpenFile(pFilePath);
    }

    if (result == Result::Success)
    {
        BeginEventLogStream(&m_jsonWriter);
        PalEventFileHeader header = {};
        header.version = PAL_EVENT_LOG_VERSION;
        header.headerSize = sizeof(PalEventFileHeader);
        SerializeEventLogFileHeader(&m_jsonWriter, header);
    }

    return result;
}

// =====================================================================================================================
// Enables logging of events to the specified file
Result EventProvider::OpenLogFile(
    const char* pFilePath)
{
    MutexAuto lock(&m_eventStreamMutex);

    return m_eventStream.OpenFile(pFilePath);
}

// =====================================================================================================================
// Disables logging of events to file, flushing and closing the open file.
void EventProvider::DisableFileLogging()
{
    MutexAuto lock(&m_eventStreamMutex);

    // Close the log file
    EndEventLogStream(&m_jsonWriter);
    m_eventStream.CloseFile();
    m_isFileLoggingActive = false;
}

// =====================================================================================================================
// Writes an event header to the log file.
// NOTE: It is assumed the caller has taken the file stream mutex
void EventProvider::WriteEventHeader(
    PalEvent eventId,
    uint32   dataSize)
{
    PalEventHeader eventHeader = {};
    eventHeader.eventId = eventId;
    eventHeader.eventDataSize = dataSize;
    eventHeader.timestamp = GetPerfCpuTime();

    SerializeEventHeader(&m_jsonWriter, eventHeader);
}

// =====================================================================================================================
// Determines if the event would be written to either the EventServer or to the log file, used to determine if a log
// event call should bother constructing the log event data structure.
bool EventProvider::ShouldLog(
    PalEvent eventId
    ) const
{
    bool shouldLog = (m_isFileLoggingActive || m_eventService.IsMemoryProfilingEnabled());
#if GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION >= GPUOPEN_EVENT_PROVIDER_VERSION
    // If the event provider and event ID are active, set shouldLog to true
#endif

    return shouldLog;
}

// =====================================================================================================================
// Logs an event on creation of a GPU Memory allocation (physical or virtual).
void EventProvider::LogCreateGpuMemoryEvent(
    const GpuMemory* pGpuMemory)
{
    // We only want to log new allocations
    if ((pGpuMemory != nullptr) && (pGpuMemory->IsGpuVaPreReserved() == false))
    {
        static constexpr PalEvent EventId = PalEvent::CreateGpuMemory;
        if (ShouldLog(EventId))
        {
            const GpuMemoryDesc desc = pGpuMemory->Desc();
            CreateGpuMemoryData data = {};
            data.handle = reinterpret_cast<GpuMemHandle>(pGpuMemory);
            data.size = desc.size;
            data.alignment = desc.alignment;
            data.preferredHeap = desc.preferredHeap;
            data.isVirtual = desc.flags.isVirtual;
            data.isInternal = pGpuMemory->IsClient();
            data.isExternalShared = desc.flags.isExternal;
            data.gpuVirtualAddr = desc.gpuVirtAddr;

    #if GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION >= GPUOPEN_EVENT_PROVIDER_VERSION
            // Call the EventServer
    #endif

            m_eventService.LogEvent(EventId, &data, sizeof(data));

            if (m_isFileLoggingActive)
            {
                MutexAuto lock(&m_jsonWriterMutex);
                WriteEventHeader(EventId, sizeof(CreateGpuMemoryData));
                SerializeCreateGpuMemoryData(&m_jsonWriter, data);
            }
        }
    }
}

// =====================================================================================================================
// Logs an event when a GPU Memory allocation (physical or virtual) is destroyed.
void EventProvider::LogDestroyGpuMemoryEvent(
    const GpuMemory* pGpuMemory)
{
    static constexpr PalEvent EventId = PalEvent::DestroyGpuMemory;
    if (ShouldLog(EventId))
    {
        DestroyGpuMemoryData data = {};
        data.handle = reinterpret_cast<GpuMemHandle>(pGpuMemory);
        data.gpuVirtualAddr = pGpuMemory->Desc().gpuVirtAddr;

#if GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION >= GPUOPEN_EVENT_PROVIDER_VERSION
        // Call the EventServer
#endif
        m_eventService.LogEvent(EventId, &data, sizeof(data));

        if (m_isFileLoggingActive)
        {
            MutexAuto lock(&m_jsonWriterMutex);
            WriteEventHeader(EventId, sizeof(DestroyGpuMemoryData));
            SerializeDestroyGpuMemoryData(&m_jsonWriter, data);
        }
    }
}

// =====================================================================================================================
// Logs an event when a resource has GPU memory bound to it.
void EventProvider::LogGpuMemoryResourceBindEvent(
    const GpuMemoryResourceBindEventData& eventData)
{
    static constexpr PalEvent EventId = PalEvent::GpuMemoryResourceBind;
    if (ShouldLog(EventId))
    {
        GpuMemoryResourceBindData data = {};
        data.handle = reinterpret_cast<GpuMemHandle>(eventData.pGpuMemory);
        data.gpuVirtualAddr = (eventData.pGpuMemory != nullptr) ? eventData.pGpuMemory->Desc().gpuVirtAddr : 0;
        PAL_ASSERT(eventData.pObj != nullptr);
        data.resourceHandle = reinterpret_cast<ResourceHandle>(eventData.pObj);
        data.requiredSize = eventData.requiredGpuMemSize;
        data.offset = eventData.offset;
        data.isSystemMemory = eventData.isSystemMemory;

#if GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION >= GPUOPEN_EVENT_PROVIDER_VERSION
        // Call the EventServer
#endif

        m_eventService.LogEvent(EventId, &data, sizeof(data));

        if (m_isFileLoggingActive)
        {
            MutexAuto lock(&m_jsonWriterMutex);
            WriteEventHeader(EventId, sizeof(GpuMemoryResourceBindData));
            SerializeGpuMemoryResourceBindData(&m_jsonWriter, data);
        }
    }
}

// =====================================================================================================================
// Logs an event when a GPU memory allocation is mapped for CPU access.
void EventProvider::LogGpuMemoryCpuMapEvent(
    const GpuMemory* pGpuMemory)
{
    static constexpr PalEvent EventId = PalEvent::GpuMemoryCpuMap;
    if (ShouldLog(EventId))
    {
        GpuMemoryCpuMapData data = {};
        data.handle = reinterpret_cast<GpuMemHandle>(pGpuMemory);
        data.gpuVirtualAddr = pGpuMemory->Desc().gpuVirtAddr;

#if GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION >= GPUOPEN_EVENT_PROVIDER_VERSION
        // Call the EventServer
#endif

        m_eventService.LogEvent(EventId, &data, sizeof(data));

        if (m_isFileLoggingActive)
        {
            MutexAuto lock(&m_jsonWriterMutex);
            WriteEventHeader(EventId, sizeof(GpuMemoryCpuMapData));
            SerializeGpuMemoryCpuMapData(&m_jsonWriter, data);
        }
    }
}

// =====================================================================================================================
// Logs an event when a GPU memory allocation is unmapped for CPU access.
void EventProvider::LogGpuMemoryCpuUnmapEvent(
    const GpuMemory* pGpuMemory)
{
    static constexpr PalEvent EventId = PalEvent::GpuMemoryCpuUnmap;
    if (ShouldLog(EventId))
    {
        GpuMemoryCpuUnmapData data = {};
        data.handle = reinterpret_cast<GpuMemHandle>(pGpuMemory);
        data.gpuVirtualAddr = pGpuMemory->Desc().gpuVirtAddr;

#if GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION >= GPUOPEN_EVENT_PROVIDER_VERSION
        // Call the EventServer
#endif

        m_eventService.LogEvent(EventId, &data, sizeof(data));

        if (m_isFileLoggingActive)
        {
            MutexAuto lock(&m_jsonWriterMutex);
            WriteEventHeader(EventId, sizeof(GpuMemoryCpuUnmapData));
            SerializeGpuMemoryCpuUnmapData(&m_jsonWriter, data);
        }
    }
}

// =====================================================================================================================
// Logs an event when GPU memory allocations are added to a per-device or per-queue reference list. The flags field is
// a GpuMemoryRefFlags flags type.
// NOTE: It is expected that pQueue will always be null for WDDM2.
void EventProvider::LogGpuMemoryAddReferencesEvent(
    uint32              gpuMemRefCount,
    const GpuMemoryRef* pGpuMemoryRefs,
    IQueue*             pQueue,
    uint32              flags)
{
    static constexpr PalEvent EventId = PalEvent::GpuMemoryAddReference;
    if (ShouldLog(EventId))
    {
        for (uint32 i=0; i < gpuMemRefCount; i++)
        {
            GpuMemoryAddReferenceData data = {};
            data.handle = reinterpret_cast<GpuMemHandle>(pGpuMemoryRefs[i].pGpuMemory);
            data.gpuVirtualAddr = pGpuMemoryRefs[i].pGpuMemory->Desc().gpuVirtAddr;
            data.queueHandle = reinterpret_cast<QueueHandle>(pQueue);
            data.flags = flags;

#if GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION >= GPUOPEN_EVENT_PROVIDER_VERSION
            // Call the EventServer
#endif

            m_eventService.LogEvent(EventId, &data, sizeof(data));

            if (m_isFileLoggingActive)
            {
                MutexAuto lock(&m_jsonWriterMutex);
                WriteEventHeader(EventId, sizeof(GpuMemoryAddReferenceData));
                SerializeGpuMemoryAddReferenceData(&m_jsonWriter, data);
            }
        }
    }
}

// =====================================================================================================================
// Logs an event when GPU memory allocations are removed from a per-device or per-queue reference list.
// NOTE: It is expected that pQueue will always be null for WDDM2.
void EventProvider::LogGpuMemoryRemoveReferencesEvent(
    uint32            gpuMemoryCount,
    IGpuMemory*const* ppGpuMemory,
    IQueue*           pQueue)
{
    static constexpr PalEvent EventId = PalEvent::GpuMemoryRemoveReference;
    if (ShouldLog(EventId))
    {
        for (uint32 i = 0; i < gpuMemoryCount; i++)
        {
            GpuMemoryRemoveReferenceData data = {};
            data.handle = reinterpret_cast<GpuMemHandle>(ppGpuMemory[i]);
            data.gpuVirtualAddr = ppGpuMemory[i]->Desc().gpuVirtAddr;
            data.queueHandle = reinterpret_cast<QueueHandle>(pQueue);

#if GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION >= GPUOPEN_EVENT_PROVIDER_VERSION
            // Call the EventServer
#endif

            m_eventService.LogEvent(EventId, &data, sizeof(data));

            if (m_isFileLoggingActive)
            {
                MutexAuto lock(&m_jsonWriterMutex);
                WriteEventHeader(EventId, sizeof(GpuMemoryRemoveReferenceData));
                SerializeGpuMemoryRemoveReferenceData(&m_jsonWriter, data);
            }
        }
    }
}

// =====================================================================================================================
// Logs an event when a resource that requires GPU memory is created.  See the ResourceType enum for the list of
// resources this applies to.
void EventProvider::LogGpuMemoryResourceCreateEvent(
    const ResourceCreateEventData& eventData)
{
    static constexpr PalEvent EventId = PalEvent::GpuMemoryResourceCreate;
#if GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION >= GPUOPEN_EVENT_PROVIDER_VERSION
    // Call the EventServer
#endif

    if (ShouldLog(EventId))
    {
        GpuMemoryResourceCreateData data = {};
        PAL_ASSERT(eventData.pObj != nullptr);
        data.handle = reinterpret_cast<ResourceHandle>(eventData.pObj);
        data.type = eventData.type;
        data.descriptionSize = eventData.resourceDescSize;
        data.pDescription = eventData.pResourceDescData;

#if GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION >= GPUOPEN_EVENT_PROVIDER_VERSION
        // Call the EventServer
#endif

        m_eventService.LogEvent(EventId, &data, sizeof(data));

        if (m_isFileLoggingActive)
        {
            MutexAuto lock(&m_jsonWriterMutex);
            WriteEventHeader(EventId, sizeof(GpuMemoryResourceCreateData) + data.descriptionSize);
            SerializeGpuMemoryResourceCreate(&m_jsonWriter, data);
        }
    }
}

// =====================================================================================================================
// Logs an event when a resource that requires GPU memory is destroyed.  See the ResourceType enum for the list of
// resources this applies to.
void EventProvider::LogGpuMemoryResourceDestroyEvent(
    const ResourceDestroyEventData& eventData)
{
    static constexpr PalEvent EventId = PalEvent::GpuMemoryResourceDestroy;

    if (ShouldLog(EventId))
    {
        GpuMemoryResourceDestroyData data = {};
        PAL_ASSERT(eventData.pObj != nullptr);
        data.handle = reinterpret_cast<ResourceHandle>(eventData.pObj);

#if GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION >= GPUOPEN_EVENT_PROVIDER_VERSION
        // Call the EventServer
#endif

        m_eventService.LogEvent(EventId, &data, sizeof(data));

        if (m_isFileLoggingActive)
        {
            MutexAuto lock(&m_jsonWriterMutex);
            WriteEventHeader(EventId, sizeof(GpuMemoryResourceDestroyData));
            SerializeGpuMemoryResourceDestroy(&m_jsonWriter, data);
        }
    }
}

// =====================================================================================================================
// Logs an event capturing the assignment of an app-specified name for an object.
void EventProvider::LogDebugNameEvent(
    const DebugNameEventData& eventData)
{
    static constexpr PalEvent EventId = PalEvent::DebugName;

    if (ShouldLog(EventId))
    {
        DebugNameData data = {};
        PAL_ASSERT(eventData.pObj != nullptr);
        data.handle = reinterpret_cast<ResourceHandle>(eventData.pObj);
        data.pDebugName = eventData.pDebugName;
        data.nameSize = static_cast<uint32>(strlen(eventData.pDebugName));

#if GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION >= GPUOPEN_EVENT_PROVIDER_VERSION
        // Call the EventServer
#endif

        m_eventService.LogEvent(EventId, &data, sizeof(data));

        if (m_isFileLoggingActive)
        {
            MutexAuto lock(&m_jsonWriterMutex);
            WriteEventHeader(EventId, sizeof(DebugNameData));
            SerializeDebugName(&m_jsonWriter, data);
        }
    }
}

// =====================================================================================================================
// Logs a miscellaneous event that requires no additional data.  See MiscEventType for the list of miscellaneous events.
void EventProvider::LogGpuMemoryMiscEvent(
    const MiscEventData& eventData)
{
    static constexpr PalEvent EventId = PalEvent::GpuMemoryMisc;

    if (ShouldLog(EventId))
    {
        GpuMemoryMiscData data = {};
        data.type = eventData.eventType;
        data.engine = eventData.engine;

#if GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION >= GPUOPEN_EVENT_PROVIDER_VERSION
        // Call the EventServer
#endif

        m_eventService.LogEvent(EventId, &data, sizeof(data));

        if (m_isFileLoggingActive)
        {
            MutexAuto lock(&m_jsonWriterMutex);
            WriteEventHeader(EventId, sizeof(GpuMemoryMiscData));
            SerializeGpuMemoryMisc(&m_jsonWriter, data);
        }
    }
}

// =====================================================================================================================
// Logs an event when an application/driver wants to insert a snapshot marker into the event data.  A snapshot is a
// named point in time that can give context to the surrounding event data.
void EventProvider::LogGpuMemorySnapshotEvent(
    const GpuMemorySnapshotEventData& eventData)
{
    static constexpr PalEvent EventId = PalEvent::GpuMemorySnapshot;

    if (ShouldLog(EventId))
    {
        GpuMemorySnapshotData data = {};
        data.pSnapshotName = eventData.pSnapshotName;

#if GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION >= GPUOPEN_EVENT_PROVIDER_VERSION
        // Call the EventServer
#endif

        m_eventService.LogEvent(EventId, &data, sizeof(data));

        if (m_isFileLoggingActive)
        {
            MutexAuto lock(&m_jsonWriterMutex);
            WriteEventHeader(EventId, sizeof(GpuMemorySnapshotData));
            SerializeGpuMemorySnapshot(&m_jsonWriter, data);
        }
    }
}

// =====================================================================================================================
EventLogStream::EventLogStream(
    Platform* pPlatform)
    :
    m_pPlatform(pPlatform),
    m_pBuffer(nullptr),
    m_bufferSize(0),
    m_bufferUsed(0),
    m_flushSize(0)
{
}

// =====================================================================================================================
EventLogStream::~EventLogStream()
{
    PAL_SAFE_FREE(m_pBuffer, m_pPlatform);
}

// =====================================================================================================================
Result EventLogStream::OpenFile(
    const char* pFilePath)
{
    Result result = m_file.Open(pFilePath, Util::FileAccessWrite);

    if (result == Result::Success)
    {
        // Write out anything that was logged before now.
        result = WriteBufferedData();
    }

    return result;
}

// =====================================================================================================================
void EventLogStream::CloseFile()
{
    if (m_file.IsOpen())
    {
        m_file.Flush();
        m_file.Close();
    }
}

// =====================================================================================================================
Result EventLogStream::WriteBufferedData()
{
    Result result = Result::Success;

    if (m_bufferUsed > 0)
    {
        result = m_file.Write(m_pBuffer, m_bufferUsed * sizeof(char));
        m_bufferUsed = 0;

        if (result == Result::Success)
        {
            // Flush to disk to make the logs more useful if the application crashes.
            result = m_file.Flush();
        }
    }

    return result;
}

// =====================================================================================================================
void EventLogStream::WriteString(
    const char* pString,
    uint32      length)
{
    // If we've already opened the log file, just write directly to it
    if (m_file.IsOpen())
    {
        if (m_file.Write(pString, length) == Result::Success)
        {
            m_flushSize += length;
            if (m_flushSize >= FlushThreshold)
            {
                // Flush to disk periodically to make the logs more useful if the application crashes.
                m_file.Flush();
                m_flushSize = 0;
            }
        }
    }
    else
    {
        // Otherwise buffer up the event data
        VerifyUnusedSpace(length);
        memcpy(m_pBuffer + m_bufferUsed, pString, length * sizeof(char));
        m_bufferUsed += length;
    }
}

// =====================================================================================================================
void EventLogStream::WriteCharacter(
    char character)
{
    // If we've already opened the log file, just write directly to it
    if (m_file.IsOpen())
    {
        if (m_file.Write(&character, 1) == Result::Success)
        {
            m_flushSize += 1;
            if (m_flushSize >= FlushThreshold)
            {
                // Flush to disk periodically to make the logs more useful if the application crashes.
                m_file.Flush();
                m_flushSize = 0;
            }
        }
    }
    else
    {
        VerifyUnusedSpace(1);
        m_pBuffer[m_bufferUsed++] = character;
    }
}

// =====================================================================================================================
// Verifies that the buffer has enough space for an additional "size" characters, reallocating if necessary.
void EventLogStream::VerifyUnusedSpace(
    uint32 size)
{
    if (m_bufferSize - m_bufferUsed < size)
    {
        const char* pOldBuffer = m_pBuffer;

        // Bump up the size of the buffer to the next multiple of 4K that fits the current contents plus "size".
        m_bufferSize = Pow2Align(m_bufferSize + size, 4096);
        m_pBuffer = static_cast<char*>(PAL_MALLOC(m_bufferSize * sizeof(char), m_pPlatform, AllocInternal));

        PAL_ASSERT(m_pBuffer != nullptr);

        memcpy(m_pBuffer, pOldBuffer, m_bufferUsed);
        PAL_SAFE_FREE(pOldBuffer, m_pPlatform);
    }
}

} // Pal
