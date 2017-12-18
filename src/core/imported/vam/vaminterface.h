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
* @file  vaminterface.h
* @brief Contains the VAM interfaces declaration and parameter defines
***************************************************************************************************
*/
#ifndef __VAMINTERFACE_H__
#define __VAMINTERFACE_H__

#include "vamtypes.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/**
***************************************************************************************************
* VAM Version
*    Date           Version  Description
*    12 May 2009    1.0      Initial version
*
* @note Minor version changes may add functionality but not break backward compatibility.
***************************************************************************************************
*/

#define VAM_VERSION_MAJOR 1
#define VAM_VERSION_MINOR 2
#define __VAM_VERSION ((VAM_VERSION_MAJOR * 0x10000) + VAM_VERSION_MINOR)

/**
***************************************************************************************************
* VAM_VERSION
*
*   @brief
*       This structure is used to pass VAM version information from the client
***************************************************************************************************
*/

typedef union _VAM_VERSION
{
    struct
    {
        UINT    minor   : 16;   ///< Minor version number
        UINT    major   : 16;   ///< Major version number
    };
    UINT value;

} VAM_VERSION;

///////////////////////////////////////////////////////////////////////////////////////////////////
//                                      Callback functions
///////////////////////////////////////////////////////////////////////////////////////////////////

/**
***************************************************************************************************
* VAM_NEEDPTB
*   @brief
*       Callback function to check if PTB management is needed. Returns VAM_OK if PTB management is needed.
***************************************************************************************************
*/
typedef VAM_RETURNCODE (VAM_API* VAM_NEEDPTB)();

/**
***************************************************************************************************
* VAM_ALLOCSYSMEM
*   @brief
*       Allocate system memory callback function. Returns valid pointer on success.
***************************************************************************************************
*/
typedef VOID* (VAM_API* VAM_ALLOCSYSMEM)(
    VAM_CLIENT_HANDLE       hClient,        ///< Client handle
    UINT                    sizeInBytes     ///< System memory allocation size in bytes
);

/**
***************************************************************************************************
* VAM_FREESYSMEM
*   @brief
*       Free system memory callback function. Returns VAM_OK on success.
***************************************************************************************************
*/
typedef VAM_RETURNCODE (VAM_API* VAM_FREESYSMEM)(
    VAM_CLIENT_HANDLE       hClient,        ///< Client handle
    VOID*                   pVirtAddr       ///< VA of system memory allocation to free
);

/**
***************************************************************************************************
* VAM_ALLOCPTB
*   @brief
*       Allocate PTB callback function. Returns valid PTB allocation handle on success.
***************************************************************************************************
*/
typedef VAM_PTB_HANDLE (VAM_API* VAM_ALLOCPTB)(
    VAM_CLIENT_HANDLE       hClient,        ///< Client handle
    VAM_VIRTUAL_ADDRESS     PTBBaseAddr     ///< Base virtual address to be mapped by the PTB
);

/**
***************************************************************************************************
* VAM_FREEPTB
*   @brief
*       Free PTB callback function. Returns VAM_OK on success.
***************************************************************************************************
*/
typedef VAM_RETURNCODE (VAM_API* VAM_FREEPTB)(
    VAM_CLIENT_HANDLE       hClient,        ///< Client handle
    VAM_PTB_HANDLE          hPTBAlloc       ///< Handle of PTB allocation to free
);

/**
***************************************************************************************************
*   VAM_ALLOCVIDMEM_INPUT
*
*   @brief
*       Input structure for allocVidMem callback function
***************************************************************************************************
*/
typedef struct _VAM_ALLOCVIDMEM_INPUT
{
    VAM_CLIENT_OBJECT       clientObject;   ///< Opaque client object
    VAM_VA_SIZE             sizeInBytes;    ///< Size in bytes of video memory to allocate
    UINT                    alignment;      ///< Required alignment of the allocation
    VAM_VIRTUAL_ADDRESS     vidMemVirtAddr; ///< Starting VA of the video memory allocation

} VAM_ALLOCVIDMEM_INPUT;

/**
***************************************************************************************************
* VAM_ALLOCVIDMEM
*   @brief
*       Allocate video memory callback function. Returns valid vidmem allocation handle on success.
***************************************************************************************************
*/
typedef VAM_VIDMEM_HANDLE (VAM_API* VAM_ALLOCVIDMEM)(
    VAM_CLIENT_HANDLE       hClient,        ///< Client handle
    VAM_ALLOCVIDMEM_INPUT*  pAllocVidMemIn  ///< Input data structure
);

