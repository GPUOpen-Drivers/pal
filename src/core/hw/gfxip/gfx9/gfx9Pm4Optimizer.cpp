/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "g_gfx9Settings.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9Pm4Optimizer.h"
#include "core/hw/gfxip/gfxCmdBuffer.h"
#include "palAutoBuffer.h"
#include "palIterator.h"

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
    uint32 newRegVal,
    uint32 regOffset,
    void*  pCurRegState) // [in,out] Current state of register being set, will be updated.
{
#if PAL_DEVELOPER_BUILD
    PAL_ASSERT((RegisterCount == CntxRegUsedRangeSize) ?
               (Pm4Optimizer::IsRegisterMustWrite(regOffset + CONTEXT_SPACE_START)    == false) :
               (Pm4Optimizer::IsRegisterMustWrite(regOffset + PERSISTENT_SPACE_START) == false));
#endif

    bool mustKeep = false;

    RegGroupState<RegisterCount>* pRegState = static_cast<RegGroupState<RegisterCount>*>(pCurRegState);

    // We must issue the write if:
    // - The new value is different than the old value.
    // - The previous state is invalid.
    // - We must always write this register.
    // - Optimizer is temporarily disabled.
    if ((pRegState->state[regOffset].value       != newRegVal) ||
        (pRegState->state[regOffset].flags.valid == 0))
    {
#if PAL_DEVELOPER_BUILD
        pRegState->keptSets[regOffset]++;
#endif

        pRegState->state[regOffset].flags.valid = 1;
        pRegState->state[regOffset].value       = newRegVal;

        mustKeep = true;
    }

#if PAL_DEVELOPER_BUILD
    pRegState->totalSets[regOffset]++;
#endif

    return mustKeep;
}

// =====================================================================================================================
Pm4Optimizer::Pm4Optimizer(
    const Device& device)
    :
#if PAL_DEVELOPER_BUILD
    m_device(device),
#endif
    m_cmdUtil(device.CmdUtil())
{
    Reset();
}

// =====================================================================================================================
// Resets the optimizer so that it's ready to begin optimizing a new command stream.
void Pm4Optimizer::Reset()
{
    // Reset the context register state.
    memset(&m_cntxRegs, 0, sizeof(m_cntxRegs));

    // Reset the SH register state.
    memset(&m_shRegs, 0, sizeof(m_shRegs));

    // Reset the SET_BASE address state
    memset(&m_setBaseStateGfx, 0, sizeof(m_setBaseStateGfx));
    memset(&m_setBaseStateCompute, 0, sizeof(m_setBaseStateCompute));
}

// =====================================================================================================================
// This functions should be called by Gfx9 CmdStream's "Write" functions to determine if it can skip writing certain
// packets up-front.
bool Pm4Optimizer::MustKeepSetContextReg(
    uint32 regAddr,
    uint32 regData)
{
    PAL_ASSERT(m_cmdUtil.IsContextReg(regAddr));

    return UpdateRegState<CntxRegUsedRangeSize>(
                regData, (regAddr - CONTEXT_SPACE_START), &m_cntxRegs);
}

// =====================================================================================================================
// This functions should be called by Gfx9 CmdStream's "Write" functions to determine if it can skip writing certain
// packets up-front.
bool Pm4Optimizer::MustKeepSetShReg(
    uint32 regAddr,
    uint32 regData)
{
    PAL_ASSERT(m_cmdUtil.IsShReg(regAddr));
    return UpdateRegState<ShRegUsedRangeSize>(regData, (regAddr - PERSISTENT_SPACE_START), &m_shRegs);
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

        mustKeep = UpdateRegState<CntxRegUsedRangeSize>(newRegVal, regOffset, &m_cntxRegs);
    }

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

