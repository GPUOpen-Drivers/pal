/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/gfx9/g_gfx9PalSettings.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9Pm4Optimizer.h"
#include "palAutoBuffer.h"

using namespace Util;

namespace Pal
{
namespace Gfx9
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

    // This workaround on gfx9 adds some writes to DB_Z_INFO which are preceded by a COND_EXEC. Make sure we don't
    // optimize away writes to this register, which would cause a hang or incorrect skipping of commands.
    if (m_waTcCompatZRange)
    {
        constexpr uint32 dbZInfoIdx = mmDB_Z_INFO__GFX09 - CONTEXT_SPACE_START;

        m_cntxRegs[dbZInfoIdx].flags.mustWrite = 1;
    }

    // Reset the SH register state.
    memset(m_shRegs, 0, sizeof(m_shRegs));

    // Always start with no context rolls
    m_contextRollDetected = false;
}

// =====================================================================================================================
// This functions should be called by Gfx9 CmdStream's "Write" functions to determine if it can skip writing certain
// packets up-front.
bool Pm4Optimizer::MustKeepSetContextReg(
    uint32 regAddr,
    uint32 regData)
{
    PAL_ASSERT(m_cmdUtil.IsContextReg(regAddr));

    const bool mustKeep = UpdateRegState(regData, m_cntxRegs + (regAddr - CONTEXT_SPACE_START));

    m_contextRollDetected |= mustKeep;

    return mustKeep;
}

// =====================================================================================================================
// This functions should be called by Gfx9 CmdStream's "Write" functions to determine if it can skip writing certain
// packets up-front.
bool Pm4Optimizer::MustKeepSetShReg(
    uint32 regAddr,
    uint32 regData)
{
    PAL_ASSERT(m_cmdUtil.IsShReg(regAddr));

    const bool mustKeep = UpdateRegState(regData, m_shRegs + (regAddr - PERSISTENT_SPACE_START));

    return mustKeep;
}

// =====================================================================================================================
// Evaluates a context reg RMW operation and returns true if it can't be skipped.
bool Pm4Optimizer::MustKeepContextRegRmw(
    uint32 regAddr,
    uint32 regMask,
    uint32 regData)
{
    PAL_ASSERT(m_cmdUtil.IsContextReg(regAddr));

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

    m_contextRollDetected |= mustKeep;

    return mustKeep;
}

// =====================================================================================================================
// Writes an optimized version of the given SET_DATA packet into pCmdSpace (along with the appropriate register data).
// Returns a pointer to the next unused DWORD in pCmdSpace.
uint32* Pm4Optimizer::WriteOptimizedSetSeqShRegs(
    PM4_ME_SET_SH_REG setData,
    const uint32*     pData,
    uint32*           pCmdSpace)
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
    PM4_PFP_SET_CONTEXT_REG setData,
    bool*                   pContextRollDetected,
    const uint32*           pData,
    uint32*                 pCmdSpace)
{
#if PAL_ENABLE_PRINTS_ASSERTS
    m_dstContainsSrc = false;
#endif

    PAL_ASSERT(pContextRollDetected != nullptr);

    uint32* pNewCmdSpace = OptimizePm4SetReg(setData, pData, pCmdSpace, m_cntxRegs);
    (*pContextRollDetected) |= ((pNewCmdSpace > pCmdSpace) != 0);
    return pNewCmdSpace;
}

// =====================================================================================================================
// Writes an optimized version of the given SET_SH_REG_OFFSET packet into pCmdSpace
// Returns a pointer to the next unused DWORD in pCmdSpace.
uint32* Pm4Optimizer::WriteOptimizedSetShShRegOffset(
    const PM4PFP_SET_SH_REG_OFFSET& setShRegOffset,
    size_t                          packetSize,
    uint32*                         pCmdSpace)
{
    // Since this is an indirect write, we do not know the exact SH register data. Invalidate SH register so that
    // the next SH register write will not be skipped inadvertently
    m_shRegs[setShRegOffset.bitfields2.reg_offset].flags.valid = 0;

    // If the index value is set to 0, this packet actually operates on two sequential SH registers so we need to
    // invalidate the following register as well.
    if (setShRegOffset.bitfields2.index == 0)
    {
        m_shRegs[setShRegOffset.bitfields2.reg_offset + 1].flags.valid = 0;
    }

    // memcpy packet into command space
    memcpy(pCmdSpace, &setShRegOffset, packetSize << 2);

    return (pCmdSpace + packetSize);
}

