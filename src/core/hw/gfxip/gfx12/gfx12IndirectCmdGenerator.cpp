/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2023-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/gfx12/gfx12CmdUtil.h"
#include "core/hw/gfxip/gfx12/gfx12ComputePipeline.h"
#include "core/hw/gfxip/gfx12/gfx12Device.h"
#include "core/hw/gfxip/gfx12/gfx12GraphicsPipeline.h"
#include "core/hw/gfxip/gfx12/gfx12HybridGraphicsPipeline.h"
#include "core/hw/gfxip/gfx12/gfx12IndirectCmdGenerator.h"
#include "core/hw/gfxip/gfx12/gfx12UserDataLayout.h"

using namespace Util;

namespace Pal
{
namespace Gfx12
{

// =====================================================================================================================
// Shift user data register offset from PERSISTENT_SPACE_START-based to COMPUTE_USER_DATA_0-based.
// It is designed for EiDispatchTaskMesh submitting to ACE
static uint8 AceTaskRegOffset(
    uint16 regOffset)
{
    uint16 aceTaskRegOffset = UserDataNotMapped;

    if (regOffset != UserDataNotMapped)
    {
        aceTaskRegOffset = regOffset + PERSISTENT_SPACE_START - mmCOMPUTE_USER_DATA_0;
    }

    PAL_ASSERT(uint8(aceTaskRegOffset) == aceTaskRegOffset);

    return uint8(aceTaskRegOffset);
}

// =====================================================================================================================
// Shift user data register offset from PERSISTENT_SPACE_START-based to mmSPI_SHADER_USER_DATA_HS_0-based.
// It is designed for EiDraw & EiDrawIndexed submitting to GFX
static uint8 GfxHsRegOffset(
    uint16 regOffset)
{
    uint16 gfxHullRegOffset = UserDataNotMapped;

    if (regOffset != UserDataNotMapped)
    {
        gfxHullRegOffset = regOffset + PERSISTENT_SPACE_START - mmSPI_SHADER_USER_DATA_HS_0;
    }

    PAL_ASSERT(uint8(gfxHullRegOffset) == gfxHullRegOffset);

    return uint8(gfxHullRegOffset);
}

// =====================================================================================================================
size_t IndirectCmdGenerator::GetSize(
    const IndirectCmdGeneratorCreateInfo& createInfo)
{
    // The required size of a command generator is the object size plus space for the parameter buffer data and the
    // client data buffer. The client data buffer and the param buffer data will immediately follow the object in
    // system memory.
    return (sizeof(IndirectCmdGenerator) + (sizeof(IndirectParamData) * createInfo.paramCount) +
            (sizeof(IndirectParam) * createInfo.paramCount));
}

// =====================================================================================================================
IndirectCmdGenerator::IndirectCmdGenerator(
    const Device&                         device,
    const IndirectCmdGeneratorCreateInfo& createInfo)
    :
    Pal::IndirectCmdGenerator(device, createInfo),
    m_pParamData(reinterpret_cast<IndirectParamData*>(this + 1)),
    m_pCreationParam(reinterpret_cast<IndirectParam*>(m_pParamData + createInfo.paramCount)),
    m_flags{}
{
    m_properties.maxUserDataEntries = device.Parent()->ChipProperties().gfxip.maxUserDataEntries;
    memcpy(&m_properties.indexTypeTokens[0], &createInfo.indexTypeTokens[0], sizeof(createInfo.indexTypeTokens));
    memcpy(m_pCreationParam, createInfo.pParams, sizeof(IndirectParam) * createInfo.paramCount);

    InitParamBuffer(createInfo);
}

// =====================================================================================================================
void IndirectCmdGenerator::InitParamBuffer(
    const IndirectCmdGeneratorCreateInfo& createInfo)
{
    m_properties.userDataArgBufOffsetBase = UINT32_MAX;

    memset(m_pParamData, 0, (sizeof(IndirectParamData) * ParameterCount()));

    uint32 argBufOffset = 0;

    // We need to remember the argument buffer offset for BindIndexData because DrawIndexed is the parameter which
    // needs to process it (because DRAW_INDEX_2 packets issue a draw and bind an IB address simultaneously). If we
    // don't encounter a BindIndexData parameter for this generator, we'll fall back to using the suboptimal
    // DRAW_INDEX_OFFSET_2 packet because that packet doesn't require us to know the full index buffer GPU address.
    uint32 argBufOffsetIndices = 0;

    m_flags.useOffsetModeVertexBuffer = createInfo.bindVertexInOffsetMode;

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
            m_flags.containIndexBuffer = true;
        }
        else
        {
            switch (param.type)
            {
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 889
            case IndirectParamType::Padding:
                m_pParamData[p].type = IndirectOpType::Skip;
                break;
#endif
            case IndirectParamType::Dispatch:
                m_pParamData[p].type = IndirectOpType::Dispatch;
                break;
            case IndirectParamType::Draw:
                m_pParamData[p].type = IndirectOpType::DrawIndexAuto;
                m_flags.useConstantDrawIndex = param.drawData.constantDrawIndex;
                break;
            case IndirectParamType::DrawIndexed:
                // See comment above for information on how we handle BindIndexData.
                m_pParamData[p].type    = ContainIndexBuffer() ? IndirectOpType::DrawIndex2
                                                               : IndirectOpType::DrawIndexOffset2;
                m_pParamData[p].data[0] = argBufOffsetIndices;
                m_flags.useConstantDrawIndex = param.drawData.constantDrawIndex;
                break;
            case IndirectParamType::DispatchMesh:
                m_pParamData[p].type    = IndirectOpType::DispatchMesh;
                m_flags.useConstantDrawIndex = param.drawData.constantDrawIndex;
                break;
            case IndirectParamType::SetUserData:
                m_pParamData[p].type = param.userData.isIncConst ? IndirectOpType::SetIncConst
                                                                 : IndirectOpType::SetUserData;
                m_pParamData[p].data[0] = param.userData.firstEntry;
                m_pParamData[p].data[1] = param.userData.entryCount;
                m_flags.containIncrementConstant |= param.userData.isIncConst;
                // The user-data watermark tracks the highest index (plus one) of user-data entries modified by this
                // command generator.
                m_properties.userDataWatermark        = Max((param.userData.firstEntry + param.userData.entryCount),
                                                            m_properties.userDataWatermark);
                // Marks where SetUserData Ops begin.
                m_properties.userDataArgBufOffsetBase = Min(m_properties.userDataArgBufOffsetBase, argBufOffset);

                // Also, we need to track the mask of which user-data entries this command-generator touches.
                WideBitfieldSetRange(m_touchedUserData, param.userData.firstEntry, param.userData.entryCount);
                break;
            case IndirectParamType::BindVertexData:
                m_pParamData[p].type    = IndirectOpType::VertexBufTableSrd;
                m_pParamData[p].data[0] = (param.vertexData.bufferId * DwordsPerBufferSrd);
                // Update the vertex buffer table size to indicate to the command-generation shader that the vertex
                // buffer is being updated by this generator.
                m_properties.vertexBufTableSize = (DwordsPerBufferSrd * MaxVertexBuffers);
                break;
            default:
                PAL_NOT_IMPLEMENTED();
                break;
            }

            m_pParamData[p].argBufOffset = argBufOffset;
            m_pParamData[p].argBufSize   = param.sizeInBytes;
        }
        argBufOffset += param.sizeInBytes;
    }

