/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/perfExperiment.h"
#include "palVector.h"

namespace Pal
{

class  CmdStream;
struct Gfx6PerfCounterInfo;
struct GpuChipProperties;

namespace Gfx6
{

class CmdUtil;
class Device;

// Perf experiments must manage three types of data:
// - Global counters, also called legacy counters. 32 or 64 bit counters we manually read from registers.
// - SPM counters, also called streaming perf monitors. 16 or 32 bit counters automatically streamed to a ring buffer.
// - Thread traces, which stream shader instruction details to memory.
//
//
// SPM counters are implemented between the RLC and the measuring blocks. The measuring blocks instantiate one or more
// perfmon counter modules, each controlled by a pair of PERFCOUNTER#_SELECT/1 registers. Each module contains a pair
// of 32-bit SPM delta counters, each 32-bit counter can be configured as a single 32-bit counter or two independent
// 16-bit counters. Each 32-bit counter has a single wire back to the RLC. When the RLC sends a sample signal the
// counters latch to their current value and send it over the wire one bit at a time from bit 0 to bit 31. The counters
// repeat the same 32-bit value once every 32 clocks until the sampling is complete. Each individual instance of each
// block has its own select registers and perfmon modules so in theory every counter in every instance could be running
// a unique counter event at the same time.
//
// The RLC defines a few independent SPM sampling modules: one for global counters and one for each shader engine.
// Each sampling module contains a 256-bit counter staging register, a mux select ram, and writes to its own ring
// buffer. Every 16 cycles, 256 bits are read from the muxsel ram, giving the RLC 16 16-bit mux selects. Each select
// identifies a single input wire from a specific block instance and 32-bit perfmon counter. 16 bits are deserialzed
// from each of the 16 wires to fill the 256-bit staging register with 16 16-bit counter values; the 256-bit register
// is then written to memory. Each 256-bit portion of the ring and muxsel ram is called a segment. Note that the RLC
// reads in a segment in 16 clocks but it takes 32 clocks for each 32-bit perfmon counter to repeat its value. This
// means the RLC can only read from any 32-bit counter's lower 16 bits during "even" segments and can only read from
// the upper 16 bits during "odd" segments. The RLC must always read and write segments in an "even odd even odd..."
// pattern; if we have more of one type of counter than the other we must pad the muxsel ram with "don't care" selects.
// The last segment in the muxsel ram can be even or odd, there is no requirement that it be balanced.

// The perfmon block defines a counter module that other blocks must import to support the generic global counter and
// streaming counter functionality. Each counter is controlled by two select registers that can configure the whole
// counter as either a 64-bit global counter, two 32-bit SPM counters, or four 16-bit SPM counters. All blocks should
// duplicate this module exactly so we can use the CB registers as a template for all blocks.
struct PerfmonSelect
{
    regCB_PERFCOUNTER0_SELECT__CI__VI  sel0;
    regCB_PERFCOUNTER0_SELECT1__CI__VI sel1;
};

// Most blocks also define legacy global counter modules. They do not support SPM and only use one register.
// There are a wide variety of different legacy SELECT registers with varying field sizes and missing fields which
// makes it very difficult to pick one register for LegacySelect. The PA_SC registers have the largest PERF_SEL field
// and lack CNTR_MODE and PERF_MODE fields which make them the most generic. There is a static assert in the cpp file
// that verifies that CNTR_MODE and PERF_MODE can be ignored as long as we initialize them to zero.
typedef regPA_SC_PERFCOUNTER1_SELECT LegacySelect;

// A helper enum to identify what kind of select a given GenericSelect is.
enum class SelectType : uint8
{
    Perfmon = 0,
    LegacySel,
    Count
};

// To improve code reuse between blocks we define a generic counter select struct. Each select can be viewed as a
// perfmon module or a legacy module but not both. The inUse bitfield tracks which PEF_SEL fields are in use; for
// example, 0x1 indicates that the first 16-bit counter is in use and 0xF indicates that the whole module is in use.
struct GenericSelect
{
    uint8             inUse;     // Bitmask of which 16-bit sub counters are in use.
    SelectType        type;      // Which member of the union we should use.
    union
    {
        PerfmonSelect perfmon;   // This counter select programmed as a perfmon module.
        LegacySelect  legacySel; // This counter select programmed as a legacy global counter.
    };
};

// Most blocks implement a generic counter programming scheme with a fixed number of perfmon modules and legacy counters
// per instance. Any blocks that deviate from the generic scheme must be handled manually.
struct GenericBlockSelect
{
    bool              hasCounters;  // If any counters are in any module are in use.
    regGRBM_GFX_INDEX grbmGfxIndex; // Use this to communicate with this block instance.
    uint32            numModules;   // The length of pModules; the total number of perfmon and legacy modules.
    GenericSelect*    pModules;     // All perfmon and/or legacy modules in this block. Note that this will only
                                    // be allocated if the client enables a counter in this instance.
};

// A helper constant to remove this cast.
constexpr uint32 GpuBlockCount = static_cast<uint32>(GpuBlock::Count);

// Define a monolithic structure that can store every possible perf counter select register configuration. For most
// blocks we can use allocate one GenericBlockSelect per global instance. Some blocks require special handling.
struct GlobalSelectState
{
    // The SQ counters, implemented in the SQG block, are special. Each module has a single register with a unique
    // format that can be a legacy counter or a single 32-bit SPM counter on gfx7+. Note that the CI/VI select
    // registers are a superset of the SI select registers.
    struct
    {
        bool                              perfmonInUse[Gfx6MaxSqgPerfmonModules];
        bool                              hasCounters;
        regGRBM_GFX_INDEX                 grbmGfxIndex;
        regSQ_PERFCOUNTER0_SELECT__CI__VI perfmon[Gfx6MaxSqgPerfmonModules];
    } sqg[Gfx6MaxShaderEngines];

