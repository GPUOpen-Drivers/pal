/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/gfx9/gfx9CmdUtil.h"

using namespace Util;

namespace Pal
{
namespace Gfx9
{

// =====================================================================================================================
ExecuteIndirectV2Meta::ExecuteIndirectV2Meta()
    :
    m_metaData{},
    m_excludeStart(0),
    m_excludeEnd(0),
    m_computeMemCopiesLut{ 0 },
    m_computeMemCopiesLutFlags{ 0 }
{
}

// =====================================================================================================================
uint32 ExecuteIndirectV2Meta::ExecuteIndirectV2WritePacked(
    uint32*       pOut,
    const uint32  bitsPerComponent,
    const uint32  componentCount,
    const uint32* pIn1,
    const uint32* pIn2,
    const uint32* pIn3)
{
    const uint32* ppInputs[] = {pIn1, pIn2, pIn3};

    // bitsPerComponent can be either 8 for Graphics or 16 for Compute.
    PAL_ASSERT_MSG(((bitsPerComponent > 0) && IsPowerOfTwo(bitsPerComponent)),
                   "bitsPerComponent incorrect/unsupported.");
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

            ExecuteIndirectV2Packed packedDword {};
            static_assert(sizeof(ExecuteIndirectV2Packed) == sizeof(uint32),
                          "ExecuteIndirectV2Packed is not DWORD sized.");

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
bool ExecuteIndirectV2Meta::NextUpdate(
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
void ExecuteIndirectV2Meta::ComputeMemCopyStructures(
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
void ExecuteIndirectV2Meta::ComputeVbSrdInitMemCopy(
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
            PAL_ASSERT(pCopy->count <= EiV2MemCopySlots);
            newCopy = true;
        }
    }

    // Enclose the last issued copy
    PAL_ASSERT(newCopy == false);
    pCopy->count++;
    PAL_ASSERT(pCopy->count <= EiV2MemCopySlots);
}

// =====================================================================================================================
void ExecuteIndirectV2Meta::ProcessInitMemCopy(
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
void ExecuteIndirectV2Meta::ProcessUpdateMemCopy(
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

        *pCurrentIdx        += nextCpyChunkSize;
        currentCpyChunkSize += nextCpyChunkSize;

        if (pEntry->argBufferOffset != nextArgBufferOffset)
        {
            break;
        }
        nextArgBufferOffset += nextCpyChunkSize;

        // Check if any next valid entries are remaining to be updated from the Look-up Table and get nextIdx.
        *pValidUpdate = NextUpdate(vbSpillTableWatermark, pNextIdx, pEntry);

    } while (*pValidUpdate && (*pCurrentIdx == *pNextIdx));

    m_metaData.updateMemCopy.sizes[(*pUpdateCount)++] = currentCpyChunkSize;
}

} // Gfx9
} // Pal
