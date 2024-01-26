/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/engine.h"
#include "palIntrusiveListImpl.h"
#include "palMutex.h"

namespace Pal
{

// =====================================================================================================================
Engine::Engine(
    const Device& device,
    EngineType    type,
    uint32        index)
    :
    m_device(device),
    m_type(type),
    m_index(index),
    m_queues(device.GetPlatform()),
    m_queueLock(),
    m_pContext(nullptr)
{
}

// =====================================================================================================================
Result Engine::Init()
{
    return  Result::Success;
}

// =====================================================================================================================
Result Engine::WaitIdleAllQueues()
{
    Result result = Result::Success;

    // Queue-list operations need to be protected.
    Util::MutexAuto lock(&m_queueLock);

    for (auto iter = m_queues.Begin(); ((iter != m_queues.End()) && (result == Result::Success)); iter.Next())
    {
        result = (*iter.Get())->WaitIdle();
    }

    return result;
}

// =====================================================================================================================
Result Engine::AddQueue(
    Queue* pQueue)
{
    PAL_ASSERT(pQueue != nullptr);

    // Queue-list operations need to be protected.
    Util::MutexAuto lock(&m_queueLock);
    return m_queues.PushBack(pQueue);
}

// =====================================================================================================================
void Engine::RemoveQueue(
    Queue* pQueue)
{
    PAL_ASSERT(pQueue != nullptr);

    // Queue-list operations need to be protected.
    Util::MutexAuto lock(&m_queueLock);
    // Try to find the allocation in the reference list
    for (auto iter = m_queues.Begin(); iter != m_queues.End(); iter.Next())
    {
        Queue* pCurQueuePtr = *iter.Get();

        PAL_ASSERT(pCurQueuePtr != nullptr);

        if (pQueue == pCurQueuePtr)
        {
            m_queues.Erase(&iter);
            break;
        }
    }
}

} // Pal
