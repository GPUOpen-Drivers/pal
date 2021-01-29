/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2009-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  vamdevice.h
* @brief Contains the VamDevice base class definition.
***************************************************************************************************
*/

#ifndef __VAMDEVICE_H__
#define __VAMDEVICE_H__

#include "vaminterface.h"
#include "vamcommon.h"
#include "vamsuballoc.h"
#include "vamsectionalloc.h"
#include "vamptb.h"

/**
***************************************************************************************************
* @brief This is the base class for the VAM Device object.
***************************************************************************************************
*/
class VamDevice : public VamObject
{
public:
    virtual ~VamDevice() {}

    static VamDevice*   GetVamDeviceObject(VAM_HANDLE hVam);
    static VamSection*  GetVamSectionObject(VAM_SECTION_HANDLE hSection);
    static VamRaft*     GetVamRaftObject(VAM_RAFT_HANDLE hRaft);

    // Functions associated with the external API
    static VamDevice* Create(
        VAM_CLIENT_HANDLE               hClient,
        const VAM_CREATE_INPUT*         pInput);

    VAM_RETURNCODE Destroy( void );

    VAM_RETURNCODE RegularAllocateVASpace(
        VAM_ALLOC_INPUT*                pAllocIn,
        VAM_ALLOC_OUTPUT*               pAllocOut);

    VAM_RETURNCODE RegularFreeVASpace(
        VAM_FREE_INPUT*                 pFreeIn);

    VAM_RETURNCODE QueryGlobalAllocStatus(
        VAM_GLOBALALLOCSTATUS_OUTPUT*   pGlobalAllocStatusOut);

    VAM_RETURNCODE ExcludeRange(
        VAM_EXCLUDERANGE_INPUT*         pExcludeRangeIn);

    VAM_SECTION_HANDLE CreateSection(
        VAM_VA_SIZE                     requestedSectionSizeInBytes,
        VAM_CLIENT_OBJECT               clientObject,
        VAM_CREATESECTION_FLAGS         flags,
        VAM_VIRTUAL_ADDRESS             sectionAddress,
        VAM_RETURNCODE* const           pRetCode);

    VAM_RETURNCODE DestroySection(
        VAM_SECTION_HANDLE              hSection);

    VAM_RETURNCODE QuerySectionAllocStatus(
        VAM_SECTION_HANDLE              hSection,
        VAM_SECTIONALLOCSTATUS_OUTPUT*  pSectionAllocStatusOut);

    VAM_RAFT_HANDLE CreateRaft(
        VAM_SECTION_HANDLE              hSection,
        VAM_VA_SIZE                     requestedRaftSizeInBytes,
        VAM_VA_SIZE                     requestedMinBlockSizeInBytes,
        VAM_CLIENT_OBJECT               clientObject,
        VAM_CREATERAFT_FLAGS            flags,
        VAM_VIRTUAL_ADDRESS             raftAddress);

    VAM_RETURNCODE DestroyRaft(
        VAM_RAFT_HANDLE                 hRaft);

    VAM_RETURNCODE Trim(
        VAM_TRIM_FLAGS                  flags);

    VAM_RETURNCODE GetRaftAllocationInfo(
        VAM_RAFT_HANDLE                 hRaft,
        VAM_ALLOC_OUTPUT*               pAllocOut);

    VAM_RETURNCODE SubAllocateVASpace(
        VAM_SUBALLOC_INPUT*             pSubAllocIn,
        VAM_SUBALLOC_OUTPUT*            pSubAllocOut);

    VAM_RETURNCODE SubFreeVASpace(
        VAM_SUBFREE_INPUT*              pSubFreeIn);

    VAM_RETURNCODE QuerySubAllocStatus(
        VAM_RAFT_HANDLE                 hRaft,
        VAM_SUBALLOCSTATUS_OUTPUT*      pSubAllocStatusOut);

    VAM_RETURNCODE QueryBlockStatus(
        VAM_RAFT_HANDLE                 hRaft,
        VAM_VIDMEM_HANDLE               hVidMem,
        VAM_BLOCKSTATUS_OUTPUT*         pQueryStatusOut);

    VAM_RETURNCODE SubAllocOffer(
        VAM_SUBALLOCOFFER_INPUT*        pOffer);

    VAM_RETURNCODE SubAllocReclaim(
        VAM_SUBALLOCRECLAIM_INPUT*      pReclaim);

    // Internal member accesors
    VamGlobalVASpace& globalVASpace(void)
    { return m_globalVASpace; }

    ExcludedRangeList& excludedRangeList(void)
    { return m_excludedRangeList; }

    SectionList& sectionList(void)
    { return m_sectionList; }

    RaftList& raftList(void)
    { return m_raftList; }

    PtbManager& ptbMgr(void)
    { return m_ptbMgr; }

    bool usingUIB(void)
    { return m_flags.useUIB; }

    // Memory management functions
    VamSection* AllocSection(
        VAM_VA_SIZE                     sectionSize,
        VAM_CLIENT_OBJECT               clientObject,
        VAM_CREATESECTION_FLAGS         flags,
        VAM_VIRTUAL_ADDRESS             sectionAddress,
        VAM_RETURNCODE* const           pRetCode);