    // The GRBM is a global block but it defines one special counter per SE. We treat its global counters generically
    // under GpuBlock::Grbm but special case the per-SE counters using GpuBlock::GrbmSe.
    struct
    {
        bool                           hasCounter;
        regGRBM_SE0_PERFCOUNTER_SELECT select;     // This is a non-standard select register as well.
    } grbmSe[Gfx6MaxShaderEngines];

    // Each SDMA engine defines two global counters controlled by one register. Note that this doesn't exist on gfx6.
    struct
    {
        bool                          hasCounter[2]; // Each SDMA control manages two global counters.
        regSDMA0_PERFMON_CNTL__CI__VI perfmonCntl;   // This acts as two selects.
    } sdma[Gfx7MaxSdmaInstances];

    // Each MC instance defines a set of special global counters. Each MC[D] defines one pair of counters for each
    // channel. We treat each channel as an instance but must convert back to MCD tiles to index into this array.
    struct
    {
        bool                           hasCounters;                                     // If any counters are in use.
        bool                           counterInUse[NumMcChannels][NumMcCountersPerCh]; // If each counter is in use.
        regMC_SEQ_PERF_SEQ_CTL__SI__CI perfSeqCntl; // Contains the lower 4 bits of each counter select.
        regMC_SEQ_PERF_CNTL_1__SI__CI  perfCntl1;   // Contains the uppermost bit of each counter select.
    } mc[MaxMcdTiles];

    // The generic block state. These arrays are sparse in that elements can be zero or nullptr if:
    // - The block doesn't exist on our device.
    // - The block requires special handling (see above).
    // - The client hasn't enabled any counters that use this block.
    uint32              numGeneric[GpuBlockCount]; // The number of global instances in each generic array.
    GenericBlockSelect* pGeneric[GpuBlockCount];   // The set of generic registers for each block type and instance.
};

// A single 16-bit muxsel value.
union MuxselEncoding
{
    struct
    {
        uint16 counter  : 6; // A special ID used by the RLC to identify a specific 16-bit value on some SPM wire.
        uint16 block    : 5; // A special block enum defined by the RLC.
        uint16 instance : 5; // The local instance of the block.
    };

    uint16 u16All; // All the fields above as a single uint16
};

// By definition there are 16 16-bit counters per muxsel state machine segment. Unfortunately RLC uses "segment" to
// denote one set of counters written per iteration, this can get confusing to us because our interface splits the SPM
// ring buffer into one "segment" per parallel SPM unit. To avoid confusion we will call a RLC "segment" a "line".
//
// Thus here we define the line sizes and the maximum number of ring segments.
constexpr uint32 MuxselLineSizeInCounters = 16;
constexpr uint32 MuxselLineSizeInDwords   = (MuxselLineSizeInCounters * sizeof(MuxselEncoding)) / sizeof(uint32);
constexpr uint32 MaxNumSpmSegments        = static_cast<uint32>(SpmDataSegmentType::Count);

// A single programming line in the RLC muxsel state machine.
union SpmLineMapping
{
    MuxselEncoding muxsel[MuxselLineSizeInCounters];
    uint32         u32Array[MuxselLineSizeInDwords];
};

// A SE/SA/instance triplet that corresponds to some global instance. This is similar to GRBM_GFX_INDEX but the
// indices follow the same abstract ordering as the global instances. This information is needed in some cases where
// GRBM_GFX_INDEX has a special bit encoding that reorders the instances, preventing us from reusing the information.
struct InstanceMapping
{
    uint32 seIndex;       // The shader engine index or zero if the instance is global.
    uint32 saIndex;       // The shader array index or zero if the instance is global or per-SE.
    uint32 instanceIndex; // The block's hardware instance within the block's PerfCounterDistribution.
};

// Stores general information we need for a single counter of any type.
struct CounterMapping
{
    // Input information.
    GpuBlock            block;          // The gpu block this counter instance belongs to.
    uint32              globalInstance; // The global instance number of this counter.
    uint32              eventId;        // The event that was tracked by this counter.

