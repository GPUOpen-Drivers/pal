/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2017 Advanced Micro Devices, Inc. All Rights Reserved.
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

static constexpr gpusize GpuRequiredMemSizeInBytes = 4;
static constexpr gpusize GpuRequiredMemAlignment   = 8;

// =====================================================================================================================
GpuEvent::GpuEvent(
    const GpuEventCreateInfo& createInfo,
    Device*                   pDevice)
    :
    m_createInfo(createInfo),
    m_pDevice(pDevice),
    m_pEventData(nullptr)
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

        const Result freeResult = m_pDevice->MemMgr()->FreeGpuMem(m_gpuMemory.Memory(), m_gpuMemory.Offset());
        PAL_ASSERT(freeResult == Result::Success);
    }
}

// =====================================================================================================================
Result GpuEvent::Init()
{
    const bool cpuVisible = (m_createInfo.flags.gpuAccessOnly == 0);

    GpuMemoryCreateInfo createInfo = {};
    createInfo.size      = GpuRequiredMemSizeInBytes;
    createInfo.alignment = GpuRequiredMemAlignment;
    createInfo.vaRange   = VaRange::Default;
    createInfo.priority  = GpuMemPriority::Normal;
    createInfo.heapCount = 1;
    createInfo.heaps[0]  = cpuVisible ? GpuHeapGartCacheable : GpuHeapInvisible;

    GpuMemoryInternalCreateInfo internalInfo = {};
    internalInfo.flags.alwaysResident = 1;

#if PAL_BUILD_GFX
    // If queues read/write through caches we can deadlock GPU events by ignoring CPU writes.
    if (m_pDevice->ChipProperties().gfxip.queuesUseCaches)
    {
        internalInfo.mtype = MType::Uncached;
    }
#endif

    GpuMemory* pMemory = nullptr;
    gpusize    offset  = 0;
    Result     result  = m_pDevice->MemMgr()->AllocateGpuMem(createInfo, internalInfo, false, &pMemory, &offset);

    if (result == Result::Success)
    {
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
    return CpuWrite(SetValue);
}

// =====================================================================================================================
// Puts the event into the "reset" state from the CPU.
// NOTE: Part of the public IGpuEvent interface.
Result GpuEvent::Reset()
{
    return CpuWrite(ResetValue);
}

// =====================================================================================================================
// Gets the status (set or reset) of the event.
// NOTE: Part of the public IGpuEvent interface.
Result GpuEvent::GetStatus()
{
    PAL_ALERT(m_pDevice->Settings().ifh != IfhModeDisabled);

    Result result = Result::ErrorInvalidPointer;

    if (m_pEventData != nullptr)
    {
        // We should only peek at the event data once; if we read from it multiple times the GPU could get in and
        // change the value when we don't expect it to change.
        const uint32 eventValue = *m_pEventData;

        switch (eventValue)
        {
        case SetValue:
            result = Result::EventSet;
            break;

        case ResetValue:
            result = Result::EventReset;
            break;

        default:
            // A GFX6 hardware bug workaround can result in the event memory temporarily being written to a value
            // other than SetValue or ResetValue. Treat this the same as being in the reset state.
            result = Result::EventReset;
            break;
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
        result        = Result::Success;
    }

    return result;
}

} // Pal
