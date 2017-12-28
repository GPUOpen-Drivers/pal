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

#include "core/hw/gfxip/gfx9/gfx9CmdUtil.h"
#include "core/hw/gfxip/gfx9/gfx9UniversalCmdBuffer.h"
#include "core/hw/gfxip/gfx9/gfx9UserDataTable.h"

namespace Pal
{
namespace Gfx9
{

// =====================================================================================================================
// Helper function for resetting a user-data ring buffer at the beginning of a command buffer.
void PAL_INLINE ResetUserDataRingBuffer(
    UserDataRingBuffer* pRing)
{
    pRing->currRingPos = 0;
}

// =====================================================================================================================
// Helper function for resetting a user-data table at the beginning of a command buffer.
void PAL_INLINE ResetUserDataTable(
    UserDataTableState* pTable)
{
    pTable->pCpuVirtAddr  = nullptr;
    pTable->gpuVirtAddr   = 0uLL;
    pTable->gpuAddrDirty  = 0;
    pTable->contentsDirty = 0;
}

// =====================================================================================================================
// Helper function for relocating a user-data table which is backed by a caller managed ringed buffer. This will advance
// the user-data table instance to the next instance within the ring buffer, wrapping when necessary. The supplied
// universal command-buffer state will be updated to reflect any DE/CE synchronization needed for properly managing
// CE dumps.
PAL_INLINE void RelocateIndirectRingedUserDataTable(
    UniversalCmdBufferState* pState,
    UserDataTableState*      pTable,
    UserDataRingBuffer*      pRing)
{
    // Handle CE ring wrapping
    const bool hasWrapped = HandleCeRinging(pState, pRing->currRingPos, 1, pRing->numInstances);

    // Reset current ring position if wrapped
    if (hasWrapped)
    {
        pRing->currRingPos = 0;
    }

    // Compute a new indirect address offset for the CE ring table
    pTable->gpuVirtAddr = (pRing->currRingPos * pRing->instanceBytes);

    // Update current position
    pRing->currRingPos++;

    // Track that we have updated this table's GPU memory location. The GPU address will need to be rewritten
    // prior to the next draw or dispatch in which the pipeline will attempt to read the table's contents.
    pTable->gpuAddrDirty = 1;

    // Increment number of ring instances used count
    pState->nestedIndirectRingInstances++;
}

// =====================================================================================================================
// Helper function for relocating a user-data table which is stored in a command buffer's embedded data chunk(s). This
// will allocate a new chunk of embedded data from the calling command buffer to use for the relocated table instance.
// The amount of embedded memory allocated is only enough to store the data which the GPU will actually process (based
// on the active pipeline).
template <typename CommandBuffer>
PAL_INLINE void RelocateEmbeddedUserDataTable(
    CommandBuffer*      pSelf,
    UserDataTableState* pTable,
    uint32              offsetInDwords, // Offset into the table where the GPU will actually read from
    uint32              dwordsNeeded)   // Number of DWORD's actually needed for the table this time
{
    PAL_ASSERT((dwordsNeeded + offsetInDwords) <= pTable->sizeInDwords);

    gpusize gpuVirtAddr  = 0uLL;
    pTable->pCpuVirtAddr = (pSelf->CmdAllocateEmbeddedData(dwordsNeeded, 1, &gpuVirtAddr) - offsetInDwords);
    pTable->gpuVirtAddr  = (gpuVirtAddr - (sizeof(uint32) * offsetInDwords));

    // Track that we have updated this table's GPU memory location. The GPU address will need to be rewritten
    // prior to the next draw or dispatch in which the pipeline will attempt to read the table's contents.
    pTable->gpuAddrDirty = 1;
}

// =====================================================================================================================
// Helper function for relocated a user-data table which is stored in a per-Device or per-Queue ring buffer. This will
// advance the user-data table to the next table instance within the ring buffer, wrapping back to the beginning as
// necessary. The supplied universal command-buffer state will be updated to reflect any DE/CE synchronization needed
// for properly managing a CE ring buffer.
PAL_INLINE void RelocateRingedUserDataTable(
    UniversalCmdBufferState* pState,
    UserDataRingBuffer*      pRing,
    UserDataTableState*      pTable,
    uint32                   ringInstances)
{
    // Handle CE ring wrapping
    const bool hasWrapped = HandleCeRinging(pState, pRing->currRingPos, ringInstances, pRing->numInstances);

    // Reset current ring position if wrapped
    if (hasWrapped)
    {
        pRing->currRingPos = 0;
    }

    // Compute a new indirect address offset for the CE ring table
    pTable->gpuVirtAddr = (pRing->currRingPos * pRing->instanceBytes) + pRing->baseGpuVirtAddr;

    // Update current position
    pRing->currRingPos += ringInstances;

    // Track that we have updated this table's GPU memory location. The GPU address will need to be rewritten
    // prior to the next draw or dispatch in which the pipeline will attempt to read the table's contents.
    pTable->gpuAddrDirty = 1;
}

// =====================================================================================================================
// Helper function for relocating a user-data table
template <bool useRingBufferForCe>
PAL_INLINE void RelocateUserDataTable(
    UniversalCmdBuffer*      pSelf,
    UniversalCmdBufferState* pState,
    UserDataTableState*      pTable,
    UserDataRingBuffer*      pRing,
    UserDataRingBuffer*      pNestedIndirectRing,
    uint32                   offsetInDwords, // Offset into the table where the GPU will actually read from
    uint32                   dwordsNeeded
    )
{
    if (pState->flags.useIndirectAddrForCe)
    {
        RelocateIndirectRingedUserDataTable(pState, pTable, pNestedIndirectRing);
    }
    else if (useRingBufferForCe)
    {
        RelocateRingedUserDataTable(pState, pRing, pTable, 1);
    }
    else
    {
        RelocateEmbeddedUserDataTable<UniversalCmdBuffer>(pSelf, pTable, offsetInDwords, dwordsNeeded);
    }
}

// =====================================================================================================================
// Helper function to upload the contents of a user-data table which is being managed by the CPU. It is an error to call
// this before the table has been relocated to its new embedded data location!
PAL_INLINE void UploadToUserDataTableCpu(
    UserDataTableState* pTable,
    uint32              offsetInDwords, // Offset into the table where the GPU will actually read from
    uint32              dwordsNeeded,   // Number of DWORD's actually needed for the table this time
    const uint32*       pSrcData)       // [in] Points to the first DWORD of the entire table's contents, not the
                                        // first DWORD where the GPU actually reads from!
{
    PAL_ASSERT((pTable->pCpuVirtAddr  != nullptr) && (pTable->contentsDirty != 0));

    memcpy((pTable->pCpuVirtAddr + offsetInDwords), (pSrcData + offsetInDwords), (sizeof(uint32) * dwordsNeeded));

    // Mark that the latest contents of the user-data table have been uploaded to the current embedded data chunk.
    pTable->contentsDirty = 0;
}

// =====================================================================================================================
// Helper function to upload the contents of a user-data table which is being managed by CE RAM. It is an error to call
// this before the table has been relocated to its new embedded data location!
PAL_INLINE uint32* UploadToUserDataTableCeRam(
    const CmdUtil&      cmdUtil,
    UserDataTableState* pTable,
    uint32              offsetInDwords,
    uint32              dwordsNeeded,
    const uint32*       pSrcData,
    uint32              highWatermark,
    uint32*             pCeCmdSpace)
{
    PAL_ASSERT((dwordsNeeded + offsetInDwords) <= pTable->sizeInDwords);

    pCeCmdSpace += cmdUtil.BuildWriteConstRam(pSrcData,
                                              (pTable->ceRamOffset + (sizeof(uint32) * offsetInDwords)),
                                              dwordsNeeded,
                                              pCeCmdSpace);

    if (offsetInDwords < highWatermark)
    {
        // CE RAM now has a more up-to-date copy of the ring data than the GPU memory buffer does, so mark that the
        // data needs to be dumped into ring memory prior to the next Draw or Dispatch, provided that some portion of
        // the upload falls within the high watermark.
        pTable->contentsDirty = 1;
    }

    return pCeCmdSpace;
}

// =====================================================================================================================
// Helper function to dump the contents of a user-data table which is being managed by CE RAM. The constant engine will
// be used to dump the table contents into GPU memory. It is an error to call this before the table has been relocated
// to its new GPU memory location!
PAL_INLINE uint32* DumpUserDataTableCeRam(
    const CmdUtil&           cmdUtil,
    UniversalCmdBufferState* pState,
    UserDataTableState*      pTable,
    uint32                   offsetInDwords,
    uint32                   dwordsNeeded,
    uint32*                  pCeCmdSpace)
{
    PAL_ASSERT((dwordsNeeded + offsetInDwords) <= pTable->sizeInDwords);

    if (pState->flags.ceWaitOnDeCounterDiff)
    {
        pCeCmdSpace += cmdUtil.BuildWaitOnDeCounterDiff(pState->minCounterDiff, pCeCmdSpace);
        pState->flags.ceWaitOnDeCounterDiff = 0;
    }

    if (pState->flags.useIndirectAddrForCe)
    {
        // Dump CE RAM contents to an indirect memory offset. The calling command buffer will allocate
        // memory and set the base address appropriately.
        PAL_ASSERT(Util::HighPart(pTable->gpuVirtAddr) == 0);
        pCeCmdSpace += cmdUtil.BuildDumpConstRamOffset((Util::LowPart(pTable->gpuVirtAddr) +
                                                        (sizeof(uint32) * offsetInDwords)),
                                                       (pTable->ceRamOffset + (sizeof(uint32) * offsetInDwords)),
                                                       dwordsNeeded,
                                                       pCeCmdSpace);
    }
    else
    {
        pCeCmdSpace += cmdUtil.BuildDumpConstRam((pTable->gpuVirtAddr + (sizeof(uint32) * offsetInDwords)),
                                                 (pTable->ceRamOffset + (sizeof(uint32) * offsetInDwords)),
                                                 dwordsNeeded,
                                                 pCeCmdSpace);
    }

    // Mark that the CE data chunk in GPU memory is now fully up-to-date with CE RAM and that a CE RAM dump has
    // occurred since the previous Draw or Dispatch.
    pTable->contentsDirty       = 0;
    pState->flags.ceStreamDirty = 1;

    return pCeCmdSpace;
}

// =====================================================================================================================
// Helper function to pass the contents of a user-data table to a nested command buffer which needs to inherit the data
// contained in the user-data table from its caller. The callee command buffer expects the data in GPU memory.
PAL_INLINE uint32* PassInheritedUserDataTableGpuMem(
    const CmdUtil& cmdUtil,
    gpusize        gpuVirtAddr,
    uint32         offsetInDwords,
    uint32         dwordsNeeded,
    const uint32*  pSrcData,        // [in] Address of the real first dword of the table being inherited, not the first
                                    // dword being uploaded to the callee!
    uint32*        pDeCmdSpace)
{
    PAL_ASSERT((dwordsNeeded != 0) && (offsetInDwords != NoUserDataSpilling));

    pDeCmdSpace += cmdUtil.BuildWriteData(EngineTypeUniversal,
                                          (gpuVirtAddr + (sizeof(uint32) * offsetInDwords)),
                                          dwordsNeeded,
                                          engine_sel__pfp_write_data__prefetch_parser,
                                          dst_sel__pfp_write_data__memory,
                                          true,
                                          (pSrcData + offsetInDwords),
                                          PredDisable,
                                          pDeCmdSpace);

    return pDeCmdSpace;
}

// =====================================================================================================================
// Helper function to pass the contents of a user-data table to a nested command buffer which needs to inherit the data
// contained in the user-data table. The callee command buffer expects the data to be present in CE RAM.
PAL_INLINE uint32* PassInheritedUserDataTableCeRam(
    const CmdUtil&      cmdUtil,
    UserDataTableState* pTable,
    uint32              offsetInDwords,
    uint32              dwordsNeeded,
    const uint32*       pSrcData,        // [in] Address of the real first dword of the table being inherited, not the
                                         // first dword being uploaded to the callee!
    uint32*             pCeCmdSpace)
{
    PAL_ASSERT((offsetInDwords + dwordsNeeded) < pTable->sizeInDwords);
    PAL_ASSERT((dwordsNeeded != 0) && (offsetInDwords != NoUserDataSpilling));

    pCeCmdSpace += cmdUtil.BuildWriteConstRam((pSrcData + offsetInDwords),
                                              (pTable->ceRamOffset + (sizeof(uint32) * offsetInDwords)),
                                              dwordsNeeded,
                                              pCeCmdSpace);

    return pCeCmdSpace;
}

}
}
