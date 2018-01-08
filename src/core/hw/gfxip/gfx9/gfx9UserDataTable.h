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

#include "core/hw/gfxip/gfx9/gfx9Chip.h"

namespace Pal
{
namespace Gfx9
{

// Contains the state of a ring buffer used for managing a user-data table stored in GPU memory. This is typically
// used for user-data tables which are managed by the constant engine.
struct UserDataRingBuffer
{
    gpusize  baseGpuVirtAddr; // Base GPU virtual address of the ring buffer memory
    uint32   instanceBytes;   // Size of each table instance contained in the ring buffer, in bytes
    uint32   numInstances;    // Number of table instances in the entire ring
    uint32   currRingPos;     // Currently active instance within the ring buffer
};

// Contains the state of a user-data table stored in GPU memory. The table could be managed using embedded data and
// the CPU, or it could be managed using the constant engine and a "staging area" in CE RAM.
struct UserDataTableState
{
    uint32  sizeInDwords; // Size of the user-data table, in DWORD's.

    // Offset into CE RAM (in bytes!) where the staging area is located. This can be zero if the table is being
    // managed using CPU updates instead of the constant engine.
    uint32  ceRamOffset;
    // CPU address of the embedded-data chunk storing the current copy of the table data. This will be null if the
    // table is being managed with the constant engine.
    uint32*  pCpuVirtAddr;

    struct
    {
        // GPU virtual address of the current copy of the table data. If the table is being managed by the constant
        // engine, this is a location in a GPU-memory ring buffer. Otherwise, it is the address of an embedded data
        // chunk.
        gpusize  gpuVirtAddr   : 62;
        // Indicates that the GPU virtual address of the current table location is dirty and should be rewritten to
        // hardware before the next draw.
        uint64   gpuAddrDirty  :  1;
        // If the table is being managed by the CPU, indicates that the copy of the table data on the CPU is more
        // up-to-date than the embedded GPU memory and should be copied before the next draw. Otherwise, indicates
        // that the copy of the table in CE RAM is more up-to-date than the GPU memory ring buffer and should be
        // dumped before the next draw.
        uint64   contentsDirty :  1;
    };
};

} // Gfx9
} // Pal
