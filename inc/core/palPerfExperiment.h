/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2017 Advanced Micro Devices, Inc. All Rights Reserved.
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
/**
 ***********************************************************************************************************************
 * @file  palPerfExperiment.h
 * @brief Defines the Platform Abstraction Library (PAL) IPerfExperiment interface and related types.
 ***********************************************************************************************************************
 */

#pragma once

#include "pal.h"
#include "palGpuMemoryBindable.h"

namespace Pal
{

/// Specifies a particular block on the GPU to gather counters for.
enum class GpuBlock : uint32
{
    Cpf = 0,
    Ia,
    Vgt,
    Pa,
    Sc,
    Spi,
    Sq,
    Sx,
    Ta,
    Td,
    Tcp,
    Tcc,
    Tca,
    Db,
    Cb,
    Gds,
    Srbm,
    Grbm,
    GrbmSe,
    Rlc,
    Dma,
    Mc,
    Cpg,
    Cpc,
    Wd,
    Tcs,
    Atc,
    AtcL2,
    McVmL2,
    Ea,
    Rpb,
    Rmi,
    Count
};

/// Distinguishes between global and streaming performance monitor (SPM) counters.
enum class PerfCounterType : uint32
{
    Global = 0x0,
    Spm    = 0x1,
    Count
};

/// Reports the type of data the hardware writes for a particular counter.
enum class PerfCounterDataType : uint32
{
    Uint32 = 0x0,
    Uint64 = 0x1,
    Count
};

/// Distinguishes between normal thread traces and streaming performance monitor (SPM) traces.
enum class PerfTraceType : uint32
{
    ThreadTrace = 0x0,
    SpmTrace    = 0x1,
    Count
};

/// Mask values ORed together to choose which shader stages a performance experiment should sample.
enum PerfExperimentShaderFlags
{
    PerfShaderMaskPs  = 0x01,
    PerfShaderMaskVs  = 0x02,
    PerfShaderMaskGs  = 0x04,
    PerfShaderMaskEs  = 0x08,
    PerfShaderMaskHs  = 0x10,
    PerfShaderMaskLs  = 0x20,
    PerfShaderMaskCs  = 0x40,
    PerfShaderMaskAll = 0x7f,
};

/// Selects one of two supported generic performance trace markers, which the client can use to track data of its own
/// choosing.
enum class PerfTraceMarkerType : uint32
{
    A = 0x0,
    B = 0x1,
    Count
};

/// Specifies available features in device for supporting performance measurements.
union PerfExperimentDeviceFeatureFlags
{
    struct
    {
        uint32 counters             : 1;       ///< Device supports performance counters.
        uint32 threadTrace          : 1;       ///< Device supports thread traces.
        uint32 spmTrace             : 1;       ///< Device supports streaming perf monitor
                                               ///  traces.
        uint32 supportPs1Events     : 1;       ///< The thread trace HW of this Device
                                               ///  is capable of producing event tokens
                                               ///  from the second PS backend of SC.
        uint32 sqttBadScPackerId    : 1;       ///< Hardware is affected by bug causing the packer ID
                                               ///  specified in new PS waves to be incorrect in
                                               ///  SQ thread trace data.
        uint32 reserved             : 26;      ///< Reserved for future use.
    };
    uint32 u32All;                             ///< Feature flags packed as 32-bit uint.
};

/// Specifies properties for a perf counter being added to a perf experiment.  Input structure to
/// IPerfExperiment::AddCounter().
struct PerfCounterInfo
{
    PerfCounterType              counterType;  ///< Type of counter to add.
    GpuBlock                     block;        ///< Which block to reference.
    uint32                       instance;     ///< Which instance of that block type to reference.
    uint32                       eventId;      ///< Which event ID to track.

    union
    {
        struct
        {
            // SQ counter specific parameters
            uint32 sqSimdMask      :  1;

            // SQ counter specific parameters for GFXIP7+
            uint32 sqSqcBankMask   :  1;
            uint32 sqSqcClientMask :  1;

            uint32 reserved        : 29;
        };
        uint32 u32All;
    } optionFlags;

    struct
    {
        // SQ counter specific parameters
        uint32 sqSimdMask;

