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
#include "core/platform.h"
#include "core/hw/gfxip/gfxDevice.h"
#include "core/hw/gfxip/graphicsPipeline.h"
#include "core/hw/gfxip/palToScpcWrapper.h"
#include "palElfPackagerImpl.h"
#include "palFormatInfo.h"
#include "palMetroHash.h"
#include "palPipelineAbiProcessorImpl.h"

using namespace Util;

namespace Pal
{

// Private structure used to store/load a graphics pipeline object (corresponds to the member data of the
// GraphicsPipeline class.
struct SerializedData
{
    uint32                   flags;
    uint32                   vertsPerPrim;
    SwizzledFormat           targetFormats[MaxColorTargets];
    uint8                    targetWriteMasks[MaxColorTargets];
    ViewInstancingDescriptor viewInstancingDesc;
};

// =====================================================================================================================
GraphicsPipeline::GraphicsPipeline(
    Device* pDevice,
    bool    isInternal)
    :
    Pal::Pipeline(pDevice, isInternal),
    m_binningOverride(BinningOverride::Default),
    m_vertsPerPrim(0)
{
    m_flags.u32All = 0;

    memset(&m_targetSwizzledFormats[0], 0, sizeof(m_targetSwizzledFormats));
    memset(&m_targetWriteMasks[0],      0, sizeof(m_targetWriteMasks));
    memset(&m_viewInstancingDesc,       0, sizeof(m_viewInstancingDesc));
}

// =====================================================================================================================
// Initialize this compute pipeline based on the provided creation info.
Result GraphicsPipeline::Init(
    const GraphicsPipelineCreateInfo&         createInfo,
    const GraphicsPipelineInternalCreateInfo& internalInfo)
{
    Result result = Result::Success;

    if (createInfo.soState.numStreamOutEntries > MaxStreamOutEntries)
    {
        result = Result::ErrorInvalidOrdinal;
    }
    else if ((createInfo.soState.numStreamOutEntries != 0) && (createInfo.soState.pSoEntries == nullptr))
    {
        result = Result::ErrorInvalidPointer;
    }
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 305
    else if ((createInfo.pPipelineBinary != nullptr) && (createInfo.pipelineBinarySize != 0))
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
#endif
    {
        PAL_ASSERT_ALWAYS();
        result = Result::ErrorInvalidPointer;
    }

    if (result == Result::Success)
    {
        PAL_ASSERT(m_pPipelineBinary != nullptr);
        result = InitFromPipelineBinary(createInfo, internalInfo);
    }

    return result;
}

// =====================================================================================================================
// Initializes this pipeline from the pipeline binary data stored in this object, combined with the specified create
// info.
Result GraphicsPipeline::InitFromPipelineBinary(
    const GraphicsPipelineCreateInfo&         createInfo,
    const GraphicsPipelineInternalCreateInfo& internalInfo)
{
    m_flags.streamOut             = (createInfo.soState.numStreamOutEntries != 0);
    m_flags.adjacencyPrim         = createInfo.iaState.topologyInfo.adjacency;
    m_flags.perpLineEndCapsEnable = createInfo.rsState.perpLineEndCapsEnable;

    m_flags.fastClearElim    = internalInfo.flags.fastClearElim;
    m_flags.fmaskDecompress  = internalInfo.flags.fmaskDecompress;
    m_flags.dccDecompress    = internalInfo.flags.dccDecompress;
    m_flags.resolveFixedFunc = internalInfo.flags.resolveFixedFunc;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 310
    m_binningOverride = createInfo.rsState.binningOverride;
#endif

    m_flags.lateAllocVsLimit = createInfo.useLateAllocVsLimit;
    m_lateAllocVsLimit       = createInfo.lateAllocVsLimit;

    // Determine the number of vertices per primitive.
    switch (createInfo.iaState.topologyInfo.primitiveType)
    {
    case PrimitiveType::Point:
        m_vertsPerPrim = 1;
        break;
    case PrimitiveType::Line:
        m_vertsPerPrim = (m_flags.adjacencyPrim != 0) ? 4 : 2;
        break;
    case PrimitiveType::Rect:
        m_vertsPerPrim = 3;
        break;
    case PrimitiveType::Triangle:
        m_vertsPerPrim = (m_flags.adjacencyPrim != 0) ? 6 : 3;
        break;
    case PrimitiveType::Quad:
        m_vertsPerPrim = 4;
        break;
    case PrimitiveType::Patch:
        m_vertsPerPrim = createInfo.iaState.topologyInfo.patchControlPoints;
        break;
    }

    // Initialize a MetroHash64 hasher for computing a hash of the creation info.
    MetroHash64 hasher;

    hasher.Update(createInfo.flags);
    hasher.Update(createInfo.iaState);
    hasher.Update(createInfo.tessState);
    hasher.Update(createInfo.vpState);
    hasher.Update(createInfo.rsState);
    hasher.Update(createInfo.cbState);
    hasher.Update(createInfo.dbState);
    hasher.Update(internalInfo.flags);

    if (m_flags.streamOut != 0)
    {
        hasher.Update(
            reinterpret_cast<const uint8*>(createInfo.soState.pSoEntries),
            static_cast<uint64>(createInfo.soState.numStreamOutEntries * sizeof(StreamOutEntry)));
        hasher.Update(createInfo.soState.rasterizedStreams);
        hasher.Update(createInfo.soState.bufferStrides);
    }

    for (uint32 i = 0; i < MaxColorTargets; ++i)
    {
        m_targetSwizzledFormats[i] = createInfo.cbState.target[i].swizzledFormat;
        m_targetWriteMasks[i]      = createInfo.cbState.target[i].channelWriteMask;
    }

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 338
    m_viewInstancingDesc                   = createInfo.viewInstancingDesc;
#endif
    m_viewInstancingDesc.viewInstanceCount = Max(m_viewInstancingDesc.viewInstanceCount, 1u);
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 338
    hasher.Update(m_viewInstancingDesc);
#endif

    AbiProcessor abiProcessor(m_pDevice->GetPlatform());
    Result result = abiProcessor.LoadFromBuffer(m_pPipelineBinary, m_pipelineBinaryLen);

    if (result == Result::Success)
    {
        ExtractPipelineInfo(abiProcessor, ShaderType::Vertex, ShaderType::Pixel);

        DumpPipelineElf(abiProcessor, "PipelineGfx");

        // The pipeline ABI reports a unique pipeline hash of all of the components of its pipeline.  However, PAL
        // includes more state in the graphics pipeline than just the shaders.  We need to incorporate the reported
        // hash into our own checksum.
        hasher.Update(m_info.compilerHash);

        if (ShaderHashIsNonzero(m_info.shader[static_cast<uint32>(ShaderType::Geometry)].hash))
        {
            m_flags.gsEnabled = 1;
        }

        if (ShaderHashIsNonzero(m_info.shader[static_cast<uint32>(ShaderType::Hull)].hash) &&
            ShaderHashIsNonzero(m_info.shader[static_cast<uint32>(ShaderType::Domain)].hash))
        {
            m_flags.tessEnabled = 1;
        }

        uint32 metadataValue = 0;
        if (abiProcessor.HasPipelineMetadataEntry(Abi::PipelineMetadataType::UsesSampleInfo, &metadataValue))
        {
            m_flags.sampleInfoEnabled = metadataValue;
        }

        if (abiProcessor.HasPipelineMetadataEntry(Abi::PipelineMetadataType::PsUsesUavs, &metadataValue))
        {
            m_flags.psUsesUavs = metadataValue;
        }

        if (abiProcessor.HasPipelineMetadataEntry(Abi::PipelineMetadataType::PsUsesRovs, &metadataValue))
        {
            m_flags.psUsesRovs = metadataValue;
        }

        if (abiProcessor.HasPipelineMetadataEntry(Abi::PipelineMetadataType::UsesViewportArrayIndex, &metadataValue))
        {
            m_flags.vportArrayIdx = metadataValue;
        }

        // Override creation options for non-internal objects based on the requested toss point mode.
        // NOTE: This is done after computing the pipeline hash because we don't want toss points to alter what hash
        // values this object is associated with!
        GraphicsPipelineCreateInfo localInfo = createInfo;

        if (IsInternal() == false)
        {
            switch (m_pDevice->Settings().tossPointMode)
            {
            case TossPointAfterRaster:
                // This toss point is used to disable rasterization.
                localInfo.rsState.rasterizerDiscardEnable = true;
                break;
            case TossPointDepthClipDisable:
                localInfo.vpState.depthClipEnable = false;
                break;
            case TossPointSimplePs:
                PAL_NOT_IMPLEMENTED();
                break;
            default:
                break;
            }
        }

        result = HwlInit(localInfo, abiProcessor);
    }

    // Finalize the hash.
    hasher.Finalize(reinterpret_cast<uint8* const>(&m_info.pipelineHash));

    return result;
}

// =====================================================================================================================
// Load data from the specified ELF object to restore a previously serialized graphics pipeline object.
Result GraphicsPipeline::LoadInit(
    const ElfReadContext<Platform>& context)
{
    Result result = Pipeline::LoadInit(context);
    if (result == Result::Success)
    {
        // Verify the correct pipeline type.
        const PipelineType* pType = nullptr;
        size_t              size  = 0;
        result = GetLoadedSectionData(context, ".pipelineType", reinterpret_cast<const void**>(&pType), &size);
        if (*pType != PipelineTypeGraphics)
        {
            result = Result::ErrorInvalidPipelineElf;
        }
    }

    if (result == Result::Success)
    {
        // NOTE: We cannot break the legacy pipeline serialization path yet because some clients still rely on it.
        // Instead, Serialize() just puts the pipeline binary blob into the ELF.

        const void* pPipelineBinary   = nullptr;
        size_t      pipelineBinaryLen = 0;
        result = GetLoadedSectionData(context, ".pipelineBinary", &pPipelineBinary, &pipelineBinaryLen);
        if (result == Result::Success)
        {
            m_pipelineBinaryLen = pipelineBinaryLen;
            m_pPipelineBinary   = PAL_MALLOC(m_pipelineBinaryLen, m_pDevice->GetPlatform(), AllocInternal);
            if (m_pPipelineBinary == nullptr)
            {
                result = Result::ErrorOutOfMemory;
            }
            else
            {
                memcpy(m_pPipelineBinary, pPipelineBinary, m_pipelineBinaryLen);
            }
        }
    }

    if (result == Result::Success)
    {
        const SerializedData* pData    = nullptr;
        size_t                dataSize = 0;
        result = GetLoadedSectionData(context,
                                      ".graphicsPipelineData",
                                      reinterpret_cast<const void**>(&pData),
                                      &dataSize);
        if (result == Result::Success)
        {
            m_flags.u32All = pData->flags;
            m_vertsPerPrim = pData->vertsPerPrim;

            for (uint32 i = 0; i < MaxColorTargets; i++)
            {
                m_targetSwizzledFormats[i] = pData->targetFormats[i];
                m_targetWriteMasks[i]      = pData->targetWriteMasks[i];
            }

            m_viewInstancingDesc = pData->viewInstancingDesc;
        }
    }

    return result;
}

// =====================================================================================================================
// Adds a binary section to the specified ELF context describing this graphics pipeline.
Result GraphicsPipeline::Serialize(
    ElfWriteContext<Platform>* pContext)
{
    // NOTE: We cannot break the legacy pipeline serialization path yet because graphics pipelines still rely on it.
    // Instead, Serialize() just puts the pipeline binary blob into the ELF.

    constexpr PipelineType Type = PipelineTypeGraphics;
    Result result = pContext->AddBinarySection(".pipelineType", &Type, sizeof(Type));
    if (result == Result::Success)
    {
        result = pContext->AddBinarySection(".pipelineBinary", m_pPipelineBinary, m_pipelineBinaryLen);
    }

    if (result == Result::Success)
    {
        SerializedData data = { };
        data.flags        = m_flags.u32All;
        data.vertsPerPrim = m_vertsPerPrim;

        for (uint32 i = 0; i < MaxColorTargets; i++)
        {
            data.targetFormats[i]    = m_targetSwizzledFormats[i];
            data.targetWriteMasks[i] = m_targetWriteMasks[i];
        }

        data.viewInstancingDesc = m_viewInstancingDesc;

        result = pContext->AddBinarySection(".graphicsPipelineData", &data, sizeof(SerializedData));
    }

    return result;
}

} // Pal
