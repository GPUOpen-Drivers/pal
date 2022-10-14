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
#include "core/platform.h"
#include "core/hw/gfxip/gfxDevice.h"
#include "core/hw/gfxip/graphicsPipeline.h"
#include "palFormatInfo.h"
#include "palMetroHash.h"
#include "palPipelineAbi.h"

using namespace Util;

namespace Pal
{

// =====================================================================================================================
GraphicsPipeline::GraphicsPipeline(
    Device* pDevice,
    bool    isInternal)
    :
    Pal::Pipeline(pDevice, isInternal),
    m_binningOverride(BinningOverride::Default),
    m_vertexBufferCount(0),
    m_numColorTargets(0),
    m_logicOp(LogicOp::Copy)
{
    m_flags.u32All = 0;

    memset(&m_targetSwizzledFormats[0], 0, sizeof(m_targetSwizzledFormats));
    memset(&m_targetWriteMasks[0],      0, sizeof(m_targetWriteMasks));
    memset(&m_viewInstancingDesc,       0, sizeof(m_viewInstancingDesc));
}

// =====================================================================================================================
// Initialize this graphics pipeline based on the provided creation info.
Result GraphicsPipeline::Init(
    const GraphicsPipelineCreateInfo&         createInfo,
    const GraphicsPipelineInternalCreateInfo& internalInfo,
    const AbiReader&                          abiReader,
    const PalAbi::CodeObjectMetadata&         metadata,
    MsgPackReader*                            pMetadataReader)
{
    Result result = Result::Success;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 709
    if ((createInfo.iaState.topologyInfo.topologyIsPolygon == true) &&
        (createInfo.iaState.topologyInfo.primitiveType != Pal::PrimitiveType::Triangle))
    {
        result = Result::ErrorInvalidValue;
    }
    else
#endif
    if ((createInfo.pPipelineBinary != nullptr) && (createInfo.pipelineBinarySize != 0))
    {
        m_pipelineBinaryLen = createInfo.pipelineBinarySize;
        m_pPipelineBinary   = PAL_MALLOC(m_pipelineBinaryLen, m_pDevice->GetPlatform(), AllocInternal);
        if (m_pPipelineBinary == nullptr)
        {
            result = Result::ErrorOutOfMemory;
        }
        else
        {
            memcpy(m_pPipelineBinary, createInfo.pPipelineBinary, m_pipelineBinaryLen);
        }
    }
    else
    {
        result = Result::ErrorInvalidPointer;
    }

    if (result == Result::Success)
    {
        PAL_ASSERT(m_pPipelineBinary != nullptr);
        result = InitFromPipelineBinary(createInfo, internalInfo, abiReader, metadata, pMetadataReader);
    }

    if (result == Result::Success)
    {
        auto*const pEventProvider = m_pDevice->GetPlatform()->GetGpuMemoryEventProvider();

        ResourceDescriptionPipeline desc { };
        desc.pPipelineInfo = &GetInfo();
        desc.pCreateFlags  = &createInfo.flags;

        ResourceCreateEventData data { };
        data.type              = ResourceType::Pipeline;
        data.pResourceDescData = &desc;
        data.resourceDescSize  = sizeof(desc);
        data.pObj              = this;
        pEventProvider->LogGpuMemoryResourceCreateEvent(data);

        GpuMemoryResourceBindEventData bindData { };
        bindData.pObj               = this;
        bindData.pGpuMemory         = m_gpuMem.Memory();
        bindData.requiredGpuMemSize = m_gpuMemSize;
        bindData.offset             = m_gpuMem.Offset();
        pEventProvider->LogGpuMemoryResourceBindEvent(bindData);
    }

    return result;
}

// =====================================================================================================================
// Initializes this pipeline from the pipeline binary data stored in this object, combined with the specified create
// info.
Result GraphicsPipeline::InitFromPipelineBinary(
    const GraphicsPipelineCreateInfo&         createInfo,
    const GraphicsPipelineInternalCreateInfo& internalInfo,
    const AbiReader&                          abiReader,
    const PalAbi::CodeObjectMetadata&         metadata,
    MsgPackReader*                            pMetadataReader)
{
    // Store the ROP code this pipeline was created with
    m_logicOp = createInfo.cbState.logicOp;

    m_depthClampMode = createInfo.rsState.depthClampMode;

    m_flags.perpLineEndCapsEnable = createInfo.rsState.perpLineEndCapsEnable;

    m_flags.fastClearElim    = internalInfo.flags.fastClearElim;
    m_flags.fmaskDecompress  = internalInfo.flags.fmaskDecompress;
    m_flags.dccDecompress    = internalInfo.flags.dccDecompress;
    m_flags.resolveFixedFunc = internalInfo.flags.resolveFixedFunc;

    m_binningOverride = createInfo.rsState.binningOverride;

    m_flags.lateAllocVsLimit = createInfo.useLateAllocVsLimit;
    m_lateAllocVsLimit       = createInfo.lateAllocVsLimit;
    m_vertexBufferCount      = createInfo.iaState.vertexBufferCount;
    for (uint8 i = 0; i < MaxColorTargets; ++i)
    {
        m_targetSwizzledFormats[i] = createInfo.cbState.target[i].swizzledFormat;
        m_targetWriteMasks[i]      = createInfo.cbState.target[i].channelWriteMask;
        if ((Formats::IsUndefined(m_targetSwizzledFormats[i].format) == false) ||
            (m_targetWriteMasks[i] != 0))
        {
            m_numColorTargets = i + 1;
        }
    }

    m_viewInstancingDesc                   = createInfo.viewInstancingDesc;
    m_viewInstancingDesc.viewInstanceCount = Max(m_viewInstancingDesc.viewInstanceCount, 1u);

    ExtractPipelineInfo(metadata, ShaderType::Task, ShaderType::Pixel);

    DumpPipelineElf("PipelineGfx", metadata.pipeline.name);

    if (ShaderHashIsNonzero(m_info.shader[static_cast<uint32>(ShaderType::Geometry)].hash))
    {
        m_flags.gsEnabled = 1;
    }
    if (ShaderHashIsNonzero(m_info.shader[static_cast<uint32>(ShaderType::Hull)].hash) &&
        ShaderHashIsNonzero(m_info.shader[static_cast<uint32>(ShaderType::Domain)].hash))
    {
        m_flags.tessEnabled = 1;
    }

    if (ShaderHashIsNonzero(m_info.shader[static_cast<uint32>(ShaderType::Mesh)].hash))
    {
        m_flags.meshShader = 1;
    }

    if (ShaderHashIsNonzero(m_info.shader[static_cast<uint32>(ShaderType::Task)].hash))
    {
        SetTaskShaderEnabled();
        m_flags.taskShader = 1;
    }
    // A task shader is not allowed unless a mesh shader is also present, but a mesh shader can be present
    // without requiring a task shader.
    PAL_ASSERT(HasMeshShader() || (HasTaskShader() == false));

    m_flags.vportArrayIdx = (metadata.pipeline.flags.usesViewportArrayIndex != 0);

    const auto& psStageMetadata = metadata.pipeline.hardwareStage[static_cast<uint32>(Abi::HardwareStage::Ps)];

    m_flags.psUsesUavs          = (psStageMetadata.flags.usesUavs          != 0);
    m_flags.psUsesRovs          = (psStageMetadata.flags.usesRovs          != 0);
    m_flags.psWritesUavs        = (psStageMetadata.flags.writesUavs        != 0);
    m_flags.psWritesDepth       = (psStageMetadata.flags.writesDepth       != 0);
    m_flags.psUsesAppendConsume = (psStageMetadata.flags.usesAppendConsume != 0);

    if ((metadata.pipeline.hardwareStage[static_cast<uint32>(Abi::HardwareStage::Ls)].flags.writesUavs) ||
        (metadata.pipeline.hardwareStage[static_cast<uint32>(Abi::HardwareStage::Hs)].flags.writesUavs) ||
        (metadata.pipeline.hardwareStage[static_cast<uint32>(Abi::HardwareStage::Es)].flags.writesUavs) ||
        (metadata.pipeline.hardwareStage[static_cast<uint32>(Abi::HardwareStage::Gs)].flags.writesUavs) ||
        (metadata.pipeline.hardwareStage[static_cast<uint32>(Abi::HardwareStage::Vs)].flags.writesUavs))
    {
        m_flags.nonPsShaderWritesUavs = true;
    }

    if (((metadata.pipeline.hardwareStage[static_cast<uint32>(Abi::HardwareStage::Ls)].hasEntry.usesPrimId) &&
            (metadata.pipeline.hardwareStage[static_cast<uint32>(Abi::HardwareStage::Ls)].flags.usesPrimId)) ||
        ((metadata.pipeline.hardwareStage[static_cast<uint32>(Abi::HardwareStage::Hs)].hasEntry.usesPrimId) &&
            (metadata.pipeline.hardwareStage[static_cast<uint32>(Abi::HardwareStage::Hs)].flags.usesPrimId)) ||
        ((metadata.pipeline.hardwareStage[static_cast<uint32>(Abi::HardwareStage::Es)].hasEntry.usesPrimId) &&
            (metadata.pipeline.hardwareStage[static_cast<uint32>(Abi::HardwareStage::Es)].flags.usesPrimId)) ||
        ((metadata.pipeline.hardwareStage[static_cast<uint32>(Abi::HardwareStage::Gs)].hasEntry.usesPrimId) &&
            (metadata.pipeline.hardwareStage[static_cast<uint32>(Abi::HardwareStage::Gs)].flags.usesPrimId)) ||
        ((metadata.pipeline.hardwareStage[static_cast<uint32>(Abi::HardwareStage::Vs)].hasEntry.usesPrimId) &&
            (metadata.pipeline.hardwareStage[static_cast<uint32>(Abi::HardwareStage::Vs)].flags.usesPrimId)) ||
        ((metadata.pipeline.hardwareStage[static_cast<uint32>(Abi::HardwareStage::Ps)].hasEntry.usesPrimId) &&
            (metadata.pipeline.hardwareStage[static_cast<uint32>(Abi::HardwareStage::Ps)].flags.usesPrimId)))
    {
        m_flags.primIdUsed = true;
    }

    Result result = HwlInit(createInfo, abiReader, metadata, pMetadataReader);

    return result;
}

} // Pal
