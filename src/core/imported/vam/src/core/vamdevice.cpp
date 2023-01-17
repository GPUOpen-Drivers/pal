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
* @file  vamdevice.cpp
* @brief Contains the VamDevice base class implementation.
***************************************************************************************************
*/

#include "vamdevice.h"
#include <string.h>

/**
***************************************************************************************************
*   VamDevice::VamDevice
*
*   @brief
*       Constructor for the Vam Device class.
*
*   @return
*       N/A
***************************************************************************************************
*/
VamDevice::VamDevice()
{
}

/**
***************************************************************************************************
*   VamDevice::VamDevice
*
*   @brief
*       Constructor for the Vam Device class with client handle as parameter.
*
*   @return
*       N/A
***************************************************************************************************
*/
VamDevice::VamDevice(VAM_CLIENT_HANDLE hClient) :
    VamObject(hClient), m_gpuCount(1), m_globalVASpace(hClient), m_ptbMgr(hClient)
{
}

VamDevice* VamDevice::GetVamDeviceObject(VAM_HANDLE hVam)
{
    VamDevice* pVam = static_cast<VamDevice *>(hVam);

    VAM_ASSERT(pVam);

    return pVam;
}

VamSection* VamDevice::GetVamSectionObject(VAM_SECTION_HANDLE hSection)
{
    VamSection* pSection = static_cast<VamSection *>(hSection);

    VAM_ASSERT(pSection);

    return pSection;
}

VamRaft* VamDevice::GetVamRaftObject(VAM_RAFT_HANDLE hRaft)
{
    VamRaft* pRaft = static_cast<VamRaft *>(hRaft);

    VAM_ASSERT(pRaft);

    return pRaft;
}

/**
***************************************************************************************************
*   VamDevice::Create
*
*   @brief
*       Creates and initializes VamDevice object.
*
*   @return
*       Returns pointer to VamDevice object if successful.
***************************************************************************************************
*/
VamDevice* VamDevice::Create(
    VAM_CLIENT_HANDLE           hClient,    ///< Handle of the client associated with this instance of VAM
    const VAM_CREATE_INPUT*     pCreateIn)  ///< Input data structure for the capabilities description
{
    VamDevice* pVamDevice = NULL;

    // Perform further sanity checks on provided setup information
    if (pCreateIn->version.major == VAM_VERSION_MAJOR &&
        pCreateIn->PTBSize    &&
        pCreateIn->bigKSize   &&
        pCreateIn->VARangeEnd &&
        pCreateIn->VARangeStart < pCreateIn->VARangeEnd)
    {
        // Create the main VAM device (i.e. instance) object
        pVamDevice = new(hClient) VamDevice(hClient);
        if (pVamDevice != NULL)
        {
            // Initialize the Vam Device object
            VAM_RETURNCODE ret = pVamDevice->Init(pCreateIn);

            if (ret != VAM_OK)
            {
                VAM_ASSERT(FALSE);
                pVamDevice->Destroy();
                pVamDevice = NULL;
            }
        }
    }

    VAM_ASSERT(pVamDevice);

    return pVamDevice;
}

/**
***************************************************************************************************
*   VamDevice::Init
*
*   @brief
*       Initializes the VamDevice object.
*
*   @return
*       Returns VAM_OK if successful.
***************************************************************************************************
*/
VAM_RETURNCODE VamDevice::Init(
    const VAM_CREATE_INPUT* pCreateIn)  ///< Input data structure for the capabilities description
{
    VAM_RETURNCODE  ret;

    // Initialize VAM device object's internal members
    m_version       = pCreateIn->version;
    m_callbacks     = pCreateIn->callbacks;
    m_VARangeStart  = pCreateIn->VARangeStart;
    m_VARangeEnd    = pCreateIn->VARangeEnd;
    m_PTBSize       = pCreateIn->PTBSize;
    m_bigKSize      = pCreateIn->bigKSize;
    m_hSyncObj      = pCreateIn->hSyncObj;
    m_flags         = pCreateIn->flags;
    m_uibVersion    = pCreateIn->uibVersion;
    if(pCreateIn->gpuCount > 1)
    {
        m_gpuCount = pCreateIn->gpuCount;
    }

    // Initialize the default global VA space state
    ret = globalVASpace().Init(
                m_VARangeStart,                     // start of global VA space
                m_VARangeEnd - m_VARangeStart + 1,  // size of global VA space
                GLOBAL_ALLOC_ALGMT_SIZE);           // minimum alignment granularity for global VA space is page size

    if (m_flags.useUIB
       )
    {
        ret = VAM_INVALIDPARAMETERS;
    }

    if (ret == VAM_OK)
    {
        // Initialize the PTB array
        ret = ptbMgr().Init(
                this,
                m_VARangeStart,
                m_VARangeEnd,
                m_PTBSize);
    }

    return ret;
}

/**
***************************************************************************************************
*   VamDevice::Destroy
*
*   @brief
*       Destroys the VamDevice object.
*
*   @return
*       Returns VAM_OK if successful.
***************************************************************************************************
*/
VAM_RETURNCODE VamDevice::Destroy(void)
{
    VAM_RETURNCODE ret = VAM_OK;

    // Destroy all the resources

    // Free all the excluded ranges
    if (!excludedRangeList().isEmpty())
    {
        for (ExcludedRangeList::SafeReverseIterator range(excludedRangeList());
                range != NULL;
                range--)
        {
            excludedRangeList().remove(range);
            delete range;
        }
    }

    // Free all sections and corresponding section allocations
    if (!sectionList().isEmpty())
    {
        for (SectionList::SafeReverseIterator section(sectionList());
                section != NULL;
                section--)
        {
            // The VAM object is being destroyed, we will need to free the sections
            // in it entirely no matter whether they are empty or not.
            ret = FreeSection(section, false);
        }
    }

    // Free all rafts and their corresponding blocks
    if (!raftList().isEmpty())
    {
        for (RaftList::SafeReverseIterator raft(raftList());
                raft != NULL;
                raft--)
        {
            // Since the VAM object is being destroyed, we will need to free the rafts
            // in its entirety, w/o having to check and see if it is empty or not.
            ret = FreeRaft(raft, false);
        }
    }

    // Free all the global chunk resources
    globalVASpace().FreeChunksFromList();

    // Free system memory used by this object.
    delete this;

    return ret;
}

