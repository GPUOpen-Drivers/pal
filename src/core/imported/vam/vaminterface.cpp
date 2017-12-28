/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2009-2017 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  vaminterface.cpp
* @brief Contains the VAM interface functions
***************************************************************************************************
*/

#include "vaminterface.h"
#include "vamdevice.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
//                                  Create/Destroy functions
///////////////////////////////////////////////////////////////////////////////////////////////////

/**
***************************************************************************************************
*   VAMCreate
*
*   @brief
*       Creates a VAM object. Must be called before any other interface calls.
*
*   @return
*       Valid VAM handle if successful
***************************************************************************************************
*/
VAM_HANDLE VAM_API VAMCreate (
    VAM_CLIENT_HANDLE           hClient,    ///< Handle of the client associated with this instance of VAM
    const VAM_CREATE_INPUT*     pCreateIn)  ///< Input data structure for the capabilities description
{
    VAM_HANDLE hVam = NULL;
    VamDevice* pObj;

    // Validate the input parameters
    if ((hClient != 0) && (pCreateIn != NULL) && (pCreateIn->size >= sizeof(VAM_CREATE_INPUT)))
    {
        // Validate the supplied callback information
        if ((pCreateIn->callbacks.allocSysMem   != NULL) &&
            (pCreateIn->callbacks.freeSysMem    != NULL) &&
            (pCreateIn->callbacks.allocPTB      != NULL) &&
            (pCreateIn->callbacks.freePTB       != NULL) &&
            (pCreateIn->callbacks.allocVidMem   != NULL) &&
            (pCreateIn->callbacks.freeVidMem    != NULL) &&
            (pCreateIn->callbacks.offerVidMem   != NULL) &&
            (pCreateIn->callbacks.reclaimVidMem != NULL) &&
            ((pCreateIn->hSyncObj == NULL) ||
             ((pCreateIn->hSyncObj                 != NULL) &&
              (pCreateIn->callbacks.acquireSyncObj != NULL) &&
              (pCreateIn->callbacks.releaseSyncObj != NULL))))
        {
            VamObject::SetupSysMemFuncs(pCreateIn->callbacks.allocSysMem,
                                        pCreateIn->callbacks.freeSysMem);

            pObj = VamDevice::Create(hClient, pCreateIn);
            hVam = static_cast<VAM_HANDLE>(pObj);
        }
    }

    return hVam;
}

