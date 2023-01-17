/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2009-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  vamptb.h
* @brief Contains definitions for the PTB (Page Table Block) management functionality.
***************************************************************************************************
*/

#ifndef __VAMPTB_H__
#define __VAMPTB_H__

#include "vamcommon.h"

// Forward declarations
class VamDevice;

//
// The total VA space is 64GB by default. Normally, each PTB covers 256MB of address space. So
// there will be 256 PTBs required. If the coverage of one PTB is reduced to 2MB (for Carrizo),
// 32768 PTBs (128 * 256) will be needed.
//

/// Default count of PTB arrays
static const UINT DEFAULT_PTB_ARRAY_COUNT   = 128;

/// Number of PTB entries per array
static const UINT NUM_PTB_ENTRIES_PER_ARRAY = 256;

/**
***************************************************************************************************
* @brief  Represent a PTB array, store a list of PTB handles.
***************************************************************************************************
*/
struct PtbArray
{
    UINT           numActivePtbEntries;                     ///< Number of active PTB entries
    VAM_PTB_HANDLE ptbEntries[NUM_PTB_ENTRIES_PER_ARRAY];   ///< PTB entry array
};

/**
***************************************************************************************************
* @brief  Definition of PtbManager class, used for the management work of PTB.
***************************************************************************************************
*/
class PtbManager
{
public:
    PtbManager(VAM_CLIENT_HANDLE hClient);
    PtbManager() {}

    ~PtbManager();

    VAM_RETURNCODE Init(VamDevice*          pVamDevice,
                        VAM_VIRTUAL_ADDRESS vaRangeStart,
                        VAM_VIRTUAL_ADDRESS vaRangeEnd,
                        UINT                ptbSize);

    VAM_RETURNCODE AssignPtb(VAM_VIRTUAL_ADDRESS vaStart, VAM_VIRTUAL_ADDRESS vaEnd);

    VAM_RETURNCODE TrimPtb(VAM_VIRTUAL_ADDRESS vaStart, VAM_VIRTUAL_ADDRESS vaEnd);

private:
    // Disallow the copy constructor
    PtbManager(const PtbManager& a);

    // Disallow the assignment operator
    PtbManager& operator=(const PtbManager& a);

    VAM_PTB_HANDLE GetPtb(UINT idx);

    VAM_RETURNCODE SetPtb(UINT idx, VAM_PTB_HANDLE hAllocPtb);

    UINT                m_ptbArrayCount;    ///< Count of PTB arrays
    PtbArray**          m_ppPtbArrays;      ///< Top-level array managing PTB arrays
    UINT                m_maxPtbEntries;    ///< Maximum number of PTB entries to back the VA range

    VAM_VA_SIZE         m_ptbMappedSize;    ///< Mapped address range by one PTB
    VAM_VIRTUAL_ADDRESS m_baseAddr;         ///< Aligned starting address of the VA range

    VamDevice*          m_pVamDevice;       ///< Pointer to the associated VAM device
};

#endif
