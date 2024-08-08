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
    PAL_ASSERT(bitsPerComponent > 0);
    constexpr uint32 bitsPerDword   = sizeof(uint32) * 8;
    const uint32 componentsPerDword = bitsPerDword/bitsPerComponent;

    uint32 outCount = 0;

    for (uint32 i = 0; i < componentCount; i += componentsPerDword)
    {
        const uint32 remainingComponents = componentCount - i;

        for (const uint32* pIn : ppInputs)
        {
            if (pIn == nullptr)
            {
                continue;
            }

            ExecuteIndirectV2Packed packedDword {};
            packedDword.u32All = 0;

            const uint32 numComponents = Min(componentsPerDword, remainingComponents);
            if (bitsPerComponent == BitsGraphics)
            {
                for (uint32 k = 0; k < numComponents; k++)
                {
                    packedDword.graphicsRegs[k] = static_cast<uint8>(pIn[i + k]);
                }
            }
            else if (bitsPerComponent == BitsCompute)
            {
                for (uint32 k = 0; k < numComponents; k++)
                {
                    packedDword.computeRegs[k] = static_cast<uint16>(pIn[i + k]);
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
        // Final MemCpy has been done set nextIdx to the end of VB+SpillTable slot.
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
        m_metaData.copyInitSrcOffsets[*pInitCount] = currentStart * sizeof(uint32);
        m_metaData.copyInitDstOffsets[*pInitCount] = currentStart * sizeof(uint32);
        m_metaData.copyInitSizes[(*pInitCount)++]  = currentEnd - currentStart;
    }
    // currentEnd is going beyond unspilled but start had unspilled entries. Highly unlikely case.
    else if (currentEnd >= m_excludeEnd)
    {
        m_metaData.copyInitSrcOffsets[*pInitCount] = m_excludeEnd * sizeof(uint32);
        m_metaData.copyInitDstOffsets[*pInitCount] = m_excludeEnd * sizeof(uint32);
        m_metaData.copyInitSizes[(*pInitCount)++]  = currentEnd - m_excludeEnd;
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
    m_metaData.copyUpdateSrcOffsets[*pUpdateCount] = pEntry->argBufferOffset * sizeof(uint32);
    m_metaData.copyUpdateDstOffsets[*pUpdateCount] = *pNextIdx * sizeof(uint32);

    // Predict what the next srcIndex value might be, clipping if nesssary
    uint32 nextCpyChunkSize = ((*pCurrentIdx + pEntry->size) < vbSpillTableWatermark) ?
                              pEntry->size : (vbSpillTableWatermark - *pCurrentIdx);

    uint32 nextArgBufferOffset = pEntry->argBufferOffset + nextCpyChunkSize;
    *pCurrentIdx += nextCpyChunkSize;

    uint32 currentCpyChunkSize = nextCpyChunkSize;

    // Check if any next valid entries are remaining to be updated from the Look-up Table and get nextIdx.
    *pValidUpdate = NextUpdate(vbSpillTableWatermark, pNextIdx, pEntry);

    // If the prediction for next entry was correct continue adding next spilled entries to this struct.
    while (*pValidUpdate && (*pCurrentIdx == *pNextIdx))
    {
        nextCpyChunkSize     = ((*pCurrentIdx + pEntry->size) < vbSpillTableWatermark) ?
                               pEntry->size : (vbSpillTableWatermark - *pCurrentIdx);
        *pCurrentIdx        += nextCpyChunkSize;
        currentCpyChunkSize += nextCpyChunkSize;

        if (pEntry->argBufferOffset != nextArgBufferOffset)
        {
            break;
        }
        nextArgBufferOffset += nextCpyChunkSize;
        *pValidUpdate = NextUpdate(vbSpillTableWatermark, pNextIdx, pEntry);
    }
    m_metaData.copyUpdateSizes[(*pUpdateCount)++] = currentCpyChunkSize;
}

} // Gfx9
} // Pal