/**
***************************************************************************************************
* VAM_FREEVIDMEM
*   @brief
*       Free video memory callback function. Returns VAM_OK on success.
***************************************************************************************************
*/
typedef VAM_RETURNCODE (VAM_API* VAM_FREEVIDMEM)(
    VAM_CLIENT_HANDLE       hClient,        ///< Client handle
    VAM_VIDMEM_HANDLE       hVidMem         ///< Handle of video memory allocation to free
);

/**
***************************************************************************************************
*   VAM_ACQSYNCOBJ_INPUT
*
*   @brief
*       Input structure for acquireSyncObj callback function
***************************************************************************************************
*/
typedef struct _VAM_ACQSYNCOBJ_INPUT
{
    VAM_SYNCOBJECT_HANDLE   hSyncObj;       ///< Handle of sync object
    UINT                    timeout;        ///< Time-out interval in ms

} VAM_ACQSYNCOBJ_INPUT;

/**
***************************************************************************************************
* VAM_ACQUIRESYNCOBJECT
*   @brief
*       Acquire Sync Object callback function. Returns VAM_OK on success.
***************************************************************************************************
*/
typedef VAM_RETURNCODE (VAM_API* VAM_ACQUIRESYNCOBJECT)(
    VAM_CLIENT_HANDLE       hClient,        ///< Client handle
    VAM_ACQSYNCOBJ_INPUT*   pAcqSyncObjIn   ///< Input data structure
);

/**
***************************************************************************************************
* VAM_RELEASESYNCOBJECT
*   @brief
*       Release Sync Object callback function.
***************************************************************************************************
*/
typedef VOID (VAM_API* VAM_RELEASESYNCOBJECT)(
    VAM_CLIENT_HANDLE       hClient,        ///< Client handle
    VAM_SYNCOBJECT_HANDLE   hSyncObj        ///< Handle of sync object
);

/**
***************************************************************************************************
* VAM_OFFERVIDMEM
*   @brief
*       Offer video memory callback function. Returns VAM_OK on success.
***************************************************************************************************
*/
typedef VAM_RETURNCODE (VAM_API* VAM_OFFERVIDMEM)(
    VAM_CLIENT_HANDLE       hClient,        ///< Client handle
    VAM_VIDMEM_HANDLE       hVidMem         ///< Handle of video memory allocation to offer
);

/**
***************************************************************************************************
* VAM_RECLAIMVIDMEM
*   @brief
*       Reclaim video memory callback function. Returns VAM_OK on success.
***************************************************************************************************
*/
typedef VAM_RETURNCODE (VAM_API* VAM_RECLAIMVIDMEM)(
    VAM_CLIENT_HANDLE       hClient,        ///< Client handle
    VAM_VIDMEM_HANDLE       hVidMem         ///< Handle of video memory allocation to reclaim
);

/**
***************************************************************************************************
* VAM_CALLBACKS
*
*   @brief
*       List of all callbacks used by VAM.
***************************************************************************************************
*/
typedef struct _VAM_CALLBACKS
{
    VAM_ALLOCSYSMEM         allocSysMem;    ///< Function to allocate system memory
    VAM_FREESYSMEM          freeSysMem;     ///< Function to free system memory
    VAM_ALLOCPTB            allocPTB;       ///< Function to allocate a PTB
    VAM_FREEPTB             freePTB;        ///< Function to free a PTB
    VAM_ALLOCVIDMEM         allocVidMem;    ///< Function to allocate video memory
    VAM_FREEVIDMEM          freeVidMem;     ///< Function to free video memory
    VAM_ACQUIRESYNCOBJECT   acquireSyncObj; ///< Function to acquire sync object
    VAM_RELEASESYNCOBJECT   releaseSyncObj; ///< Function to release sync object
    VAM_OFFERVIDMEM         offerVidMem;    ///< Function to offer video memory
    VAM_RECLAIMVIDMEM       reclaimVidMem;  ///< Function to reclaim video memory
    VAM_NEEDPTB             needPTB;        ///< Function to check if PTB management is needed
} VAM_CALLBACKS;

///////////////////////////////////////////////////////////////////////////////////////////////////
//                                  Create/Destroy functions
///////////////////////////////////////////////////////////////////////////////////////////////////

/**
***************************************************************************************************
* VAM_CREATE_FLAGS
***************************************************************************************************
*/
typedef union _VAM_CREATE_FLAGS
{
    struct
    {
        UINT useUIB         :  1;           ///< Unmap Info Buffer (UIB) will be used by the client
        UINT reserved       : 31;
    };
    UINT value;

} VAM_CREATE_FLAGS;

