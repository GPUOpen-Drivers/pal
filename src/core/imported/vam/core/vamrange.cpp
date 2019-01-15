/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2009-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

/**
***************************************************************************************************
* @file  vamrange.cpp
* @brief Contains the VamVaRange base class implementation.
***************************************************************************************************
*/

#include "vamrange.h"

VAM_RETURNCODE VamVARange::AllocateVASpace(
    VAM_VA_SIZE         sizeInBytes,
    VAM_VA_SIZE         alignment,
    VAM_ALLOCATION&     allocation)
{
    VAM_RETURNCODE  ret = VAM_OUTOFMEMORY;
    VAM_VA_SIZE     remainder, adjustment;
    VamChunk*       pExtraChunk;

    if (!sizeInBytes)
    {
        // Disallow allocations of zero size
        return VAM_INVALIDPARAMETERS;
    }

    // Iterate through all chunks, looking for first one that's big enough
    for (ChunkList::Iterator chunk( chunkList() );
         chunk != NULL;
         chunk++ )
    {
        if (sizeInBytes <= chunk->m_size)
        {
            // This chunk is a possible candidate, provided
            // that the alignment requirement is met.
            remainder = chunk->m_addr % alignment;
            if (remainder == 0)
            {
                // Both size and alignment are OK at the start of the chunk.
                // Adjust the chunk's parameters and exit with success.
                allocation.address = chunk->m_addr;
                allocation.size    = sizeInBytes;
                chunk->m_addr     += sizeInBytes;
                chunk->m_size     -= sizeInBytes;
                if (!chunk->m_size)
                {
                    // The allocation has the exact size as the chunk.
                    chunkList().remove(chunk);

                    if (m_treeEnabled)
                    {
                        chunkTree().remove(chunk);
                    }
                    FreeChunk(chunk);
                }
                ret = VAM_OK;
                break;
            }
            else
            {
                // See if the chunk's size is large enough to achieve the req'd alignment
                adjustment = alignment - remainder;
                if ((sizeInBytes + adjustment) <= chunk->m_size)
                {
                    // If the aligned allocation is smaller than the remainder of the chunk,
                    // we'll need to create a new chunk to the right of the allocation.
                    if ((sizeInBytes + adjustment) < chunk->m_size)
                    {
                        // Split what remains of the chunk. We need to create
                        // an extra chunk to reflect the remaining free space.
                        pExtraChunk = AllocChunk();
                        if (pExtraChunk != NULL)
                        {
                            // Reflect the extra chunk's properties and add it to the list
                            pExtraChunk->m_addr = chunk->m_addr + adjustment + sizeInBytes;
                            pExtraChunk->m_size = chunk->m_size - (adjustment + sizeInBytes);
                            chunkList().insertAfter(chunk, pExtraChunk);

                            if (m_treeEnabled)
                            {
                                chunkTree().insert(pExtraChunk);
                            }

                            // Adjust the existing chunk to the left of the allocation.
                            // Note that its starting address remains unaltered.
                            chunk->m_size       = adjustment;

                            allocation.address  = chunk->m_addr + adjustment;
                            allocation.size     = sizeInBytes;
                            ret = VAM_OK;
                        }
                    }
                    else
                    {
                        // Allocation fits completely in the rest of the existing chunk.
                        // Adjust the chunk's size only, since its address will remain as is.
                        allocation.address = chunk->m_addr + adjustment;
                        allocation.size    = sizeInBytes;
                        chunk->m_size      = adjustment;
                        ret = VAM_OK;
                    }
                    break;
                }
            }
        }
    }

    if (ret == VAM_OK)
    {
        decFreeSize(allocation.size);
    }

    return ret;
}

