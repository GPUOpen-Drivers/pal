/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2009-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  vamptb.cpp
* @brief Contains the implementation for the PTB (Page Table Block) functionality.
***************************************************************************************************
*/

#include "vamdevice.h"
#include <string.h>

/**
***************************************************************************************************
*   PtbManager::PtbManager
*
*   @brief
*       Constructor.
***************************************************************************************************
*/
PtbManager::PtbManager(
    VAM_CLIENT_HANDLE hClient)  ///< [in] Handle of VAM client
    :
    m_ptbArrayCount(0),
    m_ppPtbArrays(NULL),
    m_maxPtbEntries(0),
    m_ptbMappedSize(0),
    m_baseAddr(0)
{
}

/**
***************************************************************************************************
*   PtbManager::~PtbManager
*
*   @brief
*       Destructor.
***************************************************************************************************
*/
PtbManager::~PtbManager()
{
    if (m_ppPtbArrays != NULL)
    {
        for (UINT arrayIdx = 0; arrayIdx < m_ptbArrayCount; arrayIdx++)
        {
            // Visit each active PTB array
            PtbArray* pPtbArray = m_ppPtbArrays[arrayIdx];
            if (pPtbArray != NULL)
            {
                for (UINT entryIdx = 0; entryIdx < NUM_PTB_ENTRIES_PER_ARRAY; entryIdx++)
                {
                    // Visit each active PTB entry, free the PTB if found
                    VAM_PTB_HANDLE hPtbAlloc = pPtbArray->ptbEntries[entryIdx];
                    if (hPtbAlloc != NULL)
                    {
                        // Free the active PTB
                        m_pVamDevice->FreePTB(hPtbAlloc);
                    }
                }

                // Free this active PTB array
                m_pVamDevice->FreeSysMem(pPtbArray);
                m_ppPtbArrays[arrayIdx] = NULL;
            }
        }

        // Free the top-level array
        m_pVamDevice->FreeSysMem(m_ppPtbArrays);
        m_ppPtbArrays   = NULL;
        m_ptbArrayCount = 0;
    }
}

/**
***************************************************************************************************
*   PtbManager::Init
*
*   @brief
*       Perform the initialization work for PtbManager class.
*
*   @return
*       VAM_OK if successful. Other return codes indicate failure.
***************************************************************************************************
*/
VAM_RETURNCODE PtbManager::Init(
    VamDevice*              pVamDevice,     ///< [in] Pointer to the associated VAM device
    VAM_VIRTUAL_ADDRESS     vaRangeStart,   ///< VA range starting address
    VAM_VIRTUAL_ADDRESS     vaRangeEnd,     ///< VA range ending address
    UINT                    ptbSize)        ///< Size of PTB in bytes
{
    m_pVamDevice = pVamDevice;

    // Calculate the mapped address range by one PTB
    m_ptbMappedSize = (VAM_VA_SIZE)(ptbSize / PTE_SIZE_IN_BYTES) * VAM_PAGE_SIZE;
    VAM_ASSERT(m_ptbMappedSize != 0);

    // Calculate the aligned starting address of this VA range
    m_baseAddr = ROUND_DOWN(vaRangeStart, m_ptbMappedSize);

    // Calculate the total count of required PTB entries to cover this VA range
    m_maxPtbEntries  = (UINT)((vaRangeEnd - m_baseAddr) / m_ptbMappedSize);
    m_maxPtbEntries += ((vaRangeEnd % m_ptbMappedSize != 0) ? 1 : 0);

    // Calculate the count of PTB arrays
    m_ptbArrayCount  =
        ROUND_UP(m_maxPtbEntries, NUM_PTB_ENTRIES_PER_ARRAY) / NUM_PTB_ENTRIES_PER_ARRAY;
    if (m_ptbArrayCount > DEFAULT_PTB_ARRAY_COUNT)
    {
        // Count of PTB arrays is set to be Min(calculated value, default value)
        m_ptbArrayCount = DEFAULT_PTB_ARRAY_COUNT;
    }

    m_ppPtbArrays = static_cast<PtbArray**>(
                        pVamDevice->AllocSysMem(m_ptbArrayCount * sizeof(PtbArray*)));

    if (m_ppPtbArrays == NULL)
    {
        VAM_ASSERT_ALWAYS();
        return VAM_OUTOFMEMORY;
    }

    memset(m_ppPtbArrays, 0, m_ptbArrayCount * sizeof(PtbArray*));

    return VAM_OK;
}