        // SQ counter specific parameters for GFXIP7+
        uint32 sqSqcBankMask;
        uint32 sqSqcClientMask;
    } optionValues;
};

/// Reports layout of a single global perf counter sample.
struct GlobalSampleLayout
{
    GpuBlock            block;             ///< Type of GPU block.
    uint32              instance;          ///< Which instance of that type of GPU block.
    uint32              slot;              ///< Slot varies in meaning per block.
    uint32              eventId;           ///< Sampled event ID.
    PerfCounterDataType dataType;          ///< What type of data is written (e.g., 32-bit uint).
    gpusize             beginValueOffset;  ///< Offset in bytes where the sample data begins.
    gpusize             endValueOffset;    ///< Offset in bytes where the sample data ends.
};

/// Describes the layout of global perf counter data in memory.
struct GlobalCounterLayout
{
    uint32             sampleCount;  ///< Number of samples described in samples[].
    GlobalSampleLayout samples[1];   ///< Describes the layout of each sample.  This structure is repeated (sampleCount
                                     ///  - 1) additional times.
};

/// Specifies properties for a perf trace being added to a perf experiment.  Input structure to
/// IPerfExperiment::AddTrace().
struct PerfTraceInfo
{
    PerfTraceType              traceType;    ///< Type of trace to add.
    uint32                     instance;     ///< Selected trace instance.

    union
    {
        struct
        {
            // Options common to all traces
            uint32 bufferSize                :  1;

            // Thread trace only options
            uint32 threadTraceTokenMask      :  1;
            uint32 threadTraceRegMask        :  1;
            uint32 threadTraceTargetSh       :  1;
            uint32 threadTraceTargetCu       :  1;
            uint32 threadTraceSh0CounterMask :  1;
            uint32 threadTraceSh1CounterMask :  1;
            uint32 threadTraceSimdMask       :  1;
            uint32 threadTraceVmIdMask       :  1;
            uint32 threadTraceRandomSeed     :  1;
            uint32 threadTraceShaderTypeMask :  1;
            uint32 threadTraceIssueMask      :  1;
            uint32 threadTraceWrapBuffer     :  1;

            // SPM trace only options
            uint32 spmTraceSampleInterval    :  1;

            uint32 reserved                  : 18;
        };
        uint32 u32All;
    } optionFlags;

    struct
    {
        // Options common to all traces
        size_t                    bufferSize;

        // Thread trace only options
        uint32                    threadTraceTokenMask;
        uint32                    threadTraceRegMask;
        uint32                    threadTraceTargetSh;
        uint32                    threadTraceTargetCu;
        uint32                    threadTraceSh0CounterMask;
        uint32                    threadTraceSh1CounterMask;
        uint32                    threadTraceSimdMask;
        uint32                    threadTraceVmIdMask;
        uint32                    threadTraceRandomSeed;
        PerfExperimentShaderFlags threadTraceShaderTypeMask;
        uint32                    threadTraceIssueMask;
        bool                      threadTraceWrapBuffer;

        // SPM trace only options
        uint32                    spmTraceSampleInterval;
    } optionValues;
};

/// Reports thread trace data written when the trace is stopped (copied from internal SQ registers).
struct ThreadTraceInfoData
{
    uint32 curOffset;     ///< Contents of SQ_THREAD_TRACE_WPTR register.
    uint32 traceStatus;   ///< Contents of SQ_THREAD_TRACE_STATUS register.
    uint32 writeCounter;  ///< Contents of SQ_THREAD_TRACE_CNTR register.
};

/// Describes the layout of a single shader engine's thread trace data.
struct ThreadTraceSeLayout
{
    uint32  shaderEngine;  ///< Shader engine index.
    uint32  computeUnit;   ///< Compute unit index.
    gpusize infoOffset;    ///< Offset to ThreadTraceInfoData in memory.
    gpusize infoSize;      ///< Size in bytes reserved for ThreadTraceInfoData.
    gpusize dataOffset;    ///< Offset in bytes to the actual trace data.
    gpusize dataSize;      ///< Amount of trace data, in bytes.
};

/// Describes how the thread trace data is laid out.
struct ThreadTraceLayout
{
    uint32              traceCount;  ///< Number of entries in traces[].
    ThreadTraceSeLayout traces[1];   ///< ThreadTraceSeLayout repeated (traceCount - 1) times.
};

/// Layout for a single sample of SPM trace data.
struct SpmSampleLayout
{
    GpuBlock block;     ///< GPU block type that created this data.
    uint32   instance;  ///< Which instance of that GPU block created the data.
    uint32   slot;      ///< Varies in meaning from block to block.
    uint32   eventId;   ///< Sampled event ID.
    gpusize  offset;    ///< Byte offset from the beginning of the segment.
};

/// Layout for data written by the GPU for an SPM trace.
struct SpmTraceLayout
{
    gpusize         dataOffset;   ///< Offset in bytes to the start of this data from the beginning of the perf
                                  ///  experiment GPU memory.
    gpusize         dataSize;     ///< Size in bytes of the data.  Multiple of segmentSize.
    gpusize         segmentSize;  ///< Size of a single data segment.
    uint32          sampleCount;  ///< Number of counter samples in a segment.
    SpmSampleLayout samples[1];   ///< SpmSampleLayout repeated (sampleCount - 1) additional times.
};

/// Specifies properties for creation of an @ref IPerfExperiment object.  Input structure to
/// IDevice::CreatePerfExperiment().
struct PerfExperimentCreateInfo
{
    union
    {
        struct
        {
            uint32 cacheFlushOnCounterCollection :  1;
            uint32 sampleInternalOperations      :  1;
            uint32 sqShaderMask                  :  1;
            uint32 reserved                      : 29;
        };
        uint32 u32All;
    } optionFlags;

