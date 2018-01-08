/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/cmdStream.h"
#include "core/device.h"
#include "core/internalMemMgr.h"
#include "core/hw/gfxip/gfx6/gfx6Gds.h"
#include "core/hw/gfxip/gfx6/gfx6Device.h"
#include "core/hw/gfxip/gfx6/gfx6CmdStream.h"

using namespace Util;

namespace Pal
{
namespace Gfx6
{

// =====================================================================================================================
// Loads data from memory to GDS before the specified HW pipeline point.
void BuildLoadGds(
    CmdStream*        pCmdStream,
    const CmdUtil*    pCmdUtil,
    HwPipePoint       pipePoint,
    uint32            dstGdsOffset,
    const IGpuMemory& srcGpuMemory,
    gpusize           srcMemOffset,
    uint32            size)
{
    PAL_ASSERT(((dstGdsOffset % 4) == 0) && ((srcMemOffset % 4) == 0) && ((size % 4) == 0));

    uint32* pCmdSpace = pCmdStream->ReserveCommands();

    // Use DMA_DATA to copy from memory to GDS.
    DmaDataInfo dmaData  = {};
    dmaData.dstSel       = CPDMA_DST_SEL_GDS;
    dmaData.dstAddr      = dstGdsOffset;
    dmaData.dstAddrSpace = CPDMA_ADDR_SPACE_MEM;
    dmaData.srcSel       = CPDMA_SRC_SEL_SRC_ADDR;
    dmaData.srcAddr      = srcGpuMemory.Desc().gpuVirtAddr + srcMemOffset;
    dmaData.srcAddrSpace = CPDMA_ADDR_SPACE_MEM;
    dmaData.numBytes     = static_cast<uint32>(size);
    dmaData.sync         = true;
    dmaData.usePfp       = false;
    pCmdSpace += pCmdUtil->BuildDmaData(dmaData, pCmdSpace);

    pCmdStream->CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// Stores data from GDS to memory after the specified HW pipeline point.
void BuildStoreGds(
    CmdStream*        pCmdStream,
    const CmdUtil*    pCmdUtil,
    HwPipePoint       pipePoint,
    uint32            srcGdsOffset,
    const IGpuMemory& dstGpuMemory,
    gpusize           dstMemOffset,
    uint32            size,
    bool              waitForWC,
    bool              forComputeEngine,
    gpusize           fenceAddr)
{
    PAL_ASSERT(((srcGdsOffset % 4) == 0) && ((dstMemOffset % 4) == 0) && ((size % 4) == 0));

    // Depending on the HW pipeline point we'll have to do the following (considering limitations of CP):
    // - HwPipeTop, HwPipePostIndexFetch: Simply do a CPDMA from GDS to memory
    // - HwPipePreRasterization, HwPipePostPs: Do a WRITE_EVENT_EOS(PS_DONE) to copy from GDS to memory
    // - HwPipePostCs: Do a WRITE_EVENT_EOS(CS_DONE) to copy from GDS to memory
    // - HwPipePostBlt, HwPipeBottom: Do a partial CS flush and then a WRITE_EVENT_EOS(PS_DONE) to copy

    // Decide whether to do CPDMA or WRITE_EVENT based on the specified HW pipeline point.
    const bool useCpdma = (pipePoint == HwPipeTop) || (pipePoint == HwPipePostIndexFetch);

    uint32* pCmdSpace = pCmdStream->ReserveCommands();

    if (useCpdma)
    {
        // Use DMA_DATA to copy from GDS to memory.
        DmaDataInfo dmaData  = {};
        dmaData.dstSel       = CPDMA_DST_SEL_DST_ADDR;
        dmaData.dstAddr      = dstGpuMemory.Desc().gpuVirtAddr + dstMemOffset;
        dmaData.dstAddrSpace = CPDMA_ADDR_SPACE_MEM;
        dmaData.srcSel       = CPDMA_SRC_SEL_GDS;
        dmaData.srcAddr      = srcGdsOffset;
        dmaData.srcAddrSpace = CPDMA_ADDR_SPACE_MEM;
        dmaData.numBytes     = static_cast<uint32>(size);
        dmaData.sync         = true;
        dmaData.usePfp       = false;
        pCmdSpace += pCmdUtil->BuildDmaData(dmaData, pCmdSpace);
    }
    else
    {
        // Depending on what HW pipeline point is used we have to use different event type.
        VGT_EVENT_TYPE eventType = PS_DONE;
        switch (pipePoint)
        {
        case HwPipePreRasterization:
        case HwPipePostPs:
            eventType = PS_DONE;
            break;
        case HwPipePostCs:
            eventType = CS_DONE;
            break;
        case HwPipePostBlt:
        case HwPipeBottom:
            if (forComputeEngine)
            {
                // For compute engines bottom of pipe is practically equivalent with CS_DONE.
                eventType = CS_DONE;
            }
            else
            {
                // Do a partial CS flush first, as there's no way to tell CP to write GDS after bottom of pipe,
                // so after the partial CS flush we'll simply use PS_DONE instead.
                pCmdSpace += pCmdUtil->BuildEventWrite(CS_PARTIAL_FLUSH, pCmdSpace);
                eventType = PS_DONE;
            }
            break;
        default:
            PAL_ASSERT(!"Unexpected HW pipeline point");
        }

        // WRITE_EVENT requires GDS offset and size to be in dwords.
        const uint32 gdsDwordOffset = static_cast<uint32>(srcGdsOffset / sizeof(uint32));
        const uint32 gdsDwordSize   = static_cast<uint32>(size / sizeof(uint32));

        // Use WRITE_EVENT to copy from GDS to memory.
        pCmdSpace += pCmdUtil->BuildGenericEosEvent(eventType,
                                                    dstGpuMemory.Desc().gpuVirtAddr + dstMemOffset,
                                                    EVENT_WRITE_EOS_CMD_STORE_GDS_DATA_TO_MEMORY,
                                                    0,
                                                    gdsDwordOffset,
                                                    gdsDwordSize,
                                                    forComputeEngine,
                                                    pCmdSpace);

        if (waitForWC)
        {
            // This will need a lot of extra space, so commit current commands and reserve a new chunk.
            pCmdStream->CommitCommands(pCmdSpace);
            pCmdSpace = pCmdStream->ReserveCommands();

            pCmdSpace += pCmdUtil->BuildWaitOnGenericEosEvent(eventType,
                                                              fenceAddr,
                                                              forComputeEngine,
                                                              pCmdSpace);
        }
    }

    pCmdStream->CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// Updates data in GDS before the specified HW pipeline point.
void BuildUpdateGds(
    CmdStream*        pCmdStream,
    const CmdUtil*    pCmdUtil,
    HwPipePoint       pipePoint,
    uint32            gdsOffset,
    uint32            dataSize,
    const uint32*     pData)
{
    PAL_ASSERT(((gdsOffset % 4) == 0) && ((dataSize % 4) == 0) && (pData != nullptr));

    // We'll need to know how many DWORDs we can write in a single WRITE_DATA packet without exceeding the size
    // of the reserve buffer.
    const uint32 maxDwordsPerBatch = pCmdStream->ReserveLimit() - CmdUtil::GetWriteDataHeaderSize();

    uint32 dataDwords = static_cast<uint32>(dataSize / sizeof(uint32));

    while (dataDwords > 0)
    {
        const uint32 batchDwords = Min(dataDwords, maxDwordsPerBatch);

        uint32* pCmdSpace = pCmdStream->ReserveCommands();

        // Use WRITE_DATA to update the contents of the GDS.
        pCmdSpace += pCmdUtil->BuildWriteData(gdsOffset,
                                              batchDwords,
                                              WRITE_DATA_ENGINE_ME,
                                              WRITE_DATA_DST_SEL_GDS,
                                              true,
                                              pData,
                                              PredDisable,
                                              pCmdSpace);

        pCmdStream->CommitCommands(pCmdSpace);

        dataDwords  -= batchDwords;
        gdsOffset   += batchDwords * sizeof(uint32);
        pData       += batchDwords;
    }
}

// =====================================================================================================================
// Fills data in GDS before the specified HW pipeline point.
void BuildFillGds(
    CmdStream*        pCmdStream,
    const CmdUtil*    pCmdUtil,
    HwPipePoint       pipePoint,
    uint32            gdsOffset,
    uint32            fillSize,
    uint32            data)
{
    PAL_ASSERT(((gdsOffset % 4) == 0) && ((fillSize % 4) == 0));

    uint32* pCmdSpace = pCmdStream->ReserveCommands();

    // Use DMA_DATA to fill GDS range.
    DmaDataInfo dmaData  = {};
    dmaData.dstSel       = CPDMA_DST_SEL_GDS;
    dmaData.dstAddr      = gdsOffset;
    dmaData.dstAddrSpace = CPDMA_ADDR_SPACE_MEM;
    dmaData.srcSel       = CPDMA_SRC_SEL_DATA;
    dmaData.srcData      = data;
    dmaData.numBytes     = static_cast<uint32>(fillSize);
    dmaData.sync         = true;
    dmaData.usePfp       = false;
    pCmdSpace += pCmdUtil->BuildDmaData(dmaData, pCmdSpace);

    pCmdStream->CommitCommands(pCmdSpace);
}

} // Gfx6
} // Pal
