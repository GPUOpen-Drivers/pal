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
    const uint32  countInDwords,
    const uint32* pIn1,
    const uint32* pIn2,
    const uint32* pIn3)
{
    const uint32* ppInputs[] = {pIn1, pIn2, pIn3};

    // bitsPerComponent can be either 8 for Graphics or 16 for Compute.
    PAL_ASSERT(bitsPerComponent > 0);
    constexpr uint32 bitsPerDword = sizeof(uint32)*8;
    const uint32 componentsPerDword = bitsPerDword/bitsPerComponent;

    uint32 outCount = 0;

    for (uint32 i = 0; i < countInDwords; i += componentsPerDword)
    {
        const uint32 numLeft = countInDwords - i;

        for (uint32 j = 0; j < ArrayLen32(ppInputs) && (ppInputs[j] != nullptr); j++)
        {
            const uint32* pIn = ppInputs[j];

            ExecuteIndirectV2Packed packedDword {};
            packedDword.u32All = 0;

            const uint32 num = Min(componentsPerDword, numLeft);
            switch (bitsPerComponent)
            {
            case BitsGraphics:
                for (uint32 k = 0; k < num; k++)
                {
                    packedDword.graphicsRegs[k] = static_cast<uint8>(pIn[i + k]);
                }
                break;
            case BitsCompute:
                for (uint32 k = 0; k < num; k++)
                {
                    packedDword.computeRegs[k] = static_cast<uint16>(pIn[i + k]);
                }
                break;
            default:
                break;
            }
            pOut[outCount++] = packedDword.u32All;
        }
    }
    return outCount;
}

// =====================================================================================================================
bool ExecuteIndirectV2Meta::NextUpdate(
    const uint32         vbSpillTableWatermark,
    uint32*              nextIdx,
    DynamicMemCopyEntry* entry)
{
    bool nextMemCpyValid = WideBitMaskScanForward(nextIdx, m_computeMemCopiesLutFlags);
    if (nextMemCpyValid)
    {
        ClearLut(*nextIdx);
        *entry = m_computeMemCopiesLut[*nextIdx];
    }
    else
    {
        // Final MemCpy has been done set nextIdx to the end of VB+SpillTable slot.
        *nextIdx = vbSpillTableWatermark;
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

    while ((validUpdate == true) && (currentIdx < vbSpillTableWatermark))
    {
        // Needs an InitMemCpy Struct. Set it up.
        if (nextIdx != currentIdx)
        {
            uint32 nextCpyChunkSize = nextIdx - currentIdx;
            // Check that the chunk to be copied isn't extending past the watermark and if it is limit it upto the
            // watermark.
            nextCpyChunkSize = (currentIdx + nextCpyChunkSize) < vbSpillTableWatermark ?
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
            currentIdx = currentEnd;
        }
        else
        {
            // Already has an InitMemCpy struct. So get started with the UpdateMemCpy struct or it's a case like
            // DispatchRays where there is no VBTable and all UserDataEntries are force spilled. So it starts here after
            // which logic loops back to set up the InitStruct/s.
            m_metaData.copyUpdateSrcOffsets[*pUpdateCount] = entry.argBufferOffset * sizeof(uint32);
            m_metaData.copyUpdateDstOffsets[*pUpdateCount] = nextIdx               * sizeof(uint32);

            // Predict what the next srcIndex value might be, clipping if nesssary
            uint32 nextCpyChunkSize = (currentIdx + entry.size) < vbSpillTableWatermark ?
                                       entry.size : (vbSpillTableWatermark - currentIdx);

            uint32 nextArgBufferOffset = entry.argBufferOffset + nextCpyChunkSize;
            currentIdx += nextCpyChunkSize;

            // Check if any next valid entries are remaining to be updated from the Look-up Table and get nextIdx.
            validUpdate = NextUpdate(vbSpillTableWatermark, &nextIdx, &entry);

            uint32 currentCpyChunkSize = nextCpyChunkSize;

            // If the prediction for next entry was correct continue adding next spilled entries to this struct.
            while (validUpdate && (currentIdx == nextIdx))
            {
                nextCpyChunkSize     = (currentIdx + entry.size) < vbSpillTableWatermark ?
                                        entry.size : (vbSpillTableWatermark - currentIdx);
                currentIdx          += nextCpyChunkSize;
                currentCpyChunkSize += nextCpyChunkSize;

                if (entry.argBufferOffset != nextArgBufferOffset)
                {
                    break;
                }
                nextArgBufferOffset += nextCpyChunkSize;
                validUpdate = NextUpdate(vbSpillTableWatermark, &nextIdx, &entry);
            }

            // End of UpdateMemCopy struct
            m_metaData.copyUpdateSizes[(*pUpdateCount)++] = currentCpyChunkSize;
        }
    }
    if ((*pUpdateCount == 0) && (*pInitCount != 0))
    {
        PAL_ASSERT_ALWAYS_MSG("ExecuteIndirectV2 PM4 will have an InitMemCpy struct but no UpdateMemCpy struct. This \
                               indicates a problem with the UserData spilled entries.");
    }
}

} // Gfx9
} // Pal
