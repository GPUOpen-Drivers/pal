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

#include "core/cmdStream.h"
#include "core/hw/ossip/oss1/oss1DmaCmdBuffer.h"
#include "core/addrMgr/addrMgr1/addrMgr1.h"
#include "core/hw/ossip/oss1/sdma10_pkt_struct.h"
#include "palFormatInfo.h"

using namespace Util;

namespace Pal
{
namespace Oss1
{

// =====================================================================================================================
DmaCmdBuffer::DmaCmdBuffer(
    Device*                    pDevice,
    const CmdBufferCreateInfo& createInfo)
    :
    Pal::DmaCmdBuffer(pDevice->Parent(), createInfo, false)
{
    // Regarding copyOverlapHazardSyncs value in the constructor above:
    //   DMA (OSS 1.0) does not by default enable overlapped copies (DMA[1]_FIFO_CNTL.[COPY_OVERLAP_ENABLE|
    //   WRITE_OVERLAP_ENABLE] = 0), so the driver does not need to handle any synchronization because of it.
}

// =====================================================================================================================
// Writes a packet that waits for the given GPU event to be set. Returns the next unused DWORD in pCmdSpace.
uint32* DmaCmdBuffer::WriteWaitEventSet(
    const GpuEvent& gpuEvent,
    uint32*         pCmdSpace
    ) const
{
    constexpr size_t PacketDwords = sizeof(DMA_CMD_PACKET_POLL_REG_MEM) / sizeof(uint32);
    auto*const       pPacket      = reinterpret_cast<DMA_CMD_PACKET_POLL_REG_MEM*>(pCmdSpace);
    const gpusize    gpuVirtAddr  = gpuEvent.GetBoundGpuMemory().GpuVirtAddr();

    DMA_CMD_PACKET_POLL_REG_MEM packet;

    packet.count         = 0;
    packet.reserved      = 0;
    packet.mem           = 1;                            // Memory space poll.
    packet.type          = DMA_COMMAND_POLL_REG_MEM;
    packet.reserved0     = 0;
    packet.addr_lo       = LowPart(gpuVirtAddr) >> 2;
    packet.addr_hi       = HighPart(gpuVirtAddr) & 0xFF;
    packet.reserve1      = 0;
    packet.retry_count   = 0xFFF;                        // Retry infinitely.
    packet.reserve2      = 0;
    packet.mask          = 0xFFFFFFFF;
    packet.reference     = GpuEvent::SetValue;
    packet.poll_interval = 0xA;                          // Wait 160 clocks before each retry.
    packet.reserve3      = 0;
    packet.func          = 0x3;                          // Equal
    packet.reserve4      = 0;

    *pPacket = packet;

    return pCmdSpace + PacketDwords;
}

// =====================================================================================================================
// Adds a preamble to the start of a new command buffer.
Result DmaCmdBuffer::AddPreamble()
{
    // If this trips, it means that this isn't really the preamble -- i.e., somebody has inserted something into the
    // command stream before the preamble.  :-(
    PAL_ASSERT(m_cmdStream.IsEmpty());

    // Adding a NOP preamble ensures that we always have something to submit (i.e,. the app can't submit an empty
    // command buffer which causes problems to the submit routine).
    uint32* pCmdSpace = m_cmdStream.ReserveCommands();

    pCmdSpace = WriteNops(pCmdSpace, 1);

    m_cmdStream.CommitCommands(pCmdSpace);

    return Result::Success;
}

// =====================================================================================================================
// Adds a postamble to the end of a new command buffer. This will add a mem semaphore (signal) packet to increment the
// completion count of the command buffer when the GPU has finished executing it.
Result DmaCmdBuffer::AddPostamble()
{
    uint32* pCmdSpace = m_cmdStream.ReserveCommands();

    if (m_cmdStream.GetFirstChunk()->BusyTrackerGpuAddr() != 0)
    {
        const gpusize gpuAddr = m_cmdStream.GetFirstChunk()->BusyTrackerGpuAddr();
        auto*const    pPacket = reinterpret_cast<DMA_CMD_PACKET_SEMAPHORE*>(pCmdSpace);

        DMA_CMD_PACKET_SEMAPHORE packet;

        packet.header.semaphoreHeaderCayman.u32All      = 0;
        packet.header.semaphoreHeaderCayman.bits.type   = DMA_COMMAND_SEMAPHORE;
        packet.header.semaphoreHeaderCayman.bits.signal = 1;

        packet.reserved1     = 0;
        packet.sem_addr_low  = LowPart(gpuAddr >> 3);
        packet.sem_addr_high = HighPart(gpuAddr) & 0xFF;
        packet.reserved2     = 0;

        *pPacket = packet;

        pCmdSpace = reinterpret_cast<uint32*>(pPacket + 1);
    }

    m_cmdStream.CommitCommands(pCmdSpace);

    return Result::Success;
}

// =====================================================================================================================
// Writes a COND_EXE packet to predicate the next packets based on a memory value. Returns the next unused DWORD in
// pCmdSpace.
uint32* DmaCmdBuffer::WritePredicateCmd(
    size_t  predicateDwords,
    uint32* pCmdSpace
    ) const
{
    constexpr size_t PacketDwords = sizeof(DMA_CMD_CONDITIONAL_EXECUTION) / sizeof(uint32);
    auto*const       pPacket      = reinterpret_cast<DMA_CMD_CONDITIONAL_EXECUTION*>(pCmdSpace);

    DMA_CMD_CONDITIONAL_EXECUTION packet;

    packet.header.u32All           = 0;
    packet.header.bits.type        = DMA_COMMAND_CONDITIONAL_EXECUTION;
    packet.header.bits.count       = predicateDwords;
    packet.dstAddrLo.u32All        = LowPart(m_predMemAddress);
    packet.dstAddrHi.u32All        = 0;
    packet.dstAddrHi.bits.AddrHi   = HighPart(m_predMemAddress);

    *pPacket = packet;

    return pCmdSpace + PacketDwords;
}

// =====================================================================================================================
// Patches a COND_EXE packet with the given predication size.
void DmaCmdBuffer::PatchPredicateCmd(size_t predicateDwords, void* pPredicateCmd) const
{
    auto*const pPacket = reinterpret_cast<DMA_CMD_CONDITIONAL_EXECUTION*>(pPredicateCmd);

    pPacket->header.bits.count = predicateDwords;
}

// =====================================================================================================================
// Copies "copySize" bytes from srcAddr to dstAddr. This function will transfer as much as it can, but it is the
// caller's responsibility to keep calling this function until all the requested data has been copied. Returns the next
// unused DWORD in pCmdSpace.
//
uint32* DmaCmdBuffer::WriteCopyGpuMemoryCmd(
    gpusize      srcGpuAddr,
    gpusize      dstGpuAddr,
    gpusize      copySize,
    DmaCopyFlags copyFlags,
    uint32*      pCmdSpace,
    gpusize*     pBytesCopied // [out] How many bytes out of copySize this call was able to transfer.
    ) const
{
    // The spec indicates that the max size should be 0xfffff, but
    //     "Due to HW limitation, the maximum count may not be 2^n-1, can only be 2^n - 1 - start_addr[4:2]".
    //
    // Note that this is a worst-case of 2^n - 8, but doing the real calculation allows us to copy the most amount of
    // data possible.
    const gpusize maxTransferSize = (1ull << 20) - 1 - ((srcGpuAddr & 0x1C) >> 2);
    bool useDwordCopyCmd = false;

    // If the source and destination address are both dword-aligned, and we have at least one dword to copy, then we
    // can use a dword copy to get this portion of the copy
    if (IsPow2Aligned(srcGpuAddr, sizeof(uint32)) &&
        IsPow2Aligned(dstGpuAddr, sizeof(uint32)) &&
        (copySize >= sizeof(uint32)))
    {
        useDwordCopyCmd = true;
    }

    if (useDwordCopyCmd && (copyFlags == DmaCopyFlags::None))
    {
        PAL_ASSERT(copyFlags == DmaCopyFlags::None);
        const gpusize dwordsCopied = Min(copySize / sizeof(uint32), maxTransferSize);
        *pBytesCopied = dwordsCopied * sizeof(uint32);

        constexpr size_t PacketDwords = sizeof(DMA_CMD_PACKET_COPY) / sizeof(uint32);
        auto*const       pPacket      = reinterpret_cast<DMA_CMD_PACKET_COPY*>(pCmdSpace);

        DMA_CMD_PACKET_COPY packet;

        packet.header.ibHeaderSI.u32All     = 0;
        packet.header.ibHeaderSI.bits.type  = DMA_COMMAND_COPY;
        packet.header.ibHeaderSI.bits.count = dwordsCopied;
        packet.dstAddrLo.u32All             = LowPart(dstGpuAddr);
        packet.srcAddrLo.u32All             = LowPart(srcGpuAddr);
        packet.dstAddrHi.u32All             = 0;
        packet.dstAddrHi.bits.dstAddrHi     = HighPart(dstGpuAddr);
        packet.srcAddrHi.u32All             = 0;
        packet.srcAddrHi.bits.srcAddrHi     = HighPart(srcGpuAddr);

        *pPacket = packet;

        pCmdSpace += PacketDwords;
    }
    else
    {
        // Ok, we need to do a byte copy here.
        PAL_ASSERT(copyFlags == DmaCopyFlags::None);
        *pBytesCopied = Min(copySize, maxTransferSize);

        constexpr size_t PacketDwords = sizeof(DMA_CMD_LINEAR_BYTE_COPY) / sizeof(uint32);
        auto*const       pPacket      = reinterpret_cast<DMA_CMD_LINEAR_BYTE_COPY*>(pCmdSpace);

        DMA_CMD_LINEAR_BYTE_COPY packet;

        packet.header.u32All               = 0;
        packet.header.bits.type            = DMA_COMMAND_COPY;
        packet.header.bits.r8xxcmd         = 1;
        packet.header.bits.count           = *pBytesCopied;
        packet.dstAddrLo.u32All            = LowPart(dstGpuAddr);
        packet.srcAddrLo.u32All            = LowPart(srcGpuAddr);
        packet.dstAddrHi.u32All            = 0;
        packet.dstAddrHi.bits.linearAddrHi = HighPart(dstGpuAddr);
        packet.srcAddrHi.u32All            = 0;
        packet.srcAddrHi.bits.linearAddrHi = HighPart(srcGpuAddr);

        *pPacket = packet;

        pCmdSpace += PacketDwords;
    }

    return pCmdSpace;
}
// =====================================================================================================================
// Copies memory into the specified region of a typed buffer (linear image). Returns the next unused DWORD in pCmdSpace.
//
uint32* DmaCmdBuffer::WriteCopyTypedBuffer(
    const DmaTypedBufferCopyInfo& typedBufferInfo,
    uint32*                       pCmdSpace
    ) const
{
    Extent3d nextCopyExtent;
    Offset3d nextCopyOffset;
    Offset3d regionOffset       = {}; // Region offset={0,0,0} since src/dst base addr is actual addr of region
    int32 totalWidthCopied      = 0;

    // Calculating pitch in bytes
    uint32 srcRowPitchInBytes   = typedBufferInfo.src.linearRowPitch * typedBufferInfo.src.bytesPerElement;
    uint32 dstRowPitchInBytes   = typedBufferInfo.dst.linearRowPitch * typedBufferInfo.dst.bytesPerElement;
    uint32 srcSlicePitchInBytes = typedBufferInfo.src.linearDepthPitch * typedBufferInfo.src.bytesPerElement;
    uint32 dstSlicePitchInBytes = typedBufferInfo.dst.linearDepthPitch * typedBufferInfo.dst.bytesPerElement;

    while (totalWidthCopied < static_cast<int32>(typedBufferInfo.copyExtent.width))
    {
        // Need to adjust copy extents. Work around for hardware bug.
        GetNextExtentAndOffset(typedBufferInfo.copyExtent,
                               regionOffset,
                               typedBufferInfo.src.bytesPerElement,
                               totalWidthCopied,
                               &nextCopyExtent,
                               &nextCopyOffset);

        // Calculating new base address of src/dst based on updated offsets
        gpusize srcLinearBaseAddr = typedBufferInfo.src.baseAddr
                                    + (nextCopyOffset.z * srcSlicePitchInBytes)
                                    + (nextCopyOffset.y * srcRowPitchInBytes)
                                    + (nextCopyOffset.x * typedBufferInfo.src.bytesPerElement);

        gpusize dstLinearBaseAddr = typedBufferInfo.dst.baseAddr
                                    + (nextCopyOffset.z * dstSlicePitchInBytes)
                                    + (nextCopyOffset.y * dstRowPitchInBytes)
                                    + (nextCopyOffset.x * typedBufferInfo.dst.bytesPerElement);

        // This packet only works with DWORD aligned addresses and the copy region must have a DWORD-aligned byte width.
        PAL_ASSERT(IsPow2Aligned(srcLinearBaseAddr, sizeof(uint32)) &&
            IsPow2Aligned(dstLinearBaseAddr, sizeof(uint32)) &&
            IsPow2Aligned(nextCopyExtent.width * typedBufferInfo.dst.bytesPerElement, sizeof(uint32)));

        constexpr size_t PacketDwords   = sizeof(DMA_CMD_PACKET_L2L_PARTIAL_COPY) / sizeof(uint32);
        auto*const       pPacket        = reinterpret_cast<DMA_CMD_PACKET_L2L_PARTIAL_COPY*>(pCmdSpace);

        DMA_CMD_PACKET_L2L_PARTIAL_COPY packet;

        packet.header.headerCayman.u32All         = 0;
        packet.header.headerCayman.bits.type      = DMA_COMMAND_COPY;
        packet.header.headerCayman.bits.r8xxcmd   = 1;
        packet.header.headerCayman.bits.idcmd     = 1; // because the docs say so

        // Source / Dest Slice Pitch : pitch for one slice(Unit Byte)
        packet.srcAddrLo.u32All       = LowPart(srcLinearBaseAddr);
        packet.srcAddrHi.u32All       = 0;
        packet.srcAddrHi.bits.addrHi  = HighPart(srcLinearBaseAddr);
        packet.srcAddrHi.bits.pitch   = srcRowPitchInBytes;
        packet.srcSlicePitch          = srcSlicePitchInBytes;

        packet.dstAddrLo.u32All       = LowPart(dstLinearBaseAddr);
        packet.dstAddrHi.u32All       = 0;
        packet.dstAddrHi.bits.addrHi  = HighPart(dstLinearBaseAddr);
        packet.dstAddrHi.bits.pitch   = dstRowPitchInBytes;
        packet.dstSlicePitch          = dstSlicePitchInBytes;

        // DX/DY/DZ: sub-window size (unit: pixel)
        packet.sizeXY.u32All  = 0;
        packet.sizeXY.bits.dX = nextCopyExtent.width;
        packet.sizeXY.bits.dY = nextCopyExtent.height;
        packet.sizeZ.u32All   = 0;
        packet.sizeZ.bits.dZ  = nextCopyExtent.depth;

        // Size: Log2(bpp / 8)
        packet.sizeZ.bits.size = Log2(typedBufferInfo.dst.bytesPerElement);

        *pPacket = packet;

        totalWidthCopied    += nextCopyExtent.width;
        pCmdSpace           += PacketDwords;
    }

    return pCmdSpace;
}
// =====================================================================================================================
// Copies the specified region between two linear images.
//
void DmaCmdBuffer::WriteCopyImageLinearToLinearCmd(
    const DmaImageCopyInfo& imageCopyInfo)
{
    uint32*                 pCmdSpace        = m_cmdStream.ReserveCommands();
    DmaImageCopyInfo        nextCopyInfo     = imageCopyInfo;
    const DmaImageCopyInfo* pCopyInfo        = &nextCopyInfo;
    int32                   totalWidthCopied = 0;

    while (totalWidthCopied < static_cast<int32>(imageCopyInfo.copyExtent.width))
    {
        GetNextExtentAndOffset(imageCopyInfo.copyExtent,
                               imageCopyInfo.src.offset,
                               imageCopyInfo.src.bytesPerPixel,
                               totalWidthCopied,
                               &nextCopyInfo.copyExtent,
                               &nextCopyInfo.src.offset);

        GetNextExtentAndOffset(imageCopyInfo.copyExtent,
                               imageCopyInfo.dst.offset,
                               imageCopyInfo.dst.bytesPerPixel,
                               totalWidthCopied,
                               &nextCopyInfo.copyExtent,
                               &nextCopyInfo.dst.offset);

        // This packet only works with DWORD aligned addresses and the copy region must have a DWORD-aligned byte width.
        PAL_ASSERT(IsPow2Aligned(CalcLinearBaseAddr(pCopyInfo->src), sizeof(uint32)) &&
                   IsPow2Aligned(CalcLinearBaseAddr(pCopyInfo->dst), sizeof(uint32)) &&
                   IsPow2Aligned(nextCopyInfo.copyExtent.width * pCopyInfo->dst.bytesPerPixel, sizeof(uint32)));

        constexpr size_t PacketDwords = sizeof(DMA_CMD_PACKET_L2L_PARTIAL_COPY) / sizeof(uint32);
        auto*const       pPacket      = reinterpret_cast<DMA_CMD_PACKET_L2L_PARTIAL_COPY*>(pCmdSpace);

        DMA_CMD_PACKET_L2L_PARTIAL_COPY packet;

        packet.header.headerCayman.u32All       = 0;
        packet.header.headerCayman.bits.type    = DMA_COMMAND_COPY;
        packet.header.headerCayman.bits.r8xxcmd = 1;
        packet.header.headerCayman.bits.idcmd   = 1; // because the docs say so

        SetupLinearAddrAndSlicePitch(pCopyInfo->src, &packet.srcAddrLo.u32All);
        SetupLinearAddrAndSlicePitch(pCopyInfo->dst, &packet.dstAddrLo.u32All);

        packet.sizeXY.u32All   = 0;
        packet.sizeXY.bits.dX  = pCopyInfo->copyExtent.width;
        packet.sizeXY.bits.dY  = pCopyInfo->copyExtent.height;
        packet.sizeZ.u32All    = 0;
        packet.sizeZ.bits.dZ   = pCopyInfo->copyExtent.depth;
        packet.sizeZ.bits.size = Log2(pCopyInfo->dst.bytesPerPixel);

        *pPacket = packet;

        totalWidthCopied += nextCopyInfo.copyExtent.width;
        pCmdSpace        += PacketDwords;
    }

    m_cmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// Linear image to tiled image copy
void DmaCmdBuffer:: WriteCopyImageLinearToTiledCmd(
    const DmaImageCopyInfo& imageCopyInfo)
{
    uint32*  pCmdSpace = m_cmdStream.ReserveCommands();

    pCmdSpace = CopyImageLinearTiledTransform(imageCopyInfo, imageCopyInfo.src, imageCopyInfo.dst, false, pCmdSpace);

    m_cmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// Tiled image to linear image copy
void DmaCmdBuffer::WriteCopyImageTiledToLinearCmd(
    const DmaImageCopyInfo& imageCopyInfo)
{
    uint32*  pCmdSpace = m_cmdStream.ReserveCommands();

    pCmdSpace = CopyImageLinearTiledTransform(imageCopyInfo, imageCopyInfo.dst, imageCopyInfo.src, true, pCmdSpace);

    m_cmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// Returns true if scanline copies are required for a tiled-to-tiled image copy
bool DmaCmdBuffer::UseT2tScanlineCopy(
    const DmaImageCopyInfo& imageCopyInfo
    ) const
{
    constexpr  Extent3d  RequiredAlignments = { 8, 8, 1 };

    const DmaImageInfo& src          = imageCopyInfo.src;
    const DmaImageInfo& dst          = imageCopyInfo.dst;
    const uint32        srcTileToken = src.pSubresInfo->tileToken;
    const uint32        dstTileToken = dst.pSubresInfo->tileToken;

    // Assume that scanline copies will be required.
    bool  useScanlineCopy = true;

    // According to the packet spec:
    //    src_X/Y (14): must aligned to tile 8 pixel boundary
    //    dst_X/Y (14): must aligned to tile 8 pixel boundary
    //    rect_X/Y(14): must aligned to tile 8 pixel boundary
    //    Both images should have the same:
    //             micro_tile_mode,
    //             element_size
    if ((srcTileToken == dstTileToken)                                &&
        IsAlignedForT2t(src.offset,               RequiredAlignments) &&
        IsAlignedForT2t(dst.offset,               RequiredAlignments) &&
        IsAlignedForT2t(imageCopyInfo.copyExtent, RequiredAlignments))
    {
        // Now we can try to use the built-in packet!
        useScanlineCopy = false;
    }

    // Beyond the documented T2T packet restricitons, there is an apparent hardware bug with OSS 1.0
    // that causes corruption when copying from a 2D to a 3D image where the source array-slice doesn't
    // match the destination Z-slice.
    if ((src.pImage->GetImageCreateInfo().imageType == ImageType::Tex2d) &&
        (dst.pImage->GetImageCreateInfo().imageType == ImageType::Tex3d) &&
        (dst.offset.z > 0)                                               &&
        (dst.offset.z != src.offset.z))
    {
        useScanlineCopy = true;
    }

    return useScanlineCopy;
}

// =====================================================================================================================
DmaCmdBuffer::DmaMemImageCopyMethod DmaCmdBuffer::GetMemImageCopyMethod(
    bool                         isLinearImg,
    const DmaImageInfo&          imageInfo,
    const MemoryImageCopyRegion& region
    ) const
{
    DmaMemImageCopyMethod copyMethod = DmaMemImageCopyMethod::Native;

    // On OSS 1.0, the x, rect_x, src/dst_pitch and src/dst_slice_pitch must be dword-aligned when
    // expressed in units of bytes for both L2L and L2T copies.
    if (AreMemImageXParamsDwordAligned(imageInfo, region) == false)
    {
        copyMethod = DmaMemImageCopyMethod::DwordUnaligned;
    }

    return copyMethod;
}

// =====================================================================================================================
// Tiled image to tiled image copy.
//
void DmaCmdBuffer::WriteCopyImageTiledToTiledCmd(
    const DmaImageCopyInfo& imageCopyInfo)
{
    // Setup the tile-mode info.
    const auto* pSrcSubresInfo = imageCopyInfo.src.pSubresInfo;
    const auto* pDstSubresInfo = imageCopyInfo.dst.pSubresInfo;

    uint32*          pCmdSpace    = m_cmdStream.ReserveCommands();
    constexpr size_t PacketDwords = sizeof(DMA_CMD_PACKET_T2T_PARTIAL_COPY) / sizeof(uint32);
    auto*const       pPacket      = reinterpret_cast<DMA_CMD_PACKET_T2T_PARTIAL_COPY*>(pCmdSpace);

    DMA_CMD_PACKET_T2T_PARTIAL_COPY packet;

    packet.header.headerCayman.u32All       = 0;
    packet.header.headerCayman.bits.type    = DMA_COMMAND_COPY;
    packet.header.headerCayman.bits.r8xxcmd = 1; // because the docs say so
    packet.header.headerCayman.bits.tiling  = 1; // both images are tiled
    packet.header.headerCayman.bits.idcmd   = 5; // because the docs say so

    // Setup location and size of both surfaces.
    SetupL2tT2lAddrAndSize(imageCopyInfo.src, &packet.srcAddr.u32All);
    SetupL2tT2lAddrAndSize(imageCopyInfo.dst, &packet.dstAddr.u32All);

    AddrMgr1::TileToken tileToken = {};
    tileToken.u32All = pSrcSubresInfo->tileToken;

    packet.info0.u32All             = 0;
    packet.info0.si_bits.array_mode = tileToken.bits.tileMode;
    packet.info0.si_bits.bankheight = tileToken.bits.bankHeight;
    packet.info0.si_bits.bankwidth  = tileToken.bits.bankWidth;
    packet.info0.si_bits.numbank    = tileToken.bits.banks;
    packet.info0.si_bits.mtaspect   = tileToken.bits.macroAspectRatio;
    packet.info0.si_bits.mt_mode    = tileToken.bits.tileType;
    packet.info0.si_bits.tilesplit  = tileToken.bits.tileSplitBytes;
    packet.info0.si_bits.pixel_size = tileToken.bits.elementSize;

    packet.xInfo1.u32All = 0;
    packet.yInfo1.u32All = 0;
    packet.zInfo2.u32All = 0;

    // Setup the starting corner of the source rectangle.
    packet.xInfo1.bits.src  = imageCopyInfo.src.offset.x >> 3;
    packet.yInfo1.bits.src  = imageCopyInfo.src.offset.y >> 3;
    packet.zInfo2.si_bits.srcz = imageCopyInfo.src.offset.z;

    // Setup the starting corner of the destination rectangle.
    packet.xInfo1.bits.dst  = imageCopyInfo.dst.offset.x >> 3;
    packet.yInfo1.bits.dst  = imageCopyInfo.dst.offset.y >> 3;
    packet.zInfo2.si_bits.dstz = imageCopyInfo.dst.offset.z;

    packet.dInfo1.u32All  = 0;
    packet.dzInfo3.u32All = 0;

    // Setup the size of the region being copied. The header is messed up as the setting for the height and width
    // dimensions is called "src" and "dst" respectively.
    packet.dInfo1.bits.src    = imageCopyInfo.copyExtent.height >> 3;
    packet.dInfo1.bits.dst    = imageCopyInfo.copyExtent.width  >> 3;
    packet.dzInfo3.si_bits.dz = imageCopyInfo.copyExtent.depth;

    *pPacket = packet;

    m_cmdStream.CommitCommands(pCmdSpace + PacketDwords);
}

// =====================================================================================================================
// Copies memory into the specified region of a linear image. Returns the next unused DWORD in pCmdSpace.
//
uint32* DmaCmdBuffer::WriteCopyMemToLinearImageCmd(
    const GpuMemory&             srcGpuMemory,
    const DmaImageInfo&          dstImage,
    const MemoryImageCopyRegion& rgn,
    uint32*                      pCmdSpace
    ) const
{
    DmaImageInfo dstImageInfo          = dstImage;
    int32        totalWidthCopiedSoFar = 0;

    while (totalWidthCopiedSoFar < static_cast<int32>(rgn.imageExtent.width))
    {
        Extent3d nextExtent;

        GetNextExtentAndOffset(rgn.imageExtent,
                               rgn.imageOffset,
                               dstImage.bytesPerPixel,
                               totalWidthCopiedSoFar,
                               &nextExtent,
                               &dstImageInfo.offset);

        const gpusize srcAddr = srcGpuMemory.Desc().gpuVirtAddr +
                                rgn.gpuMemoryOffset             +
                                totalWidthCopiedSoFar * dstImage.bytesPerPixel;

        // This packet only works with DWORD aligned addresses and the copy region must have a DWORD-aligned byte width.
        PAL_ASSERT(IsPow2Aligned(srcAddr, sizeof(uint32)) &&
                   IsPow2Aligned(CalcLinearBaseAddr(dstImageInfo), sizeof(uint32)) &&
                   IsPow2Aligned(nextExtent.width * dstImage.bytesPerPixel, sizeof(uint32)));

        constexpr size_t PacketDwords = sizeof(DMA_CMD_PACKET_L2L_PARTIAL_COPY) / sizeof(uint32);
        auto*const       pPacket      = reinterpret_cast<DMA_CMD_PACKET_L2L_PARTIAL_COPY*>(pCmdSpace);

        DMA_CMD_PACKET_L2L_PARTIAL_COPY packet;

        packet.header.headerCayman.u32All       = 0;
        packet.header.headerCayman.bits.type    = DMA_COMMAND_COPY;
        packet.header.headerCayman.bits.r8xxcmd = 1;
        packet.header.headerCayman.bits.idcmd   = 1;

        packet.srcAddrLo.u32All      = LowPart(srcAddr);
        packet.srcAddrHi.u32All      = 0;
        packet.srcAddrHi.bits.addrHi = HighPart(srcAddr);
        packet.srcAddrHi.bits.pitch  = static_cast<uint32>(rgn.gpuMemoryRowPitch);
        packet.srcSlicePitch         = static_cast<uint32>(rgn.gpuMemoryDepthPitch);

        SetupLinearAddrAndSlicePitch(dstImageInfo, &packet.dstAddrLo.u32All);

        packet.sizeXY.u32All   = 0;
        packet.sizeXY.bits.dX  = nextExtent.width;
        packet.sizeXY.bits.dY  = nextExtent.height;
        packet.sizeZ.u32All    = 0;
        packet.sizeZ.bits.dZ   = nextExtent.depth;
        packet.sizeZ.bits.size = Log2(dstImage.bytesPerPixel);

        *pPacket = packet;

        totalWidthCopiedSoFar += nextExtent.width;
        pCmdSpace             += PacketDwords;
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Copies the specified region of a linear image into memory. Returns the next unused DWORD in pCmdSpace.
uint32* DmaCmdBuffer::WriteCopyLinearImageToMemCmd(
    const DmaImageInfo&          srcImage,
    const GpuMemory&             dstGpuMemory,
    const MemoryImageCopyRegion& rgn,
    uint32*                      pCmdSpace
    ) const
{
    DmaImageInfo srcImageInfo          = srcImage;
    int32        totalWidthCopiedSoFar = 0;

    while (totalWidthCopiedSoFar < static_cast<int32>(rgn.imageExtent.width))
    {
        Extent3d nextExtent;

        GetNextExtentAndOffset(rgn.imageExtent,
                               rgn.imageOffset,
                               srcImage.bytesPerPixel,
                               totalWidthCopiedSoFar,
                               &nextExtent,
                               &srcImageInfo.offset);

        const gpusize dstAddr = dstGpuMemory.Desc().gpuVirtAddr +
                                rgn.gpuMemoryOffset             +
                                totalWidthCopiedSoFar * srcImage.bytesPerPixel;

        // This packet only works with DWORD aligned addresses and the copy region must have a DWORD-aligned byte width.
        PAL_ASSERT(IsPow2Aligned(dstAddr, sizeof(uint32)) &&
                   IsPow2Aligned(CalcLinearBaseAddr(srcImageInfo), sizeof(uint32)) &&
                   IsPow2Aligned(nextExtent.width * srcImage.bytesPerPixel, sizeof(uint32)));

        constexpr size_t PacketDwords = sizeof(DMA_CMD_PACKET_L2L_PARTIAL_COPY) / sizeof(uint32);
        auto*const       pPacket      = reinterpret_cast<DMA_CMD_PACKET_L2L_PARTIAL_COPY*>(pCmdSpace);

        DMA_CMD_PACKET_L2L_PARTIAL_COPY packet;

        packet.header.headerCayman.u32All       = 0;
        packet.header.headerCayman.bits.type    = DMA_COMMAND_COPY;
        packet.header.headerCayman.bits.r8xxcmd = 1;
        packet.header.headerCayman.bits.idcmd   = 1;

        SetupLinearAddrAndSlicePitch(srcImageInfo, &packet.srcAddrLo.u32All);

        packet.dstAddrLo.u32All      = LowPart(dstAddr);
        packet.dstAddrHi.u32All      = 0;
        packet.dstAddrHi.bits.addrHi = HighPart(dstAddr);
        packet.dstAddrHi.bits.pitch  = static_cast<uint32>(rgn.gpuMemoryRowPitch);
        packet.dstSlicePitch         = static_cast<uint32>(rgn.gpuMemoryDepthPitch);

        packet.sizeXY.u32All   = 0;
        packet.sizeXY.bits.dX  = nextExtent.width;
        packet.sizeXY.bits.dY  = nextExtent.height;
        packet.sizeZ.u32All    = 0;
        packet.sizeZ.bits.dZ   = nextExtent.depth;
        packet.sizeZ.bits.size = Log2(srcImage.bytesPerPixel);

        *pPacket = packet;

        totalWidthCopiedSoFar += nextExtent.width;
        pCmdSpace             += PacketDwords;
    }

    return pCmdSpace;
}

// =====================================================================================================================
void DmaCmdBuffer::CmdUpdateMemory(
    const IGpuMemory& dstGpuMemory,
    gpusize           dstOffset,
    gpusize           dataSize,
    const uint32*     pData)
{
    gpusize dstAddr = dstGpuMemory.Desc().gpuVirtAddr + dstOffset;

    // Both the destination address and the dataSize need to be dword aligned, so verify that here.
    PAL_ASSERT(IsPow2Aligned(dstAddr,  sizeof(uint32)));
    PAL_ASSERT(IsPow2Aligned(dataSize, sizeof(uint32)));

    // Get the size of the packet header and the upper limit on its data payload.
    constexpr uint32 PacketHdrSizeInDwords = sizeof(DMA_CMD_PACKET_WRITE) / sizeof(uint32);
    constexpr uint32 PacketMaxDataInDwords = (1u << 20) - 1;

    // Given that PacketMaxDataInDwords is quite large, we're likely limited by the size of the reserve buffer.
    const uint32 maxDataDwords = Min(m_cmdStream.ReserveLimit() - PacketHdrSizeInDwords, PacketMaxDataInDwords);

    // Loop until we've submitted enough packets to upload the whole src buffer.
    const uint32* pRemainingSrcData   = pData;
    uint32        remainingDataDwords = static_cast<uint32>(dataSize) / sizeof(uint32);
    while (remainingDataDwords > 0)
    {
        const uint32 packetDataDwords = Min(remainingDataDwords, maxDataDwords);

        // Setup the packet.
        uint32*    pCmdSpace = m_cmdStream.ReserveCommands();
        auto*const pPacket   = reinterpret_cast<DMA_CMD_PACKET_WRITE*>(pCmdSpace);

        DMA_CMD_PACKET_WRITE packet;

        packet.header.ibHeaderSI.u32All     = 0;
        packet.header.ibHeaderSI.bits.type  = DMA_COMMAND_WRITE;
        packet.header.ibHeaderSI.bits.count = packetDataDwords;
        packet.dstAddrLo.u32All             = LowPart(dstAddr);
        packet.dstAddrHi.u32All             = 0;
        packet.dstAddrHi.bits.dstAddrHi     = HighPart(dstAddr) & 0xFF;

        *pPacket = packet;

        pCmdSpace += PacketHdrSizeInDwords;

        // Copy the next block of source data into the command stream as well.
        memcpy(pCmdSpace, pRemainingSrcData, packetDataDwords * sizeof(uint32));

        pCmdSpace += packetDataDwords;

        m_cmdStream.CommitCommands(pCmdSpace);

        // Update all variable addresses and sizes.
        remainingDataDwords -= packetDataDwords;
        pRemainingSrcData   += packetDataDwords;
        dstAddr             += packetDataDwords * sizeof(uint32);
    }
}

// =====================================================================================================================
// Performs a memset on the specified memory region using the specified "data" value. Returns the next unused DWORD in
// pCmdSpace.
//
uint32* DmaCmdBuffer::WriteFillMemoryCmd(
    gpusize  dstAddr,
    gpusize  byteSize,
    uint32   data,
    uint32*  pCmdSpace,
    gpusize* pBytesCopied // [out] How many bytes out of byteSize this call was able to transfer.
    ) const
{
    constexpr size_t PacketDwords = sizeof(DMA_CMD_PACKET_CONSTANT_FILL) / sizeof(uint32);
    auto*const       pPacket      = reinterpret_cast<DMA_CMD_PACKET_CONSTANT_FILL*>(pCmdSpace);

    DMA_CMD_PACKET_CONSTANT_FILL packet;

    // The fillSize provided by the caller is in bytes, but this packet always take the fill size in terms of dwords.
    constexpr gpusize MaxFillSize  = (1ull << 20) - 1;
    *pBytesCopied = Min(MaxFillSize * sizeof(uint32), byteSize);

    packet.header.ibHeaderSI.u32All     = 0;
    packet.header.ibHeaderSI.bits.type  = DMA_COMMAND_CONSTANT_FILL;
    packet.header.ibHeaderSI.bits.count = *pBytesCopied / sizeof(uint32);
    packet.dstAddrLo.u32All             = LowPart(dstAddr);
    packet.sourceData.bits.fillPattern  = data;
    packet.dstAddrHi.u32All             = 0;
    packet.dstAddrHi.bits.dstAddrHi     = HighPart(dstAddr);

    *pPacket = packet;

    return pCmdSpace + PacketDwords;
}

// =====================================================================================================================
// Adds commands necessary to write "data" to the specified memory
void DmaCmdBuffer::WriteEventCmd(
    const BoundGpuMemory& boundMemObj,
    HwPipePoint           pipePoint,
    uint32                data)
{
    const gpusize dstAddr = boundMemObj.GpuVirtAddr();

    // Make sure our destination address is dword aligned.
    PAL_ASSERT(IsPow2Aligned(dstAddr, sizeof(uint32)));

    uint32* pCmdSpace = m_cmdStream.ReserveCommands();

    constexpr size_t PacketDwords = sizeof(DMA_CMD_FENCE) / sizeof(uint32);
    auto*const       pPacket      = reinterpret_cast<DMA_CMD_FENCE*>(pCmdSpace);

    DMA_CMD_FENCE packet;

    packet.header.u32All    = 0;
    packet.header.bits.type = DMA_COMMAND_FENCE;
    packet.v                = 1;  // GPU address is virtual.
    packet.reserved1        = 0;
    packet.fence_base_lo    = LowPart(dstAddr) >> 2;
    packet.fence_base_hi    = HighPart(dstAddr) & 0xFF;
    packet.reserved2        = 0;
    packet.fence_data       = data;

    *pPacket = packet;

    m_cmdStream.CommitCommands(pCmdSpace + PacketDwords);
}

// =====================================================================================================================
// Writes an immediate value to specified address.
void DmaCmdBuffer::CmdWriteImmediate(
    HwPipePoint        pipePoint,
    uint64             data,
    ImmediateDataWidth dataSize,
    gpusize            address)
{
    // Make sure our destination address is dword aligned.
    PAL_ASSERT(IsPow2Aligned(address, sizeof(uint32)));

    // We only support 32bit data
    PAL_ASSERT(dataSize == ImmediateDataWidth::ImmediateData32Bit);

    uint32* pCmdSpace = m_cmdStream.ReserveCommands();

    constexpr size_t PacketDwords = sizeof(DMA_CMD_FENCE) / sizeof(uint32);
    auto*const       pPacket      = reinterpret_cast<DMA_CMD_FENCE*>(pCmdSpace);

    DMA_CMD_FENCE packet;

    packet.header.u32All    = 0;
    packet.header.bits.type = DMA_COMMAND_FENCE;
    packet.v                = 1;  // GPU address is virtual.
    packet.reserved1        = 0;
    packet.fence_base_lo    = LowPart(address) >> 2;
    packet.fence_base_hi    = HighPart(address) & 0xFF;
    packet.reserved2        = 0;
    packet.fence_data       = data;

    *pPacket = packet;

    m_cmdStream.CommitCommands(pCmdSpace + PacketDwords);
}

// =====================================================================================================================
// Writes a NOP packet.
uint32* DmaCmdBuffer::WriteNops(
    uint32* pCmdSpace,
    uint32  numDwords
    ) const
{
    constexpr size_t PacketDwords = sizeof(DMA_CMD_NOP) / sizeof(uint32);
    static_assert(PacketDwords == 1, "WriteNops implementation assumes NOP packet is 1 dword.");

    DMA_CMD_NOP packet   = { };
    packet.header.bits.type = DMA_COMMAND_NOP;

    auto* pPacket = reinterpret_cast<DMA_CMD_NOP*>(pCmdSpace);

    for (uint32 i = 0; i < numDwords; i++)
    {
        *pPacket++ = packet;
    }

    return reinterpret_cast<uint32*>(pPacket);
}

// =====================================================================================================================
// Either copies a linear image into a tiled one (deTile == false) or vice versa. Returns the next unused DWORD in
// pCmdSpace.
//
uint32* DmaCmdBuffer::CopyImageLinearTiledTransform(
    const DmaImageCopyInfo&  copyInfo,  // info on the images being copied
    const DmaImageInfo&      linearImg, // linear image, source if deTile==false
    const DmaImageInfo&      tiledImg,  // tiled image, source if deTile==true
    bool                     deTile,    // True for copying pTiledImg into pLinearImg
    uint32*                  pCmdSpace)
{
    Extent3d      nextExtent       = {};
    DmaImageInfo  nextLinearImg    = linearImg;
    DmaImageInfo  nextTiledImg     = tiledImg;
    int32         totalWidthCopied = 0;

    while (totalWidthCopied < static_cast<int32>(copyInfo.copyExtent.width))
    {
        GetNextExtentAndOffset(copyInfo.copyExtent,
                               linearImg.offset,
                               linearImg.bytesPerPixel,
                               totalWidthCopied,
                               &nextExtent,
                               &nextLinearImg.offset);

        GetNextExtentAndOffset(copyInfo.copyExtent,
                               tiledImg.offset,
                               tiledImg.bytesPerPixel,
                               totalWidthCopied,
                               &nextExtent,
                               &nextTiledImg.offset);

        PAL_ASSERT(IsPow2Aligned(nextTiledImg.offset.x     * tiledImg.bytesPerPixel, sizeof(uint32)) &&
                   IsPow2Aligned(copyInfo.copyExtent.width * tiledImg.bytesPerPixel, sizeof(uint32)));

        constexpr size_t PacketDwords = sizeof(DMA_CMD_PACKET_L2TT2L_PARTIAL_COPY) / sizeof(uint32);
        auto*const       pPacket      = reinterpret_cast<DMA_CMD_PACKET_L2TT2L_PARTIAL_COPY*>(pCmdSpace);

        DMA_CMD_PACKET_L2TT2L_PARTIAL_COPY packet;

        packet.header.headerCayman.u32All       = 0;
        packet.header.headerCayman.bits.type    = DMA_COMMAND_COPY;
        packet.header.headerCayman.bits.r8xxcmd = 1;
        packet.header.headerCayman.bits.tiling  = 1;
        packet.header.headerCayman.bits.idcmd   = 1;

        SetupL2tT2lAddrAndTileInfo(nextTiledImg, deTile, &packet);

        const gpusize linearAddr = CalcLinearBaseAddr(nextLinearImg);

        packet.linearAddrLo.u32All      = LowPart(linearAddr);
        packet.linearAddrHi.u32All      = 0;
        packet.linearAddrHi.bits.addrHi = HighPart(linearAddr);
        packet.linearAddrHi.bits.pitch  = static_cast<uint32>(nextLinearImg.pSubresInfo->rowPitch);

        packet.linearPitch = static_cast<uint32>(nextLinearImg.pSubresInfo->depthPitch);

        packet.sizeXY.u32All  = 0;
        packet.sizeXY.bits.dX = nextExtent.width;
        packet.sizeXY.bits.dY = nextExtent.height;
        packet.sizeZ.u32All   = 0;
        packet.sizeZ.bits.dZ  = nextExtent.depth;

        *pPacket = packet;

        totalWidthCopied += nextExtent.width;
        pCmdSpace        += PacketDwords;
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Either copies gpuMemory to image (deTile = false) or vice versa. Returns the next unused DWORD in pCmdSpace.
//
uint32* DmaCmdBuffer::CopyImageMemTiledTransform(
    const DmaImageInfo&          image,
    const GpuMemory&             gpuMemory,
    const MemoryImageCopyRegion& rgn,
    bool                         deTile,
    uint32*                      pCmdSpace)
{
    int32 totalWidthCopiedSoFar = 0;
    while (totalWidthCopiedSoFar < static_cast<int32>(rgn.imageExtent.width))
    {
        Extent3d     nextExtent;
        DmaImageInfo tiledImage = image;

        GetNextExtentAndOffset(rgn.imageExtent,
                               rgn.imageOffset,
                               tiledImage.bytesPerPixel,
                               totalWidthCopiedSoFar,
                               &nextExtent,
                               &tiledImage.offset);

        PAL_ASSERT(IsPow2Aligned(tiledImage.offset.x * tiledImage.bytesPerPixel, sizeof(uint32)) &&
                   IsPow2Aligned(nextExtent.width    * tiledImage.bytesPerPixel, sizeof(uint32)));

        constexpr size_t PacketDwords = sizeof(DMA_CMD_PACKET_L2TT2L_PARTIAL_COPY) / sizeof(uint32);
        auto*const       pPacket      = reinterpret_cast<DMA_CMD_PACKET_L2TT2L_PARTIAL_COPY*>(pCmdSpace);

        DMA_CMD_PACKET_L2TT2L_PARTIAL_COPY packet;

        packet.header.headerCayman.u32All       = 0;
        packet.header.headerCayman.bits.type    = DMA_COMMAND_COPY;
        packet.header.headerCayman.bits.r8xxcmd = 1;
        packet.header.headerCayman.bits.tiling  = 1;
        packet.header.headerCayman.bits.idcmd   = 1;

        SetupL2tT2lAddrAndTileInfo(tiledImage, deTile, &packet);

        const gpusize linearAddr = gpuMemory.Desc().gpuVirtAddr +
                                   rgn.gpuMemoryOffset          +
                                   totalWidthCopiedSoFar * tiledImage.bytesPerPixel;

        packet.linearAddrLo.u32All      = LowPart(linearAddr);
        packet.linearAddrHi.u32All      = 0;
        packet.linearAddrHi.bits.addrHi = HighPart(linearAddr);
        packet.linearAddrHi.bits.pitch  = static_cast<uint32>(rgn.gpuMemoryRowPitch);
        packet.linearPitch              = static_cast<uint32>(rgn.gpuMemoryDepthPitch);

        packet.sizeXY.u32All  = 0;
        packet.sizeXY.bits.dX = nextExtent.width;
        packet.sizeXY.bits.dY = nextExtent.height;
        packet.sizeZ.u32All   = 0;
        packet.sizeZ.bits.dZ  = nextExtent.depth;

        *pPacket = packet;

        totalWidthCopiedSoFar += nextExtent.width;
        pCmdSpace             += PacketDwords;
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Computes the next extent and offset that a sectioned-blt should do.  If the original extent is valid, then the "next"
// extent is equivalent to the original extent.
void DmaCmdBuffer::GetNextExtentAndOffset(
    const Extent3d& origExtent,
    const Offset3d& origOffset,
    uint32          bytesPerPixel,
    int32           totalWidthCopiedSoFar,
    Extent3d*       pNextExtent,
    Offset3d*       pNextOffset)
{
    // The "height" and "depth" of the extent aren't affected by the HW bug, so we can always keep those values.  We
    // just need to adjust the width of the extent and the x offset components.
    *pNextExtent = origExtent;
    *pNextOffset = origOffset;

    const int32 remainingWidth = origExtent.width - totalWidthCopiedSoFar;
    if ((remainingWidth % CalcBadModValue(bytesPerPixel)) != 0)
    {
        // What's left with this copy can work, so this is easy -- jus do everything.
        pNextExtent->width = remainingWidth;
    }
    else
    {
        // Ok, what's left still isn't going to work :-(, so we have to sub-divide.  Do everything
        // except the last eight pixels worth.  Eight is a somewhat arbitrary number, but it keeps
        // all the alignments working for the next stage of the copy.
        pNextExtent->width = remainingWidth - 8;
    }

    pNextOffset->x = origOffset.x + totalWidthCopiedSoFar;
}

// =====================================================================================================================
// Calculates the combination of the baseAddr and the offset fields from the supplied imageInfo structure.
gpusize DmaCmdBuffer::CalcLinearBaseAddr(
    const DmaImageInfo& imageInfo)
{
    return imageInfo.baseAddr + (imageInfo.offset.z * imageInfo.pSubresInfo->depthPitch)
                              + (imageInfo.offset.y * imageInfo.pSubresInfo->rowPitch)
                              + (imageInfo.offset.x * imageInfo.bytesPerPixel);
}

// =====================================================================================================================
// Returns the base multiple, in terms of pixels, that doesn't work for the specified bytes-per-pixel value.  i.e., copy
// widths that are a multiple of the returned value need to be broken up into multiple copies.
//
// There is a HW bug related to a shift operation.  All the below cases are affected.
//           Psize=1: DX=0x2000
//           Psize=2: DX=any multiple of 0x1000
//           Psize=3: DX=any multiple of 0x800
//           Psize=4: DX=any multiple of 0x400
//
//       Where "psize" is equal to log2(bytes-per-pixel)
int32 DmaCmdBuffer::CalcBadModValue(
    uint32 bytesPerPixel)
{
    return (0x4000 >> Log2(bytesPerPixel));
}

// =====================================================================================================================
// Sets the following structures:
//      pPacket[0] : DMA_COPY_LINEAR_PARTIAL_ADDR_LO
//      pPacket[1] : DMA_COPY_LINEAR_PARTIAL_ADDR_HI
//      pPacket[2] : slice pitch
void DmaCmdBuffer::SetupLinearAddrAndSlicePitch(
    const DmaImageInfo& imageInfo,
    uint32*             pPacket)
{
    DMA_COPY_LINEAR_PARTIAL_ADDR_LO* pPktAddrLo = reinterpret_cast<DMA_COPY_LINEAR_PARTIAL_ADDR_LO*>(&pPacket[0]);
    DMA_COPY_LINEAR_PARTIAL_ADDR_HI* pPktAddrHi = reinterpret_cast<DMA_COPY_LINEAR_PARTIAL_ADDR_HI*>(&pPacket[1]);

    const gpusize addr      = CalcLinearBaseAddr(imageInfo);
    pPktAddrLo->u32All      = LowPart(addr);
    pPktAddrHi->u32All      = 0;
    pPktAddrHi->bits.addrHi = HighPart(addr);
    pPktAddrHi->bits.pitch  = static_cast<uint32>(imageInfo.pSubresInfo->rowPitch);
    pPacket[2]              = static_cast<uint32>(imageInfo.pSubresInfo->depthPitch);
}

// =====================================================================================================================
// Sets up the following fields of the supplied "pPacket" structure:
//      DMA_COPY_L2TT2L_ADDR
//      DMA_COPY_L2TT2L_INFO_1
//      DMA_COPY_L2TT2L_INFO_2
void DmaCmdBuffer::SetupL2tT2lAddrAndSize(
    const DmaImageInfo& imageInfo,
    uint32*             pPacket)
{
    DMA_COPY_L2TT2L_ADDR*   pPktAddr  = reinterpret_cast<DMA_COPY_L2TT2L_ADDR*>(&pPacket[0]);
    DMA_COPY_L2TT2L_INFO_1* pPktInfo1 = reinterpret_cast<DMA_COPY_L2TT2L_INFO_1*>(&pPacket[1]);
    DMA_COPY_L2TT2L_INFO_2* pPktInfo2 = reinterpret_cast<DMA_COPY_L2TT2L_INFO_2*>(&pPacket[2]);

    const AddrMgr1::TileInfo*const pTileInfo =
        AddrMgr1::GetTileInfo(static_cast<const Image*>(imageInfo.pImage), imageInfo.pSubresInfo->subresId);

    // Don't need to include the pOffset field in this calculation, since the packet in question has entries for both
    // source and destination offsets.
    pPktAddr->bits.baseaddr         = Get256BAddrLo(imageInfo.baseAddr);
    pPktInfo1->u32All               = 0;
    pPktInfo1->bits.pitchTileMax    = GetPitchTileMax(imageInfo);
    pPktInfo1->bits.heightMax       = imageInfo.actualExtent.height - 1;
    pPktInfo2->u32All               = 0;
    pPktInfo2->si_bits.sliceTileMax = GetSliceTileMax(imageInfo);
    pPktInfo2->si_bits.pipe_config  = pTileInfo->pipeConfig;
}

// =====================================================================================================================
// Sets up the following fields of the supplied "pPacket" structure:
//      DMA_COPY_L2TT2L_ADDR                 tiledAddr;
//      DMA_COPY_L2TT2L_INFO_0               tiledInfo0;
//      DMA_COPY_L2TT2L_INFO_1               tiledInfo1;
//      DMA_COPY_L2TT2L_INFO_2               tiledInfo2;
//      DMA_COPY_L2TT2L_INFO_3               tiledInfo3;
//      DMA_COPY_L2TT2L_PARTIAL_INFO_4       tiledInfo4;
void DmaCmdBuffer::SetupL2tT2lAddrAndTileInfo(
    const DmaImageInfo&                     dmaImgInfo, // input image
    bool                                    deTile,      // direction of the copy
    _DMA_CMD_PACKET_L2TT2L_PARTIAL_COPY* pPacket)     // populated dma packet
{
    const AddrMgr1::TileInfo*const pTileInfo =
        AddrMgr1::GetTileInfo(static_cast<const Image*>(dmaImgInfo.pImage), dmaImgInfo.pSubresInfo->subresId);

    pPacket->tiledAddr.bits.baseaddr    = Get256BAddrLo(dmaImgInfo.baseAddr);
    pPacket->tiledInfo0.u32All          = 0;
    pPacket->tiledInfo0.bits.array_mode = pTileInfo->tileMode;
    pPacket->tiledInfo0.bits.bankheight = pTileInfo->bankHeight;
    pPacket->tiledInfo0.bits.bankwidth  = pTileInfo->bankWidth;
    pPacket->tiledInfo0.bits.direction  = deTile;
    pPacket->tiledInfo0.bits.mtaspect   = pTileInfo->macroAspectRatio;
    pPacket->tiledInfo0.bits.pixel_size = Log2(dmaImgInfo.bytesPerPixel);

    pPacket->tiledInfo1.u32All            = 0;
    pPacket->tiledInfo1.bits.pitchTileMax = GetPitchTileMax(dmaImgInfo);
    pPacket->tiledInfo1.bits.heightMax    = dmaImgInfo.actualExtent.height - 1;

    pPacket->tiledInfo2.u32All               = 0;
    pPacket->tiledInfo2.si_bits.sliceTileMax = GetSliceTileMax(dmaImgInfo);
    pPacket->tiledInfo2.si_bits.pipe_config  = pTileInfo->pipeConfig;

    pPacket->tiledInfo3.u32All = 0;
    pPacket->tiledInfo3.bits.x = dmaImgInfo.offset.x;
    pPacket->tiledInfo3.bits.z = dmaImgInfo.offset.z;

    pPacket->tiledInfo4.u32All            = 0;
    pPacket->tiledInfo4.si_bits.y         = dmaImgInfo.offset.y;
    pPacket->tiledInfo4.si_bits.mtmode    = pTileInfo->tileType;
    pPacket->tiledInfo4.si_bits.numbank   = pTileInfo->banks;
    pPacket->tiledInfo4.si_bits.tilesplit = pTileInfo->tileSplitBytes;
}

// =====================================================================================================================
// Shift tile swizzle to start at bit 8 here. OSS1 shifts 8 bits to the right, and it ends up in the normal spot for a
// 256B address.
gpusize DmaCmdBuffer::GetSubresourceBaseAddr(
    const Image&    image,
    const SubresId& subresource
    ) const
{
    const AddrMgr1::TileInfo*const pTileInfo = AddrMgr1::GetTileInfo(&image, subresource);

    return image.GetSubresourceBaseAddr(subresource) | (static_cast<gpusize>(pTileInfo->tileSwizzle) << 8);
}

} // Oss1
} // Pal
