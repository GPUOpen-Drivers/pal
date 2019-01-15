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

#include "core/hw/gfxip/gfx6/gfx6Chip.h"
#include "core/hw/gfxip/gfx6/gfx6Device.h"
#include "core/hw/gfxip/gfx6/gfx6PerfCtrInfo.h"
#include "core/perfTrace.h"
#include "palPerfExperiment.h"

namespace Pal
{
namespace Gfx6
{

class CmdStream;

// =====================================================================================================================
// Provides Gfx6-specific functionality for SPM traces.
class SpmTrace : public Pal::SpmTrace
{
public:
    explicit SpmTrace(const Device* pDevice);
    virtual ~SpmTrace();

    virtual uint32* WriteStartCommands(Pal::CmdStream* pCmdStream, uint32* pCmdSpace) override;
    virtual uint32* WriteEndCommands(Pal::CmdStream* pCmdStream, uint32* pCmdSpace) override;
    virtual uint32* WriteSetupCommands(gpusize ringBaseAddress, Pal::CmdStream* pCmdStream, uint32* pCmdSpace) override;
    virtual Result  Init(const SpmTraceCreateInfo& createInfo) override;

    virtual gpusize GetRingSize() const override { return static_cast<gpusize>(m_ringSize.bits.RING_BASE_SIZE); }

private:
    const Device& m_device;

    regRLC_SPM_PERFMON_RING_BASE_HI__CI__VI m_ringBaseHi;
    regRLC_SPM_PERFMON_RING_BASE_LO__CI__VI m_ringBaseLo;
    regRLC_SPM_PERFMON_RING_SIZE__CI__VI    m_ringSize;
    regRLC_SPM_PERFMON_CNTL__CI__VI         m_spmPerfmonCntl;
    regRLC_SPM_PERFMON_SEGMENT_SIZE__CI__VI m_segmentSize;    // Describes layout and number of 256-bit chunks of data
                                                              // per sample.

    PAL_DISALLOW_DEFAULT_CTOR(SpmTrace);
    PAL_DISALLOW_COPY_AND_ASSIGN(SpmTrace);
};

// =====================================================================================================================
// Provides Gfx6-specific functionality for thread traces.
class ThreadTrace : public Pal::ThreadTrace
{
public:
    ThreadTrace(const Device* pDevice, const ThreadTraceInfo& info);

    /// Destructor has nothing to do.
    virtual ~ThreadTrace() {}

    // Returns the CU that was selected for this thread trace.
    virtual uint32 GetComputeUnit() const override { return m_sqThreadTraceMask.bits.CU_SEL; }

    /// Returns the alignment requirement for a thread trace's data segment.
    virtual size_t GetDataAlignment() const override { return PerfCtrInfo::BufferAlignment; }

    /// Returns the alignment requirement for a thread trace's info segment (DWORD aligned).
    virtual size_t GetInfoAlignment() const override { return sizeof(uint32); }

    uint32* WriteSetupCommands(gpusize baseGpuVirtAddr, CmdStream* pCmdStream, uint32* pCmdSpace) const;
    uint32* WriteUpdateSqttTokenMaskCommands(
        CmdStream*                    pCmdStream,
        uint32*                       pCmdSpace,
        const ThreadTraceTokenConfig& sqttTokenConfig) const;
    uint32* WriteStartCommands(CmdStream* pCmdStream, uint32* pCmdSpace) const;
    uint32* WriteStopCommands(gpusize baseGpuVirtAddr, CmdStream* pCmdStream, uint32* pCmdSpace) const;

protected:
    void SetOptions(const ThreadTraceInfo& info);

private:
    // Represents the token mask register bit fields for gfx6+.
    union SqttTokenMask
    {
        struct
        {
            uint16 misc         : 1;
            uint16 timestamp    : 1;
            uint16 reg          : 1;
            uint16 waveStart    : 1;
            uint16 waveAlloc    : 1;
            uint16 regCsPriv    : 1;
            uint16 waveEnd      : 1;
            uint16 event        : 1;
            uint16 eventCs      : 1;
            uint16 eventGfx1    : 1;
            uint16 inst         : 1;
            uint16 instPc       : 1;
            uint16 instUserData : 1;
            uint16 issue        : 1;
            uint16 perf         : 1;
            uint16 regCs        : 1;
        };

        uint16 u16All;
    };

    // Represents the register mask bit field in the thread trace token mask register for gfxip 6+.
    union SqttRegMask
    {
        struct
        {
            uint8 eventInitiator         : 1;
            uint8 drawInitiator          : 1;
            uint8 dispatchInitiator      : 1;
            uint8 userData               : 1;
            uint8 ttMarkerEventInitiator : 1;
            uint8 gfxdec                 : 1;
            uint8 shdec                  : 1;
            uint8 other                  : 1;
        };
        uint8 u8All;
    };

    uint32* WriteGrbmGfxIndex(CmdStream* pCmdStream, uint32* pCmdSpace) const;
    void GetHwTokenConfig(const ThreadTraceTokenConfig& tokenConfig,
                          SqttTokenMask*                pTokenMask,
                          SqttRegMask*                  pRegMask) const;

    const Device& m_device;

    regSQ_THREAD_TRACE_SIZE        m_sqThreadTraceSize;      ///< Size of thread trace buffer
    regSQ_THREAD_TRACE_MODE        m_sqThreadTraceMode;      ///< Thread trace mode
    regSQ_THREAD_TRACE_MASK        m_sqThreadTraceMask;      ///< Thread trace wave mask
    regSQ_THREAD_TRACE_TOKEN_MASK  m_sqThreadTraceTokenMask; ///< Thread trace token mask
    regSQ_THREAD_TRACE_PERF_MASK   m_sqThreadTracePerfMask;  ///< Thread trace perf mask
    regSQ_THREAD_TRACE_HIWATER     m_sqThreadTraceHiWater;   ///< Thread trace hi water mark

    PAL_DISALLOW_DEFAULT_CTOR(ThreadTrace);
    PAL_DISALLOW_COPY_AND_ASSIGN(ThreadTrace);
};

} // Gfx6
} // Pal
