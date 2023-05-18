/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/device.h"
#include "core/gpuMemPatchList.h"
#include "palVectorImpl.h"

using namespace Util;

namespace Pal
{

// =====================================================================================================================
GpuMemoryPatchList::GpuMemoryPatchList(
    Device* pDevice)
    :
    m_pDevice(pDevice),
    m_gpuMemoryRefs(pDevice->GetPlatform()),
    m_patchEntries(pDevice->GetPlatform())
{
}

// =====================================================================================================================
GpuMemoryPatchList::~GpuMemoryPatchList()
{
}

// =====================================================================================================================
void GpuMemoryPatchList::Reset()
{
    m_gpuMemoryRefs.Clear();
    m_patchEntries.Clear();

    constexpr GpuMemoryRef NullMemoryRef = { };
    Result result = m_gpuMemoryRefs.PushBack(NullMemoryRef);

    // Note: This should never fail due to using an initial data buffer larger than one entry.
    PAL_ASSERT(result == Result::Success);
}

// =====================================================================================================================
// Adds a patch location and memory reference entry to the patch list.
Result GpuMemoryPatchList::AddPatchEntry(
    GpuMemory*       pGpuMem,
    gpusize          gpuMemOffset,
    GpuMemoryPatchOp patchOp,
    uint32           patchOpNum,
    bool             readOnly,
    uint32           chunkIdx,
    uint32           chunkOffset)
{
    PAL_ASSERT(HighPart(gpuMemOffset) == 0); // If this fires, the caller should be using AddWidePatchEntry()!
    PAL_ASSERT(patchOp != GpuMemoryPatchOp::Count);

    Result result = Result::Success;

    GpuMemoryPatchEntry entry = { };
    entry.flags.readOnly = (readOnly ? 1 : 0);
    entry.gpuMemOffset   = LowPart(gpuMemOffset);
    entry.chunkIdx       = chunkIdx;
    entry.chunkOffset    = chunkOffset;
    entry.patchOp        = patchOp;
    entry.patchOpNum     = patchOpNum;

    if (pGpuMem != nullptr)
    {
        result = FindGpuMemoryRefIndex(pGpuMem, readOnly, &entry.gpuMemRefIdx);
    }

    if (result == Result::Success)
    {
        result = m_patchEntries.PushBack(entry);
    }

    return result;
}

// =====================================================================================================================
// Adds a patch location and memory reference entry to the patch list. This version adds two patch location entries for
// addresses wider than 32 bits.
Result GpuMemoryPatchList::AddWidePatchEntry(
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
    gpusize          resourceSize,
    PatchType        resourceType,
    CmdAllocType     patchBuffer,
    GpuMemory*       pPatchBufferGpuMem)

{
    PAL_ASSERT((patchOpLo != GpuMemoryPatchOp::Count) && (patchOpHi != GpuMemoryPatchOp::Count));
    PAL_ASSERT((patchBuffer == CmdAllocType::CommandDataAlloc) || (patchBuffer == CmdAllocType::EmbeddedDataAlloc));

    Result result = Result::Success;

    GpuMemoryPatchEntry entry = { };
    entry.flags.highEntryFollows = 1;
    entry.flags.readOnly         = (readOnly ? 1 : 0);
    entry.gpuMemOffset           = LowPart(gpuMemOffset);
    entry.chunkIdx               = chunkIdx;
    entry.chunkOffset            = chunkOffsetLo;
    entry.patchOp                = patchOpLo;
    entry.patchOpNum             = patchOpNumLo;

    if (pGpuMem != nullptr)
    {
        result = FindGpuMemoryRefIndex(pGpuMem, readOnly, &entry.gpuMemRefIdx);
    }

    if (result == Result::Success)
    {
        result = m_patchEntries.PushBack(entry);
        if (result == Result::Success)
        {
            entry.flags.highEntryFollows = 0;
            entry.gpuMemOffset           = HighPart(gpuMemOffset);
            entry.chunkOffset            = chunkOffsetHi;
            entry.patchOp                = patchOpHi;
            entry.patchOpNum             = patchOpNumHi;
            result = m_patchEntries.PushBack(entry);
        }
    }

    return result;
}

// =====================================================================================================================
// Helper method which finds the index in the memory reference list where the specified GPU memory is located. If the
// memory object is not yet on the list, it will be added.
Result GpuMemoryPatchList::FindGpuMemoryRefIndex(
    GpuMemory* pGpuMem,
    bool       readOnly,
    uint32*    pIndex)      // out: Index into the memory reference list where the GPU memory object was found.
{
    PAL_ASSERT((pGpuMem != nullptr) && (pIndex != nullptr));

    Result result = Result::Success;

    // Note: Right now we're just doing a linear scan of the reference list to find the memory object's location. This
    // isn't the most efficient choice, but since the command buffers which use patch lists tend to have only a few
    // entries. For tiny patch/reference lists the linear scan isn't expected to be a problem.
    for ((*pIndex) = 1; (*pIndex) < m_gpuMemoryRefs.NumElements(); ++(*pIndex))
    {
        auto*const pMemRef = &m_gpuMemoryRefs.At(*pIndex);

        if (pMemRef->pGpuMemory == pGpuMem)
        {
            pMemRef->flags.readOnly = (readOnly ? pMemRef->flags.readOnly : 0);
            break;
        }
    }

    if ((*pIndex) == m_gpuMemoryRefs.NumElements())
    {
        // The memory object wasn't in the reference list before, so add it.
        GpuMemoryRef memRef   = { };
        memRef.pGpuMemory     = pGpuMem;
        memRef.flags.readOnly = (readOnly ? 1 : 0);

        result = m_gpuMemoryRefs.PushBack(memRef);
    }

    PAL_ASSERT((*pIndex) < m_gpuMemoryRefs.NumElements());
    return result;
}

}
