/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

// This is temporary. Eventually this will be defined by devDriver
#define GPUOPEN_EVENT_PROVIDER_VERSION 999

#include "palFile.h"
#include "palGpuMemoryBindable.h"
#include "palJsonWriter.h"
#include "palMutex.h"
#include "palPlatform.h"
#include "core/eventDefs.h"

#if GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION >= GPUOPEN_EVENT_PROVIDER_VERSION
#include "protocols/ddEventServer.h"
#endif

namespace Pal
{

class Device;
class GpuMemory;
class Platform;
class Queue;

// =====================================================================================================================
// JSON stream that records the text stream using a staging buffer and a log file. WriteFile must be called explicitly
// to flush all buffered text. Note that this makes it possible to generate JSON text before OpenFile has been called.
class EventLogStream : public Util::JsonStream
{
public:
    explicit EventLogStream(Platform* pPlatform);
    virtual ~EventLogStream();

    Result OpenFile(const char* pFilePath);
    void CloseFile();
    Result WriteBufferedData();

    // Returns true if the log file has already been opened.
    bool IsFileOpen() const { return m_file.IsOpen(); }

    virtual void WriteString(const char* pString, uint32 length) override;
    virtual void WriteCharacter(char character) override;

private:
    void VerifyUnusedSpace(uint32 size);

    Platform*const m_pPlatform;
    Util::File     m_file;       // The text stream is being written here.
    char*          m_pBuffer;    // Buffered text data that needs to be written to the file.
    uint32         m_bufferSize; // The size of the buffer in characters.
    uint32         m_bufferUsed; // How many characters of the buffer are in use.
    uint32         m_flushSize;  // How many bytes have been written since the last flush to disk.

    // Determines how many bytes should be written to the file between each flush to disk
    static constexpr uint32 FlushThreshold = 4096;

    PAL_DISALLOW_DEFAULT_CTOR(EventLogStream);
    PAL_DISALLOW_COPY_AND_ASSIGN(EventLogStream);
};

// =====================================================================================================================
// The PalEventProvider class is a class derived from DevDriver EventProvider that is be responsible for logging
// developer mode events in PAL.
#if GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION >= GPUOPEN_EVENT_PROVIDER_VERSION
class EventProvider : public DevDriver::EventProtocol::EventProvider
#else
class EventProvider
#endif
{
public:
    EventProvider(Platform* pPlatform)
        :
#if GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION >= GPUOPEN_EVENT_PROVIDER_VERSION
        DevDriver::EventProtocol::EventProvider(),
#endif
        m_pPlatform(pPlatform),
        m_isFileLoggingActive(false),
        m_eventStream(pPlatform),
        m_jsonWriter(&m_eventStream)
        {}

    virtual ~EventProvider() {}

    Result Init();

    void Destroy();

    Result EnableFileLogging(const char* pFilePath);
    void DisableFileLogging();
    Result OpenLogFile(const char* pFilePath);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Event Log Functions
    // These functions will result in an event being sent through the DevDriver EventProtocol or to the event log file
    // if the provider and event are enabled.

    void LogCreateGpuMemoryEvent(const GpuMemory* pGpuMemory, Result result, bool isInternal);

    void LogDestroyGpuMemoryEvent(const GpuMemory* pGpuMemory);

    void LogGpuMemoryResourceBindEvent(
        const IDestroyable* pObj,
        gpusize             requiredGpuMemSize,
        const IGpuMemory*   pGpuMemory,
        gpusize             offset);

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

    // End of Event Log Functions
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

private:
    void WriteEventHeader(PalEvent eventId, uint32 dataSize);
    bool ShouldLog(PalEvent eventId) const;

    Platform*        m_pPlatform;
    bool             m_isFileLoggingActive;
    Util::Mutex      m_eventStreamMutex;
    EventLogStream   m_eventStream;
    Util::Mutex      m_jsonWriterMutex;
    Util::JsonWriter m_jsonWriter;

    PAL_DISALLOW_COPY_AND_ASSIGN(EventProvider);
};

} // Pal