    VAM_RETURNCODE FreeSection(
        VamSection*                     pSection,
        bool                            checkForEmpty);

    VamRaft* AllocRaft(
        VAM_SECTION_HANDLE              hSection,
        VAM_VA_SIZE                     raftSize,
        VAM_VA_SIZE                     minBlockSize,
        VAM_CLIENT_OBJECT               clientObject,
        VAM_CREATERAFT_FLAGS            flags,
        VAM_VIRTUAL_ADDRESS             raftAddress);

    VAM_RETURNCODE FreeRaft(
        VamRaft*                        pRaft,
        bool                            checkForEmpty);

    // Callback methods
    VOID* AllocSysMem(
        UINT                            sizeInBytes);

    VAM_RETURNCODE FreeSysMem(
        VOID*                           pVirtAddr);

    VAM_PTB_HANDLE AllocPTB(
        VAM_VIRTUAL_ADDRESS             PTBBaseAddr,
        VAM_RETURNCODE* const           pRetCode);

    VAM_RETURNCODE FreePTB(
        VAM_CLIENT_HANDLE               hPTBAlloc);

    VAM_RETURNCODE MapPTB(
        VAM_ALLOCATION&                 allocation);

    VAM_VIDMEM_HANDLE AllocVidMem(
        VAM_ALLOCVIDMEM_INPUT*          pAllocVidMemIn);

    VAM_RETURNCODE FreeVidMem(
        VAM_VIDMEM_HANDLE               hVidMem);

    VAM_RETURNCODE AcquireSyncObj(void);

    VOID ReleaseSyncObj(void);

    VAM_RETURNCODE OfferVidMem(
        VAM_VIDMEM_HANDLE               hVidMem);

    VAM_RETURNCODE ReclaimVidMem(
        VAM_VIDMEM_HANDLE               hVidMem);

    bool needPTB(void)
    { return (m_callbacks.needPTB() == VAM_OK); }

protected:
    VamDevice();      // Constructor is protected.
    VamDevice(VAM_CLIENT_HANDLE hClient);

    VAM_RETURNCODE Init(
        const VAM_CREATE_INPUT*         pCreateIn);

private:
    VAM_VERSION             m_version;              ///< VAM version number
    VAM_CALLBACKS           m_callbacks;            ///< Supported callbacks
    VAM_VIRTUAL_ADDRESS     m_VARangeStart;         ///< VA range starting address (4KB aligned)
    VAM_VIRTUAL_ADDRESS     m_VARangeEnd;           ///< VA range ending address (4KB aligned)
    UINT                    m_PTBSize;              ///< Size of a PTB in bytes
    UINT                    m_bigKSize;             ///< Size of a big-K fragment in bytes
    VAM_SYNCOBJECT_HANDLE   m_hSyncObj;             ///< Handle of sync object (0=client is thread-safe)
    VAM_CREATE_FLAGS        m_flags;                ///< Create Descriptor Flags
    UINT                    m_uibVersion;           ///< UIB format version number
    UINT                    m_gpuCount;             ///< used for MGPU configurations - default is 1

    VamGlobalVASpace        m_globalVASpace;        ///< Global VA space
    ExcludedRangeList       m_excludedRangeList;    ///< List of Excluded Range
    SectionList             m_sectionList;          ///< List of Sections
    RaftList                m_raftList;             ///< List of Rafts
    PtbManager              m_ptbMgr;               ///< PTB Manager

    // Find the section where the specified VA range resides
    VamSection* FindSectionVAResideIn(
        VAM_VIRTUAL_ADDRESS             startVA,
        VAM_VA_SIZE                     size);

    // Disallow the copy constructor
    VamDevice(const VamDevice& a);

    // Disallow the assignment operator
    VamDevice& operator=(const VamDevice& a);
};

VAM_INLINE VAM_RETURNCODE VamDevice::MapPTB(
    VAM_ALLOCATION&     allocation)
{
    VAM_RETURNCODE  ret;

    ret = ptbMgr().AssignPtb(allocation.address,
                             allocation.address + allocation.size);

    return ret;
}

VAM_INLINE VAM_RETURNCODE VamDevice::AcquireSyncObj(void)
{
    VAM_RETURNCODE  ret = VAM_OK;

    if (m_hSyncObj)
    {
        VAM_ACQSYNCOBJ_INPUT    AcqSyncObjIn;

        AcqSyncObjIn.hSyncObj = m_hSyncObj; // Handle of sync object
        AcqSyncObjIn.timeout  = 1;          // Time-out interval in ms

        if (m_callbacks.acquireSyncObj != NULL)
        {
            ret = m_callbacks.acquireSyncObj(m_hClient, &AcqSyncObjIn);
        }
    }

    return ret;
}

VAM_INLINE VOID VamDevice::ReleaseSyncObj(void)
{
    if (m_hSyncObj)
    {
        if (m_callbacks.releaseSyncObj != NULL)
        {
            m_callbacks.releaseSyncObj(m_hClient, m_hSyncObj);
        }
    }
}

#endif
