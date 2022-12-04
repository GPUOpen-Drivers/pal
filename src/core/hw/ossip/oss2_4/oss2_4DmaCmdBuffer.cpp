/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/gfx6/gfx6Chip.h"
#include "core/hw/ossip/oss2_4/oss2_4DmaCmdBuffer.h"
#include "palFormatInfo.h"

using namespace Util;

namespace Pal
{
namespace Oss2_4
{
using namespace Pal::Gfx6::Chip;

#include "core/hw/ossip/oss2_4/sdma24_pkt_struct.h"

// =====================================================================================================================
DmaCmdBuffer::DmaCmdBuffer(
    Device*                    pDevice,
    const CmdBufferCreateInfo& createInfo)
    :
    Pal::DmaCmdBuffer(pDevice->Parent(),
                      createInfo,
                      ((1 << static_cast<uint32>(ImageType::Count)) - 1))
{
    // Regarding copyOverlapHazardSyncs value in the constructor above:
    //   SDMA may execute sequences of small copies/writes asynchronously (It is controlled by
    //   SDMA0/1_CHICKEN_BITS.COPY_OVERLAP_ENABLE which is on by default).  Driver needs to manually insert
    //   a NOP packet as a fence between copies that may have a hazard.  This is done within CmdBarrier().
}

// =====================================================================================================================
// Writes a packet that waits for the given GPU event to be set. Returns the next unused DWORD in pCmdSpace.
uint32* DmaCmdBuffer::WriteWaitEventSet(
    const GpuEvent& gpuEvent,
    uint32*         pCmdSpace
    ) const
{
    constexpr size_t PacketDwords = sizeof(SDMA_PKT_POLL_REGMEM) / sizeof(uint32);
    auto*const       pPacket      = reinterpret_cast<SDMA_PKT_POLL_REGMEM*>(pCmdSpace);
    const gpusize    gpuVirtAddr  = gpuEvent.GetBoundGpuMemory().GpuVirtAddr();

    SDMA_PKT_POLL_REGMEM packet;

    packet.HEADER_UNION.DW_0_DATA   = 0;
    packet.HEADER_UNION.op          = SDMA_OP_POLL_REGMEM;
    packet.HEADER_UNION.func        = 0x3;                    // Equal
    packet.HEADER_UNION.mem_poll    = 1;                      // Memory space poll.

    packet.ADDR_LO_UNION.addr_31_0  = LowPart(gpuVirtAddr);
    packet.ADDR_HI_UNION.addr_63_32 = HighPart(gpuVirtAddr);

    packet.VALUE_UNION.value        = GpuEvent::SetValue;
    packet.MASK_UNION.mask          = 0xFFFFFFFF;

    packet.DW5_UNION.DW_5_DATA      = 0;
    packet.DW5_UNION.interval       = 0xA;                    // Wait 160 clocks before each retry.
    packet.DW5_UNION.retry_count    = 0xFFF;                  // Retry infinitely.

    *pPacket = packet;

    return pCmdSpace + PacketDwords;
}

// =====================================================================================================================
// Helper function for writing the current GPU timestamp value into the specified memory.
void DmaCmdBuffer::WriteTimestampCmd(
    gpusize dstAddr)
{
    //     No need to issue a Fence prior to the timestamp command. The Timestamp itself can ensure previous commands
    //     all completed.

    // If the address isn't 32-byte aligned, this packet will just write a zero into the dest.
    PAL_ASSERT(IsPow2Aligned(dstAddr, 32));

    uint32* pCmdSpace = m_cmdStream.ReserveCommands();

    constexpr size_t PacketDwords = sizeof(SDMA_PKT_TIMESTAMP_SET) / sizeof(uint32);
    auto*const       pPacket      = reinterpret_cast<SDMA_PKT_TIMESTAMP_SET*>(pCmdSpace);

    SDMA_PKT_TIMESTAMP_SET packet;

    packet.HEADER_UNION.DW_0_DATA             = 0;
    packet.HEADER_UNION.op                    = SDMA_OP_TIMESTAMP;
    packet.HEADER_UNION.sub_op                = SDMA_SUBOP_TIMESTAMP_GET_GLOBAL;
    packet.INIT_DATA_LO_UNION.init_data_31_0  = LowPart(dstAddr);
    packet.INIT_DATA_HI_UNION.init_data_63_32 = HighPart(dstAddr);

    *pPacket = packet;

    m_cmdStream.CommitCommands(pCmdSpace + PacketDwords);
}

// =====================================================================================================================
// Writes the current GPU timestamp value into the specified memory.
void DmaCmdBuffer::CmdWriteTimestamp(
    HwPipePoint       pipePoint,
    const IGpuMemory& dstGpuMemory,
    gpusize           dstOffset)
{
    const GpuMemory& gpuMemory = static_cast<const GpuMemory&>(dstGpuMemory);
    const gpusize    dstAddr   = gpuMemory.Desc().gpuVirtAddr + dstOffset;

    WriteTimestampCmd(dstAddr);
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

    uint32* pCmdSpace = m_cmdStream.ReserveCommands();

    constexpr size_t PacketDwords = sizeof(SDMA_PKT_FENCE) / sizeof(uint32);
    auto*const       pPacket      = reinterpret_cast<SDMA_PKT_FENCE*>(pCmdSpace);

    SDMA_PKT_FENCE packet;

    packet.HEADER_UNION.DW_0_DATA   = 0;
    packet.HEADER_UNION.op          = SDMA_OP_FENCE;
    packet.ADDR_LO_UNION.addr_31_0  = LowPart(address);
    packet.ADDR_HI_UNION.addr_63_32 = HighPart(address);
    packet.DATA_UNION.DW_3_DATA     = LowPart(data);

    *pPacket = packet;
    size_t dwordsWritten = PacketDwords;

    if (dataSize == ImmediateDataWidth::ImmediateData64Bit)
    {
        address += sizeof(uint32);
        packet.ADDR_LO_UNION.addr_31_0  = LowPart(address);
        packet.ADDR_HI_UNION.addr_63_32 = HighPart(address);
        packet.DATA_UNION.DW_3_DATA     = HighPart(data);

        pPacket[1] = packet;
        dwordsWritten += PacketDwords;
    }

    m_cmdStream.CommitCommands(pCmdSpace + dwordsWritten);
}

// =====================================================================================================================
// Adds a preamble to the start of a new command buffer.
Result DmaCmdBuffer::AddPreamble()
{
    // If this trips, it means that this isn't really the preamble -- i.e., somebody has inserted something into the
    // command stream before the preamble.  :-(
    PAL_ASSERT(m_cmdStream.IsEmpty());

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
        auto*const    pPacket = reinterpret_cast<SDMA_PKT_SEMAPHORE*>(pCmdSpace);

        SDMA_PKT_SEMAPHORE packet;

        packet.HEADER_UNION.DW_0_DATA   = 0;
        packet.HEADER_UNION.op          = SDMA_OP_SEM;
        packet.HEADER_UNION.signal      = 1;
        packet.ADDR_LO_UNION.addr_31_0  = LowPart(gpuAddr);
        // Only 40 bit addresses are supported for the semaphore address.
        packet.ADDR_HI_UNION.addr_63_32 = HighPart(gpuAddr) & 0xFF;

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
    constexpr size_t PacketDwords = sizeof(SDMA_PKT_COND_EXE) / sizeof(uint32);
    auto*const       pPacket      = reinterpret_cast<SDMA_PKT_COND_EXE*>(pCmdSpace);

    SDMA_PKT_COND_EXE packet;

    PAL_NOT_TESTED();

    packet.HEADER_UNION.DW_0_DATA      = 0;
    packet.HEADER_UNION.op             = SDMA_OP_COND_EXE;
    packet.ADDR_LO_UNION.addr_31_0     = LowPart(m_predMemAddress);
    packet.ADDR_HI_UNION.addr_63_32    = HighPart(m_predMemAddress);
    packet.REFERENCE_UNION.reference   = 1;
    packet.EXEC_COUNT_UNION.DW_4_DATA  = 0;
    packet.EXEC_COUNT_UNION.exec_count = predicateDwords;

    *pPacket = packet;

    return pCmdSpace + PacketDwords;
}

// =====================================================================================================================
// Patches a COND_EXE packet with the given predication size.
void DmaCmdBuffer::PatchPredicateCmd(size_t predicateDwords, void* pPredicateCmd) const
{
    auto*const pPacket = reinterpret_cast<SDMA_PKT_COND_EXE*>(pPredicateCmd);

    pPacket->EXEC_COUNT_UNION.exec_count = predicateDwords;
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
    // The count field of the copy packet is 22 bits wide.  There is apparently an undocumented HW "feature" that
    // prevents the HW from copying past 256 bytes of (1 << 22) though.
    //
    //     "Due to HW limitation, the maximum count may not be 2^n-1, can only be 2^n - 1 - start_addr[4:2]".
    constexpr gpusize MaxCopySize = ((1ull << 22ull) - 256ull);

    *pBytesCopied = Min(copySize, MaxCopySize);

    if (IsPow2Aligned(srcGpuAddr, sizeof(uint32)) &&
        IsPow2Aligned(dstGpuAddr, sizeof(uint32)) &&
        (*pBytesCopied >= sizeof(uint32)))
    {
        // If the source and destination are dword aligned and the size is at least one DWORD, then go ahead and do
        // DWORD copies.  Note that the SDMA microcode makes the switch between byte and DWORD copies automagically,
        // depending on the addresses being dword aligned and the size being a dword multiple.
        *pBytesCopied = Pow2AlignDown(*pBytesCopied, sizeof(uint32));
    }

    constexpr size_t PacketDwords = sizeof(SDMA_PKT_COPY_LINEAR) / sizeof(uint32);
    auto*const       pPacket      = reinterpret_cast<SDMA_PKT_COPY_LINEAR*>(pCmdSpace);

    SDMA_PKT_COPY_LINEAR packet;

    packet.HEADER_UNION.DW_0_DATA           = 0;
    packet.HEADER_UNION.op                  = SDMA_OP_COPY;
    packet.HEADER_UNION.sub_op              = SDMA_SUBOP_COPY_LINEAR;
    if (copyFlags & DmaCopyFlags::TmzCopy)
    {
        packet.HEADER_UNION.tmz = 1;
    }
    packet.COUNT_UNION.DW_1_DATA            = 0;
    packet.COUNT_UNION.count                = *pBytesCopied;
    packet.PARAMETER_UNION.DW_2_DATA        = 0;
    packet.SRC_ADDR_LO_UNION.src_addr_31_0  = LowPart(srcGpuAddr);
    packet.SRC_ADDR_HI_UNION.src_addr_63_32 = HighPart(srcGpuAddr);
    packet.DST_ADDR_LO_UNION.dst_addr_31_0  = LowPart(dstGpuAddr);
    packet.DST_ADDR_HI_UNION.dst_addr_63_32 = HighPart(dstGpuAddr);

    *pPacket = packet;

    return pCmdSpace + PacketDwords;
}

// =====================================================================================================================
// Copies memory into the specified region of a typed buffer (linear image). Returns the next unused DWORD in pCmdSpace.
//
uint32* DmaCmdBuffer::WriteCopyTypedBuffer(
    const DmaTypedBufferCopyInfo& typedBufferInfo,
    uint32* pCmdSpace
    ) const
{
    constexpr size_t PacketDwords   = sizeof(SDMA_PKT_COPY_LINEAR_SUBWIN) / sizeof(uint32);
    auto*const       pPacket        = reinterpret_cast<SDMA_PKT_COPY_LINEAR_SUBWIN*>(pCmdSpace);

    SDMA_PKT_COPY_LINEAR_SUBWIN packet;

    packet.HEADER_UNION.DW_0_DATA     = 0;
    packet.HEADER_UNION.op            = SDMA_OP_COPY;
    packet.HEADER_UNION.sub_op        = SDMA_SUBOP_COPY_LINEAR_SUB_WIND;
    packet.HEADER_UNION.elementsize   = Log2(typedBufferInfo.dst.bytesPerElement);
    packet.HEADER_UNION.tmz           = (typedBufferInfo.flags & DmaCopyFlags::TmzCopy) ? 1 : 0;

    // Setup the source base address.
    packet.SRC_ADDR_LO_UNION.src_addr_31_0    = LowPart(typedBufferInfo.src.baseAddr);
    packet.SRC_ADDR_HI_UNION.src_addr_63_32   = HighPart(typedBufferInfo.src.baseAddr);

    // Setup the start of the source rect.
    // Offset is 0 since the base address is the actual address of the sub-region
    packet.DW_3_UNION.DW_3_DATA   = 0;
    packet.DW_4_UNION.DW_4_DATA   = 0;

    // Setup the source surface dimensions.
    packet.DW_4_UNION.src_pitch           = typedBufferInfo.src.linearRowPitch - 1;
    packet.DW_5_UNION.DW_5_DATA           = 0;
    packet.DW_5_UNION.src_slice_pitch     = typedBufferInfo.src.linearDepthPitch - 1;

    // Setup the destination base address.
    packet.DST_ADDR_LO_UNION.dst_addr_31_0    = LowPart(typedBufferInfo.dst.baseAddr);
    packet.DST_ADDR_HI_UNION.dst_addr_63_32   = HighPart(typedBufferInfo.dst.baseAddr);

    // Setup the start of the destination rectangle.
    // Offset is 0 since the base address is the actual address of the sub-region
    packet.DW_8_UNION.DW_8_DATA   = 0;
    packet.DW_9_UNION.DW_9_DATA   = 0;

    // Setup the destination surface dimensions.
    packet.DW_9_UNION.dst_pitch           = typedBufferInfo.dst.linearRowPitch - 1;
    packet.DW_10_UNION.DW_10_DATA         = 0;
    packet.DW_10_UNION.dst_slice_pitch    = typedBufferInfo.dst.linearDepthPitch - 1;

    // Setup the rectangle dimensions.
    packet.DW_11_UNION.DW_11_DATA     = 0;
    packet.DW_11_UNION.rect_x         = typedBufferInfo.copyExtent.width - 1;
    packet.DW_11_UNION.rect_y         = typedBufferInfo.copyExtent.height - 1;
    packet.DW_12_UNION.DW_12_DATA     = 0;
    packet.DW_12_UNION.rect_z         = typedBufferInfo.copyExtent.depth - 1;

    *pPacket = packet;

    return pCmdSpace + PacketDwords;
}

// =====================================================================================================================
// Copies the specified region between two linear images.
//
void DmaCmdBuffer::WriteCopyImageLinearToLinearCmd(
    const DmaImageCopyInfo& imageCopyInfo)
{
    uint32*          pCmdSpace    = m_cmdStream.ReserveCommands();
    constexpr size_t PacketDwords = sizeof(SDMA_PKT_COPY_LINEAR_SUBWIN) / sizeof(uint32);
    auto*const       pPacket      = reinterpret_cast<SDMA_PKT_COPY_LINEAR_SUBWIN*>(pCmdSpace);

    SDMA_PKT_COPY_LINEAR_SUBWIN packet;

    packet.HEADER_UNION.DW_0_DATA   = 0;
    packet.HEADER_UNION.op          = SDMA_OP_COPY;
    packet.HEADER_UNION.sub_op      = SDMA_SUBOP_COPY_LINEAR_SUB_WIND;
    packet.HEADER_UNION.elementsize = Log2(imageCopyInfo.dst.bytesPerPixel);
    packet.HEADER_UNION.tmz         = IsImageTmzProtected(imageCopyInfo.src);

    // Setup the source base address.
    packet.SRC_ADDR_LO_UNION.src_addr_31_0  = LowPart(imageCopyInfo.src.baseAddr);
    packet.SRC_ADDR_HI_UNION.src_addr_63_32 = HighPart(imageCopyInfo.src.baseAddr);

    // Setup the start of the source rect.
    packet.DW_3_UNION.DW_3_DATA = 0;
    packet.DW_3_UNION.src_x     = imageCopyInfo.src.offset.x;
    packet.DW_3_UNION.src_y     = imageCopyInfo.src.offset.y;
    packet.DW_4_UNION.DW_4_DATA = 0;
    packet.DW_4_UNION.src_z     = imageCopyInfo.src.offset.z;

    // Setup the source surface dimensions.
    packet.DW_4_UNION.src_pitch       = GetLinearRowPitch(imageCopyInfo.src);
    packet.DW_5_UNION.DW_5_DATA       = 0;
    packet.DW_5_UNION.src_slice_pitch = GetLinearDepthPitch(imageCopyInfo.src);

    // Setup the destination base address.
    packet.DST_ADDR_LO_UNION.dst_addr_31_0  = LowPart(imageCopyInfo.dst.baseAddr);
    packet.DST_ADDR_HI_UNION.dst_addr_63_32 = HighPart(imageCopyInfo.dst.baseAddr);

    // Setup the start of the destination rectangle.
    packet.DW_8_UNION.DW_8_DATA = 0;
    packet.DW_8_UNION.dst_x     = imageCopyInfo.dst.offset.x;
    packet.DW_8_UNION.dst_y     = imageCopyInfo.dst.offset.y;
    packet.DW_9_UNION.DW_9_DATA = 0;
    packet.DW_9_UNION.dst_z     = imageCopyInfo.dst.offset.z;

    // Setup the destination surface dimensions.
    packet.DW_9_UNION.dst_pitch        = GetLinearRowPitch(imageCopyInfo.dst);
    packet.DW_10_UNION.DW_10_DATA      = 0;
    packet.DW_10_UNION.dst_slice_pitch = GetLinearDepthPitch(imageCopyInfo.dst);

    /// Setup the rectangle dimensions.
    packet.DW_11_UNION.DW_11_DATA = 0;
    packet.DW_11_UNION.rect_x     = imageCopyInfo.copyExtent.width  - 1;
    packet.DW_11_UNION.rect_y     = imageCopyInfo.copyExtent.height - 1;
    packet.DW_12_UNION.DW_12_DATA = 0;
    packet.DW_12_UNION.rect_z     = imageCopyInfo.copyExtent.depth  - 1;

    *pPacket = packet;

    m_cmdStream.CommitCommands(pCmdSpace + PacketDwords);
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
        const auto& srcTileInfo = *AddrMgr1::GetTileInfo(static_cast<const Image*>(src.pImage),
                                                         src.pSubresInfo->subresId);

        constexpr uint32 ZAlignmentForHwArrayMode[] =
        {
            1, // ARRAY_LINEAR_GENERAL
            1, // ARRAY_LINEAR_ALIGNED
            1, // ARRAY_1D_TILED_THIN1
            4, // ARRAY_1D_TILED_THICK
            1, // ARRAY_2D_TILED_THIN1
            1, // ARRAY_PRT_TILED_THIN1__CI__VI
            1, // ARRAY_PRT_2D_TILED_THIN1__CI__VI
            4, // ARRAY_2D_TILED_THICK
            8, // ARRAY_2D_TILED_XTHICK
            4, // ARRAY_PRT_TILED_THICK__CI__VI
            4, // ARRAY_PRT_2D_TILED_THICK__CI__VI
            1, // ARRAY_PRT_3D_TILED_THIN1__CI__VI
            1, // ARRAY_3D_TILED_THIN1
            4, // ARRAY_3D_TILED_THICK
            8, // ARRAY_3D_TILED_XTHICK
            4, // ARRAY_PRT_3D_TILED_THICK__CI__VI
        };

        const uint32 zAlignment = ZAlignmentForHwArrayMode[srcTileInfo.tileMode];

        if ((srcTileInfo.tileType != ADDR_SURF_THICK_MICRO_TILING__CI__VI) ||
            ((IsPow2Aligned(src.offset.z,                   zAlignment)) &&
             (IsPow2Aligned(dst.offset.z,                   zAlignment)) &&
             (IsPow2Aligned(imageCopyInfo.copyExtent.depth, zAlignment))))
        {
            // Wow!  We can use the built-in packet!
            useScanlineCopy = false;
        }
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

    // On OSS 2.0-2.4, the x, rect_x, src/dst_pitch and src/dst_slice_pitch must be dword-aligned when
    // expressed in units of bytes for both L2L and L2T/T2L copies
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
    const DmaImageInfo& src = imageCopyInfo.src;
    const DmaImageInfo& dst = imageCopyInfo.dst;

    const AddrMgr1::TileInfo& srcTileInfo  = *AddrMgr1::GetTileInfo(static_cast<const Image*>(src.pImage),
                                                                    src.pSubresInfo->subresId);
    const AddrMgr1::TileInfo& dstTileInfo  = *AddrMgr1::GetTileInfo(static_cast<const Image*>(dst.pImage),
                                                                    dst.pSubresInfo->subresId);

    uint32*          pCmdSpace    = m_cmdStream.ReserveCommands();
    constexpr size_t PacketDwords = sizeof(SDMA_PKT_COPY_T2T) / sizeof(uint32);
    auto*const       pPacket      = reinterpret_cast<SDMA_PKT_COPY_T2T*>(pCmdSpace);

    SDMA_PKT_COPY_T2T packet;

    // Packet header
    packet.HEADER_UNION.DW_0_DATA = 0;
    packet.HEADER_UNION.op        = SDMA_OP_COPY;
    packet.HEADER_UNION.sub_op    = SDMA_SUBOP_COPY_T2T_SUB_WIND;
    packet.HEADER_UNION.tmz       = IsImageTmzProtected(imageCopyInfo.src);

    // Setup the start, offset, and dimenions of the source surface.
    packet.SRC_ADDR_LO_UNION.src_addr_31_0  = LowPart(src.baseAddr);
    packet.SRC_ADDR_HI_UNION.src_addr_63_32 = HighPart(src.baseAddr);

    packet.DW_3_UNION.DW_3_DATA = 0;
    packet.DW_3_UNION.src_x     = src.offset.x;
    packet.DW_3_UNION.src_y     = src.offset.y;
    packet.DW_4_UNION.DW_4_DATA = 0;
    packet.DW_4_UNION.src_z     = src.offset.z;

    packet.DW_4_UNION.src_pitch_in_tile = GetPitchTileMax(src);
    packet.DW_5_UNION.DW_5_DATA         = 0;
    packet.DW_5_UNION.src_slice_pitch   = GetSliceTileMax(src);

    // Setup the tile mode of the destination surface.
    packet.DW_6_UNION.DW_6_DATA          = 0;
    packet.DW_6_UNION.src_element_size   = Log2(dst.bytesPerPixel);
    packet.DW_6_UNION.src_mit_mode       = srcTileInfo.tileType;
    packet.DW_6_UNION.src_array_mode     = srcTileInfo.tileMode;
    packet.DW_6_UNION.src_pipe_config    = srcTileInfo.pipeConfig;
    packet.DW_6_UNION.src_mat_aspt       = srcTileInfo.macroAspectRatio;
    packet.DW_6_UNION.src_num_bank       = srcTileInfo.banks;
    packet.DW_6_UNION.src_bank_h         = srcTileInfo.bankHeight;
    packet.DW_6_UNION.src_bank_w         = srcTileInfo.bankWidth;
    packet.DW_6_UNION.src_tilesplit_size = srcTileInfo.tileSplitBytes;

    // Setup the start, offset, and dimenions of the destination surface.
    packet.DST_ADDR_LO_UNION.dst_addr_31_0  = LowPart(dst.baseAddr);
    packet.DST_ADDR_HI_UNION.dst_addr_63_32 = HighPart(dst.baseAddr);

    packet.DW_9_UNION.DW_9_DATA   = 0;
    packet.DW_9_UNION.dst_x       = dst.offset.x;
    packet.DW_9_UNION.dst_y       = dst.offset.y;
    packet.DW_10_UNION.DW_10_DATA = 0;
    packet.DW_10_UNION.dst_z      = dst.offset.z;

    packet.DW_10_UNION.dst_pitch_in_tile = GetPitchTileMax(dst);
    packet.DW_11_UNION.DW_11_DATA        = 0;
    packet.DW_11_UNION.dst_slice_pitch   = GetSliceTileMax(dst);

    // Setup the tile mode of the destination surface.
    packet.DW_12_UNION.DW_12_DATA         = 0;
    packet.DW_12_UNION.dst_array_mode     = dstTileInfo.tileMode;
    packet.DW_12_UNION.dst_pipe_config    = dstTileInfo.pipeConfig;
    packet.DW_12_UNION.dst_mat_aspt       = dstTileInfo.macroAspectRatio;
    packet.DW_12_UNION.dst_num_bank       = dstTileInfo.banks;
    packet.DW_12_UNION.dst_bank_h         = dstTileInfo.bankHeight;
    packet.DW_12_UNION.dst_bank_w         = dstTileInfo.bankWidth;
    packet.DW_12_UNION.dst_tilesplit_size = dstTileInfo.tileSplitBytes;

    // Setup the size of the copy region.
    // OSS2_4 T2T transfers require that the RECT_X and RECY_Y fields are in tiles, not pixels.
    packet.DW_13_UNION.DW_13_DATA = 0;
    packet.DW_13_UNION.rect_x     = (((imageCopyInfo.copyExtent.width  >> 3) - 1) << 3);
    packet.DW_13_UNION.rect_y     = (((imageCopyInfo.copyExtent.height >> 3) - 1) << 3);
    packet.DW_14_UNION.DW_14_DATA = 0;
    packet.DW_14_UNION.rect_z     = imageCopyInfo.copyExtent.depth - 1;

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
    constexpr size_t PacketDwords = sizeof(SDMA_PKT_COPY_LINEAR_SUBWIN) / sizeof(uint32);
    auto*const       pPacket      = reinterpret_cast<SDMA_PKT_COPY_LINEAR_SUBWIN*>(pCmdSpace);

    SDMA_PKT_COPY_LINEAR_SUBWIN packet;

    packet.HEADER_UNION.DW_0_DATA   = 0;
    packet.HEADER_UNION.op          = SDMA_OP_COPY;
    packet.HEADER_UNION.sub_op      = SDMA_SUBOP_COPY_LINEAR_SUB_WIND;
    packet.HEADER_UNION.elementsize = Log2(dstImage.bytesPerPixel);
    packet.HEADER_UNION.tmz         = srcGpuMemory.IsTmzProtected();

    // Setup the source base address.
    const gpusize srcBaseAddr = srcGpuMemory.Desc().gpuVirtAddr + rgn.gpuMemoryOffset;
    packet.SRC_ADDR_LO_UNION.src_addr_31_0  = LowPart(srcBaseAddr);
    packet.SRC_ADDR_HI_UNION.src_addr_63_32 = HighPart(srcBaseAddr);

    // Setup the start of the source rect (all zeros).
    packet.DW_3_UNION.DW_3_DATA = 0;
    packet.DW_4_UNION.DW_4_DATA = 0;

    // Setup the source surface dimensions.
    packet.DW_4_UNION.src_pitch       = GetLinearRowPitch(rgn.gpuMemoryRowPitch, dstImage.bytesPerPixel);
    packet.DW_5_UNION.DW_5_DATA       = 0;
    packet.DW_5_UNION.src_slice_pitch = GetLinearDepthPitch(rgn.gpuMemoryDepthPitch, dstImage.bytesPerPixel);

    // Setup the destination base address.
    packet.DST_ADDR_LO_UNION.dst_addr_31_0  = LowPart(dstImage.baseAddr);
    packet.DST_ADDR_HI_UNION.dst_addr_63_32 = HighPart(dstImage.baseAddr);

    // Setup the start of the destination rectangle.
    packet.DW_8_UNION.DW_8_DATA = 0;
    packet.DW_8_UNION.dst_x     = rgn.imageOffset.x;
    packet.DW_8_UNION.dst_y     = rgn.imageOffset.y;
    packet.DW_9_UNION.DW_9_DATA = 0;
    packet.DW_9_UNION.dst_z     = rgn.imageOffset.z;

    // Setup the destination surface dimensions.
    packet.DW_9_UNION.dst_pitch        = GetLinearRowPitch(dstImage);
    packet.DW_10_UNION.DW_10_DATA      = 0;
    packet.DW_10_UNION.dst_slice_pitch = GetLinearDepthPitch(dstImage);

    // Setup the rectangle dimensions.
    packet.DW_11_UNION.DW_11_DATA = 0;
    packet.DW_11_UNION.rect_x     = rgn.imageExtent.width  - 1;
    packet.DW_11_UNION.rect_y     = rgn.imageExtent.height - 1;
    packet.DW_12_UNION.DW_12_DATA = 0;
    packet.DW_12_UNION.rect_z     = rgn.imageExtent.depth  - 1;

    *pPacket = packet;

    return pCmdSpace + PacketDwords;
}

// =====================================================================================================================
// Copies the specified region of a linear image into memory. Returns the next unused DWORD in pCmdSpace.
//
uint32* DmaCmdBuffer::WriteCopyLinearImageToMemCmd(
    const DmaImageInfo&          srcImage,
    const GpuMemory&             dstGpuMemory,
    const MemoryImageCopyRegion& rgn,
    uint32*                      pCmdSpace
    ) const
{
    constexpr size_t PacketDwords = sizeof(SDMA_PKT_COPY_LINEAR_SUBWIN) / sizeof(uint32);
    auto*const       pPacket      = reinterpret_cast<SDMA_PKT_COPY_LINEAR_SUBWIN*>(pCmdSpace);

    SDMA_PKT_COPY_LINEAR_SUBWIN packet;

    packet.HEADER_UNION.DW_0_DATA   = 0;
    packet.HEADER_UNION.op          = SDMA_OP_COPY;
    packet.HEADER_UNION.sub_op      = SDMA_SUBOP_COPY_LINEAR_SUB_WIND;
    packet.HEADER_UNION.elementsize = Log2(srcImage.bytesPerPixel);
    packet.HEADER_UNION.tmz         = IsImageTmzProtected(srcImage);

    // Setup the source base address.
    packet.SRC_ADDR_LO_UNION.src_addr_31_0  = LowPart(srcImage.baseAddr);
    packet.SRC_ADDR_HI_UNION.src_addr_63_32 = HighPart(srcImage.baseAddr);

    // Setup the start of the source rect.
    packet.DW_3_UNION.DW_3_DATA = 0;
    packet.DW_3_UNION.src_x     = rgn.imageOffset.x;
    packet.DW_3_UNION.src_y     = rgn.imageOffset.y;
    packet.DW_4_UNION.DW_4_DATA = 0;
    packet.DW_4_UNION.src_z     = rgn.imageOffset.z;

    // Setup the source surface dimensions.
    packet.DW_4_UNION.src_pitch       = GetLinearRowPitch(srcImage);
    packet.DW_5_UNION.DW_5_DATA       = 0;
    packet.DW_5_UNION.src_slice_pitch = GetLinearDepthPitch(srcImage);

    // Setup the destination base address.
    const gpusize dstBaseAddr = dstGpuMemory.Desc().gpuVirtAddr + rgn.gpuMemoryOffset;
    packet.DST_ADDR_LO_UNION.dst_addr_31_0  = LowPart(dstBaseAddr);
    packet.DST_ADDR_HI_UNION.dst_addr_63_32 = HighPart(dstBaseAddr);

    // Setup the start of the destination rectangle (all zeros).
    packet.DW_8_UNION.DW_8_DATA = 0;
    packet.DW_9_UNION.DW_9_DATA = 0;

    // Setup the destination surface dimensions.
    packet.DW_9_UNION.dst_pitch        = GetLinearRowPitch(rgn.gpuMemoryRowPitch, srcImage.bytesPerPixel);
    packet.DW_10_UNION.DW_10_DATA      = 0;
    packet.DW_10_UNION.dst_slice_pitch = GetLinearDepthPitch(rgn.gpuMemoryDepthPitch, srcImage.bytesPerPixel);

    /// Setup the rectangle dimensions.
    packet.DW_11_UNION.DW_11_DATA = 0;
    packet.DW_11_UNION.rect_x     = rgn.imageExtent.width  - 1;
    packet.DW_11_UNION.rect_y     = rgn.imageExtent.height - 1;
    packet.DW_12_UNION.DW_12_DATA = 0;
    packet.DW_12_UNION.rect_z     = rgn.imageExtent.depth  - 1;

    *pPacket = packet;

    return pCmdSpace + PacketDwords;
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

    // The SDMA_PKT_WRITE_UNTILED definition contains space for one dword of data.  To make the math a little simpler
    // below, we consider the packetHeader size to be the packet size without any associated data. The SDMA packet spec
    // says the max-size field is 20 bits, the sdma packet header claims the max size is 22 bits. Lovely.  :-(
    constexpr uint32 PacketHdrSizeInDwords = (sizeof(SDMA_PKT_WRITE_UNTILED) / sizeof(uint32)) - 1;
    constexpr uint32 PacketMaxDataInDwords = (1u << 22) - 1;

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
        auto*const pPacket   = reinterpret_cast<SDMA_PKT_WRITE_UNTILED*>(pCmdSpace);

        SDMA_PKT_WRITE_UNTILED packet;

        packet.HEADER_UNION.DW_0_DATA           = 0;
        packet.HEADER_UNION.op                  = SDMA_OP_WRITE;
        packet.HEADER_UNION.sub_op              = SDMA_SUBOP_WRITE_LINEAR;
        packet.DST_ADDR_LO_UNION.dst_addr_31_0  = LowPart(dstAddr);
        packet.DST_ADDR_HI_UNION.dst_addr_63_32 = HighPart(dstAddr);
        packet.DW_3_UNION.DW_3_DATA             = 0;
        packet.DW_3_UNION.count                 = packetDataDwords;

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
    constexpr size_t PacketDwords = sizeof(SDMA_PKT_CONSTANT_FILL) / sizeof(uint32);
    auto*const       pPacket      = reinterpret_cast<SDMA_PKT_CONSTANT_FILL*>(pCmdSpace);

    SDMA_PKT_CONSTANT_FILL packet;

    // Because we will set fillsize = 2, the low two bits of our "count" are ignored, but we still program this in
    // terms of bytes.
    constexpr gpusize MaxFillSize = ((1ul << 22ull) - 1ull) & (~0x3ull);
    *pBytesCopied = Min(byteSize, MaxFillSize);

    packet.HEADER_UNION.DW_0_DATA           = 0;
    packet.HEADER_UNION.op                  = SDMA_OP_CONST_FILL;
    packet.HEADER_UNION.fillsize            = 2;  // 2 size means that "count" is in dwords
    packet.DST_ADDR_LO_UNION.dst_addr_31_0  = LowPart(dstAddr);
    packet.DST_ADDR_HI_UNION.dst_addr_63_32 = HighPart(dstAddr);
    packet.DATA_UNION.src_data_31_0         = data;
    packet.COUNT_UNION.DW_4_DATA            = 0;
    packet.COUNT_UNION.count                = *pBytesCopied;

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

    constexpr size_t PacketDwords = sizeof(SDMA_PKT_FENCE) / sizeof(uint32);
    auto*const       pPacket      = reinterpret_cast<SDMA_PKT_FENCE*>(pCmdSpace);

    SDMA_PKT_FENCE packet;

    packet.HEADER_UNION.DW_0_DATA   = 0;
    packet.HEADER_UNION.op          = SDMA_OP_FENCE;
    packet.ADDR_LO_UNION.addr_31_0  = LowPart(dstAddr);
    packet.ADDR_HI_UNION.addr_63_32 = HighPart(dstAddr);
    packet.DATA_UNION.DW_3_DATA     = data;

    *pPacket = packet;

    m_cmdStream.CommitCommands(pCmdSpace + PacketDwords);
}

// =====================================================================================================================
// Build a NOP packet.
uint32* DmaCmdBuffer::BuildNops(
    uint32* pCmdSpace,
    uint32  numDwords)
{
    constexpr size_t PacketDwords = sizeof(SDMA_PKT_NOP) / sizeof(uint32);
    static_assert(PacketDwords == 1, "WriteNops implementation assumes NOP packet is 1 dword.");

    SDMA_PKT_NOP packet    = { };
    packet.HEADER_UNION.op = SDMA_OP_NOP;

    auto* pPacket = reinterpret_cast<SDMA_PKT_NOP*>(pCmdSpace);

    for (uint32 i = 0; i < numDwords; i++)
    {
        *pPacket++ = packet;
    }

    return reinterpret_cast<uint32*>(pPacket);
}

// =====================================================================================================================
// Writes a NOP packet.
uint32* DmaCmdBuffer::WriteNops(
    uint32* pCmdSpace,
    uint32  numDwords
    ) const
{
    return BuildNops(pCmdSpace, numDwords);
}

// =====================================================================================================================
void DmaCmdBuffer::CmdNop(
    const void* pPayload,
    uint32      payloadSize)
{
    // See BuildNops() above, on OSS_2_4, NOP packet is fixed length with 1 DWORD and can't carry payload.
    PAL_ALERT_ALWAYS_MSG("Unsupported CmdNop with payload on OSS_2_4!");
}

// =====================================================================================================================
// Either copies a linear image into a tiled one (deTile == false) or vice versa. Returns the next unused DWORD in
// pCmdSpace.
//
uint32* DmaCmdBuffer::CopyImageLinearTiledTransform(
    const DmaImageCopyInfo& copyInfo,  // info on the images being copied
    const DmaImageInfo&     linearImg, // linear image, source if deTile==false
    const DmaImageInfo&     tiledImg,  // tiled image, source if deTile==true
    bool                    deTile,    // True for copying pTiledImg into pLinearImg
    uint32*                 pCmdSpace)
{
    // From the SDMA spec:
    //    For both linear and tiled surface, X and Rect X should be aligned to DW (multiple of four for 8bpp, multiple
    //    of 2 for 16bpp).
    // If this assert fails, how can we fix it? Looks like the SDMA_SUBOP_COPY_TILED might work with scanline copies?
    const uint32 multiple = Max(1u, (4 / linearImg.bytesPerPixel));

    PAL_ASSERT(((tiledImg.offset.x         % multiple) == 0) &&
               ((linearImg.offset.x        % multiple) == 0) &&
               ((copyInfo.copyExtent.width % multiple) == 0));

    constexpr size_t PacketDwords = sizeof(SDMA_PKT_COPY_TILED_SUBWIN) / sizeof(uint32);
    auto*const       pPacket      = reinterpret_cast<SDMA_PKT_COPY_TILED_SUBWIN*>(pCmdSpace);

    SDMA_PKT_COPY_TILED_SUBWIN packet;

    packet.HEADER_UNION.DW_0_DATA = 0;
    packet.HEADER_UNION.op        = SDMA_OP_COPY;
    packet.HEADER_UNION.sub_op    = SDMA_SUBOP_COPY_TILED_SUB_WIND;
    packet.HEADER_UNION.detile    = deTile;
    packet.HEADER_UNION.tmz       = IsImageTmzProtected(copyInfo.src);

    // Setup the tiled surface here.
    packet.TILED_ADDR_LO_UNION.tiled_addr_31_0  = LowPart(tiledImg.baseAddr);
    packet.TILED_ADDR_HI_UNION.tiled_addr_63_32 = HighPart(tiledImg.baseAddr);

    // Setup the tiled start location.
    packet.DW_3_UNION.DW_3_DATA = 0;
    packet.DW_3_UNION.tiled_x   = tiledImg.offset.x;
    packet.DW_3_UNION.tiled_y   = tiledImg.offset.y;
    packet.DW_4_UNION.DW_4_DATA = 0;
    packet.DW_4_UNION.tiled_z   = tiledImg.offset.z;

    // Setup the tiled surface dimensions.
    packet.DW_4_UNION.pitch_in_tile = GetPitchTileMax(tiledImg);
    packet.DW_5_UNION.DW_5_DATA     = 0;
    packet.DW_5_UNION.slice_pitch   = GetSliceTileMax(tiledImg);

    // Setup the tiled surface tiling info.
    SetDw6TilingInfo(tiledImg, &packet);

    // Setup the linear surface here.
    packet.LINEAR_ADDR_LO_UNION.linear_addr_31_0  = LowPart(linearImg.baseAddr);
    packet.LINEAR_ADDR_HI_UNION.linear_addr_63_32 = HighPart(linearImg.baseAddr);

    // Setup the linear start location.
    packet.DW_9_UNION.DW_9_DATA   = 0;
    packet.DW_9_UNION.linear_x    = linearImg.offset.x;
    packet.DW_9_UNION.linear_y    = linearImg.offset.y;
    packet.DW_10_UNION.DW_10_DATA = 0;
    packet.DW_10_UNION.linear_z   = linearImg.offset.z;

    // Linear is the source.
    packet.DW_10_UNION.linear_pitch       = GetLinearRowPitch(linearImg);
    packet.DW_11_UNION.DW_11_DATA         = 0;
    packet.DW_11_UNION.linear_slice_pitch = GetLinearDepthPitch(linearImg);

    // Setup the rectangle to copy.
    packet.DW_12_UNION.DW_12_DATA = 0;
    packet.DW_12_UNION.rect_x     = copyInfo.copyExtent.width  - 1;
    packet.DW_12_UNION.rect_y     = copyInfo.copyExtent.height - 1;
    packet.DW_13_UNION.DW_13_DATA = 0;
    packet.DW_13_UNION.rect_z     = copyInfo.copyExtent.depth  - 1;

    *pPacket = packet;

    return pCmdSpace + PacketDwords;
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
    // From the SDMA spec:
    //    For both linear and tiled surface, X and Rect X should be aligned to DW (multiple of four for 8bpp, multiple
    //    of 2 for 16bpp).
    // If this assert fails, how can we fix it? Looks like the SDMA_SUBOP_COPY_TILED might work with scanline copies?
    const uint32 multiple = Max(1u, (4 / image.bytesPerPixel));

    PAL_ASSERT(((rgn.imageOffset.x     % multiple) == 0) &&
               ((rgn.imageExtent.width % multiple) == 0));

    constexpr size_t PacketDwords = sizeof(SDMA_PKT_COPY_TILED_SUBWIN) / sizeof(uint32);
    auto*const       pPacket      = reinterpret_cast<SDMA_PKT_COPY_TILED_SUBWIN*>(pCmdSpace);

    SDMA_PKT_COPY_TILED_SUBWIN packet;

    packet.HEADER_UNION.DW_0_DATA = 0;
    packet.HEADER_UNION.op        = SDMA_OP_COPY;
    packet.HEADER_UNION.sub_op    = SDMA_SUBOP_COPY_TILED_SUB_WIND;
    packet.HEADER_UNION.detile    = deTile; // One packet handles both directions.
    packet.HEADER_UNION.tmz       = deTile ? IsImageTmzProtected(image) : gpuMemory.IsTmzProtected();

    // Setup the tiled surface here.
    packet.TILED_ADDR_LO_UNION.tiled_addr_31_0  = LowPart(image.baseAddr);
    packet.TILED_ADDR_HI_UNION.tiled_addr_63_32 = HighPart(image.baseAddr);

    // Setup the tiled start location.
    packet.DW_3_UNION.DW_3_DATA = 0;
    packet.DW_3_UNION.tiled_x   = rgn.imageOffset.x;
    packet.DW_3_UNION.tiled_y   = rgn.imageOffset.y;
    packet.DW_4_UNION.DW_4_DATA = 0;
    packet.DW_4_UNION.tiled_z   = rgn.imageOffset.z;

    // Setup the tiled surface dimensions.
    packet.DW_4_UNION.pitch_in_tile = GetPitchTileMax(image);
    packet.DW_5_UNION.DW_5_DATA     = 0;
    packet.DW_5_UNION.slice_pitch   = GetSliceTileMax(image);

    // Setup the tiled surface tiling info.
    SetDw6TilingInfo(image, &packet);

    // Setup the linear surface here.
    const gpusize linearBaseAddr = gpuMemory.Desc().gpuVirtAddr + rgn.gpuMemoryOffset;
    packet.LINEAR_ADDR_LO_UNION.linear_addr_31_0  = LowPart(linearBaseAddr);
    packet.LINEAR_ADDR_HI_UNION.linear_addr_63_32 = HighPart(linearBaseAddr);

    // Setup the linear start location (all zeros).
    packet.DW_9_UNION.DW_9_DATA   = 0;
    packet.DW_10_UNION.DW_10_DATA = 0;

    // Setup the linear surface dimensions.
    packet.DW_10_UNION.linear_pitch       = GetLinearRowPitch(rgn.gpuMemoryRowPitch, image.bytesPerPixel);
    packet.DW_11_UNION.DW_11_DATA         = 0;
    packet.DW_11_UNION.linear_slice_pitch = GetLinearDepthPitch(rgn.gpuMemoryDepthPitch, image.bytesPerPixel);

    // Setup the rectangle to copy.
    packet.DW_12_UNION.DW_12_DATA = 0;
    packet.DW_12_UNION.rect_x     = rgn.imageExtent.width  - 1;
    packet.DW_12_UNION.rect_y     = rgn.imageExtent.height - 1;
    packet.DW_13_UNION.DW_13_DATA = 0;
    packet.DW_13_UNION.rect_z     = rgn.imageExtent.depth  - 1;

    *pPacket = packet;

    return pCmdSpace + PacketDwords;
}

// =====================================================================================================================
// Sets up the DW_6_UNION field of the COPY_TILED_SUBWIN SDMA packet.
void DmaCmdBuffer::SetDw6TilingInfo(
    const DmaImageInfo&             imgInfo,
    SDMA_PKT_COPY_TILED_SUBWIN_TAG* pPacket) // [out] Populates the DW_6_UNION field
{
    const AddrMgr1::TileInfo*const pTileInfo = AddrMgr1::GetTileInfo(static_cast<const Image*>(imgInfo.pImage),
                                                                     imgInfo.pSubresInfo->subresId);

    pPacket->DW_6_UNION.DW_6_DATA      = 0;
    pPacket->DW_6_UNION.element_size   = Log2(imgInfo.bytesPerPixel);
    pPacket->DW_6_UNION.mit_mode       = pTileInfo->tileType;
    pPacket->DW_6_UNION.array_mode     = pTileInfo->tileMode;
    pPacket->DW_6_UNION.pipe_config    = pTileInfo->pipeConfig;
    pPacket->DW_6_UNION.mat_aspt       = pTileInfo->macroAspectRatio;
    pPacket->DW_6_UNION.num_bank       = pTileInfo->banks;
    pPacket->DW_6_UNION.bank_h         = pTileInfo->bankHeight;
    pPacket->DW_6_UNION.bank_w         = pTileInfo->bankWidth;
    pPacket->DW_6_UNION.tilesplit_size = pTileInfo->tileSplitBytes;
}

// =====================================================================================================================
// Shift tile swizzle to start at bit 8 here. OSS2_4 shifts 8 bits to the right, and it ends up in the normal spot for
// a 256B address.
gpusize DmaCmdBuffer::GetSubresourceBaseAddr(
    const Image&    image,
    const SubresId& subresource
    ) const
{
    const AddrMgr1::TileInfo*const pTileInfo = AddrMgr1::GetTileInfo(&image, subresource);

    return image.GetSubresourceBaseAddr(subresource) | (static_cast<gpusize>(pTileInfo->tileSwizzle) << 8);
}

// =====================================================================================================================
// Returns the multiplier required to align the linear row pitch with OSS2_4 HW requirements
uint32 DmaCmdBuffer::GetLinearRowPitchAlignment(
    uint32 bytesPerPixel
    ) const
{
    return bytesPerPixel;
}

// =====================================================================================================================
void DmaCmdBuffer::ResetState()
{
    CmdBuffer::ResetState();
}

} // Oss2_4
} // Pal
