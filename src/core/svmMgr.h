/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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

#pragma once

#include "palMutex.h"
#include "palBestFitAllocator.h"
#include "core/platform.h"

namespace Pal
{
class Platform;

// =====================================================================================================================
// SvmMgr provides a clean interface between PAL and the BestFitAllocator, which is used to allocate and free GPU
// virtual address space for SVM allocations on Windows WDDM2 and Linux platforms.
// This GPU virtual address is shared with CPU.
// On WDDM1 platforms, VamMgr provides VA managements for SVM.
//
// Some commonly used abbreviations throughout the implementation of this class:
//     - VA:  Virtual address
//     - SVM: Shared Virtual Memory
class SvmMgr
{
public:
    explicit SvmMgr(Device* pDevice);
    virtual ~SvmMgr();

    Result Init(VaRangeInfo* pSvmVaInfo);
    Result Cleanup();

    Result AllocVa(gpusize size, uint32 align, gpusize* pVirtualAddress);
    void   FreeVa(gpusize virtualAddress);

    gpusize GetStartAddr() const { return m_vaStart; }

private:
    Device*const m_pDevice;
    gpusize      m_vaStart;
    gpusize      m_vaSize;

    Util::BestFitAllocator<Pal::Platform>* m_pSubAllocator;  // Suballocator used for the suballocation

    Util::Mutex  m_allocFreeVaLock;                          // Mutex protecting allocation and free of SVM va

    PAL_DISALLOW_DEFAULT_CTOR(SvmMgr);
    PAL_DISALLOW_COPY_AND_ASSIGN(SvmMgr);
};

} // Pal
