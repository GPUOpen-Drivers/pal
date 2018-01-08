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

#pragma once

#include "core/hw/gfxip/gfx6/gfx6PerfCtrInfo.h"
#include "core/perfCounter.h"

namespace Pal
{
namespace Gfx6
{

class CmdStream;
class Device;

// =====================================================================================================================
// Provides Gfx6-specific functionality for global (i.e., "summary") performance counters.
class PerfCounter : public Pal::PerfCounter
{
public:
    PerfCounter(const Device& device, const PerfCounterInfo& info, uint32 slot);
    virtual ~PerfCounter() {}

    void SetupMcSeqRegisters(PerfCtrInfo::regMC_SEQ_PERF_SEQ_CTL* pMcSeqPerfCtl,
                             PerfCtrInfo::regMC_SEQ_PERF_CNTL_1*  pMcSeqPerfCtl1) const;
    uint32 SetupSdmaSelectReg(regSDMA0_PERFMON_CNTL__CI__VI* pSdma0PerfmonCntl,
                              regSDMA1_PERFMON_CNTL__CI__VI* pSdma1PerfmonCntl) const;

    uint32* WriteSetupCommands(CmdStream* pCmdStream, uint32* pCmdSpace) const;
    uint32* WriteSampleCommands(gpusize baseGpuVirtAddr, CmdStream* pCmdStream, uint32* pCmdSpace) const;

    // Returns true if the GPU block this counter samples from is indexed for reads and writes
    bool IsIndexed() const { return (m_flags.isIndexed != 0); }

    // Helper functions used by both PerfCounter and StreamingPerfCounter.
    static uint32 GetSeIndex(const Gfx6PerfCounterInfo& perfInfo, const GpuBlock& block, uint32 instance)
    {
        return InstanceIdToSe(perfInfo, block, instance);
    }
    static uint32 InstanceIdToSe(const Gfx6PerfCounterInfo& perfInfo, const GpuBlock& block, uint32 instance);
    static uint32 InstanceIdToSh(const Gfx6PerfCounterInfo& perfInfo, const GpuBlock& block, uint32 instance);
    static uint32 InstanceIdToInstance(const Gfx6PerfCounterInfo& perfInfo, const GpuBlock& block, uint32 instance);

private:
    uint32* WriteGrbmGfxIndex(CmdStream* pCmdStream, uint32* pCmdSpace) const;
    uint32* WriteGrbmGfxBroadcastSe(CmdStream* pCmdStream, uint32* pCmdSpace) const;

    union Flags
    {
        struct
        {
            uint32  isIndexed :  1; // Set if the Block is indexed for ctr reads/writes
            uint32  reserved  : 31; // Reserved bits
        };
        uint32 u32All;
    };

    const Device& m_device;
    Flags         m_flags;

    uint32 m_numActiveRegs;                               // Number of active select registers
    uint32 m_selectReg[PerfCtrInfo::MaxPerfCtrSelectReg]; // Value of each performance counter select register.

    uint32 m_perfCountLoAddr; // Register address of the low 32 bits of the perf counter
    uint32 m_perfCountHiAddr; // Register address of the high 32 bits of the perf counter
    uint32 m_perfCountSrcSel; // Source-select value to use for COPY_DATA PM4 commands

    PAL_DISALLOW_DEFAULT_CTOR(PerfCounter);
    PAL_DISALLOW_COPY_AND_ASSIGN(PerfCounter);
};

// =====================================================================================================================
// Provides Gfx6-specific functionality for streaming performance counters.
class StreamingPerfCounter : public Pal::StreamingPerfCounter
{
public:
    StreamingPerfCounter(const Device& device, GpuBlock block, uint32 instance, uint32 slot);

    virtual Result AddEvent(const GpuBlock& block, uint32 event) override;
    uint32* WriteSetupCommands(Pal::CmdStream* pCmdStream, uint32* pCmdSpace) override;

protected:
    bool IsSelect0RegisterValid() const;
    bool IsSelect1RegisterValid() const;
    uint32 GetSelect0RegisterData() const;
    uint32 GetSelect1RegisterData() const;

private:
    const Device& m_device;

    PAL_DISALLOW_DEFAULT_CTOR(StreamingPerfCounter);
    PAL_DISALLOW_COPY_AND_ASSIGN(StreamingPerfCounter);
};

} // Gfx6
} // Pal
