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

#include "palPerfExperiment.h"

namespace Pal
{

class CmdStream;
class Device;

// Represents the maximum number of 16-bit streaming counters that a normal 64-bit summary counter supports.
constexpr uint32 MaxNumStreamingCtrPerSummaryCtr = 4;

// This represents the muxsel encoding of a single 16 bit streaming performance counter. The RLC uses this code to
// select the appropriate counter from all the streaming counter data that is sent from various blocks and write to mem.
// This format is used for both global and per-SE blocks till Gfx9.
union MuxselEncoding
{
    struct
    {
        uint16 counter  : 6; // The counter within the instance to select.
        uint16 block    : 5; // The block to select. See PerfCounterInfo.spmBlockSelectCode.
        uint16 instance : 5; // The instance of the block, if there are multiple instances.
    };

    uint16 u16All;           // All the fields above as a single uint16
};

// =====================================================================================================================
// Core implementation of the 'PerfCounter' object. PerfCounters are not exposed to the clients directly; rather, they
// are contained within an PerfExperiment object. Each object of this class encapsulates a single GPU performance
// counter instance.
class PerfCounter
{
public:
    /// Destructor has nothing to do.
    virtual ~PerfCounter() {}

    GpuBlock BlockType() const     { return m_info.block; }
    uint32   GetInstanceId() const { return m_info.instance; }
    uint32   GetSlot() const       { return m_slot; }
    uint32   GetEventId() const    { return m_info.eventId; }
    size_t   GetSampleSize() const { return m_dataSize; }
    gpusize  GetDataOffset() const { return m_dataOffset; }

    void SetDataOffset(gpusize offset) { m_dataOffset = offset; }

    // Compute the Shader Engine associated with this counter's global instance ID.
    static PAL_INLINE uint32 InstanceIdToSe(
        uint32 instancesPerShaderArray,
        uint32 shaderArraysPerSe,
        uint32 deviceInstance)
    {
        // SE is the truncated result of dividing our instanceId by the total instances per SE.
        return (deviceInstance / (instancesPerShaderArray * shaderArraysPerSe));
    }
    // Compute the Shader Array associated with this counter's global instance ID.
    static PAL_INLINE uint32 InstanceIdToSh(
        uint32 instancesPerShaderArray,
        uint32 shaderArraysPerSe,
        uint32 deviceInstance)
    {
        // SH is the modulus of the total arrays in our instanceId and number of arrays per SE.
        return ((deviceInstance / instancesPerShaderArray) % shaderArraysPerSe);
    }
    // Compute the Instance Index, relative to the shader array, associated with this counter's global instance ID.
    static PAL_INLINE uint32 InstanceIdToInstance(
        uint32 instancesPerShaderArray,
        uint32 deviceInstance)
    {
        // 'Local' instance index is the modulus of the global instance index and the number of instances per shader
        // array.
        return (deviceInstance % instancesPerShaderArray);
    }

protected:
    PerfCounter(Device* pDevice, const PerfCounterInfo& info, uint32 slot);

    const PerfCounterInfo    m_info;
    uint32                   m_slot;

    gpusize  m_dataOffset;    // GPU memory offset from the beginning of the 'start' and 'end' memory segments
    size_t   m_dataSize;      // Size of each data sample, in bytes

private:
    const Device&            m_device;

    PAL_DISALLOW_DEFAULT_CTOR(PerfCounter);
    PAL_DISALLOW_COPY_AND_ASSIGN(PerfCounter);
};

// =====================================================================================================================
// Represents one 64-bit HW counter which could be configured to track upto 4 events as 16-bit streaming counters.
// Separate HWL implementations exist because the bit widths of the perfcounter_select registers differ by gfxip family.
class StreamingPerfCounter
{
public:
    // Event id of unused slots in the HW streaming counter.
    static constexpr uint32 InvalidEventId = 0xFFFFFFFF;

    virtual ~StreamingPerfCounter() {}
    virtual Result AddEvent(const GpuBlock& block, uint32 eventId) = 0;
    virtual uint32* WriteSetupCommands(CmdStream* pCmdStream, uint32* pCmdSpace) = 0;
    virtual uint16 GetMuxselEncoding(uint32 subSlot) const = 0;

    GpuBlock BlockType() const                   { return m_block; }
    uint32   GetSlot() const                     { return m_slot; }
    uint32   GetInstanceId() const               { return m_instance; }
    uint32   GetEventId(uint32 subSlot) const    { return m_eventId[subSlot]; }
    gpusize  GetDataOffset(uint32 subSlot) const { return m_dataOffset[subSlot]; }
    SpmDataSegmentType GetSpmSegmentType() const { return m_segmentType; }

    void SetDataOffset(uint32 subSlot, gpusize offset) { m_dataOffset[subSlot] = offset; }

    // Returns true if the GPU block this counter samples from is indexed for reads and writes
    bool IsIndexed() const { return (m_flags.isIndexed != 0); }

protected:
    union Flags
    {
        struct
        {
            uint16 isIndexed     : 1;  // Indicates that this block has multiple instances and we would need to
                                       // program the GRBM_GFX_INDEX to select the correct instance.
            uint16 isGlobalBlock : 1;  // Indicates whether the counter belongs to the global or per-SE data segments.
            uint16 reserved      : 14;
        };

        uint16 u16All;
    };

    StreamingPerfCounter(Device* pDevice, GpuBlock block, uint32 instance, uint32 slot);

    GpuBlock m_block;    // The block this streaming perf counter represents.
    uint32   m_instance; // The device instance of this block.
    uint32   m_slot;     // The counter ID of this streaming counter.

    // Events tracked by each 16 bit sub-slot. Note: for SQ, only the first element is used for tracking event id
    // since each 64bit SQ counter only supports one 16 bit counter.
    uint32   m_eventId[MaxNumStreamingCtrPerSummaryCtr];

    gpusize            m_dataOffset[MaxNumStreamingCtrPerSummaryCtr]; // Data offset in the results buffer.
    SpmDataSegmentType m_segmentType;                                 // The segment this counter belongs to.
    Flags              m_flags;                                       // Internal flags.

    const Device& m_device;

private:
    PAL_DISALLOW_DEFAULT_CTOR(StreamingPerfCounter);
    PAL_DISALLOW_COPY_AND_ASSIGN(StreamingPerfCounter);
};

} // Pal