VAM_RETURNCODE VamVARange::AllocateVASpaceWithAddress(
    VAM_VIRTUAL_ADDRESS  requestedVA,
    VAM_VA_SIZE          sizeInBytes,
    VAM_ALLOCATION&      allocation,
    bool                 beyondBaseVA)
{
    VAM_RETURNCODE      ret = VAM_VIRTUALADDRESSCONFLICT;
    VAM_VIRTUAL_ADDRESS startVA, endVA, offsetVA;
    VAM_VA_SIZE         adjustedSize;
    VamChunk*           pExtraChunk;

    if (!sizeInBytes)
    {
        // Disallow allocations of zero size.
        return VAM_INVALIDPARAMETERS;
    }

    // Adjust the specified VA and size, so that the allocation is made
    // in line with the VA space's alignment granularity requirements.
    startVA      = ROUND_DOWN(requestedVA, (long long) alignmentGranularity());
    endVA        = ROUND_UP(requestedVA + sizeInBytes, (long long) alignmentGranularity()) - 1;
    adjustedSize = endVA - startVA + 1;

    // Iterate through all chunks, looking for the one that's applicable
    for (ChunkList::Iterator chunk( chunkList() );
          chunk != NULL;
          chunk++ )
    {
        // Find the following address after requested VA base
        // note: chunks are located in the range order, if (startVA < chunk->m_addr),
        // then we passed the original startVA for >= already
        if (beyondBaseVA &&
            (startVA < chunk->m_addr) &&
            (adjustedSize <= chunk->m_size))
        {
            // chunk->m_addr should be already aligned to alignmentGranularity()
            startVA = chunk->m_addr;
            endVA   = ROUND_UP(startVA + sizeInBytes, (long long) alignmentGranularity()) - 1;
            adjustedSize = endVA - startVA + 1;
        }

        // Check if the requested allocation is within this chunk's range
        if ((startVA >= chunk->m_addr) &&
            (endVA   <= (chunk->m_addr + chunk->m_size)))
        {
            // This chunk is good. Check to see if we need
            // a chunk on the left side of the allocation.
            offsetVA = startVA - chunk->m_addr;
            if (offsetVA == 0)
            {
                // There will be no chunk to the left. Adjust the
                // existing chunk's parameters and exit with success.
                allocation.address = startVA;
                allocation.size    = adjustedSize;
                chunk->m_addr     += adjustedSize;
                chunk->m_size     -= adjustedSize;
                if (!chunk->m_size)
                {
                    // The allocation has the exact size as the chunk.
                    // This chunk will not be needed, so get rid of it.
                    chunkList().remove(chunk);

                    if (m_treeEnabled)
                    {
                        chunkTree().remove(chunk);
                    }
                    FreeChunk(chunk);
                }
                ret = VAM_OK;
                break;
            }
            else
            {
                // If the aligned allocation is smaller than the remainder of the chunk,
                // we'll need to create a new chunk to the right of the allocation.
                if ((offsetVA + adjustedSize) < chunk->m_size)
                {
                    // Split what remains of the chunk. We need to create an extra chunk
                    // to the right of the allocation to reflect the remaining free space.
                    pExtraChunk = AllocChunk();
                    if (pExtraChunk != NULL)
                    {
                        // Reflect the extra chunk's properties and add it to the list
                        pExtraChunk->m_addr = endVA + 1;
                        pExtraChunk->m_size = chunk->m_size - (offsetVA + adjustedSize);
                        chunkList().insertAfter(chunk, pExtraChunk);

                        if (m_treeEnabled)
                        {
                            chunkTree().insert(pExtraChunk);
                        }

                        // Adjust the existing chunk to the left of the allocation.
                        // Note that its starting address remains unaltered.
                        chunk->m_size       = offsetVA;

                        allocation.address  = startVA;
                        allocation.size     = adjustedSize;
                        ret = VAM_OK;
                    }
                }
                else
                {
                    // Allocation fits completely in the rest of the existing chunk.
                    // Adjust the chunk's size only, since its address will remain as is.
                    allocation.address = startVA;
                    allocation.size    = adjustedSize;
                    chunk->m_size      = offsetVA;
                    ret = VAM_OK;
                }
                break;
            }
        }
    }

    if (ret == VAM_OK)
    {
        decFreeSize(allocation.size);
    }
    else
    {
        if (chunkList().isEmpty())
        {
            ret = VAM_OUTOFMEMORY;
        }
    }

    return ret;
}

VAM_RETURNCODE VamVARange::FreeVASpace(
    VAM_VIRTUAL_ADDRESS  virtualAddress,
    VAM_VA_SIZE          actualSize)
{
    VAM_RETURNCODE ret = VAM_OK;

    if (m_treeEnabled)
    {
        ret = FreeVASpaceWithTreeEnabled(virtualAddress, actualSize);
    }
    else
    {
        ret = FreeVASpaceWithTreeDisabled(virtualAddress, actualSize);

        const unsigned int threshold = 256;

        // When number of chunks in the list is greater than our threshold,
        // build chunk tree to optimzie VA free performance.
        if (chunkList().numObjects() >= threshold)
        {
            for (ChunkList::Iterator chunk(chunkList());
                 chunk != NULL;
                 chunk++)
            {
                chunkTree().insert(chunk);
            }

            m_treeEnabled = true;
        }
    }

    return ret;
}