/**
***************************************************************************************************
*   PtbManager::GetPtb
*
*   @brief
*       Get PTB handle based on the specified PTB index.
*
*   @return
*       PTB allocation handle corresponding to the index.
***************************************************************************************************
*/
VAM_PTB_HANDLE PtbManager::GetPtb(
    UINT idx)   ///< PTB index
{
    VAM_CLIENT_HANDLE hPtbAlloc = NULL;

    if (idx < m_maxPtbEntries)
    {
        // Calculate the PTB array index and entry index within this array
        UINT arrayIdx = idx / NUM_PTB_ENTRIES_PER_ARRAY;
        UINT entryIdx = idx % NUM_PTB_ENTRIES_PER_ARRAY;

        if (arrayIdx < m_ptbArrayCount)
        {
            const PtbArray* pPtbArray = m_ppPtbArrays[arrayIdx];

            if (pPtbArray != NULL)
            {
                // The PTB array should be active
                hPtbAlloc = pPtbArray->ptbEntries[entryIdx];
            }
        }
    }
    else
    {
        // PTB index is out of valid range [0, m_maxPtbEntries - 1]
        VAM_ASSERT_ALWAYS();
    }

    return hPtbAlloc;
}

/**
***************************************************************************************************
*   PtbManager::SetPtb
*
*   @brief
*       Set PTB allocation handle based on the specified PTB index.
*
*   @return
*       VAM_OK if successful. Other return codes indicate failure.
***************************************************************************************************
*/
VAM_RETURNCODE PtbManager::SetPtb(
    UINT            idx,        ///< PTB index
    VAM_PTB_HANDLE  hPtbAlloc)  ///< [in] PTB allocation handle
{
    VAM_RETURNCODE ret = VAM_ERROR;

    if (idx < m_maxPtbEntries)
    {
        if (idx >= m_ptbArrayCount * NUM_PTB_ENTRIES_PER_ARRAY)
        {
            // The PTB index is out of currently-supported range. Increase the top-level array
            // in size accordingly (add more PTB arrays) so as to include this specified index.
            UINT ptbArrayCountNew = 2 * m_ptbArrayCount;

            if (idx >= ptbArrayCountNew * NUM_PTB_ENTRIES_PER_ARRAY)
            {
                // We have doubled the top-level array size. If that still does not meet the need,
                // re-calculate the new size, which should be minimum yet is still able to include
                // the specified index.
                ptbArrayCountNew =
                    ROUND_UP(idx, NUM_PTB_ENTRIES_PER_ARRAY) / NUM_PTB_ENTRIES_PER_ARRAY;
            }

            // Allocate memory for the new top-level manager array
            PtbArray** ppPtbArraysNew = static_cast<PtbArray**>(m_pVamDevice->AllocSysMem(
                                            ptbArrayCountNew * sizeof(PtbArray*)));
            if (ppPtbArraysNew == NULL)
            {
                VAM_ASSERT_ALWAYS();
                return VAM_OUTOFMEMORY;
            }

            // Copy data of the existing top-level array to the newly created one
            memset(ppPtbArraysNew, 0, ptbArrayCountNew * sizeof(PtbArray*));
            memcpy(ppPtbArraysNew, m_ppPtbArrays, m_ptbArrayCount * sizeof(PtbArray*));

            // Update the top-level mamager array and corresponding count of PTB arrays.
            m_pVamDevice->FreeSysMem(m_ppPtbArrays);
            m_ppPtbArrays   = ppPtbArraysNew;
            m_ptbArrayCount = ptbArrayCountNew;
        }

        UINT arrayIdx = idx / NUM_PTB_ENTRIES_PER_ARRAY;
        UINT entryIdx = idx % NUM_PTB_ENTRIES_PER_ARRAY;

        PtbArray* pPtbArray = m_ppPtbArrays[arrayIdx];
        if (hPtbAlloc != NULL)
        {
            // PTB allocation handle is valid, store it to the corresponding entry.
            if (pPtbArray == NULL)
            {
                // The PTB array is inactive, allocate it
                pPtbArray = static_cast<PtbArray*>(m_pVamDevice->AllocSysMem(sizeof(PtbArray)));
                if (pPtbArray == NULL)
                {
                    VAM_ASSERT_ALWAYS();
                    return VAM_OUTOFMEMORY;
                }

                memset(pPtbArray, 0, sizeof(PtbArray));
                m_ppPtbArrays[arrayIdx] = pPtbArray;
            }

            pPtbArray->ptbEntries[entryIdx] = hPtbAlloc;
            pPtbArray->numActivePtbEntries++;
        }
        else
        {
            // PTB allocation handle is NULL (the request is from PTB trimming), remove the entry.
            VAM_ASSERT(pPtbArray != NULL);

            pPtbArray->ptbEntries[entryIdx] = NULL;
            pPtbArray->numActivePtbEntries--;

            if (pPtbArray->numActivePtbEntries == 0)
            {
                // The whole PTB array does not contain any active PTB, destroy it
                m_pVamDevice->FreeSysMem(pPtbArray);
                m_ppPtbArrays[arrayIdx] = NULL;
            }
        }

        ret = VAM_OK;
    }
    else
    {
        // PTB index is out of valid range [0, m_maxPtbEntries - 1]
        VAM_ASSERT_ALWAYS();
    }

    return ret;
}

