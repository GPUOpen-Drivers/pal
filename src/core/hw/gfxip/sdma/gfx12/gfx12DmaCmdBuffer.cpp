/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2022-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/gpuMemory.h"
#include "core/settingsLoader.h"
#include "core/addrMgr/addrMgr3/addrMgr3.h"
#include "core/hw/gfxip/gfx12/gfx12Device.h"
#include "core/hw/gfxip/gfx12/gfx12Image.h"
#include "core/hw/gfxip/sdma/gfx12/gfx12DmaCmdBuffer.h"
#include "core/hw/gfxip/sdma/gfx12/gfx12_merged_sdma_packets.h"
#include "core/addrMgr/addrMgr2/addrMgr2.h"

#include "palFormatInfo.h"

using namespace Util;
using namespace Pal::Gfx12;

namespace Pal
{
namespace Gfx12
{

constexpr size_t NopSizeDwords = sizeof(SDMA_PKT_NOP) / sizeof(uint32);

// Read compression mode that Sdma packet COPY_LINEAR / COPY_TILED_SUBWIN / COPY_T2T could specify.
enum class SdmaReadCompressionMode : uint32
{
    BypassCompression = 0,
    Reserved1         = 1,
    ReadDecompressed  = 2,
    Reserved2         = 3,
};

// Write compression mode that Sdma packet COPY_LINEAR / COPY_TILED_SUBWIN / COPY_T2T could specify.
enum class SdmaWriteCompressionMode : uint32
{
    BypassCompression        = 0,
    EnableCompression        = 1,
    WriteCompressionDisabled = 2,
    Reserved                 = 3,
};

// =====================================================================================================================
DmaCmdBuffer::DmaCmdBuffer(
    Pal::Device&               device,
    const CmdBufferCreateInfo& createInfo)
    :
    Pal::DmaCmdBuffer(&device, createInfo, ((1 << static_cast<uint32>(ImageType::Count)) - 1))
{

    // Regarding copyOverlapHazardSyncs value in the constructor above:
    //   While GFX12 may execute sequences of small copies/writes asynchronously, the hardware should
    //   have automatic detection of hazards between these copies based on VA range comparison, so the
    //   driver does not itself need to do any manual synchronization.

    // Temporary note: The above description is not correct at the moment: there is a likely HW bug with the the copy
    // overlap feature and it is temporarily disabled. This could also be a PAL bug because sDMA is only meant to
    // detect some RAW hazards. Some copies (which?) do require manual SW barriers which we don't do currently.
}

// =====================================================================================================================
// Writes a packet that waits for the given GPU event to be set. Returns the next unused DWORD in pCmdSpace.
//
uint32* DmaCmdBuffer::WriteWaitEventSet(
    const GpuEvent& gpuEvent,
    uint32*         pCmdSpace
    ) const
{
    constexpr size_t  PacketDwords = NumBytesToNumDwords(sizeof(SDMA_PKT_POLL_REGMEM));
    auto*const        pPacket      = reinterpret_cast<SDMA_PKT_POLL_REGMEM*>(pCmdSpace);
    const gpusize     gpuVirtAddr  = gpuEvent.GetBoundGpuMemory().GpuVirtAddr();

    // The GPU address for poll_regmem must be 4 bytes aligned.
    PAL_ASSERT(IsPow2Aligned(gpuVirtAddr, sizeof(uint32)));

    SDMA_PKT_POLL_REGMEM packet = {};

    packet.HEADER_UNION.op          = SDMA_OP_POLL_REGMEM;
    packet.HEADER_UNION.mall_policy = GetMallPolicy(true);
    packet.HEADER_UNION.mode        = 0;
    packet.HEADER_UNION.func        = 0x3;                    // Equal
    packet.HEADER_UNION.mem_poll    = 1;                      // Memory space poll.

    packet.ADDR_LO_UNION.addr_31_2  = LowPart(gpuVirtAddr) >> 2; // Dword aligned.
    packet.ADDR_HI_UNION.addr_63_32 = HighPart(gpuVirtAddr);

    packet.VALUE_UNION.value        = GpuEvent::SetValue;
    packet.MASK_UNION.mask          = UINT32_MAX;

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

    uint32* pCmdSpace = m_cmdStream.ReserveCommands();

    constexpr size_t PacketDwords = NumBytesToNumDwords(sizeof(SDMA_PKT_TIMESTAMP_GET_GLOBAL));
    auto*const       pPacket      = reinterpret_cast<SDMA_PKT_TIMESTAMP_GET_GLOBAL*>(pCmdSpace);

    SDMA_PKT_TIMESTAMP_GET_GLOBAL packet = {};

    packet.HEADER_UNION.op                      = SDMA_OP_TIMESTAMP;
    packet.HEADER_UNION.sub_op                  = SDMA_SUBOP_TIMESTAMP_GET_GLOBAL;
    packet.HEADER_UNION.mall_policy             = GetMallPolicy(false);
    packet.WRITE_ADDR_LO_UNION.DW_1_DATA        = LowPart(dstAddr);
    packet.WRITE_ADDR_HI_UNION.write_addr_63_32 = HighPart(dstAddr);

    *pPacket = packet;

    m_cmdStream.CommitCommands(pCmdSpace + PacketDwords);
}

// =====================================================================================================================
// Adds a preamble to the start of a new command buffer.
void DmaCmdBuffer::AddPreamble()
{
    // If this trips, it means that this isn't really the preamble -- i.e., somebody has inserted something into the
    // command stream before the preamble.  :-(
    PAL_ASSERT(m_cmdStream.IsEmpty());

    // Adding a NOP preamble ensures that we always have something to submit (i.e,. the app can't submit an empty
    // command buffer which causes problems to the submit routine).
    uint32* pCmdSpace = m_cmdStream.ReserveCommands();

    pCmdSpace = WriteNops(pCmdSpace, 1);

    m_cmdStream.CommitCommands(pCmdSpace);

}

// =====================================================================================================================
// Build a NOP packet.
uint32* DmaCmdBuffer::BuildNops(
    uint32* pCmdSpace,
    uint32  numDwords)
{
    // Starting with OSS4, the NOP packet is variable length.  Note that the count field is the size of the body of the
    // NOP excluding the 1 dword packet header.
    SDMA_PKT_NOP packet       = { };
    packet.HEADER_UNION.op    = SDMA_OP_NOP;
    packet.HEADER_UNION.count = numDwords - 1;

    *reinterpret_cast<SDMA_PKT_NOP*>(pCmdSpace) = packet;

    return pCmdSpace + numDwords;
}

// =====================================================================================================================
// Adds a postamble to the end of a new command buffer. This will add a mem_incr packet to increment the completion
// count of the command buffer when the GPU has finished executing it.
void DmaCmdBuffer::AddPostamble()
{

    uint32* pCmdSpace = m_cmdStream.ReserveCommands();

    if (m_cmdStream.GetFirstChunk()->BusyTrackerGpuAddr() != 0)
    {
        const gpusize gpuAddr = m_cmdStream.GetFirstChunk()->BusyTrackerGpuAddr();
        auto*const    pPacket = reinterpret_cast<SDMA_PKT_MEM_INCR*>(pCmdSpace);

        SDMA_PKT_MEM_INCR packet = {};

        // The GPU address for mem_incr must be 8 byte aligned.
        constexpr uint32 SemaphoreAlign = 8;
        PAL_ASSERT(IsPow2Aligned(gpuAddr, SemaphoreAlign));

        packet.HEADER_UNION.op          = SDMA_OP_SEM;
        packet.HEADER_UNION.sub_op      = SDMA_SUBOP_MEM_INCR;
        packet.HEADER_UNION.mall_policy = GetMallPolicy(false);
        packet.ADDR_LO_UNION.addr_31_3  = LowPart(gpuAddr) >> 3; // 2 Dwords aligned
        packet.ADDR_HI_UNION.addr_63_32 = HighPart(gpuAddr);

        *pPacket  = packet;
        pCmdSpace = reinterpret_cast<uint32*>(pPacket + 1);
    }

    m_cmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
void DmaCmdBuffer::CmdNop(
    const void* pPayload,
    uint32      payloadSize)
{
    uint32*      pCmdSpace  = m_cmdStream.ReserveCommands();
    const size_t packetSize = NopSizeDwords + payloadSize;
    auto*const   pPacket    = reinterpret_cast<SDMA_PKT_NOP*>(pCmdSpace);
    uint32*      pData      = reinterpret_cast<uint32*>(pPacket + 1);

    BuildNops(pCmdSpace, uint32(packetSize));

    // Append data
    memcpy(pData, pPayload, payloadSize * sizeof(uint32));

    m_cmdStream.CommitCommands(pCmdSpace + packetSize);
}

// =====================================================================================================================
// Copy and convert predicate value from outer predication memory to internal predication memory
// Predication value will be converted to 0 or 1 based on value in outer predication memory and predication polarity.
uint32* DmaCmdBuffer::WriteSetupInternalPredicateMemoryCmd(
    gpusize predMemAddress,
    uint32  predCopyData,
    uint32* pCmdSpace
    ) const
{
    constexpr uint32 FencePktSizeInDwords = NumBytesToNumDwords(sizeof(SDMA_PKT_FENCE));

    // LSB 0-31 bit predication
    pCmdSpace = WriteCondExecCmd(pCmdSpace, predMemAddress, FencePktSizeInDwords);

    // "Write data"
    pCmdSpace = WriteFenceCmd(pCmdSpace, m_predInternalAddr, predCopyData);

    // MSB 32-63 bit predication
    pCmdSpace = WriteCondExecCmd(pCmdSpace, predMemAddress + 4, FencePktSizeInDwords);

    // "Write data"
    pCmdSpace = WriteFenceCmd(pCmdSpace, m_predInternalAddr, predCopyData);

    return pCmdSpace;
}

// =====================================================================================================================
// Writes a COND_EXE packet to predicate the next packets based on a memory value. Returns the next unused DWORD in
// pCmdSpace.
//
uint32* DmaCmdBuffer::WritePredicateCmd(
    uint32* pCmdSpace
    ) const
{
    if (m_predMemEnabled)
    {
        // Predication with Internal Memory
        pCmdSpace = WriteCondExecCmd(pCmdSpace, m_predInternalAddr, 0);
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Patches a COND_EXE packet with the given predication size.
//
void DmaCmdBuffer::PatchPredicateCmd(
    uint32* pPredicateCmd,
    uint32* pCurCmdSpace
    ) const
{
    if (m_predMemEnabled)
    {
        PAL_ASSERT(pCurCmdSpace > pPredicateCmd);

        auto* const  pPacket = reinterpret_cast<SDMA_PKT_COND_EXE*>(pPredicateCmd);
        const uint32 skipDws = (pCurCmdSpace - pPredicateCmd) - NumBytesToNumDwords(sizeof(SDMA_PKT_COND_EXE));

        decltype(pPacket->EXEC_COUNT_UNION) pktDw = {};
        pktDw.exec_count = skipDws;
        pPacket->EXEC_COUNT_UNION.DW_4_DATA = pktDw.DW_4_DATA;
    }
}

// =====================================================================================================================
uint32* DmaCmdBuffer::WriteCondExecCmd(
    uint32* pCmdSpace,
    gpusize predMemory,
    uint32  skipCountInDwords
    ) const
{
    auto*const pPacket = reinterpret_cast<SDMA_PKT_COND_EXE*>(pCmdSpace);

    // The GPU address for cond_exec memory must be 4 bytes aligned.
    PAL_ASSERT(IsPow2Aligned(predMemory, sizeof(uint32)));

    SDMA_PKT_COND_EXE packet = {};
    packet.HEADER_UNION.op             = SDMA_OP_COND_EXE;
    packet.HEADER_UNION.mall_policy    = GetMallPolicy(true);
    packet.ADDR_LO_UNION.addr_31_2     = LowPart(predMemory) >> 2; // Dword aligned
    packet.ADDR_HI_UNION.addr_63_32    = HighPart(predMemory);
    packet.REFERENCE_UNION.reference   = 1;
    packet.EXEC_COUNT_UNION.exec_count = skipCountInDwords;

    *pPacket = packet;

    return pCmdSpace + NumBytesToNumDwords(sizeof(SDMA_PKT_COND_EXE));
}

// =====================================================================================================================
uint32* DmaCmdBuffer::WriteFenceCmd(
    uint32* pCmdSpace,
    gpusize fenceMemory,
    uint32  predCopyData
    ) const
{
    PAL_ASSERT(IsPow2Aligned(fenceMemory, sizeof(uint32)));

    auto*const pFencePacket = reinterpret_cast<SDMA_PKT_FENCE*>(pCmdSpace);

    SDMA_PKT_FENCE fencePacket = {};
    fencePacket.HEADER_UNION.op          = SDMA_OP_FENCE;
    fencePacket.HEADER_UNION.mall_policy = GetMallPolicy(false);
    fencePacket.ADDR_LO_UNION.addr_31_2  = LowPart(fenceMemory) >> 2; // Dword aligned
    fencePacket.ADDR_HI_UNION.addr_63_32 = HighPart(fenceMemory);
    fencePacket.DATA_UNION.DW_3_DATA     = predCopyData;

    *pFencePacket = fencePacket;

    return pCmdSpace + NumBytesToNumDwords(sizeof(SDMA_PKT_FENCE));
}

// =====================================================================================================================
uint32 DmaCmdBuffer::GetMallPolicy(
    bool isCopySrc
    ) const
{
    //
    // Cache Policy is as known as Temporal Hint (TH). It is associated with all operations that read/write memory which
    // is an indicator to the hardware of expected reuse and is used for prioritization in retention of data in cache
    // hierarchy.
    //
    // TH[2:0] encoding allows for bifurcating a cache hierarchy into near caches (smaller, lower latency, higher
    // throughput) from far cache(s) (larger, higher latency, lower throughput) such that there is orthogonal
    // temporal hint control between the near caches and the far caches. The near cache refers to GL2 while
    // the far cache refers MALL.
    //
    //
    // 0-RT    - regular temporal (default) for both near and far caches
    // 1-NT    - non-temporal (re-use not expected) for both near and far caches
    // 2-HT    - High-priority temporal (precedence over RT) for both near and far caches
    // 3-LU    - Last-use (non-temporal AND discard dirty if it hits)
    // 4-NT_RT - non-temporal for near cache(s) and regular for far caches
    // 5-RT_NT - regular for near cache(s) and non-temporal for far caches
    // 6-NT_HT - non-temporal for near cache(s) and high-priority temporal for far caches

    static_assert(SdmaMallPolicyRt == 0, "SdmaMallPolicy mismatches HW definition values!");
    static_assert(SdmaMallPolicyNt == 1, "SdmaMallPolicy mismatches HW definition values!");
    static_assert(SdmaMallPolicyHt == 2, "SdmaMallPolicy mismatches HW definition values!");
    static_assert(SdmaMallPolicyLu == 3, "SdmaMallPolicy mismatches HW definition values!");

    uint32 mallPolicy = 0;

    if (m_pDevice->MemoryProperties().flags.supportsMall != 0)
    {
        const auto& gfx12Device = static_cast<const Gfx12::Device&>(*m_pDevice->GetGfxDevice());
        const Gfx12PalSettings& settings = gfx12Device.Settings();

        mallPolicy = isCopySrc ? settings.sdmaSrcMallPolicy : settings.sdmaDstMallPolicy;
    }

    return mallPolicy;
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
    // The count field of the copy packet is 30 bits wide for all products since GFX10.3+
    constexpr gpusize MaxCopySize = (1ull << 30);

    *pBytesCopied = Min(copySize, MaxCopySize);

    if (IsPow2Aligned(srcGpuAddr, sizeof(uint32)) &&
        IsPow2Aligned(dstGpuAddr, sizeof(uint32)) &&
        (*pBytesCopied >= sizeof(uint32)))
    {
        // If the source and destination are DWORD aligned and the size is at least one DWORD, then go ahead and do
        // DWORD copies.  Note that the SDMA microcode makes the switch between byte and DWORD copies automagically,
        // depending on the addresses being DWORD aligned and the size being a DWORD multiple.
        *pBytesCopied = Pow2AlignDown(*pBytesCopied, sizeof(uint32));
    }

    size_t     packetDwords = NumBytesToNumDwords(sizeof(SDMA_PKT_COPY_LINEAR));

    SDMA_PKT_COPY_LINEAR packet = {};

    packet.HEADER_UNION.op     = SDMA_OP_COPY;
    packet.HEADER_UNION.sub_op = SDMA_SUBOP_COPY_LINEAR;
    packet.HEADER_UNION.tmz    = TestAnyFlagSet(copyFlags, DmaCopyFlags::TmzCopy);
    packet.COUNT_UNION.count   = *pBytesCopied - 1;

    packet.PARAMETER_UNION.dst_mall_policy = GetMallPolicy(false);
    packet.PARAMETER_UNION.src_mall_policy = GetMallPolicy(true);

    packet.SRC_ADDR_LO_UNION.src_addr_31_0  = LowPart(srcGpuAddr);
    packet.SRC_ADDR_HI_UNION.src_addr_63_32 = HighPart(srcGpuAddr);
    packet.DST_ADDR_LO_UNION.dst_addr_31_0  = LowPart(dstGpuAddr);
    packet.DST_ADDR_HI_UNION.dst_addr_63_32 = HighPart(dstGpuAddr);

    const bool srcCompressed = TestAnyFlagSet(copyFlags, DmaCopyFlags::CompressedCopySrc);
    const bool dstCompressed = TestAnyFlagSet(copyFlags, DmaCopyFlags::CompressedCopyDst);

    if (srcCompressed || dstCompressed)
    {
        SetupMetaData<SDMA_PKT_COPY_LINEAR>(nullptr,
                                            nullptr,
                                            &packet,
                                            srcCompressed,
                                            dstCompressed,
                                            ChNumFormat::Undefined);
    }
    else
    {
        // Packet dword 7 META_CONFIG_UNION is only present when compression is used.
        packetDwords--;
    }

    memcpy(pCmdSpace, &packet, packetDwords * sizeof(uint32));

    return pCmdSpace + packetDwords;
}

// =====================================================================================================================
// Copies memory into the specified region of a typed buffer (linear image). Returns the next unused DWORD in pCmdSpace.
//
uint32* DmaCmdBuffer::WriteCopyTypedBuffer(
    const DmaTypedBufferCopyInfo& typedBufferInfo,
    uint32*                       pCmdSpace
    ) const
{
    constexpr size_t PacketDwords = NumBytesToNumDwords(sizeof(SDMA_PKT_COPY_LINEAR_SUBWIN));
    auto*const       pPacket      = reinterpret_cast<SDMA_PKT_COPY_LINEAR_SUBWIN*>(pCmdSpace);

    SDMA_PKT_COPY_LINEAR_SUBWIN packet = {};

    packet.HEADER_UNION.op                  = SDMA_OP_COPY;
    packet.HEADER_UNION.sub_op              = SDMA_SUBOP_COPY_LINEAR_SUB_WIND;
    packet.HEADER_UNION.tmz                 = TestAnyFlagSet(typedBufferInfo.flags, DmaCopyFlags::TmzCopy);
    packet.HEADER_UNION.elementsize         = Log2(typedBufferInfo.dst.bytesPerElement);

    // Setup the source base address.
    packet.SRC_ADDR_LO_UNION.src_addr_31_0  = LowPart(typedBufferInfo.src.baseAddr);
    packet.SRC_ADDR_HI_UNION.src_addr_63_32 = HighPart(typedBufferInfo.src.baseAddr);

    // Setup the start of the source rect.
    // Offset is 0 since the base address is the actual address of the sub-region

    // Setup the source surface dimensions.
    packet.DW_4_UNION.src_pitch             = typedBufferInfo.src.linearRowPitch - 1;
    packet.DW_5_UNION.src_slice_pitch       = typedBufferInfo.src.linearDepthPitch - 1;

    // Setup the destination base address.
    packet.DST_ADDR_LO_UNION.dst_addr_31_0  = LowPart(typedBufferInfo.dst.baseAddr);
    packet.DST_ADDR_HI_UNION.dst_addr_63_32 = HighPart(typedBufferInfo.dst.baseAddr);

    // Setup the start of the destination rectangle.
    // Offset is 0 since the base address is the actual address of the sub-region

    // Setup the destination surface dimensions.
    // The unit of linear pitch and linear slice is pixel number minus 1.
    packet.DW_9_UNION.dst_pitch             = typedBufferInfo.dst.linearRowPitch - 1;
    packet.DW_10_UNION.dst_slice_pitch      = typedBufferInfo.dst.linearDepthPitch - 1;

    // Setup the rectangle dimensions.
    // rect_dx/dy (14b),  rect_dz(11b): rectangle width/height/depth minus 1.
    packet.DW_11_UNION.rect_x               = typedBufferInfo.copyExtent.width - 1;
    packet.DW_11_UNION.rect_y               = typedBufferInfo.copyExtent.height - 1;
    packet.DW_12_UNION.rect_z               = typedBufferInfo.copyExtent.depth - 1;

    packet.DW_12_UNION.dst_mall_policy = GetMallPolicy(false);
    packet.DW_12_UNION.src_mall_policy = GetMallPolicy(true);

    *pPacket = packet;

    return pCmdSpace + PacketDwords;
}

// =====================================================================================================================
// Copies the specified region between two linear images.
//
uint32* DmaCmdBuffer::WriteCopyImageLinearToLinearCmd(
    const DmaImageCopyInfo& imageCopyInfo,
    uint32*                 pCmdSpace)
{
    constexpr size_t PacketDwords = NumBytesToNumDwords(sizeof(SDMA_PKT_COPY_LINEAR_SUBWIN));
    auto*const       pPacket      = reinterpret_cast<SDMA_PKT_COPY_LINEAR_SUBWIN*>(pCmdSpace);

    SDMA_PKT_COPY_LINEAR_SUBWIN packet = {};

    packet.HEADER_UNION.op          = SDMA_OP_COPY;
    packet.HEADER_UNION.sub_op      = SDMA_SUBOP_COPY_LINEAR_SUB_WIND;
    packet.HEADER_UNION.elementsize = Log2(imageCopyInfo.dst.bytesPerPixel);
    packet.HEADER_UNION.tmz         = IsImageTmzProtected(imageCopyInfo.src);

    // Base addresses should be dword aligned.
    PAL_ASSERT(((imageCopyInfo.src.baseAddr & 0x3) == 0) && ((imageCopyInfo.dst.baseAddr & 0x3) == 0));

    // Setup the source base address.
    packet.SRC_ADDR_LO_UNION.src_addr_31_0  = LowPart(imageCopyInfo.src.baseAddr);
    packet.SRC_ADDR_HI_UNION.src_addr_63_32 = HighPart(imageCopyInfo.src.baseAddr);

    // Setup the start of the source rect.
    packet.DW_3_UNION.src_x     = imageCopyInfo.src.offset.x;
    packet.DW_3_UNION.src_y     = imageCopyInfo.src.offset.y;
    packet.DW_4_UNION.src_z     = GetImageZ(imageCopyInfo.src);

    // Setup the source surface dimensions.
    packet.DW_4_UNION.src_pitch       = GetLinearRowPitch(imageCopyInfo.src);
    packet.DW_5_UNION.src_slice_pitch = GetLinearDepthPitch(imageCopyInfo.src);

    // Setup the destination base address.
    packet.DST_ADDR_LO_UNION.dst_addr_31_0  = LowPart(imageCopyInfo.dst.baseAddr);
    packet.DST_ADDR_HI_UNION.dst_addr_63_32 = HighPart(imageCopyInfo.dst.baseAddr);

    // Setup the start of the destination rectangle.
    packet.DW_8_UNION.dst_x     = imageCopyInfo.dst.offset.x;
    packet.DW_8_UNION.dst_y     = imageCopyInfo.dst.offset.y;
    packet.DW_9_UNION.dst_z     = GetImageZ(imageCopyInfo.dst);

    // Setup the destination surface dimensions.
    packet.DW_9_UNION.dst_pitch        = GetLinearRowPitch(imageCopyInfo.dst);
    packet.DW_10_UNION.dst_slice_pitch = GetLinearDepthPitch(imageCopyInfo.dst);

    /// Setup the rectangle dimensions.
    packet.DW_11_UNION.rect_x     = imageCopyInfo.copyExtent.width  - 1;
    packet.DW_11_UNION.rect_y     = imageCopyInfo.copyExtent.height - 1;
    packet.DW_12_UNION.rect_z     = imageCopyInfo.copyExtent.depth  - 1;

    packet.DW_12_UNION.dst_mall_policy = GetMallPolicy(false);
    packet.DW_12_UNION.src_mall_policy = GetMallPolicy(true);

    *pPacket = packet;

    return pCmdSpace + PacketDwords;
}

// =====================================================================================================================
// Tiled image to tiled image copy.
//
uint32* DmaCmdBuffer::WriteCopyImageTiledToTiledCmd(
    const DmaImageCopyInfo& imageCopyInfo,
    uint32*                 pCmdSpace)
{
    const auto* pAddrMgr   = static_cast<const AddrMgr3::AddrMgr3*>(m_pDevice->GetAddrMgr());
    const auto& src        = imageCopyInfo.src;
    const auto& dst        = imageCopyInfo.dst;
    const auto  srcSwizzle = GetSwizzleMode(src);
    const auto  dstSwizzle = GetSwizzleMode(dst);

    size_t packetDwords = NumBytesToNumDwords(sizeof(SDMA_PKT_COPY_T2T));

    SDMA_PKT_COPY_T2T packet = {};

    // Packet header
    packet.HEADER_UNION.op        = SDMA_OP_COPY;
    packet.HEADER_UNION.sub_op    = SDMA_SUBOP_COPY_T2T_SUB_WIND;
    packet.HEADER_UNION.tmz       = IsImageTmzProtected(imageCopyInfo.src);

    // Setup the start, offset, and dimensions of the source surface.
    packet.SRC_ADDR_LO_UNION.src_addr_31_0  = LowPart(src.baseAddr);
    packet.SRC_ADDR_HI_UNION.src_addr_63_32 = HighPart(src.baseAddr);

    packet.DW_3_UNION.src_x       = src.offset.x;
    packet.DW_3_UNION.src_y       = src.offset.y;
    packet.DW_4_UNION.src_z       = GetImageZ(src);
    packet.DW_4_UNION.src_width   = src.extent.width - 1;
    packet.DW_5_UNION.src_height  = src.extent.height - 1;
    packet.DW_5_UNION.src_depth   = src.extent.depth - 1;

    // Setup the tile mode of the destination surface.
    packet.DW_6_UNION.src_element_size  = Log2(src.bytesPerPixel);
    packet.DW_6_UNION.src_swizzle_mode  = pAddrMgr->GetHwSwizzleMode(srcSwizzle);
    packet.DW_6_UNION.src_dimension     = GetHwDimension(src);
    packet.DW_6_UNION.src_mip_max       = GetMaxMip(src);
    packet.DW_6_UNION.src_mip_id        = src.pSubresInfo->subresId.mipLevel;

    // Setup the start, offset, and dimensions of the destination surface.
    packet.DST_ADDR_LO_UNION.dst_addr_31_0  = LowPart(dst.baseAddr);
    packet.DST_ADDR_HI_UNION.dst_addr_63_32 = HighPart(dst.baseAddr);

    packet.DW_9_UNION.dst_x       = dst.offset.x;
    packet.DW_9_UNION.dst_y       = dst.offset.y;

    packet.DW_10_UNION.dst_z      = GetImageZ(dst);
    packet.DW_10_UNION.dst_width  = dst.extent.width - 1;

    packet.DW_11_UNION.dst_height  = dst.extent.height - 1;
    packet.DW_11_UNION.dst_depth   = dst.extent.depth - 1;

    // Setup the tile mode of the destination surface.
    packet.DW_12_UNION.dst_element_size = Log2(dst.bytesPerPixel);
    packet.DW_12_UNION.dst_swizzle_mode = pAddrMgr->GetHwSwizzleMode(dstSwizzle);
    packet.DW_12_UNION.dst_dimension    = GetHwDimension(dst);
    packet.DW_12_UNION.dst_mip_max      = GetMaxMip(dst);
    packet.DW_12_UNION.dst_mip_id       = dst.pSubresInfo->subresId.mipLevel;

    // Setup the size of the copy region.
    packet.DW_13_UNION.rect_x     = imageCopyInfo.copyExtent.width - 1;
    packet.DW_13_UNION.rect_y     = imageCopyInfo.copyExtent.height - 1;
    packet.DW_14_UNION.rect_z     = imageCopyInfo.copyExtent.depth - 1;

    packet.DW_14_UNION.dst_mall_policy = GetMallPolicy(false);
    packet.DW_14_UNION.src_mall_policy = GetMallPolicy(true);

    const bool srcIsCompressed = IsImageCompressed(src);
    const bool dstIsCompressed = IsImageCompressed(dst);

    if (srcIsCompressed || dstIsCompressed)
    {
        SetupMetaData<SDMA_PKT_COPY_T2T>(&src, &dst, &packet, false, false, ChNumFormat::Undefined);
    }
    else
    {
        // Packet dword 15 META_CONFIG_UNION is only present when compression is used.
        packetDwords--;
    }

    memcpy(pCmdSpace, &packet, packetDwords * sizeof(uint32));
    pCmdSpace += packetDwords;

    return pCmdSpace;
}

// =====================================================================================================================
// Returns true if scanline copies are required for a tiled-to-tiled image copy
bool DmaCmdBuffer::UseT2tScanlineCopy(
    const DmaImageCopyInfo& imageCopyInfo
    ) const
{
    const auto&             src           = imageCopyInfo.src;
    const auto&             dst           = imageCopyInfo.dst;
    const auto&             srcCreateInfo = src.pImage->GetImageCreateInfo();
    const auto&             dstCreateInfo = dst.pImage->GetImageCreateInfo();
    const Addr3SwizzleMode  srcSwizzle    = GetSwizzleMode(src);

    // Assume, that by some miracle, all of the requirements for using the built-in T2T copy are actually met.
    bool useScanlineCopy = false;

    // The alignment requirements for the offsets / rectangle sizes are format and image type dependent.
    // In some 3D transfer cases, the hardware will need to split the transfers into muliple planar copies
    // in which case the 3D alignment table can not be used. Variable name was updated to reflect this.
    static constexpr Extent3d CopyAlignmentsFor2dAndPlanarCopy3d[] =
    {
        { 16, 16, 1 }, // 1bpp
        { 16,  8, 1 }, // 2bpp
        {  8,  8, 1 }, // 4bpp
        {  8,  4, 1 }, // 8bpp
        {  4,  4, 1 }, // 16bpp
    };

    static constexpr Extent3d CopyAlignmentsFor3d[] =
    {
        {  8, 4, 8 }, // 1bpp
        {  4, 4, 8 }, // 2bpp
        {  4, 4, 4 }, // 4bpp
        {  4, 2, 4 }, // 8bpp
        {  2, 2, 4 }, // 16bpp
    };

    // 1D images have to be linear, what are we doing here?
    PAL_ASSERT(srcCreateInfo.imageType != ImageType::Tex1d);

    // This is a violation of the PAL API...
    PAL_ASSERT(srcCreateInfo.imageType == dstCreateInfo.imageType);

    // SDMA engine can't do format conversions.
    PAL_ASSERT(src.bytesPerPixel == dst.bytesPerPixel);

    // 3D StandardSwizzle and 3D DisplayableSwizzle are aligned using the 3D alignment table
    // Otherwise the alignment table for 2D and PlanarCopy 3D is used
    const uint32    log2Bpp        = Util::Log2(src.bytesPerPixel);
    const Extent3d& copyAlignments = (srcCreateInfo.imageType == ImageType::Tex3d)
                                        ? CopyAlignmentsFor3d[log2Bpp]
                                        : CopyAlignmentsFor2dAndPlanarCopy3d[log2Bpp];

    // Have to use scanline copies unless the copy region and the src / dst offsets are properly aligned.
    useScanlineCopy = ((IsAlignedForT2t(imageCopyInfo.copyExtent, copyAlignments) == false) ||
                       (IsAlignedForT2t(src.offset,               copyAlignments) == false) ||
                       (IsAlignedForT2t(dst.offset,               copyAlignments) == false));

    //       This command does not support tiling format transformation, source and destination both should have
    //       the same element size, swizzle mode and Dimension since the HW process this command as a linear copy
    //       within one tile and use two address_calc modules to calculate tile start address.
    useScanlineCopy |= (srcSwizzle != GetSwizzleMode(dst));

    return useScanlineCopy;
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
    constexpr size_t PacketDwords = NumBytesToNumDwords(sizeof(SDMA_PKT_COPY_LINEAR_SUBWIN));
    auto*const       pPacket      = reinterpret_cast<SDMA_PKT_COPY_LINEAR_SUBWIN*>(pCmdSpace);

    SDMA_PKT_COPY_LINEAR_SUBWIN packet = {};

    packet.HEADER_UNION.op          = SDMA_OP_COPY;
    packet.HEADER_UNION.sub_op      = SDMA_SUBOP_COPY_LINEAR_SUB_WIND;
    packet.HEADER_UNION.elementsize = Log2(dstImage.bytesPerPixel);
    packet.HEADER_UNION.tmz         = srcGpuMemory.IsTmzProtected();

    // Setup the source base address.
    const gpusize srcBaseAddr = srcGpuMemory.Desc().gpuVirtAddr + rgn.gpuMemoryOffset;
    packet.SRC_ADDR_LO_UNION.src_addr_31_0  = LowPart(srcBaseAddr);
    packet.SRC_ADDR_HI_UNION.src_addr_63_32 = HighPart(srcBaseAddr);

    // Setup the start of the source rect (all zeros).

    // Setup the source surface dimensions.
    ValidateLinearRowPitch(rgn.gpuMemoryRowPitch, rgn.imageExtent.height, dstImage.bytesPerPixel);
    packet.DW_4_UNION.src_pitch       = GetLinearRowPitch(rgn.gpuMemoryRowPitch, dstImage.bytesPerPixel);
    packet.DW_5_UNION.src_slice_pitch = GetLinearDepthPitch(rgn.gpuMemoryDepthPitch, dstImage.bytesPerPixel);

    // Setup the destination base address.
    packet.DST_ADDR_LO_UNION.dst_addr_31_0  = LowPart(dstImage.baseAddr);
    packet.DST_ADDR_HI_UNION.dst_addr_63_32 = HighPart(dstImage.baseAddr);

    // Setup the start of the destination rectangle.
    packet.DW_8_UNION.dst_x     = rgn.imageOffset.x;
    packet.DW_8_UNION.dst_y     = rgn.imageOffset.y;
    packet.DW_9_UNION.dst_z     = GetImageZ(dstImage, rgn.imageOffset.z);

    // Setup the destination surface dimensions.
    packet.DW_9_UNION.dst_pitch        = GetLinearRowPitch(dstImage);
    packet.DW_10_UNION.dst_slice_pitch = GetLinearDepthPitch(dstImage);

    // Setup the rectangle dimensions.
    packet.DW_11_UNION.rect_x     = rgn.imageExtent.width  - 1;
    packet.DW_11_UNION.rect_y     = rgn.imageExtent.height - 1;
    packet.DW_12_UNION.rect_z     = rgn.imageExtent.depth  - 1;

    packet.DW_12_UNION.dst_mall_policy = GetMallPolicy(false);
    packet.DW_12_UNION.src_mall_policy = GetMallPolicy(true);

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
    constexpr size_t PacketDwords = NumBytesToNumDwords(sizeof(SDMA_PKT_COPY_LINEAR_SUBWIN));
    auto*const       pPacket      = reinterpret_cast<SDMA_PKT_COPY_LINEAR_SUBWIN*>(pCmdSpace);

    SDMA_PKT_COPY_LINEAR_SUBWIN packet = {};

    packet.HEADER_UNION.op          = SDMA_OP_COPY;
    packet.HEADER_UNION.sub_op      = SDMA_SUBOP_COPY_LINEAR_SUB_WIND;
    packet.HEADER_UNION.elementsize = Log2(srcImage.bytesPerPixel);
    packet.HEADER_UNION.tmz         = IsImageTmzProtected(srcImage);

    // Setup the source base address.
    packet.SRC_ADDR_LO_UNION.src_addr_31_0  = LowPart(srcImage.baseAddr);
    packet.SRC_ADDR_HI_UNION.src_addr_63_32 = HighPart(srcImage.baseAddr);

    // Setup the start of the source rect.
    packet.DW_3_UNION.src_x = rgn.imageOffset.x;
    packet.DW_3_UNION.src_y = rgn.imageOffset.y;
    packet.DW_4_UNION.src_z = GetImageZ(srcImage, rgn.imageOffset.z);

    // Setup the source surface dimensions.
    packet.DW_4_UNION.src_pitch       = GetLinearRowPitch(srcImage);
    packet.DW_5_UNION.src_slice_pitch = GetLinearDepthPitch(srcImage);

    // Setup the destination base address.
    const gpusize dstBaseAddr = dstGpuMemory.Desc().gpuVirtAddr + rgn.gpuMemoryOffset;
    packet.DST_ADDR_LO_UNION.dst_addr_31_0  = LowPart(dstBaseAddr);
    packet.DST_ADDR_HI_UNION.dst_addr_63_32 = HighPart(dstBaseAddr);

    // Setup the start of the destination rectangle (all zeros).

    // Setup the destination surface dimensions.
    ValidateLinearRowPitch(rgn.gpuMemoryRowPitch, rgn.imageExtent.height, srcImage.bytesPerPixel);
    packet.DW_9_UNION.dst_pitch        = GetLinearRowPitch(rgn.gpuMemoryRowPitch, srcImage.bytesPerPixel);
    packet.DW_10_UNION.dst_slice_pitch = GetLinearDepthPitch(rgn.gpuMemoryDepthPitch, srcImage.bytesPerPixel);

    /// Setup the rectangle dimensions.
    packet.DW_11_UNION.rect_x = rgn.imageExtent.width  - 1;
    packet.DW_11_UNION.rect_y = rgn.imageExtent.height - 1;
    packet.DW_12_UNION.rect_z = rgn.imageExtent.depth  - 1;

    packet.DW_12_UNION.dst_mall_policy = GetMallPolicy(false);
    packet.DW_12_UNION.src_mall_policy = GetMallPolicy(true);

    *pPacket = packet;

    return pCmdSpace + PacketDwords;
}

// =====================================================================================================================
// Copies the data from "pData" into the dstGpuMemory.
//
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

    // We're likely limited by the size of the embedded data.
    const uint32 maxDataDwords = GetEmbeddedDataLimit();

    // Loop until we've submitted enough packets to upload the whole src buffer.
    const uint32* pRemainingSrcData   = pData;
    uint32        remainingDataDwords = static_cast<uint32>(dataSize) / sizeof(uint32);

    const GpuMemory& dstGpuMem = static_cast<const GpuMemory&>(dstGpuMemory);

    while (remainingDataDwords > 0)
    {
        gpusize      offset           = 0;
        GpuMemory*   pGpuMem          = nullptr;
        const uint32 packetDataDwords = Min(remainingDataDwords, maxDataDwords);
        uint32*      pEmbeddedData    = CmdAllocateEmbeddedData(packetDataDwords, 1u, &pGpuMem, &offset);
        gpusize      gpuVa            = pGpuMem->Desc().gpuVirtAddr + offset;

        DmaCopyFlags copyFlags = DmaCopyFlags::None;

        if (pGpuMem->MaybeCompressed())
        {
            copyFlags |= DmaCopyFlags::CompressedCopySrc;
        }
        if (dstGpuMem.MaybeCompressed())
        {
            copyFlags |= DmaCopyFlags::CompressedCopyDst;
        }

        // Copy the src data into memory prepared for embedded data.
        memcpy(pEmbeddedData, pRemainingSrcData, sizeof(uint32) * packetDataDwords);

        gpusize bytesJustCopied = 0;
        gpusize bytesLeftToCopy = static_cast<gpusize>(packetDataDwords * sizeof(uint32));
        gpusize srcGpuAddr      = gpuVa;
        gpusize dstGpuAddr      = dstAddr;
        // Copy the embedded data into dstAddr.

        while (bytesLeftToCopy > 0)
        {
            uint32* pCmdSpace = m_cmdStream.ReserveCommands();
            pCmdSpace = WriteCopyGpuMemoryCmd(
                gpuVa,
                dstAddr,
                bytesLeftToCopy,
                copyFlags,
                pCmdSpace,
                &bytesJustCopied);
            m_cmdStream.CommitCommands(pCmdSpace);

            bytesLeftToCopy     -= bytesJustCopied;
            srcGpuAddr          += bytesJustCopied;
            dstGpuAddr          += bytesJustCopied;
        }

        // Update all variable addresses and sizes.
        remainingDataDwords -= packetDataDwords;
        pRemainingSrcData   += packetDataDwords;
        dstAddr             += packetDataDwords * sizeof(uint32);
    }
}

// =====================================================================================================================
// Writes an immediate value to specified address.
void DmaCmdBuffer::CmdWriteImmediate(
    uint32             stageMask, // Bitmask of PipelineStageFlag
    uint64             data,
    ImmediateDataWidth dataSize,
    gpusize            address)
{
    // Make sure our destination address is dword aligned.
    PAL_ASSERT(IsPow2Aligned(address, sizeof(uint32)));

    uint32* pCmdSpace = m_cmdStream.ReserveCommands();

    constexpr size_t PacketDwords = sizeof(SDMA_PKT_FENCE) / sizeof(uint32);
    auto*const       pPacket      = reinterpret_cast<SDMA_PKT_FENCE*>(pCmdSpace);

    SDMA_PKT_FENCE packet = {};

    packet.HEADER_UNION.op          = SDMA_OP_FENCE;
    packet.HEADER_UNION.mall_policy = GetMallPolicy(false);
    packet.ADDR_LO_UNION.addr_31_2  = LowPart(address) >> 2; // Dword aligned
    packet.ADDR_HI_UNION.addr_63_32 = HighPart(address);
    packet.DATA_UNION.DW_3_DATA     = LowPart(data);

    *pPacket = packet;
    size_t dwordsWritten = PacketDwords;

    if (dataSize == ImmediateDataWidth::ImmediateData64Bit)
    {
        address += sizeof(uint32);
        packet.ADDR_LO_UNION.addr_31_2  = LowPart(address) >> 2; // Dword aligned
        packet.ADDR_HI_UNION.addr_63_32 = HighPart(address);
        packet.DATA_UNION.DW_3_DATA     = HighPart(data);

        pPacket[1]     = packet;
        dwordsWritten += PacketDwords;
    }

    m_cmdStream.CommitCommands(pCmdSpace + dwordsWritten);
}

// =====================================================================================================================
// Performs a memset on the specified memory region using the specified "data" value. Returns the next unused DWORD in
// pCmdSpace.
//
uint32* DmaCmdBuffer::WriteFillMemoryCmd(
    gpusize  dstAddr,
    gpusize  byteSize,
    uint32   data,
    bool     isBufferCompressed,
    uint32*  pCmdSpace,
    gpusize* pBytesCopied // [out] How many bytes out of byteSize this call was able to transfer.
    ) const
{
    constexpr size_t PacketDwords = NumBytesToNumDwords(sizeof(SDMA_PKT_CONSTANT_FILL));
    auto*const       pPacket      = reinterpret_cast<SDMA_PKT_CONSTANT_FILL*>(pCmdSpace);

    SDMA_PKT_CONSTANT_FILL packet = {};

    packet.HEADER_UNION.op                  = SDMA_OP_CONST_FILL;
    packet.HEADER_UNION.nopte_comp          = isBufferCompressed ? 1 : 0; // 0 - Compression write bypass
                                                                          // 1 - Compression write disable
    packet.HEADER_UNION.mall_policy         = GetMallPolicy(false);
    packet.HEADER_UNION.fillsize            = 2;  // 2 size means that "count" is in dwords
    packet.DST_ADDR_LO_UNION.dst_addr_31_0  = LowPart(dstAddr);
    packet.DST_ADDR_HI_UNION.dst_addr_63_32 = HighPart(dstAddr);
    packet.DATA_UNION.src_data_31_0         = data;

    // Because we will set fillsize = 2, the low two bits of our "count" are ignored, but we still program
    // this in terms of bytes.
    constexpr gpusize MaxFillSize = ((1ul << 30ull) - 1ull) & (~0x3ull);
    *pBytesCopied = Min(byteSize, MaxFillSize);

    packet.COUNT_UNION.count                = *pBytesCopied - 4;

    *pPacket = packet;

    return pCmdSpace + PacketDwords;
}

// =====================================================================================================================
// Adds commands necessary to write "data" to the specified memory
//
void DmaCmdBuffer::WriteEventCmd(
    const BoundGpuMemory& boundMemObj,
    uint32                stageMask,   // Bitmask of PipelineStageFlag
    uint32                data)
{
    const gpusize dstAddr = boundMemObj.GpuVirtAddr();

    // Make sure our destination address is dword aligned.
    PAL_ASSERT(IsPow2Aligned(dstAddr, sizeof(uint32)));

    uint32* pCmdSpace = m_cmdStream.ReserveCommands();

    constexpr size_t PacketDwords = NumBytesToNumDwords(sizeof(SDMA_PKT_FENCE));
    auto*            pPacket      = reinterpret_cast<SDMA_PKT_FENCE*>(pCmdSpace);

    SDMA_PKT_FENCE packet = {};

    packet.HEADER_UNION.op          = SDMA_OP_FENCE;
    packet.HEADER_UNION.mall_policy = GetMallPolicy(false);
    packet.ADDR_LO_UNION.addr_31_2  = LowPart(dstAddr) >> 2; // Dword aligned
    packet.ADDR_HI_UNION.addr_63_32 = HighPart(dstAddr);
    packet.DATA_UNION.DW_3_DATA     = data;

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
    return BuildNops(pCmdSpace, numDwords);
}

// =====================================================================================================================
// Returns true if the supplied image has any meta-data associated with it.
bool DmaCmdBuffer::IsImageCompressed(
    const DmaImageInfo& imageInfo)
{
    const Pal::Image* pPalImage = static_cast<const Pal::Image*>(imageInfo.pImage);
    PAL_ASSERT(pPalImage->GetBoundGpuMemory().IsBound());

    return pPalImage->GetBoundGpuMemory().Memory()->MaybeCompressed();
}

// =====================================================================================================================
// Return the read / write compression mode for image or buffer.
// The nullptr pImageInfo parameter is valid which indicates it is buffer.
uint32 DmaCmdBuffer::GetCompressionMode(
    const DmaImageInfo* pImageInfo,
    bool                isRead,
    bool                bufferCompressed
    ) const
{
    uint32 compressionMode = static_cast<uint32>(CompressionMode::Default);

    const Gfx12PalSettings& settings = GetGfx12Settings(m_pDevice);

    bool isImage = (pImageInfo != nullptr);

    static_assert((uint32(CompressionMode::Default)                == SdmaImageCompressionDefault)                &&
                  (uint32(CompressionMode::ReadEnableWriteEnable)  == SdmaImageCompressionReadEnableWriteEnable)  &&
                  (uint32(CompressionMode::ReadEnableWriteDisable) == SdmaImageCompressionReadEnableWriteDisable));

    static_assert((uint32(CompressionMode::Default)                == SdmaBufferCompressionDefault)                &&
                  (uint32(CompressionMode::ReadEnableWriteEnable)  == SdmaBufferCompressionReadEnableWriteEnable)  &&
                  (uint32(CompressionMode::ReadEnableWriteDisable) == SdmaBufferCompressionReadEnableWriteDisable));

    CompressionMode finalCompressionMode = isImage ? static_cast<CompressionMode>(settings.sdmaImageCompressionMode)
                                                   : static_cast<CompressionMode>(settings.sdmaBufferCompressionMode);

    if (finalCompressionMode == CompressionMode::Default)
    {
        if (isImage)
        {
            const Pal::Image* pPalImage = static_cast<const Pal::Image*>(pImageInfo->pImage);
            const Device*     pDevice   = static_cast<const Pal::Gfx12::Device*>(m_pDevice->GetGfxDevice());
            finalCompressionMode = pDevice->GetImageViewCompressionMode(CompressionMode::Default,
                                                                        pPalImage->GetImageCreateInfo().compressionMode,
                                                                        pPalImage->GetBoundGpuMemory().Memory());
        }
        else
        {
            finalCompressionMode = bufferCompressed ? CompressionMode::ReadEnableWriteDisable :
                                                      CompressionMode::ReadBypassWriteDisable;
        }
    }

    if (isRead)
    {
        switch (finalCompressionMode)
        {
        case CompressionMode::Default:
        case CompressionMode::ReadEnableWriteEnable:
        case CompressionMode::ReadEnableWriteDisable:
            compressionMode = static_cast<uint32>(SdmaReadCompressionMode::ReadDecompressed);
            break;
        case CompressionMode::ReadBypassWriteDisable:
            compressionMode =
                static_cast<uint32>(settings.enableCompressionReadBypass ? SdmaReadCompressionMode::BypassCompression
                                                                         : SdmaReadCompressionMode::ReadDecompressed);
            break;
        default:
            PAL_NEVER_CALLED();
        }
    }
    else
    {
        switch (finalCompressionMode)
        {
        case CompressionMode::Default:
        case CompressionMode::ReadEnableWriteEnable:
            compressionMode = static_cast<uint32>(SdmaWriteCompressionMode::EnableCompression);
            break;
        case CompressionMode::ReadEnableWriteDisable:
        case CompressionMode::ReadBypassWriteDisable:
            compressionMode = static_cast<uint32>(SdmaWriteCompressionMode::WriteCompressionDisabled);
            break;
        default:
            PAL_NEVER_CALLED();
        }
    }

    return compressionMode;
}

// =====================================================================================================================
// The copy-tiled-subwindow packet has added support for understanding the concept of metadata, compressed surfaces
// etc.  Setup those fields here.
// The nullptr pImageInfo parameter is valid which indicates it is buffer.
template <typename PacketName>
void DmaCmdBuffer::SetupMetaData(
    const DmaImageInfo* pSrcImageInfo,
    const DmaImageInfo* pDstImageInfo,
    PacketName*         pPacket,
    bool                srcBufferCompressed,
    bool                dstBufferCompressed,
    ChNumFormat         dstBufferFormat
    ) const
{
    const bool srcCompressed = (pSrcImageInfo == nullptr) ? srcBufferCompressed : IsImageCompressed(*pSrcImageInfo);
    const bool dstCompressed = (pDstImageInfo == nullptr) ? dstBufferCompressed : IsImageCompressed(*pDstImageInfo);

    pPacket->HEADER_UNION.dcc = 1;

    auto* pMetaConfig = &pPacket->META_CONFIG_UNION;

    if (srcCompressed)
    {
        pMetaConfig->read_compression_mode  = GetCompressionMode(pSrcImageInfo, true, srcBufferCompressed);
    }

    if (dstCompressed)
    {
        uint32      dstMaxCompressedSize;
        uint32      dstMaxUncompressedSize;
        ChNumFormat format;

        if (pDstImageInfo != nullptr)
        {
            const Pal::Image& image      = *static_cast<const Pal::Image*>(pDstImageInfo->pImage);
            const Image&      gfx12Image = *static_cast<Image*>(image.GetGfxImage());
            const uint32      plane      = pDstImageInfo->pSubresInfo->subresId.plane;

            format                 = image.GetImageCreateInfo().swizzledFormat.format;
            dstMaxCompressedSize   = gfx12Image.GetMaxCompressedSize(plane);
            dstMaxUncompressedSize = gfx12Image.GetMaxUncompressedSize(plane);
        }
        else // Buffer
        {
            const auto& gfx12Device = static_cast<const Gfx12::Device&>(*m_pDevice->GetGfxDevice());

            // DCC works for all formats. Once compressed, the info is in compressed key and it works
            // even if you read out with a different format through buffer SRD.
            //
            // For buffer copy, there may be no format provided. We use default format X32_Uint in this case.
            format = (dstBufferFormat != ChNumFormat::Undefined) ? dstBufferFormat : ChNumFormat::X32_Uint;

            // Buffer uses default control settings
            dstMaxCompressedSize   = gfx12Device.Settings().defaultMaxCompressedBlockSize;
            dstMaxUncompressedSize = DefaultMaxUncompressedSize;
        }

        pMetaConfig->data_format            = Formats::Gfx12::HwColorFmt(format);
        pMetaConfig->number_type            = Formats::Gfx12::ColorSurfNum(format);
        pMetaConfig->write_compression_mode = GetCompressionMode(pDstImageInfo, false, dstBufferCompressed);
        pMetaConfig->max_comp_block_size    = dstMaxCompressedSize;
        pMetaConfig->max_uncomp_block_size  = dstMaxUncompressedSize;
    }
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
    uint32*                 pCmdSpace
    ) const
{
    const auto*  pAddrMgr     = static_cast<const AddrMgr3::AddrMgr3*>(m_pDevice->GetAddrMgr());
    const auto*  pPalImage    = static_cast<const Pal::Image*>(tiledImg.pImage);

    size_t packetDwords = NumBytesToNumDwords(sizeof(SDMA_PKT_COPY_TILED_SUBWIN));

    SDMA_PKT_COPY_TILED_SUBWIN packet = {};

    packet.HEADER_UNION.op         = SDMA_OP_COPY;
    packet.HEADER_UNION.sub_op     = SDMA_SUBOP_COPY_TILED_SUB_WIND;
    packet.HEADER_UNION.tmz        = IsImageTmzProtected(copyInfo.src);
    packet.HEADER_UNION.detile     = deTile;

    // Setup the tiled surface here.
    packet.TILED_ADDR_LO_UNION.tiled_addr_31_0  = LowPart(tiledImg.baseAddr);
    packet.TILED_ADDR_HI_UNION.tiled_addr_63_32 = HighPart(tiledImg.baseAddr);

    // Setup the tiled start location.
    packet.DW_3_UNION.tiled_x      = tiledImg.offset.x;
    packet.DW_3_UNION.tiled_y      = tiledImg.offset.y;

    packet.DW_4_UNION.tiled_z      = GetImageZ(tiledImg);
    packet.DW_4_UNION.width        = tiledImg.extent.width - 1;

    // Setup the tiled surface dimensions.
    packet.DW_5_UNION.height       = tiledImg.extent.height - 1;
    packet.DW_5_UNION.depth        = tiledImg.extent.depth - 1;

    packet.DW_6_UNION.element_size = Log2(tiledImg.bytesPerPixel);
    packet.DW_6_UNION.swizzle_mode = pAddrMgr->GetHwSwizzleMode(GetSwizzleMode(tiledImg));
    packet.DW_6_UNION.dimension    = GetHwDimension(tiledImg);
    packet.DW_6_UNION.mip_max      = GetMaxMip(tiledImg);
    packet.DW_6_UNION.mip_id       = tiledImg.pSubresInfo->subresId.mipLevel;

    // Setup the linear surface here.
    packet.LINEAR_ADDR_LO_UNION.linear_addr_31_0  = LowPart(linearImg.baseAddr);
    packet.LINEAR_ADDR_HI_UNION.linear_addr_63_32 = HighPart(linearImg.baseAddr);

    // Setup the linear start location.
    packet.DW_9_UNION.linear_x     = linearImg.offset.x;
    packet.DW_9_UNION.linear_y     = linearImg.offset.y;
    packet.DW_10_UNION.linear_z    = GetImageZ(linearImg);

    // Linear is the source.
    packet.DW_10_UNION.linear_pitch       = GetLinearRowPitch(linearImg);
    packet.DW_11_UNION.linear_slice_pitch = GetLinearDepthPitch(linearImg);

    // Setup the rectangle to copy.
    packet.DW_12_UNION.rect_x      = copyInfo.copyExtent.width - 1;
    packet.DW_12_UNION.rect_y      = copyInfo.copyExtent.height - 1;
    packet.DW_13_UNION.rect_z      = copyInfo.copyExtent.depth - 1;

    packet.DW_13_UNION.linear_mall_policy = GetMallPolicy(deTile ? false : true);
    packet.DW_13_UNION.tile_mall_policy   = GetMallPolicy(deTile ? true  : false);

    const bool tiledImgIsCompressed  = IsImageCompressed(tiledImg);
    const bool linearImgIsCompressed = IsImageCompressed(linearImg);

    if (tiledImgIsCompressed || linearImgIsCompressed)
    {
        const DmaImageInfo* pSrc = deTile ? &tiledImg : &linearImg;
        const DmaImageInfo* pDst = deTile ? &linearImg : &tiledImg;

        SetupMetaData<SDMA_PKT_COPY_TILED_SUBWIN>(pSrc, pDst, &packet, false, false, ChNumFormat::Undefined);
    }
    else
    {
        // Packet dword 14 META_CONFIG_UNION is only present when compression is used.
        packetDwords--;
    }

    memcpy(pCmdSpace, &packet, packetDwords * sizeof(uint32));
    pCmdSpace += packetDwords;

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
    uint32*                      pCmdSpace
    ) const
{
    const auto*  pAddrMgr     = static_cast<const AddrMgr3::AddrMgr3*>(m_pDevice->GetAddrMgr());

    size_t packetDwords = NumBytesToNumDwords(sizeof(SDMA_PKT_COPY_TILED_SUBWIN));

    SDMA_PKT_COPY_TILED_SUBWIN packet = {};

    packet.HEADER_UNION.op         = SDMA_OP_COPY;
    packet.HEADER_UNION.sub_op     = SDMA_SUBOP_COPY_TILED_SUB_WIND;
    packet.HEADER_UNION.tmz        = deTile ? IsImageTmzProtected(image) : gpuMemory.IsTmzProtected();
    packet.HEADER_UNION.detile     = deTile; // One packet handles both directions.

    // Setup the tiled surface here.
    packet.TILED_ADDR_LO_UNION.tiled_addr_31_0  = LowPart(image.baseAddr);
    packet.TILED_ADDR_HI_UNION.tiled_addr_63_32 = HighPart(image.baseAddr);

    // Setup the tiled start location.
    packet.DW_3_UNION.tiled_x      = rgn.imageOffset.x;
    packet.DW_3_UNION.tiled_y      = rgn.imageOffset.y;

    packet.DW_4_UNION.tiled_z      = GetImageZ(image, rgn.imageOffset.z);
    packet.DW_4_UNION.width        = image.extent.width - 1;

    // Setup the tiled surface dimensions.
    packet.DW_5_UNION.height       = image.extent.height - 1;
    packet.DW_5_UNION.depth        = image.extent.depth - 1;

    packet.DW_6_UNION.element_size = Log2(image.bytesPerPixel);
    packet.DW_6_UNION.swizzle_mode = pAddrMgr->GetHwSwizzleMode(GetSwizzleMode(image));
    packet.DW_6_UNION.dimension    = GetHwDimension(image);
    packet.DW_6_UNION.mip_max      = GetMaxMip(image);
    packet.DW_6_UNION.mip_id       = image.pSubresInfo->subresId.mipLevel;

    // Setup the linear surface here.
    const gpusize linearBaseAddr = gpuMemory.Desc().gpuVirtAddr + rgn.gpuMemoryOffset;
    packet.LINEAR_ADDR_LO_UNION.linear_addr_31_0  = LowPart(linearBaseAddr);
    packet.LINEAR_ADDR_HI_UNION.linear_addr_63_32 = HighPart(linearBaseAddr);

    // Setup the linear start location (all zeros).

    // Setup the linear surface dimensions.
    ValidateLinearRowPitch(rgn.gpuMemoryRowPitch, rgn.imageExtent.height, image.bytesPerPixel);
    packet.DW_10_UNION.linear_pitch       = GetLinearRowPitch(rgn.gpuMemoryRowPitch, image.bytesPerPixel);
    packet.DW_11_UNION.linear_slice_pitch = GetLinearDepthPitch(rgn.gpuMemoryDepthPitch, image.bytesPerPixel);

    // Setup the rectangle to copy.
    packet.DW_12_UNION.rect_x      = rgn.imageExtent.width  - 1;
    packet.DW_12_UNION.rect_y      = rgn.imageExtent.height - 1;
    packet.DW_13_UNION.rect_z      = rgn.imageExtent.depth  - 1;

    packet.DW_13_UNION.linear_mall_policy = GetMallPolicy(deTile ? false : true);
    packet.DW_13_UNION.tile_mall_policy   = GetMallPolicy(deTile ? true  : false);

    const bool imageIsCompressed  = IsImageCompressed(image);
    const bool bufferIsCompressed = gpuMemory.MaybeCompressed();

    if (imageIsCompressed || bufferIsCompressed)
    {
        const DmaImageInfo* pSrc = deTile ? &image : nullptr;
        const DmaImageInfo* pDst = deTile ? nullptr : &image;

        const ChNumFormat dstBufferFormat = (rgn.swizzledFormat.format != ChNumFormat::Undefined)
                                            ? rgn.swizzledFormat.format
                                            : image.pSubresInfo->format.format;

        SetupMetaData<SDMA_PKT_COPY_TILED_SUBWIN>(pSrc,
                                                  pDst,
                                                  &packet,
                                                  bufferIsCompressed,
                                                  bufferIsCompressed,
                                                  dstBufferFormat);
    }
    else
    {
        // Packet dword 14 META_CONFIG_UNION is only present when compression is used.
        packetDwords--;
    }

    memcpy(pCmdSpace, &packet, packetDwords * sizeof(uint32));
    pCmdSpace += packetDwords;

    return pCmdSpace;
}

// =====================================================================================================================
// Returns the dimension (1D, 2D, 3D) of the specified surface as a HW enumeration
uint32 DmaCmdBuffer::GetHwDimension(
    const DmaImageInfo& dmaImageInfo)
{
    const Pal::ImageType imageType = dmaImageInfo.pImage->GetImageCreateInfo().imageType;

    // The HW dimension enumerations match our image-type dimensions.  i.e., 0 = linear/1d, 1 = 2d, 2 = 3d.
    return uint32(imageType);
}

// =====================================================================================================================
uint32 DmaCmdBuffer::GetLinearRowPitch(
    gpusize  rowPitchInBytes,
    uint32   bytesPerPixel
    ) const
{
    const uint32 rowPitchInPixels = uint32(rowPitchInBytes / bytesPerPixel);

    // The unit of linear pitch ... is pixel number minus 1
    return rowPitchInPixels - 1;
}

// =====================================================================================================================
void DmaCmdBuffer::ValidateLinearRowPitch(
    gpusize rowPitchInBytes,
    gpusize height,
    uint32  bytesPerPixel)
{
#if PAL_ENABLE_PRINTS_ASSERTS
    PAL_ASSERT((rowPitchInBytes % bytesPerPixel) == 0);
    // If this linear image's height is 1, no need to pad it's pitch to dword as SDMA engine
    // doesn't need this info to calculate the next row's address.
    if (height > 1)
    {
        const uint32 rowPitchInPixels = static_cast<uint32>(rowPitchInBytes / bytesPerPixel);
        //  The alignment restriction of linear pitch is:
        //    Multiple of 4 for 8bpp
        //    Multiple of 2 for 16bpp
        //    Multiple of 1 for 32bpp
        if ((rowPitchInPixels % Util::Max(1u, (4 / bytesPerPixel))) != 0)
        {
            PAL_ASSERT_ALWAYS_MSG("Invalid RowPitch of linear image.");
        }
    }
#endif
}

// =====================================================================================================================
// Returns the maximum number of mip levels that are associated with the specified image.  Doesn't count the base level
uint32 DmaCmdBuffer::GetMaxMip(
    const DmaImageInfo& dmaImageInfo)
{
    const auto& imageCreateInfo = dmaImageInfo.pImage->GetImageCreateInfo();

    return (imageCreateInfo.mipLevels - 1);
}

// =====================================================================================================================
// Returns the swizzle mode as a SW enumeration (Addr3SwizzleMode) for the specified image
Addr3SwizzleMode DmaCmdBuffer::GetSwizzleMode(
    const DmaImageInfo& dmaImageInfo)
{
    const auto* const      pImage    = static_cast<const Pal::Image*>(dmaImageInfo.pImage);
    const GfxImage* const  pGfxImage = pImage->GetGfxImage();
    const Addr3SwizzleMode tileMode  = static_cast<Addr3SwizzleMode>(pGfxImage->GetSwTileMode(dmaImageInfo.pSubresInfo));

    return tileMode;
}

// =====================================================================================================================
// Returns the pipe/bank xor value for the specified image / subresource.
uint32 DmaCmdBuffer::GetPipeBankXor(
    const Pal::Image& image,
    SubresId          subresource)
{
    const auto*  pTileInfo = AddrMgr3::GetTileInfo(&image, subresource);

    return pTileInfo->pipeBankXor;
}

// =====================================================================================================================
// Returns the base address for HW programming purposes of the specified sub-resource, complete with any pipe-bank-xor
// bits included.  Since in some situations the HW calculates the mip-level and array slice offsets itself, those may
// not be reflected in ther returned address.
gpusize DmaCmdBuffer::GetSubresourceBaseAddr(
    const Pal::Image& image,
    SubresId          subresource
    ) const
{
    gpusize      baseAddr   = 0;
    const uint32 arraySlice = (image.IsYuvPlanarArray() ? subresource.arraySlice : 0);

    if (image.IsSubResourceLinear(subresource))
    {
        const SubresId baseSubres = Subres(subresource.plane, subresource.mipLevel, arraySlice);

        // Verify that we don't have to take into account the pipe/bank xor value here.
        PAL_ASSERT(GetPipeBankXor(image, subresource) == 0);

        // Return the address of the subresource.
        baseAddr = image.GetSubresourceBaseAddr(baseSubres);
    }
    else
    {
        const GfxImage* pGfxImage = image.GetGfxImage();

        baseAddr = pGfxImage->GetPlaneBaseAddr(subresource.plane, arraySlice);
    }

    return baseAddr;
}

// =====================================================================================================================
// Returns the multiplier required to align the linear row pitch with Gfx10 HW requirements
uint32 DmaCmdBuffer::GetLinearRowPitchAlignment(
    uint32 bytesPerPixel
    ) const
{
    return Util::Max(1u, (4 / bytesPerPixel));
}

// =====================================================================================================================
// Gfx12 assumes that tiled images will also be programmed with the dimensions of the base mip level, so retrieve those
// dimensions here.  It doesn't really matter for linear images since the extent information isn't used for linear
// images.  Besides, GFX12 doesn't support linear mip-mapped images anyway.
void DmaCmdBuffer::SetupDmaInfoExtent(
    DmaImageInfo*  pImageInfo
    ) const
{
    const Pal::Image* pImage          = reinterpret_cast<const Pal::Image*>(pImageInfo->pImage);
    const SubresId    baseSubResId    = { pImageInfo->pSubresInfo->subresId.plane, 0, 0 };
    const auto*       pBaseSubResInfo = pImage->SubresourceInfo(baseSubResId);
    const uint32      bytesPerPixel   = pBaseSubResInfo->bitsPerTexel / 8;
    const bool        nonPow2Bpp      = (IsPowerOfTwo(bytesPerPixel) == false);

    if (nonPow2Bpp || Formats::IsBlockCompressed(pImageInfo->pSubresInfo->format.format))
    {
        pImageInfo->extent       = pBaseSubResInfo->extentElements;
        pImageInfo->actualExtent = pBaseSubResInfo->actualExtentElements;
    }
    else
    {
        pImageInfo->extent       = pBaseSubResInfo->extentTexels;
        pImageInfo->actualExtent = pBaseSubResInfo->actualExtentTexels;
    }

    if (pImageInfo->pImage->GetImageCreateInfo().imageType != ImageType::Tex3d)
    {
        pImageInfo->extent.depth = pImageInfo->pImage->GetImageCreateInfo().arraySize;
    }
}

// =====================================================================================================================
DmaCmdBuffer::DmaMemImageCopyMethod DmaCmdBuffer::GetMemImageCopyMethod(
    bool                         isLinearImg,
    const DmaImageInfo&          imageInfo,
    const MemoryImageCopyRegion& region
    ) const
{
    DmaMemImageCopyMethod copyMethod = DmaCmdBuffer::DmaMemImageCopyMethod::Native;

    // On OSS-7.0, the linear pitch (gpuMemoryRowPitch) needs to be dword aligned for linear and tiled subwindow copy
    // and the linear slice pitch (gpuMemoryDepthPitch) needs to be dword aligned for tiled subwindow copy.
    if ((IsPow2Aligned(region.gpuMemoryRowPitch, sizeof(uint32)) == false) ||
        ((IsPow2Aligned(region.gpuMemoryDepthPitch, sizeof(uint32)) == false) && (isLinearImg == false)))
    {
        copyMethod = DmaMemImageCopyMethod::DwordUnaligned;
    }

    return copyMethod;
}

} // Gfx12
} // Pal
