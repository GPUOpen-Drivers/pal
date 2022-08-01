/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/eventDefs.h"
#include "g_coreSettings.h"
#include "core/gpuEvent.h"
#include "palAssert.h"

namespace Pal
{

static constexpr gpusize  GpuRequiredMemSizePerSlotInBytes = 4;
static constexpr gpusize  GpuRequiredMemAlignment          = 8;
static constexpr uint32_t NumSlotsPerEvent                 = 1; // The current implementation only supports 1 event.

// =====================================================================================================================
GpuEvent::GpuEvent(
    const GpuEventCreateInfo& createInfo,
    Device*                   pDevice)
    :
    m_createInfo(createInfo),
    m_pDevice(pDevice),
    m_pEventData(nullptr)
{
    ResourceDescriptionGpuEvent desc = {};
    desc.pCreateInfo = &m_createInfo;

    ResourceCreateEventData data = {};
    data.type = ResourceType::GpuEvent;
    data.pResourceDescData = static_cast<void*>(&desc);
    data.resourceDescSize = sizeof(ResourceDescriptionGpuEvent);
    data.pObj = this;
    m_pDevice->GetPlatform()->GetEventProvider()->LogGpuMemoryResourceCreateEvent(data);
}

// =====================================================================================================================
GpuEvent::~GpuEvent()
{
    ResourceDestroyEventData data = {};
    data.pObj = this;
    m_pDevice->GetPlatform()->GetEventProvider()->LogGpuMemoryResourceDestroyEvent(data);

    if (m_gpuMemory.IsBound())
    {
        if (m_createInfo.flags.gpuAccessOnly == 0)
        {
            const Result unmapResult = m_gpuMemory.Unmap();
            PAL_ASSERT(unmapResult == Result::Success);
        }
    }
}

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

    return CpuWrite(SetValue);
}

// =====================================================================================================================
// Puts the event into the "reset" state from the CPU.
// NOTE: Part of the public IGpuEvent interface.
Result GpuEvent::Reset()
{
    PAL_ASSERT(m_createInfo.flags.gpuAccessOnly == 0);

    return CpuWrite(ResetValue);
}

// =====================================================================================================================
// Gets the status (set or reset) of the event.
// NOTE: Part of the public IGpuEvent interface.
Result GpuEvent::GetStatus()
{
    PAL_ASSERT(m_createInfo.flags.gpuAccessOnly == 0);
    PAL_ALERT(m_pDevice->GetIfhMode() != IfhModeDisabled);

    Result result = Result::ErrorInvalidPointer;

    if (m_pEventData != nullptr)
    {
        result = Result::EventSet;

        // We should only peek at the event data once; if we read from it multiple times the GPU could get in and
        // change the value when we don't expect it to change.
        const uint32 eventValue = *m_pEventData;

        if (eventValue == ResetValue)
        {
            result = Result::EventReset;
        }
        else if (eventValue != SetValue)
        {
            // A GFX6 hardware bug workaround can result in the event memory temporarily being written to a value
            // other than SetValue or ResetValue. Treat this the same as being in the reset state.
            result = Result::EventReset;
        }
    }

    return result;
}

// =====================================================================================================================
// Helper function to execute the memory map and CPU-side write to the GPU memory.
Result GpuEvent::CpuWrite(
    uint32 data)
{
    Result result = Result::ErrorInvalidPointer;

    if (m_pEventData != nullptr)
    {
        *m_pEventData = data;
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

    pGpuMemReqs->size      = GpuRequiredMemSizePerSlotInBytes * NumSlotsPerEvent;
    pGpuMemReqs->alignment = GpuRequiredMemAlignment;
    pGpuMemReqs->flags.u32All = 0;
    pGpuMemReqs->flags.cpuAccess = (m_createInfo.flags.gpuAccessOnly == 0);

    const bool haveInvisibleMem = (m_pDevice->MemoryProperties().invisibleHeapSize > 0);
    if (haveInvisibleMem && (m_createInfo.flags.gpuAccessOnly == 1))
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
    const gpusize gpuRequiredMemSizeInBytes = GpuRequiredMemSizePerSlotInBytes * NumSlotsPerEvent;

    Result result = Device::ValidateBindObjectMemoryInput(pGpuMemory,
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

        GpuMemoryResourceBindEventData data = {};
        data.pObj = this;
        data.pGpuMemory = pGpuMemory;
        data.requiredGpuMemSize = gpuRequiredMemSizeInBytes;
        data.offset = offset;
        m_pDevice->GetPlatform()->GetEventProvider()->LogGpuMemoryResourceBindEvent(data);
    }

    return result;
}

} // Pal