// =====================================================================================================================
// Analyze the specified PM4 commands and optimize them. pCmdSize must contain the size of pSrcCmds in DWORDs and will
// return the size of pDstCmds in DWORDs. It's legal for pSrcCmds and pDstCmds to point to the same commands.
// Returns true if a context roll was detected.
bool Pm4Optimizer::OptimizePm4Commands(
    const uint32* pSrcCmds,
    uint32*       pDstCmds,
    uint32*       pCmdSize)
{
    const uint32* pOrigCmdCur = pSrcCmds;             // Current unoptimized command.
    const uint32* pOrigCmdEnd = pSrcCmds + *pCmdSize; // End of the unoptimized commands.

    uint32* pOptCmdCur = pDstCmds; // Location for the next optimized command.

#if PAL_ENABLE_PRINTS_ASSERTS
    m_dstContainsSrc = (pDstCmds == pSrcCmds);
#endif

    while (pOrigCmdCur < pOrigCmdEnd)
    {
        bool optimized = false;

        const PM4_PFP_TYPE_3_HEADER origPm4Hdr = reinterpret_cast<const PM4_PFP_TYPE_3_HEADER&>(*pOrigCmdCur);

        // We only support TYPE 3 packets.
        PAL_ASSERT(origPm4Hdr.type == 3);

        const IT_OpCodeType opcode      = static_cast<IT_OpCodeType>(origPm4Hdr.opcode);
        const uint32        origPktSize = GetPm4PacketSize(origPm4Hdr);

        if (opcode == IT_SET_CONTEXT_REG)
        {
            optimized  = true;
            const uint32* pPreOptCmdCur = pOptCmdCur;
            pOptCmdCur = OptimizePm4SetReg(reinterpret_cast<const PM4_PFP_SET_CONTEXT_REG&>(*pOrigCmdCur),
                                           pOrigCmdCur + CmdUtil::ContextRegSizeDwords,
                                           pOptCmdCur,
                                           &m_cntxRegs[0]);
            m_contextRollDetected |= ((pOptCmdCur > pPreOptCmdCur) != 0);
        }
        else if ((opcode == IT_SET_SH_REG) || (opcode == IT_SET_SH_REG_INDEX))
        {
            optimized  = true;
            pOptCmdCur = OptimizePm4SetReg(reinterpret_cast<const PM4_ME_SET_SH_REG&>(*pOrigCmdCur),
                                           pOrigCmdCur + CmdUtil::ShRegSizeDwords,
                                           pOptCmdCur,
                                           &m_shRegs[0]);
        }
        else if (opcode == IT_SET_SH_REG_OFFSET)
        {
            HandlePm4SetShRegOffset(reinterpret_cast<const PM4PFP_SET_SH_REG_OFFSET&>(*pOrigCmdCur));
        }
        else if (opcode == IT_LOAD_CONTEXT_REG)
        {
            HandlePm4LoadReg(reinterpret_cast<const PM4_PFP_LOAD_CONTEXT_REG&>(*pOrigCmdCur), &m_cntxRegs[0]);
            m_contextRollDetected = true;
        }
        else if (opcode == IT_LOAD_CONTEXT_REG_INDEX)
        {
            HandlePm4LoadRegIndex(reinterpret_cast<const PM4_PFP_LOAD_CONTEXT_REG_INDEX&>(*pOrigCmdCur),
                                  &m_cntxRegs[0]);
            m_contextRollDetected = true;
        }
        else if (opcode == IT_LOAD_SH_REG)
        {
            HandlePm4LoadReg(reinterpret_cast<const PM4_ME_LOAD_SH_REG&>(*pOrigCmdCur), &m_shRegs[0]);
        }
        else if (opcode == IT_LOAD_SH_REG_INDEX)
        {
            HandlePm4LoadRegIndex(reinterpret_cast<const PM4_ME_LOAD_SH_REG_INDEX&>(*pOrigCmdCur), &m_shRegs[0]);
        }
        else if (opcode == IT_CONTEXT_REG_RMW)
        {
            const auto& packet = reinterpret_cast<const PM4_PFP_CONTEXT_REG_RMW&>(*pOrigCmdCur);

            // This packet modifies a single register so if we can skip it we just have to set the optimized flag and
            // the logic below will automatically omit the packet. Note that we have to add CONTEXT_SPACE_START to the
            // register offset because we're reusing one of the public "MustKeep" methods which use register addresses.
            optimized = !MustKeepContextRegRmw(packet.bitfields2.reg_offset + CONTEXT_SPACE_START,
                                               packet.reg_mask,
                                               packet.reg_data);
            m_contextRollDetected |= (optimized == false);
        }
        // The CP will write the base vertex location and start instance location SH registers directly on an indirect
        // draw. We don't know what the new values will be so clear their valid bits.
        else if (opcode == IT_DRAW_INDIRECT)
        {
            const auto& packet = reinterpret_cast<const PM4_PFP_DRAW_INDIRECT&>(*pOrigCmdCur);
            m_shRegs[packet.bitfields3.base_vtx_loc].flags.valid   = 0;
            m_shRegs[packet.bitfields4.start_inst_loc].flags.valid = 0;
        }
        else if (opcode == IT_DRAW_INDIRECT_MULTI)
        {
            const auto& packet = reinterpret_cast<const PM4_PFP_DRAW_INDIRECT_MULTI&>(*pOrigCmdCur);
            m_shRegs[packet.bitfields3.base_vtx_loc].flags.valid   = 0;
            m_shRegs[packet.bitfields4.start_inst_loc].flags.valid = 0;
            if (packet.bitfields5.draw_index_enable != 0)
            {
                m_shRegs[packet.bitfields5.draw_index_loc].flags.valid = 0;
            }
        }
        else if (opcode == IT_DRAW_INDEX_INDIRECT)
        {
            const auto& packet = reinterpret_cast<const PM4_PFP_DRAW_INDEX_INDIRECT&>(*pOrigCmdCur);
            m_shRegs[packet.bitfields3.base_vtx_loc].flags.valid   = 0;
            m_shRegs[packet.bitfields4.start_inst_loc].flags.valid = 0;
        }
        else if (opcode == IT_DRAW_INDEX_INDIRECT_MULTI)
        {
            const auto& packet = reinterpret_cast<const PM4_PFP_DRAW_INDEX_INDIRECT_MULTI&>(*pOrigCmdCur);
            m_shRegs[packet.bitfields3.base_vtx_loc].flags.valid   = 0;
            m_shRegs[packet.bitfields4.start_inst_loc].flags.valid = 0;
            if (packet.bitfields5.draw_index_enable != 0)
            {
                m_shRegs[packet.bitfields5.draw_index_loc].flags.valid = 0;
            }
        }
        else if (opcode == IT_INDIRECT_BUFFER)
        {
            // Nested command buffer register state is not visible to the command buffer it gets executed on.
            // This causes the current PM4 optimizer state to be out of sync after a nested command buffer
            // execute and can incorrectly optimize commands from the executing command buffer. We need to
            // invalidate the PM4 optimizer state if we detect a IT_INDIRECT_BUFFER packet in the stream.
            Reset();
        }

        if (optimized == false)
        {
            // No optimization for this packet. Just copy it.
            if (pOptCmdCur != pOrigCmdCur)
            {
                memmove(pOptCmdCur, pOrigCmdCur, origPktSize * sizeof(uint32));
            }

            pOptCmdCur += origPktSize;
        }

        pOrigCmdCur += origPktSize;

#if PAL_ENABLE_PRINTS_ASSERTS
        // If this fails we're clobbering commands before we can optimize them.
        PAL_ASSERT((m_dstContainsSrc == false) || (pOptCmdCur <= pOrigCmdCur));
#endif
    }

    *pCmdSize = static_cast<uint32>(pOptCmdCur - pDstCmds);

    return m_contextRollDetected;
}

