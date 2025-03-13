/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2024-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/gfx12/gfx12CmdUtil.h"

using namespace Util;

namespace Pal
{
namespace Gfx12
{

// =====================================================================================================================
ExecuteIndirectMeta::ExecuteIndirectMeta()
    :
    m_metaData{},
    m_op{},
    m_excludeStart(0),
    m_excludeEnd(0),
    m_computeMemCopiesLut{ 0 },
    m_computeMemCopiesLutFlags{ 0 }
{
}

// =====================================================================================================================
uint32 ExecuteIndirectMeta::ExecuteIndirectWritePacked(
    uint32*       pOut,
    const uint32  bitsPerComponent,
    const uint32  componentCount,
    const uint32* pIn1,
    const uint32* pIn2,
    const uint32* pIn3)
{
    const uint32* ppInputs[] = {pIn1, pIn2, pIn3};

    PAL_ASSERT_MSG(((bitsPerComponent > 0) &&
                    IsPowerOfTwo(bitsPerComponent)),"bitsPerComponent incorrect/unsupported.");
    constexpr uint32 BitsPerDword   = sizeof(uint32) * 8;
    const uint32 componentsPerDword = BitsPerDword / bitsPerComponent;

    uint32 outCount = 0;

    // This loop increments by componentsPerDword (2 or 4).
    for (uint32 componentIdx = 0; componentIdx < componentCount; componentIdx += componentsPerDword)
    {
        const uint32 remainingComponents = componentCount - componentIdx;

        for (const uint32* pIn : ppInputs)
        {
            if (pIn == nullptr)
            {
                continue;
            }

            ExecuteIndirectPacked packedDword {};
            static_assert(sizeof(ExecuteIndirectPacked) == sizeof(uint32),"ExecuteIndirectPacked is not DWORD sized.");

            const uint32 numPackedComponents = Min(componentsPerDword, remainingComponents);

            for (uint32 packedIdx = 0; packedIdx < numPackedComponents; packedIdx++)
            {
                uint32 originalComponent = pIn[componentIdx + packedIdx];
                if (bitsPerComponent == 8)
                {
                    packedDword.u8bitComponents[packedIdx] = static_cast<uint8>(originalComponent);
                }
                else if (bitsPerComponent == 16)
                {
                    packedDword.u16bitComponents[packedIdx] = static_cast<uint16>(originalComponent);
                }
            }
            pOut[outCount++] = packedDword.u32All;
        }
    }
    return outCount;
}

// =====================================================================================================================
uint32 ExecuteIndirectMeta::AppendUserDataMec(
    uint32*       pPackedUserData,
    const uint32  userDataCount,
    const uint32* pUserData)
{
    // Struct for MEC UserDataReg format.
    union MecPackedDword
    {
        struct
        {
            uint16 numRegisters;
            uint16 startRegOffset;
        };
        uint32 u32All;
    };

    uint32 packedIdx = 0; // idx for output/pPackedUserData.

    // Initialize pPackedUserData[packedIdx == 0] from pUserData[0].
    MecPackedDword packedDword = { {.numRegisters = 1, .startRegOffset = static_cast<uint16>(pUserData[0])} };
    pPackedUserData[packedIdx] = packedDword.u32All;

    // If we have more than 1 UserData entries here, check whether UserDataRegOffsets are contiguous or create new
    // entries for the pPackedUserData output array.
    for (uint32 userDataIdx = 1; userDataIdx < userDataCount; userDataIdx++)
    {
        // If current UserDataRegOffset is right after the previous UserDataRegOffset, RegOffsets are contiguous and we
        // only increment register count as startRegOffset remains the same.
        if (CheckSequential( { pUserData[userDataIdx - 1], pUserData[userDataIdx] } ))
        {
            packedDword.numRegisters++;
        }
        // Else overwrite packedDword for the next entry in pOut.
        else
        {
            packedDword.numRegisters = 1;
            packedDword.startRegOffset = static_cast<uint16>(pUserData[userDataIdx]);
            // We only increment packedIdx when a new entry is added to the output array.
            packedIdx++;
        }
        // If we only update numRegisters then we overwrite the previous packedIdx's entry here, else we write a new
        // packedIdx entry.
        pPackedUserData[packedIdx] = packedDword.u32All;
    }
    // This'll represent the count of dwords that will be appended to base PM4 for the UserData read/copy OP.
    return (packedIdx + 1);
}

// =====================================================================================================================
bool ExecuteIndirectMeta::NextUpdate(
    const uint32         vbSpillTableWatermark,
    uint32*              pNextIdx,
    DynamicMemCopyEntry* pEntry)
{
    bool nextMemCpyValid = WideBitMaskScanForward(pNextIdx, m_computeMemCopiesLutFlags);
    if (nextMemCpyValid)
    {
        ClearLut(*pNextIdx);
        *pEntry = m_computeMemCopiesLut[*pNextIdx];
    }
    else
    {
        // Final MemCpy has been done set nextIdx to the end of VB+SpillTable slot i.e. last entry to be updated.
        *pNextIdx = vbSpillTableWatermark;
    }
    return nextMemCpyValid;
}

// =====================================================================================================================
void ExecuteIndirectMeta::ComputeMemCopyStructures(
    const uint32 vbSpillTableWatermark,
    uint32*      pInitCount,
    uint32*      pUpdateCount)
{
    uint32 currentIdx = 0;
    uint32 nextIdx    = 0;
    DynamicMemCopyEntry entry = {};

    bool validUpdate = NextUpdate(vbSpillTableWatermark, &nextIdx, &entry);

    while (validUpdate && (currentIdx < vbSpillTableWatermark))
    {
        // Needs an InitMemCpy Struct. Set it up.
        if (nextIdx != currentIdx)
        {
            ProcessInitMemCopy(vbSpillTableWatermark, pInitCount, currentIdx, nextIdx);
            currentIdx = nextIdx;
        }
        else
        {
            // Already has an InitMemCpy struct. So get started with the UpdateMemCpy struct or it's a case like
            // DispatchRays where there is no VBTable and all UserDataEntries are force spilled so, it starts here after
            // which logic loops back to set up the InitStruct/s.
            ProcessUpdateMemCopy(vbSpillTableWatermark, pUpdateCount, &currentIdx, &nextIdx, &entry, &validUpdate);
        }
    }

    if ((*pUpdateCount == 0) && (*pInitCount != 0))
    {
        // Force InitMemCpyCount to be 0 because CP will use driver provided CmdAllocEmbeddedData version of UserData
        // and does not need Global Spill Table. This is so that we don't end up allocating the Global Spill Table and
        // reduce some operations in the driver.
        *pInitCount = 0;
    }
}

// =====================================================================================================================
void ExecuteIndirectMeta::ComputeVbSrdInitMemCopy(
   uint32 vbSlotMask)
{
    PAL_ASSERT(vbSlotMask != 0);

    uint32 startIdx = 0;
    bool ret = BitMaskScanForward(&startIdx, vbSlotMask);
    PAL_ASSERT(ret);

    CpMemCopy* pCopy   = &m_metaData.initMemCopy;
    bool       newCopy = true;

    for (uint32 idx = startIdx; vbSlotMask != 0; idx++)
    {
        if (TestAnyFlagSet(vbSlotMask, 1u << idx))
        {
            if (newCopy)
            {
                pCopy->srcOffsets[pCopy->count] = startIdx * DwordsPerBufferSrd * sizeof(uint32); // in bytes
                pCopy->dstOffsets[pCopy->count] = startIdx * DwordsPerBufferSrd * sizeof(uint32); // in bytes
                pCopy->sizes[pCopy->count]      = DwordsPerBufferSrd; // in dwords
                newCopy = false;
            }
            else
            {
                pCopy->sizes[pCopy->count] += DwordsPerBufferSrd; // in dwords
            }

            vbSlotMask &= ~(1u << idx);
        }
        else
        {
            pCopy->count++;
            PAL_ASSERT(pCopy->count <= EiMemCopySlots);
            newCopy = true;
        }
    }

    // Enclose the last issued copy
    PAL_ASSERT(newCopy == false);
    pCopy->count++;
    PAL_ASSERT(pCopy->count <= EiMemCopySlots);
}

// =====================================================================================================================
void ExecuteIndirectMeta::ProcessInitMemCopy(
    const uint32 vbSpillTableWatermark,
    uint32*      pInitCount,
    uint32       currentIdx,
    uint32       nextIdx)
{
    uint32 nextCpyChunkSize = nextIdx - currentIdx;
    // Check that the chunk to be copied isn't extending past the watermark and if it is limit it upto the
    // watermark.
    nextCpyChunkSize = ((currentIdx + nextCpyChunkSize) < vbSpillTableWatermark) ?
                        nextCpyChunkSize : (vbSpillTableWatermark - currentIdx);

    uint32 currentStart = currentIdx;
    uint32 currentEnd   = currentIdx + nextCpyChunkSize;

    // Slots between m_excludeStart and m_excludeEnd are supposed to be reserved for unspilled UserData entries.
    if (currentStart >= m_excludeEnd) [[likely]]
    {
        // Copy in one chunk
        m_metaData.initMemCopy.srcOffsets[*pInitCount] = currentStart * sizeof(uint32);
        m_metaData.initMemCopy.dstOffsets[*pInitCount] = currentStart * sizeof(uint32);
        m_metaData.initMemCopy.sizes[(*pInitCount)++]  = currentEnd - currentStart;
    }
    // currentEnd is going beyond unspilled but start had unspilled entries. Highly unlikely case.
    else if (currentEnd >= m_excludeEnd)
    {
        m_metaData.initMemCopy.srcOffsets[*pInitCount] = m_excludeEnd * sizeof(uint32);
        m_metaData.initMemCopy.dstOffsets[*pInitCount] = m_excludeEnd * sizeof(uint32);
        m_metaData.initMemCopy.sizes[(*pInitCount)++]  = currentEnd - m_excludeEnd;
    }
}

// =====================================================================================================================
void ExecuteIndirectMeta::ProcessUpdateMemCopy(
    const uint32         vbSpillTableWatermark,
    uint32*              pUpdateCount,
    uint32*              pCurrentIdx,
    uint32*              pNextIdx,
    DynamicMemCopyEntry* pEntry,
    bool*                pValidUpdate)
{
    m_metaData.updateMemCopy.srcOffsets[*pUpdateCount] = pEntry->argBufferOffset * sizeof(uint32);
    m_metaData.updateMemCopy.dstOffsets[*pUpdateCount] = *pNextIdx * sizeof(uint32);

    uint32 currentCpyChunkSize = 0;
    uint32 nextArgBufferOffset = pEntry->argBufferOffset;

    do
    {
        // Predict what the next srcIndex value might be, clipping if necessary and if the prediction for next entry
        // was correct continue adding next spilled entries to this struct.
        uint32 nextCpyChunkSize = ((*pCurrentIdx + pEntry->size) < vbSpillTableWatermark) ?
                                   pEntry->size : (vbSpillTableWatermark - *pCurrentIdx);

        // Only integrate next entry into the same CpMemCopy when argBufferOffset is contiguous. Note that current
        // index shouldn't be updated before continuity check here.
        if (pEntry->argBufferOffset != nextArgBufferOffset)
        {
            break;
        }

        *pCurrentIdx        += nextCpyChunkSize;
        currentCpyChunkSize += nextCpyChunkSize;
        nextArgBufferOffset += nextCpyChunkSize;

        // Check if any next valid entries are remaining to be updated from the Look-up Table and get nextIdx.
        *pValidUpdate = NextUpdate(vbSpillTableWatermark, pNextIdx, pEntry);

    } while (*pValidUpdate && (*pCurrentIdx == *pNextIdx));

    // Conclude copy chunk size when we either encounter discontinuity or complete full range iteration.
    m_metaData.updateMemCopy.sizes[(*pUpdateCount)++] = currentCpyChunkSize;
}

// =====================================================================================================================
uint16 ExecuteIndirectMeta::ProcessCommandIndex(
    const uint8 drawIndexRegOffset,
    const bool  useConstantDrawIndex,
    const bool  useEightBitMask)
{
    uint16 commandIndex = 0;
    constexpr uint32 EightBitMask = 0xFF;

    const bool incConstRegMapped  = (m_metaData.incConstRegCount > 0);
    const bool drawIndexRegMapped = (drawIndexRegOffset != UserDataNotMapped) && (useConstantDrawIndex == false);
    PAL_ASSERT((incConstRegMapped && drawIndexRegMapped) == false);

    if (incConstRegMapped)
    {
        if (m_metaData.incConstRegCount > 1)
        {
            PAL_NOT_IMPLEMENTED();
        }

        m_metaData.commandIndexEnable = true;
        commandIndex = (useEightBitMask ? m_metaData.incConstReg[0] & EightBitMask : m_metaData.incConstReg[0]);
    }
    else if (drawIndexRegMapped)
    {
        m_metaData.commandIndexEnable = true;
        commandIndex = drawIndexRegOffset;
    }
    else
    {
        // None of them are in use.
        m_metaData.commandIndexEnable = false;
    }

    return commandIndex;
}

} // Gfx12
} // Pal
