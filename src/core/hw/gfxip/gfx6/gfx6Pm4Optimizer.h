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

#include "core/hw/gfxip/gfx6/gfx6CmdUtil.h"

namespace Pal
{
namespace Gfx6
{

class Device;

// Structure used during PM4 optimization to track the current value of registers.
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

// =====================================================================================================================
// Utility class which provides routines to optimize PM4 command streams. Currently it only optimizes SH register writes
// and context register writes.
class Pm4Optimizer
{
public:
    Pm4Optimizer(const Device& device);

    void Reset();

    void SetShRegInvalid(uint32 regAddr) { m_shRegs[regAddr - PERSISTENT_SPACE_START].flags.valid = 0; }

    bool MustKeepSetContextReg(uint32 regAddr, uint32 regData);
    bool MustKeepSetShReg(uint32 regAddr, uint32 regData);
    bool MustKeepContextRegRmw(uint32 regAddr, uint32 regMask, uint32 regData);

    // These functions take a fully built SET_DATA header and the corresponding register data and will write the an
    // optimized version into pCmdSpace.
    uint32* WriteOptimizedSetSeqShRegs(const PM4CMDSETDATA& setData, const uint32* pData, uint32* pCmdSpace);
    uint32* WriteOptimizedSetSeqContextRegs(const PM4CMDSETDATA& setData, const uint32* pData, uint32* pCmdSpace);
    uint32* WriteOptimizedSetShShRegOffset(
        const PM4CMDSETSHREGOFFSET& setShRegOffset,
        size_t                      packetSize,
        uint32*                     pCmdSpace);

    // These functions take a fully built LOAD_DATA header(s) and will update the state of the optimizer state
    // based on the packet's contents.
    void HandleLoadShRegs(const PM4CMDLOADDATA& loadData) { HandlePm4LoadReg(loadData, &m_shRegs[0]); }
    void HandleLoadContextRegs(const PM4CMDLOADDATA& loadData) { HandlePm4LoadReg(loadData, &m_cntxRegs[0]); }

    // This generic function can be called by just about any step in the command stream building scheme. It can account
    // for cond exec packets assuming that the cond exec block is contained within pSrcCmds.
    void OptimizePm4Commands(const uint32* pSrcCmds, uint32* pDstCmds, uint32* pCmdSize);

private:
    uint32* OptimizePm4SetReg(
        const PM4CMDSETDATA& setData,
        const uint32* pRegData,
        uint32* pDstCmd,
        RegState* pRegStateBase);

    void HandlePm4LoadReg(const PM4CMDLOADDATA& loadData, RegState* pRegStateBase);
    void HandlePm4LoadRegIndex(const PM4CMDLOADDATAINDEX& loadDataIndex, RegState* pRegStateBase);
    void HandlePm4SetShRegOffset(const PM4CMDSETSHREGOFFSET& setShRegOffset);
    void HandlePm4SetContextRegIndirect(const PM4CMDSETDATA& setData);

    uint32 GetPm4PacketSize(PM4_TYPE_3_HEADER pm4Header) const;

    const CmdUtil&   m_cmdUtil;
    const GfxIpLevel m_chipFamily;
    const bool       m_waShaderSpiWriteShaderPgmRsrc2Ls; // Caching this workaround setting is probably a good idea.
    const bool       m_waTcCompatZRange;

#if PAL_ENABLE_PRINTS_ASSERTS
    bool     m_dstContainsSrc; // Knowing when the dst and src buffers are the same lets us do additional debug checks.
#endif

    // Shadow register state for context and SH registers.
    RegState m_cntxRegs[CntxRegUsedRangeSize];
    RegState m_shRegs[ShRegUsedRangeSize];
};

} // Gfx6
} // Pal
