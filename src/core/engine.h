/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/device.h"
#include "core/platform.h"
#include "core/queue.h"
#include "core/queueContext.h"
#include "palList.h"
#include "palListImpl.h"

namespace Pal
{

// =====================================================================================================================
class Engine
{
public:
    Engine(
        const Device& device,
        EngineType    type,
        uint32        index);
    virtual ~Engine() {}

    virtual Result Init();

    Result AddQueue(Queue* pQueue);
    void RemoveQueue(Queue* pQueue);

    Result WaitIdleAllQueues();

    EngineType Type() const { return m_type; }

protected:
    const Device&                m_device;
    EngineType                   m_type;
    uint32                       m_index;

    Util::List<Queue*, Platform> m_queues;
    Util::Mutex                  m_queueLock;

    QueueContext*                m_pContext;

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(Engine);
    PAL_DISALLOW_DEFAULT_CTOR(Engine);
};
} // Pal
