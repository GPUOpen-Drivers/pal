/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "palCmdBuffer.h"
#include "palDequeImpl.h"
#include "palGpuEvent.h"
#include "palGpuEventPool.h"
#include "palLinearAllocator.h"

namespace GpuUtil
{

// =====================================================================================================================
template <typename PlatformAllocator, typename GpuEventAllocator>
GpuEventPool<PlatformAllocator, GpuEventAllocator>::GpuEventPool(
    Pal::IDevice*      pDevice,
    PlatformAllocator* pPlatformAllocator,
    GpuEventAllocator* pAllocator)
    :
    m_pDevice(pDevice),
    m_pAllocator(pAllocator),
    m_freeEventList(pPlatformAllocator),
    m_globalEventList(pPlatformAllocator)
{
}

// =====================================================================================================================
template <typename PlatformAllocator, typename GpuEventAllocator>
GpuEventPool<PlatformAllocator, GpuEventAllocator>::~GpuEventPool()
{
    DXC_ASSERT(m_freeEventList.NumElements() == m_globalEventList.NumElements());

    while (m_freeEventList.NumElements() > 0)
    {
        Pal::IGpuEvent* pEvent = nullptr;
        m_freeEventList.PopFront(&pEvent);
    }
    while (m_globalEventList.NumElements() > 0)
    {
        Pal::IGpuEvent* pEvent = nullptr;
        m_globalEventList.PopFront(&pEvent);
        pEvent->Destroy();
        PAL_SAFE_FREE(pEvent, m_pAllocator);
    }
}

// =====================================================================================================================
// Unmap the backing video memory, free up the system memory of the IGpuEvent objects. And release all the event entries
// from the lists.
template <typename PlatformAllocator, typename GpuEventAllocator>
Result GpuEventPool<PlatformAllocator, GpuEventAllocator>::Reset()
{
    Result result = Result::Success;

    // At this point we expect all allocated GpuEvents have been returned back to m_freeEventList.
    DXC_ASSERT(m_freeEventList.NumElements() == m_globalEventList.NumElements());

    // Some allocators don't require freeing the GpuEvent objects memory here (like VirtualLinearAllocator that will
    // rewind).
    while (m_freeEventList.NumElements() > 0)
    {
        Pal::IGpuEvent* pEvent = nullptr;
        m_freeEventList.PopFront(&pEvent);
    }
    while (m_globalEventList.NumElements() > 0)
    {
        Pal::IGpuEvent* pEvent = nullptr;
        m_globalEventList.PopFront(&pEvent);
        pEvent->Destroy();
        PAL_SAFE_FREE(pEvent, m_pAllocator);
    }

    return result;
}

// =====================================================================================================================
template <typename PlatformAllocator, typename GpuEventAllocator>
Result GpuEventPool<PlatformAllocator, GpuEventAllocator>::GetFreeEvent(
    Pal::ICmdBuffer*      pCmdBuffer,
    Pal::IGpuEvent**const ppEvent)
{
    Result result = Result::Success;

    if (m_freeEventList.NumElements() > 0)
    {
        result = m_freeEventList.PopFront(ppEvent);
    }
    else
    {
        result = CreateNewEvent(pCmdBuffer, ppEvent);
        m_globalEventList.PushBack(*ppEvent);
    }

    return result;
}

// =====================================================================================================================
template <typename PlatformAllocator, typename GpuEventAllocator>
Result GpuEventPool<PlatformAllocator, GpuEventAllocator>::CreateNewEvent(
    Pal::ICmdBuffer*      pCmdBuffer,
    Pal::IGpuEvent**const ppEvent)
{
    Result result = Result::Success;

    // Create gpuEvent for this pool.
    Pal::GpuEventCreateInfo createInfo  = {};
    createInfo.flags.gpuAccessOnly = 1;

    const size_t eventSize = m_pDevice->GetGpuEventSize(createInfo, &result);

    if (result == Result::Success)
    {
        result = Result::ErrorOutOfMemory;
        void* pMemory = PAL_MALLOC(eventSize,
                                   m_pAllocator,
                                   Util::SystemAllocType::AllocObject);

        if (pMemory != nullptr)
        {
            result = m_pDevice->CreateGpuEvent(createInfo, pMemory, ppEvent);

            if (result != Result::Success)
            {
                PAL_SAFE_FREE(pMemory, m_pAllocator);
            }
        }
    }

    // Bind GPU memory to the event.
    if (result == Result::Success)
    {
        result = pCmdBuffer->AllocateAndBindGpuMemToEvent(*ppEvent);
    }

    return result;
}

// =====================================================================================================================
template <typename PlatformAllocator, typename GpuEventAllocator>
Result GpuEventPool<PlatformAllocator, GpuEventAllocator>::ReturnEvent(
    Pal::IGpuEvent* pEvent)
{
    return m_freeEventList.PushBack(pEvent);
}

} //GpuUtil