VAM_RETURNCODE VamDevice::RegularAllocateVASpace(
    VAM_ALLOC_INPUT*    pAllocIn,
    VAM_ALLOC_OUTPUT*   pAllocOut)
{
    VAM_RETURNCODE      ret = VAM_OK;
    VAM_VA_SIZE         sizeToUse;
    UINT                alignmentToUse;
    VAM_ALLOCATION      allocation;

    if (pAllocIn->sizeInBytes == 0)
    {
        return VAM_INVALIDPARAMETERS;
    }

    if (AcquireSyncObj() != VAM_OK)
    {
        return VAM_ERROR;
    }

    if (m_gpuCount > 1 && pAllocIn->gpuMask == 0)
    {
        pAllocIn->gpuMask = ((1 << m_gpuCount) - 1);
    }
    else
    {
        if (m_gpuCount == 1)
        {
            pAllocIn->gpuMask = 0;
        }
    }

    // default is NULL for allocation tracker
    pAllocOut->hVaAlloc = NULL;

    // We're allocating from global VA space. See if optional VA is specified.
    if (pAllocIn->virtualAddress)
    {
        // Optional VA is requested. Is fragment specified?
        if (pAllocIn->flags.useFragment)
        {
            // Yes. Fragment is specified
            if (IS_ALIGNED(pAllocIn->virtualAddress, m_bigKSize))
            {
                // Yes. Ensure that the size is fragment-aligned as well.
                sizeToUse = ROUND_UP(pAllocIn->sizeInBytes, (long long) m_bigKSize);
            }
            else
            {
                // No. Allocation will fail due to address not being fragment-aligned
                ret = VAM_OPTIONALVANOTFRAGMENTALIGNED;
            }
        }
        else
        {
            // No. Fragment is not specified
            sizeToUse = pAllocIn->sizeInBytes;
        }

        if (ret == VAM_OK)
        {
            // Call the alloc routine that supports the optional VA
            if (pAllocIn->hSection)
            {
                // Allocation from specified section
                VamSection* pSection = GetVamSectionObject(pAllocIn->hSection);
                if (pSection)
                {
                    VAM_ASSERT(sectionList().contains(pSection));
                    ret = pSection->VASpace().AllocateVASpaceWithAddress(pAllocIn->virtualAddress,
                                                                         sizeToUse,
                                                                         allocation,
                                                                         pAllocIn->flags.beyondRequestedVa);
                }
                else
                {
                    // Invalid section handle
                    ret = VAM_INVALIDPARAMETERS;
                }
            }
            else
            {
                // Allocation from global VA space
                ret = globalVASpace().AllocateVASpaceWithAddress(pAllocIn->virtualAddress,
                                                                 sizeToUse,
                                                                 allocation,
                                                                 pAllocIn->flags.beyondRequestedVa);
            }
        }
    }
    else
    {
        // Optional VA not specified. Is alignment value a power-of-2 ?
        if (POW2(pAllocIn->alignment))
        {
            // Yes. Is fragment specified?
            if (pAllocIn->flags.useFragment)
            {
                // Yes. Fragment is specified; round up to fragment size
                sizeToUse = ROUND_UP(pAllocIn->sizeInBytes, (long long) m_bigKSize);
                alignmentToUse = ROUND_UP(pAllocIn->alignment, m_bigKSize);
            }
            else
            {
                // No. Fragment is not specified; round up to page size
                sizeToUse = ROUND_UP(pAllocIn->sizeInBytes, (long long) GLOBAL_ALLOC_ALGMT_SIZE);
                alignmentToUse = ROUND_UP(pAllocIn->alignment, GLOBAL_ALLOC_ALGMT_SIZE);
            }
        }
        else
        {
            // No. Allocation will fail due to alignment not being a POW2 value
            ret = VAM_INVALIDPARAMETERS;
        }

        if (ret == VAM_OK)
        {
            // Call the alloc routine that doesn't support the optional VA
            if (pAllocIn->hSection)
            {
                // Allocation from specified section
                VamSection* pSection = GetVamSectionObject(pAllocIn->hSection);
                if (pSection)
                {
                    VAM_ASSERT(sectionList().contains(pSection));
                    ret = pSection->VASpace().AllocateVASpace(sizeToUse,
                                                              alignmentToUse,
                                                              allocation);
                }
                else
                {
                    // Invalid section handle
                    ret = VAM_INVALIDPARAMETERS;
                }
            }
            else
            {
                // Allocation from global VA space
                ret = globalVASpace().AllocateVASpace(sizeToUse,
                                                      alignmentToUse,
                                                      allocation);
            }
        }
    }

    if (ret == VAM_OK)
    {
        // Allocation was successful

        // For all global allocations (except rafts), it will need to be
        // ensured that the allocated space is properly mapped by a PTB.
        if (m_callbacks.needPTB() == VAM_OK)
        {
            ret = MapPTB(allocation);
        }
        if (ret == VAM_OK)
        {
            pAllocOut->virtualAddress = allocation.address;
            pAllocOut->actualSize     = allocation.size;

            // check for multi GPU case
            if (pAllocIn->gpuMask > 0)
            {
                // hRaft is NULL here for the allocation (default)
                VamAllocation*   pAllocation = new(m_hClient) VamAllocation(m_hClient, pAllocIn->gpuMask);

                pAllocOut->hVaAlloc = (VAM_ALLOCATION_HANDLE) pAllocation;

                if (!pAllocation)
                {
                    ret = VAM_OUTOFMEMORY;
                }
            }
        }
        else
        {
            // We failed to allocate a PTB; release the allocation
            if (pAllocIn->hSection)
            {
                // free VA to specified section
                VamSection* pSection = GetVamSectionObject(pAllocIn->hSection);
                VAM_ASSERT(pSection != NULL);
                VAM_ASSERT(sectionList().contains(pSection));

                pSection->VASpace().FreeVASpace(allocation.address,
                                                allocation.size);
            }
            else
            {
                // free VA to global VA space
                globalVASpace().FreeVASpace(allocation.address,
                                            allocation.size);
            }

            pAllocOut->virtualAddress = 0;
            pAllocOut->actualSize     = 0;
        }
    }
    else
    {
        // Allocation failed
        pAllocOut->virtualAddress = 0;
        pAllocOut->actualSize     = 0;

        if (pAllocIn->flags.useFragment && ret != VAM_OPTIONALVANOTFRAGMENTALIGNED)
        {
            // Not able to find sufficient contiguous VA space
            // to accommodate even one fragment size.
            ret = VAM_FRAGMENTALLOCFAILED;
        }
    }

    ReleaseSyncObj();

    return ret;
}

