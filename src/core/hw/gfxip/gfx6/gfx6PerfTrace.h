/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2017 Advanced Micro Devices, Inc. All Rights Reserved.
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
// Provides Gfx6-specific functionality for thread traces.
class ThreadTrace : public Pal::ThreadTrace
{
public:
    ThreadTrace(const Device* pDevice, const PerfTraceInfo& info);

    /// Destructor has nothing to do.
    ~ThreadTrace() {}

    // Returns the CU that was selected for this thread trace.
    uint32 GetComputeUnit() const override { return m_sqThreadTraceMask.bits.CU_SEL; }

    /// Returns the alignment requirement for a thread trace's data segment.
    size_t GetDataAlignment() const { return PerfCtrInfo::BufferAlignment; }

    /// Returns the alignment requirement for a thread trace's info segment (DWORD aligned).
    size_t GetInfoAlignment() const { return sizeof(uint32); }

    uint32* WriteSetupCommands(gpusize baseGpuVirtAddr, CmdStream* pCmdStream, uint32* pCmdSpace) const;
    uint32* WriteUpdateSqttTokenMaskCommands(
        CmdStream* pCmdStream,
        uint32*    pCmdSpace,
        uint32     sqttTokenMask) const;
    uint32* WriteStartCommands(CmdStream* pCmdStream, uint32* pCmdSpace) const;
    uint32* WriteStopCommands(gpusize baseGpuVirtAddr, CmdStream* pCmdStream, uint32* pCmdSpace) const;

    uint32* WriteInsertMarker(
        PerfTraceMarkerType markerType,
        uint32              data,
        CmdStream*          pCmdStream,
        uint32*             pCmdSpace) const;

protected:
    void SetOptions(const PerfTraceInfo& info);

private:
    uint32* WriteGrbmGfxIndex(CmdStream* pCmdStream, uint32* pCmdSpace) const;

    const Device& m_device;

    regSQ_THREAD_TRACE_SIZE        m_sqThreadTraceSize;      ///< Size of thread trace buffer
    regSQ_THREAD_TRACE_MODE        m_sqThreadTraceMode;      ///< Thread trace mode
    regSQ_THREAD_TRACE_MASK        m_sqThreadTraceMask;      ///< Thread trace wave mask
    regSQ_THREAD_TRACE_TOKEN_MASK  m_sqThreadTraceTokenMask; ///< Thread trace token mask
    regSQ_THREAD_TRACE_PERF_MASK   m_sqThreadTracePerfMask;  ///< Thread trace perf mask

    PAL_DISALLOW_DEFAULT_CTOR(ThreadTrace);
    PAL_DISALLOW_COPY_AND_ASSIGN(ThreadTrace);
};

} // Gfx6
} // Pal
