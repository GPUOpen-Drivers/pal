/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/gfx9/gfx9Chip.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9PerfCtrInfo.h"
#include "core/perfTrace.h"
#include "palPerfExperiment.h"

namespace Pal
{
namespace Gfx9
{

class CmdStream;

// =====================================================================================================================
// Implements Gfx9-specific functionality for SPM traces.
class Gfx9SpmTrace : public Pal::SpmTrace
{
public:
    explicit Gfx9SpmTrace(const Device* pDevice);

    virtual uint32* WriteSetupCommands(gpusize ringBaseAddr, Pal::CmdStream* pCmdStream, uint32*  pCmdSpace) override;
    virtual uint32* WriteStartCommands(Pal::CmdStream* pCmdStream, uint32* pCmdSpace) override;
    virtual uint32* WriteEndCommands(Pal::CmdStream* pCmdStream, uint32* pCmdSpace) override;
    virtual Result  Init(const SpmTraceCreateInfo& createInfo) override;

    gpusize GetRingSize() const override { return m_ringSize.bits.RING_BASE_SIZE; }

private:
    const Device& m_device;

    regRLC_SPM_PERFMON_RING_SIZE    m_ringSize;
    regRLC_SPM_PERFMON_RING_BASE_HI m_ringBaseHi;
    regRLC_SPM_PERFMON_RING_BASE_LO m_ringBaseLo;
    regRLC_SPM_PERFMON_CNTL         m_spmPerfmonCntl;

    PAL_DISALLOW_DEFAULT_CTOR(Gfx9SpmTrace);
    PAL_DISALLOW_COPY_AND_ASSIGN(Gfx9SpmTrace);
};

// =====================================================================================================================
// Provides HWL-specific functionality for thread traces.
class ThreadTrace : public Pal::ThreadTrace
{
public:
    /// Destructor has nothing to do.
    virtual ~ThreadTrace() {}

    /// Returns the alignment requirement for a thread trace's data segment.
    size_t GetDataAlignment() const { return PerfCtrInfo::BufferAlignment; }

    /// Returns the alignment requirement for a thread trace's info segment (DWORD aligned).
    size_t GetInfoAlignment() const { return sizeof(uint32); }

    virtual uint32* WriteSetupCommands(gpusize baseGpuVirtAddr, CmdStream* pCmdStream, uint32* pCmdSpace) const = 0;
    virtual uint32* WriteStartCommands(CmdStream* pCmdStream, uint32* pCmdSpace) const = 0;
    virtual uint32* WriteUpdateSqttTokenMaskCommands(
        CmdStream* pCmdStream,
        uint32*    pCmdSpace,
        uint32     sqttTokenMask) const = 0;
    virtual uint32* WriteStopCommands(gpusize baseGpuVirtAddr, CmdStream* pCmdStream, uint32* pCmdSpace) const = 0;

    uint32* WriteInsertMarker(
        PerfTraceMarkerType markerType,
        uint32              data,
        CmdStream*          pCmdStream,
        uint32*             pCmdSpace) const;

    virtual Result Init() { return Result::Success; }

protected:
    ThreadTrace(const Device* pDevice, const ThreadTraceInfo& info);

    const Device&       m_device;
    const ThreadTraceInfo m_info;

private:
    PAL_DISALLOW_DEFAULT_CTOR(ThreadTrace);
    PAL_DISALLOW_COPY_AND_ASSIGN(ThreadTrace);
};

// =====================================================================================================================
// Provides GFX9-specific functionality for thread traces.
class Gfx9ThreadTrace : public ThreadTrace
{
public:
    Gfx9ThreadTrace(const Device* pDevice, const ThreadTraceInfo& info);

    virtual ~Gfx9ThreadTrace() {}

    // Returns the CU that was selected for this thread trace.
    virtual uint32 GetComputeUnit() const override { return m_sqThreadTraceMask.bits.CU_SEL; }

    virtual uint32* WriteSetupCommands(
        gpusize    baseGpuVirtAddr,
        CmdStream* pCmdStream,
        uint32*    pCmdSpace) const override;

    virtual uint32* WriteStartCommands(
        CmdStream* pCmdStream,
        uint32*    pCmdSpace) const override;

    virtual uint32* WriteUpdateSqttTokenMaskCommands(
        CmdStream* pCmdStream,
        uint32*    pCmdSpace,
        uint32     sqttTokenMask) const override;

    virtual uint32* WriteStopCommands(
        gpusize    baseGpuVirtAddr,
        CmdStream* pCmdStream,
        uint32*    pCmdSpace) const override;

    Result Init() override;

private:
    void    SetOptions();
    uint32* WriteGrbmGfxIndex(CmdStream* pCmdStream, uint32* pCmdSpace) const;

    /// Default thread trace SIMD mask: enable all four SIMD's.
    static constexpr uint32 SimdMaskAll = 0xF;
    /// Default thread trace Token mask: enable all 16 token types.
    static constexpr uint32 TokenMaskAll = 0xFFFF;
    /// Default thread trace register mask: enable all 8 register types.
    static constexpr uint32 RegMaskAll = 0xFF;
    /// Default thread trace CU mask: enable all CU's in a shader array.
    static constexpr uint32 ShCuMaskAll = 0xFFFF;

    regSQ_THREAD_TRACE_SIZE__GFX09        m_sqThreadTraceSize;      ///< Size of thread trace buffer
    regSQ_THREAD_TRACE_MODE__GFX09        m_sqThreadTraceMode;      ///< Thread trace mode
    regSQ_THREAD_TRACE_MASK__GFX09        m_sqThreadTraceMask;      ///< Thread trace wave mask
    regSQ_THREAD_TRACE_TOKEN_MASK__GFX09  m_sqThreadTraceTokenMask; ///< Thread trace token mask
    regSQ_THREAD_TRACE_PERF_MASK__GFX09   m_sqThreadTracePerfMask;  ///< Thread trace perf mask

    PAL_DISALLOW_DEFAULT_CTOR(Gfx9ThreadTrace);
    PAL_DISALLOW_COPY_AND_ASSIGN(Gfx9ThreadTrace);
};

} // Gfx9
} // Pal
