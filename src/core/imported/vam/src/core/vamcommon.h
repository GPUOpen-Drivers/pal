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
* @file  vamcommon.h
* @brief Contains definitions that are common to all files.
***************************************************************************************************
*/

#ifndef __VAMCOMMON_H__
#define __VAMCOMMON_H__

#include "vammacros.h"
#include "vamobject.h"
#include "vamlink.h"
#include "vamtree.h"

struct VAM_ALLOCATION
{
    VAM_VIRTUAL_ADDRESS     address;    // allocation's starting VA
    VAM_VA_SIZE             size;       // allocation's actual size
};

struct VamChunk : public VamObject, public VamLink<VamChunk>, public VamTreeNode<VamChunk>
{
    VamChunk(VAM_CLIENT_HANDLE hClient)
    :   VamObject(hClient),
        VamLink<VamChunk>(),
        VamTreeNode<VamChunk>()
    {
        m_addr = 0;
        m_size = 0;
    }
    ~VamChunk() {}

    VAM_VA_SIZE& value() { return m_addr; }

    VAM_VIRTUAL_ADDRESS     m_addr;
    VAM_VA_SIZE             m_size;
};

typedef VamList<VamChunk> ChunkList;
typedef VamTree<VamChunk, VAM_VA_SIZE> ChunkTree;

struct VamExcludedRange : public VamObject, public VamLink<VamExcludedRange>
{
    VamExcludedRange(VAM_CLIENT_HANDLE hClient)
    :   VamObject(hClient),
        VamLink<VamExcludedRange>()
    {
        m_addrRequested = 0;
        m_sizeRequested = 0;
        m_addrActual    = 0;
        m_sizeActual    = 0;
    }
   ~VamExcludedRange() {}

    void Init(VAM_VIRTUAL_ADDRESS     addrRequested,
              VAM_VA_SIZE             sizeRequested,
              VAM_VIRTUAL_ADDRESS     addrActual,
              VAM_VA_SIZE             sizeActual)
    {
        m_addrRequested = addrRequested;
        m_sizeRequested = sizeRequested;
        m_addrActual    = addrActual;
        m_sizeActual    = sizeActual;
    }

    VAM_VIRTUAL_ADDRESS     m_addrRequested;
    VAM_VA_SIZE             m_sizeRequested;
    VAM_VIRTUAL_ADDRESS     m_addrActual;
    VAM_VA_SIZE             m_sizeActual;
};

typedef VamList<VamExcludedRange> ExcludedRangeList;

struct VamAllocation : public VamObject
{
    VamAllocation(VAM_CLIENT_HANDLE hClient, DWORD gpuMask = 0, VAM_RAFT_HANDLE hRaft = 0)
    :   VamObject(hClient), m_gpuMask(gpuMask), m_hRaft(hRaft)
    {
    }
   ~VamAllocation() {}

    DWORD               m_gpuMask;
    VAM_RAFT_HANDLE     m_hRaft;
};

#endif