VAM_RETURNCODE VamVARange::FreeVASpaceWithTreeDisabled(
    VAM_VIRTUAL_ADDRESS  virtualAddress,
    VAM_VA_SIZE          actualSize)
{
    VamChunk*           pNewChunk = NULL;
    VAM_RETURNCODE      ret = VAM_OK;
    VAM_VIRTUAL_ADDRESS adjustedVA;
    VAM_VA_SIZE         adjustedSize;

    VamChunk*           pChunkL = NULL;
    VamChunk*           pChunkR = NULL;

    bool                freedRangeValid = true;

    if (!actualSize)
    {
        // Disallow freeing of allocations of zero size
        return VAM_INVALIDPARAMETERS;
    }

    // Make sure the range to be freed in within the VA space bounds
    if (!IsVAInsideRange(virtualAddress) || !IsVAInsideRange(virtualAddress + actualSize - 1))
    {
        return VAM_INVALIDPARAMETERS;
    }

    // Adjust the specified VA and size, so that the allocation is made
    // in line with the VA space's alignment granularity requirements.
    adjustedVA   = ROUND_DOWN(virtualAddress, (long long) alignmentGranularity());
    adjustedSize = ROUND_UP(actualSize, (long long) alignmentGranularity());

    // If the chunk list has two or more entries, walk the list from the start if
    // the freed address is in the first half. Otherwise, walk it from the end.
    if (chunkList().numObjects() > 1)
    {
        if (adjustedVA < ((chunkList().first()->m_addr + chunkList().last()->m_addr) / 2))
        {
            // Search from front to back
            for (ChunkList::Iterator chunk( chunkList() );
                  chunk != NULL;
                  chunk++ )
            {
                if (IsVASpaceInsideChunk(adjustedVA, adjustedSize, chunk))
                {
                    freedRangeValid = false;
                    break;
                }

                if (adjustedVA < chunk->m_addr)
                {
                    VAM_ASSERT(chunk->m_addr >= (adjustedVA + adjustedSize));
                    pChunkR = chunk;
                    pChunkL = chunk->prev();
                    break;
                }
            }
        }
        else
        {
            // Search from back to front
            for (ChunkList::ReverseIterator chunk( chunkList() );
                  chunk != NULL;
                  chunk-- )
            {
                if (IsVASpaceInsideChunk(adjustedVA, adjustedSize, chunk))
                {
                    freedRangeValid = false;
                    break;
                }

                if (adjustedVA > chunk->m_addr)
                {
                    VAM_ASSERT(chunk->m_addr + chunk->m_size <= adjustedVA);
                    pChunkL = chunk;
                    pChunkR = chunk->next();
                    break;
                }
            }
        }
    }
    else if (chunkList().numObjects() == 1)
    {
        if (IsVASpaceInsideChunk(adjustedVA, adjustedSize, chunkList().first()))
        {
            freedRangeValid = false;
        }

        if (adjustedVA < chunkList().first()->m_addr)
        {
            VAM_ASSERT(chunkList().first()->m_addr >= (adjustedVA + adjustedSize));
            pChunkR = chunkList().first();
        }
        else
        {
            VAM_ASSERT(chunkList().first()->m_addr + chunkList().first()->m_size <= adjustedVA);
            pChunkL = chunkList().first();
        }
    }

    // If the specified VA range to be freed is inside any of the existing
    //  chunks, treat this as an error and return with failure.
    if (!freedRangeValid)
    {
        return VAM_INVALIDPARAMETERS;
    }

    // Freed area may be adjacent to existing chunk(s). Let's try to coalesce.
    if (pChunkL && pChunkL->m_addr + pChunkL->m_size == adjustedVA)
    {
        pChunkL->m_size += adjustedSize;

        if (pChunkR)
        {
            if (pChunkL->m_addr + pChunkL->m_size == pChunkR->m_addr)
            {
                pChunkL->m_size += pChunkR->m_size;
                chunkList().remove(pChunkR);
                FreeChunk(pChunkR);
            }
        }
    }
    else if (pChunkR && (adjustedVA + adjustedSize == pChunkR->m_addr))
    {
        pChunkR->m_addr -= adjustedSize;
        pChunkR->m_size += adjustedSize;
    }
    else
    {
        pNewChunk = AllocChunk();
        if (pNewChunk != NULL)
        {
            // Add a new chunk entry.
            pNewChunk->m_addr = adjustedVA;
            pNewChunk->m_size = adjustedSize;

            // Ensure that the chunk list is kept ordered
            if (pChunkR)
            {
                chunkList().insertBefore(pChunkR, pNewChunk);
            }
            else
            {
                chunkList().insertLast(pNewChunk);
            }
        }
        else
        {
            VAM_ASSERT_ALWAYS();
            ret = VAM_OUTOFMEMORY;
        }
    }

    if (ret == VAM_OK)
    {
        incFreeSize(adjustedSize);
    }

    return ret;
}

