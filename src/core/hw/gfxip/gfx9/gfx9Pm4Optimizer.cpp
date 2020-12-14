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
template <size_t RegisterCount>
static bool UpdateRegState(
    uint32                        newRegVal,
    uint32                        regOffset,
    bool                          tempDisableOptimizer,
    RegGroupState<RegisterCount>* pCurRegState) // [in,out] Current state of register being set, will be updated.
{
    bool mustKeep = false;

    // We must issue the write if:
    // - The new value is different than the old value.
    // - The previous state is invalid.
    // - We must always write this register.
    // - Optimizer is temporarily disabled.
    if ((pCurRegState->state[regOffset].value           != newRegVal) ||
        (pCurRegState->state[regOffset].flags.valid     == 0)         ||
        (pCurRegState->state[regOffset].flags.mustWrite == 1)         ||
        tempDisableOptimizer)
    {
#if PAL_BUILD_PM4_INSTRUMENTOR
        pCurRegState->keptSets[regOffset]++;
#endif

        pCurRegState->state[regOffset].flags.valid = 1;
        pCurRegState->state[regOffset].value       = newRegVal;

        mustKeep = true;
    }

#if PAL_BUILD_PM4_INSTRUMENTOR
    pCurRegState->totalSets[regOffset]++;
#endif

    return mustKeep;
}

// =====================================================================================================================
Pm4Optimizer::Pm4Optimizer(
    const Device& device)
    :
    m_device(device),
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
    memset(&m_cntxRegs, 0, sizeof(m_cntxRegs));

    // Mark the "vector" context registers as mustWrite. There are some PA registers that require setting the entire
    // vector if any register in the vector needs to change. According to the PA and SC hardware team, these registers
    // consist of the viewport scale/offset regs, viewport scissor regs, and guardband regs.
    constexpr uint32 VportStart = mmPA_CL_VPORT_XSCALE     - CONTEXT_SPACE_START;
    constexpr uint32 VportEnd   = mmPA_CL_VPORT_ZOFFSET_15 - CONTEXT_SPACE_START;
    for (uint32 regOffset = VportStart; regOffset <= VportEnd; ++regOffset)
    {
        m_cntxRegs.state[regOffset].flags.mustWrite = 1;
    }

    constexpr uint32 VportScissorStart = mmPA_SC_VPORT_SCISSOR_0_TL - CONTEXT_SPACE_START;
    constexpr uint32 VportScissorEnd   = mmPA_SC_VPORT_ZMAX_15      - CONTEXT_SPACE_START;
    for (uint32 regOffset = VportScissorStart; regOffset <= VportScissorEnd; ++regOffset)
    {
        m_cntxRegs.state[regOffset].flags.mustWrite = 1;
    }

    constexpr uint32 GuardbandStart = mmPA_CL_GB_VERT_CLIP_ADJ - CONTEXT_SPACE_START;
    constexpr uint32 GuardbandEnd   = mmPA_CL_GB_HORZ_DISC_ADJ - CONTEXT_SPACE_START;
    for (uint32 regOffset = GuardbandStart; regOffset <= GuardbandEnd; ++regOffset)
    {
        m_cntxRegs.state[regOffset].flags.mustWrite = 1;
    }

    // This workaround on gfx9 adds some writes to DB_Z_INFO which are preceded by a COND_EXEC. Make sure we don't
    // optimize away writes to this register, which would cause a hang or incorrect skipping of commands.
    if (m_waTcCompatZRange)
    {
        constexpr uint32 dbZInfoIdx = Gfx09::mmDB_Z_INFO - CONTEXT_SPACE_START;

        m_cntxRegs.state[dbZInfoIdx].flags.mustWrite = 1;
    }

    // Reset the SH register state.
    memset(&m_shRegs, 0, sizeof(m_shRegs));

    // Reset the SET_BASE address state
    memset(&m_setBaseStateGfx, 0, sizeof(m_setBaseStateGfx));
    memset(&m_setBaseStateCompute, 0, sizeof(m_setBaseStateCompute));

    // Always start with no context rolls
    m_contextRollDetected = false;

    // Always start enabled
    m_isTempDisabled      = false;
}

// =====================================================================================================================
// This functions should be called by Gfx9 CmdStream's "Write" functions to determine if it can skip writing certain
// packets up-front.
bool Pm4Optimizer::MustKeepSetContextReg(
    uint32 regAddr,
    uint32 regData)
{
    PAL_ASSERT(m_cmdUtil.IsContextReg(regAddr));

    const bool mustKeep = UpdateRegState(regData, (regAddr - CONTEXT_SPACE_START), m_isTempDisabled, &m_cntxRegs);

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
    return UpdateRegState(regData, (regAddr - PERSISTENT_SPACE_START), m_isTempDisabled, &m_shRegs);
}

