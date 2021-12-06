/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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
    bool MustKeepSetBase(gpusize address, uint32 index, Pm4ShaderType shaderType);

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

    // These functions take a fully built LOAD_DATA header(s) and will update the state of the optimizer state
    // based on the packet's contents.
    void HandleLoadShRegs(const PM4_ME_LOAD_SH_REG& loadData)
        { HandlePm4LoadReg(loadData, PM4_ME_LOAD_SH_REG_SIZEDW__CORE, &m_shRegs); }
    void HandleLoadContextRegs(const PM4_PFP_LOAD_CONTEXT_REG& loadData)
        { HandlePm4LoadReg(loadData, PM4_ME_LOAD_CONTEXT_REG_SIZEDW__CORE, &m_cntxRegs); }
    void HandleLoadContextRegsIndex(const PM4_PFP_LOAD_CONTEXT_REG_INDEX& loadData);

    void HandleDynamicLaunchDesc();

#if PAL_DEVELOPER_BUILD
    void IssueHotRegisterReport(GfxCmdBuffer* pCmdBuf) const;
#endif

    // Allows caller to disable/re-enable PM4 optimizer dynamically.
    void TempSetPm4OptimizerMode(bool isEnabled)
    {
        PAL_ASSERT(m_isTempDisabled != !isEnabled); // Not an error but unexpected.
        m_isTempDisabled = !isEnabled;
    }

private:
    template <typename SetDataPacket, size_t RegisterCount>
    uint32* OptimizePm4SetReg(
        SetDataPacket                 setData,
        const uint32*                 pRegData,
        uint32*                       pDstCmd,
        RegGroupState<RegisterCount>* pRegState);

    template <typename LoadDataPacket, size_t RegisterCount>
    void HandlePm4LoadReg(
        const LoadDataPacket&         loadData,
        uint32                        loadDataSize,
        RegGroupState<RegisterCount>* pRegState);

    template <typename LoadDataIndexPacket, size_t RegisterCount>
    void HandlePm4LoadRegIndex(
        const LoadDataIndexPacket&    loadDataIndex,
        uint32                        loadDataIndexSize,
        RegGroupState<RegisterCount>* pRegState);

    void HandlePm4SetShRegOffset(const PM4_PFP_SET_SH_REG_OFFSET& setShRegOffset);
    void HandlePm4SetContextRegIndirect(const PM4_PFP_SET_CONTEXT_REG& setData);

    uint32 GetPm4PacketSize(PM4_PFP_TYPE_3_HEADER pm4Header) const;

    const Device&   m_device;
    const CmdUtil&  m_cmdUtil;

    const bool m_waTcCompatZRange; // If the waTcCompatZRange workaround is enabled or not
    const bool m_splitPackets;

#if PAL_ENABLE_PRINTS_ASSERTS
    bool  m_dstContainsSrc; // Knowing when the dst and src buffers are the same lets us do additional debug checks.
#endif

    // Shadow register state for context and SH registers.
    CntxRegState  m_cntxRegs;
    ShRegState    m_shRegs;

    // Base addresses set for SET_BASE
    SetBaseState  m_setBaseStateGfx[MaxSetBaseIndex + 1];
    SetBaseState  m_setBaseStateCompute;

    bool  m_contextRollDetected;
    bool  m_isTempDisabled;
};

} // Gfx9
} // Pal
