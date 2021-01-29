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

#include "core/device.h"
#include "core/svmMgr.h"
#include "palBestFitAllocatorImpl.h"

using namespace Util;

namespace Pal
{

// =====================================================================================================================
SvmMgr::SvmMgr(
    Device* pDevice)
    :
    m_pDevice(pDevice),
    m_vaStart(0),
    m_vaSize(0),
    m_pSubAllocator(nullptr)
{
}

// =====================================================================================================================
SvmMgr::~SvmMgr()
{
    PAL_DELETE(m_pSubAllocator, m_pDevice->GetPlatform());
}

// =====================================================================================================================
// Peforms extra initialization which needs to be done when the client is ready to start using the device.
// - Reserve CPU & GPU virtual address for SVM use.
Result SvmMgr::Init(
    VaRangeInfo* pSvmVaInfo)
{
    Result result = Result::Success;

    Platform* pPlatform = m_pDevice->GetPlatform();
    const GpuMemoryProperties& memProps = m_pDevice->MemoryProperties();
    const VaRangeInfo& defaultRange = memProps.vaRange[static_cast<uint32>(VaPartition::Default)];
    m_vaStart = defaultRange.baseVirtAddr;
    gpusize vaEnd = m_vaStart + defaultRange.size;
    m_vaSize = pPlatform->GetMaxSizeOfSvm();

    if (result == Result::Success)
    {
        gpusize reservedVaEnd = 0u;
        // Loop through all the devices and skip any VA ranges that were already reserved for SVM
        // This is to guarantee that the SVM space of each device doesn't overlap with those of other devices
        for (uint32 i = 0; i < pPlatform->GetDeviceCount(); i++)
        {
            const Device* pDevice = pPlatform->GetDevice(i);
            const VaRangeInfo& svmRange = pDevice->MemoryProperties().vaRange[static_cast<uint32>(VaPartition::Svm)];

            const gpusize svmVaStart = svmRange.baseVirtAddr;
            const gpusize svmVaEnd = svmVaStart + svmRange.size;

            if (svmRange.size != 0)
            {
                reservedVaEnd = Util::Max(reservedVaEnd, svmVaEnd);
            }
        }

        constexpr gpusize SvmVaAlignment = (1ull << 32u);
        m_vaStart = Util::Pow2Align(Util::Max(m_vaStart, reservedVaEnd), SvmVaAlignment);
        PAL_ASSERT((vaEnd - m_vaStart) >= m_vaSize);
        for (; m_vaStart <= (vaEnd - m_vaSize); m_vaStart += SvmVaAlignment)
        {
            gpusize cpuVaAllocated = 0u;
            gpusize gpuVaAllocated = 0u;

            // Try to reserve the range on the CPU side
            result = VirtualReserve(static_cast<size_t>(m_vaSize),
                                    reinterpret_cast<void**>(&cpuVaAllocated),
                                    reinterpret_cast<void*>(m_vaStart));

            // Make sure we get the address that we requested
            if ((result == Result::Success) &&
                (cpuVaAllocated != m_vaStart))
            {
                result = Result::ErrorOutOfMemory;
            }

            if (result == Result::Success)
            {
                // Try to reserve the range on the GPU side
                result = m_pDevice->ReserveGpuVirtualAddress(VaPartition::Svm, m_vaStart, m_vaSize, false,
                                                             VirtualGpuMemAccessMode::Undefined, &gpuVaAllocated);

                // Make sure we get the address that we requested
                if ((result == Result::Success) &&
                    (gpuVaAllocated != m_vaStart))
                {
                    result = Result::ErrorOutOfGpuMemory;
                }
            }

            if (result == Result::Success)
            {
                pSvmVaInfo->baseVirtAddr = m_vaStart;
                pSvmVaInfo->size         = m_vaSize;
                break;
            }
            // If we weren't able to reserve the specified VA range on the CPU and GPU
            // Release any reserved VA ranges and try again with a different one
            else
            {
                if (cpuVaAllocated != 0)
                {
                    result = VirtualRelease(reinterpret_cast<void*>(cpuVaAllocated),
                                            static_cast<size_t>(m_vaSize));
                    PAL_ALERT(result != Result::Success);
                }

                if (gpuVaAllocated != 0)
                {
                    result = m_pDevice->FreeGpuVirtualAddress(gpuVaAllocated, m_vaSize);
                    PAL_ALERT(result != Result::Success);
                }

                result = Result::Success;
            }
        }
    }

    if (result == Result::Success)
    {
        // Create and initialize the suballocator
        m_pSubAllocator = PAL_NEW(BestFitAllocator<Platform>, pPlatform, AllocInternal)
                                (pPlatform, m_vaSize, memProps.fragmentSize);
        if (m_pSubAllocator != nullptr)
        {
            result = m_pSubAllocator->Init();
        }
        else
        {
            result = Result::ErrorOutOfMemory;
        }
    }

    return result;
}

// =====================================================================================================================
// Peforms extra initialization which needs to be done when the client is ready to start using the device.
// - Release CPU & GPU virtual address of SVM.
Result SvmMgr::Cleanup()
{
    Result result = Result::Success;

    if (m_vaStart != 0)
    {
        result = m_pDevice->FreeGpuVirtualAddress(m_vaStart, m_vaSize);
    }

    return result;
}

// =====================================================================================================================
Result SvmMgr::AllocVa(
    gpusize size,
    uint32  align,
    gpusize* pVirtualAddress)
{
    gpusize assignedVa = 0;
    MutexAuto lock(&m_allocFreeVaLock);

    Result result = m_pSubAllocator->Allocate(size, align, &assignedVa);
    *pVirtualAddress = assignedVa + m_vaStart;

    return result;
}

// =====================================================================================================================
void SvmMgr::FreeVa(
    gpusize virtualAddress)
{
    MutexAuto lock(&m_allocFreeVaLock);

    m_pSubAllocator->Free((virtualAddress - m_vaStart));
    return;
}
} // Pal
