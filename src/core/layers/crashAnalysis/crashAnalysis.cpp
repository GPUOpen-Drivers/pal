/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/layers/crashAnalysis/crashAnalysis.h"
#include "core/layers/crashAnalysis/crashAnalysisDevice.h"

#include "palVectorImpl.h"
#include "palSysMemory.h"
#include "palStringBagImpl.h"

using namespace UmdCrashAnalysisEvents;

namespace Pal
{
namespace CrashAnalysis
{

// =====================================================================================================================
RefCounter::RefCounter(
    IPlatform* pPlatform)
    :
    m_refCount(1),
    m_pPlatform(pPlatform)
{
}

// =====================================================================================================================
void RefCounter::TakeReference()
{
    PAL_ASSERT(m_refCount > 0);
    Util::AtomicIncrement(&m_refCount);
}

// =====================================================================================================================
void RefCounter::ReleaseReference()
{
    PAL_ASSERT(m_refCount > 0);

    if (Util::AtomicDecrement(&m_refCount) == 0)
    {
        Destroy();
    }
}

// =====================================================================================================================
MemoryChunk::MemoryChunk(
    Device* pDevice)
    :
    RefCounter(static_cast<IPlatform*>(pDevice->GetPlatform())),
    gpuVirtAddr(0),
    pCpuAddr(nullptr),
    raftIndex(0),
    pDevice(pDevice)
{
}

// =====================================================================================================================
void MemoryChunk::Destroy()
{
    pDevice->FreeMemoryChunkAllocation(raftIndex, gpuVirtAddr);
    PAL_DELETE_THIS(MemoryChunk, GetAllocator());
}

// =====================================================================================================================
EventCache::EventCache(
    IPlatform* pPlatform)
    :
    RefCounter(pPlatform),
    m_eventCache(pPlatform),
    m_markerNameBag(pPlatform)
{
}

// =====================================================================================================================
void EventCache::Destroy()
{
    m_eventCache.Clear();
    PAL_DELETE_THIS(EventCache, GetAllocator());
}

// =====================================================================================================================
// Serializes an ExecutionMarkerTop event info the event cache.
Result EventCache::CacheExecutionMarkerBegin(
    uint32      cmdBufferId,
    uint32      markerValue,
    const char* pMarkerName,
    uint32      markerNameSize)
{
    Result result = Result::Success;

    EventData eventData              = { };
    eventData.eventId                = EventId::ExecutionMarkerTop;
    eventData.cmdBufferId            = cmdBufferId;
    eventData.markerValue            = markerValue;

    if ((markerNameSize == 0) || (pMarkerName == nullptr))
    {
        // If an invalid marker name is provided, reflect that with an invalid handle.
        eventData.markerNameHandle = Util::StringBagHandle<char>();
        PAL_ASSERT(eventData.markerNameHandle.IsValid() == false);
    }
    else
    {
        // Otherwise, store the marker name in the string bag and cache the handle.
        eventData.markerNameHandle = m_markerNameBag.PushBack(pMarkerName,
                                                              markerNameSize,
                                                              &result);
    }

    if (result == Result::Success)
    {
        result = m_eventCache.PushBack(eventData);
    }

    return result;
}

// =====================================================================================================================
// Serializes an ExecutionMarkerBottom event into the event cache.
Result EventCache::CacheExecutionMarkerEnd(
    uint32      cmdBufferId,
    uint32      markerValue)
{
    EventData eventData        = { };
    eventData.eventId          = EventId::ExecutionMarkerBottom;
    eventData.cmdBufferId      = cmdBufferId;
    eventData.markerValue      = markerValue;
    eventData.markerNameHandle = Util::StringBagHandle<char>();
    PAL_ASSERT(eventData.markerNameHandle.IsValid() == false);

    return m_eventCache.PushBack(eventData);
}

// =====================================================================================================================
Result EventCache::GetEventAt(
    uint32                           index,
    UmdCrashAnalysisEvents::EventId* pEventId,
    uint32*                          pCmdBufferId,
    uint32*                          pMarkerValue,
    const char**                     ppMarkerName,
    uint32*                          pMarkerNameSize) const
{
    Result result = Result::ErrorUnknown;

    if (index < m_eventCache.NumElements())
    {
        const EventData& eventData = m_eventCache.At(index);

        if ((pEventId        != nullptr) &&
            (pCmdBufferId    != nullptr) &&
            (pMarkerValue    != nullptr) &&
            (ppMarkerName    != nullptr) &&
            (pMarkerNameSize != nullptr))
        {
            (*pEventId)        = eventData.eventId;
            (*pCmdBufferId)    = eventData.cmdBufferId;
            (*pMarkerValue)    = eventData.markerValue;

            if ((eventData.eventId == EventId::ExecutionMarkerTop) &&
                (eventData.markerNameHandle.IsValid()))
            {
                Util::StringView<char> markerName = m_markerNameBag.At(eventData.markerNameHandle);
                (*ppMarkerName)                   = markerName.Data();
                (*pMarkerNameSize)                = markerName.Length();
            }
            else
            {
                (*ppMarkerName)    = nullptr;
                (*pMarkerNameSize) = 0;
            }

            result = Result::Success;
        }
    }

    return result;
}

} // namespace CrashAnalysis
} // namespace Pal