/**
***************************************************************************************************
* VAM_CREATE_INPUT
*
*   @brief
*       Parameters used to create a VAM instance object. Caller must populate all structure members.
***************************************************************************************************
*/
typedef struct _VAM_CREATE_INPUT
{
    UINT                    size;           ///< Size of this structure in bytes
    VAM_VERSION             version;        ///< VAM version number
    VAM_CALLBACKS           callbacks;      ///< Supported callbacks
    VAM_VIRTUAL_ADDRESS     VARangeStart;   ///< VA range starting address (4KB aligned)
    VAM_VIRTUAL_ADDRESS     VARangeEnd;     ///< VA range ending address (4KB aligned)
    UINT                    PTBSize;        ///< Size of a PTB in bytes
    UINT                    bigKSize;       ///< Size of a big-K fragment in bytes
    VAM_SYNCOBJECT_HANDLE   hSyncObj;       ///< Handle of sync object (0=client is thread-safe)
    VAM_CREATE_FLAGS        flags;          ///< Create Descriptor Flags
    UINT                    uibVersion;     ///< UIB format version number
    UINT                    gpuCount;       ///< used for MGPU configurations
    UINT                    reserved[10];   ///< Reserved for future expansion

} VAM_CREATE_INPUT;

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
    const VAM_CREATE_INPUT*     pCreateIn   ///< Input data structure for the capabilities description
);

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
    VAM_HANDLE                  hVam        ///< Input handle of the VAM instance to be destroyed
);

///////////////////////////////////////////////////////////////////////////////////////////////////
//                     Public VAM APIs for managing Global Virtual Address space
///////////////////////////////////////////////////////////////////////////////////////////////////

/**
***************************************************************************************************
*   VAM_ALLOC_FLAGS
*
*   @brief
*       Flags for VAMAlloc allocation function
***************************************************************************************************
*/
typedef union _VAM_ALLOC_FLAGS
{
    struct
    {
        UINT useFragment        : 1;    ///< Fragment(s) to be used in allocation. VAM will align and
                                        ///  size the allocation in multiples of 'big-K' bytes.
        UINT beyondRequestedVa  : 1;    ///< Allocate VA beyond the desired VA, if possible
        UINT reserved           : 30;
    };
    UINT value;

} VAM_ALLOC_FLAGS;

/**
***************************************************************************************************
*   VAM_ALLOC_INPUT
*
*   @brief
*       Input structure for VAMAlloc
***************************************************************************************************
*/
typedef struct _VAM_ALLOC_INPUT
{
    VAM_VA_SIZE             sizeInBytes;        ///< Size in bytes to be allocated
    UINT                    alignment;          ///< Required POW2 alignment of the allocation
    VAM_ALLOC_FLAGS         flags;              ///< Allocation flags
    VAM_VIRTUAL_ADDRESS     virtualAddress;     ///< Optional desired VA for the allocation
    DWORD                   gpuMask;            ///< mask for multi GPU allocations (default is zero)
    VAM_SECTION_HANDLE      hSection;           ///< Handle of section to allocate from (NULL for global space)

} VAM_ALLOC_INPUT;

/**
***************************************************************************************************
*   VAM_ALLOC_OUTPUT
*
*   @brief
*       Output structure for VAMAlloc
***************************************************************************************************
*/
typedef struct _VAM_ALLOC_OUTPUT
{
    VAM_VIRTUAL_ADDRESS     virtualAddress;     ///< Assigned VA of the allocation
    VAM_VA_SIZE             actualSize;         ///< Actual size of the allocation
    VAM_ALLOCATION_HANDLE   hVaAlloc;           ///< Sys mem allocation tracking the gpuMask for multi GPU configurations

} VAM_ALLOC_OUTPUT;

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
    VAM_ALLOC_OUTPUT*   pAllocOut   ///< Output data structure filled in by the function
);

/**
***************************************************************************************************
*   VAM_FREE_INPUT
*
*   @brief
*       Input structure for VAMFree
***************************************************************************************************
*/
typedef struct _VAM_FREE_INPUT
{
    VAM_VIRTUAL_ADDRESS     virtualAddress;     ///< VA of the allocation to be freed
    VAM_VA_SIZE             actualSize;         ///< Actual size of allocation to be freed
    VAM_ALLOCATION_HANDLE   hVaAlloc;           ///< Handle for the allocation tracker used for MGPU
    DWORD                   gpuMask;            ///< which GPU the input applies to (only used for MGPU)
    VAM_SECTION_HANDLE      hSection;           ///< Handle of section to free to (NULL for global space)

} VAM_FREE_INPUT;

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
    VAM_FREE_INPUT*     pFreeIn     ///< Input data structure describing the allocation to be freed
);

