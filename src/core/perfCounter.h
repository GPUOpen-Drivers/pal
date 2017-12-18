/*
 *******************************************************************************
 *
 * Copyright (c) 2015-2017 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#pragma once

#include "palPerfExperiment.h"

namespace Pal
{

class Device;

// =====================================================================================================================
// Core implementation of the 'PerfCounter' object. PerfCounters are not exposed to the clients directly; rather, they
// are contained within an PerfExperiment object. Each object of this class encapsulates a single GPU performance
// counter instance.
class PerfCounter
{
public:
    /// Destructor has nothing to do.
    virtual ~PerfCounter() {}

    GpuBlock BlockType() const { return m_info.block; }
    uint32   GetInstanceId() const { return m_info.instance; }
    uint32   GetSlot() const { return m_slot; }
    uint32   GetEventId() const { return m_info.eventId; }
    size_t   GetSampleSize() const { return m_dataSize; }
    gpusize  GetDataOffset() const { return m_dataOffset; }

    void SetDataOffset(gpusize offset) { m_dataOffset = offset; }

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

} // Pal
