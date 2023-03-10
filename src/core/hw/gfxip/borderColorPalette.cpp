/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/borderColorPalette.h"
#include "core/device.h"
#include "palEventDefs.h"

using namespace Util;

namespace Pal
{

// Each border color palette entry consists of a 4-component RGBA float vector.
static constexpr gpusize EntrySize = 4 * sizeof(float);

// =====================================================================================================================
BorderColorPalette::BorderColorPalette(
    const Device&                       device,
    const BorderColorPaletteCreateInfo& createInfo,
    gpusize                             gpuMemAlign)
    :
    m_device(device),
    m_numEntries(createInfo.paletteSize),
    m_gpuMemSize(createInfo.paletteSize * EntrySize),
    m_gpuMemAlignment(gpuMemAlign)
{
    ResourceDescriptionBorderColorPalette desc = {};
    desc.pCreateInfo = &createInfo;
    ResourceCreateEventData data = {};
    data.type = ResourceType::BorderColorPalette;
    data.pResourceDescData = static_cast<void*>(&desc);
    data.resourceDescSize = sizeof(ResourceDescriptionBorderColorPalette);
    data.pObj = this;
    m_device.GetPlatform()->GetGpuMemoryEventProvider()->LogGpuMemoryResourceCreateEvent(data);
}

// =====================================================================================================================
BorderColorPalette::~BorderColorPalette()
{
    ResourceDestroyEventData data = {};
    data.pObj = this;
    m_device.GetPlatform()->GetGpuMemoryEventProvider()->LogGpuMemoryResourceDestroyEvent(data);
}

// =====================================================================================================================
// Specifies requirements for GPU memory a client must bind to this object before using it: size, alignment, and heaps.
void BorderColorPalette::GetGpuMemoryRequirements(
    GpuMemoryRequirements* pGpuMemReqs
    ) const
{
    pGpuMemReqs->size      = m_gpuMemSize;
    pGpuMemReqs->alignment = m_gpuMemAlignment;

    pGpuMemReqs->flags.u32All    = 0;
    pGpuMemReqs->flags.cpuAccess = 1;

    pGpuMemReqs->heapCount = 3;
    pGpuMemReqs->heaps[0]  = GpuHeapLocal;
    pGpuMemReqs->heaps[1]  = GpuHeapGartUswc;
    pGpuMemReqs->heaps[2]  = GpuHeapGartCacheable;
}

// =====================================================================================================================
// Update the specified entries in this palette with the provided color data.
Result BorderColorPalette::Update(
    uint32       firstEntry,
    uint32       entryCount,
    const float* pEntries)
{
    Result result = Result::Success;

    if (firstEntry + entryCount > m_numEntries)
    {
        result = Result::ErrorInvalidValue;
    }
    else if (m_gpuMemory.IsBound() == false)
    {
        result = Result::ErrorGpuMemoryNotBound;
    }
    else
    {
        void* pData = nullptr;
        result = m_gpuMemory.Map(&pData);

        if (result == Result::Success)
        {
            pData = VoidPtrInc(pData, firstEntry * EntrySize);
            memcpy(pData, pEntries, entryCount * EntrySize);

            result = m_gpuMemory.Unmap();
        }
    }

    return result;
}

// =====================================================================================================================
// Binds a block of GPU memory to this object.
// NOTE: Part of the public IGpuMemoryBindable interface.
Result BorderColorPalette::BindGpuMemory(
    IGpuMemory* pGpuMemory,
    gpusize     offset)
{
    Result result = Device::ValidateBindObjectMemoryInput(pGpuMemory, offset, m_gpuMemSize, m_gpuMemAlignment, false);

    if (result == Result::Success)
    {
        m_gpuMemory.Update(pGpuMemory, offset);
        if (m_gpuMemory.IsBound())
        {
            UpdateGpuMemoryBinding(static_cast<gpusize>(m_gpuMemory.GpuVirtAddr()));
        }

        GpuMemoryResourceBindEventData data = {};
        data.pObj = this;
        data.pGpuMemory = pGpuMemory;
        data.requiredGpuMemSize = m_gpuMemSize;
        data.offset = offset;
        m_device.GetPlatform()->GetGpuMemoryEventProvider()->LogGpuMemoryResourceBindEvent(data);

        Developer::BindGpuMemoryData callbackData = {};
        callbackData.pObj               = data.pObj;
        callbackData.requiredGpuMemSize = data.requiredGpuMemSize;
        callbackData.pGpuMemory         = data.pGpuMemory;
        callbackData.offset             = data.offset;
        callbackData.isSystemMemory     = data.isSystemMemory;
        m_device.DeveloperCb(Developer::CallbackType::BindGpuMemory, &callbackData);
    }

    return result;
}

} // Pal
