/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2009-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  vamsuballoc.cpp
* @brief Contains the implementation for the suballocation functionalities.
***************************************************************************************************
*/

#include "vamdevice.h"
#include "vamsuballoc.h"

VamBlock::VamBlock(
    VAM_CLIENT_HANDLE hClient,
    VamRaft* pRaft)
    :   VamObject(hClient),
        VamLink<VamBlock>(),
        m_VASpace(hClient)
{
    m_pRaft      = pRaft;
    m_hVidMem    = 0;
    m_Offered       = false;
    m_OfferListSize = 0;
}

VamBlock::~VamBlock()
{
}

VamRaft::VamRaft(
    VAM_CLIENT_HANDLE       hClient,
    VamDevice*              pVamDevice,
    VAM_VA_SIZE             minBlockSizeInBytes,
    VAM_CLIENT_OBJECT       clientObject,
    VAM_CREATERAFT_FLAGS    flags,
    VAM_SECTION_HANDLE      hSection)
    :   VamObject(hClient),
        VamLink<VamRaft>(),
        m_VASpace(hClient),
        m_blockList(),
        m_hSection(hSection)
{
    m_clientObject  = clientObject;
    m_flags         = flags;
    m_minBlockSize  = minBlockSizeInBytes;
    m_pVamDevice    = pVamDevice;
}

VamRaft::~VamRaft()
{
}

VAM_VA_SIZE VamRaft::GetTotalFreeSize(void)
{
    VAM_VA_SIZE totalFree = 0;

    // We need to walk each block contained in the raft, in order to
    // determine the amount of free space inside existing blocks.
    for (BlockList::Iterator block( blockList() );
          block != NULL;
          block++ )
    {
        totalFree += block->VASpace().totalFreeSize();
    }

    // ... and tally up the raft area that is outside of any blocks.
    totalFree += VASpace().totalFreeSize();

    return totalFree;
}

VamBlock* VamRaft::AllocBlock(VAM_VA_SIZE reqBlockSize)
{
    VAM_RETURNCODE          ret;
    VamBlock*               pBlock;
    VAM_ALLOCATION          blockAllocation;
    VAM_VIDMEM_HANDLE       hVidMem = 0;
    VAM_ALLOCVIDMEM_INPUT   AllocVidMemIn;

    // Create a new block object
    pBlock = new(m_hClient) VamBlock(m_hClient, this);
    if (pBlock)
    {
        // Round up block size to computed minimum block size multiple
        reqBlockSize = ROUND_UP(reqBlockSize, m_minBlockSize);

        // We will use our allocation routine to find VA space
        // for the blocks inside each raft.
        ret = VASpace().AllocateVASpace(
                    reqBlockSize,      // requested size of block
                    m_minBlockSize,    // blocks are to be sized in multiples of fragment sizes
                    blockAllocation);  // result of the allocation

        if (ret == VAM_OK)
        {
            // Initialize the block's default VA space state
            ret = pBlock->VASpace().Init(
                        blockAllocation.address,    // start of block's VA space
                        blockAllocation.size,       // size of block's VA space
                        SUB_ALLOC_ALGMT_SIZE);      // minimum alignment granularity for block VA space is 256 bytes

            // ... and add it to the raft's block list
            blockList().insertLast(pBlock);

            if (ret == VAM_OK)
            {
                // Next, allocate physical video memory for this block
                AllocVidMemIn.clientObject   = m_clientObject;
                AllocVidMemIn.sizeInBytes    = blockAllocation.size;
                AllocVidMemIn.alignment      = VASpace().alignmentGranularity();
                AllocVidMemIn.vidMemVirtAddr = blockAllocation.address;
                hVidMem = pVamDevice()->AllocVidMem(&AllocVidMemIn);

                if (hVidMem)
                {
                    pBlock->setVidMemHandle(hVidMem);

                    // Finally, ensure that the block is properly mapped by PTB(s)
                    if (pVamDevice()->needPTB())
                    {
                        ret = pVamDevice()->MapPTB(blockAllocation);
                    }
                }
            }
        }

        if (!hVidMem || (ret != VAM_OK))
        {
            // Could not successfully allocate the block. Clean up.
            FreeBlock(pBlock);
            pBlock = NULL;
        }
    }

    return pBlock;
}

VAM_RETURNCODE VamRaft::FreeBlock(VamBlock* pBlock)
{
    VAM_RETURNCODE  ret = VAM_OK;

    VAM_ASSERT(pBlock != NULL);

    // Free the video memory associated with this block
    if (pBlock->hVidMem())
    {
        ret = pVamDevice()->FreeVidMem(pBlock->hVidMem());

        if (VAM_OK == ret)
        {
            pBlock->setVidMemHandle(0);
        }
    }

    // Free the chunks from the chunk list
    pBlock->VASpace().FreeChunksFromList();

    if (!pVamDevice()->usingUIB())
    {
        // Free the VA space which the block was using inside the raft.
        VASpace().FreeVASpace(pBlock->VASpace().addr(),    // block's starting VA
                              pBlock->VASpace().size());   // block's size
    }

    // Remove the block from the list and free the block object
    if (blockList().contains(pBlock))
    {
        blockList().remove(pBlock);
    }
    delete pBlock;

    return ret;
}

VamOfferEntry* VamBlock::AddToOfferList(
    VAM_VIRTUAL_ADDRESS addr,
    VAM_VA_SIZE         size)
{
    // Add the requested suballocation to the offer list.
    VamOfferEntry*  pOffer;

    pOffer = new(m_hClient) VamOfferEntry(m_hClient);
    if (pOffer != NULL)
    {
        pOffer->m_addr = addr;
        pOffer->m_size = size;
        offerList().insertLast(pOffer);
        m_OfferListSize += size;
    }

    return pOffer;
}

void VamBlock::RemoveFromOfferList(VamOfferEntry* pOffer)
{
    if (pOffer != NULL)
    {
        m_OfferListSize -= pOffer->m_size;

        offerList().remove(pOffer);

        delete pOffer;
    }
}