VAM_RETURNCODE VamVARange::FreeVASpaceWithTreeEnabled(
    VAM_VIRTUAL_ADDRESS  virtualAddress,
    VAM_VA_SIZE          actualSize)
{
    VamChunk*           pNewChunk = NULL;
    VAM_RETURNCODE      ret = VAM_OK;
    VAM_VIRTUAL_ADDRESS adjustedVA;
    VAM_VA_SIZE         adjustedSize;

    VamChunk*           pChunkL = NULL;
    VamChunk*           pChunkR = NULL;

    if (!actualSize)
    {
        // Disallow freeing of allocations of zero size
        return VAM_INVALIDPARAMETERS;
    }

    // Make sure the range to be freed in within the VA space bounds
    if (!IsVAInsideRange(virtualAddress) || !IsVAInsideRange(virtualAddress + actualSize - 1))
    {
        return VAM_INVALIDPARAMETERS;
    }

    // Adjust the specified VA and size, so that the allocation is made
    // in line with the VA space's alignment granularity requirements.
    adjustedVA   = ROUND_DOWN(virtualAddress, (long long) alignmentGranularity());
    adjustedSize = ROUND_UP(actualSize, (long long) alignmentGranularity());

    chunkTree().findContainingNodes(adjustedVA, &pChunkL, &pChunkR);

    if (pChunkL && IsVASpaceInsideChunk(adjustedVA, adjustedSize, pChunkL))
    {
        return VAM_INVALIDPARAMETERS;
    }
    else if (pChunkR && IsVASpaceInsideChunk(adjustedVA, adjustedSize, pChunkR))
    {
        return VAM_INVALIDPARAMETERS;
    }

    // Freed area may be adjacent to existing chunk(s). Let's try to coalesce.
    if (pChunkL && pChunkL->m_addr + pChunkL->m_size == adjustedVA)
    {
        pChunkL->m_size += adjustedSize;

        if (pChunkR)
        {
            if (pChunkL->m_addr + pChunkL->m_size == pChunkR->m_addr)
            {
                pChunkL->m_size += pChunkR->m_size;
                chunkList().remove(pChunkR);
                chunkTree().remove(pChunkR);
                FreeChunk(pChunkR);
            }
        }
    }
    else if (pChunkR && (adjustedVA + adjustedSize == pChunkR->m_addr))
    {
        pChunkR->m_addr -= adjustedSize;
        pChunkR->m_size += adjustedSize;
    }
    else
    {
        pNewChunk = AllocChunk();
        if (pNewChunk != NULL)
        {
            // Add a new chunk entry.
            pNewChunk->m_addr = adjustedVA;
            pNewChunk->m_size = adjustedSize;

            // Ensure that the chunk list is kept ordered
            if (pChunkR)
            {
                chunkList().insertBefore(pChunkR, pNewChunk);
            }
            else
            {
                chunkList().insertLast(pNewChunk);
            }

            chunkTree().insert(pNewChunk);
        }
        else
        {
            VAM_ASSERT_ALWAYS();
            ret = VAM_OUTOFMEMORY;
        }
    }

    if (ret == VAM_OK)
    {
        incFreeSize(adjustedSize);
    }

    return ret;
}

bool VamVARange::IsVASpaceInsideChunk(
    VAM_VIRTUAL_ADDRESS vaStart,
    VAM_VA_SIZE         vaSize,
    VamChunk*           pChunk)
{
    if ((vaStart >= pChunk->m_addr) && (vaStart + vaSize) <= (pChunk->m_addr + pChunk->m_size))
    {
        return true;
    }
    else
    {
        return false;
    }
}

VAM_RETURNCODE VamVARange::Init(
    VAM_VIRTUAL_ADDRESS     addr,
    VAM_VA_SIZE             size,
    UINT                    aligmtGranularity)
{
    VAM_RETURNCODE  ret = VAM_ERROR;
    VamChunk*       pChunk;

    // Create the first chunk, which by default, will map the whole VA space
    pChunk = AllocChunk();
    if (pChunk != NULL)
    {
        // Initialize the chunk and add it to the list
        pChunk->m_addr = addr;
        pChunk->m_size = size;
        chunkList().insertFirst(pChunk);

        // Initialize the VA space state to specified defaults
        m_addr                  = addr;
        m_size                  = size;
        m_allocationCount       = 0;
        m_alignmentGranularity  = aligmtGranularity;
        m_totalFreeSize         = size;

        ret = VAM_OK;
    }

    return ret;
}

VamChunk* VamVARange::AllocChunk(void)
{
    VamChunk*   pChunk;

    pChunk = new(m_hClient) VamChunk(m_hClient);

    return pChunk;
}

void VamVARange::FreeChunk(VamChunk* pChunk)
{
    delete pChunk;
}

void VamVARange::FreeChunksFromList(void)
{
    // Free the chunks from the chunk list
    if (!chunkList().isEmpty())
    {
        for (ChunkList::SafeReverseIterator chunk( chunkList() );
              chunk != NULL;
              chunk-- )
        {
            chunkList().remove(chunk);
            FreeChunk(chunk);
        }
    }
}