    // The data type we use to send the counter's value back to the client. For global counters this is decided by
    // PAL. For SPM counters this is decided by the client (assumed to be 16-bit for now).
    PerfCounterDataType dataType;
};

// Stores information we need for a single global counter.
struct GlobalCounterMapping
{
    CounterMapping general;   // General counter information.
    uint32         counterId; // Which counter this is within its block.
    gpusize        offset;    // Offset within the begin/end global buffers to the counter's value.
};

// Stores information we need for a single SPM counter.
struct SpmCounterMapping
{
    CounterMapping     general;    // General counter information.

    // RLC muxsel information. Note that isEven and isOdd can both be true if the counter is 32-bit!
    SpmDataSegmentType segment;    // Segment this counter belongs to (global, Se0, Se1 etc).
    MuxselEncoding     evenMuxsel; // Selects the lower half of a specific SPM wire for some block instance.
    MuxselEncoding     oddMuxsel;  // Selects the upper half of a specific SPM wire for some block instance.
    bool               isEven;     // If the counter requires the lower 16-bits of a 32-bit counter wire.
    bool               isOdd;      // If the counter requires the upper 16-bits of a 32-bit counter wire.

    // Output information.
    gpusize            offsetLo;   // Offset within the segment's output buffer to the counter's lower 16 bits.
    gpusize            offsetHi;   // For 32-bit counters, the corresponding offset for the upper 16 bits.
};

// =====================================================================================================================
// Provides Gfx6-specific behavior for perf experiment objects.
class PerfExperiment : public Pal::PerfExperiment
{
public:
    PerfExperiment(const Device* pDevice, const PerfExperimentCreateInfo& createInfo);

    Result Init();

    virtual Result AddCounter(const PerfCounterInfo& counterInfo) override;
    virtual Result AddThreadTrace(const ThreadTraceInfo& traceInfo) override;
    virtual Result AddSpmTrace(const SpmTraceCreateInfo& spmCreateInfo) override;
    virtual Result Finalize() override;

    virtual Result GetGlobalCounterLayout(GlobalCounterLayout* pLayout) const override;
    virtual Result GetThreadTraceLayout(ThreadTraceLayout* pLayout) const override;
    virtual Result GetSpmTraceLayout(SpmTraceLayout* pLayout) const override;

    // These functions are called internally by our command buffers.
    virtual void IssueBegin(GfxCmdBuffer* pCmdBuffer, Pal::CmdStream* pPalCmdStream) const override;
    virtual void IssueEnd(GfxCmdBuffer* pCmdBuffer, Pal::CmdStream* pPalCmdStream) const override;

    virtual void BeginInternalOps(Pal::CmdStream* pPalCmdStream) const override;
    virtual void EndInternalOps(Pal::CmdStream* pPalCmdStream) const override;

    virtual void UpdateSqttTokenMask(
        Pal::CmdStream*               pPalCmdStream,
        const ThreadTraceTokenConfig& sqttTokenConfig) const override;

    static void UpdateSqttTokenMaskStatic(
        Pal::CmdStream*               pPalCmdStream,
        const ThreadTraceTokenConfig& sqttTokenConfig,
        const Device&                 device);

protected:
    virtual ~PerfExperiment();

private:
    Result AllocateGenericStructs(GpuBlock block, uint32 globalInstance);
    Result AddSpmCounter(const PerfCounterInfo& counterInfo, SpmCounterMapping* pMapping);
    Result BuildCounterMapping(const PerfCounterInfo& info, CounterMapping* pMapping) const;
    Result BuildInstanceMapping(GpuBlock block, uint32 globalInstance, InstanceMapping* pMapping) const;

    regGRBM_GFX_INDEX BuildGrbmGfxIndex(const InstanceMapping& mapping, GpuBlock block) const;

