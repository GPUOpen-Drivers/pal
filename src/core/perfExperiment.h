/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/gpuMemory.h"
#include "palPerfExperiment.h"

namespace Pal
{

class CmdStream;
class Device;
class GfxCmdBuffer;
class Platform;

// These flags indicate whether Performance (Global) Counters, SPM Trace and/or Thread (SQ) Trace  have been
// enabled through this command buffer so that appropriate submit-time operations can be done.
union PerfExperimentFlags
{
    struct
    {
        uint32 perfCtrsEnabled   : 1;
        uint32 spmTraceEnabled   : 1;
        uint32 sqtTraceEnabled   : 1;
        uint32 dfSpmTraceEnabled : 1;
        uint32 reserved          : 28;
    };
    uint32 u32All;
};

/// This is all of the data that needs to be passed down to the KMD for them to start a DF SPM trace.
struct DfSpmPerfmonInfo
{
    GpuMemory* pDfSpmTraceBuffer;
    GpuMemory* pDfSpmMetadataBuffer;

    uint32 perfmonUsed;
    uint16 perfmonEvents[8];
    uint8  perfmonUnitMasks[8];

    uint32 samplingIntervalNs;
};

// =====================================================================================================================
// Core implementation of the IPerfExperiment interface.
class PerfExperiment : public IPerfExperiment
{
public:
    virtual void GetGpuMemoryRequirements(GpuMemoryRequirements* pGpuMemReqs) const override;
    virtual Result BindGpuMemory(IGpuMemory* pGpuMemory, gpusize offset) override;

    virtual void Destroy() override { this->~PerfExperiment(); }

    // These functions are called internally by our command buffers.
    virtual void IssueBegin(GfxCmdBuffer* pCmdBuffer, CmdStream* pPalCmdStream) const = 0;
    virtual void IssueEnd(GfxCmdBuffer* pCmdBuffer, CmdStream* pPalCmdStream) const = 0;

    virtual void BeginInternalOps(CmdStream* pPalCmdStream) const = 0;
    virtual void EndInternalOps(CmdStream* pPalCmdStream) const = 0;

    virtual void UpdateSqttTokenMask(CmdStream* pPalCmdStream, const ThreadTraceTokenConfig& sqttTokenConfig) const = 0;

    PerfExperimentFlags TracesEnabled() const { return m_perfExperimentFlags; }

    virtual const DfSpmPerfmonInfo* GetDfSpmPerfmonInfo() const = 0;

protected:
    PerfExperiment(Device* pDevice, const PerfExperimentCreateInfo& createInfo, gpusize memAlignment);
    virtual ~PerfExperiment();

    Device*                        m_pDevice;
    Platform*                      m_pPlatform;
    const PerfExperimentCreateInfo m_createInfo;
    const gpusize                  m_memAlignment;      // The GPU memory alignment required by this perf experiment.
    BoundGpuMemory                 m_gpuMemory;
    bool                           m_isFinalized;
    PerfExperimentFlags            m_perfExperimentFlags;

    // Information describing the size and layout of our bound GPU memory.
    gpusize                        m_globalBeginOffset; // Offset to the "begin" global counters.
    gpusize                        m_globalEndOffset;   // Offset to the "end" global counters.
    gpusize                        m_spmRingOffset;     // Offset to the SPM ring buffer.
    gpusize                        m_totalMemSize;

private:
    PAL_DISALLOW_DEFAULT_CTOR(PerfExperiment);
    PAL_DISALLOW_COPY_AND_ASSIGN(PerfExperiment);
};

} // Pal