/**
***************************************************************************************************
*   PtbManager::AssignPtb
*
*   @brief
*       Assign PTBs to back the specified VA range.
*
*   @return
*       VAM_OK if successful. Other return codes indicate failure.
***************************************************************************************************
*/
VAM_RETURNCODE PtbManager::AssignPtb(
    VAM_VIRTUAL_ADDRESS vaStart,    ///< Starting VA
    VAM_VIRTUAL_ADDRESS vaEnd)      ///< Ending VA
{
    VAM_RETURNCODE ret = VAM_OK;

    VAM_ASSERT(vaStart <= vaEnd);

    // No need to calculate all the indices and trying to loop if vaEnd is not bigger than
    // vaStart
    if (vaStart < vaEnd)
    {
        VAM_PTB_HANDLE hPtbAlloc = NULL;

        // Offset the input vaStart/vaEnd (substract m_baseAddr) in order to calcualte the real
        // starting/ending PTB index
        vaEnd = vaEnd - 1;
        UINT idxStart = (UINT)((vaStart - m_baseAddr) / m_ptbMappedSize);
        UINT idxEnd   = (UINT)((vaEnd   - m_baseAddr) / m_ptbMappedSize);
        VAM_ASSERT(idxEnd < m_maxPtbEntries);

        for (UINT idx = idxStart; idx <= idxEnd; idx++)
        {
            if (GetPtb(idx) == NULL)
            {
                // No active PTB at this index, allocate one
                hPtbAlloc = m_pVamDevice->AllocPTB(idx * m_ptbMappedSize + m_baseAddr, &ret);

                if (hPtbAlloc != NULL)
                {
                    ret = SetPtb(idx, hPtbAlloc);
                }

                // Error found, terminate immediately
                if (ret != VAM_OK)
                {
                    break;
                }
            }
        }
    }

    return ret;
}

/**
***************************************************************************************************
*   PtbManager::TrimPtb
*
*   @brief
*       Trim the backing PTBs corresponding to the specified VA range.
*
*   @return
*       VAM_OK if successful. Other return codes indicate failure.
***************************************************************************************************
*/
VAM_RETURNCODE PtbManager::TrimPtb(
    VAM_VIRTUAL_ADDRESS vaStart,    ///< Starting VA
    VAM_VIRTUAL_ADDRESS vaEnd)      ///< Ending VA
{
    VAM_RETURNCODE ret = VAM_OK;

    VAM_VA_SIZE vaSize  = vaEnd - vaStart;

    if (vaSize >= m_ptbMappedSize)
    {
        VAM_PTB_HANDLE hPtbAlloc = NULL;

        UINT idxStart = (UINT)(ROUND_UP(vaStart - m_baseAddr, m_ptbMappedSize) / m_ptbMappedSize);
        UINT idxEnd   = (UINT)(ROUND_DOWN(vaEnd - m_baseAddr, m_ptbMappedSize) / m_ptbMappedSize);
        VAM_ASSERT(idxEnd < m_maxPtbEntries);

        for (UINT idx = idxStart; idx < idxEnd; idx++)
        {
            hPtbAlloc = GetPtb(idx);
            if (hPtbAlloc != NULL)
            {
                // Find an active PTB, free it.
                m_pVamDevice->FreePTB(hPtbAlloc);
                ret = SetPtb(idx, NULL);

                // Error found, terminate immediately
                if (ret != VAM_OK)
                {
                    break;
                }
            }
        }
    }

    return ret;
}
