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

#include "core/device.h"
#include "core/queue.h"
#include "core/queueContext.h"
#include "palAssert.h"

using namespace Util;

namespace Pal
{

// =====================================================================================================================
QueueContext::~QueueContext()
{
    if (m_waitForIdleTs.IsBound())
    {
        m_waitForIdleTs.Update(nullptr, 0);

        // We assume we allocated this timestamp together with the exclusive exec TS.
        PAL_ASSERT(m_exclusiveExecTs.IsBound());

        if ((m_pDevice->GetPlatform() != nullptr) && (m_pDevice->GetPlatform()->GetGpuMemoryEventProvider() != nullptr))
        {
            ResourceDestroyEventData destroyData = {};
            destroyData.pObj = &m_waitForIdleTs;
            m_pDevice->GetPlatform()->GetGpuMemoryEventProvider()->LogGpuMemoryResourceDestroyEvent(destroyData);
        }
    }

    if (m_exclusiveExecTs.IsBound())
    {
        m_pDevice->MemMgr()->FreeGpuMem(m_exclusiveExecTs.Memory(), m_exclusiveExecTs.Offset());
        m_exclusiveExecTs.Update(nullptr, 0);

        if ((m_pDevice->GetPlatform() != nullptr) && (m_pDevice->GetPlatform()->GetGpuMemoryEventProvider() != nullptr))
        {
            ResourceDestroyEventData destroyData = {};
            destroyData.pObj = &m_exclusiveExecTs;
            m_pDevice->GetPlatform()->GetGpuMemoryEventProvider()->LogGpuMemoryResourceDestroyEvent(destroyData);
        }
    }
}

// =====================================================================================================================
// Initializes the queue context submission info describing the submission preamble, postamble and paging fence value.
Result QueueContext::PreProcessSubmit(
    InternalSubmitInfo*       pSubmitInfo,
    uint32                    cmdBufferCount,
    const  ICmdBuffer* const* ppCmdBuffers)
{
    pSubmitInfo->numPreambleCmdStreams  = 0;
    pSubmitInfo->numPostambleCmdStreams = 0;
    pSubmitInfo->pagingFence            = 0;

    return Result::Success;
}

// =====================================================================================================================
// Suballocates any timestamp memory needed by our subclasses. The memory is mapped and initialized to zero.
Result QueueContext::CreateTimestampMem(
    bool needWaitForIdleMem)
{
    // We always allocate the exclusive exec timestamp but might not need the wait-for-idle timestamp.
    GpuMemoryCreateInfo createInfo = { };
    createInfo.alignment = sizeof(uint32);
    createInfo.size      = needWaitForIdleMem ? sizeof(uint64) : sizeof(uint32);
    createInfo.priority  = GpuMemPriority::Normal;
    createInfo.vaRange   = VaRange::Default;
    createInfo.heaps[0]  = GpuHeap::GpuHeapLocal;
    createInfo.heaps[1]  = GpuHeap::GpuHeapGartUswc;
    createInfo.heapCount = 2;

    GpuMemoryInternalCreateInfo internalInfo = { };
    internalInfo.flags.alwaysResident = 1;

    GpuMemory* pGpuMemory = nullptr;
    gpusize    offset     = 0;
    Result     result     = m_pDevice->MemMgr()->AllocateGpuMem(createInfo, internalInfo, false, &pGpuMemory, &offset);

    if (result == Result::Success)
    {
        m_exclusiveExecTs.Update(pGpuMemory, offset);

        if (needWaitForIdleMem)
        {
            m_waitForIdleTs.Update(pGpuMemory, offset + sizeof(uint32));
        }

        if ((m_pDevice->GetPlatform() != nullptr) && (m_pDevice->GetPlatform()->GetGpuMemoryEventProvider() != nullptr))
        {
            ResourceCreateEventData createData = {};
            createData.type = ResourceType::Timestamp;
            createData.pObj = &m_exclusiveExecTs;
            createData.pResourceDescData = nullptr;
            createData.resourceDescSize = 0;

            m_pDevice->GetPlatform()->GetGpuMemoryEventProvider()->LogGpuMemoryResourceCreateEvent(createData);

            GpuMemoryResourceBindEventData bindData = {};
            bindData.pGpuMemory = pGpuMemory;
            bindData.pObj = &m_exclusiveExecTs;
            bindData.offset = offset;
            bindData.requiredGpuMemSize = sizeof(uint32);
            m_pDevice->GetPlatform()->GetGpuMemoryEventProvider()->LogGpuMemoryResourceBindEvent(bindData);

            Developer::BindGpuMemoryData callbackData = {};
            callbackData.pObj               = bindData.pObj;
            callbackData.requiredGpuMemSize = bindData.requiredGpuMemSize;
            callbackData.pGpuMemory         = bindData.pGpuMemory;
            callbackData.offset             = bindData.offset;
            callbackData.isSystemMemory     = bindData.isSystemMemory;
            m_pDevice->DeveloperCb(Developer::CallbackType::BindGpuMemory, &callbackData);

            if (needWaitForIdleMem)
            {
                createData.pObj = &m_waitForIdleTs;
                m_pDevice->GetPlatform()->GetGpuMemoryEventProvider()->LogGpuMemoryResourceCreateEvent(createData);

                bindData.offset = offset + sizeof(uint32);
                bindData.pObj = &m_waitForIdleTs;
                m_pDevice->GetPlatform()->GetGpuMemoryEventProvider()->LogGpuMemoryResourceBindEvent(bindData);

                callbackData.offset = bindData.offset;
                callbackData.pObj   = bindData.pObj;
                m_pDevice->DeveloperCb(Developer::CallbackType::BindGpuMemory, &callbackData);
            }

        }

        void* pPtr = nullptr;
        result     = m_exclusiveExecTs.Map(&pPtr);

        if (result == Result::Success)
        {
            if (needWaitForIdleMem)
            {
                *static_cast<uint64*>(pPtr) = 0;
            }
            else
            {
                *static_cast<uint32*>(pPtr) = 0;
            }

            result = m_exclusiveExecTs.Unmap();
        }
    }

    return result;
}

} // Pal