    // We reset userDataArgBufOffsetBase if it's value did not change.
    if (m_properties.userDataArgBufOffsetBase == UINT32_MAX)
    {
        m_properties.userDataArgBufOffsetBase = 0;
    }

    m_properties.argBufStride = Max(argBufOffset, createInfo.strideInBytes);
}

// =====================================================================================================================
// Separate function for handling the SetUserDataOps from ExecuteIndirect.
uint32 IndirectCmdGenerator::ManageUserDataOp(
    const UserDataLayout*      pUserDataLayout,
    ExecuteIndirectMeta*       pMeta,
    ExecuteIndirectPacketInfo* pPacketInfo,
    const uint32               vertexBufTableDwords,
    const bool                 isGfx,
    const bool                 isTaskOnAce
    ) const
{
    const uint32                    cmdCount   = ParameterCount();
    const IndirectParamData*const   pParamData = GetIndirectParamData();
    const GeneratorProperties&      properties = Properties();
    ExecuteIndirectMetaData*        pMetaData  = pMeta->GetMetaData();

    const uint32 spillThreshold = pUserDataLayout->GetSpillThreshold();
    const bool   userDataSpills = (spillThreshold < properties.maxUserDataEntries);

    // For Graphics, we will need to find the real stageUsageMask based on the UserData RegMapping for the current
    // workload. For Compute, we know it's just the 1.
    uint32 stageUsageMask = isGfx ? 0 : 1;

    uint32 incConstRegCount = 0;

    if (WideBitfieldIsAnyBitSet(TouchedUserDataEntries()))
    {
        const uint32* pMap        = pUserDataLayout->GetMapping();
        const uint32  maxMapWords = pUserDataLayout->GetNumMapWords();
        uint32 argSizeDw          = 0;

        if (userDataSpills)
        {
            // Initialize the Look-Up Table for the (VBTable + UserDataSpill) Buffer we create for ExecuteIndirect Op.
            // Since Look-up for modification is only required for Spilled UserData Entries and not the VertexBuffer
            // Table we will exclude the part of the Buffer which contains the VBtable and UserDataEntries not spilled
            // i.e. up to the SpillThreshold.
            pMeta->InitLut();
            pMeta->SetMemCpyRange(pPacketInfo->vbTableSizeDwords, pPacketInfo->vbTableSizeDwords + spillThreshold);
        }

        for (uint32 cmdIndex = 0; cmdIndex < cmdCount; cmdIndex++)
        {
            if (pParamData[cmdIndex].type != IndirectOpType::SetUserData)
            {
                continue;
            }

            // Offset for the first UserData entry/entries to Set. If the first SetUserData is lowest then offset is 0.
            const uint32 argBufOffsetDw = (pParamData[cmdIndex].argBufOffset >> 2) -
                                          (properties.userDataArgBufOffsetBase >> 2);
            const uint32 firstEntry = pParamData[cmdIndex].data[0];
            const uint32 entryCount = pParamData[cmdIndex].data[1];

            // This op's argument space must exactly fit its user-data values, we assume this below.
            PAL_ASSERT(pParamData[cmdIndex].argBufSize == (entryCount * sizeof(uint32)));

            const uint32 lastEntry = firstEntry + entryCount - 1;

            // Step 1: Update UserData Entries that lie in the UserDataRegister range.
            // Graphics has multiple Shader Stages while Compute has only one.
            static_assert(NumHwShaderStagesGfx == 3);
            const uint32 numHwShaderStgs = isGfx ? NumHwShaderStagesGfx : 1;

            // "entry" can be any virtual user-data index, even one below the spill threshold.
            // We should only load it if it's within this op's entry range.
            // Every valid entry could contain a RegOffset for any/all of the possible shader stages.
            for (uint32 entry = firstEntry; (entry <= lastEntry) && (entry < maxMapWords); entry++)
            {
                // No valid UserDataRegOffset is mapped in this entry move on to the next.
                if (pMap[entry] == 0)
                {
                    continue;
                }
                // Check this entry for all possible Shader stages.
                for (uint32 stgId = 0; stgId < numHwShaderStgs; stgId++)
                {
                    // mappingShift masks the UserDataRegOffset of previous stage so that .regOffset only shows the
                    // 10 bits corresponding to the current stage.
                    const uint32 mappingShift = 10 * stgId;
                    UserDataReg regMapping = { .u32All = pMap[entry] >> mappingShift };

                    uint32 regOffset = regMapping.regOffset;

                    constexpr uint32 StgGs = 0;
                    constexpr uint32 StgHs = 1;
                    constexpr uint32 StgPs = 2;

                    if (regOffset != 0)
                    {
                        // In Gfx12, it is not guaranteed say that stg[0]UserData will contain PS, stg[1] GS and
                        // stg[2] HS. It could be that have GS + PS, but only the stg[0]UserData slots are populated.
                        // So we need to individually check every RegOffset for what HW Shader stage they are actually
                        // referring to and mark them here.
                        if (isGfx)
                        {
                            if (InRange(regOffset + PERSISTENT_SPACE_START,
                                        mmSPI_SHADER_USER_DATA_GS_0,
                                        mmSPI_SHADER_USER_DATA_GS_31))
                            {
                                stageUsageMask |= (1 << StgGs);
                            }
                            else if (InRange(regOffset + PERSISTENT_SPACE_START,
                                             mmSPI_SHADER_USER_DATA_HS_0,
                                             mmSPI_SHADER_USER_DATA_HS_31))
                            {
                                if (m_device.Parent()->ChipProperties().pfpUcodeVersion >= EiV2HsHwRegFixPfpVersion)
                                {
                                    regOffset = GfxHsRegOffset(regOffset);
                                }
                                stageUsageMask |= (1 << StgHs);
                            }
                            else if (InRange(regOffset + PERSISTENT_SPACE_START,
                                             mmSPI_SHADER_USER_DATA_PS_0,
                                             mmSPI_SHADER_USER_DATA_PS_31))
                            {
                                stageUsageMask |= (1 << StgPs);
                            }
                        }

                        // argBufIdx is the dword at which we can find this UserData Entry to update in the ArgBuffer.
                        const uint32 argBufIdx = argBufOffsetDw + (entry - firstEntry);
                        argSizeDw = Max(argSizeDw, argBufIdx + 1);

                        // Since the argBufOffset is relative to the owning Cmd's (this case SetUserData's) offset. The
                        // argBufIdx and argSizeDw cannot be greater than the API NumUserDataRegisters.
                        PAL_ASSERT(argBufIdx <  (isGfx ? NumUserDataRegisters : NumUserDataRegistersAce));
                        PAL_ASSERT(argSizeDw <= (isGfx ? NumUserDataRegisters : NumUserDataRegistersAce));

                        // Calculate which UserData Register for this stage needs to be modified with the new value.
                        pMetaData->userData[(NumUserDataRegisters * stgId) + argBufIdx] = regOffset;
                    }
                }
            }

            // Step 2: Issue a MemCopy command to the CP to update the UserDataSpill table. This MemCopy will be done
            // by the CP during execution of the ExecuteIndirectV2 PM4 based on the MemCopy structures.
            if (spillThreshold <= lastEntry)
            {
                // In cases like the DispatchRays Cmd call DXC forces spilling and the spillThreshold can be 0.
                // spillOffset is the offset into the ArgBuffer from which point forward UserData entries would need to
                // be copied into the SpillTable.
                const uint32 spillOffset = (spillThreshold > firstEntry) ? spillThreshold - firstEntry : 0;
                const uint32 spillCount  = entryCount - spillOffset;
                // argBufIdx is the dword at which we can find the first spilling UserData Entry in the ArgBuffer.
                const uint32 argBufIdx   = argBufOffsetDw + spillOffset;
                // (VBTable + UserDataSpill) Buffer saves space for VBTable and also the UserData entries that are
                // copied onto registers before starting with the spilled entries.
                const uint32 spillBufIdx = vertexBufTableDwords + firstEntry + spillOffset;

                pMeta->SetLut(spillBufIdx, argBufIdx, spillCount);
            }
        }

        if (ContainIncrementingConstant())
        {
            for (uint32 cmdIndex = 0; cmdIndex < cmdCount; cmdIndex++)
            {
                if (pParamData[cmdIndex].type != IndirectOpType::SetIncConst)
                {
                    continue;
                }

                const uint32 incConstEntry   = pParamData[cmdIndex].data[0];
                const uint32 numHwShaderStgs = isGfx ? NumHwShaderStagesGfx : 1;

                if (pMap[incConstEntry] == 0)
                {
                    continue;
                }

                // Check this entry for all possible Shader stages.
                for (uint32 stgId = 0; stgId < numHwShaderStgs; stgId++)
                {
                    const uint32 mappingShift = 10 * stgId;
                    UserDataReg  regMapping   = { .u32All = pMap[incConstEntry] >> mappingShift };
                    const uint32 regOffset    = regMapping.regOffset;

                    if (regOffset != 0)
                    {
                        // Translation to COMPUTE_USER_DATA_0-based offset if filling into EiDispatchTaskMesh on Ace
                        pMetaData->incConstReg[incConstRegCount++] = isTaskOnAce ?
                                                                     AceTaskRegOffset(regOffset) : regOffset;
                    }
                }
                // There can only be one IndirectOpType::SetIncConst in an IndirectCmdGenerator.
                break;
            }
        }

        uint32 initCount   = 0;
        uint32 updateCount = 0;
        if (userDataSpills)
        {
            pMeta->ComputeMemCopyStructures(pPacketInfo->vbTableSizeDwords + properties.userDataWatermark,
                                            &initCount,
                                            &updateCount);
        }

        pMetaData->initMemCopy.count   = initCount;
        pMetaData->updateMemCopy.count = updateCount;
        pMetaData->userDataOffset      = properties.userDataArgBufOffsetBase;
        pMetaData->userDataDwCount     = argSizeDw;
        pMetaData->incConstRegCount    = incConstRegCount;
    }
    return stageUsageMask;
}

