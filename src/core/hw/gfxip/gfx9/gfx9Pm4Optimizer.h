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

#pragma once

#include "core/hw/gfxip/gfx9/gfx9CmdUtil.h"

namespace Pal
{

class GfxCmdBuffer;

namespace Gfx9
{

class Device;

// Structure used during PM4 optimization to track the current value of a single register.
struct RegState
{
    struct
    {
        uint32 valid     :  1;  // This register has been set in this stream, value is valid.
        uint32 reserved  : 31;
    } flags;

    uint32 value;
};

// Structure used during PM4 optimization and instrumentation to track the current value of registers as well as the
// number of times the register was written (via a SET packet) or ignored due to optimization.
template <size_t RegisterCount>
struct RegGroupState
{
    RegState  state[RegisterCount];     // State of each register in the group.
#if PAL_DEVELOPER_BUILD
    uint32    totalSets[RegisterCount]; // Number of writes to each register using SET packets.
    uint32    keptSets[RegisterCount];  // Number of writes to each register using SET packets which were not ignored
                                        // due to PM4 optimization.
#endif
};

// Structure used duing PM4 optimization and instrumentation to track the current value of SET_BASE addresses
// as well as the number of times the address was set via the SET_BASE packet or ignored due to optimization.
struct SetBaseState
{
    gpusize address;
#if PAL_DEVELOPER_BUILD
    uint32  totalSets;
    uint32  keptSets;
#endif
};

enum StateType
{
    Sh,
    Context
};

using ShRegState   = RegGroupState<ShRegUsedRangeSize>;
using CntxRegState = RegGroupState<CntxRegUsedRangeSize>;

// =====================================================================================================================
// Utility class which provides routines to optimize PM4 command streams. Currently it only optimizes SH register writes
// and context register writes.
class Pm4Optimizer
{
public:
    Pm4Optimizer(const Device& device);

    void Reset();

    void SetShRegInvalid(uint32 regAddr)
    {
         PAL_ASSERT((regAddr >= PERSISTENT_SPACE_START) && (regAddr < (PERSISTENT_SPACE_START + ShRegUsedRangeSize)));
         m_shRegs.state[regAddr - PERSISTENT_SPACE_START].flags.valid = 0;
    }

    void SetCtxRegInvalid(uint32 regAddr)
    {
        PAL_ASSERT((regAddr >= CONTEXT_SPACE_START) && (regAddr < (CONTEXT_SPACE_START + CntxRegUsedRangeSize)));
        m_cntxRegs.state[regAddr - CONTEXT_SPACE_START].flags.valid = 0;
    }

    bool MustKeepSetContextReg(uint32 regAddr, uint32 regData);
    bool MustKeepSetShReg(uint32 regAddr, uint32 regData);
    bool MustKeepContextRegRmw(uint32 regAddr, uint32 regMask, uint32 regData);
    bool MustKeepSetBase(gpusize address, uint32 index, Pm4ShaderType shaderType);

    template <Pm4ShaderType ShaderType>
    uint32* WriteOptimizedSetShRegPairs(
        const PackedRegisterPair* pRegPairs,
        uint32                    numRegs,
        uint32*                   pCmdSpace);
    uint32* WriteOptimizedSetContextRegPairs(
        const PackedRegisterPair* pRegPairs,
        uint32                    numRegs,
        uint32*                   pCmdSpace);

    template <Pm4ShaderType ShaderType>
    uint32* WriteOptimizedSetShRegPairs(
        const RegisterValuePair* pRegPairs,
        uint32                   numRegs,
        uint32*                  pCmdSpace);
    uint32* WriteOptimizedSetContextRegPairs(
        const RegisterValuePair* pRegPairs,
        uint32                   numRegs,
        uint32*                  pCmdSpace);

#if PAL_DEVELOPER_BUILD
    void IssueHotRegisterReport(GfxCmdBuffer* pCmdBuf) const;
#endif

    static bool IsRegisterMustWrite(uint32 regOffset);

    template <StateType RegType>
    bool OptimizePm4SetRegSeq(
        uint32*        pStartRegAddr,
        uint32*        pEndRegAddr,
        const uint32** ppData);

private:
    template <RegisterRangeType RegType>
    uint32* OptimizePm4SetRegPairsPacked(
        const PackedRegisterPair* pRegPairs,
        uint32                    numRegs,
        uint32*                   pCmdSpace);

    template <RegisterRangeType RegType>
    uint32* OptimizePm4SetRegPairs(
        const RegisterValuePair* pRegPairs,
        uint32                   numRegs,
        uint32*                  pCmdSpace);

#if PAL_DEVELOPER_BUILD
    const Device&   m_device;
#endif
    const CmdUtil&  m_cmdUtil;

    // Shadow register state for context and SH registers.
    CntxRegState  m_cntxRegs;
    ShRegState    m_shRegs;

    // Base addresses set for SET_BASE
    SetBaseState  m_setBaseStateGfx[MaxSetBaseIndex + 1];
    SetBaseState  m_setBaseStateCompute;
};

} // Gfx9
} // Pal
