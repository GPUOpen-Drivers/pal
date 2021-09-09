/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/gfx9/gfx9CmdUtil.h"
#include "core/hw/gfxip/gfx9/gfx9ComputePipeline.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9GraphicsPipeline.h"
#include "core/hw/gfxip/gfx9/gfx9HybridGraphicsPipeline.h"
#include "core/hw/gfxip/gfx9/gfx9IndirectCmdGenerator.h"
#include "core/g_palPlatformSettings.h"
#include "palInlineFuncs.h"
#include "palFormatInfo.h"

using namespace Util;

namespace Pal
{
namespace Gfx9
{

// Contains all information the indirect command generation shader(s) need to represent a compute pipeline signature.
// NOTE: This *must* be compatible with the 'ComputePipelineSignature' structure defined in
// core/hw/gfxip/rpm/gfx9/gfx9Chip.hlsl!
struct ComputePipelineSignatureData
{
    // First user-data entry which is spilled to GPU memory. A value of 'NO_SPILLING' indicates the pipeline does
    // not spill user data to memory.
    uint32  spillThreshold;
    // Register address for the GPU virtual address pointing to the internal constant buffer containing the number
    // of thread groups launched in a Dispatch operation. Two sequential SPI user-data registers are needed to store
    // the address, this is the first register.
    uint32  numWorkGroupsRegAddr;
    // Register address for the dispatch dimensions of task shaders.
    uint32  taskDispatchDimsRegAddr;
    // Register address for the ring index for task shaders.
    uint32  taskRingIndexAddr;
};

// Contains all information the indirect command generation shader(s) need to represent a graphics pipeline signature.
// NOTE: This *must* be compatible with the 'GraphicsPipelineSignature' structure defined in
// core/hw/gfxip/rpm/gfx9/gfx9Chip.hlsl!
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
    uint32  vertexBufTableRegAddr;
    // Register address for the dispatch dimensions of mesh shaders.
    uint32  meshDispatchDimsRegAddr;
    // Register address for the ring index for mesh shaders.
    uint32  meshRingIndexAddr;
};

// NOTE: The shader(s) used to generate these indirect command buffers launch one thread per command in the Y
// dimension and one thread per command parameter in the X dimension. The threadgroup size is 8x8x1, so we need to
// round up the number of command parameters to be a multiple of 8. The extra parameters will have a size of zero,
// which indicates to the shader(s) that the thread should not generate any commands.
constexpr uint32 CmdCountAlignment = 8u;

// =====================================================================================================================
// Helper function to compute the padded parameter count for a command generator (this is needed by RPM's shaders).
constexpr uint32 PaddedParamCount(
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
    return (sizeof(IndirectCmdGenerator) + (sizeof(IndirectParamData) * PaddedParamCount(createInfo.paramCount)) +
            (sizeof(IndirectParam) * createInfo.paramCount));
}

// =====================================================================================================================
IndirectCmdGenerator::IndirectCmdGenerator(
    const Device&                         device,
    const IndirectCmdGeneratorCreateInfo& createInfo)
    :
    Pal::IndirectCmdGenerator(device, createInfo),
    m_bindsIndexBuffer(false),
    m_pParamData(reinterpret_cast<IndirectParamData*>(this + 1)),
    m_pCreationParam(reinterpret_cast<IndirectParam*>(m_pParamData+PaddedParamCount(createInfo.paramCount))),
    m_cmdSizeNeedPipeline(false)
{
    m_properties.maxUserDataEntries = device.Parent()->ChipProperties().gfxip.maxUserDataEntries;
    memcpy(&m_properties.indexTypeTokens[0], &createInfo.indexTypeTokens[0], sizeof(createInfo.indexTypeTokens));
    memcpy(m_pCreationParam, createInfo.pParams, sizeof(IndirectParam)*createInfo.paramCount);

    InitParamBuffer(createInfo);

    m_gpuMemSize = m_cmdSizeNeedPipeline ?
        8 : (sizeof(m_properties) + (sizeof(IndirectParamData) * PaddedParamCount(ParameterCount())));
}

// =====================================================================================================================
Result IndirectCmdGenerator::BindGpuMemory(
    IGpuMemory* pGpuMemory,
    gpusize     offset)
{
    Result result = Pal::IndirectCmdGenerator::BindGpuMemory(pGpuMemory, offset);
    if ((result == Result::Success) && (m_cmdSizeNeedPipeline == false))
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
    // NOTE: We do not know whether this command signature will be used with a NGG pipeline. We always assume non - NGG
    //       which has the worst case total count of HW shader stages

    uint32 numHwStages = 0;
    // For pre-PS API shaders, due to shader merge there are 5 possible HW shader stage combinations
    // (1) HW      VS   : API Tess off GS off
    // (2) HW      GS   : API Tess off GS on, or API Mesh shader
    // (3) HW HS + VS   : API Tess on  GS off
    // (4) HW HS + GS   : API Tess on  GS on
    // (5) HW CS + GS   : API Task + Mesh shader
    // We do not expect user data to be bound to the copy shader other than the streamout SRD table.
    // Streamout targets cannot be changed by an indirect command generator, so we don't need to flag this stage.

    const uint32 hwHsCsEnable = TestAnyFlagSet(param.userDataShaderUsage,
        ApiShaderStageVertex | ApiShaderStageHull | ApiShaderStageTask) ? 1 : 0;
    const uint32 hwGsVsEnable = TestAnyFlagSet(param.userDataShaderUsage,
        ApiShaderStageVertex | ApiShaderStageDomain | ApiShaderStageGeometry | ApiShaderStageMesh) ? 1 : 0;
    numHwStages += hwHsCsEnable + hwGsVsEnable;

    // API pixel shader stage accounts for one HW shader stage
    if (TestAllFlagsSet(param.userDataShaderUsage, ApiShaderStagePixel))
    {
        numHwStages++;
    }

    const uint32 shaderStageCount = ((type == GeneratorType::Dispatch) ? 1 : numHwStages);
    PAL_ASSERT((type != GeneratorType::Dispatch) || (param.userDataShaderUsage == ApiShaderStageCompute));

    uint32 size = 0;
    switch (opType)
    {
    case IndirectOpType::DrawIndexAuto:
        // DRAW_INDEX_AUTO operations generate the following PM4 packets in the worst case:
        //  + SET_SH_REG (2 registers)
        //  + SET_SH_REG (1 register)
        //  + NUM_INSTANCES
        //  + DRAW_INDEX_AUTO
        size = (CmdUtil::ShRegSizeDwords + 2) +
               (CmdUtil::ShRegSizeDwords + 1) +
                CmdUtil::NumInstancesDwords   +
                CmdUtil::DrawIndexAutoSize;
        break;
    case IndirectOpType::DrawIndex2:
        // DRAW_INDEX_2 operations generate the following PM4 packets in the worst case:
        //  + SET_SH_REG (2 registers)
        //  + SET_SH_REG (1 register)
        //  + NUM_INSTANCES
        //  + INDEX_TYPE
        //  + DRAW_INDEX_2
        size = (CmdUtil::ShRegSizeDwords + 2)     +
               (CmdUtil::ShRegSizeDwords + 1)     +
                CmdUtil::NumInstancesDwords       +
               (CmdUtil::ConfigRegSizeDwords + 1) +
                CmdUtil::DrawIndex2Size;
        break;
    case IndirectOpType::DrawIndexOffset2:
        // DRAW_INDEX_OFFSET_2 operations generate the following PM4 packets in the worst case:
        //  + SET_SH_REG (2 registers)
        //  + SET_SH_REG (1 register)
        //  + NUM_INSTANCES
        //  + DRAW_INDEX_OFFSET_2
        size = (CmdUtil::ShRegSizeDwords + 2) +
               (CmdUtil::ShRegSizeDwords + 1) +
                CmdUtil::NumInstancesDwords   +
                CmdUtil::DrawIndexOffset2Size;
        break;
    case IndirectOpType::Dispatch:
        // DISPATCH operations generate the following PM4 packets in the worst case:
        //  + SET_SH_REG (2 registers)
        //  + DISPATCH_DIRECT
        size = (CmdUtil::ShRegSizeDwords + 2) +
                CmdUtil::DispatchDirectSize;
        break;
    case IndirectOpType::SetUserData:
        // SETUSERDATA operations generate the following PM4 packets in the worst case:
        //  + SET_SH_REG (N registers; one packet per shader stage)
        size = ((CmdUtil::ShRegSizeDwords + param.userData.entryCount) * shaderStageCount);
        break;
    case IndirectOpType::VertexBufTableSrd:
    case IndirectOpType::Skip:
        // INDIRECT_TABLE_SRD and SKIP operations don't directly generate any PM4 packets.
        break;
    case IndirectOpType::DispatchMesh:
        // DISPATCH_MESH operations handle both mesh-only pipelines and task+mesh pipelines.
        // In the case of mesh-only piplines we generate the following in the worst case:
        //  + SET_SH_REG (3 registers)
        //  + SET_SH_REG (1 register)
        //  + NUM_INSTANCES
        //  + DRAW_INDEX_AUTO
        // For task+mesh pipelines, we generate the following on the gfx cmdStream:
        //  + DISPATCH_TASKMESH_GFX
        // For the ace cmdStream, we generate the following:
        //  + SET_SH_REG (3 registers)
        //  + DISPATCH_TASKMESH_DIRECT_ACE

        size = Max((CmdUtil::ShRegSizeDwords + 3) +
                   (CmdUtil::ShRegSizeDwords + 1) +
                   CmdUtil::NumInstancesDwords +
                   CmdUtil::DrawIndexAutoSize,
                   Max((CmdUtil::ShRegSizeDwords + 3) +
                       CmdUtil::DispatchTaskMeshDirectMecSize,
                       CmdUtil::DispatchTaskMeshGfxSize));
        break;
    default:
        PAL_NOT_IMPLEMENTED();
        break;
    }

    if ((opType == IndirectOpType::Dispatch)       ||
        (opType == IndirectOpType::DispatchMesh)   ||
        (opType == IndirectOpType::DrawIndexAuto)  ||
        (opType == IndirectOpType::DrawIndex2)     ||
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

            size += ((CmdUtil::ShRegSizeDwords + 1) * spillTableShaderStageCount);
        }

        if (m_properties.vertexBufTableSize != 0)
        {
            size += (CmdUtil::ShRegSizeDwords + 1);
        }

        if (m_device.Parent()->IssueSqttMarkerEvents())
        {
            size += CmdUtil::WriteNonSampleEventDwords;
        }
    }