VAM_RETURNCODE VamDevice::RegularFreeVASpace(
    VAM_FREE_INPUT*     pFreeIn)
{
    VAM_RETURNCODE  ret = VAM_INVALIDPARAMETERS;

    if(m_gpuCount > 1 && (pFreeIn->gpuMask == 0  || pFreeIn->gpuMask > (DWORD)((1 << m_gpuCount) - 1)))
    {
        // multi GPU case needs valid gpuMask
        return VAM_INVALIDPARAMETERS;
    }

    if (AcquireSyncObj() != VAM_OK)
    {
        return VAM_ERROR;
    }

    if (!usingUIB())
    {
        // UIB is not being used, so VAMFree is allowed to be executed

        // check for multi GPU case
        if (m_gpuCount > 1)
        {
            if (pFreeIn->hVaAlloc != NULL)
            {
                ret = VAM_OK; // return success if nothing is freed in the multi GPU case

                VamAllocation* pAllocation = (VamAllocation*) pFreeIn->hVaAlloc;
                pAllocation->m_gpuMask &= ~pFreeIn->gpuMask;

                if (pAllocation->m_gpuMask == 0)
                {
                    // last GPU -> perform the actual free
                    if (pFreeIn->hSection)
                    {
                        // free VA to specified section
                        VamSection* pSection = GetVamSectionObject(pFreeIn->hSection);
                        if (pSection)
                        {
                            VAM_ASSERT(sectionList().contains(pSection));
                            ret = pSection->VASpace().FreeVASpace(pFreeIn->virtualAddress, pFreeIn->actualSize);
                        }
                        else
                        {
                            // Invalid section handle
                            ret = VAM_INVALIDPARAMETERS;
                        }
                    }
                    else
                    {
                        // free VA to global VA space
                        ret = globalVASpace().FreeVASpace(pFreeIn->virtualAddress, pFreeIn->actualSize);
                    }

                    if (ret == VAM_OK)
                    {
                        // free the tracking allocation
                        delete pAllocation;
                    }
                    else
                    {
                        // free failed - restore gpuMask in case free may be re-attempted
                        pAllocation->m_gpuMask |= pFreeIn->gpuMask;
                    }
                }
            }
        }
        else
        {
            if (pFreeIn->hSection)
            {
                // free VA to specified section
                VamSection* pSection = GetVamSectionObject(pFreeIn->hSection);
                if (pSection)
                {
                    VAM_ASSERT(sectionList().contains(pSection));
                    ret = pSection->VASpace().FreeVASpace(pFreeIn->virtualAddress, pFreeIn->actualSize);
                }
                else
                {
                    // Invalid section handle
                    ret = VAM_INVALIDPARAMETERS;
                }
            }
            else
            {
                // free VA to global VA space
                ret = globalVASpace().FreeVASpace(pFreeIn->virtualAddress, pFreeIn->actualSize);
            }
        }
    }

    ReleaseSyncObj();

    return ret;
}

VAM_RETURNCODE VamDevice::QueryGlobalAllocStatus(
    VAM_GLOBALALLOCSTATUS_OUTPUT*   pGlobalAllocStatusOut)
{
    VAM_RETURNCODE  ret = VAM_OK;

    if (AcquireSyncObj() != VAM_OK)
    {
        return VAM_ERROR;
    }

    // Pull in the data from the global VA space class and return to client
    pGlobalAllocStatusOut->totalSizeInBytes         = globalVASpace().size();
    pGlobalAllocStatusOut->numberOfAllocs           = globalVASpace().allocationCount();
    pGlobalAllocStatusOut->numberOfRafts            = globalVASpace().raftCount();
    pGlobalAllocStatusOut->numberOfSections         = globalVASpace().sectionCount();
    pGlobalAllocStatusOut->numberOfExcludedRanges   = globalVASpace().excludedRangeCount();
    pGlobalAllocStatusOut->freeSizeInBytes          = globalVASpace().totalFreeSize();
    pGlobalAllocStatusOut->usedSizeInBytes          = pGlobalAllocStatusOut->totalSizeInBytes -
                                                      pGlobalAllocStatusOut->freeSizeInBytes;

    ReleaseSyncObj();

    return ret;
}

VAM_RETURNCODE VamDevice::ExcludeRange(
    VAM_EXCLUDERANGE_INPUT* pExcludeRangeIn)
{
    VAM_RETURNCODE      ret = VAM_OK;
    VAM_ALLOCATION      allocation;
    VamExcludedRange*   pExclRange;

    if (AcquireSyncObj() != VAM_OK)
    {
        return VAM_ERROR;
    }

    if(!(m_VARangeStart <= pExcludeRangeIn->virtualAddress &&
        (pExcludeRangeIn->virtualAddress + pExcludeRangeIn->sizeInBytes -1) <= m_VARangeEnd))
    {
        ret = VAM_INVALIDPARAMETERS;
    }

    if(ret == VAM_OK)
    {
        ret = globalVASpace().AllocateVASpaceWithAddress(pExcludeRangeIn->virtualAddress,
            pExcludeRangeIn->sizeInBytes,
            allocation);
    }

    if (ret == VAM_OK)
    {
        // Create memory for the excluded range object
        pExclRange = new(m_hClient) VamExcludedRange(m_hClient);
        if (pExclRange)
        {
            // Ensure that the excluded range is properly mapped by PTB(s)
            if (m_callbacks.needPTB() == VAM_OK)
            {
                ret = MapPTB(allocation);
            }
            if (ret == VAM_OK)
            {
                // Update its parameters and add the excluded range object to the list
                pExclRange->Init(pExcludeRangeIn->virtualAddress,
                                 pExcludeRangeIn->sizeInBytes,
                                 allocation.address,
                                 allocation.size);

                excludedRangeList().insertLast(pExclRange);

                // Bump the total number excluded ranges
                globalVASpace().incExcludedRangeCount();
            }
            else
            {
                // We failed to allocate a PTB; release the allocation and free the object
                globalVASpace().FreeVASpace(allocation.address,
                                            allocation.size);

                delete pExclRange;
            }
        }
    }

    ReleaseSyncObj();

    return ret;
}

VAM_SECTION_HANDLE VamDevice::CreateSection(
    VAM_VA_SIZE                     requestedSectionSizeInBytes,
    VAM_CLIENT_OBJECT               clientObject,
    VAM_CREATESECTION_FLAGS         flags,
    VAM_VIRTUAL_ADDRESS             sectionAddress,
    VAM_RETURNCODE* const           pRetCode)
{
    VAM_VA_SIZE         sectionSize;
    VAM_SECTION_HANDLE  hSection;
    VamSection*         pSection;

    if (requestedSectionSizeInBytes == 0)
    {
        *pRetCode = VAM_INVALIDPARAMETERS;
        // Requested section size is 0, error
        return NULL;
    }

    *pRetCode = AcquireSyncObj();
    if (*pRetCode != VAM_OK)
    {
        return NULL;
    }

    // The requested section size is rounded up to the bigK value to ensure a section
    // is sized in fragment multiples. As a result, the actual section size may end up
    // being bigger than what the client has originally requested.
    sectionSize = ROUND_UP(requestedSectionSizeInBytes, (long long) m_bigKSize);

    // Allocate VA space for section, create section object and initialize it.
    pSection = AllocSection(sectionSize,
                            clientObject,
                            flags,
                            sectionAddress,
                            pRetCode);

    // The section handle returned to the client is simply the pointer to this section object.
    hSection = static_cast<VAM_SECTION_HANDLE>(pSection);

    ReleaseSyncObj();

    return hSection;
}

VAM_RETURNCODE VamDevice::DestroySection(
    VAM_SECTION_HANDLE              hSection)
{
    VAM_RETURNCODE  ret = VAM_INVALIDPARAMETERS;
    VamSection*     pSection;

    if (AcquireSyncObj() != VAM_OK)
    {
        return VAM_ERROR;
    }

    pSection = GetVamSectionObject(hSection);
    if (pSection)
    {
        VAM_ASSERT(sectionList().contains(pSection));

        // This call is coming from public API and we have to check to see the specified section
        // is still empty. This function will return error if section allocations are present.
        ret = FreeSection(pSection, true);
    }

    ReleaseSyncObj();

    return ret;
}

