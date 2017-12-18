/*
 *******************************************************************************
 *
 * Copyright (c) 2009-2017 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

/**
***************************************************************************************************
* @file  vamrange.h
* @brief Contains the VamVARange base class definition.
***************************************************************************************************
*/

#ifndef __VAMRANGE_H__
#define __VAMRANGE_H__

#include "vamcommon.h"

class VamVARange
{
public:
    VamVARange()
    :
    m_treeEnabled(false)
    {}
    VamVARange(VAM_CLIENT_HANDLE hClient)
    :
    m_treeEnabled(false)
    {
        m_addr                  = 0;
        m_size                  = 0;
        m_allocationCount       = 0;
        m_alignmentGranularity  = 0;
        m_hClient               = hClient;
        m_totalFreeSize         = 0;
    }
   ~VamVARange() {}

    bool IsVAInsideRange( VAM_VIRTUAL_ADDRESS virtAddr )
    {
        return ( (virtAddr >= m_addr) &&
                 (virtAddr < (m_addr + m_size)) );
    }

    VAM_VIRTUAL_ADDRESS addr(void)
    {return m_addr;}

    VAM_VA_SIZE size(void)
    {return m_size;}

    ChunkList& chunkList(void)
    {return m_chunkList;}

    ChunkTree& chunkTree(void)
    {return m_chunkTree;}

    void incFreeSize(VAM_VA_SIZE size)
    {
        m_totalFreeSize += size;
        decAllocationCount();
    }

    void decFreeSize(VAM_VA_SIZE size)
    {
        m_totalFreeSize -= size;
        incAllocationCount();
    }

    void incAllocationCount(void)
    {m_allocationCount++;}

    void decAllocationCount(void)
    {if (m_allocationCount) m_allocationCount--;}

    UINT allocationCount(void)
    {return m_allocationCount;}

    UINT alignmentGranularity(void)
    {return m_alignmentGranularity;}

    VAM_VA_SIZE totalFreeSize(void)
    {return m_totalFreeSize;}

    VAM_RETURNCODE Init(
        VAM_VIRTUAL_ADDRESS     addr,
        VAM_VA_SIZE             size,
        UINT                    aligmtGranularity);

    VamChunk* AllocChunk(void);

    void FreeChunk(
        VamChunk*               pChunk);

    void FreeChunksFromList(void);

    bool IsVASpaceInsideChunk(
        VAM_VIRTUAL_ADDRESS vaStart,
        VAM_VA_SIZE         vaSize,
        VamChunk*           pChunk);

    VAM_RETURNCODE AllocateVASpace(
        VAM_VA_SIZE             sizeInBytes,
        VAM_VA_SIZE             alignment,
        VAM_ALLOCATION&         allocation);

    VAM_RETURNCODE FreeVASpace(
        VAM_VIRTUAL_ADDRESS     virtualAddress,
        VAM_VA_SIZE             actualSize);

    VAM_RETURNCODE AllocateVASpaceWithAddress(
        VAM_VIRTUAL_ADDRESS     virtualAddress,
        VAM_VA_SIZE             sizeInBytes,
        VAM_ALLOCATION&         allocation,
        bool                    beyondBaseVA = false);
private:
    VAM_RETURNCODE FreeVASpaceWithTreeEnabled(
        VAM_VIRTUAL_ADDRESS     virtualAddress,
        VAM_VA_SIZE             actualSize);

    VAM_RETURNCODE FreeVASpaceWithTreeDisabled(
        VAM_VIRTUAL_ADDRESS     virtualAddress,
        VAM_VA_SIZE             actualSize);

private:
    VAM_VIRTUAL_ADDRESS     m_addr;                 // Starting address of VA range to be managed
    VAM_VA_SIZE             m_size;                 // Size of VA range to be managed
    UINT                    m_allocationCount;      // Number of allocations in this VA range
    UINT                    m_alignmentGranularity; // Minimum allocation alignment granularity for this VA range
    VAM_CLIENT_HANDLE       m_hClient;

    VAM_VA_SIZE             m_totalFreeSize;        // Amount of total free space in this VA range
    ChunkList               m_chunkList;            // Chunk list to record free VA chunks
    ChunkTree               m_chunkTree;            // Chunk tree to record free VA chunks
    bool                    m_treeEnabled;          // If chunk tree is enabled
};

class VamGlobalVASpace : public VamVARange
{
public:
    VamGlobalVASpace() {}
    VamGlobalVASpace(VAM_CLIENT_HANDLE hClient) : VamVARange(hClient)
    {
        m_raftCount             = 0;
        m_sectionCount          = 0;
        m_excludedRangeCount    = 0;
    }
    ~VamGlobalVASpace() {}

    UINT raftCount(void)
    { return m_raftCount; }

    void incRaftCount(void)
    { m_raftCount++; }

    void decRaftCount(void)
    { if (m_raftCount) m_raftCount--; }

    UINT sectionCount(void)
    { return m_sectionCount; }

    void incSectionCount(void)
    { m_sectionCount++; }

    void decSectionCount(void)
    { if (m_sectionCount) m_sectionCount--; }

    UINT excludedRangeCount(void)
    { return m_excludedRangeCount; }

    void incExcludedRangeCount(void)
    { m_excludedRangeCount++; }

private:
    UINT                    m_raftCount;            // Total number of rafts
    UINT                    m_sectionCount;         // Total number of sections
    UINT                    m_excludedRangeCount;   // Total number of excluded ranges
};

#endif // __VAMRANGE_H__
