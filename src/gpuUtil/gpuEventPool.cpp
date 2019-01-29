/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "palDequeImpl.h"
#include "palGpuEvent.h"
#include "palGpuEventPool.h"

using namespace Pal;

namespace GpuUtil
{

// =====================================================================================================================
GpuEventPool::GpuEventPool(
    IPlatform* pPlatform,
    IDevice*   pDevice)
    :
    m_pPlatform(pPlatform),
    m_pDevice(pDevice),
    m_availableEvents(m_pPlatform),
    m_busyEvents(m_pPlatform)
{
}

// =====================================================================================================================
GpuEventPool::~GpuEventPool()
{
    while (m_busyEvents.NumElements() > 0)
    {
        IGpuEvent* pEvent = nullptr;
        m_busyEvents.PopFront(&pEvent);
        pEvent->Destroy();
        PAL_SAFE_FREE(pEvent, m_pPlatform);
    }
    while (m_availableEvents.NumElements() > 0)
    {
        IGpuEvent* pEvent = nullptr;
        m_availableEvents.PopFront(&pEvent);
        pEvent->Destroy();
        PAL_SAFE_FREE(pEvent, m_pPlatform);
    }
}

// =====================================================================================================================
Result GpuEventPool::Init(
    uint32 defaultCapacity)
{
    // Pre-allocate some gpuEvents for this pool.
    GpuEventCreateInfo createInfo  = {};

    Result result = Result::Success;
    const size_t eventSize = m_pDevice->GetGpuEventSize(createInfo, &result);

    if (result == Result::Success)
    {
        for (uint32 i = 0; i < defaultCapacity; i++)
        {
            result = Result::ErrorOutOfMemory;
            void* pMemory = PAL_MALLOC(eventSize,
                                       m_pPlatform,
                                       Util::SystemAllocType::AllocObject);

            IGpuEvent* pEvent = nullptr;
            if (pMemory != nullptr)
            {
                result = m_pDevice->CreateGpuEvent(createInfo, pMemory, &pEvent);

                if (result == Result::Success)
                {
                    result = m_availableEvents.PushBack(pEvent);
                }
                else
                {
                    PAL_SAFE_FREE(pMemory, m_pPlatform);
                }
            }
        }
    }

    PAL_ASSERT(m_availableEvents.NumElements() == defaultCapacity);

    return result;
}

// =====================================================================================================================
Result GpuEventPool::Reset()
{
    Result result = Result::Success;

    while (m_busyEvents.NumElements() > 0)
    {
        IGpuEvent* pEvent = nullptr;
        m_busyEvents.PopFront(&pEvent);
        pEvent->Reset();
        m_availableEvents.PushBack(pEvent);
    }

    PAL_ASSERT(m_busyEvents.NumElements() == 0);
    return result;
}

// =====================================================================================================================
Result GpuEventPool::AcquireEvent(
    IGpuEvent**const ppEvent)
{
    Result result = Result::Success;

    if (m_availableEvents.NumElements() > 0)
    {
        result = m_availableEvents.PopFront(ppEvent);
    }
    else
    {
        // Create gpuEvent for this pool.
        GpuEventCreateInfo createInfo  = {};

        const size_t eventSize = m_pDevice->GetGpuEventSize(createInfo, &result);

        if (result == Result::Success)
        {
            result = Result::ErrorOutOfMemory;
            void* pMemory = PAL_MALLOC(eventSize,
                                       m_pPlatform,
                                       Util::SystemAllocType::AllocObject);

            if (pMemory != nullptr)
            {
                result = m_pDevice->CreateGpuEvent(createInfo, pMemory, ppEvent);

                if (result != Result::Success)
                {
                    PAL_SAFE_FREE(pMemory, m_pPlatform);
                }
            }
        }
    }

    if (result == Result::Success)
    {
        result = m_busyEvents.PushBack(*ppEvent);
    }

    return result;
}

} //GpuUtil
