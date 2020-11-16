/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/gfx6/g_gfx6PalSettings.h"
#include "core/hw/gfxip/gfx6/gfx6Device.h"
#include "core/hw/gfxip/gfx6/gfx6Pm4Optimizer.h"
#include "palAutoBuffer.h"

using namespace Util;

namespace Pal
{
namespace Gfx6
{

// =====================================================================================================================
// Checks the current register state versus the next written value.  Determines whether a new SET command is necessary,
// and updates the register state. Returns true if the given register value must be written to HW.
static bool UpdateRegState(
    uint32    newRegVal,
    RegState* pCurRegState) // [in,out] Current state of register being set, will be updated.
{
    bool mustKeep = false;

    // We must issue the write if:
    // - The new value is different than the old value.
    // - The previous state is invalid.
    // - We must always write this register.
    if ((pCurRegState->value != newRegVal) ||
        (pCurRegState->flags.valid == 0)   ||
        (pCurRegState->flags.mustWrite == 1))
    {
        pCurRegState->flags.valid = 1;
        pCurRegState->value       = newRegVal;

        mustKeep = true;
    }

    return mustKeep;
}

// =====================================================================================================================
Pm4Optimizer::Pm4Optimizer(
    const Device& device)
    :
    m_cmdUtil(device.CmdUtil()),
    m_chipFamily(device.Parent()->ChipProperties().gfxLevel),
    m_waShaderSpiWriteShaderPgmRsrc2Ls(device.WaShaderSpiWriteShaderPgmRsrc2Ls()),
    m_waTcCompatZRange(device.WaTcCompatZRange())
#if PAL_ENABLE_PRINTS_ASSERTS
    , m_dstContainsSrc(false)
#endif
{
    Reset();
}

// =====================================================================================================================
// Resets the optimizer so that it's ready to begin optimizing a new command stream. Each time this is called we have
// to reset all mustWrite flags which is a bit wasteful but we'd rather not add two more big arrays to this class.
void Pm4Optimizer::Reset()
{
    // Reset the context register state.
    memset(m_cntxRegs, 0, sizeof(m_cntxRegs));

    // Mark the "vector" context registers as mustWrite. There are some PA registers that require setting the entire
    // vector if any register in the vector needs to change. According to the PA and SC hardware team, these registers
    // consist of the viewport scale/offset regs, viewport scissor regs, and guardband regs.
    constexpr uint32 VportStart = mmPA_CL_VPORT_XSCALE     - CONTEXT_SPACE_START;
    constexpr uint32 VportEnd   = mmPA_CL_VPORT_ZOFFSET_15 - CONTEXT_SPACE_START;
    for (uint32 regOffset = VportStart; regOffset <= VportEnd; ++regOffset)
    {
        m_cntxRegs[regOffset].flags.mustWrite = 1;
    }

    constexpr uint32 VportScissorStart = mmPA_SC_VPORT_SCISSOR_0_TL - CONTEXT_SPACE_START;
    constexpr uint32 VportScissorEnd   = mmPA_SC_VPORT_ZMAX_15      - CONTEXT_SPACE_START;
    for (uint32 regOffset = VportScissorStart; regOffset <= VportScissorEnd; ++regOffset)
    {
        m_cntxRegs[regOffset].flags.mustWrite = 1;
    }

    constexpr uint32 GuardbandStart = mmPA_CL_GB_VERT_CLIP_ADJ - CONTEXT_SPACE_START;
    constexpr uint32 GuardbandEnd   = mmPA_CL_GB_HORZ_DISC_ADJ - CONTEXT_SPACE_START;
    for (uint32 regOffset = GuardbandStart; regOffset <= GuardbandEnd; ++regOffset)
    {
        m_cntxRegs[regOffset].flags.mustWrite = 1;
    }

    // This workaround adds some writes to DB_Z_INFO which are preceded by a COND_EXEC. Make sure we don't optimize
    // away writes to this register, which would cause a hang or incorrect skipping of commands.
    if (m_waTcCompatZRange)
    {
        constexpr uint32 dbZInfoIdx = mmDB_Z_INFO - CONTEXT_SPACE_START;

        m_cntxRegs[dbZInfoIdx].flags.mustWrite = 1;
    }

    // Reset the SH register state.
    memset(m_shRegs,   0, sizeof(m_shRegs));

    // Reset the SET_BASE address state
    memset(&m_setBaseStateGfx, 0, sizeof(m_setBaseStateGfx));
    memset(&m_setBaseStateCompute, 0, sizeof(m_setBaseStateCompute));

    // Some Gfx7 chips have an SPI bug whose workaround requires redundant writes to the SPI_SHADER_PGM_RSRC2_LS
    // register to occur with a write to SPI_SHADER_PGM_RSRC1 in between. Make sure that we don't optimize away a
    // necessary write to either of those two registers. See: Gfx6PipelineChunkLsHs::BuildPm4Headers().
    if (m_waShaderSpiWriteShaderPgmRsrc2Ls)
    {
        constexpr uint32 SpiShaderPgmRsrc1LsIdx = mmSPI_SHADER_PGM_RSRC1_LS - PERSISTENT_SPACE_START;
        constexpr uint32 SpiShaderPgmRsrc2LsIdx = mmSPI_SHADER_PGM_RSRC2_LS - PERSISTENT_SPACE_START;

        m_shRegs[SpiShaderPgmRsrc1LsIdx].flags.mustWrite = 1;
        m_shRegs[SpiShaderPgmRsrc2LsIdx].flags.mustWrite = 1;
    }
}

// =====================================================================================================================
// This functions should be called by Gfx6 CmdStream's "Write" functions to determine if it can skip writing certain
// packets up-front.
bool Pm4Optimizer::MustKeepSetContextReg(
    uint32 regAddr,
    uint32 regData)
{
    return UpdateRegState(regData, m_cntxRegs + (regAddr - CONTEXT_SPACE_START));
}

// =====================================================================================================================
// This functions should be called by Gfx6 CmdStream's "Write" functions to determine if it can skip writing certain
// packets up-front.
bool Pm4Optimizer::MustKeepSetShReg(
    uint32 regAddr,
    uint32 regData)
{
    return UpdateRegState(regData, m_shRegs + (regAddr - PERSISTENT_SPACE_START));
}

// =====================================================================================================================
// Evaluates a context reg RMW operation and returns true if it can't be skipped.
bool Pm4Optimizer::MustKeepContextRegRmw(
    uint32 regAddr,
    uint32 regMask,
    uint32 regData)
{
    const uint32 regOffset = regAddr - CONTEXT_SPACE_START;
    auto*const   pRegState = &m_cntxRegs[regOffset];

    bool mustKeep = true;

    // We must keep this RMW if we haven't done a SET on this register at least once because we need a fully defined
    // regState value to compute newRegVal. If we tried to do it anyway, the fact that our regMask will have some bits
    // disabled means that we would be setting regState's value to something partially invalid which may cause us to
    // skip needed packets in the future.
    if (pRegState->flags.valid == 1)
    {
        // Computed according to the formula stated in the definition of CmdUtil::BuildContextRegRmw.
        const uint32 newRegVal = (pRegState->value & ~regMask) | (regData & regMask);

        mustKeep = UpdateRegState(newRegVal, pRegState);
    }

    return mustKeep;
}

// =====================================================================================================================
// Thi functions should be called by Gfx9 CmdStream's "Write" functions to determine if it can skip writing certain
// a SET_BASE packet up-front.
bool Pm4Optimizer::MustKeepSetBase(
    gpusize         address,
    uint32          index,
    PM4ShaderType   shaderType)
{
    PAL_ASSERT(address != 0);
    PAL_ASSERT(index <= MaxSetBaseIndex);
    PAL_ASSERT((shaderType == ShaderCompute) || (shaderType == ShaderGraphics));

    SetBaseState* pBaseState = nullptr;

    if ((index == BASE_INDEX_DISPATCH_INDIRECT) &&
        (shaderType == ShaderCompute))
    {
        // According to the PM4 packet spec, only the patch table base index
        // has a different base for ShaderGraphics and ShaderCompute
        pBaseState = &m_setBaseStateCompute;
    }
    else
    {
        pBaseState = &m_setBaseStateGfx[index];
    }

    const bool mustKeep = (pBaseState->address != address);

    pBaseState->address = address;

    return mustKeep;
}

// =====================================================================================================================
// Writes an optimized version of the given SET_DATA packet into pCmdSpace (along with the appropriate register data).
// Returns a pointer to the next unused DWORD in pCmdSpace.
uint32* Pm4Optimizer::WriteOptimizedSetSeqShRegs(
    const PM4CMDSETDATA& setData,
    const uint32*        pData,
    uint32*              pCmdSpace)
{
#if PAL_ENABLE_PRINTS_ASSERTS
    m_dstContainsSrc = false;
#endif

    return OptimizePm4SetReg(setData, pData, pCmdSpace, m_shRegs);
}

// =====================================================================================================================
// Writes an optimized version of the given SET_DATA packet into pCmdSpace (along with the appropriate register data).
// Returns a pointer to the next unused DWORD in pCmdSpace.
uint32* Pm4Optimizer::WriteOptimizedSetSeqContextRegs(
    const PM4CMDSETDATA& setData,
    const uint32*        pData,
    uint32*              pCmdSpace)
{
#if PAL_ENABLE_PRINTS_ASSERTS
    m_dstContainsSrc = false;
#endif

    return OptimizePm4SetReg(setData, pData, pCmdSpace, m_cntxRegs);
}

// =====================================================================================================================
// Optimize the specified PM4 SET packet. May remove the SET packet completely, reduce the range of registers it sets,
// break it into multiple smaller SET commands, or leave it unmodified. Returns a pointer to the next free location in
// the optimized command stream.
uint32* Pm4Optimizer::OptimizePm4SetReg(
    const PM4CMDSETDATA& setData,
    const uint32*        pRegData,
    uint32*              pDstCmd,
    RegState*            pRegStateBase)
{
    const uint32 numRegs = setData.header.count;

    // Determine which of the registers written by this set command can't be skipped because they must always be set or
    // are taking on a new value.
    //
    // We assume that no more than 32 registers are being set. Currently the driver only sets more than 32 registers in
    // the viewport state object. Luckily, those registers are vector registers so we can't optimize them anyway.

    // Previous code used memmove for all copies to allow for overlapped copies, but that doesn't seem necessary.
    // Copying starting at the beginning should work if the destination is the source.  The case copying starting at
    // the beginning can't handle is if the destination starts part way through the source data.  That doesn't
    // make sense for the pm4 optimizer.  When we know we have at most 32 uint32s to copy (and often just 1) the
    // overhead of memcpy/memmove isn't worth it.  The compiler does inline the memcpys of PM4CMDSETDATA since they are
    // a small fixed size.
    PAL_ASSERT((pDstCmd <= pRegData) || (pDstCmd >= (pRegData + numRegs)));

    // Take care of large register writes.  Not sure if 0 is valid value, but take care of this case here so we don't
    // have to worry about this later.  Handling 0 here avoids checking for 0 at the beginning of 2 loops below.
    // Compiler turns this into a single comparison.
    PAL_ASSERT(numRegs != 0);
    if ((numRegs > 32) || (numRegs == 0))
    {
        // No register writes can be skipped: emit all registers.
        memcpy(pDstCmd, &setData, sizeof(setData));
        pDstCmd += PM4_CMD_SET_DATA_DWORDS;

        memmove(pDstCmd, pRegData, numRegs * sizeof(uint32));
        pDstCmd += numRegs;
    }
    else
    {
        const uint32 regOffset = setData.regOffset;
        RegState*    pRegState = pRegStateBase + regOffset;

        uint32 keepRegCount = 0;
        uint32 keepRegMask = 0;
        uint32 i = 0;
        do
        {
            if (UpdateRegState(pRegData[i], pRegState + i))
            {
                keepRegCount++;
                keepRegMask |= 1 << i;
            }

            i++;
        } while (i < numRegs);

        if (keepRegCount == numRegs)
        {
            // No register writes can be skipped: emit all registers.
            memcpy(pDstCmd, &setData, sizeof(setData));
            pDstCmd += PM4_CMD_SET_DATA_DWORDS;

            do
            {
                *pDstCmd = *pRegData;
                pDstCmd++;
                pRegData++;
                keepRegCount--;
            } while (keepRegCount > 0);
        }
        else if (keepRegCount > 0)
        {
            // A clause of optimized registers starts with a non-skipped register and continues until either 1) there is a
            // big enough gap in the non-skipped registers that we can start a new clause, or 2) the source packet ends.
            //
            // The "big enough" gap size is set to the size of a SET_DATA command (two DWORDs). This prevents us from
            // using more command space than an unoptimized command while conceeding that in some cases we may write
            // redundant registers. The difference between clauseEndIdx and curRegIdx will be one greater than the gap size
            // so we need to add one to the constant below.
            constexpr uint32 MinClauseIdxGap = PM4_CMD_SET_DATA_DWORDS + 1;

            // Since the keepRegCount is non-zero we must have at least one bit set, find it and use it to start a clause.
            uint32 curRegIdx = 0;
            bool   foundNewIdx = BitMaskScanForward(&curRegIdx, keepRegMask);
            uint32 clauseStartIdx = curRegIdx;
            uint32 clauseEndIdx = curRegIdx;

            PM4CMDSETDATA newSetData = setData;

            do
            {
                // Find the next non-skipped register index, if any, for us to consider. We must mask off the bit that we
                // queried last time to prevent an infinite loop.
                keepRegMask &= ~(1 << curRegIdx);
                foundNewIdx = BitMaskScanForward(&curRegIdx, keepRegMask);

                // Check our end-of-clause conditions as stated above.
                if ((foundNewIdx == false) || (curRegIdx - clauseEndIdx >= MinClauseIdxGap))
                {
                    const uint32 clauseRegCount = clauseEndIdx - clauseStartIdx + 1;

                    newSetData.header.count = clauseRegCount;
                    newSetData.regOffset    = regOffset + clauseStartIdx;

                    memcpy(pDstCmd, &newSetData, sizeof(newSetData));
                    pDstCmd += PM4_CMD_SET_DATA_DWORDS;

                    uint32 j = 0;
                    do
                    {
                        pDstCmd[j] = pRegData[clauseStartIdx + j];
                        j++;
                    } while (j < clauseRegCount);

                    pDstCmd += clauseRegCount;

#if PAL_ENABLE_PRINTS_ASSERTS
                    // If we're reading and writing to the same buffer we can't write past the end of this clause's data.
                    PAL_ASSERT((m_dstContainsSrc == false) || (pDstCmd <= pRegData + clauseEndIdx + 1));
#endif

                    // The next clause begins at the current index.
                    clauseStartIdx = curRegIdx;
                    clauseEndIdx   = curRegIdx;
                }
                else
                {
                    // There isn't enough space to end the clause so we must continue it through the current index.
                    clauseEndIdx = curRegIdx;
                }
            } while (foundNewIdx);
        }
    }

    return pDstCmd;
}

// =====================================================================================================================
// Handle an occurrence of a PM4 LOAD packet: there's no optimization we can do on these, but we need to invalidate the
// state of the affected register(s) because this packet will set them to unknowable values.
void Pm4Optimizer::HandlePm4LoadReg(
    const PM4CMDLOADDATA& loadData,
    RegState*             pRegStateBase)
{
    // NOTE: IT_LOAD_*_REG is a variable-length packet which loads N groups of consecutive register values from GPU
    // memory. The LOAD packet uses 3 DWORD's for the PM4 header and for the GPU virtual address to load from. The
    // last two DWORD's in the packet can be repeated for each group of registers the packet will load from memory.

    const uint32* pRegisterGroup = ((&loadData.ordinal1) + PM4_CMD_LOAD_DATA_DWORDS - 2);
    const uint32* pNextHeader    = ((&loadData.ordinal1) + loadData.header.count + 2);

    while (pRegisterGroup != pNextHeader)
    {
        const uint32& startRegOffset = pRegisterGroup[0];
        const uint32  endRegOffset   = (startRegOffset + pRegisterGroup[1] - 1);
        for (uint32 reg = startRegOffset; reg <= endRegOffset; ++reg)
        {
            pRegStateBase[reg].flags.valid = 0;
        }

        pRegisterGroup += 2;
    }
}

// =====================================================================================================================
// Handle an occurrence of a PM4 LOAD INDEX packet: there's no optimization we can do on these, but we need to
// invalidate the state of the affected register(s) because this packet will set them to unknowable values.
void Pm4Optimizer::HandlePm4LoadRegIndex(
    const PM4CMDLOADDATAINDEX& loadDataIndex,
    RegState*                  pRegStateBase)
{
    // NOTE: IT_LOAD_*_REG_INDEX is nearly identical to IT_LOAD_*_REG except the register offset values in it are only
    //       16 bits wide. This means we need to perform some special logic when traversing the dwords that follow the
    //       packet header to avoid reading the reserved bits by accident.

    const void* pRegisterGroup = ((&loadDataIndex.ordinal1) + PM4_CMD_LOAD_DATA_INDEX_DWORDS - 2);
    const void* pNextHeader    = ((&loadDataIndex.ordinal1) + loadDataIndex.header.count + 2);

    while (pRegisterGroup != pNextHeader)
    {
        const uint32 startRegOffset = *static_cast<const uint16*>(pRegisterGroup);
        const uint32 numRegs        = *static_cast<const uint32*>(VoidPtrInc(pRegisterGroup, sizeof(uint32)));
        const uint32 endRegOffset   = (startRegOffset + numRegs - 1);
        for (uint32 reg = startRegOffset; reg <= endRegOffset; ++reg)
        {
            pRegStateBase[reg].flags.valid = 0;
        }

        pRegisterGroup = VoidPtrInc(pRegisterGroup, sizeof(uint32) * 2);
    }
}

// =====================================================================================================================
// Handle an occurrence of a PM4 SET SH REG OFFSET packet: there's no optimization we can do on these, but we need to
// invalidate the state of the affected register(s) because this packet will set them to unknowable values.
void Pm4Optimizer::HandlePm4SetShRegOffset(const PM4CMDSETSHREGOFFSET& setShRegOffset)
{
    // Invalidate the register the packet is operating on.
    m_shRegs[setShRegOffset.regOffset].flags.valid = 0;

    // If the index value is set to 0, this packet actually operates on two sequential SH registers so we need to
    // invalidate the following register as well.
    if (setShRegOffset.index__VI == 0)
    {
        m_shRegs[setShRegOffset.regOffset + 1].flags.valid = 0;
    }
}

// =====================================================================================================================
// Handle an occurrence of a PM4 SET CONTEXT REG INDIRECT packet: there's no optimization we can do on these, but we
// need to invalidate the state of the affected register(s) because this packet will set them to unknowable values.
void Pm4Optimizer::HandlePm4SetContextRegIndirect(const PM4CMDSETDATA& setData)
{
    const uint32 startRegOffset = static_cast<uint32>(setData.regOffset);
    const uint32  endRegOffset  = (startRegOffset + (setData.header.count - 1));
    for (uint32 reg = startRegOffset; reg <= endRegOffset; ++reg)
    {
        m_cntxRegs[reg].flags.valid = 0;
    }
}

// =====================================================================================================================
// Decode PM4 header to determine the size. Returns the packet size of the specified PM4 header in dwords. Includes the
// header itself.
uint32 Pm4Optimizer::GetPm4PacketSize(
    PM4_TYPE_3_HEADER pm4Header
    ) const
{
    const uint32 pm4Count   = pm4Header.count;
    uint32       packetSize = pm4Count + 2;

    // Gfx8 ASICs have added a one DWORD type-3 NOP packet. If the size field is its maximum value (0x3FFF), then the
    // CP interprets this as having a size of one.
    if ((pm4Count == 0x3FFF) && (pm4Header.opcode == IT_NOP) && (m_chipFamily >= GfxIpLevel::GfxIp8))
    {
        packetSize = 1;
    }

    return packetSize;
}

} // Gfx6
} // Pal