    static_assert(CmdUtil::MinNopSizeInDwords == 1,
                  "If CmdUtil::MinNopSizeInDwords is larger than one then we need to increase size appropriately.");

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
            case IndirectParamType::DispatchMesh:
                m_pParamData[p].type = IndirectOpType::DispatchMesh;
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
                if (Type() != GeneratorType::Dispatch)
                {
                    m_cmdSizeNeedPipeline = true;
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

    m_properties.cmdBufStride = m_cmdSizeNeedPipeline ? 0 : cmdBufOffset;
    m_properties.argBufStride = Max(argBufOffset, createInfo.strideInBytes);
}

// =====================================================================================================================
void IndirectCmdGenerator::PopulateParameterBuffer(
    GfxCmdBuffer*   pCmdBuffer,
    const Pipeline* pPipeline,
    void*           pSrd
    ) const
{
    if (m_cmdSizeNeedPipeline)
    {
        PAL_ASSERT(Type() != GeneratorType::Dispatch);
        const auto& signature = static_cast<const GraphicsPipeline*>(pPipeline)->Signature();
        const uint32 paddedParamCount = PaddedParamCount(ParameterCount());

        BufferViewInfo viewInfo = { };
        viewInfo.swizzledFormat = UndefinedSwizzledFormat;
        viewInfo.range          = (sizeof(IndirectParamData) * paddedParamCount);
        viewInfo.stride         = sizeof(IndirectParamData);

        IndirectParamData* pData = reinterpret_cast<IndirectParamData*>(pCmdBuffer->CmdAllocateEmbeddedData(
            sizeof(IndirectParamData) * paddedParamCount / sizeof(uint32), 1, &viewInfo.gpuAddr));

        PAL_ASSERT(pData != nullptr);
        memcpy(pData, m_pParamData, sizeof(IndirectParamData) * paddedParamCount);

        uint32 cmdBufOffset = 0;

        for (uint32 p = 0; ((m_pCreationParam != nullptr) && (p < ParameterCount())); ++p)
        {
            const IndirectParam& param = m_pCreationParam[p];

            if (param.type == IndirectParamType::SetUserData)
            {
                const UserDataEntryMap* pStage = &signature.stage[0];
                uint32 numHwStages = 0;

                for (uint32 stage = 0; stage < NumHwShaderStagesGfx; ++stage)
                {
                    for (uint32 i = 0; i < pStage->userSgprCount; ++i)
                    {
                        if (pStage->mappedEntry[i] == param.userData.firstEntry)
                        {
                            numHwStages++;
                            break;
                        }
                    }
                    ++pStage;
                }
                uint32 size = (CmdUtil::ShRegSizeDwords + param.userData.entryCount) * numHwStages;
                pData[p].cmdBufSize = sizeof(uint32) * size;
            }
            pData[p].cmdBufOffset = cmdBufOffset;
            cmdBufOffset += pData[p].cmdBufSize;
        }

        m_device.Parent()->CreateUntypedBufferViewSrds(1, &viewInfo, pSrd);
    }
    else
    {
        memcpy(pSrd, ParamBufferSrd(), sizeof(BufferSrd));
    }
}

// =====================================================================================================================
uint32 IndirectCmdGenerator::CmdBufStride(
    const Pipeline* pPipeline
    ) const
{
    uint32  cmdBufStride = 0;

    if (m_cmdSizeNeedPipeline)
    {
        const auto& signature = static_cast<const GraphicsPipeline*>(pPipeline)->Signature();

        for (uint32 p = 0; ((m_pCreationParam != nullptr) && (p < ParameterCount())); ++p)
        {
            const IndirectParam& param = m_pCreationParam[p];
            uint32 cmdSize = 0;

            if (param.type == IndirectParamType::SetUserData)
            {
                const UserDataEntryMap* pStage = &signature.stage[0];
                uint32 numHwStages = 0;

                for (uint32 stage = 0; stage < NumHwShaderStagesGfx; ++stage)
                {
                    for (uint32 i = 0; i < pStage->userSgprCount; ++i)
                    {
                        if (pStage->mappedEntry[i] == param.userData.firstEntry)
                        {
                            numHwStages++;
                            break;
                        }
                    }
                    ++pStage;
                }
                uint32 size = (CmdUtil::ShRegSizeDwords + param.userData.entryCount) * numHwStages;
                cmdSize = sizeof(uint32) * size;
            }
            else
            {
                cmdSize = m_pParamData[p].cmdBufSize;
            }
            cmdBufStride += cmdSize;
        }
    }
    else
    {
        cmdBufStride = m_properties.cmdBufStride;
    }

    return (cmdBufStride);
}

// =====================================================================================================================
void IndirectCmdGenerator::PopulatePropertyBuffer(
    GfxCmdBuffer*   pCmdBuffer,
    const Pipeline* pPipeline,
    void*           pSrd
    ) const
{
    if (m_cmdSizeNeedPipeline)
    {
        BufferViewInfo viewInfo = { };
        viewInfo.stride        = (sizeof(uint32) * 4);
        viewInfo.range         = Util::RoundUpToMultiple<gpusize>(sizeof(m_properties), viewInfo.stride);
        viewInfo.swizzledFormat.format  = ChNumFormat::X32Y32Z32W32_Uint;
        viewInfo.swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        GeneratorProperties* pData = reinterpret_cast<GeneratorProperties*>(pCmdBuffer->CmdAllocateEmbeddedData(
            static_cast<uint32>(viewInfo.range) / sizeof(uint32), 1, &viewInfo.gpuAddr));
        memcpy(pData, &m_properties, sizeof(m_properties));
        pData->cmdBufStride = CmdBufStride(pPipeline);

        m_device.Parent()->CreateTypedBufferViewSrds(1, &viewInfo, pSrd);
    }
    else
    {
        memcpy(pSrd, PropertiesSrd(), sizeof(BufferSrd));
    }
}

// =====================================================================================================================
void IndirectCmdGenerator::PopulateInvocationBuffer(
    GfxCmdBuffer*   pCmdBuffer,
    const Pipeline* pPipeline,
    bool            isTaskEnabled,
    gpusize         argsGpuAddr,
    uint32          maximumCount,
    uint32          indexBufSize,
    void*           pSrd
    ) const
{
    BufferViewInfo viewInfo = { };
    viewInfo.stride         = (sizeof(uint32) * 4);
    viewInfo.range          = sizeof(InvocationProperties);

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

    if ((Type() == GeneratorType::Dispatch) || ((Type() == GeneratorType::DispatchMesh) && isTaskEnabled))
    {
        bool csWave32              = false;
        bool disablePartialPreempt = false;

        if (Type() == GeneratorType::Dispatch)
        {
            const ComputePipeline* pCsPipeline = static_cast<const ComputePipeline*>(pPipeline);
            csWave32              = pCsPipeline->Signature().flags.isWave32;
            disablePartialPreempt = pCsPipeline->DisablePartialPreempt();
        }
        else
        {
            const HybridGraphicsPipeline* pTsMsPipeline = static_cast<const HybridGraphicsPipeline*>(pPipeline);
            csWave32              = pTsMsPipeline->GetTaskSignature().flags.isWave32;
            disablePartialPreempt = true;
        }

        regCOMPUTE_DISPATCH_INITIATOR dispatchInitiator = {};

        dispatchInitiator.bits.COMPUTE_SHADER_EN  = 1;
        dispatchInitiator.bits.ORDER_MODE         = 1;

        if (IsGfx10Plus(*m_device.Parent()))
        {
            dispatchInitiator.gfx10Plus.CS_W32_EN     = csWave32;
            dispatchInitiator.gfx10Plus.TUNNEL_ENABLE = pCmdBuffer->UsesDispatchTunneling();
        }
        if (disablePartialPreempt)
        {
            dispatchInitiator.u32All |= ComputeDispatchInitiatorDisablePartialPreemptMask;
        }

        pData->gfx9.dispatchInitiator = dispatchInitiator.u32All;
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
    else if (Type() == GeneratorType::DispatchMesh)
    {
        BufferViewInfo secondViewInfo = {};

        // We want to set up the compute pipeline first as we don't want the gpuAddr being overwritten for the graphics
        // pipeline.
        secondViewInfo.stride   = sizeof(ComputePipelineSignatureData);
        auto* const pSecondData = reinterpret_cast<ComputePipelineSignatureData*>(pCmdBuffer->CmdAllocateEmbeddedData(
            static_cast<uint32>(secondViewInfo.stride / sizeof(uint32)),
            1,
            &secondViewInfo.gpuAddr));
        PAL_ASSERT(pSecondData != nullptr);

        const auto& secondSignature = static_cast<const HybridGraphicsPipeline*>(pPipeline)->GetTaskSignature();

        pSecondData->spillThreshold          = secondSignature.spillThreshold;
        pSecondData->numWorkGroupsRegAddr    = secondSignature.numWorkGroupsRegAddr;
        pSecondData->taskDispatchDimsRegAddr = secondSignature.taskDispatchDimsAddr;
        pSecondData->taskRingIndexAddr       = secondSignature.taskRingIndexAddr;

        secondViewInfo.range          = secondViewInfo.stride;
        secondViewInfo.swizzledFormat = UndefinedSwizzledFormat;

        m_device.Parent()->CreateUntypedBufferViewSrds(1, &secondViewInfo, pSrd);
        pSrd = VoidPtrInc(pSrd, 4 * sizeof(uint32));

        viewInfo.stride   = sizeof(GraphicsPipelineSignatureData);
        auto* const pData = reinterpret_cast<GraphicsPipelineSignatureData*>(pCmdBuffer->CmdAllocateEmbeddedData(
            static_cast<uint32>(viewInfo.stride / sizeof(uint32)),
            1,
            &viewInfo.gpuAddr));
        PAL_ASSERT(pData != nullptr);

        const auto& signature = static_cast<const GraphicsPipeline*>(pPipeline)->Signature();

        pData->spillThreshold          = signature.spillThreshold;
        pData->vertexOffsetRegAddr     = signature.vertexOffsetRegAddr;
        pData->drawIndexRegAddr        = signature.drawIndexRegAddr;
        pData->vertexBufTableRegAddr   = signature.vertexBufTableRegAddr;
        pData->meshDispatchDimsRegAddr = signature.meshDispatchDimsRegAddr;
        pData->meshRingIndexAddr       = signature.meshRingIndexAddr;
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

        pData->spillThreshold          = signature.spillThreshold;
        pData->vertexOffsetRegAddr     = signature.vertexOffsetRegAddr;
        pData->drawIndexRegAddr        = signature.drawIndexRegAddr;
        pData->vertexBufTableRegAddr   = signature.vertexBufTableRegAddr;
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
    viewInfo.stride         = sizeof(uint32);
    viewInfo.range          = (stageCount * sizeof(uint32) * dwordsPerStage);

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

} // Gfx9
} // Pal