VAM_RETURNCODE VamDevice::QuerySectionAllocStatus(
    VAM_SECTION_HANDLE              hSection,
    VAM_SECTIONALLOCSTATUS_OUTPUT*  pSectionAllocStatusOut)
{
    VAM_RETURNCODE  ret = VAM_INVALIDPARAMETERS;
    VamSection*     pSection;

    if (AcquireSyncObj() != VAM_OK)
    {
        return VAM_ERROR;
    }

    pSection = GetVamSectionObject(hSection);
    if (pSection)
    {
        VAM_ASSERT(sectionList().contains(pSection));

        // Fetch data from the section's VA space class and return to client
        pSectionAllocStatusOut->sectionSizeInBytes  = pSection->VASpace().size();
        pSectionAllocStatusOut->numberOfAllocs      = pSection->VASpace().allocationCount();
        pSectionAllocStatusOut->freeSizeInBytes     = pSection->VASpace().totalFreeSize();
        pSectionAllocStatusOut->usedSizeInBytes     = pSectionAllocStatusOut->sectionSizeInBytes -
                                                      pSectionAllocStatusOut->freeSizeInBytes;
        pSectionAllocStatusOut->sectionAddress      = pSection->VASpace().addr();

        ret = VAM_OK;
    }

    ReleaseSyncObj();

    return ret;
}

VAM_RAFT_HANDLE VamDevice::CreateRaft(
    VAM_SECTION_HANDLE      hSection,
    VAM_VA_SIZE             requestedRaftSizeInBytes,
    VAM_VA_SIZE             requestedMinBlockSizeInBytes,
    VAM_CLIENT_OBJECT       clientObject,
    VAM_CREATERAFT_FLAGS    flags,
    VAM_VIRTUAL_ADDRESS     raftAddress)
{
    VAM_VA_SIZE         raftSize, minBlockSize;
    VAM_RAFT_HANDLE     hRaft;
    VamRaft*            pRaft;

    if (requestedRaftSizeInBytes == 0)
    {
        return 0;
    }

    if (AcquireSyncObj() != VAM_OK)
    {
        return 0;
    }

    // The requested raft size is rounded up to the bigK value to ensure that
    // the raft is sized in bigK multiples. As a result, the actual raft size
    // may end up being bigger than what the client has originally requested.
    raftSize = ROUND_UP(requestedRaftSizeInBytes, (long long) m_bigKSize);

    if (!requestedMinBlockSizeInBytes)
    {
        // Client has no preference for the block size. VAM will determine the
        // sizes of the blocks. Since VAM is under the assumption that a large
        // number of small-sized allocations are to be made from the blocks,
        // it will by default create blocks in bigK fragment size. However, if
        // a requested suballocation exceeds the block size, VAM will try to
        // create a new block that is a bigK-multiple in order to fit the
        // allocation, but without going over the raft's overall size.
        minBlockSize = m_bigKSize;
    }
    else
    {
        // Client has requested a specific minimum block size figure.
        // Round it up to the fragment size.
        minBlockSize = ROUND_UP(requestedMinBlockSizeInBytes, (long long) m_bigKSize);
    }

    // Allocate VA space for raft, create raft object and initialize it.
    pRaft = AllocRaft(hSection,
                      raftSize,
                      minBlockSize,
                      clientObject,
                      flags,
                      raftAddress);

    // The raft handle returned to the client is
    // simply the pointer to our raft object.
    hRaft = static_cast<VAM_RAFT_HANDLE>(pRaft);

    ReleaseSyncObj();

    return hRaft;
}

VAM_RETURNCODE VamDevice::DestroyRaft(
    VAM_RAFT_HANDLE     hRaft)
{
    VAM_RETURNCODE  ret = VAM_INVALIDPARAMETERS;
    VamRaft*        pRaft;

    if (AcquireSyncObj() != VAM_OK)
    {
        return VAM_ERROR;
    }

    pRaft = GetVamRaftObject(hRaft);
    if (pRaft)
    {
        VAM_ASSERT(raftList().contains(pRaft));

        // Since this is coming from the public API, we will need to check to see
        // if it is still empty. will return error, if allocations are present.
        ret = FreeRaft(pRaft, true);
    }

    ReleaseSyncObj();

    return ret;
}

VAM_RETURNCODE VamDevice::Trim(
    VAM_TRIM_FLAGS      flags)
{
    VAM_RETURNCODE  ret = VAM_OK;

    if (AcquireSyncObj() != VAM_OK)
    {
        return VAM_ERROR;
    }

    // Walk the list of all rafts
    // We will be freeing up unused space, and calculating unused page tables
    if (!raftList().isEmpty())
    {
        for (RaftList::SafeReverseIterator raft(raftList());
                ((raft != NULL) && (ret == VAM_OK));
                raft--)
        {
            // Walk the list of all the blocks in this raft
            for (BlockList::SafeReverseIterator block( raft->blockList() );
                    ((block != NULL) && (ret == VAM_OK));
                    block-- )
            {
                if ((block->VASpace().allocationCount() == 0) && flags.trimEmptyBlocks)
                {
                    // Only continue if the block was not offered (or user forced the situation)
                    if (!block->offered() || flags.trimOfferedBlocks)
                    {
                        ret = raft->FreeBlock(block);
                        if (ret != VAM_OK)
                        {
                            break; // Something unexpected, so abort the trim process
                        }
                    }
                }
            }
        }
    }

    if (ret == VAM_OK)
    {
        if (!globalVASpace().chunkList().isEmpty() && flags.trimEmptyPageTables)
        {
            for (ChunkList::Iterator chunk( globalVASpace().chunkList() );
                  chunk != NULL;
                  chunk++ )
            {
                ret = ptbMgr().TrimPtb(chunk->m_addr, chunk->m_addr + chunk->m_size);

                if (ret != VAM_OK)
                {
                    break;
                }
            }
        }
    }

    ReleaseSyncObj();

    return ret;
}

VAM_RETURNCODE VamDevice::GetRaftAllocationInfo(
    VAM_RAFT_HANDLE                 hRaft,
    VAM_ALLOC_OUTPUT*               pAllocOut)
{
    VAM_RETURNCODE  ret = VAM_INVALIDPARAMETERS;
    VamRaft*        pRaft;

    if (AcquireSyncObj() != VAM_OK)
    {
        return VAM_ERROR;
    }

    pRaft = GetVamRaftObject(hRaft);
    if (pRaft)
    {
        VAM_ASSERT(raftList().contains(pRaft));

        pAllocOut->virtualAddress = pRaft->VASpace().addr();
        pAllocOut->actualSize = pRaft->VASpace().size();
        pAllocOut->hVaAlloc = 0;

        ret = VAM_OK;
    }

    ReleaseSyncObj();

    return ret;
}

