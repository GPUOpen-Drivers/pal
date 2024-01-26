/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "palAssert.h"
#include "core/eventDefs.h"
#include "core/device.h"
#include "core/hw/gfxip/gfxDevice.h"
#include "core/hw/gfxip/pm4IndirectCmdGenerator.h"

namespace Pal
{
namespace Pm4
{

constexpr gpusize GpuMemoryAlignment = sizeof(uint32);

// =====================================================================================================================
Result IndirectCmdGenerator::ValidateCreateInfo(
    const IndirectCmdGeneratorCreateInfo& createInfo)
{
    PAL_ASSERT(createInfo.paramCount >= 1);

    Result result = Result::Success;

    // The type of draw or dispatch is always determined by the final command parameter.
    const IndirectParamType drawType    = createInfo.pParams[createInfo.paramCount - 1].type;
    size_t                  minimumSize = createInfo.pParams[createInfo.paramCount - 1].sizeInBytes;

    if ((drawType != IndirectParamType::Draw)         &&
        (drawType != IndirectParamType::DrawIndexed)  &&
        (drawType != IndirectParamType::DispatchMesh) &&
        (drawType != IndirectParamType::Dispatch))
    {
        result = Result::ErrorInvalidValue;
    }
    else
    {
        for (uint32 param = 0; param < (createInfo.paramCount - 1); ++param)
        {
            minimumSize += createInfo.pParams[param].sizeInBytes;

            switch (createInfo.pParams[param].type)
            {
            case IndirectParamType::Draw:
            case IndirectParamType::DrawIndexed:
            case IndirectParamType::DispatchMesh:
            case IndirectParamType::Dispatch:
                // These must only appear as the final command parameter!
                result = Result::ErrorInvalidValue;
                break;
            case IndirectParamType::BindIndexData:
                if (drawType != IndirectParamType::DrawIndexed)
                {
                    // BindIndexData is only allowed for commands which issue an indexed draw!
                    result = Result::ErrorInvalidValue;
                }
                break;
            case IndirectParamType::BindVertexData:
                if (drawType == IndirectParamType::Dispatch)
                {
                    // BindVertexData is only allowed for commands which issue a draw!
                    result = Result::ErrorInvalidValue;
                }
                break;
            default:
                break;
            }
        }
    }

    if (minimumSize > createInfo.strideInBytes)
    {
        // The per-command byte stride is not large enough to fit all of the specified parameters!
        result = Result::ErrorInvalidValue;
    }

    return result;
}

// =====================================================================================================================
// Helper function to determine the type of indirect command generator given the creation info.
static GeneratorType DetermineGeneratorType(
    const IndirectCmdGeneratorCreateInfo& createInfo)
{
    GeneratorType type = GeneratorType::Dispatch;

    switch (createInfo.pParams[createInfo.paramCount - 1].type)
    {
    case IndirectParamType::Dispatch:
        type = GeneratorType::Dispatch;
        break;
    case IndirectParamType::DispatchMesh:
        type = GeneratorType::DispatchMesh;
        break;
    case IndirectParamType::Draw:
        type = GeneratorType::Draw;
        break;
    case IndirectParamType::DrawIndexed:
        type = GeneratorType::DrawIndexed;
        break;
    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    return type;
}

// =====================================================================================================================
IndirectCmdGenerator::IndirectCmdGenerator(
    const GfxDevice&                      device,
    const IndirectCmdGeneratorCreateInfo& createInfo)
    :
    m_device(device),
    m_gpuMemory(),
    m_gpuMemSize(0),
    m_type(DetermineGeneratorType(createInfo)),
    m_paramCount(createInfo.paramCount)
{
    memset(&m_properties, 0, sizeof(m_properties));
    m_properties.gfxLevel = m_device.Parent()->ChipProperties().gfxLevel;

    memset(&m_propertiesSrd[0], 0, sizeof(m_propertiesSrd));
    memset(&m_paramBufSrd[0], 0, sizeof(m_paramBufSrd));
    memset(&m_touchedUserData[0], 0, sizeof(m_touchedUserData));

    ResourceCreateEventData data = {};
    data.type = ResourceType::IndirectCmdGenerator;
    data.pResourceDescData = nullptr;
    data.resourceDescSize = 0;
    data.pObj = this;
    m_device.GetPlatform()->GetGpuMemoryEventProvider()->LogGpuMemoryResourceCreateEvent(data);
}

// =====================================================================================================================
void IndirectCmdGenerator::Destroy()
{
    ResourceDestroyEventData data = {};
    data.pObj = this;
    m_device.GetPlatform()->GetGpuMemoryEventProvider()->LogGpuMemoryResourceDestroyEvent(data);

    this->~IndirectCmdGenerator();
}

// =====================================================================================================================
void IndirectCmdGenerator::GetGpuMemoryRequirements(
    GpuMemoryRequirements* pGpuMemReqs
    ) const
{
    memset(pGpuMemReqs, 0, sizeof(GpuMemoryRequirements));

    pGpuMemReqs->alignment = GpuMemoryAlignment;
    pGpuMemReqs->size      = m_gpuMemSize;

    pGpuMemReqs->flags.u32All    = 0;
    pGpuMemReqs->flags.cpuAccess = 1;

    pGpuMemReqs->heapCount = 2;
    pGpuMemReqs->heaps[0]  = GpuHeap::GpuHeapGartUswc;
    pGpuMemReqs->heaps[1]  = GpuHeap::GpuHeapGartCacheable;
}

// =====================================================================================================================
Result IndirectCmdGenerator::BindGpuMemory(
    IGpuMemory* pGpuMemory,
    gpusize     offset)
{
    Result result = Device::ValidateBindObjectMemoryInput(pGpuMemory,
                                                          offset,
                                                          m_gpuMemSize,
                                                          GpuMemoryAlignment);
    if (result == Result::Success)
    {
        m_gpuMemory.Update(pGpuMemory, offset);

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
        m_device.Parent()->DeveloperCb(Developer::CallbackType::BindGpuMemory, &callbackData);
    }

    return result;
}

} // Pup
} // Pal
