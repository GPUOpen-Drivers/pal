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

#include "palCmdBuffer.h"
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
    m_pCmdBuffer(nullptr),
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
    ICmdBuffer* pCmdBuffer,
    uint32      defaultCapacity)
{
    // Initialize GPU memory allocator.
    m_pCmdBuffer = pCmdBuffer;

    Result result = Result::Success;

    // Pre-allocate some gpuEvents for this pool.
    for (uint32 i = 0; i < defaultCapacity; i++)
    {
        IGpuEvent* pEvent = nullptr;
        result = CreateNewEvent(&pEvent);

        if (result == Result::Success)
        {
            result = m_availableEvents.PushBack(pEvent);
        }
        else
        {
            break;
        }
    }

    PAL_ASSERT(m_availableEvents.NumElements() == defaultCapacity);

    return result;
}

// =====================================================================================================================
Result GpuEventPool::Reset(
    ICmdBuffer* pCmdBuffer) // We need it in case gpuEventPool's container updates to a new palCmdBuffer at Reset.
{
    Result result = Result::Success;

    // Update to new CmdBuffer.
    m_pCmdBuffer = pCmdBuffer;

    while (m_busyEvents.NumElements() > 0)
    {
        IGpuEvent* pEvent = nullptr;
        m_busyEvents.PopFront(&pEvent);
        m_availableEvents.PushBack(pEvent);
    }

    PAL_ASSERT(m_busyEvents.NumElements() == 0);
    return result;
}

// =====================================================================================================================
Result GpuEventPool::GetFreeEvent(
    IGpuEvent**const ppEvent)
{
    Result result = Result::Success;

    if (m_availableEvents.NumElements() > 0)
    {
        result = m_availableEvents.PopFront(ppEvent);
    }
    else
    {
        result = CreateNewEvent(ppEvent);
    }

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 474
    // Bind GPU memory to the event.
    if (result == Result::Success)
    {
        m_pCmdBuffer->AllocateAndBindGpuMemToEvent(*ppEvent);
    }
#endif

    if (result == Result::Success)
    {
        result = m_busyEvents.PushBack(*ppEvent);
    }

    return result;
}

// =====================================================================================================================
Result GpuEventPool::CreateNewEvent(
    IGpuEvent**const ppEvent)
{
    Result result = Result::Success;

    // Create gpuEvent for this pool.
    GpuEventCreateInfo createInfo  = {};
    createInfo.flags.gpuAccessOnly = 1;

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

    return result;
}

} //GpuUtil
