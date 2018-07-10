/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9PrefetchMgr.h"

using namespace Util;

namespace Pal
{
namespace Gfx9
{

// =====================================================================================================================
PrefetchMgr::PrefetchMgr(
    const Device& device)
    :
    Pal::PrefetchMgr(device)
{
    const Gfx9PalSettings& settings = static_cast<const Device&>(m_device).Settings();

    memset(&m_prefetchDescriptors[0], 0, sizeof(m_prefetchDescriptors));

    // Initialize prefetch methods.
    m_prefetchDescriptors[PrefetchVs].method = settings.shaderPrefetchMethod;
    m_prefetchDescriptors[PrefetchHs].method = settings.shaderPrefetchMethod;
    m_prefetchDescriptors[PrefetchDs].method = settings.shaderPrefetchMethod;
    m_prefetchDescriptors[PrefetchGs].method = settings.shaderPrefetchMethod;
    m_prefetchDescriptors[PrefetchPs].method = settings.shaderPrefetchMethod;
    m_prefetchDescriptors[PrefetchCs].method = settings.shaderPrefetchMethod;

    // Initialize prefetch minimum sizes.
    m_prefetchDescriptors[PrefetchVs].minSize = settings.shaderPrefetchMinSize;
    m_prefetchDescriptors[PrefetchHs].minSize = settings.shaderPrefetchMinSize;
    m_prefetchDescriptors[PrefetchDs].minSize = settings.shaderPrefetchMinSize;
    m_prefetchDescriptors[PrefetchGs].minSize = settings.shaderPrefetchMinSize;
    m_prefetchDescriptors[PrefetchPs].minSize = settings.shaderPrefetchMinSize;
    m_prefetchDescriptors[PrefetchCs].minSize = settings.shaderPrefetchMinSize;

    // Initialize prefetch clamp sizes.
    m_prefetchDescriptors[PrefetchVs].clampSize = settings.shaderPrefetchClampSize;
    m_prefetchDescriptors[PrefetchHs].clampSize = settings.shaderPrefetchClampSize;
    m_prefetchDescriptors[PrefetchDs].clampSize = settings.shaderPrefetchClampSize;
    m_prefetchDescriptors[PrefetchGs].clampSize = settings.shaderPrefetchClampSize;
    m_prefetchDescriptors[PrefetchPs].clampSize = settings.shaderPrefetchClampSize;
    m_prefetchDescriptors[PrefetchCs].clampSize = settings.shaderPrefetchClampSize;
}

// =====================================================================================================================
// Called by the command buffer to request a prefetch be performed.  Depending on the type of prefetch and the
// settings, this may be performed immediately or delayed until draw/dispatch.
uint32* PrefetchMgr::RequestPrefetch(
    PrefetchType prefetchType,  // Type of prefetch to perform.
    gpusize      addr,          // Address to prefetch.
    size_t       sizeInBytes,   // How many bytes to prefetch.
    uint32*      pCmdSpace
    ) const
{
    if ((TestAnyFlagSet(m_curPrefetchMask, (1 << prefetchType)) == true) &&
        (sizeInBytes >= m_prefetchDescriptors[prefetchType].minSize))
    {
        if (m_prefetchDescriptors[prefetchType].clampSize != 0)
        {
            sizeInBytes = Min(sizeInBytes, m_prefetchDescriptors[prefetchType].clampSize);
        }

        if (m_prefetchDescriptors[prefetchType].method == PrefetchPrimeUtcL2)
        {
            // We'll underflow the numPages calculation if we're priming zero bytes.
            PAL_ASSERT(sizeInBytes > 0);

            const gpusize firstPage = Pow2AlignDown(addr, PrimeUtcL2MemAlignment);
            const gpusize lastPage  = Pow2AlignDown(addr + sizeInBytes - 1, PrimeUtcL2MemAlignment);
            const size_t  numPages  = 1 + static_cast<size_t>((lastPage - firstPage) / PrimeUtcL2MemAlignment);
            const Device& device    = static_cast<const Device&>(m_device);

            pCmdSpace += device.CmdUtil().BuildPrimeUtcL2(firstPage,
                                                          cache_perm__pfp_prime_utcl2__execute,
                                                          prime_mode__pfp_prime_utcl2__dont_wait_for_xack,
                                                          engine_sel__pfp_prime_utcl2__prefetch_parser,
                                                          numPages,
                                                          pCmdSpace);
        }
        else if (m_prefetchDescriptors[prefetchType].method == PrefetchCpDma)
        {
            // CP DMA prefetches should be issued right away to give them a little head start.
            const Device& device = static_cast<const Device&>(m_device);

            DmaDataInfo dmaDataInfo  = {};
            dmaDataInfo.dstAddr      = addr;
            dmaDataInfo.dstAddrSpace = das__pfp_dma_data__memory;
            dmaDataInfo.dstSel       = dst_sel__pfp_dma_data__dst_addr_using_l2;
            dmaDataInfo.srcAddr      = addr;
            dmaDataInfo.srcAddrSpace = sas__pfp_dma_data__memory;
            dmaDataInfo.srcSel       = src_sel__pfp_dma_data__src_addr_using_l2;
            dmaDataInfo.numBytes     = static_cast<uint32>(sizeInBytes);
            dmaDataInfo.disWc        = true;

            pCmdSpace += device.CmdUtil().BuildDmaData(dmaDataInfo, pCmdSpace);
        }
    }

    return pCmdSpace;
}

} // Gfx9
} // Pal
