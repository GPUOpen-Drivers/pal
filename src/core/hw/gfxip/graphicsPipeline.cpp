/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/graphicsShaderLibrary.h"
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
    m_gfxShaderLibraries{},
    m_numGfxShaderLibraries(0),
    m_logicOp(LogicOp::Copy),
    m_outputNumVertices(0)
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
    const AbiReader*                          pAbiReader,
    const PalAbi::CodeObjectMetadata*         pMetadata,
    MsgPackReader*                            pMetadataReader)
{
    Result result = Result::Success;

    if ((createInfo.iaState.topologyInfo.topologyIsPolygon == true) &&
        (createInfo.iaState.topologyInfo.primitiveType != Pal::PrimitiveType::Triangle))
    {
        result = Result::ErrorInvalidValue;
    }
    else if (createInfo.numShaderLibraries > 0)
    {
        result = InitFromLibraries(createInfo, internalInfo);
    }
    else if ((createInfo.pPipelineBinary != nullptr) && (createInfo.pipelineBinarySize != 0))
    {
        void*  pipelineBinary     = PAL_MALLOC(createInfo.pipelineBinarySize, m_pDevice->GetPlatform(), AllocInternal);
        size_t pipelineBinarySize = (pipelineBinary != nullptr) ? createInfo.pipelineBinarySize : 0;

        m_pipelineBinary = { pipelineBinary, pipelineBinarySize };

        if (m_pipelineBinary.Data() == nullptr)
        {
            result = Result::ErrorOutOfMemory;
        }
        else
        {
            memcpy(m_pipelineBinary.Data(), createInfo.pPipelineBinary, createInfo.pipelineBinarySize);

            result = InitFromPipelineBinary(createInfo, internalInfo, *pAbiReader, *pMetadata, pMetadataReader);
        }
    }
    else if (internalInfo.flags.isPartialPipeline)
    {
        result = InitFromPipelineBinary(createInfo, internalInfo, *pAbiReader, *pMetadata, pMetadataReader);
    }
    else
    {
        result = Result::ErrorInvalidPointer;
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
        bindData.requiredGpuMemSize = m_gpuMemSize      - m_gpuMemOffset;
        bindData.offset             = m_gpuMem.Offset() + m_gpuMemOffset;
        pEventProvider->LogGpuMemoryResourceBindEvent(bindData);

        Developer::BindGpuMemoryData callbackData = {};
        callbackData.pObj               = bindData.pObj;
        callbackData.requiredGpuMemSize = bindData.requiredGpuMemSize;
        callbackData.pGpuMemory         = bindData.pGpuMemory;
        callbackData.offset             = bindData.offset;
        callbackData.isSystemMemory     = bindData.isSystemMemory;
        m_pDevice->DeveloperCb(Developer::CallbackType::BindGpuMemory, &callbackData);
    }

    PAL_ASSERT((HasTaskShader() == false) ||
               Pipeline::DispatchInterleaveSizeIsValid(createInfo.taskInterleaveSize, m_pDevice->ChipProperties()));

    return result;
}

// =====================================================================================================================
// Initialize flags and some common variables from createInfo and internalInfo.
void GraphicsPipeline::InitFlags(
    const GraphicsPipelineCreateInfo&         createInfo,
    const GraphicsPipelineInternalCreateInfo& internalInfo)
{
    // Store the ROP code this pipeline was created with
    m_logicOp = createInfo.cbState.logicOp;

    m_depthClampMode = createInfo.rsState.depthClampMode;

    m_flags.perpLineEndCapsEnable = createInfo.rsState.perpLineEndCapsEnable;

    m_flags.fastClearElim     = internalInfo.flags.fastClearElim;
    m_flags.fmaskDecompress   = internalInfo.flags.fmaskDecompress;
    m_flags.dccDecompress     = internalInfo.flags.dccDecompress;
    m_flags.resolveFixedFunc  = internalInfo.flags.resolveFixedFunc;
    m_flags.isPartialPipeline = internalInfo.flags.isPartialPipeline;
    m_binningOverride         = createInfo.rsState.binningOverride;

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
}