VAM_RETURNCODE VamDevice::SubAllocateVASpace(
    VAM_SUBALLOC_INPUT*     pSubAllocIn,
    VAM_SUBALLOC_OUTPUT*    pSubAllocOut)
{
    VAM_RETURNCODE      ret = VAM_OUTOFMEMORY;
    VAM_ALLOCATION      subAllocation;
    VAM_VA_SIZE         sizeToUse;
    UINT                alignmentToUse;
    VamRaft*            pRaft;
    VamBlock*           pBlock;

    if (AcquireSyncObj() != VAM_OK)
    {
        return VAM_ERROR;
    }

    if (!POW2(pSubAllocIn->alignment) || !pSubAllocIn->hRaft
        || pSubAllocIn->sizeInBytes == 0)
    {
        // Alignments must be POW2 values and hRaft must be valid
        ret = VAM_INVALIDPARAMETERS;
    }

    if(ret != VAM_INVALIDPARAMETERS)
    {
        pRaft = GetVamRaftObject(pSubAllocIn->hRaft);
        if (pRaft)
        {
            if(m_gpuCount > 1 && pSubAllocIn->gpuMask == 0)
            {
                pSubAllocIn->gpuMask = ((1 << m_gpuCount) - 1);
            }
            else
            {
                if(m_gpuCount == 1)
                {
                    pSubAllocIn->gpuMask = 0;
                }
            }

            VAM_ASSERT(raftList().contains(pRaft));

            // Adjust the specified size and alignment, so that the allocation is made
            // in line with the VA space's alignment granularity requirements.
            sizeToUse = ROUND_UP(pSubAllocIn->sizeInBytes, (long long) SUB_ALLOC_ALGMT_SIZE);
            alignmentToUse = ROUND_UP(pSubAllocIn->alignment, SUB_ALLOC_ALGMT_SIZE);

            // Cycle through existing blocks to see if requested VA space exists
            for (BlockList::Iterator block( pRaft->blockList() );
                block != NULL;
                block++ )
            {
                if (block->offered() == true)
                {
                    // Can not suballocate from blocks that have been offered
                    continue;
                }

                ret = block->VASpace().AllocateVASpace(sizeToUse,
                    alignmentToUse,
                    subAllocation);

                if (ret == VAM_OK)
                {
                    // Found a block and successfully allocated from inside it
                    pBlock = block;
                    break;
                }
            }

            if (ret != VAM_OK)
            {
                // All blocks exhausted. If there is still room in the raft, create a
                // new block and attempt to allocate from it.
                // We will also get here when allocating the 1st block in an empty raft.
                pBlock = pRaft->AllocBlock(pSubAllocIn->sizeInBytes);
                if (pBlock)
                {
                    ret = pBlock->VASpace().AllocateVASpace(sizeToUse,
                        alignmentToUse,
                        subAllocation);

                    // The AllocateVASpace() fnc bumps up the allocation count for the
                    // parent object. This means that when a new block is created, the
                    // raft's allocation count is bumped by 1.
                    // In order to compensate for that, we need to decrement it here by 1,
                    // so as to report the proper suballocation count inside the raft object.
                    pRaft->VASpace().decAllocationCount();
                }
            }

            if (ret == VAM_OK)
            {
                // Suballocation successful; propagate the results to the caller
                pSubAllocOut->virtualAddress = subAllocation.address;
                pSubAllocOut->actualSize     = subAllocation.size;
                pSubAllocOut->hVidMem        = pBlock->hVidMem();
                pSubAllocOut->offsetInBytes  = (UINT)(subAllocation.address - pBlock->VASpace().addr());
                pSubAllocOut->hVaAlloc       = NULL;

                // check for multi GPU case
                if(pSubAllocIn->gpuMask > 0)
                {
                    VamAllocation*   pAllocation = new(m_hClient) VamAllocation(m_hClient,
                                                                                pSubAllocIn->gpuMask,
                                                                                pSubAllocIn->hRaft);

                    pSubAllocOut->hVaAlloc = (VAM_ALLOCATION_HANDLE) pAllocation;

                    if(!pAllocation)
                    {
                        ret = VAM_OUTOFMEMORY;
                    }
                }

                // Bump the allocation counts in raft's VA space
                pRaft->VASpace().incAllocationCount();
            }
            else
            {
                // Suballocation failed; clear out the output data structure
                memset(pSubAllocOut, 0, sizeof(*pSubAllocOut));
            }
        }
    }

    ReleaseSyncObj();

    return ret;
}

VAM_RETURNCODE VamDevice::SubFreeVASpace(
    VAM_SUBFREE_INPUT*      pSubFreeIn)
{
    VAM_RETURNCODE  ret = VAM_INVALIDPARAMETERS;
    VamRaft*        pRaft;

    if(m_gpuCount > 1 && (pSubFreeIn->gpuMask == 0  || pSubFreeIn->gpuMask > (DWORD)((1 << m_gpuCount) - 1)))
    {
        // multi GPU case needs valid gpuMask
        return VAM_INVALIDPARAMETERS;
    }

    if (AcquireSyncObj() != VAM_OK)
    {
        return VAM_ERROR;
    }

    pRaft = GetVamRaftObject(pSubFreeIn->hRaft);
    if (pRaft && pSubFreeIn->actualSize <= pRaft->VASpace().size())
    {
        VAM_ASSERT(raftList().contains(pRaft));

        // Make sure that the VA is inside the raft
        if (pRaft->VASpace().IsVAInsideRange( pSubFreeIn->virtualAddress ))
        {
            // Find the block where the allocation resides
            for (BlockList::Iterator block( pRaft->blockList() );
                  block != NULL;
                  block++ )
            {
                if (block->offered() == true)
                {
                    // Can not free from blocks that have been offered
                    continue;
                }

                // Check if the supplied VA is in range
                if (block->VASpace().IsVAInsideRange( pSubFreeIn->virtualAddress ))
                {
                    // Found the block - free the suballocation.
                    if(m_gpuCount > 1)
                    {
                        if(pSubFreeIn->hVaAlloc != NULL && pSubFreeIn->gpuMask <= (DWORD)((1 << m_gpuCount) - 1))
                        {
                            ret = VAM_OK; // return success if nothing is freed in the multi GPU case

                            VamAllocation* pAllocation = (VamAllocation*) pSubFreeIn->hVaAlloc;
                            pAllocation->m_gpuMask &= ~pSubFreeIn->gpuMask;

                            if(pAllocation->m_gpuMask == 0)
                            {
                                // last GPU -> perform the actual free
                                ret = block->VASpace().FreeVASpace(pSubFreeIn->virtualAddress,
                                    pSubFreeIn->actualSize);

                                if (ret == VAM_OK)
                                {
                                    // free the tracking allocation
                                    delete pAllocation;

                                    // Decrement the allocation count in raft's VA spaces
                                    pRaft->VASpace().decAllocationCount();

                                    if (!block->VASpace().allocationCount() &&
                                        (!pRaft->keepBlocksResident()))
                                    {
                                        // If all suballocations have been freed from the block and
                                        // client requested to not keep the block resident, then
                                        // go ahead and release the block now.
                                        pRaft->FreeBlock(block);
                                    }
                                }
                                else
                                {
                                    // free failed - restore gpuMask in case free may be re-attempted
                                    pAllocation->m_gpuMask |= pSubFreeIn->gpuMask;
                                }
                            }
                        }
                    }
                    else
                    {
                        ret = block->VASpace().FreeVASpace(pSubFreeIn->virtualAddress,
                            pSubFreeIn->actualSize);

                        if (ret == VAM_OK)
                        {
                            // Decrement the allocation count in raft's VA spaces
                            pRaft->VASpace().decAllocationCount();

                            if (!block->VASpace().allocationCount() &&
                                (!pRaft->keepBlocksResident()))
                            {
                                // If all suballocations have been freed from the block and
                                // client requested to not keep the block resident, then
                                // go ahead and release the block now.
                                pRaft->FreeBlock(block);
                            }
                        }
                    }

                    break;
                }
            }
        }
    }

    ReleaseSyncObj();

    return ret;
}