/**
***************************************************************************************************
*   VAMDestroy
*
*   @brief
*       Destroys an existing VAM object. Frees all internally allocated resources.
*
*   @return
*       VAM_OK if successful
***************************************************************************************************
*/
VAM_RETURNCODE VAM_API VAMDestroy (
    VAM_HANDLE                  hVam)       ///< Input handle of the VAM instance to be destroyed
{
    VAM_RETURNCODE ret;

    VamDevice* pObj = VamDevice::GetVamDeviceObject(hVam);

    // Validate the provided object handle.
    if ( pObj != NULL )
    {
        // Destroy the Vam device object
        ret = pObj->Destroy();
    }
    else
    {
        ret = VAM_INVALIDPARAMETERS;
    }

    return ret;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//                     Public VAM APIs for managing Global Virtual Address space
///////////////////////////////////////////////////////////////////////////////////////////////////

/**
***************************************************************************************************
*   VAMAlloc
*
*   @brief
*       Creates a normal allocation in the global VA space.
*       A preferred virtual address may be optionally specified.
*
*   @return
*       VAM_OK if successful
***************************************************************************************************
*/
VAM_RETURNCODE VAM_API VAMAlloc (
    VAM_HANDLE          hVam,       ///< Input handle of the VAM instance.
    VAM_ALLOC_INPUT*    pAllocIn,   ///< Input data structure for the allocation request
    VAM_ALLOC_OUTPUT*   pAllocOut)  ///< Output data structure filled in by the function
{
    VAM_RETURNCODE ret;

    VamDevice* pObj = VamDevice::GetVamDeviceObject(hVam);

    if ((pObj != NULL) && (pAllocIn != NULL) && (pAllocOut != NULL))
    {
        ret = pObj->RegularAllocateVASpace(pAllocIn, pAllocOut);
    }
    else
    {
        ret = VAM_INVALIDPARAMETERS;
    }

    return ret;
}

/**
***************************************************************************************************
*   VAMFree
*
*   @brief
*       Frees a normal global allocation that is no longer in use
*
*   @return
*       VAM_OK if successful
***************************************************************************************************
*/
VAM_RETURNCODE VAM_API VAMFree (
    VAM_HANDLE          hVam,       ///< Input handle of the VAM instance
    VAM_FREE_INPUT*     pFreeIn)    ///< Input data structure describing the allocation to be freed
{
    VAM_RETURNCODE ret;

    VamDevice* pObj = VamDevice::GetVamDeviceObject(hVam);

    if ((pObj != NULL) && (pFreeIn != NULL))
    {
        ret = pObj->RegularFreeVASpace(pFreeIn);
    }
    else
    {
        ret = VAM_INVALIDPARAMETERS;
    }

    return ret;
}

/**
***************************************************************************************************
*   VAMQueryGlobalAllocStatus
*
*   @brief
*       Allows the client to query the status of all allocations that are present in the global
*       VA space.
*
*   @return
*       VAM_OK if successful
***************************************************************************************************
*/
VAM_RETURNCODE VAM_API VAMQueryGlobalAllocStatus (
    VAM_HANDLE                      hVam,                   ///< Input handle of the VAM instance
    VAM_GLOBALALLOCSTATUS_OUTPUT*   pGlobalAllocStatusOut)  ///< Output data structure containing global
                                                            ///  allocation status details
{
    VAM_RETURNCODE ret;

    VamDevice* pObj = VamDevice::GetVamDeviceObject(hVam);

    if ((pObj != NULL) && (pGlobalAllocStatusOut != NULL))
    {
        ret = pObj->QueryGlobalAllocStatus(pGlobalAllocStatusOut);
    }
    else
    {
        ret = VAM_INVALIDPARAMETERS;
    }

    return ret;
}

/**
***************************************************************************************************
*   VAMExcludeRange
*
*   @brief
*       Excludes a specified VA range from ever being used by subsequent VAMAlloc, VAMCreateRaft or
*       VAMExcludeRange calls.
*
*   @return
*       VAM_OK if successful
***************************************************************************************************
*/
VAM_RETURNCODE VAM_API VAMExcludeRange (
    VAM_HANDLE              hVam,               ///< Input handle of the VAM instance
    VAM_EXCLUDERANGE_INPUT* pExcludeRangeIn)    ///< Input data structure of VA range to exclude
{
    VAM_RETURNCODE ret;

    VamDevice* pObj = VamDevice::GetVamDeviceObject(hVam);

    if ((pObj != NULL) && (pExcludeRangeIn != NULL))
    {
        ret = pObj->ExcludeRange(pExcludeRangeIn);
    }
    else
    {
        ret = VAM_INVALIDPARAMETERS;
    }

    return ret;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//                              Public VAM APIs for managing Section
///////////////////////////////////////////////////////////////////////////////////////////////////

/**
***************************************************************************************************
*   VAMCreateSection
*
*   @brief
*       Creates a section by reserving contiguous range of VA space that does not overlap with any
*       normal allocations, rafts, excluded ranges or other sections.
*       Subsequent allocations may be performed either in global VA space or in created sections.
*
*   @return
*       Valid VAM section handle if successful
***************************************************************************************************
*/
VAM_SECTION_HANDLE VAM_API VAMCreateSection (
    VAM_HANDLE                      hVam,                       ///< Input handle of the VAM instance
    VAM_CREATESECTION_INPUT*        pCreateSectionIn)           ///< Input data structure for creating the section
{
    VAM_SECTION_HANDLE hSection = NULL;

    VamDevice* pObj = VamDevice::GetVamDeviceObject(hVam);

    if ((pObj != NULL) && (pCreateSectionIn != NULL))
    {
        hSection = pObj->CreateSection( pCreateSectionIn->sectionSizeInBytes,
                                        pCreateSectionIn->clientObject,
                                        pCreateSectionIn->flags,
                                        pCreateSectionIn->sectionAddress);
    }

    return hSection;
}

/**
***************************************************************************************************
*   VAMDestroySection
*
*   @brief
*       Destroys a currently existing section
*
*   @return
*       VAM_OK if successful
***************************************************************************************************
*/
VAM_RETURNCODE VAM_API VAMDestroySection (
    VAM_HANDLE                      hVam,                       ///< Input handle of the VAM instance
    VAM_SECTION_HANDLE              hSection)                   ///< Handle of the section to be freed
{
    VAM_RETURNCODE ret;

    VamDevice* pObj = VamDevice::GetVamDeviceObject(hVam);

    if (pObj != NULL)
    {
        ret = pObj->DestroySection(hSection);
    }
    else
    {
        ret = VAM_INVALIDPARAMETERS;
    }

    return ret;
}

/**
***************************************************************************************************
*   VAMQuerySectionAllocStatus
*
*   @brief
*       Allows the client to query the internal status of a section
*
*   @return
*       VAM_OK if successful
***************************************************************************************************
*/
VAM_RETURNCODE VAM_API VAMQuerySectionAllocStatus (
    VAM_HANDLE                      hVam,                       ///< Input handle of the VAM instance
    VAM_SECTION_HANDLE              hSection,                   ///< Handle of the section to query from
    VAM_SECTIONALLOCSTATUS_OUTPUT*  pSectionAllocStatusOut)     ///< Output data structure containing section status details
{
    VAM_RETURNCODE ret;

    VamDevice* pObj = VamDevice::GetVamDeviceObject(hVam);

    if ((pObj != NULL) && (pSectionAllocStatusOut != NULL))
    {
        ret = pObj->QuerySectionAllocStatus(hSection, pSectionAllocStatusOut);
    }
    else
    {
        ret = VAM_INVALIDPARAMETERS;
    }

    return ret;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//                              Public VAM APIs for managing Sub-allocations
///////////////////////////////////////////////////////////////////////////////////////////////////

/**
***************************************************************************************************
*   VAMCreateRaft
*
*   @brief
*       Creates a raft by reserving contiguous range of VA space that does not overlap with any
*       normal allocations, excluded ranges or other rafts.
*       Suballocations may subsequently be performed against a raft.
*
*   @return
*       Valid VAM raft handle if successful
***************************************************************************************************
*/
VAM_RAFT_HANDLE VAM_API VAMCreateRaft (
    VAM_HANDLE              hVam,           ///< Input handle of the VAM instance
    VAM_CREATERAFT_INPUT*   pCreateRaftIn)  ///< Input data structure for creating the raft
{
    VAM_RAFT_HANDLE hRaft = NULL;

    VamDevice* pObj = VamDevice::GetVamDeviceObject(hVam);

    if ((pObj != NULL) && (pCreateRaftIn != NULL))
    {
        hRaft = pObj->CreateRaft( pCreateRaftIn->hSection,
                                  pCreateRaftIn->raftSizeInBytes,
                                  pCreateRaftIn->minBlockSizeInBytes,
                                  pCreateRaftIn->clientObject,
                                  pCreateRaftIn->flags,
                                  pCreateRaftIn->raftAddress);
    }

    return hRaft;
}

/**
***************************************************************************************************
*   VAMDestroyRaft
*
*   @brief
*       Destroys a currently existing raft
*
*   @return
*       VAM_OK if successful
***************************************************************************************************
*/
VAM_RETURNCODE VAM_API VAMDestroyRaft (
    VAM_HANDLE              hVam,           ///< Input handle of the VAM instance
    VAM_RAFT_HANDLE         hRaft)          ///< Handle of the raft to be freed
{
    VAM_RETURNCODE ret;

    VamDevice* pObj = VamDevice::GetVamDeviceObject(hVam);

    if (pObj != NULL)
    {
        ret = pObj->DestroyRaft(hRaft);
    }
    else
    {
        ret = VAM_INVALIDPARAMETERS;
    }

    return ret;
}

/**
***************************************************************************************************
*   VAMTrim
*
*   @brief
*       Trim VAMDevice to free as much memory as possible
*
*   @return
*       VAM_OK if successful
***************************************************************************************************
*/
VAM_RETURNCODE VAM_API VAMTrim (
    VAM_HANDLE              hVam,           ///< Input handle of the VAM instance
    VAM_TRIM_FLAGS          flags           ///< Flags to control what memory/resources to trim
)
{
    VAM_RETURNCODE ret;

    VamDevice* pObj = VamDevice::GetVamDeviceObject(hVam);

    if (pObj != NULL)
    {
        ret = pObj->Trim(flags);
    }
    else
    {
        ret = VAM_INVALIDPARAMETERS;
    }

    return ret;
}

/**
***************************************************************************************************
*   VAMGetRaftAllocationInfo
*
*   @brief
*       Returns raft address and size
*
*   @return
*       VAM_OK if successful
***************************************************************************************************
*/
VAM_RETURNCODE VAM_API VAMGetRaftAllocationInfo (
    VAM_HANDLE              hVam,           ///< Input handle of the VAM instance
    VAM_RAFT_HANDLE         hRaft,           ///< Handle of the raft to be freed
    VAM_ALLOC_OUTPUT*       pAllocOut)
{
    VAM_RETURNCODE ret;

    VamDevice* pObj = VamDevice::GetVamDeviceObject(hVam);

    if (pObj != NULL)
    {
        ret = pObj->GetRaftAllocationInfo(hRaft, pAllocOut);
    }
    else
    {
        ret = VAM_INVALIDPARAMETERS;
    }

    return ret;
}

/**
***************************************************************************************************
*   VAMSubAlloc
*
*   @brief
*       Performs suballocations from an existing raft
*
*   @return
*       VAM_OK if successful
***************************************************************************************************
*/
VAM_RETURNCODE VAM_API VAMSubAlloc (
    VAM_HANDLE              hVam,           ///< Input handle of the VAM instance
    VAM_SUBALLOC_INPUT*     pSubAllocIn,    ///< Input data structure for the suballocation request
    VAM_SUBALLOC_OUTPUT*    pSubAllocOut)   ///< Output data structure filled in by this function
{
    VAM_RETURNCODE ret;

    VamDevice* pObj = VamDevice::GetVamDeviceObject(hVam);

    if ((pObj != NULL) && (pSubAllocIn != NULL) && (pSubAllocOut != NULL))
    {
        ret = pObj->SubAllocateVASpace( pSubAllocIn, pSubAllocOut );
    }
    else
    {
        ret = VAM_INVALIDPARAMETERS;
    }

    return ret;
}

/**
***************************************************************************************************
*   VAMSubFree
*
*   @brief
*       Frees a suballocation that is no longer in use
*
*   @return
*       VAM_OK if successful
***************************************************************************************************
*/
VAM_RETURNCODE VAM_API VAMSubFree (
    VAM_HANDLE          hVam,       ///< Input handle of the VAM instance
    VAM_SUBFREE_INPUT*  pSubFreeIn) ///< Input data structure describing the suballocation to be freed
{
    VAM_RETURNCODE ret;

    VamDevice* pObj = VamDevice::GetVamDeviceObject(hVam);

    if ((pObj != NULL) && (pSubFreeIn != NULL))
    {
        ret = pObj->SubFreeVASpace( pSubFreeIn );
    }
    else
    {
        ret = VAM_INVALIDPARAMETERS;
    }

    return ret;
}

/**
***************************************************************************************************
*   VAMQuerySubAllocStatus
*
*   @brief
*       Allows the client to query the internal status of a raft
*
*   @return
*       VAM_OK if successful
***************************************************************************************************
*/
VAM_RETURNCODE VAM_API VAMQuerySubAllocStatus (
    VAM_HANDLE                  hVam,               ///< Input handle of the VAM instance
    VAM_RAFT_HANDLE             hRaft,              ///< Handle of the raft to query from
    VAM_SUBALLOCSTATUS_OUTPUT*  pSubAllocStatusOut) ///< Output data structure containing raft status details
{
    VAM_RETURNCODE ret;

    VamDevice* pObj = VamDevice::GetVamDeviceObject(hVam);

    if ((pObj != NULL) && (pSubAllocStatusOut != NULL))
    {
        ret = pObj->QuerySubAllocStatus(hRaft, pSubAllocStatusOut);
    }
    else
    {
        ret = VAM_INVALIDPARAMETERS;
    }

    return ret;
}

/**
***************************************************************************************************
*   VAMQueryBlockStatus
*
*   @brief
*       Allows the client to query the internal status of a block
*
*   @return
*       VAM_OK if successful
***************************************************************************************************
*/
VAM_RETURNCODE VAM_API VAMQueryBlockStatus (
    VAM_HANDLE              hVam,               ///< Input handle of the VAM instance
    VAM_RAFT_HANDLE         hRaft,              ///< Handle of the raft to query from
    VAM_VIDMEM_HANDLE       hVidMem,            ///< Handle of the VidMem object
    VAM_BLOCKSTATUS_OUTPUT* pQueryStatusOut)    ///< Output data structure containing block status
                                                ///< details
{
    VAM_RETURNCODE ret;

    VamDevice* pObj = VamDevice::GetVamDeviceObject(hVam);
    if ((pObj != NULL) && (pQueryStatusOut != NULL))
    {
        ret = pObj->QueryBlockStatus(hRaft, hVidMem, pQueryStatusOut);
    }
    else
    {
        ret = VAM_INVALIDPARAMETERS;
    }

    return ret;
}

/**
***************************************************************************************************
*   VAMSubAllocOffer
*
*   @brief
*       Informs VAM of a specific suballocation which is to be offered
*
*   @return
*       VAM_OK if successful
***************************************************************************************************
*/
VAM_RETURNCODE VAM_API VAMSubAllocOffer (
    VAM_HANDLE                  hVam,       ///< Input handle of the VAM instance
    VAM_SUBALLOCOFFER_INPUT*    pOfferIn)   ///< Input data structure describing the suballocation to be offered
{
    VAM_RETURNCODE ret;

    VamDevice* pObj = VamDevice::GetVamDeviceObject(hVam);

    if ((pObj != NULL) && (pOfferIn != NULL))
    {
        ret = pObj->SubAllocOffer( pOfferIn );
    }
    else
    {
        ret = VAM_INVALIDPARAMETERS;
    }

    return ret;
}

/**
***************************************************************************************************
*   VAMSubAllocReclaim
*
*   @brief
*       Informs VAM of a specific suballocation which is to be reclaimed
*
*   @return
*       VAM_OK if successful
***************************************************************************************************
*/
VAM_RETURNCODE VAM_API VAMSubAllocReclaim (
    VAM_HANDLE                  hVam,       ///< Input handle of the VAM instance
    VAM_SUBALLOCRECLAIM_INPUT*  pReclaimIn) ///< Pointer to the input data structure describing the
                                            ///  suballocation to be reclaimed.
{
    VAM_RETURNCODE ret;

    VamDevice* pObj = VamDevice::GetVamDeviceObject(hVam);

    if ((pObj != NULL) && (pReclaimIn != NULL))
    {
        ret = pObj->SubAllocReclaim( pReclaimIn );
    }
    else
    {
        ret = VAM_INVALIDPARAMETERS;
    }

    return ret;
}