#if PAL_DEVELOPER_BUILD
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
// Returns a pointer to the next unused DWORD in pCmdSpace.
template <Pm4ShaderType ShaderType>
uint32* Pm4Optimizer::WriteOptimizedSetShRegPairs(
    const PackedRegisterPair* pRegPairs,
    uint32                    numRegs,
    uint32*                   pCmdSpace)
{
    constexpr RegisterRangeType RegType = (ShaderType == ShaderGraphics) ? RegRangeGfxSh : RegRangeCsSh;

    return OptimizePm4SetRegPairsPacked<RegType>(pRegPairs, numRegs, pCmdSpace);
}

template
uint32* Pm4Optimizer::WriteOptimizedSetShRegPairs<ShaderGraphics>(
    const PackedRegisterPair* pRegPairs,
    uint32                    numRegs,
    uint32*                   pCmdSpace);
template
uint32* Pm4Optimizer::WriteOptimizedSetShRegPairs<ShaderCompute>(
    const PackedRegisterPair* pRegPairs,
    uint32                    numRegs,
    uint32*                   pCmdSpace);

// =====================================================================================================================
// Returns a pointer to the next unused DWORD in pCmdSpace.
template <Pm4ShaderType ShaderType>
uint32* Pm4Optimizer::WriteOptimizedSetShRegPairs(
    const RegisterValuePair* pRegPairs,
    uint32                   numRegs,
    uint32*                  pCmdSpace)
{
    constexpr RegisterRangeType RegType = (ShaderType == ShaderGraphics) ? RegRangeGfxSh : RegRangeCsSh;

    return OptimizePm4SetRegPairs<RegType>(pRegPairs, numRegs, pCmdSpace);
}

template
uint32* Pm4Optimizer::WriteOptimizedSetShRegPairs<ShaderGraphics>(
    const RegisterValuePair* pRegPairs,
    uint32                   numRegs,
    uint32*                  pCmdSpace);
template
uint32* Pm4Optimizer::WriteOptimizedSetShRegPairs<ShaderCompute>(
    const RegisterValuePair* pRegPairs,
    uint32                   numRegs,
    uint32*                  pCmdSpace);

// =====================================================================================================================
// Returns a pointer to the next unused DWORD in pCmdSpace.
uint32* Pm4Optimizer::WriteOptimizedSetContextRegPairs(
    const RegisterValuePair* pRegPairs,
    uint32                   numRegs,
    uint32*                  pCmdSpace)
{
    return OptimizePm4SetRegPairs<RegRangeContext>(pRegPairs, numRegs, pCmdSpace);
}

// =====================================================================================================================
// Returns a pointer to the next unused DWORD in pCmdSpace.
uint32* Pm4Optimizer::WriteOptimizedSetContextRegPairs(
    const PackedRegisterPair* pRegPairs,
    uint32                    numRegs,
    uint32*                   pCmdSpace)
{
    return OptimizePm4SetRegPairsPacked<RegRangeContext>(pRegPairs, numRegs, pCmdSpace);
}

// =====================================================================================================================
// Returns a pointer to the next free location in the optimized command stream.
template <RegisterRangeType RegType>
uint32* Pm4Optimizer::OptimizePm4SetRegPairs(
    const RegisterValuePair* pRegPairs,
    uint32                   numRegs,
    uint32*                  pCmdSpace)
{
    static_assert((RegType == RegRangeContext) || (RegType == RegRangeGfxSh) || (RegType == RegRangeCsSh));

    constexpr uint32 RegisterCount = (RegType == RegRangeContext) ? CntxRegUsedRangeSize : ShRegUsedRangeSize;

    // We cast to void to avoid the compiler complaining about mismatching RegisterCount despite matching types.
    void* pRegState = (RegType == RegRangeContext) ?  static_cast<void*>(&m_cntxRegs) : static_cast<void*>(&m_shRegs);

    uint32* pStart      = pCmdSpace;
    uint32  numRegsKept = 0;
    size_t  packetSize  = 0;

    // Reserve a spot for the header.
    pCmdSpace++;

    for (uint32 i = 0; i < numRegs; i++)
    {
        // Check if we must keep this register - if so add it to the command space.
        if (UpdateRegState<RegisterCount>(pRegPairs[i].value, pRegPairs[i].offset, pRegState))
        {
            pCmdSpace[0] = pRegPairs[i].offset;
            pCmdSpace[1] = pRegPairs[i].value;
            pCmdSpace += 2;

            numRegsKept++;
        }
    }

    if (numRegsKept > 0)
    {
        packetSize = CmdUtil::BuildSetRegPairsHeader<RegType>(numRegsKept, pStart);
    }
    else
    {
        // Remove the header reservation if we filtered all regs.
        pCmdSpace--;
    }

    PAL_DEBUG_BUILD_ONLY_ASSERT(packetSize == size_t(pCmdSpace - pStart));

    return pCmdSpace;
}