VAM_RETURNCODE VamDevice::QuerySubAllocStatus(
    VAM_RAFT_HANDLE             hRaft,
    VAM_SUBALLOCSTATUS_OUTPUT*  pSubAllocStatusOut)
{
    VAM_RETURNCODE  ret = VAM_INVALIDPARAMETERS;
    VamRaft*        pRaft;

    if (AcquireSyncObj() != VAM_OK)
    {
        return VAM_ERROR;
    }

    pRaft = GetVamRaftObject(hRaft);
    if (pRaft)
    {
        VAM_ASSERT(raftList().contains(pRaft));

        // Pull in the data from the raft's VA space class and return to client
        pSubAllocStatusOut->raftSizeInBytes     = pRaft->VASpace().size();
        pSubAllocStatusOut->numberOfBlocks      = pRaft->blockList().numObjects();
        pSubAllocStatusOut->numberOfSubAllocs   = pRaft->VASpace().allocationCount();
        pSubAllocStatusOut->minBlockSizeInBytes = pRaft->minBlockSize();
        pSubAllocStatusOut->freeSizeInBytes     = pRaft->GetTotalFreeSize();
        pSubAllocStatusOut->usedSizeInBytes     = pSubAllocStatusOut->raftSizeInBytes -
                                                  pSubAllocStatusOut->freeSizeInBytes;

        ret = VAM_OK;
    }

    ReleaseSyncObj();

    return ret;
}

VAM_RETURNCODE VamDevice::QueryBlockStatus(
    VAM_RAFT_HANDLE         hRaft,              ///< Handle of the raft to query from
    VAM_VIDMEM_HANDLE       hVidMem,            ///< Handle of the VidMem object
    VAM_BLOCKSTATUS_OUTPUT* pQueryStatusOut)    ///< Output data structure containing block status
                                                ///< details
{
    VAM_RETURNCODE ret = VAM_INVALIDPARAMETERS;

    if (AcquireSyncObj() == VAM_OK)
    {
        VamRaft* pRaft = GetVamRaftObject(hRaft);
        if (pRaft)
        {
            VAM_ASSERT(raftList().contains(pRaft));

            // Find the block where the allocation resides
            for (BlockList::Iterator block( pRaft->blockList() ); (block != NULL); block++ )
            {
                if (block->hVidMem() == hVidMem)
                {
                    pQueryStatusOut->numberOfSubAllocs = block->VASpace().allocationCount();
                    ret = VAM_OK;
                    break;
                }
            }
        }

        ReleaseSyncObj();
    }
    else
    {
        ret = VAM_ERROR;
    }

    return ret;
}

VamSection* VamDevice::AllocSection(
    VAM_VA_SIZE                     sectionSize,
    VAM_CLIENT_OBJECT               clientObject,
    VAM_CREATESECTION_FLAGS         flags,
    VAM_VIRTUAL_ADDRESS             sectionAddress,
    VAM_RETURNCODE* const           pRetCode)
{
    VAM_ALLOCATION      sectionAllocation = { 0 };
    VamSection*         pSection = NULL;

    // Create the section object
    pSection = new(m_hClient) VamSection(m_hClient, this, clientObject, flags);
    if (pSection)
    {
        // Allocate room for the section from the global VA space
        if (sectionAddress == 0)
        {
            // Optional start address is absent, bigK size alignment
            *pRetCode = globalVASpace().AllocateVASpace(sectionSize,
                                                  m_bigKSize,
                                                  sectionAllocation);
        }
        else
        {
            // Section start address specified
            VAM_VIRTUAL_ADDRESS startVA, endVA;
            VAM_VA_SIZE         adjustedSize;

            // Adjust the specified VA and size, so that the allocation is made
            // in line with the bigK alignment granularity requirements for sections.
            startVA      = ROUND_DOWN(sectionAddress, (long long) m_bigKSize);
            endVA        = ROUND_UP(sectionAddress + sectionSize, (long long) m_bigKSize) - 1;
            adjustedSize = endVA - startVA + 1;

            *pRetCode = globalVASpace().AllocateVASpaceWithAddress(startVA,
                                                             adjustedSize,
                                                             sectionAllocation);
        }

        if (*pRetCode == VAM_OK)
        {
            // Initialize the section's default VA space state
            *pRetCode = pSection->VASpace().Init(sectionAllocation.address, // start of section's VA space
                                           sectionAllocation.size,          // size of section's VA space
                                           GLOBAL_ALLOC_ALGMT_SIZE);        // minimum alignment granularity for section VA space is page size

            if (*pRetCode == VAM_OK)
            {
                // Add the newly-created section to section list
                sectionList().insertLast(pSection);
            }
        }
        else
        {
            sectionAllocation.address = 0;
        }

        if (*pRetCode == VAM_OK)
        {
            // Count the total number of sections
            globalVASpace().incSectionCount();
        }
        else
        {
            // If failed, free the zombie section
            if (sectionAllocation.address != 0)
            {
                globalVASpace().FreeVASpace(pSection->VASpace().addr(),   // section's starting VA
                                            pSection->VASpace().size());  // section's size
            }
            delete pSection;
            pSection = NULL;
        }
    }
    else
    {
        *pRetCode = VAM_OUTOFMEMORY;
    }

    return pSection;
}

VAM_RETURNCODE VamDevice::FreeSection(
    VamSection*                     pSection,
    bool                            checkForEmpty)
{
    VAM_RETURNCODE  ret = VAM_ERROR;

    VAM_ASSERT(pSection != NULL);

    if (checkForEmpty && pSection->VASpace().allocationCount())
    {
        // Client is destroying a section which still has outstanding section allocations.
        ret = VAM_SECTIONNOTEMPTY;
    }
    else
    {
        // Free chunks from the chunk list
        pSection->VASpace().FreeChunksFromList();

        // Free the VA space which the section was using inside the global VA Space.
        globalVASpace().FreeVASpace(pSection->VASpace().addr(),   // section's starting VA
                                    pSection->VASpace().size());  // section's size

        // Remove the section object from section list
        if (!sectionList().isEmpty())
        {
            sectionList().remove(pSection);
        }
        delete pSection;

        // Decrement the total number of sections
        globalVASpace().decSectionCount();
        ret = VAM_OK;
    }

    return ret;
}