/**
***************************************************************************************************
*   VAM_GLOBALALLOCSTATUS_OUTPUT
*
*   @brief
*       Output structure for VAMQueryGlobalAllocStatus
***************************************************************************************************
*/
typedef struct _VAM_GLOBALALLOCSTATUS_OUTPUT
{
    VAM_VA_SIZE     totalSizeInBytes;       ///< Total VA space size in bytes
    UINT            numberOfAllocs;         ///< Total number of existing allocations
    UINT            numberOfRafts;          ///< Total number of rafts
    UINT            numberOfSections;       ///< Total number of sections
    UINT            numberOfExcludedRanges; ///< Total number of excluded ranges
    VAM_VA_SIZE     usedSizeInBytes;        ///< Total allocated size in bytes
    VAM_VA_SIZE     freeSizeInBytes;        ///< Total free size in bytes

} VAM_GLOBALALLOCSTATUS_OUTPUT;

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
    VAM_GLOBALALLOCSTATUS_OUTPUT*   pGlobalAllocStatusOut   ///< Output data structure containing global
                                                            ///  allocation status details
);

/**
***************************************************************************************************
*   VAM_EXCLUDERANGE_INPUT
*
*   @brief
*       Input structure for VAMExcludeRange
***************************************************************************************************
*/
typedef struct _VAM_EXCLUDERANGE_INPUT
{
    VAM_VIRTUAL_ADDRESS virtualAddress;     ///< Required starting VA of range to exclude
    VAM_VA_SIZE         sizeInBytes;        ///< Size in bytes of range to exclude

} VAM_EXCLUDERANGE_INPUT;

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
    VAM_HANDLE              hVam,           ///< Input handle of the VAM instance
    VAM_EXCLUDERANGE_INPUT* pExcludeRangeIn ///< Input data structure of VA range to exclude
);

///////////////////////////////////////////////////////////////////////////////////////////////////
//                              Public VAM APIs for managing Section
///////////////////////////////////////////////////////////////////////////////////////////////////

/**
***************************************************************************************************
*   VAM_CREATESECTION_FLAGS
*
*   @brief
*       Flags for VAMCreateSection
***************************************************************************************************
*/
typedef union _VAM_CREATESECTION_FLAGS
{
    struct
    {
        UINT reserved           : 32;               ///< For future expansion
    };
    UINT value;

} VAM_CREATESECTION_FLAGS;

/**
***************************************************************************************************
*   VAM_CREATESECTION_INPUT
*
*   @brief
*       Input structure for VAMCreateSection
***************************************************************************************************
*/
typedef struct _VAM_CREATESECTION_INPUT
{
    VAM_VA_SIZE                 sectionSizeInBytes;     ///< Size in bytes of VA space to reserve for section
    VAM_CLIENT_OBJECT           clientObject;           ///< Opaque client object
    VAM_CREATESECTION_FLAGS     flags;                  ///< Section creation flags
    VAM_VIRTUAL_ADDRESS         sectionAddress;         ///< optional virtual address for the section

} VAM_CREATESECTION_INPUT;

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
    VAM_CREATESECTION_INPUT*        pCreateSectionIn            ///< Input data structure for creating the section
);

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
    VAM_SECTION_HANDLE              hSection                    ///< Handle of the section to be freed
);

/**
***************************************************************************************************
*   VAM_SECTIONALLOCSTATUS_OUTPUT
*
*   @brief
*       Output structure for VAMQuerySectionAllocStatus
***************************************************************************************************
*/
typedef struct _VAM_SECTIONALLOCSTATUS_OUTPUT
{
    VAM_VA_SIZE         sectionSizeInBytes;     ///< Section size in bytes
    UINT                numberOfAllocs;         ///< Number of existing allocations in section
    VAM_VA_SIZE         usedSizeInBytes;        ///< Allocated size in bytes
    VAM_VA_SIZE         freeSizeInBytes;        ///< Free size in bytes
    VAM_VIRTUAL_ADDRESS sectionAddress;         ///< Section base address

} VAM_SECTIONALLOCSTATUS_OUTPUT;

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
    VAM_SECTIONALLOCSTATUS_OUTPUT*  pSectionAllocStatusOut      ///< Output data structure containing section status details
);