    struct
    {
        bool                      cacheFlushOnCounterCollection;
        bool                      sampleInternalOperations;
        PerfExperimentShaderFlags sqShaderMask;
    } optionValues;
};

/**
 ***********************************************************************************************************************
 * @interface IPerfExperiment
 * @brief     Set of performance profiling activities to be performed over a specific range of commands in a command
 *            buffer.
 *
 * @warning The details of building a performance experiment are not very well documented here.  Please see your local
 *          hardware performance expert for more details until this documentation can be fully fleshed out.
 *
 * @see IDevice::CreatePerfExperiment
 ***********************************************************************************************************************
 */
class IPerfExperiment : public IGpuMemoryBindable
{
public:
    /// Adds the specified performance counter to be tracked as part of this perf experiment.
    ///
    /// @param [in] counterInfo Specifies which counter to add: which hardware block, instance, any options, etc.
    ///
    /// @returns Success if the counter was successfully added to the experiment, otherwise an appropriate error code.
    virtual Result AddCounter(
        const PerfCounterInfo& counterInfo) = 0;

    /// Queries the layout of counter results in memory for this perf experiment.
    ///
    /// @param [out] pLayout Layout describing the begin and end offset of each counter in the resulting GPU memory once
    ///                      this perf experiment is executed.  Should correspond with counters added via AddCounter().
    ///
    /// @returns Success if the layout was successfully returned in pLayout, otherwise an appropriate error code.
    virtual Result GetGlobalCounterLayout(
        GlobalCounterLayout* pLayout) const = 0;

    /// Addes the specified thread or SPM trace to be recorded as part of this perf experiment.
    ///
    /// @param [in] traceInfo Specifies what type of trace to record, which block instance to trace, and options, etc.
    ///
    /// @returns Success if the trace was successfully added to the experiment, otherwise an appropriate error code.
    virtual Result AddTrace(
        const PerfTraceInfo& traceInfo) = 0;

    /// Queries the layout of thread trace results in memory for this perf experiment.
    ///
    /// @param [out] pLayout Layout describing how the results of each thread trace will be written to GPU memory when
    ///                      this perf experiment is executed.  Should correspond with counters added via AddTrace().
    ///
    /// @returns Success if the layout was successfully returned in pLayout, otherwise an appropriate error code.
    virtual Result GetThreadTraceLayout(
        ThreadTraceLayout* pLayout) const = 0;

    /// Queries the layout of streaming performance monitor trace results in memory for this perf experiment.
    ///
    /// @param [out] pLayout Layout describing how the results of each SPM trace will be written to GPU memory when
    ///                      this perf experiment is executed.  Should correspond with counters added via AddTrace().
    ///
    /// @returns Success if the layout was successfully returned in pLayout, otherwise an appropriate error code.
    virtual Result GetSpmTraceLayout(
        SpmTraceLayout* pLayout) const = 0;

    /// Finalizes the performance experiment preparing it for execution.
    ///
    /// @returns Success if the operation executed successfully, otherwise an appropriate error code.
    virtual Result Finalize() = 0;

    /// Returns the value of the associated arbitrary client data pointer.
    /// Can be used to associate arbitrary data with a particular PAL object.
    ///
    /// @returns Pointer to client data.
    PAL_INLINE void* GetClientData() const
    {
        return m_pClientData;
    }

    /// Sets the value of the associated arbitrary client data pointer.
    /// Can be used to associate arbitrary data with a particular PAL object.
    ///
    /// @param  [in]    pClientData     A pointer to arbitrary client data.
    PAL_INLINE void SetClientData(
        void* pClientData)
    {
        m_pClientData = pClientData;
    }

protected:
    /// @internal Constructor. Prevent use of new operator on this interface. Client must create objects by explicitly
    /// called the proper create method.
    IPerfExperiment() : m_pClientData(nullptr) {}

    /// @internal Destructor.  Prevent use of delete operator on this interface.  Client must destroy objects by
    /// explicitly calling IDestroyable::Destroy() and is responsible for freeing the system memory allocated for the
    /// object on their own.
    virtual ~IPerfExperiment() { }

private:
    /// @internal Client data pointer. This can have an arbitrary value and can be returned by calling GetClientData()
    /// and set via SetClientData().
    /// For non-top-layer objects, this will point to the layer above the current object.
    void* m_pClientData;
};

} // Pal
