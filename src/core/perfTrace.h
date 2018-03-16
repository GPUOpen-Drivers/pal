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

#include "palDeque.h"
#include "perfCounter.h"
#include "palPerfExperiment.h"
#include "palUtil.h"

namespace Pal
{

// Forward decl's
class CmdStream;
class Device;

constexpr uint32 MaxNumShaderEngines = 4;
constexpr uint32 NumWordsPerBitLine = 256 / 16;

// Datatypes used for streaming performance counters common for both hardware layers.
// This tracks the number of even and odd counters enabled in a given block instance for spm traces.
struct ParityCount
{
    uint32 evenCount;   // number of counters with even index.
    uint32 oddCount;    // number of counters with odd index.
};

// In some cases we need 16-bit addressability for the muxsel ram, in other cases we need 32 bit addressability. We use
// as convenience union.
union MuxselRamData
{
    uint32* pMuxselRamUint32;
    uint16* pMuxselRamUint16;
};

// Encoding for the muxsel ram data used for configuring the SPM trace. This corresponds to the PERFMON_SEL_DATA of
// the per-SE and global muxsel data registers in RLC.
union PerfmonSelData
{
    struct
    {
        uint16 counter  : 6;
        uint16 block    : 5;
        uint16 instance : 5;
    };

    uint16 u16All;
};

// Flags used to track properties of the created spm trace.
union SpmTraceFlags
{
    struct
    {
        uint16 hasIndexedCounters : 1;  // Has counters that are indexed and have to be programmed using GRBM_GFX_INDEX.
        uint16 reserved           : 15;
    };

    uint16 u16All;
};

// =====================================================================================================================
// Core implementation of the 'PerfTrace' object. PerfTrace serves as a common base for both Thread Trace and
// SPM Trace objects.
class PerfTrace
{
public:
    virtual ~PerfTrace() {}

    // Getter for the size of the trace's data segment.
    size_t GetDataSize() const { return m_dataSize; }

    // Getter for the data offset to this trace's data segment.
    gpusize GetDataOffset() const { return m_dataOffset; }

    // Set a new value for the offset to this trace's data segment.
    void SetDataOffset(gpusize offset) { m_dataOffset = offset; }

protected:
    explicit PerfTrace(Device* pDevice);

    const Device& m_device;

    gpusize       m_dataOffset;  // GPU memory offset to the beginning of this trace
    size_t        m_dataSize;    // Size of the trace GPU memory buffer, in bytes

private:
    PAL_DISALLOW_DEFAULT_CTOR(PerfTrace);
    PAL_DISALLOW_COPY_AND_ASSIGN(PerfTrace);
};

// =====================================================================================================================
// Core implementation of SpmTrace. Unlike for ThreadTrace, where a unique instance is created for each ShaderEngine,
// one SpmTrace corresponds to management of state for the entire GPU.
class SpmTrace : public PerfTrace
{
public:
    // Represents the size of each "bit line" in each segment (global, se0, se1 etc) of a single sample worth of
    // spm data.
    static constexpr uint32 NumBitsPerBitline       = 256;

    // Represents the number of 16bit entries in a bit line in mux ram and in the sample data.
    static constexpr uint32 MuxselEntriesPerBitline = 16;

    typedef uint32 SpmDataSegmentSizes[static_cast<uint32>(SpmDataSegmentType::Count) + 1];

    virtual ~SpmTrace();
    virtual uint32* WriteSetupCommands(gpusize ringBaseAddr, CmdStream* pCmdStream, uint32* pCmdSpace) = 0;
    virtual uint32* WriteStartCommands(CmdStream* pCmdStream, uint32* pCmdSpace) = 0;
    virtual uint32* WriteEndCommands(CmdStream* pCmdStream, uint32* pCmdSpace) = 0;
    virtual gpusize GetRingSize() const = 0;
    virtual Result  Init(const SpmTraceCreateInfo& createInfo) = 0;

    Result GetTraceLayout(SpmTraceLayout* pLayout) const;
    Result Finalize();
    Result AddStreamingCounter(StreamingPerfCounter* pCounter);

protected:
    explicit SpmTrace(Device* pDevice);

    uint32 GetMuxselRamDwords(uint32 seIndex) const;

    void CalculateSegmentSizes();
    Result CalculateMuxselRam();

    Util::Deque<StreamingPerfCounter*, Platform> m_spmCounters;     // Represents HW counters.
    uint32                                       m_spmInterval;     // Spm trace sampling interval.
    uint32                                       m_numPerfCounters; // Number of perf counters in this trace.
    PerfCounterInfo*                             m_pPerfCounterCreateInfos; // Local copy of create infos.
    SpmTraceFlags                                m_flags;
    MuxselRamData                                m_muxselRamData[static_cast<uint32>(SpmDataSegmentType::Count)];
    SpmDataSegmentSizes                          m_segmentSizes;    // Number of 256-bit lines per segment.
    bool                                         m_ctrLimitReached; // Indicates that the number of counters per segment
                                                                    // exceeds 31.
private:

    PAL_DISALLOW_DEFAULT_CTOR(SpmTrace);
    PAL_DISALLOW_COPY_AND_ASSIGN(SpmTrace);
};

// =====================================================================================================================
// Core implementation of the 'ThreadTrace' object. ThreadTraces are not exposed to the client directly; rather, they
// are contained within an PerfExperiment object. Each object of this class encapsulates a single SE's thread trace
// instance.
class ThreadTrace : public PerfTrace
{
public:
    virtual ~ThreadTrace() {}

    // Getter for the Shader Engine this thread trace runs on.
    uint32 GetShaderEngine() const { return m_shaderEngine; }

    // Getter for the Compute Unit this thread trace runs on.
    virtual uint32 GetComputeUnit() const = 0;

    // Getter for the size of the thread trace's info segment.
    size_t GetInfoSize() const { return m_infoSize; }

    // Getter for the offset to the thread trace's info segment.
    gpusize GetInfoOffset() const { return m_infoOffset; }

    // Set a new value for the offset to this thread trace's info segment.
    void SetInfoOffset(gpusize offset) { m_infoOffset = offset; }

    // Returns the alignment requirement for a thread trace's data segment.
    virtual size_t GetDataAlignment() const = 0;

    // Returns the alignment requirement for a thread trace's info segment.
    virtual size_t GetInfoAlignment() const = 0;

protected:
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 373
    ThreadTrace(Device* pDevice, const PerfTraceInfo& info);
#else
    ThreadTrace(Device* pDevice, const ThreadTraceInfo& info);
#endif

    const uint32   m_shaderEngine;  // Shader Engine this thread trace runs on

    gpusize        m_infoOffset;    // GPU memory offset to the beginning of the "info data"
    const size_t   m_infoSize;      // Size of the thread trace's "info data", in bytes

private:
    PAL_DISALLOW_DEFAULT_CTOR(ThreadTrace);
    PAL_DISALLOW_COPY_AND_ASSIGN(ThreadTrace);
};

} // Pal