// =====================================================================================================================
// Initialize pipeline from graphics shader libraries.
Result GraphicsPipeline::InitFromLibraries(
    const GraphicsPipelineCreateInfo&         createInfo,
    const GraphicsPipelineInternalCreateInfo& internalInfo)
{
    Result result = Result::Success;
    InitFlags(createInfo, internalInfo);

    Util::MetroHash64 stableHasher;
    Util::MetroHash64 uniqueHasher;
    Util::MetroHash64 resourceHasher;

    Util::MetroHash::Hash stableHash64 = {};
    Util::MetroHash::Hash uniqueHash64 = {};
    Util::MetroHash::Hash resourceHash64 = {};

    uint32 pipelineApiShaderMask = 0;
    uint32 pipelineHwStageMask   = 0;

    PAL_ASSERT((createInfo.numShaderLibraries <= MaxGfxShaderLibraryCount) && (createInfo.numShaderLibraries > 0));
    m_numGfxShaderLibraries = static_cast<uint32>(createInfo.numShaderLibraries);

    // Merge flags and command info from partial pipeline
    for (uint32 i = 0; i < createInfo.numShaderLibraries; i++)
    {
        const GraphicsShaderLibrary* pLib =
            reinterpret_cast<const GraphicsShaderLibrary*>(createInfo.ppShaderLibraries[i]);
        m_gfxShaderLibraries[i] = pLib;

        const GraphicsPipeline*      pPartialPipeline = pLib->GetPartialPipeline();
        uint32                       apiShaderMask    = pLib->GetApiShaderMask();

        if (Util::TestAnyFlagSet(pipelineApiShaderMask, pLib->GetApiShaderMask()) ||
            Util::TestAnyFlagSet(pipelineHwStageMask, pLib->GetHwShaderMask()))
        {
            PAL_NEVER_CALLED();
            result = Result::ErrorBadPipelineData;
            break;
        }
        pipelineApiShaderMask |= apiShaderMask;
        pipelineHwStageMask   |= pLib->GetHwShaderMask();

        // m_flags
        m_flags.gsEnabled             |= pPartialPipeline->m_flags.gsEnabled;
        m_flags.tessEnabled           |= pPartialPipeline->m_flags.tessEnabled;
        m_flags.meshShader            |= pPartialPipeline->m_flags.meshShader;
        m_flags.taskShader            |= pPartialPipeline->m_flags.taskShader;
        m_flags.vportArrayIdx         |= pPartialPipeline->m_flags.vportArrayIdx;
        m_flags.psUsesUavs            |= pPartialPipeline->m_flags.psUsesUavs;
        m_flags.psUsesRovs            |= pPartialPipeline->m_flags.psUsesRovs;
        m_flags.psWritesUavs          |= pPartialPipeline->m_flags.psWritesUavs;
        m_flags.psWritesDepth         |= pPartialPipeline->m_flags.psWritesDepth;

        m_flags.psUsesAppendConsume   |= pPartialPipeline->m_flags.psUsesAppendConsume;
        m_flags.nonPsShaderWritesUavs |= pPartialPipeline->m_flags.nonPsShaderWritesUavs;
        m_flags.primIdUsed            |= pPartialPipeline->m_flags.primIdUsed;
        m_flags.isGsOnchip            |= pPartialPipeline->m_flags.isGsOnchip;
        // Hash
        stableHasher.Update(pPartialPipeline->m_info.internalPipelineHash.stable);
        uniqueHasher.Update(pPartialPipeline->m_info.internalPipelineHash.unique);
        resourceHasher.Update(pPartialPipeline->m_info.resourceMappingHash);

        for (uint32 stage = static_cast<uint32>(ShaderType::Task);
            stage < static_cast<uint32>(ShaderType::Count);
            stage++)
        {
            if (Util::TestAnyFlagSet(apiShaderMask, 1 << stage))
            {
                const uint32 shaderTypeIdx = static_cast<uint32>(
                    PalShaderTypeToAbiShaderType(static_cast<ShaderType>(stage)));

                // Check there isn't api shader stage overlapped among input graphics shader libraries.
                PAL_ASSERT((m_info.shader[stage].hash.lower == 0) && (m_info.shader[stage].hash.upper == 0));

                m_info.shader[stage].hash = pPartialPipeline->m_info.shader[stage].hash;
                m_apiHwMapping.apiShaders[shaderTypeIdx] = pPartialPipeline->m_apiHwMapping.apiShaders[shaderTypeIdx];
            }
        }

        // m_info
        m_info.ps.flags.usesSampleMask |= pPartialPipeline->m_info.ps.flags.usesSampleMask;
        m_info.ps.flags.enablePops |= pPartialPipeline->m_info.ps.flags.enablePops;

        // Uploading fence
        m_uploadFenceToken = Max(m_uploadFenceToken, pPartialPipeline->GetUploadFenceToken());
        m_pagingFenceVal = Max(m_pagingFenceVal, pPartialPipeline->GetPagingFenceVal());
    }

    if (result == Result::Success)
    {
        if (m_flags.taskShader)
        {
            SetTaskShaderEnabled();
        }

        stableHasher.Finalize(stableHash64.bytes);
        uniqueHasher.Finalize(uniqueHash64.bytes);
        resourceHasher.Finalize(resourceHash64.bytes);
        m_info.internalPipelineHash.stable = stableHash64.qwords[0];
        m_info.internalPipelineHash.unique = uniqueHash64.qwords[0];
        m_info.resourceMappingHash         = resourceHash64.qwords[0];

        result = LinkGraphicsLibraries(createInfo);
        CalculateOutputNumVertices();
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
    InitFlags(createInfo, internalInfo);

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

// =====================================================================================================================
// Gets the code object pointer according to shader type.
const void* GraphicsPipeline::GetCodeObjectWithShaderType(
    ShaderType shaderType,
    size_t*    pSize
    ) const
{
    const void* pBinary = nullptr;
    if (m_pipelineBinary.IsEmpty() == false)
    {
        pBinary = m_pipelineBinary.Data();
        if (pSize != nullptr)
        {
            *pSize = m_pipelineBinary.SizeInBytes();
        }
    }
    else
    {
        for (uint32 i = 0; i < m_numGfxShaderLibraries; i++)
        {
            if (Util::TestAnyFlagSet(m_gfxShaderLibraries[i]->GetApiShaderMask(), 1 << static_cast<uint32>(shaderType)))
            {
                Span<const void> binary = m_gfxShaderLibraries[i]->GetCodeObject();
                pBinary = binary.Data();
                if (pSize != nullptr)
                {
                    *pSize  = binary.SizeInBytes();
                }
                break;
            }
        }
    }

    return pBinary;
}

// =====================================================================================================================
// Query this pipeline's Bound GPU Memory.
Result GraphicsPipeline::QueryAllocationInfo(
    size_t*                   pNumEntries,
    GpuMemSubAllocInfo* const pGpuMemList
    ) const
{
    Result result = Result::ErrorInvalidPointer;

    if (pNumEntries != nullptr)
    {
        if (m_gpuMem.Memory() != nullptr)
        {
            (*pNumEntries) = 1;

            if (pGpuMemList != nullptr)
            {
                pGpuMemList[0].address = m_gpuMem.Memory()->Desc().gpuVirtAddr;
                pGpuMemList[0].offset  = m_gpuMem.Offset();
                pGpuMemList[0].size    = m_gpuMemSize;
            }
        }
        else
        {
            size_t numEntries = 0;
            (*pNumEntries) = 0;

            GpuMemSubAllocInfo*  pSubAllocInfo = pGpuMemList;
            for (uint32 i = 0; i < m_numGfxShaderLibraries; i++)
            {
                m_gfxShaderLibraries[i]->QueryAllocationInfo(&numEntries, pSubAllocInfo);
                if (pSubAllocInfo != nullptr)
                {
                    pSubAllocInfo += numEntries;
                }
                (*pNumEntries) += numEntries;
            }
        }

        result = Result::Success;
    }

    return result;
}

} // Pal