template
uint32* Pm4Optimizer::OptimizePm4SetRegPairs<RegRangeGfxSh>(
    const RegisterValuePair* pRegPairs,
    uint32                   numRegs,
    uint32*                  pCmdSpace);
template
uint32* Pm4Optimizer::OptimizePm4SetRegPairs<RegRangeContext>(
    const RegisterValuePair* pRegPairs,
    uint32                   numRegs,
    uint32*                  pCmdSpace);
template
uint32* Pm4Optimizer::OptimizePm4SetRegPairs<RegRangeCsSh>(
    const RegisterValuePair* pRegPairs,
    uint32                   numRegs,
    uint32*                  pCmdSpace);

static constexpr uint32 PackedRegisterPairSizeInDws = sizeof(PackedRegisterPair) / sizeof(uint32);

// =====================================================================================================================
// Local helper for OptimizePm4SetRegPairsPacked.
// This helper accumulates register state into a local PackedRegisterPair until it is full and them emits it into the
// command stream.
static uint32* OptPm4SetRegPairsPackedAddRegHelper(
    uint32              offsetToAdd,
    uint32              valueToAdd,
    PackedRegisterPair* pTempStorage,
    uint32*             pPendingRegs,
    uint32*             pNumPairsAdded,
    uint32*             pDstCmd)
{
    // If one register is already pending in pTempStorage:
    //   Add the new offset/value, emit PackedRegisterPair into the command stream and update associated metadata.
    if (*pPendingRegs == 1)
    {
        pTempStorage->offset1 = offsetToAdd;
        pTempStorage->value1  = valueToAdd;

        memcpy(pDstCmd, pTempStorage, sizeof(PackedRegisterPair));

        pDstCmd += PackedRegisterPairSizeInDws;
        *pPendingRegs = 0;
        (*pNumPairsAdded)++;
    }
    // If no registers are pending:
    //   Simply add the new offset/value to pTempStorage and update pPendingRegs.
    else
    {
        PAL_DEBUG_BUILD_ONLY_ASSERT(*pPendingRegs == 0);

        pTempStorage->offset0 = offsetToAdd;
        pTempStorage->value0  = valueToAdd;
        *pPendingRegs = 1;
    }

    return pDstCmd;
}

// =====================================================================================================================
// Optimize a sequential SET register packet.
// This method determines if the packet needs to be issued or not (fully redundant or not).
// If it is not fully redundant, modified start/end addresses and pData pointers are returned which strip out any
// leading or trailing redundant register writes.
template <StateType RegType>
bool Pm4Optimizer::OptimizePm4SetRegSeq(
    uint32*        pStartRegAddr,
    uint32*        pEndRegAddr,
    const uint32** ppData)
{
    static_assert((RegType == Context) || (RegType == Sh));

    constexpr uint32 RegisterCount = (RegType == Context) ? CntxRegUsedRangeSize : ShRegUsedRangeSize;
    constexpr uint32 SpaceStart    = (RegType == Context) ? CONTEXT_SPACE_START  : PERSISTENT_SPACE_START;

    const uint32  origStartAddr      = *pStartRegAddr;
    void*         pRegState          = (RegType == Context) ? static_cast<void*>(&m_cntxRegs) :
                                                              static_cast<void*>(&m_shRegs);
    const uint32  numRegs            = (*pEndRegAddr - *pStartRegAddr + 1);
    bool          packetNeeded       = false;
    uint32        firstKeptRegOffset = 0;
    uint32        lastRegAddrNeeded  = 0;
    const uint32* pData              = *ppData;

    // Loop over every reg being updated.
    for (uint32 i = 0; i < numRegs; i++)
    {
        const uint32 regAddr = (origStartAddr + i);
        const uint32 regData = pData[i];

        // Check if we must keep this register.
        if (UpdateRegState<RegisterCount>(regData, regAddr - SpaceStart, pRegState))
        {
            if (packetNeeded == false)
            {
                firstKeptRegOffset = i;
                packetNeeded       = true;
            }

            lastRegAddrNeeded = regAddr;
        }
    }

    if (packetNeeded)
    {
        *pStartRegAddr += firstKeptRegOffset;
        *pEndRegAddr   =  lastRegAddrNeeded;
        *ppData        =  pData + firstKeptRegOffset;
    }

    return packetNeeded;
}

