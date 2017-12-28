/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2017 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/gfx6/gfx6Device.h"
#include "core/hw/gfxip/gfx6/gfx6PrefetchMgr.h"

using namespace Util;

namespace Pal
{
namespace Gfx6
{

// =====================================================================================================================
PrefetchMgr::PrefetchMgr(
    const Device& device)
    :
    Pal::PrefetchMgr(device)
{
    // Default Gfx6 prefetching to off for now.  Will enable in a follow-up change.
    m_curPrefetchMask = 0;

    const Gfx6PalSettings& settings = static_cast<const Device&>(m_device).Settings();

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
// Called by the command buffer to request a prefetch be performed. Depending on the type of prefetch and the settings,
// this may be performed immediately or delayed until draw/dispatch. Returns the next unused DWORD in pCmdSpace.
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
        PAL_ASSERT((addr & (RequiredStartAlign - 1)) == 0);
        PAL_ASSERT((sizeInBytes & (RequiredSizeAlign - 1)) == 0);

        if (m_prefetchDescriptors[prefetchType].clampSize != 0)
        {
            sizeInBytes = Min(sizeInBytes, m_prefetchDescriptors[prefetchType].clampSize);
        }

        if (m_prefetchDescriptors[prefetchType].method == PrefetchCpDma)
        {
            // CP DMA prefetches should be issued right away to give them a little head start.
            const Device& device = static_cast<const Device&>(m_device);

            if (device.Parent()->ChipProperties().gfxLevel != GfxIpLevel::GfxIp6)
            {
                // We can't write to L2 if this workaround is enabled.
                const bool noDstL2 = device.WaCpDmaHangMcTcAckDrop();

                DmaDataInfo dmaDataInfo  = {};
                dmaDataInfo.dstAddr      = addr;
                dmaDataInfo.dstAddrSpace = CPDMA_ADDR_SPACE_MEM;
                dmaDataInfo.dstSel       = noDstL2 ? CPDMA_DST_SEL_DST_ADDR : CPDMA_DST_SEL_DST_ADDR_USING_L2;
                dmaDataInfo.srcAddr      = addr;
                dmaDataInfo.srcAddrSpace = CPDMA_ADDR_SPACE_MEM;
                dmaDataInfo.srcSel       = CPDMA_SRC_SEL_SRC_ADDR_USING_L2;
                dmaDataInfo.numBytes     = static_cast<uint32>(sizeInBytes);

                pCmdSpace += device.CmdUtil().BuildDmaData(dmaDataInfo, pCmdSpace);
            }
        }
    }

    return pCmdSpace;
}

} // Gfx6
} // Pal
