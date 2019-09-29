/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
        uint32 mustWrite :  1;  // All writes to this register must be preserved (can't optimize them out).
        uint32 reserved  : 30;
    } flags;

    uint32 value;
};

// Structure used during PM4 optimization and instrumentation to track the current value of registers as well as the
// number of times the register was written (via a SET packet) or ignored due to optimization.
template <size_t RegisterCount>
struct RegGroupState
{
    RegState  state[RegisterCount];     // State of each register in the group.
#if PAL_BUILD_PM4_INSTRUMENTOR
    uint32    totalSets[RegisterCount]; // Number of writes to each register using SET packets.
    uint32    keptSets[RegisterCount];  // Number of writes to each register using SET packets which were not ignored
                                        // due to PM4 optimization.
#endif
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

    void SetShRegInvalid(uint32 regAddr) { m_shRegs.state[regAddr - PERSISTENT_SPACE_START].flags.valid = 0; }

    bool MustKeepSetContextReg(uint32 regAddr, uint32 regData);
    bool MustKeepSetShReg(uint32 regAddr, uint32 regData);
    bool MustKeepContextRegRmw(uint32 regAddr, uint32 regMask, uint32 regData);

    bool GetContextRollState() const { return m_contextRollDetected; }
    void ResetContextRollState() { m_contextRollDetected = false; }

    // These functions take a fully built packet header and the corresponding register data and will write the
    // optimized version into pCmdSpace.
    uint32* WriteOptimizedSetSeqShRegs(PM4_ME_SET_SH_REG setData, const uint32* pData, uint32* pCmdSpace);
    uint32* WriteOptimizedSetSeqContextRegs(
        PM4_PFP_SET_CONTEXT_REG setData,
        bool*                   pContextRollDetected,
        const uint32*           pData,
        uint32*                 pCmdSpace);

    uint32* WriteOptimizedSetShShRegOffset(
        const PM4PFP_SET_SH_REG_OFFSET& setShRegOffset,
        size_t                          packetSize,
        uint32*                         pCmdSpace);

    // These functions take a fully built LOAD_DATA header(s) and will update the state of the optimizer state
    // based on the packet's contents.
    void HandleLoadShRegs(const PM4_ME_LOAD_SH_REG& loadData)
        { HandlePm4LoadReg(loadData, &m_shRegs); }
    void HandleLoadContextRegs(const PM4_PFP_LOAD_CONTEXT_REG& loadData)
        { HandlePm4LoadReg(loadData, &m_cntxRegs); }
    void HandleLoadContextRegsIndex(const PM4PFP_LOAD_CONTEXT_REG_INDEX& loadData);

    // This generic function can be called by just about any step in the command stream building scheme. It can account
    // for cond exec packets assuming that the cond exec block is contained within pSrcCmds.
    // Returns true if a context roll was detected.
    bool OptimizePm4Commands(const uint32* pSrcCmds, uint32* pDstCmds, uint32* pCmdSize);

#if PAL_BUILD_PM4_INSTRUMENTOR
    void IssueHotRegisterReport(GfxCmdBuffer* pCmdBuf) const;
#endif

private:
    template <typename SetDataPacket, size_t RegisterCount>
    uint32* OptimizePm4SetReg(
        SetDataPacket                 setData,
        const uint32*                 pRegData,
        uint32*                       pDstCmd,
        RegGroupState<RegisterCount>* pRegState);

    template <typename LoadDataPacket, size_t RegisterCount>
    void HandlePm4LoadReg(const LoadDataPacket& loadData, RegGroupState<RegisterCount>* pRegState);

    template <typename LoadDataIndexPacket, size_t RegisterCount>
    void HandlePm4LoadRegIndex(const LoadDataIndexPacket& loadDataIndex, RegGroupState<RegisterCount>* pRegState);

    void HandlePm4SetShRegOffset(const PM4PFP_SET_SH_REG_OFFSET& setShRegOffset);
    void HandlePm4SetContextRegIndirect(const PM4_PFP_SET_CONTEXT_REG& setData);

    uint32 GetPm4PacketSize(PM4_PFP_TYPE_3_HEADER pm4Header) const;

    const Device&   m_device;
    const CmdUtil&  m_cmdUtil;

    const bool m_waTcCompatZRange; // If the waTcCompatZRange workaround is enabled or not

#if PAL_ENABLE_PRINTS_ASSERTS
    bool  m_dstContainsSrc; // Knowing when the dst and src buffers are the same lets us do additional debug checks.
#endif

    // Shadow register state for context and SH registers.
    CntxRegState  m_cntxRegs;
    ShRegState    m_shRegs;

    bool  m_contextRollDetected;
};

} // Gfx9
} // Pal
