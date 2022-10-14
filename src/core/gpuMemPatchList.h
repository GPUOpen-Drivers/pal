/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "palQueue.h"
#include "palVector.h"

namespace Pal
{

// Forward declarations:
class  CmdAllocator;
class  Device;
class  GpuMemory;
class  ICmdAllocator;
struct GpuMemoryRef;

// Enumerates the different types of patch operations each entry in a patch list can have. These are used by the KMD to
// determine how the resource being patched is used.
enum class GpuMemoryPatchOp : uint32
{
    NoOp = 0,
    VceSurfAddrLo,
    VceSurfAddrHi,
    VceSessionId,
    UvdSurfAddrLo,
    UvdSurfAddrHi,
    VideoSessionId,
    SpuSurfBufLo,
    SpuSurfBufHi,
    PspSurfBufLo,
    PspSurfBufHi,
    Count,
};

// patch type used for IB patching
enum class PatchType : uint32
{
    None = 0,
    Input,
    Output,
    FeedBack,
    InputOutput,
    Count,
};

// An entry in a patch list: contains information informing the KMD how to patch a GPU memory object's physical GPU
// address into a command buffer at submission-time.
struct GpuMemoryPatchEntry
{
    uint32 gpuMemRefIdx;            // Which entry in the GPU memory reference list this patch entry refers to
    uint32 gpuMemOffset;            // Offset into the GPU memory object of the patched address

    uint32 chunkIdx;                // Identifies the chunk in the command stream which gets patched
    uint32 chunkOffset;             // Offset into that chunk where the address gets patched

    GpuMemoryPatchOp  patchOp;      // Opcode describing the use-case of the GPU memory resource
    uint32            patchOpNum;   // Opaque number which distinguishes multiple patch entries with the same opcode

    union
    {
        struct
        {
            uint32 readOnly         :  1;   // The operation being patched in the command buffer doesn't write to
                                            // the GPU memory allocation
            uint32 highEntryFollows :  1;   // Indicates that the high bits of a patched address are contained in the
                                            // next patch list entry.
            uint32 reserved         : 30;
        };
        uint32 u32All;
    } flags;

};

// =====================================================================================================================
// Manages a GPU memory reference list and patch-location list associated with a single command stream. This is used to
// store the information needed by the KMD when submitting a command buffer to be executed on a Queue using physical
// mode addressing for GPU memory. In such a model, the UMD cannot write GPU addresses into its command buffer because
// we don't know the physical GPU addresses for allocations.
class GpuMemoryPatchList
{
    // Useful shorthands for vectors of memory references and memory patch entries.
    typedef Util::Vector<GpuMemoryRef, 16, Platform>  MemoryRefVector;
    typedef Util::Vector<GpuMemoryPatchEntry, 16, Platform>  PatchEntryVector;

public:
    explicit GpuMemoryPatchList(
        Device* pDevice);

    ~GpuMemoryPatchList();

    // Resets this patch list by clearing the contents of the memory reference and patch entry lists.
    void Reset();

    Result AddPatchEntry(
        GpuMemory*       pGpuMem,
        gpusize          gpuMemOffset,
        GpuMemoryPatchOp patchOp,
        uint32           patchOpNum,
        bool             readOnly,
        uint32           chunkIdx,
        uint32           chunkOffset);

    Result AddWidePatchEntry(
        GpuMemory*       pGpuMem,
        gpusize          gpuMemOffset,
        GpuMemoryPatchOp patchOpLo,
        uint32           patchOpNumLo,
        GpuMemoryPatchOp patchOpHi,
        uint32           patchOpNumHi,
        bool             readOnly,
        uint32           chunkIdx,
        uint32           chunkOffsetLo,
        uint32           chunkOffsetHi,
        gpusize          patchSize   = 0,
        PatchType        patchType   = PatchType::None,
        CmdAllocType     patchBuffer = CmdAllocType::CommandDataAlloc,
        GpuMemory*       pPatchBufferGpuMem = nullptr);

    uint32 NumMemoryRefs() const { return m_gpuMemoryRefs.NumElements(); }
    uint32 NumPatchEntries() const { return m_patchEntries.NumElements(); }

    MemoryRefVector::Iter GetMemoryRefIter() const { return m_gpuMemoryRefs.Begin(); }
    PatchEntryVector::Iter GetPatchEntryIter() const { return m_patchEntries.Begin(); }

private:
    Result FindGpuMemoryRefIndex(
        GpuMemory* pGpuMem,
        bool       readOnly,
        uint32*    pIndex);

    Device*const  m_pDevice;

    MemoryRefVector   m_gpuMemoryRefs;
    PatchEntryVector  m_patchEntries;

    PAL_DISALLOW_DEFAULT_CTOR(GpuMemoryPatchList);
    PAL_DISALLOW_COPY_AND_ASSIGN(GpuMemoryPatchList);
};

}