VamRaft* VamDevice::AllocRaft(
    VAM_SECTION_HANDLE      hSection,
    VAM_VA_SIZE             raftSize,
    VAM_VA_SIZE             minBlockSize,
    VAM_CLIENT_OBJECT       clientObject,
    VAM_CREATERAFT_FLAGS    flags,
    VAM_VIRTUAL_ADDRESS     raftAddress)
{
    VAM_RETURNCODE      ret;
    VAM_ALLOCATION      raftAllocation;
    VamRaft*            pRaft = NULL;

    VamSection* pSection = static_cast<VamSection *>(hSection);
    if (!pSection)
    {
        return NULL;
    }
    // Create the raft object
    pRaft = new(m_hClient) VamRaft(m_hClient, this, minBlockSize, clientObject, flags, hSection);
    if (pRaft)
    {
        // Allocate room for the raft from the global VA space
        if(raftAddress == 0)
        {
            ret = pSection->VASpace().AllocateVASpace(raftSize,
                                minBlockSize,
                                raftAllocation);
        }
        else
        {
            VAM_VIRTUAL_ADDRESS startVA, endVA;
            VAM_VA_SIZE         adjustedSize;
            // Adjust the specified VA and size, so that the allocation is made
            // in line with the bigK alignment granularity requirements for rafts.
            startVA      = ROUND_DOWN(raftAddress, (long long) minBlockSize);
            endVA        = ROUND_UP(raftAddress + raftSize, (long long) minBlockSize) - 1;
            adjustedSize = endVA - startVA + 1;

            ret = pSection->VASpace().AllocateVASpaceWithAddress(startVA,
                                adjustedSize,
                                raftAllocation);
        }

        if (ret == VAM_OK)
        {
            // Initialize the raft's default VA space state
            ret = pRaft->VASpace().Init(
                        raftAllocation.address,           // start of raft's VA space
                        raftAllocation.size,              // size of raft's VA space
                        static_cast<UINT>(minBlockSize)); // minimum alignment granularity for raft VA space is fragment size

            // ... and add to the raft list
            raftList().insertLast(pRaft);
        }

        if (ret == VAM_OK)
        {
            // Bump the total number of rafts
            globalVASpace().incRaftCount();
        }
        else
        {
            FreeRaft(pRaft, false);
            pRaft = NULL;
        }
    }

    return pRaft;
}

VAM_RETURNCODE VamDevice::FreeRaft(
    VamRaft*    pRaft,
    bool        checkForEmpty)
{
    VAM_RETURNCODE  ret = VAM_ERROR;

    VAM_ASSERT(pRaft != NULL);

    VamSection* pSection = static_cast<VamSection *>(pRaft->getParentSection());

    VAM_ASSERT(pSection);

    if (checkForEmpty && pRaft->VASpace().allocationCount())
    {
        // Client is destroying a raft which still has existing suballocations.
        ret = VAM_RAFTNOTEMPTY;
    }
    else
    {
        // Free the blocks belonging to this raft
        for (BlockList::SafeReverseIterator block(pRaft->blockList());
              block != NULL;
              block-- )
        {
            pRaft->FreeBlock(block);
        }

        // Free the chunks from the chunk list
        pRaft->VASpace().FreeChunksFromList();

        if (!usingUIB())
        {
            // Free the VA space which the raft was using inside the global VA Space.
            pSection->VASpace().FreeVASpace(pRaft->VASpace().addr(),   // raft's starting VA
                                        pRaft->VASpace().size());  // raft's size
        }

        // And release the raft object itself
        if (!raftList().isEmpty())
        {
            raftList().remove(pRaft);
        }
        delete pRaft;

        // Decrement the total number of rafts
        globalVASpace().decRaftCount();
        ret = VAM_OK;
    }

    return ret;
}

VAM_RETURNCODE VamDevice::SubAllocOffer(
    VAM_SUBALLOCOFFER_INPUT*        pOffer)
{
    VAM_RETURNCODE  ret = VAM_INVALIDPARAMETERS;
    VamRaft*        pRaft = NULL;

    // Client must have the 'offer' callback defined
    if (m_callbacks.offerVidMem == NULL)
    {
        VAM_ASSERT(FALSE);
        return VAM_ERROR;
    }

    if (AcquireSyncObj() != VAM_OK)
    {
        return VAM_ERROR;
    }

    // Specified suballocation must have a non-0 size to proceed
    if (pOffer->actualSize)
    {
        // Verify the specified raft handle
        pRaft = GetVamRaftObject(pOffer->hRaft);
        if (pRaft && pOffer->actualSize <= pRaft->VASpace().size())
        {
            VAM_ASSERT(raftList().contains(pRaft));

            // Make sure that the VA is inside the specified raft
            if (pRaft->VASpace().IsVAInsideRange( pOffer->virtualAddress ))
            {
                // Find the block where the suballocation resides
                for (BlockList::Iterator block( pRaft->blockList() );
                    block != NULL;
                    block++ )
                {
                    // Check if the specified VA is in block's range
                    if (block->VASpace().IsVAInsideRange( pOffer->virtualAddress ))
                    {
                        ret = VAM_OK;

                        // Attempt to 'offer' the suballocation
                        if (!block->offerList().isEmpty())
                        {
                            // Check against the offer list entries to make sure that
                            // the requested suballocation is not there already.
                            for (OfferList::Iterator offerEntry( block->offerList() );
                                offerEntry != NULL;
                                offerEntry++ )
                            {
                                if (offerEntry->m_addr == pOffer->virtualAddress &&
                                    offerEntry->m_size == pOffer->actualSize)
                                {
                                    // Requested suballocation is already in the offer list
                                    ret = VAM_ERROR;
                                    break;
                                }
                            }
                        }

                        if (ret == VAM_OK)
                        {
                            // And make sure that it is not in the free list
                            for (ChunkList::Iterator chunk( block->VASpace().chunkList() );
                                chunk != NULL;
                                chunk++ )
                            {
                                if (block->VASpace().IsVASpaceInsideChunk(pOffer->virtualAddress,
                                                                          pOffer->actualSize,
                                                                          chunk))
                                {
                                    // Requested suballocation is doesn't exist
                                    ret = VAM_ERROR;
                                    break;
                                }
                            }

                            if (ret == VAM_OK)
                            {
                                // Add the requested suballocation to the offer list
                                VamOfferEntry* pOfferEntry = block->AddToOfferList(pOffer->virtualAddress,
                                                                                   pOffer->actualSize);

                                // Check to see if block is full of offered suballocations. VAM will only offer the
                                // block's vidmem when *all* of the suballocations inside it have been offered.
                                if (block->VASpace().size() == (block->totalOfferSize() + block->VASpace().totalFreeSize()))
                                {
                                    ret = OfferVidMem(block->hVidMem());
                                    if (ret == VAM_OK)
                                    {
                                        block->offered() = true;
                                    }
                                }
                            }
                        }
                        break;
                    }
                }
            }
        }
    }

    ReleaseSyncObj();

    return ret;
}