    // Here are a few helper functions which write into reserved command space.
    uint32* WriteSpmSetup(CmdStream* pCmdStream, uint32* pCmdSpace) const;
    uint32* WriteStartThreadTraces(CmdStream* pCmdStream, uint32* pCmdSpace) const;
    uint32* WriteStopThreadTraces(CmdStream* pCmdStream, uint32* pCmdSpace) const;
    uint32* WriteSelectRegisters(CmdStream* pCmdStream, uint32* pCmdSpace) const;
    uint32* WriteEnableCfgRegisters(bool enable, bool clear, CmdStream* pCmdStream, uint32* pCmdSpace) const;

    uint32* WriteStopAndSampleGlobalCounters(bool          isBeginSample,
                                             GfxCmdBuffer* pCmdBuffer,
                                             CmdStream*    pCmdStream,
                                             uint32*       pCmdSpace) const;

    uint32* WriteCopy64BitCounter(uint32     regAddrLo,
                                  uint32     regAddrHi,
                                  gpusize    destAddr,
                                  CmdStream* pCmdStream,
                                  uint32*    pCmdSpace) const;

    uint32* WriteGrbmGfxIndexBroadcastGlobal(CmdStream* pCmdStream, uint32* pCmdSpace) const;
    uint32* WriteGrbmGfxIndexBroadcastSe(uint32 seIndex, CmdStream* pCmdStream, uint32* pCmdSpace) const;
    uint32* WriteMcConfigBroadcastGlobal(CmdStream* pCmdStream, uint32* pCmdSpace) const;
    uint32* WriteMcConfigTargetInstance(uint32 mcdInstance, CmdStream* pCmdStream, uint32* pCmdSpace) const;
    uint32* WriteUpdateSpiConfigCntl(bool enableSqgEvents, CmdStream* pCmdStream, uint32* pCmdSpace) const;
    uint32* WriteUpdateWindowedCounters(bool enable, CmdStream* pCmdStream, uint32* pCmdSpace) const;
    uint32* WriteWaitIdle(bool flushCaches, GfxCmdBuffer* pCmdBuffer, CmdStream* pCmdStream, uint32* pCmdSpace) const;

    // Helper functions to check if we've enabled any counters for a block.
    bool HasGenericCounters(GpuBlock block) const;
    bool HasMcCounters() const;

    // Some helpful references.
    const GpuChipProperties&   m_chipProps;
    const Gfx6PerfCounterInfo& m_counterInfo;
    const Gfx6PalSettings&     m_settings;
    const RegisterInfo&        m_registerInfo;
    const CmdUtil&             m_cmdUtil;

    // Global counters are added iteratively so just use a vector to hold them.
    Util::Vector<GlobalCounterMapping, 32, Platform> m_globalCounters;

    // Thread trace state. Each SQG runs an independent thread trace.
    struct
    {
        bool                          inUse;        // If this thread trace is in use.
        gpusize                       infoOffset;   // The offset to the ThreadTraceInfoData within our GPU memory.
        gpusize                       bufferOffset; // The offset to the output buffer within our GPU memory.
        gpusize                       bufferSize;   // The size of this trace's output buffer in bytes.
        regGRBM_GFX_INDEX             grbmGfxIndex; // Used to write this trace's registers.
        regSQ_THREAD_TRACE_CTRL       ctrl;
        regSQ_THREAD_TRACE_MODE       mode;
        regSQ_THREAD_TRACE_MASK       mask;
        regSQ_THREAD_TRACE_PERF_MASK  perfMask;
        regSQ_THREAD_TRACE_TOKEN_MASK tokenMask;
    } m_sqtt[Gfx6MaxShaderEngines];

    // Global SPM state.
    SpmCounterMapping* m_pSpmCounters;                      // The list of all enabled SPM counters.
    uint32             m_numSpmCounters;
    SpmLineMapping*    m_pMuxselRams[MaxNumSpmSegments];    // One array of muxsel programmings for each segment.
    uint32             m_numMuxselLines[MaxNumSpmSegments];
    uint32             m_spmRingSize;                       // The SPM ring buffer size in bytes.
    uint16             m_spmSampleInterval;                 // The SPM sample interval in sclks.

    // A big struct that lists every block's PERFCOUNTER#_SELECT registers.
    GlobalSelectState m_select;

    PAL_DISALLOW_DEFAULT_CTOR(PerfExperiment);
    PAL_DISALLOW_COPY_AND_ASSIGN(PerfExperiment);
};

} // Gfx6
} // Pal
