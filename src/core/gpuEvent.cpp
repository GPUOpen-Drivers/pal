/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/g_palSettings.h"
#include "core/gpuEvent.h"
#include "palAssert.h"

namespace Pal
{

static constexpr gpusize GpuRequiredMemSizePerSlotInBytes = 4;
static constexpr gpusize GpuRequiredMemAlignment          = 8;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 474
// =====================================================================================================================
Result GpuEvent::CreateInternal(
    Device*                   pDevice,
    const GpuEventCreateInfo& createInfo,
    void*                     pMemory,
    GpuEvent**                ppEvent)
{
    (*ppEvent) = PAL_PLACEMENT_NEW(pMemory) GpuEvent(createInfo, pDevice);

    // Intentionally do not call Init() here, since we don't want the init method to allocate GPU memory!  It will be
    // managed by the caller using BindGpuMemory().

    return Result::Success;
}
#endif

// =====================================================================================================================
GpuEvent::GpuEvent(
    const GpuEventCreateInfo& createInfo,
    Device*                   pDevice)
    :
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 474
    m_ownsGpuMemory(false),
#endif
    m_createInfo(createInfo),
    m_pDevice(pDevice),
    m_pEventData(nullptr),
    m_numSlotsPerEvent(pDevice->ChipProperties().gfxip.numSlotsPerEvent)
{
}

// =====================================================================================================================
GpuEvent::~GpuEvent()
{
    if (m_gpuMemory.IsBound())
    {
        if (m_createInfo.flags.gpuAccessOnly == 0)
        {
            const Result unmapResult = m_gpuMemory.Unmap();
            PAL_ASSERT(unmapResult == Result::Success);
        }

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 474
        if (m_ownsGpuMemory)
        {
            const Result freeResult = m_pDevice->MemMgr()->FreeGpuMem(m_gpuMemory.Memory(), m_gpuMemory.Offset());
            PAL_ASSERT(freeResult == Result::Success);
        }
#endif
    }
}

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 474
// =====================================================================================================================
Result GpuEvent::Init()
{
    const bool    cpuVisible                = (m_createInfo.flags.gpuAccessOnly == 0);
    const gpusize gpuRequiredMemSizeInBytes = GpuRequiredMemSizePerSlotInBytes * m_numSlotsPerEvent;

    GpuMemoryCreateInfo createInfo = {};
    createInfo.size      = gpuRequiredMemSizeInBytes;
    createInfo.alignment = GpuRequiredMemAlignment;
    createInfo.vaRange   = VaRange::Default;
    createInfo.priority  = GpuMemPriority::Normal;
    createInfo.heapCount = 1;
    createInfo.heaps[0]  = cpuVisible ? GpuHeapGartCacheable : GpuHeapInvisible;

    GpuMemoryInternalCreateInfo internalInfo = {};
    internalInfo.flags.alwaysResident = 1;

#if PAL_BUILD_GFX
    // If queues read/write through caches we can deadlock GPU events by ignoring CPU writes.
    if (m_pDevice->ChipProperties().gfxip.supportGl2Uncached && cpuVisible)
    {
        internalInfo.mtype = MType::Uncached;
    }
#endif

    GpuMemory* pMemory = nullptr;
    gpusize    offset  = 0;
    Result     result  = m_pDevice->MemMgr()->AllocateGpuMem(createInfo, internalInfo, false, &pMemory, &offset);

    if (result == Result::Success)
    {
        m_ownsGpuMemory = true;
        m_gpuMemory.Update(pMemory, offset);

        // If the event is CPU-visible we keep it mapped and initialize it to the reset state.
        if (cpuVisible)
        {
            void* pCpuAddr = nullptr;
            result = m_gpuMemory.Map(&pCpuAddr);

            if (result == Result::Success)
            {
                m_pEventData = static_cast<uint32*>(pCpuAddr);
                result       = Reset();
            }
        }
    }

    return result;
}
#endif

// =====================================================================================================================
// Destroys this GpuEvent object. Clients are responsible for freeing the system memory the object occupies.
// NOTE: Part of the public IDestroyable interface.
void GpuEvent::Destroy()
{
    this->~GpuEvent();
}

// =====================================================================================================================
// Puts the event into the "set" state from the CPU.
// NOTE: Part of the public IGpuEvent interface.
Result GpuEvent::Set()
{
    PAL_ASSERT(m_createInfo.flags.gpuAccessOnly == 0);

    Result result = Result::Success;
    for (uint32 slotIdx = 0; (result == Result::Success) && (slotIdx < m_numSlotsPerEvent); slotIdx++)
    {
        result = CpuWrite(slotIdx, SetValue);
    }
    return result;
}

// =====================================================================================================================
// Puts the event into the "reset" state from the CPU.
// NOTE: Part of the public IGpuEvent interface.
Result GpuEvent::Reset()
{
    PAL_ASSERT(m_createInfo.flags.gpuAccessOnly == 0);

    Result result = Result::Success;
    for (uint32 slotIdx = 0; (result == Result::Success) && (slotIdx < m_numSlotsPerEvent); slotIdx++)
    {
        result = CpuWrite(slotIdx, ResetValue);
    }
    return result;
}

// =====================================================================================================================
// Gets the status (set or reset) of the event.
// NOTE: Part of the public IGpuEvent interface.
Result GpuEvent::GetStatus()
{
    PAL_ASSERT(m_createInfo.flags.gpuAccessOnly == 0);
    PAL_ALERT(m_pDevice->Settings().ifh != IfhModeDisabled);

    Result result = Result::ErrorInvalidPointer;

    if (m_pEventData != nullptr)
    {
        result = Result::EventSet;

        // Treat event as reset if a single slot is reset, treat as set only if all slots are set.
        for (uint32 slotIdx = 0; slotIdx < m_numSlotsPerEvent; slotIdx++)
        {
            // We should only peek at the event data once; if we read from it multiple times the GPU could get in and
            // change the value when we don't expect it to change.
            const uint32 eventValue = *(m_pEventData + slotIdx);

            if (eventValue == ResetValue)
            {
                result = Result::EventReset;
                break;
            }
            else if (eventValue != SetValue)
            {
                // A GFX6 hardware bug workaround can result in the event memory temporarily being written to a value
                // other than SetValue or ResetValue. Treat this the same as being in the reset state.
                result = Result::EventReset;
                break;
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Helper function to execute the memory map and CPU-side write to the GPU memory.
Result GpuEvent::CpuWrite(
    uint32 slotIdx,
    uint32 data)
{
    PAL_ASSERT(slotIdx < m_numSlotsPerEvent);

    Result result = Result::ErrorInvalidPointer;

    if (m_pEventData != nullptr)
    {
        *(m_pEventData + slotIdx) = data;
        result = Result::Success;
    }

    return result;
}

// =====================================================================================================================
// Specifies requirements for GPU memory a client must bind to the object before using it: size, alignment, and heaps.
// NOTE: Part of the public IGpuMemoryBindable interface.
void GpuEvent::GetGpuMemoryRequirements(
    GpuMemoryRequirements* pGpuMemReqs
    ) const
{
    pGpuMemReqs->size      = GpuRequiredMemSizePerSlotInBytes * m_numSlotsPerEvent;
    pGpuMemReqs->alignment = GpuRequiredMemAlignment;

    if (m_createInfo.flags.gpuAccessOnly == 1)
    {
        pGpuMemReqs->heapCount = 4;
        pGpuMemReqs->heaps[0]  = GpuHeapInvisible;
        pGpuMemReqs->heaps[1]  = GpuHeapLocal;
        pGpuMemReqs->heaps[2]  = GpuHeapGartUswc;
        pGpuMemReqs->heaps[3]  = GpuHeapGartCacheable;
    }
    else
    {
        pGpuMemReqs->heapCount = 3;
        pGpuMemReqs->heaps[0]  = GpuHeapLocal;
        pGpuMemReqs->heaps[1]  = GpuHeapGartUswc;
        pGpuMemReqs->heaps[2]  = GpuHeapGartCacheable;
    }
}

// =====================================================================================================================
// Binds a block of GPU memory to this object.
// NOTE: Part of the public IGpuMemoryBindable interface.
Result GpuEvent::BindGpuMemory(
    IGpuMemory* pGpuMemory,
    gpusize     offset)
{
    const gpusize gpuRequiredMemSizeInBytes = GpuRequiredMemSizePerSlotInBytes * m_numSlotsPerEvent;

    Result result = m_pDevice->ValidateBindObjectMemoryInput(pGpuMemory,
                                                             offset,
                                                             gpuRequiredMemSizeInBytes,
                                                             GpuRequiredMemAlignment,
                                                             false);

    // First try to unmap currently bound GPU memory if it is CPU-accessable memory.
    if (result == Result::Success)
    {
        if (m_gpuMemory.IsBound() && (m_createInfo.flags.gpuAccessOnly == 0))
        {
            result = m_gpuMemory.Unmap();
        }
    }

    // Then bind the new GPU memory.
    if (result == Result::Success)
    {
        m_gpuMemory.Update(pGpuMemory, offset);

        if (m_gpuMemory.IsBound() && (m_createInfo.flags.gpuAccessOnly == 0))
        {
            // The backwards compatibility code assumes the GPU memory is mappable which must be the case because:
            // 1. GetGpuMemoryRequirements requires GART cacheable.
            // 2. The old code unconditionally called Reset which maps the memory.
            void* pCpuAddr = nullptr;
            result = m_gpuMemory.Map(&pCpuAddr);

            if (result == Result::Success)
            {
                m_pEventData = static_cast<uint32*>(pCpuAddr);
                result       = Reset();
            }
        }
    }

    return result;
}

} // Pal