template
bool Pm4Optimizer::OptimizePm4SetRegSeq<StateType::Sh>(
    uint32*        pStartRegAddr,
    uint32*        pEndRegAddr,
    const uint32** ppData);
template
bool Pm4Optimizer::OptimizePm4SetRegSeq<StateType::Context>(
    uint32*        pStartRegAddr,
    uint32*        pEndRegAddr,
    const uint32** ppData);

// =====================================================================================================================
// Returns a pointer to the next free location in the optimized command stream.
template <RegisterRangeType RegType>
uint32* Pm4Optimizer::OptimizePm4SetRegPairsPacked(
    const PackedRegisterPair* pRegPairs,
    uint32                    numRegs,
    uint32*                   pDstCmd)
{
    static_assert((RegType == RegRangeContext) || (RegType == RegRangeGfxSh) || (RegType == RegRangeCsSh));

    constexpr bool          IsCtx         = (RegType == RegRangeContext);
    constexpr Pm4ShaderType ShaderType    = (RegType == RegRangeCsSh) ? ShaderCompute : ShaderGraphics;
    constexpr uint32        RegisterCount = IsCtx ? CntxRegUsedRangeSize : ShRegUsedRangeSize;

    // We cast to void to avoid the compiler complaining about mismatching RegisterCount despite matching types.
    void*        pRegState     = IsCtx ? static_cast<void*>(&m_cntxRegs) : static_cast<void*>(&m_shRegs);
    const bool   numRegsIsEven = ((numRegs % 2) == 0);
    const uint32 loopCount     = Pow2Align(numRegs, 2) / 2;

    uint32*            pStart              = pDstCmd;
    uint32             numPackedPairsAdded = 0;
    uint32             pendingRegs         = 0;
    size_t             packetSize          = 0;
    PackedRegisterPair tempStorage;

    // Reserve SetRegPairsPackedHeaderSizeInDwords DWs (2) for the header/regCount.
    pDstCmd += CmdUtil::SetRegPairsPackedHeaderSizeInDwords;

    // Loop over each pair of regs
    for (uint32 i = 0; i < loopCount; i++)
    {
        const bool isLastLoop = (i == (loopCount - 1));

        // Check if value0 is unique - this is always valid.
        if (UpdateRegState<RegisterCount>(pRegPairs[i].value0, pRegPairs[i].offset0, pRegState))
        {
            pDstCmd = OptPm4SetRegPairsPackedAddRegHelper(pRegPairs[i].offset0, pRegPairs[i].value0, &tempStorage,
                                                          &pendingRegs, &numPackedPairsAdded, pDstCmd);
        }

        // Check if value1 is unique - only valid if an even # of regs is set or we're not on the last loop
        // When setting an off # of regs, the last value1 is not valid.
        if (numRegsIsEven || (isLastLoop == false))
        {
            if (UpdateRegState<RegisterCount>(pRegPairs[i].value1, pRegPairs[i].offset1, pRegState))
            {
                pDstCmd = OptPm4SetRegPairsPackedAddRegHelper(pRegPairs[i].offset1, pRegPairs[i].value1, &tempStorage,
                                                              &pendingRegs, &numPackedPairsAdded, pDstCmd);
            }
        }
    }

    PAL_DEBUG_BUILD_ONLY_ASSERT((pendingRegs == 0) || (pendingRegs == 1));

    // If we already added any packed pairs - we will need to use a REG_PAIRS_PACKED packet.
    if (numPackedPairsAdded > 0)
    {
        // If we have a pending reg, fill in the last slot with the very first reg and add the "pair".
        // It is important that we fill out the extra slot with a register offset/value far away from
        // it as there are specific rules about restrictions with close offset/value pairs.
        // Using the first one should always be safe.
        if (pendingRegs > 0)
        {
            tempStorage.offset1 = pRegPairs[0].offset0;
            tempStorage.value1  = pRegPairs[0].value0;

            memcpy(pDstCmd, &tempStorage, sizeof(PackedRegisterPair));
            pDstCmd += PackedRegisterPairSizeInDws;

            numPackedPairsAdded++;
        }

        // Add the header for the REG_PAIRS_PACKED packet.
        packetSize = m_cmdUtil.BuildSetRegPairsPackedHeader<RegType>(numPackedPairsAdded * 2, pStart);
    }
    // We only have a single register to write, use the normal SET_*_REG packet.
    else if (pendingRegs > 0)
    {
        packetSize = IsCtx ?
                     m_cmdUtil.BuildSetOneContextReg(tempStorage.offset0 + CONTEXT_SPACE_START, pStart) :
                     m_cmdUtil.BuildSetOneShReg(tempStorage.offset0 + PERSISTENT_SPACE_START, ShaderType, pStart);

        // All potential packets have 2 DWs of fixed header size.
        static_assert((CmdUtil::ContextRegSizeDwords == 2) && (CmdUtil::ShRegSizeDwords == 2),
                      "Context and Sh packet sizes do not match REG_PAIRS_PACKED!");

        pDstCmd[0] = tempStorage.value0; //< pDstCmd already incremented to the payload.
        pDstCmd++;
    }
    else
    {
        // No unique regs added, remove the pre-allocated header space.
        pDstCmd -= CmdUtil::SetRegPairsPackedHeaderSizeInDwords;
    }

    PAL_DEBUG_BUILD_ONLY_ASSERT(packetSize == size_t(pDstCmd - pStart));

    return pDstCmd;
}