///////////////////////////////////////////////////////////////////////////////////////////////////
//                              Public VAM APIs for managing Sub-allocations
///////////////////////////////////////////////////////////////////////////////////////////////////

/**
***************************************************************************************************
*   VAM_CREATERAFT_FLAGS
*
*   @brief
*       Flags for VAMCreateRaft
***************************************************************************************************
*/
typedef union _VAM_CREATERAFT_FLAGS
{
    struct
    {
        UINT keepBlocksResident : 1;    ///< Do not release blocks if all suballocations freed from raft.
                                        ///  Blocks will be released by VAM when the raft is destroyed.
        UINT reserved           : 31;
    };
    UINT value;

} VAM_CREATERAFT_FLAGS;

/**
***************************************************************************************************
*   VAM_CREATERAFT_INPUT
*
*   @brief
*       Input structure for VAMCreateRaft
***************************************************************************************************
*/
typedef struct _VAM_CREATERAFT_INPUT
{
    VAM_VA_SIZE             raftSizeInBytes;        ///< Size in bytes of VA space to reserve for raft
    VAM_VA_SIZE             minBlockSizeInBytes;    ///< Minimum block size in bytes
    VAM_CLIENT_OBJECT       clientObject;           ///< Opaque client object
    VAM_CREATERAFT_FLAGS    flags;                  ///< Raft creation flags
    VAM_VIRTUAL_ADDRESS     raftAddress;            ///< optional virtual address for the raft
    VAM_SECTION_HANDLE      hSection;               ///< Section where raft is created from
} VAM_CREATERAFT_INPUT;

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
    VAM_CREATERAFT_INPUT*   pCreateRaftIn   ///< Input data structure for creating the raft
);

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
    VAM_RAFT_HANDLE         hRaft           ///< Handle of the raft to be freed
);

/**
***************************************************************************************************
*   VAM_TRIM_FLAGS
*
*   @brief
*       Flags for VAMTrim
***************************************************************************************************
*/
typedef union _VAM_TRIM_FLAGS
{
    struct
    {
        UINT trimEmptyBlocks        : 1;      ///< Destroy any empty block of a given raft
        UINT trimOfferedBlocks      : 1;      ///< free all blocks that have been offered to OS
        UINT trimEmptyPageTables    : 1;      ///< Destroy any page tables that no longer have any valid mappings
        UINT reserved               : 29;
    };
    UINT value;

} VAM_TRIM_FLAGS;

/**
***************************************************************************************************
*   VAMTrim
*
*   @brief
*       Clean/Trim any temporary storage and/or unused resources
*
*   @return
*       VAM_OK if successful
***************************************************************************************************
*/
VAM_RETURNCODE VAM_API VAMTrim (
    VAM_HANDLE      hVam,           ///< Input handle of the VAM instance
    VAM_TRIM_FLAGS  flags           ///< Flags that controls how Raft should be cleaned
);

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
    VAM_ALLOC_OUTPUT*       pAllocOut
);

/**
***************************************************************************************************
*   VAM_SUBALLOC_INPUT
*
*   @brief
*       Input structure for VAMSubAlloc
***************************************************************************************************
*/
typedef struct _VAM_SUBALLOC_INPUT
{
    VAM_RAFT_HANDLE     hRaft;          ///< Handle of the raft to suballocate from
    VAM_VA_SIZE         sizeInBytes;    ///< Size in bytes to be allocated
    UINT                alignment;      ///< Required alignment of the allocation; POW2 value required
    DWORD               gpuMask;        ///< Which GPU applies (multi GPU case only)

} VAM_SUBALLOC_INPUT;

/**
***************************************************************************************************
*   VAM_SUBALLOC_OUTPUT
*
*   @brief
*       Output structure for VAMSubAlloc
***************************************************************************************************
*/
typedef struct _VAM_SUBALLOC_OUTPUT
{
    VAM_VIRTUAL_ADDRESS     virtualAddress; ///< Assigned VA of the allocation
    VAM_VA_SIZE             actualSize;     ///< Actual size of the allocation
    VAM_VIDMEM_HANDLE       hVidMem;        ///< Handle of the allocation block
    UINT                    offsetInBytes;  ///< Byte offset within the allocation block
    VAM_ALLOCATION_HANDLE   hVaAlloc;       ///< Allocation tracker handle (multi GPU only)

} VAM_SUBALLOC_OUTPUT;

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
    VAM_SUBALLOC_OUTPUT*    pSubAllocOut    ///< Output data structure filled in by this function
);

