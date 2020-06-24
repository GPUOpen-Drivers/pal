/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2009-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  vamsuballoc.h
* @brief Contains definitions of rafts and blocks specific to suballocations.
***************************************************************************************************
*/

#ifndef __VAMSUBALLOC_H__
#define __VAMSUBALLOC_H__

#include "vamcommon.h"
#include "vamrange.h"

// Forward references
class VamRaft;
class VamDevice;

struct VamOfferEntry : public VamObject, public VamLink<VamOfferEntry>
{
    VamOfferEntry(VAM_CLIENT_HANDLE hClient)
    :   VamObject(hClient),
        VamLink<VamOfferEntry>()
    {
        m_addr = 0;
        m_size = 0;
    }
   ~VamOfferEntry() {}

    VAM_VIRTUAL_ADDRESS     m_addr;
    VAM_VA_SIZE             m_size;
};

typedef VamList<VamOfferEntry> OfferList;

class VamBlock : public VamObject, public VamLink<VamBlock>
{
public:
    VamBlock(VAM_CLIENT_HANDLE hClient, VamRaft* pRaft);
   ~VamBlock();

    VamVARange& VASpace(void)
    {return m_VASpace;}

    VAM_VIDMEM_HANDLE hVidMem(void)
    {return m_hVidMem;}

    void setVidMemHandle(VAM_VIDMEM_HANDLE hVidMem)
    {m_hVidMem = hVidMem;}

    VamOfferEntry* AddToOfferList(
        VAM_VIRTUAL_ADDRESS addr,
        VAM_VA_SIZE         size);

    void RemoveFromOfferList(VamOfferEntry* pOffer);

    bool &offered(void)
    { return m_Offered; }

    VAM_VA_SIZE totalOfferSize(void)
    {return m_OfferListSize;}

    OfferList& offerList(void)
    {return m_OfferList;}

private:
    VamRaft*                m_pRaft;            // points to the raft that owns this block
    VAM_VIDMEM_HANDLE       m_hVidMem;          // block's video memory handle
    VamVARange              m_VASpace;          // block's VA space status
    OfferList               m_OfferList;        // block's list of suballocations that have been offered
    VAM_VA_SIZE             m_OfferListSize;    // block's total size of all the offered suballocations
    bool                    m_Offered;          // true if block had been offered
};

typedef VamList<VamBlock> BlockList;

class VamRaft : public VamObject, public VamLink<VamRaft>
{
public:
    VamRaft() {}
    VamRaft(VAM_CLIENT_HANDLE       hClient,
            VamDevice*              pVamDevice,
            VAM_VA_SIZE             minBlockSizeInBytes,
            VAM_CLIENT_OBJECT       clientObject,
            VAM_CREATERAFT_FLAGS    flags,
            VAM_SECTION_HANDLE      hSection);
   ~VamRaft();

    VamVARange& VASpace(void)
    {return m_VASpace;}

    BlockList& blockList(void)
    {return m_blockList;}

    VAM_VA_SIZE minBlockSize(void)
    {return m_minBlockSize;}

    bool keepBlocksResident(void)
    {return m_flags.keepBlocksResident;}

    VAM_VA_SIZE GetTotalFreeSize(void);

    VamBlock* AllocBlock(VAM_VA_SIZE reqBlockSize);

    VAM_RETURNCODE FreeBlock(VamBlock* pBlock);

    VamDevice* pVamDevice(void)
    {return m_pVamDevice;}

    VAM_SECTION_HANDLE getParentSection(void)
    {return m_hSection;}

private:
    VAM_VA_SIZE             m_minBlockSize;         // adjusted minimum block size
    VAM_CLIENT_OBJECT       m_clientObject;         // client's opaque object
    VAM_CREATERAFT_FLAGS    m_flags;

    VamVARange              m_VASpace;              // raft's VA space status
    BlockList               m_blockList;            // list of blocks belonging to this raft

    VamDevice*              m_pVamDevice;           // pointer to the device object
    VAM_SECTION_HANDLE      m_hSection;             // section where raft is created from
};

typedef VamList<VamRaft> RaftList;

#endif
