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

#include "palPerfExperiment.h"

namespace Pal
{

// Forward decl's
class CmdStream;
class Device;

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
    PerfTrace(Device* pDevice, const PerfTraceInfo& info);

    const Device& m_device;

    gpusize       m_dataOffset;  // GPU memory offset to the beginning of this trace
    size_t        m_dataSize;    // Size of the trace GPU memory buffer, in bytes

private:
    PAL_DISALLOW_DEFAULT_CTOR(PerfTrace);
    PAL_DISALLOW_COPY_AND_ASSIGN(PerfTrace);
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
    ThreadTrace(Device* pDevice, const PerfTraceInfo& info);

    const uint32   m_shaderEngine;  // Shader Engine this thread trace runs on

    gpusize        m_infoOffset;    // GPU memory offset to the beginning of the "info data"
    const size_t   m_infoSize;      // Size of the thread trace's "info data", in bytes

private:
    PAL_DISALLOW_DEFAULT_CTOR(ThreadTrace);
    PAL_DISALLOW_COPY_AND_ASSIGN(ThreadTrace);
};

} // Pal