// =====================================================================================================================
// Evaluates a context reg RMW operation and returns true if it can't be skipped.
bool Pm4Optimizer::MustKeepContextRegRmw(
    uint32 regAddr,
    uint32 regMask,
    uint32 regData)
{
    PAL_ASSERT(m_cmdUtil.IsContextReg(regAddr));

    const uint32 regOffset = (regAddr - CONTEXT_SPACE_START);

    bool mustKeep = true;

    // We must keep this RMW if we haven't done a SET on this register at least once because we need a fully defined
    // regState value to compute newRegVal. If we tried to do it anyway, the fact that our regMask will have some bits
    // disabled means that we would be setting regState's value to something partially invalid which may cause us to
    // skip needed packets in the future.
    if (m_cntxRegs.state[regOffset].flags.valid != 0)
    {
        // Computed according to the formula stated in the definition of CmdUtil::BuildContextRegRmw.
        const uint32 newRegVal = (m_cntxRegs.state[regOffset].value & ~regMask) | (regData & regMask);

        mustKeep = UpdateRegState(newRegVal, regOffset, m_isTempDisabled, &m_cntxRegs);
    }

    m_contextRollDetected |= mustKeep;

    return mustKeep;
}

// =====================================================================================================================
// Thi functions should be called by Gfx9 CmdStream's "Write" functions to determine if it can skip writing certain
// a SET_BASE packet up-front.
bool Pm4Optimizer::MustKeepSetBase(
    gpusize         address,
    uint32          index,
    Pm4ShaderType   shaderType)
{
    PAL_ASSERT(address != 0);
    PAL_ASSERT(index <= MaxSetBaseIndex);
    PAL_ASSERT((shaderType == ShaderCompute) || (shaderType == ShaderGraphics));

    SetBaseState* pBaseState = nullptr;

    if ((index == base_index__pfp_set_base__patch_table_base) &&
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

#if PAL_BUILD_PM4_INSTRUMENTOR
    pBaseState->totalSets++;
    if (mustKeep)
    {
        pBaseState->keptSets++;
    }
#endif

    pBaseState->address = address;

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

    return OptimizePm4SetReg(setData, pData, pCmdSpace, &m_shRegs);
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

    uint32* pNewCmdSpace = OptimizePm4SetReg(setData,
                                             pData,
                                             pCmdSpace,
                                             &m_cntxRegs);
    (*pContextRollDetected) |= ((pNewCmdSpace > pCmdSpace) != 0);
    return pNewCmdSpace;
}

// =====================================================================================================================
// Optimize the specified PM4 SET packet. May remove the SET packet completely, reduce the range of registers it sets,
// break it into multiple smaller SET commands, or leave it unmodified. Returns a pointer to the next free location in
// the optimized command stream.
template <typename SetDataPacket, size_t RegisterCount>
uint32* Pm4Optimizer::OptimizePm4SetReg(
    SetDataPacket                 setData,
    const uint32*                 pRegData,
    uint32*                       pDstCmd,
    RegGroupState<RegisterCount>* pRegState)
{
    constexpr uint32 SetDataSize = sizeof(SetDataPacket) / sizeof(uint32);

    const uint32 numRegs   = setData.ordinal1.header.count;
    const uint32 regOffset = setData.ordinal2.bitfields.reg_offset;

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
        if (UpdateRegState(pRegData[i], (regOffset + i), m_isTempDisabled, pRegState))
        {
            keepRegCount++;
            keepRegMask |= 1 << i;
        }
    }

    PAL_ASSERT((keepRegCount == numRegs) || (numRegs <= 32));

    if ((keepRegCount == numRegs) || (numRegs > 32))
    {
        // No register writes can be skipped: emit all registers.
        memcpy(pDstCmd, &setData, SetDataSize * sizeof(uint32));
        pDstCmd += SetDataSize;

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
        constexpr uint32 MinClauseIdxGap = SetDataSize + 1;

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

                setData.ordinal1.header.count         = clauseRegCount;
                setData.ordinal2.bitfields.reg_offset = regOffset + clauseStartIdx;

                memcpy(pDstCmd, &setData, SetDataSize * sizeof(uint32));
                pDstCmd += SetDataSize;

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
template <typename LoadDataPacket, size_t RegisterCount>
void Pm4Optimizer::HandlePm4LoadReg(
    const LoadDataPacket&         loadData,
    uint32                        loadDataSize,
    RegGroupState<RegisterCount>* pRegState)
{
    // NOTE: IT_LOAD_*_REG is a variable-length packet which loads N groups of consecutive register values from GPU
    // memory. The LOAD packet uses 3 DWORD's for the PM4 header and for the GPU virtual address to load from. The
    // last two DWORD's in the packet can be repeated for each group of registers the packet will load from memory.

    const uint32* pRegisterGroup = ((&loadData.ordinal1.u32All) + loadDataSize                   - 2);
    const uint32* pNextHeader    = ((&loadData.ordinal1.u32All) + loadData.ordinal1.header.count + 2);

    while (pRegisterGroup != pNextHeader)
    {
        const uint32& startRegOffset = pRegisterGroup[0];
        const uint32  endRegOffset   = (startRegOffset + pRegisterGroup[1] - 1);
        for (uint32 reg = startRegOffset; reg <= endRegOffset; ++reg)
        {
            pRegState->state[reg].flags.valid = 0;
        }

        pRegisterGroup += 2;
    }
}

// =====================================================================================================================
// Handle an occurrence of a PM4 LOAD INDEX packet: there's no optimization we can do on these, but we need to
// invalidate the state of the affected register(s) because this packet will set them to unknowable values.
template <typename LoadDataIndexPacket, size_t RegisterCount>
void Pm4Optimizer::HandlePm4LoadRegIndex(
    const LoadDataIndexPacket&    loadDataIndex,
    uint32                        loadDataIndexSize,
    RegGroupState<RegisterCount>* pRegState)
{
    // NOTE: IT_LOAD_*_REG_INDEX is nearly identical to IT_LOAD_*_REG except the register offset values in it are only
    //       16 bits wide. This means we need to perform some special logic when traversing the dwords that follow the
    //       packet header to avoid reading the reserved bits by accident.

    const void* pRegisterGroup = ((&loadDataIndex.ordinal1.u32All) + loadDataIndexSize                   - 2);
    const void* pNextHeader    = ((&loadDataIndex.ordinal1.u32All) + loadDataIndex.ordinal1.header.count + 2);

    while (pRegisterGroup != pNextHeader)
    {
        const uint32 startRegOffset = *static_cast<const uint16*>(pRegisterGroup);
        const uint32 numRegs        = *static_cast<const uint32*>(VoidPtrInc(pRegisterGroup, sizeof(uint32)));
        const uint32 endRegOffset   = (startRegOffset + numRegs - 1);
        for (uint32 reg = startRegOffset; reg <= endRegOffset; ++reg)
        {
            pRegState->state[reg].flags.valid = 0;
        }

        pRegisterGroup = VoidPtrInc(pRegisterGroup, sizeof(uint32) * 2);
    }
}

// =====================================================================================================================
// This function must be defined in the cpp file because it calls a template function that is defined in this file.
void Pm4Optimizer::HandleLoadContextRegsIndex(
     const PM4_PFP_LOAD_CONTEXT_REG_INDEX& loadData)
{
    HandlePm4LoadRegIndex<PM4_PFP_LOAD_CONTEXT_REG_INDEX>(loadData,
                                                          PM4_PFP_LOAD_CONTEXT_REG_INDEX_SIZEDW__CORE,
                                                          &m_cntxRegs);
}

// =====================================================================================================================
// Handle an occurrence of a PM4 SET SH REG OFFSET packet: there's no optimization we can do on these, but we need to
// invalidate the state of the affected register(s) because this packet will set them to unknowable values.
void Pm4Optimizer::HandlePm4SetShRegOffset(
    const PM4_PFP_SET_SH_REG_OFFSET& setShRegOffset)
{
    // Invalidate the register the packet is operating on.
    m_shRegs.state[setShRegOffset.ordinal2.bitfields.reg_offset].flags.valid = 0;

    // If the index value is set to 0, this packet actually operates on two sequential SH registers so we need to
    // invalidate the following register as well.
    if (setShRegOffset.ordinal2.bitfields.index == 0)
    {
        m_shRegs.state[setShRegOffset.ordinal2.bitfields.reg_offset + 1].flags.valid = 0;
    }
}

// =====================================================================================================================
// Handle an occurrence of a PM4 SET CONTEXT REG INDIRECT packet: there's no optimization we can do on these, but we
// need to invalidate the state of the affected register(s) because this packet will set them to unknowable values.
void Pm4Optimizer::HandlePm4SetContextRegIndirect(
    const PM4_PFP_SET_CONTEXT_REG& setData)
{
    const uint32 startRegOffset = static_cast<uint32>(setData.ordinal2.bitfields.reg_offset);
    const uint32  endRegOffset  = (startRegOffset + (setData.ordinal1.header.count - 1));

    for (uint32 reg = startRegOffset; reg <= endRegOffset; ++reg)
    {
        m_cntxRegs.state[reg].flags.valid = 0;
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

#if PAL_BUILD_PM4_INSTRUMENTOR
// =====================================================================================================================
// Calls the PAL developer callback to issue a report on how many times SET packets to each SH and context register were
// seen by the optimizer and kept after redundancy checking.
void Pm4Optimizer::IssueHotRegisterReport(
    GfxCmdBuffer* pCmdBuf
    ) const
{
    m_device.DescribeHotRegisters(pCmdBuf,
                                  &m_shRegs.totalSets[0],
                                  &m_shRegs.keptSets[0],
                                  ShRegUsedRangeSize,
                                  PERSISTENT_SPACE_START,
                                  &m_cntxRegs.totalSets[0],
                                  &m_cntxRegs.keptSets[0],
                                  CntxRegUsedRangeSize,
                                  CONTEXT_SPACE_START);
}
#endif

} // Gfx9
} // Pal