// =====================================================================================================================
// Optimize the specified PM4 SET packet. May remove the SET packet completely, reduce the range of registers it sets,
// break it into multiple smaller SET commands, or leave it unmodified. Returns a pointer to the next free location in
// the optimized command stream.
template <typename SetDataPacket>
uint32* Pm4Optimizer::OptimizePm4SetReg(
    SetDataPacket setData,
    const uint32* pRegData,
    uint32*       pDstCmd,
    RegState*     pRegStateBase)
{
    const uint32 numRegs   = setData.header.count;
    const uint32 regOffset = setData.bitfields2.reg_offset;
    RegState*    pRegState = pRegStateBase + regOffset;

    // Determine which of the registers written by this set command can't be skipped because they must always be set or
    // are taking on a new value.
    //
    // We assume that no more than 32 registers are being set. Currently the driver only sets more than 32 registers in
    // the viewport state object. Luckily, those registers are vector regisers so we can't optimize them anyway. If we
    // ever encounter a set command with more than 32 registers that has redundant values the assert below will trigger.
    uint32 keepRegCount = 0;
    uint32 keepRegMask  = 0;
    for (uint32 i = 0; i < numRegs; i++)
    {
        if (UpdateRegState(pRegData[i], pRegState + i))
        {
            keepRegCount++;
            keepRegMask |= 1 << i;
        }
    }

    PAL_ASSERT((keepRegCount == numRegs) || (numRegs <= 32));

    if ((keepRegCount == numRegs) || (numRegs > 32))
    {
        // No register writes can be skipped: emit all registers.
        memcpy(pDstCmd, &setData, sizeof(setData));
        pDstCmd += Util::NumBytesToNumDwords(sizeof(SetDataPacket));

        memmove(pDstCmd, pRegData, numRegs * sizeof(uint32));
        pDstCmd += numRegs;
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
        const uint32 MinClauseIdxGap = Util::NumBytesToNumDwords(sizeof(SetDataPacket)) + 1;

        // Since the keepRegCount is non-zero we must have at least one bit set, find it and use it to start a clause.
        uint32 curRegIdx      = 0;
        bool   foundNewIdx    = BitMaskScanForward(&curRegIdx, keepRegMask);
        uint32 clauseStartIdx = curRegIdx;
        uint32 clauseEndIdx   = curRegIdx;

        do
        {
            // Find the next non-skipped register index, if any, for us to consider. We must mask off the bit that we
            // queried last time to prevent an infinite loop.
            keepRegMask &= ~(1 << curRegIdx);
            foundNewIdx  = BitMaskScanForward(&curRegIdx, keepRegMask);

            // Check our end-of-clause conditions as stated above.
            if ((foundNewIdx == false) || (curRegIdx - clauseEndIdx >= MinClauseIdxGap))
            {
                const uint32 clauseRegCount = clauseEndIdx - clauseStartIdx + 1;

                setData.header.count          = clauseRegCount;
                setData.bitfields2.reg_offset = regOffset + clauseStartIdx;

                memcpy(pDstCmd, &setData, sizeof(setData));
                pDstCmd += Util::NumBytesToNumDwords(sizeof(SetDataPacket));

                memmove(pDstCmd, pRegData + clauseStartIdx, clauseRegCount * sizeof(uint32));
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
        }
        while (foundNewIdx);
    }

    return pDstCmd;
}

// =====================================================================================================================
// Handle an occurrence of a PM4 LOAD packet: there's no optimization we can do on these, but we need to invalidate the
// state of the affected register(s) because this packet will set them to unknowable values.
template <typename LoadDataPacket>
void Pm4Optimizer::HandlePm4LoadReg(
    const LoadDataPacket& loadData,
    RegState*             pRegStateBase)
{
    // NOTE: IT_LOAD_*_REG is a variable-length packet which loads N groups of consecutive register values from GPU
    // memory. The LOAD packet uses 3 DWORD's for the PM4 header and for the GPU virtual address to load from. The
    // last two DWORD's in the packet can be repeated for each group of registers the packet will load from memory.

    const uint32* pRegisterGroup = ((&loadData.ordinal1) + Util::NumBytesToNumDwords(sizeof(LoadDataPacket)) - 2);
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
template <typename LoadDataIndexPacket>
void Pm4Optimizer::HandlePm4LoadRegIndex(
    const LoadDataIndexPacket& loadDataIndex,
    RegState*                  pRegStateBase)
{
    // NOTE: IT_LOAD_*_REG_INDEX is nearly identical to IT_LOAD_*_REG except the register offset values in it are only
    //       16 bits wide. This means we need to perform some special logic when traversing the dwords that follow the
    //       packet header to avoid reading the reserved bits by accident.

    const void* pRegisterGroup = ((&loadDataIndex.ordinal1) + NumBytesToNumDwords(sizeof(LoadDataIndexPacket)) - 2);
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
void Pm4Optimizer::HandlePm4SetShRegOffset(const PM4PFP_SET_SH_REG_OFFSET& setShRegOffset)
{
    // Invalidate the register the packet is operating on.
    m_shRegs[setShRegOffset.bitfields2.reg_offset].flags.valid = 0;

    // If the index value is set to 0, this packet actually operates on two sequential SH registers so we need to
    // invalidate the following register as well.
    if (setShRegOffset.bitfields2.index == 0)
    {
        m_shRegs[setShRegOffset.bitfields2.reg_offset + 1].flags.valid = 0;
    }
}

// =====================================================================================================================
// Handle an occurrence of a PM4 SET CONTEXT REG INDIRECT packet: there's no optimization we can do on these, but we
// need to invalidate the state of the affected register(s) because this packet will set them to unknowable values.
void Pm4Optimizer::HandlePm4SetContextRegIndirect(const PM4_PFP_SET_CONTEXT_REG& setData)
{
    const uint32 startRegOffset = static_cast<uint32>(setData.bitfields2.reg_offset);
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
    PM4_PFP_TYPE_3_HEADER pm4Header
    ) const
{
    const uint32 pm4Count   = pm4Header.count;
    uint32       packetSize = pm4Count + 2;

    // Gfx9 ASICs have a one DWORD type-3 NOP packet. If the size field is its maximum value (0x3FFF), then the
    // CP interprets this as having a size of one.
    if ((pm4Count == 0x3FFF) && (pm4Header.opcode == IT_NOP))
    {
        packetSize = 1;
    }

    return packetSize;
}

} // Gfx9
} // Pal
