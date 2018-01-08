/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/g_palSettings.h"
#include "core/gpuMemory.h"
#include "core/image.h"
#include "core/internalMemMgr.h"
#include "core/platform.h"
#include "core/vamMgr.h"
#include "palSysMemory.h"

using namespace Util;

namespace Pal
{

// =====================================================================================================================
// Note that this constructor is invoked before settings have been committed.
VamMgr::VamMgr()
    :
    m_hVamInstance(nullptr),
    m_ptbSize(0)
{
    memset(&m_hSection[0], 0, sizeof(m_hSection));
}

// =====================================================================================================================
VamMgr::~VamMgr()
{
// Note: OCL API doesn't provide explicit device destruction
    // The VAM instance must be destroyed by calling Cleanup().
    PAL_ASSERT(m_hVamInstance == nullptr);
}

// =====================================================================================================================
// Performs early initialization of this object; this occurs when the device owning is created.
Result VamMgr::EarlyInit()
{
    return Result::Success;
}

// =====================================================================================================================
// This must clean up all internal GPU memory allocations and all objects created after EarlyInit. Note that EarlyInit
// is called when the platform creates the device objects so the work it does must be preserved if we are to reuse
// this object.
Result VamMgr::Cleanup(
    Device* pDevice)
{
    if (m_hVamInstance != nullptr)
    {
        for (uint32 i = 0; i < static_cast<uint32>(VaPartition::Count); ++i)
        {
            if (m_hSection[i] != nullptr)
            {
                VAMDestroySection(m_hVamInstance, m_hSection[i]);
                m_hSection[i] = nullptr;
            }
        }

        VAMDestroy(m_hVamInstance);
        m_hVamInstance = nullptr;
    }
    return Result::Success;
}

// =====================================================================================================================
// Peforms extra initialization which needs to be done when the client is ready to start using the device.
// - Allocates the GPU page directory.
// - Creates VAM's exluced ranges and sets-up the virtual addresss sections PAL uses.
Result VamMgr::Finalize(
    Device* pDevice)
{
    Result      result   = Result::Success;
    const auto& memProps = pDevice->MemoryProperties();

    if (result == Result::Success)
    {
        // Add excluded VA ranges: this will cause PTBs to be allocated for the excluded VA ranges.
        VAM_EXCLUDERANGE_INPUT excludeRangeIn = { };

        for (size_t i = 0; i < memProps.numExcludedVaRanges; ++i)
        {
            excludeRangeIn.virtualAddress = memProps.excludedRange[i].baseVirtAddr;
            excludeRangeIn.sizeInBytes    = memProps.excludedRange[i].size;

            if (VAMExcludeRange(m_hVamInstance, &excludeRangeIn) != VAM_OK)
            {
                PAL_ALERT_ALWAYS();
                result = Result::ErrorOutOfGpuMemory;
                break;
            }
        }
    }

    if (result == Result::Success)
    {
        // Add VAM sections for each virtual address range partition which has a nonzero size.
        VAM_CREATESECTION_INPUT vamSectionIn = { };

        for (uint32 i = 0; i < static_cast<uint32>(VaPartition::Count); ++i)
        {
            // "vaRange[i].size == 0x0" means that this partition is not supported.
            if (memProps.vaRange[i].size == 0x0)
            {
                continue;
            }
            vamSectionIn.sectionAddress     = memProps.vaRange[i].baseVirtAddr;
            vamSectionIn.sectionSizeInBytes = memProps.vaRange[i].size;

            if (vamSectionIn.sectionSizeInBytes > 0)
            {
                m_hSection[i] = VAMCreateSection(m_hVamInstance, &vamSectionIn);
                if (m_hSection[i] == nullptr)
                {
                    PAL_ALERT_ALWAYS();
                    result = Result::ErrorOutOfGpuMemory;
                    break;
                }
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Calculates the page table block size.
gpusize VamMgr::CalcPtbSize(
    Device* pDevice
    ) const
{
    const GpuMemoryProperties& memProps = pDevice->MemoryProperties();

    // The size of a PTB is: (spaceMappedPerPDE / spaceMappedPerPTE) * pteSize.  Each PTE maps a page's worth of VA
    // space.  numPtbsPerGroup > 1 indicates that PTB will be allocated by groups, such as for Carrizo, so the ptbSize
    // shall be multipled by numPtbsPerGroup.
    PAL_ASSERT(memProps.numPtbsPerGroup > 0);
    return (memProps.spaceMappedPerPde / SpaceMappedPerPte) * memProps.pteSize * memProps.numPtbsPerGroup;
}

} // Pal