#if PAL_DEVELOPER_BUILD
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

// =====================================================================================================================
// Check if a given register value is a special "must write" because HW requires it is written in certain granularities.
bool Pm4Optimizer::IsRegisterMustWrite(
    uint32 regOffset)
{
    // There are some PA registers that require setting the entire vector if any register in the vector needs to change.
    // According to the PA and SC hardware team, these registers consist of the viewport scale/offset regs, viewport
    // scissor regs, and guardband regs. We are relying on the the main driver code to never call into the PM4Optimizer
    // with these registers.
    constexpr uint32 VportStart = mmPA_CL_VPORT_XSCALE;
    constexpr uint32 VportEnd   = mmPA_CL_VPORT_ZOFFSET_15;

    constexpr uint32 VportScissorStart = mmPA_SC_VPORT_SCISSOR_0_TL;
    constexpr uint32 VportScissorEnd   = mmPA_SC_VPORT_ZMAX_15;

    constexpr uint32 GuardbandStart = mmPA_CL_GB_VERT_CLIP_ADJ;
    constexpr uint32 GuardbandEnd   = mmPA_CL_GB_HORZ_DISC_ADJ;

    bool isMustWriteReg = false;

    if (((regOffset >= VportStart)        && (regOffset <= VportEnd))        ||
        ((regOffset >= VportScissorStart) && (regOffset <= VportScissorEnd)) ||
        ((regOffset >= GuardbandStart)    && (regOffset <= GuardbandEnd)))
    {
        isMustWriteReg = true;
    }

    return isMustWriteReg;
}

} // Gfx9
} // Pal
