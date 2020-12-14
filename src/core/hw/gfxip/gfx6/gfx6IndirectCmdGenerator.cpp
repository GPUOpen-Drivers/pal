/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/gfxCmdBuffer.h"
#include "core/hw/gfxip/gfx6/gfx6CmdUtil.h"
#include "core/hw/gfxip/gfx6/gfx6ComputePipeline.h"
#include "core/hw/gfxip/gfx6/gfx6Device.h"
#include "core/hw/gfxip/gfx6/gfx6GraphicsPipeline.h"
#include "core/hw/gfxip/gfx6/gfx6IndirectCmdGenerator.h"
#include "core/g_palPlatformSettings.h"
#include "palInlineFuncs.h"
#include "palFormatInfo.h"

using namespace Util;

namespace Pal
{
namespace Gfx6
{

// Contains all information the indirect command generation shader(s) need to represent a compute pipeline signature.
// NOTE: This *must* be compatible with the 'ComputePipelineSignature' structure defined in
// core/hw/gfxip/rpm/gfx6/gfx6Chip.hlsl!
struct ComputePipelineSignatureData
{
    // First user-data entry which is spilled to GPU memory. A value of 'NO_SPILLING' indicates the pipeline does
    // not spill user data to memory.
    uint32  spillThreshold;
    // Register address for the GPU virtual address pointing to the internal constant buffer containing the number
    // of thread groups launched in a Dispatch operation. Two sequential SPI user-data registers are needed to store
    // the address, this is the first register.
    uint32  numWorkGroupsRegAddr;
};

// Contains all information the indirect command generation shader(s) need to represent a graphics pipeline signature.
// NOTE: This *must* be compatible with the 'GraphicsPipelineSignature' structure defined in
// core/hw/gfxip/rpm/gfx6/gfx6Chip.hlsl!
struct GraphicsPipelineSignatureData
{
    // First user-data entry which is spilled to GPU memory. A value of 'NO_SPILLING' indicates the pipeline does
    // not spill user data to memory.
    uint32  spillThreshold;
    // Register address for the vertex ID offset of a draw. The instance ID offset is always the very next register.
    uint32  vertexOffsetRegAddr;
    // Register address for the draw index of a multi-draw indirect. This is an optional feature for each pipeline,
    // so it may be 'ENTRY_NOT_MAPPED'.
    uint32  drawIndexRegAddr;
    // Register address for the GPU virtual address of the vertex buffer table used by this pipeline. Zero
    // indicates that the vertex buffer table is not accessed.
    uint32 vertexBufTableRegAddr;
};

// NOTE: The shader(s) used to generate these indirect command buffers launch one thread per command in the Y
// dimension and one thread per command parameter in the X dimension. The threadgroup size is 8x8x1, so we need to
// round up the number of command parameters to be a multiple of 8. The extra parameters will have a size of zero,
// which indicates to the shader(s) that the thread should not generate any commands.
constexpr uint32 CmdCountAlignment = 8u;

// =====================================================================================================================
// Helper function to compute the padded parameter count for a command generator (this is needed by RPM's shaders).
static uint32 PAL_INLINE PaddedParamCount(
    uint32 paramCount)
{
    return RoundUpToMultiple(paramCount, CmdCountAlignment);
}

// =====================================================================================================================
size_t IndirectCmdGenerator::GetSize(
    const IndirectCmdGeneratorCreateInfo& createInfo)
{
    // The required size of a command generator is the object size plus space for the parameter buffer data and the
    // client data buffer. The client data buffer and the param buffer data will immediately follow the object in
    // system memory.
    return (sizeof(IndirectCmdGenerator) + (sizeof(IndirectParamData) * PaddedParamCount(createInfo.paramCount)));
}

// =====================================================================================================================
IndirectCmdGenerator::IndirectCmdGenerator(
    const Device&                         device,
    const IndirectCmdGeneratorCreateInfo& createInfo)
    :
    Pal::IndirectCmdGenerator(device, createInfo),
    m_bindsIndexBuffer(false),
    m_pParamData(reinterpret_cast<IndirectParamData*>(this + 1))
{
    m_properties.maxUserDataEntries = device.Parent()->ChipProperties().gfxip.maxUserDataEntries;
    memcpy(&m_properties.indexTypeTokens[0], &createInfo.indexTypeTokens[0], sizeof(createInfo.indexTypeTokens));

    InitParamBuffer(createInfo);

    m_gpuMemSize = (sizeof(m_properties) + (sizeof(IndirectParamData) * PaddedParamCount(ParameterCount())));
}

// =====================================================================================================================
Result IndirectCmdGenerator::BindGpuMemory(
    IGpuMemory* pGpuMemory,
    gpusize     offset)
{
    Result result = Pal::IndirectCmdGenerator::BindGpuMemory(pGpuMemory, offset);
    if (result == Result::Success)
    {
        const uint32 paddedParamCount = PaddedParamCount(ParameterCount());

        void* pMappedAddr = nullptr;
        result = m_gpuMemory.Map(&pMappedAddr);
        if (result == Result::Success)
        {
            memcpy(pMappedAddr, &m_properties, sizeof(m_properties));
            memcpy(VoidPtrInc(pMappedAddr, sizeof(m_properties)),
                   m_pParamData,
                   (sizeof(IndirectParamData) * paddedParamCount));

            result = m_gpuMemory.Unmap();
        }

        // Build a typed SRD for the constant buffer containing the generator's properties structure.
        BufferViewInfo bufferInfo = { };
        bufferInfo.gpuAddr       = Memory().GpuVirtAddr();
        bufferInfo.stride        = (sizeof(uint32) * 4);
        bufferInfo.range         = Util::RoundUpToMultiple<gpusize>(sizeof(m_properties), bufferInfo.stride);
        bufferInfo.swizzledFormat.format  = ChNumFormat::X32Y32Z32W32_Uint;
        bufferInfo.swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };
        m_device.Parent()->CreateTypedBufferViewSrds(1, &bufferInfo, &m_propertiesSrd);

        // Build an untyped SRD for the structured-buffer containing the generator's indirect parameter data.
        bufferInfo.gpuAddr       += sizeof(m_properties);
        bufferInfo.swizzledFormat = UndefinedSwizzledFormat;
        bufferInfo.range          = (sizeof(IndirectParamData) * paddedParamCount);
        bufferInfo.stride         = sizeof(IndirectParamData);
        m_device.Parent()->CreateUntypedBufferViewSrds(1, &bufferInfo, &m_paramBufSrd);
    }