VAM_RETURNCODE VamDevice::SubAllocReclaim(
    VAM_SUBALLOCRECLAIM_INPUT*      pReclaim)
{
    VAM_RETURNCODE  ret = VAM_INVALIDPARAMETERS;
    VamRaft*        pRaft = NULL;
    VamOfferEntry*  pOfferEntry = NULL;

    // Client must have the 'reclaim' callback defined
    if (m_callbacks.reclaimVidMem == NULL)
    {
        VAM_ASSERT(FALSE);
        return VAM_ERROR;
    }

    if (AcquireSyncObj() != VAM_OK)
    {
        return VAM_ERROR;
    }

    // Specified suballocation must have a non-0 size to proceed
    if (pReclaim->actualSize)
    {
        // Verify the specified raft handle
        pRaft = GetVamRaftObject(pReclaim->hRaft);
        if (pRaft && pReclaim->actualSize <= pRaft->VASpace().size())
        {
            VAM_ASSERT(raftList().contains(pRaft));

            // Make sure that the VA is inside the specified raft
            if (pRaft->VASpace().IsVAInsideRange( pReclaim->virtualAddress ))
            {
                // Find the block where the suballocation resides
                for (BlockList::Iterator block( pRaft->blockList() );
                    block != NULL;
                    block++ )
                {
                    // Check if the specified VA is in block's range
                    if (block->VASpace().IsVAInsideRange( pReclaim->virtualAddress ))
                    {
                        // Locate the offer list entry with the specified VA and size
                        for (OfferList::Iterator offerEntry( block->offerList() );
                            offerEntry != NULL;
                            offerEntry++ )
                        {
                            if (offerEntry->m_addr == pReclaim->virtualAddress &&
                                offerEntry->m_size == pReclaim->actualSize)
                            {
                                // Requested suballocation has been found in the offer list
                                pOfferEntry = offerEntry;
                                break;
                            }
                        }

                        if (pOfferEntry != NULL)
                        {
                            // Is this block's video memory offered?
                            if (block->offered() == true)
                            {
                                // Yes. We need to bring in (i.e. reclaim) the block's video memory
                                ret = ReclaimVidMem(block->hVidMem());
                                if (ret == VAM_OK)
                                {
                                    // We've successfully reclaimed the block, so it's no longer offered
                                    block->offered() = false;
                                }
                            }
                            else
                            {
                                // The block is not offered, but one of the offered suballocations
                                // is being reclaimed. We should just process it right away.
                                ret = VAM_OK;
                            }

                            // Remove the requested suballocation from the offer list
                            block->RemoveFromOfferList(pOfferEntry);
                        }
                        break;
                    }
                }
            }
        }
    }

    ReleaseSyncObj();

    return ret;
}

VamSection* VamDevice::FindSectionVAResideIn(
    VAM_VIRTUAL_ADDRESS             startVA,
    VAM_VA_SIZE                     size)
{
    VamSection* pSection = NULL;

    if (!sectionList().isEmpty())
    {
        // Traverse the section list to find which section the specified VA range resides in.
        for (SectionList::SafeIterator section(sectionList());
                section != NULL;
                section++)
        {
            if (section->VASpace().IsVAInsideRange(startVA) &&              // Start VA address
                section->VASpace().IsVAInsideRange(startVA + size - 1))     // End VA address
            {
                // The section whwere VA range resides is found
                pSection = section;
                break;
            }
        }
    }

    // Returned value NULL corresponds to global space
    return pSection;
}

VOID* VamDevice::AllocSysMem(
    UINT    sizeInBytes)
{
    VOID* pAddr = NULL;

    if (m_callbacks.allocSysMem != NULL)
    {
        pAddr = m_callbacks.allocSysMem(m_hClient, sizeInBytes);
    }

    return pAddr;
}

VAM_RETURNCODE VamDevice::FreeSysMem(
    VOID*   pVirtAddr)
{
    VAM_RETURNCODE ret = VAM_ERROR;

    if (m_callbacks.freeSysMem != NULL)
    {
        ret = m_callbacks.freeSysMem(m_hClient, pVirtAddr);
    }

    return ret;
}

VAM_PTB_HANDLE VamDevice::AllocPTB(
    VAM_VIRTUAL_ADDRESS     PTBBaseAddr,
    VAM_RETURNCODE* const   pRetCode)
{
    VAM_PTB_HANDLE   hPTBAlloc = 0;

    if (m_callbacks.allocPTB != NULL)
    {
        hPTBAlloc = m_callbacks.allocPTB(m_hClient, PTBBaseAddr, pRetCode);
    }
    else
    {
        *pRetCode = VAM_OK;
    }

    return hPTBAlloc;
}

VAM_RETURNCODE VamDevice::FreePTB(
    VAM_CLIENT_HANDLE   hPTBAlloc)
{
    VAM_RETURNCODE ret = VAM_ERROR;

    if (m_callbacks.freePTB != NULL)
    {
        ret = m_callbacks.freePTB(m_hClient, hPTBAlloc);
    }

    return ret;
}

VAM_VIDMEM_HANDLE VamDevice::AllocVidMem(
    VAM_ALLOCVIDMEM_INPUT* pAllocVidMemIn)
{
    VAM_VIDMEM_HANDLE   hVidMem = 0;

    if (m_callbacks.allocVidMem != NULL)
    {
        hVidMem = m_callbacks.allocVidMem(m_hClient, pAllocVidMemIn);
    }

    return hVidMem;
}

VAM_RETURNCODE VamDevice::FreeVidMem(
    VAM_VIDMEM_HANDLE hVidMem)
{
    VAM_RETURNCODE ret = VAM_ERROR;

    if (m_callbacks.freeVidMem != NULL)
    {
        ret = m_callbacks.freeVidMem(m_hClient, hVidMem);
    }

    return ret;
}

VAM_RETURNCODE VamDevice::OfferVidMem(
    VAM_VIDMEM_HANDLE hVidMem)
{
    VAM_RETURNCODE ret = VAM_ERROR;

    if (m_callbacks.offerVidMem != NULL)
    {
        ret = m_callbacks.offerVidMem(m_hClient, hVidMem);
    }

    return ret;
}

VAM_RETURNCODE VamDevice::ReclaimVidMem(
    VAM_VIDMEM_HANDLE hVidMem)
{
    VAM_RETURNCODE ret = VAM_ERROR;

    if (m_callbacks.reclaimVidMem != NULL)
    {
        ret = m_callbacks.reclaimVidMem(m_hClient, hVidMem);
    }

    return ret;
}