/**
***************************************************************************************************
*   VAM_SUBFREE_INPUT
*
*   @brief
*       Input structure for VAMSubFree
***************************************************************************************************
*/
typedef struct _VAM_SUBFREE_INPUT
{
    VAM_RAFT_HANDLE         hRaft;          ///< Handle of the raft to free from
    VAM_VIRTUAL_ADDRESS     virtualAddress; ///< VA of the allocation to be freed
    VAM_VA_SIZE             actualSize;     ///< Actual size of allocation to be freed
    VAM_ALLOCATION_HANDLE   hVaAlloc;       ///< Allocation tracker handle (multi GPU only)
    DWORD                   gpuMask;        ///< Which GPU applies (multi GPU case only)
} VAM_SUBFREE_INPUT;

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
    VAM_SUBFREE_INPUT*  pSubFreeIn  ///< Input data structure describing the suballocation to be freed
);

/**
***************************************************************************************************
*   VAM_SUBALLOCSTATUS_OUTPUT
*
*   @brief
*       Output structure for VAMQuerySubAllocStatus
***************************************************************************************************
*/
typedef struct _VAM_SUBALLOCSTATUS_OUTPUT
{
    VAM_VA_SIZE     raftSizeInBytes;        ///< Raft size in bytes
    UINT            numberOfBlocks;         ///< Number of blocks in the raft
    UINT            numberOfSubAllocs;      ///< Number of existing suballocations in raft
    VAM_VA_SIZE     minBlockSizeInBytes;    ///< Minimum block size being used
    VAM_VA_SIZE     usedSizeInBytes;        ///< Allocated size in bytes
    VAM_VA_SIZE     freeSizeInBytes;        ///< Free size in bytes

} VAM_SUBALLOCSTATUS_OUTPUT;

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
    VAM_SUBALLOCSTATUS_OUTPUT*  pSubAllocStatusOut  ///< Output data structure containing raft status details
);

/**
***************************************************************************************************
*   VAM_BLOCKSTATUS_OUTPUT
*
*   @brief
*       Output structure for VAMQueryBlockStatus
***************************************************************************************************
*/
typedef struct _VAM_BLOCKSTATUS_OUTPUT
{
    UINT            numberOfSubAllocs;      ///< Number of existing suballocations in owning block
} VAM_BLOCKSTATUS_OUTPUT;

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
    VAM_HANDLE                  hVam,           ///< Input handle of the VAM instance
    VAM_RAFT_HANDLE             hRaft,          ///< Handle of the raft to query from
    VAM_VIDMEM_HANDLE           hVidMem,        ///< Handle of the VidMem object
    VAM_BLOCKSTATUS_OUTPUT*     pQueryStatusOut ///< Output data structure containing block
                                                ///< status details
);

/**
***************************************************************************************************
*   VAM_SUBALLOCOFFER_INPUT
*
*   @brief
*       Input structure for VAMSubAllocOffer
***************************************************************************************************
*/
typedef struct _VAM_SUBALLOCOFFER_INPUT
{
    VAM_RAFT_HANDLE     hRaft;          ///< Handle of the raft to offer from
    VAM_VIRTUAL_ADDRESS virtualAddress; ///< VA of the allocation to be offered
    VAM_VA_SIZE         actualSize;     ///< Actual size of allocation to be offered

} VAM_SUBALLOCOFFER_INPUT;

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
    VAM_SUBALLOCOFFER_INPUT*    pOfferIn    ///< Input data structure describing the suballocation to be offered
);

/**
***************************************************************************************************
*   VAM_SUBALLOCRECLAIM_INPUT
*
*   @brief
*       Input structure for VAMSubAllocReclaim
***************************************************************************************************
*/
typedef struct _VAM_SUBALLOCRECLAIM_INPUT
{
    VAM_RAFT_HANDLE     hRaft;          ///< Handle of the raft to reclaim into
    VAM_VIRTUAL_ADDRESS virtualAddress; ///< VA of the allocation to be reclaimed
    VAM_VA_SIZE         actualSize;     ///< Actual size of allocation to be reclaimed

} VAM_SUBALLOCRECLAIM_INPUT;

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
    VAM_SUBALLOCRECLAIM_INPUT*  pReclaimIn  ///< Pointer to the input data structure describing the
                                            ///  suballocation to be reclaimed.
);

#if defined(__cplusplus)
}
#endif

#endif // __VAMINTERFACE_H__
