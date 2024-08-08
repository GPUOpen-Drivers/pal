/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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
// Blocks can be distributed across the GPU in a few different ways.
enum class PerfCounterDistribution : uint32
{
    Unavailable = 0,    // Performance counter is unavailable.
    PerShaderEngine,    // Performance counter instances are per shader engine.
    PerShaderArray,     // Performance counter instances are per shader array.
    GlobalBlock,        // Performance counter exists outside of the shader engines.
};

// Set this to the highest number of perf counter modules across all blocks (except the UMCCH which is a special case).
constexpr uint32 MaxPerfModules = 16;

// The "PERFCOUNTER" registers frequently change address values between ASICs. To help keep this mess out of the
// perf experiment code we define two structs and add them to the perf counter block info struct. Note that the
// interpretation of these values depends on other state from the block info struct and some fields will be zero
// if they have no meaning to a particular block or if that block has special case logic.

// This struct has any registers that might have different addresses for each module.
struct PerfCounterRegAddrPerModule
{
    uint32 selectOrCfg; // PERFCOUNTER#_SELECT or PERFCOUNTER#_CFG, depending on whether it's cfg-style.
    uint32 select1;     // PERFCOUNTER#_SELECT1 for perfmon modules.
    uint32 lo;          // PERCOUNTER#_LO or PERFCOUNTER_LO for cfg-style.
    uint32 hi;          // PERCOUNTER#_HI or PERFCOUNTER_HI for cfg-style.
};

// A container for all perf counter register addresses for a single block. This is only used by blocks which use the
// same register addresses for all instances. Some global blocks with multiple instances (e.g., SDMA) don't listen to
// GRBM_GFX_INDEX and instead have unique register addresses for each instance so they can't use this struct; they are
// rare so we treat them as special cases.
struct PerfCounterRegAddr
{
    uint32 perfcounterRsltCntl; // Cfg-style blocks define a shared PERFCOUNTER_RSLT_CNTL register.

    // Any registers that might have different addresses for each module. Indexed by the counter number in the registers
    // (e.g., the 2 in CB_PERFCOUNTER2_LO).
    PerfCounterRegAddrPerModule perfcounter[MaxPerfModules];
};

// Contains general information about perf counters for a HW block.
struct PerfCounterBlockInfo
{
    PerfCounterDistribution distribution;            // How the block is distributed across the chip.
    uint32                  numScopedInstances;      // Number of block instances in each distribution.
    uint32                  numInstances;            // Number of scoped instances multiplied by the number of distributions.
    uint32                  maxEventId;              // Maximum valid event ID for this block, note zero is valid.
    uint32                  num16BitSpmCounters;     // Maximum number of 16-bit SPM counters per instance.
    uint32                  num32BitSpmCounters;     // Maximum number of 32-bit SPM counters per instance.
    uint32                  numGlobalOnlyCounters;   // Number of global counters that are legacy only, per instance.
    uint32                  numGlobalSharedCounters; // Number of global counters that use the same counter state as
                                                     // SPM counters, per instance.

    // If the instance group size is equal to one, every block instance has its own independent counter hardware.
    // PAL guarantees this is true for all non-DF blocks.
    //
    // Otherwise the instance group size will be a value greater than one which indicates how many sequential
    // instances share the same counter hardware. The client must take care to not enable too many counters within
    // each of these groups.
    //
    // For example, the DfMall block may expose 16 instances with 8 global counters but define a group size of 16.
    // In that case all instances are part of one massive group which uses one pool of counter state such that no
    // combination of DfMall counter configurations can exceed 8 global counters.
    uint32 instanceGroupSize;

    // These fields are meant only for internal use in the perf experiment code.
    PerfCounterRegAddr regAddr;     // The perfcounter register addresses for this block.
    uint32 numGenericSpmModules;    // Number of SPM perfmon modules per instance. Can be configured as 1 global
                                    // counter, 1-2 32-bit SPM counters, or 1-4 16-bit SPM counters.
    uint32 numGenericLegacyModules; // Number of legacy (global only) counter modules per instance.
    uint32 numSpmWires;             // The number of 32-bit serial data wires going to the RLC. This is the ultimate
                                    // limit on the number of SPM counters.
    uint32 spmBlockSelect;          // Identifies this block in the RLC's SPM select logic.
    bool   isCfgStyle;              // An alternative counter programming model that: specifies legacy  "CFG" registers
                                    // instead of "SELECT" registers, uses a master "RSLT_CNTL" register, and can
                                    // optionally use generic SPM.
};

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
        uint32 dfCtrsEnabled     : 1;
        uint32 reserved          : 27;
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