// =====================================================================================================================
// The PacketOp stores what operation this ExecuteIndirectV2 PM4 will be programmed to perform and the MetaData struct
// stores some data to program the PM4 ordinals and other data like the Look-Up Table implemented to help add Spilled
// UserData entries to a buffer in memory.
void IndirectCmdGenerator::PopulateExecuteIndirectParams(
    const IPipeline*           pPipeline,
    const bool                 isGfx,
    const bool                 onAceQueue,
    ExecuteIndirectPacketInfo* pPacketInfo,
    ExecuteIndirectMeta*       pMeta,
    uint32                     vbTableDwords,
    const EiDispatchOptions&   options,
    const EiUserDataRegs&      regs
    ) const
{
    const uint32                  cmdCount   = ParameterCount();
    const IndirectParamData*const pParamData = GetIndirectParamData();
    ExecuteIndirectMetaData*      pMetaData  = pMeta->GetMetaData();
    ExecuteIndirectOp*            pPacketOp  = pMeta->GetOp();

    const uint32 pfpVersion = m_device.Parent()->ChipProperties().pfpUcodeVersion;

    const auto* pGfxPipeline    = isGfx ? static_cast<const GraphicsPipeline*>(pPipeline) : nullptr;
    const auto* pCsPipeline     = isGfx ? nullptr : static_cast<const ComputePipeline*>(pPipeline);
    const auto* pHybridPipeline = isGfx ? static_cast<const HybridGraphicsPipeline*>(pPipeline) : nullptr;

    const UserDataLayout* pTaskUserDataLayout = (pHybridPipeline != nullptr) ? pHybridPipeline->TaskUserDataLayout() :
                                                nullptr;

    const UserDataLayout* pUserDataLayout = isGfx ? static_cast<const UserDataLayout*>(pGfxPipeline->UserDataLayout())
                                                  : static_cast<const UserDataLayout*>(pCsPipeline->UserDataLayout());

    const bool hasTaskShader = isGfx ? pGfxPipeline->HasTaskShader() : false;
    const bool isTaskEnabled = ((Type() == GeneratorType::DispatchMesh) && hasTaskShader);

    const bool isTessEnabled = isGfx ? pGfxPipeline->IsTessEnabled() : false;

    const bool hsHwRegSupport      = (pfpVersion >= EiV2HsHwRegFixPfpVersion);
    const bool workGroupRegSupport = (pfpVersion >= EiV2WorkGroupRegFixPfpVersion);

    for (uint32 cmdIndex = 0; cmdIndex < cmdCount; cmdIndex++)
    {
        if (pParamData[cmdIndex].type == IndirectOpType::VertexBufTableSrd)
        {
            // data[0] here indicates offset into table where SRD is written.
            vbTableDwords = Max(pParamData[cmdIndex].data[0] + DwordsPerBufferSrd, vbTableDwords);
        }
    }

    // Set VertexBuffer parameters.
    if (vbTableDwords > 0)
    {
        pPacketInfo->vbTableSizeDwords = vbTableDwords;
        pPacketInfo->vbTableRegOffset  = (isTessEnabled && hsHwRegSupport) ?
                                         GfxHsRegOffset(regs.vtxTableReg) : regs.vtxTableReg;
    }

    uint32 vbSlotMask = BitfieldGenMask(vbTableDwords / DwordsPerBufferSrd);

    // If this call was made by the UniversalCmdBuffer processing the Task shader part on Compute Queue we need to
    // consider the UserDataOp with TaskUserDataLayout.
    const bool isTaskOnAceQueue = (isGfx && isTaskEnabled && onAceQueue);

    // We handle all SetUserData ops here. The other kinds of indirect ops will be handled after.
    const uint32 stageUsageMask = isTaskOnAceQueue ?
        ManageUserDataOp(pTaskUserDataLayout, pMeta, pPacketInfo, vbTableDwords, false, isTaskOnAceQueue) :
        ManageUserDataOp(pUserDataLayout,     pMeta, pPacketInfo, vbTableDwords, isGfx, isTaskOnAceQueue);

    pMetaData->stageUsageCount = CountSetBits(stageUsageMask);

    // For a case where no HW Shader Stages are active we do not want userDataScatterMode to be uint(-1).
    if (stageUsageMask != 0)
    {
        pMetaData->userDataScatterMode = pMetaData->stageUsageCount - 1;
    }

    // Now loop over the indirect ops. Only one OpType between these Dispatches/Draws is valid over one loop.
    for (uint32 cmdIndex = 0; cmdIndex < cmdCount; cmdIndex++)
    {
        switch (pParamData[cmdIndex].type)
        {
        case IndirectOpType::Dispatch:
        {
            PAL_ASSERT(pCsPipeline != nullptr);
            static_assert(static_cast<uint8>(operation__mec_execute_indirect_v2__dispatch) ==
                          static_cast<uint8>(operation__pfp_execute_indirect_v2__dispatch));
            pMetaData->opType = operation__pfp_execute_indirect_v2__dispatch;

            pPacketOp->dispatch.dataOffset                 = pParamData[cmdIndex].argBufOffset;
            pPacketOp->dispatch.locData.numWorkGroup       =
                workGroupRegSupport ? regs.numWorkGroupReg : 0;
            pPacketOp->dispatch.locData.numWorkGroupEnable =
                workGroupRegSupport ? (regs.numWorkGroupReg != UserDataNotMapped) : 0;
            pPacketOp->dispatch.locData.commandIndex       =
                pMeta->ProcessCommandIndex(UserDataNotMapped, UseConstantDrawIndex(), false);

            pPacketOp->dispatch.dispatchInitiator.bits.COMPUTE_SHADER_EN  = 1;
            pPacketOp->dispatch.dispatchInitiator.bits.FORCE_START_AT_000 = 1;
            pPacketOp->dispatch.dispatchInitiator.bits.PING_PONG_EN       = options.pingPongEnable;
            pPacketOp->dispatch.dispatchInitiator.bits.TUNNEL_ENABLE      = options.usesDispatchTunneling;
            pPacketOp->dispatch.dispatchInitiator.bits.INTERLEAVE_2D_EN   = options.enable2dInterleave;
            pPacketOp->dispatch.dispatchInitiator.bits.CS_W32_EN          = options.isWave32;
            break;
        }

        case IndirectOpType::DrawIndexAuto:
        {
            pMetaData->opType = operation__pfp_execute_indirect_v2__draw;

            pPacketOp->draw.dataOffset           = pParamData[cmdIndex].argBufOffset;
            pPacketOp->draw.locData.startVertex  =
                (isTessEnabled && hsHwRegSupport) ? GfxHsRegOffset(regs.vtxOffsetReg) : regs.vtxOffsetReg;
            pPacketOp->draw.locData.startInst    =
                (isTessEnabled && hsHwRegSupport) ? GfxHsRegOffset(regs.instOffsetReg) : regs.instOffsetReg;
            pPacketOp->draw.locData.drawRegsInHs = (isTessEnabled && hsHwRegSupport);
            pPacketOp->draw.locData.commandIndex =
                pMeta->ProcessCommandIndex(regs.drawIndexReg, UseConstantDrawIndex(), true);
            pPacketOp->draw.drawInitiator.bits.SOURCE_SELECT = DI_SRC_SEL_AUTO_INDEX;
            break;
        }

        case IndirectOpType::DrawIndex2:
        {
            // location of INDEX_ATTRIBUTES
            pMetaData->fetchIndexAttributes  = true;
            pMetaData->indexAttributesOffset = pParamData[cmdIndex].data[0];
            [[fallthrough]];
        }

        case IndirectOpType::DrawIndexOffset2:
        {
            pMetaData->opType = operation__pfp_execute_indirect_v2__draw_index;

            pPacketOp->drawIndexed.dataOffset           = pParamData[cmdIndex].argBufOffset;
            pPacketOp->drawIndexed.locData.startVertex  =
                (isTessEnabled && hsHwRegSupport) ? GfxHsRegOffset(regs.vtxOffsetReg) : regs.vtxOffsetReg;
            pPacketOp->drawIndexed.locData.startInst    =
                (isTessEnabled && hsHwRegSupport) ? GfxHsRegOffset(regs.instOffsetReg) : regs.instOffsetReg;
            pPacketOp->drawIndexed.locData.drawRegsInHs = (isTessEnabled && hsHwRegSupport);
            pPacketOp->drawIndexed.locData.commandIndex =
                pMeta->ProcessCommandIndex(regs.drawIndexReg, UseConstantDrawIndex(), true);
            pPacketOp->drawIndexed.drawInitiator.bits.SOURCE_SELECT = DI_SRC_SEL_DMA;
            break;
        }

        case IndirectOpType::VertexBufTableSrd:
        {
            const uint32 idx = pMetaData->buildSrd.count++;
            pMetaData->buildSrd.srcOffsets[idx] = pParamData[cmdIndex].argBufOffset;
            pMetaData->buildSrd.dstOffsets[idx] = static_cast<uint16>(pParamData[cmdIndex].data[0] * sizeof(uint32));

            // Remove VB slots that will be copied by Build SRD Op/s from being considered in VB MemCpy.
            vbSlotMask &= ~(1u << pParamData[cmdIndex].data[0]);
            break;
        }
        case IndirectOpType::DispatchMesh:
        {
            EiDispatchTaskMesh* pDispatchTaskMesh = &pPacketOp->dispatchTaskMesh;

            if (onAceQueue)
            {
                pMetaData->opType = operation__mec_execute_indirect_v2__dispatch_taskmesh;

                pDispatchTaskMesh->dataOffset                   = pParamData[cmdIndex].argBufOffset;
                pDispatchTaskMesh->locData.ringEntry            = AceTaskRegOffset(regs.aceMeshTaskRingIndexReg);
                pDispatchTaskMesh->locData.linearDispatchEnable = options.isLinearDispatch;
                pDispatchTaskMesh->locData.xyzDim               = AceTaskRegOffset(regs.aceTaskDispatchDimsReg);
                pDispatchTaskMesh->locData.xyzDimEnable         =
                    (regs.aceTaskDispatchDimsReg != UserDataNotMapped);
                pDispatchTaskMesh->locData.commandIndex         =
                    pMeta->ProcessCommandIndex(AceTaskRegOffset(regs.aceTaskDispatchIndexReg),
                                               UseConstantDrawIndex(),
                                               true);

                pDispatchTaskMesh->dispatchInitiator.bits.COMPUTE_SHADER_EN = 1;
                pDispatchTaskMesh->dispatchInitiator.bits.AMP_SHADER_EN     = 1;
                pDispatchTaskMesh->dispatchInitiator.bits.ORDER_MODE        = 1;
                pDispatchTaskMesh->dispatchInitiator.bits.PING_PONG_EN      = options.pingPongEnable;
                pDispatchTaskMesh->dispatchInitiator.bits.TUNNEL_ENABLE     = options.usesDispatchTunneling;
                pDispatchTaskMesh->dispatchInitiator.bits.INTERLEAVE_2D_EN  = options.enable2dInterleave;
                pDispatchTaskMesh->dispatchInitiator.bits.CS_W32_EN         = options.isWave32;
            }
            else
            {
                pMetaData->opType = isTaskEnabled ? operation__pfp_execute_indirect_v2__dispatch_taskmesh
                                                  : operation__pfp_execute_indirect_v2__dispatch_mesh;

                pDispatchTaskMesh->dataOffset                   = pParamData[cmdIndex].argBufOffset;
                pDispatchTaskMesh->locData.ringEntry            = regs.meshRingIndexReg;
                pDispatchTaskMesh->locData.linearDispatchEnable = options.isLinearDispatch;
                pDispatchTaskMesh->locData.xyzDim               = regs.meshDispatchDimsReg;
                pDispatchTaskMesh->locData.xyzDimEnable         =
                    (regs.meshDispatchDimsReg != UserDataNotMapped);
                pDispatchTaskMesh->locData.commandIndex         =
                    pMeta->ProcessCommandIndex(regs.drawIndexReg, UseConstantDrawIndex(), true);

                pDispatchTaskMesh->drawInitiator.bits.SOURCE_SELECT = DI_SRC_SEL_AUTO_INDEX;
            }
            break;
        }
        case IndirectOpType::Skip:
        case IndirectOpType::SetUserData:
        case IndirectOpType::SetIncConst:
            // Nothing to do here.
            break;

        default:
            // What's this?
            PAL_ASSERT_ALWAYS();
            break;
        }
    }

    // If the global SpillTable needs to be used instead of just the local SpillTable, setup initMemCopy for VB SRDs
    // setup from CPU side. In CP FW code, global SpillTable use (called dynamicSpillMode) is enabled and required when
    // (UpdateMemCopyCount | BuildSrdCount != 0), so we use the same check here.
    if ((vbSlotMask != 0) && ((pMetaData->buildSrd.count | pMetaData->updateMemCopy.count) > 0))
    {
        pMeta->ComputeVbSrdInitMemCopy(vbSlotMask);
    }

    if (pfpVersion >= EiV2OffsetModeVertexBindingFixPfpVersion)
    {
        // This bit must be set (as long as we have indirect vertex buffer binding) for offset mode binding.
        pMetaData->vertexOffsetModeEnable = m_flags.useOffsetModeVertexBuffer && (pMetaData->buildSrd.count > 0);
    }

    // This bit must be set (as long as we have indirect vertex buffer binding) for numRecords calculation.
    pMetaData->vertexBoundsCheckEnable = (pMetaData->buildSrd.count > 0);
}

} // Gfx12
} // Pal