    return result;
}

// =====================================================================================================================
uint32 IndirectCmdGenerator::DetermineMaxCmdBufSize(
    GeneratorType        type,
    IndirectOpType       opType,
    const IndirectParam& param
    ) const
{
    // NOTE: We can use CountSetBits because the API shader stages line up 1:1 with the HW shader stages except for cases
    //       where all stages are enabled. We do not expect user data to be bound to the copy shader other than the
    //       streamout SRD table. Streamout targets cannot be changed by an indirect command generator, so we don't need
    //       to flag this stage.
    const uint32 numHwStages = Util::CountSetBits(param.userDataShaderUsage);
    const uint32 shaderStageCount = ((type == GeneratorType::Dispatch) ? 1 : numHwStages);

    uint32 size = 0;
    switch (opType)
    {
    case IndirectOpType::DrawIndexAuto:
        // DRAW_INDEX_AUTO operations generate the following PM4 packets in the worst case:
        //  + SET_SH_REG (2 registers)
        //  + SET_SH_REG (1 register)
        //  + NUM_INSTANCES
        //  + DRAW_INDEX_AUTO
        size = (CmdUtil::GetSetDataHeaderSize() + 2) +
               (CmdUtil::GetSetDataHeaderSize() + 1) +
                CmdUtil::GetNumInstancesSize() +
                CmdUtil::GetDrawIndexAutoSize();
        break;
    case IndirectOpType::DrawIndex2:
        // DRAW_INDEX_2 operations generate the following PM4 packets in the worst case:
        //  + SET_SH_REG (2 registers)
        //  + SET_SH_REG (1 register)
        //  + NUM_INSTANCES
        //  + INDEX_TYPE
        //  + DRAW_INDEX_2
        size = (CmdUtil::GetSetDataHeaderSize() + 2) +
               (CmdUtil::GetSetDataHeaderSize() + 1) +
                CmdUtil::GetNumInstancesSize() +
                CmdUtil::GetIndexTypeSize() +
                CmdUtil::GetDrawIndex2Size();
        break;
    case IndirectOpType::DrawIndexOffset2:
        // DRAW_INDEX_OFFSET_2 operations generate the following PM4 packets in the worst case:
        //  + SET_SH_REG (2 registers)
        //  + SET_SH_REG (1 register)
        //  + NUM_INSTANCES
        //  + DRAW_INDEX_OFFSET_2
        size = (CmdUtil::GetSetDataHeaderSize() + 2) +
               (CmdUtil::GetSetDataHeaderSize() + 1) +
                CmdUtil::GetNumInstancesSize() +
                CmdUtil::GetDrawIndexOffset2Size();
        break;
    case IndirectOpType::Dispatch:
        // DISPATCH operations generate the following PM4 packets in the worst case:
        //  + SET_SH_REG (2 registers)
        //  + DISPATCH_DIRECT
        size = (CmdUtil::GetSetDataHeaderSize() + 2) +
                CmdUtil::GetDispatchDirectSize();
        break;
    case IndirectOpType::SetUserData:
        // SETUSERDATA operations generate the following PM4 packets in the worst case:
        //  + SET_SH_REG (N registers; one packet per shader stage)
        size = ((CmdUtil::GetSetDataHeaderSize() + param.userData.entryCount) * shaderStageCount);
        break;
    case IndirectOpType::VertexBufTableSrd:
    case IndirectOpType::Skip:
        // INDIRECT_TABLE_SRD and SKIP operations don't directly generate any PM4 packets.
        break;
    default:
        PAL_NOT_IMPLEMENTED();
        break;
    }

    if ((opType == IndirectOpType::Dispatch)      ||
        (opType == IndirectOpType::DrawIndexAuto) ||
        (opType == IndirectOpType::DrawIndex2)    ||
        (opType == IndirectOpType::DrawIndexOffset2))
    {
        // Each type of Dispatch or Draw operation may require additional command buffer space if this command
        // generator modifies user-data entries or the vertex buffer table:
        //  + SET_SH_REG (1 register); one packet per HW shader stage [Spill Table]
        //  + SET_SH_REG (1 register); one packet per draw [VB table]
        if (m_properties.userDataWatermark != 0)
        {
            // Spill table applies to all HW shader stages if any user data spilled.
            const uint32 spillTableShaderStageCount = (opType == IndirectOpType::Dispatch) ? 1 : NumHwShaderStagesGfx;

            size += ((CmdUtil::GetSetDataHeaderSize() + 1) * spillTableShaderStageCount);
        }
        if (m_properties.vertexBufTableSize != 0)
        {
            size += (CmdUtil::GetSetDataHeaderSize() + 1);
        }
        const PalPlatformSettings& settings = m_device.Parent()->GetPlatform()->PlatformSettings();
        const bool sqttEnabled = (settings.gpuProfilerMode > GpuProfilerCounterAndTimingOnly) &&
                                 (Util::TestAnyFlagSet(settings.gpuProfilerConfig.traceModeMask, GpuProfilerTraceSqtt));
        const bool issueSqttMarkerEvent = (sqttEnabled ||
                                           m_device.Parent()->GetPlatform()->IsDevDriverProfilingEnabled());

        if (issueSqttMarkerEvent)
        {
            size += CmdUtil::GetWriteEventWriteSize();
        }
    }

    const uint32 minNopDwords = static_cast<const Gfx6::Device&>(m_device).CmdUtil().GetMinNopSizeInDwords();

    if ((size != 0) && (minNopDwords > 1))
    {
        // NOTE: If this command parameter writes any command-buffer data, we need to be careful: when the command
        // generator actually runs, it may need to write slightly fewer DWORD's worth of commands than we computed
        // for the worst-case. If this happens, we cannot guarantee that the leftover space is large enough to be
        // a valid PM4 NOP packet. To protect against this, add the minimum NOP size to whatever we compute for the
        // parameter's worst-case command buffer size.
        size += minNopDwords;
    }

    return (sizeof(uint32) * size); // Convert dwords to bytes
}

// =====================================================================================================================
void IndirectCmdGenerator::InitParamBuffer(
    const IndirectCmdGeneratorCreateInfo& createInfo)
{
    constexpr uint32 BufferSrdDwords = ((sizeof(BufferSrd) / sizeof(uint32)));

    memset(m_pParamData, 0, (sizeof(IndirectParamData) * PaddedParamCount(ParameterCount())));

    uint32 argBufOffset = 0;
    uint32 cmdBufOffset = 0;

    // We need to remember the argument buffer offset for BindIndexData because DrawIndexed is the parameter which
    // needs to process it (because DRAW_INDEX_2 packets issue a draw and bind an IB address simultaneously). If we
    // don't encounter a BindIndexData parameter for this generator, we'll fall back to using the suboptimal
    // DRAW_INDEX_OFFSET_2 packet because that packet doesn't require us to know the full index buffer GPU address.
    uint32 argBufOffsetIndices = 0;

    // Initialize all of the elements in the parameter data buffer which are not "dummy" parameters for thread-group
    // padding. Leaving the padding elements zeroed will indicate to the shader that no processing should be done.
    for (uint32 p = 0; ((createInfo.pParams != nullptr) && (p < createInfo.paramCount)); ++p)
    {
        const auto& param = createInfo.pParams[p];

        if (param.type == IndirectParamType::BindIndexData)
        {
            // See comment above for information on how we handle BindIndexData!
            m_pParamData[p].type = IndirectOpType::Skip;
            argBufOffsetIndices  = argBufOffset;
            m_bindsIndexBuffer   = true;
        }
        else
        {
            switch (param.type)
            {
            case IndirectParamType::Dispatch:
                m_pParamData[p].type = IndirectOpType::Dispatch;
                break;
            case IndirectParamType::Draw:
                m_pParamData[p].type = IndirectOpType::DrawIndexAuto;
                break;
            case IndirectParamType::DrawIndexed:
                // See comment above for information on how we handle BindIndexData.
                m_pParamData[p].type    = ContainsIndexBufferBind() ? IndirectOpType::DrawIndex2
                                                                    : IndirectOpType::DrawIndexOffset2;
                m_pParamData[p].data[0] = argBufOffsetIndices;
                break;
            case IndirectParamType::SetUserData:
                m_pParamData[p].type    = IndirectOpType::SetUserData;
                m_pParamData[p].data[0] = param.userData.firstEntry;
                m_pParamData[p].data[1] = param.userData.entryCount;
                // The user-data watermark tracks the highest index (plus one) of user-data entries modified by this
                // command generator.
                m_properties.userDataWatermark = Max((param.userData.firstEntry + param.userData.entryCount),
                                                     m_properties.userDataWatermark);
                // Also, we need to track the mask of which user-data entries this command-generator touches.
                for (uint32 e = 0; e < param.userData.entryCount; ++e)
                {
                    WideBitfieldSetBit(m_touchedUserData, (e + param.userData.firstEntry));
                }
                break;
            case IndirectParamType::BindVertexData:
                m_pParamData[p].type    = IndirectOpType::VertexBufTableSrd;
                m_pParamData[p].data[0] = (param.vertexData.bufferId * BufferSrdDwords);
                // Update the vertex buffer table size to indicate to the command-generation shader that the vertex
                // buffer is being updated by this generator.
                m_properties.vertexBufTableSize = (BufferSrdDwords * MaxVertexBuffers);
                break;
            default:
                PAL_NOT_IMPLEMENTED();
                break;
            }

            m_pParamData[p].argBufOffset = argBufOffset;
            m_pParamData[p].argBufSize   = param.sizeInBytes;
            m_pParamData[p].cmdBufOffset = cmdBufOffset;
            m_pParamData[p].cmdBufSize   = DetermineMaxCmdBufSize(Type(), m_pParamData[p].type, param);
        }

        cmdBufOffset += m_pParamData[p].cmdBufSize;
        argBufOffset += param.sizeInBytes;
    }

    m_properties.cmdBufStride = cmdBufOffset;
    m_properties.argBufStride = Max(argBufOffset, createInfo.strideInBytes);
}

// =====================================================================================================================
void IndirectCmdGenerator::PopulateInvocationBuffer(
    GfxCmdBuffer*   pCmdBuffer,
    const Pipeline* pPipeline,
    gpusize         argsGpuAddr,
    uint32          maximumCount,
    uint32          indexBufSize,
    void*           pSrd
    ) const
{
    BufferViewInfo viewInfo = { };
    viewInfo.stride        = (sizeof(uint32) * 4);
    viewInfo.range         = sizeof(InvocationProperties);

    viewInfo.swizzledFormat.format  = ChNumFormat::X32Y32Z32W32_Uint;
    viewInfo.swizzledFormat.swizzle =
        { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

    auto*const pData = reinterpret_cast<InvocationProperties*>(pCmdBuffer->CmdAllocateEmbeddedData(
        (sizeof(InvocationProperties) / sizeof(uint32)),
        1,
        &viewInfo.gpuAddr));
    PAL_ASSERT(pData != nullptr);

    pData->maximumCmdCount    = maximumCount;
    pData->indexBufSize       = indexBufSize;
    pData->argumentBufAddr[0] = LowPart(argsGpuAddr);
    pData->argumentBufAddr[1] = HighPart(argsGpuAddr);

    if (pCmdBuffer->GetEngineType() == EngineTypeCompute)
    {
        const auto*const pCsPipeline = static_cast<const ComputePipeline*>(pPipeline);
        pCsPipeline->ThreadsPerGroupXyz(&pData->gfx6.threadsPerGroup[0],
                                        &pData->gfx6.threadsPerGroup[1],
                                        &pData->gfx6.threadsPerGroup[2]);

        pData->gfx6.dimInThreads =
            ((static_cast<const Pal::Gfx6::Device&>(m_device).WaAsyncComputeMoreThan4096ThreadGroups() != false) &&
            (pCsPipeline->ThreadsPerGroup() >= 4096));
    }
    else
    {
        pData->gfx6.dimInThreads = false;

        pData->gfx6.threadsPerGroup[0] = 1;
        pData->gfx6.threadsPerGroup[1] = 1;
        pData->gfx6.threadsPerGroup[2] = 1;
    }

    if (m_device.Parent()->ChipProperties().gfxLevel >= GfxIpLevel::GfxIp8)
    {
        pData->gfx6.indexBufMType = MTYPE_UC;
    }

    m_device.Parent()->CreateTypedBufferViewSrds(1, &viewInfo, pSrd);
}

// =====================================================================================================================
// This helper allocates and populates an embedded-data structured buffer which contains the pipeline signature for the
// provided pipeline.
void IndirectCmdGenerator::PopulateSignatureBuffer(
    GfxCmdBuffer*   pCmdBuffer,
    const Pipeline* pPipeline,
    void*           pSrd        // [out] The embedded-data buffer's SRD will be written to this location.
    ) const
{
    BufferViewInfo viewInfo = { };

    if (Type() == GeneratorType::Dispatch)
    {
        viewInfo.stride  = sizeof(ComputePipelineSignatureData);
        auto*const pData = reinterpret_cast<ComputePipelineSignatureData*>(pCmdBuffer->CmdAllocateEmbeddedData(
            static_cast<uint32>(viewInfo.stride / sizeof(uint32)),
            1,
            &viewInfo.gpuAddr));
        PAL_ASSERT(pData != nullptr);

        const auto& signature = static_cast<const ComputePipeline*>(pPipeline)->Signature();

        pData->spillThreshold       = signature.spillThreshold;
        pData->numWorkGroupsRegAddr = signature.numWorkGroupsRegAddr;
    }
    else
    {
        viewInfo.stride = sizeof(GraphicsPipelineSignatureData);
        auto*const pData = reinterpret_cast<GraphicsPipelineSignatureData*>(pCmdBuffer->CmdAllocateEmbeddedData(
            static_cast<uint32>(viewInfo.stride / sizeof(uint32)),
            1,
            &viewInfo.gpuAddr));
        PAL_ASSERT(pData != nullptr);

        const auto& signature = static_cast<const GraphicsPipeline*>(pPipeline)->Signature();

        pData->spillThreshold        = signature.spillThreshold;
        pData->vertexOffsetRegAddr   = signature.vertexOffsetRegAddr;
        pData->drawIndexRegAddr      = signature.drawIndexRegAddr;
        pData->vertexBufTableRegAddr = signature.vertexBufTableRegAddr;
    }

    viewInfo.range          = viewInfo.stride;
    viewInfo.swizzledFormat = UndefinedSwizzledFormat;

    m_device.Parent()->CreateUntypedBufferViewSrds(1, &viewInfo, pSrd);
}

// =====================================================================================================================
// This helper allocates and populates an embedded-data typed buffer which contains the user-data register mappings for
// each shader stage in the provided pipeline. The layout of this buffer is each user-data entry's register mapping, and
// another uint32 for the spill table address mapping. This layout is repeated for each hardware shader stage.
void IndirectCmdGenerator::PopulateUserDataMappingBuffer(
    GfxCmdBuffer*   pCmdBuffer,
    const Pipeline* pPipeline,
    void*           pSrd        // [out] The embedded-data buffer's SRD will be written to this location.
    ) const
{
    const UserDataEntryMap* pStage     = nullptr;
    uint32                  stageCount = 0;

    if (Type() == GeneratorType::Dispatch)
    {
        const auto& signature = static_cast<const ComputePipeline*>(pPipeline)->Signature();

        pStage     = &signature.stage;
        stageCount = 1;
    }
    else
    {
        const auto& signature = static_cast<const GraphicsPipeline*>(pPipeline)->Signature();

        pStage     = &signature.stage[0];
        stageCount = NumHwShaderStagesGfx;
    }

    // Number of DWORD's in the embedded-data buffer per hardware shader stage: one for the spill table address, and
    // one for each user-data entry's register mapping.
    const uint32 dwordsPerStage = (1 + m_device.Parent()->ChipProperties().gfxip.maxUserDataEntries);

    BufferViewInfo viewInfo = { };
    viewInfo.stride        = sizeof(uint32);
    viewInfo.range         = (stageCount * sizeof(uint32) * dwordsPerStage);

    viewInfo.swizzledFormat.format  = ChNumFormat::X32_Uint;
    viewInfo.swizzledFormat.swizzle =
        { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One };

    uint32* pData = pCmdBuffer->CmdAllocateEmbeddedData((stageCount * dwordsPerStage), 1, &viewInfo.gpuAddr);

    for (uint32 stage = 0; stage < stageCount; ++stage)
    {
        uint32 entryMap[MaxUserDataEntries] = { };
        for (uint32 i = 0; i < pStage->userSgprCount; ++i)
        {
            entryMap[pStage->mappedEntry[i]] = (pStage->firstUserSgprRegAddr + i);
        }

        memcpy(pData, &entryMap[0], sizeof(uint32) * (dwordsPerStage - 1));
        pData[dwordsPerStage - 1] = pStage->spillTableRegAddr;

        ++pStage;
        pData += dwordsPerStage;
    }

    m_device.Parent()->CreateTypedBufferViewSrds(1, &viewInfo, pSrd);
}

} // Gfx6
} // Pal
