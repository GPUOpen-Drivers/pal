/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2023-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/gfx12/gfx12CmdUtil.h"
#include "core/hw/gfxip/gfx12/gfx12Device.h"
#include "core/hw/gfxip/gfx12/gfx12UserDataLayout.h"
#include "palInlineFuncs.h"

using namespace Util;
using namespace Pal::Gfx12::Chip;

namespace Pal
{
namespace Gfx12
{
static constexpr ME_EVENT_WRITE_event_index_enum VgtEventIndex[]=
{
    event_index__me_event_write__other,                                  // 0x0: Reserved_0x00,
    event_index__me_event_write__other,                                  // 0x1: SAMPLE_STREAMOUTSTATS1,
    event_index__me_event_write__other,                                  // 0x2: SAMPLE_STREAMOUTSTATS2,
    event_index__me_event_write__other,                                  // 0x3: SAMPLE_STREAMOUTSTATS3,
    event_index__me_event_write__other,                                  // 0x4: CACHE_FLUSH_TS,
    event_index__me_event_write__other,                                  // 0x5: CONTEXT_DONE,
    event_index__me_event_write__other,                                  // 0x6: CACHE_FLUSH,
    event_index__me_event_write__cs_vs_ps_partial_flush,                 // 0x7: CS_PARTIAL_FLUSH,
    event_index__me_event_write__other,                                  // 0x8: VGT_STREAMOUT_SYNC,
    event_index__me_event_write__other,                                  // 0x9: Reserved_0x09,
    event_index__me_event_write__other,                                  // 0xa: VGT_STREAMOUT_RESET,
    event_index__me_event_write__other,                                  // 0xb: END_OF_PIPE_INCR_DE,
    event_index__me_event_write__other,                                  // 0xc: END_OF_PIPE_IB_END,
    event_index__me_event_write__other,                                  // 0xd: RST_PIX_CNT,
    event_index__me_event_write__other,                                  // 0xe: BREAK_BATCH,
    event_index__me_event_write__cs_vs_ps_partial_flush,                 // 0xf: VS_PARTIAL_FLUSH,
    event_index__me_event_write__cs_vs_ps_partial_flush,                 // 0x10: PS_PARTIAL_FLUSH,
    event_index__me_event_write__other,                                  // 0x11: FLUSH_HS_OUTPUT,
    event_index__me_event_write__other,                                  // 0x12: FLUSH_DFSM,
    event_index__me_event_write__other,                                  // 0x13: RESET_TO_LOWEST_VGT,
    event_index__me_event_write__other,                                  // 0x14: CACHE_FLUSH_AND_INV_TS_EVENT,
    event_index__me_event_write__pixel_pipe_stat_control_or_dump,        // 0x15: ZPASS_DONE,
    event_index__me_event_write__other,                                  // 0x16: CACHE_FLUSH_AND_INV_EVENT,
    event_index__me_event_write__other,                                  // 0x17: PERFCOUNTER_START,
    event_index__me_event_write__other,                                  // 0x18: PERFCOUNTER_STOP,
    event_index__me_event_write__other,                                  // 0x19: PIPELINESTAT_START,
    event_index__me_event_write__other,                                  // 0x1a: PIPELINESTAT_STOP,
    event_index__me_event_write__other,                                  // 0x1b: PERFCOUNTER_SAMPLE,
    event_index__me_event_write__other,                                  // 0x1c: FLUSH_ES_OUTPUT,
    event_index__me_event_write__other,                                  // 0x1d: BIN_CONF_OVERRIDE_CHECK,
    event_index__me_event_write__sample_pipelinestat,                    // 0x1e: SAMPLE_PIPELINESTAT,
    event_index__me_event_write__other,                                  // 0x1f: SO_VGTSTREAMOUT_FLUSH,
    event_index__me_event_write__other,                                  // 0x20: SAMPLE_STREAMOUTSTATS,
    event_index__me_event_write__other,                                  // 0x21: RESET_VTX_CNT,
    event_index__me_event_write__other,                                  // 0x22: BLOCK_CONTEXT_DONE,
    event_index__me_event_write__other,                                  // 0x23: CS_CONTEXT_DONE,
    event_index__me_event_write__other,                                  // 0x24: VGT_FLUSH,
    event_index__me_event_write__other,                                  // 0x25: TGID_ROLLOVER,
    event_index__me_event_write__other,                                  // 0x26: SQ_NON_EVENT,
    event_index__me_event_write__other,                                  // 0x27: SC_SEND_DB_VPZ,
    event_index__me_event_write__other,                                  // 0x28: BOTTOM_OF_PIPE_TS,
    event_index__me_event_write__other,                                  // 0x29: FLUSH_SX_TS,
    event_index__me_event_write__other,                                  // 0x2a: DB_CACHE_FLUSH_AND_INV,
    event_index__me_event_write__other,                                  // 0x2b: FLUSH_AND_INV_DB_DATA_TS,
    event_index__me_event_write__other,                                  // 0x2c: FLUSH_AND_INV_DB_META,
    event_index__me_event_write__other,                                  // 0x2d: FLUSH_AND_INV_CB_DATA_TS,
    event_index__me_event_write__other,                                  // 0x2e: FLUSH_AND_INV_CB_META,
    event_index__me_event_write__other,                                  // 0x2f: CS_DONE,
    event_index__me_event_write__other,                                  // 0x30: PS_DONE,
    event_index__me_event_write__other,                                  // 0x31: FLUSH_AND_INV_CB_PIXEL_DATA,
    event_index__me_event_write__other,                                  // 0x32: SX_CB_RAT_ACK_REQUEST,
    event_index__me_event_write__other,                                  // 0x33: THREAD_TRACE_START,
    event_index__me_event_write__other,                                  // 0x34: THREAD_TRACE_STOP,
    event_index__me_event_write__other,                                  // 0x35: THREAD_TRACE_MARKER,
    event_index__me_event_write__other,                                  // 0x36: THREAD_TRACE_FLUSH/DRAW,
    event_index__me_event_write__other,                                  // 0x37: THREAD_TRACE_FINISH,
    event_index__me_event_write__pixel_pipe_stat_control_or_dump,        // 0x38: PIXEL_PIPE_STAT_CONTROL,
    event_index__me_event_write__pixel_pipe_stat_control_or_dump,        // 0x39: PIXEL_PIPE_STAT_DUMP,
    event_index__me_event_write__other,                                  // 0x3a: PIXEL_PIPE_STAT_RESET,
    event_index__me_event_write__other,                                  // 0x3b: CONTEXT_SUSPEND,
    event_index__me_event_write__other,                                  // 0x3c: OFFCHIP_HS_DEALLOC,
    event_index__me_event_write__other,                                  // 0x3d: ENABLE_NGG_PIPELINE,
    event_index__me_event_write__other,                                  // 0x3e: ENABLE_LEGACY_PIPELINE,
    event_index__me_event_write__other,                                  // 0x3f: DRAW_DONE,
};

static constexpr bool VgtEventHasTs[]=
{
    false, // 0x0: Reserved_0x00,
    false, // 0x1: SAMPLE_STREAMOUTSTATS1,
    false, // 0x2: SAMPLE_STREAMOUTSTATS2,
    false, // 0x3: SAMPLE_STREAMOUTSTATS3,
    true,  // 0x4: CACHE_FLUSH_TS,
    false, // 0x5: CONTEXT_DONE,
    false, // 0x6: CACHE_FLUSH,
    false, // 0x7: CS_PARTIAL_FLUSH,
    false, // 0x8: VGT_STREAMOUT_SYNC,
    false, // 0x9: Reserved_0x09,
    false, // 0xa: VGT_STREAMOUT_RESET,
    false, // 0xb: END_OF_PIPE_INCR_DE,
    false, // 0xc: END_OF_PIPE_IB_END,
    false, // 0xd: RST_PIX_CNT,
    false, // 0xe: BREAK_BATCH,
    false, // 0xf: VS_PARTIAL_FLUSH,
    false, // 0x10: PS_PARTIAL_FLUSH,
    false, // 0x11: FLUSH_HS_OUTPUT,
    false, // 0x12: FLUSH_DFSM,
    false, // 0x13: RESET_TO_LOWEST_VGT,
    true,  // 0x14: CACHE_FLUSH_AND_INV_TS_EVENT,
    false, // 0x15: ZPASS_DONE,
    false, // 0x16: CACHE_FLUSH_AND_INV_EVENT,
    false, // 0x17: PERFCOUNTER_START,
    false, // 0x18: PERFCOUNTER_STOP,
    false, // 0x19: PIPELINESTAT_START,
    false, // 0x1a: PIPELINESTAT_STOP,
    false, // 0x1b: PERFCOUNTER_SAMPLE,
    false, // 0x1c: Available_0x1c,
    false, // 0x1d: Available_0x1d,
    false, // 0x1e: SAMPLE_PIPELINESTAT,
    false, // 0x1f: SO_VGTSTREAMOUT_FLUSH,
    false, // 0x20: SAMPLE_STREAMOUTSTATS,
    false, // 0x21: RESET_VTX_CNT,
    false, // 0x22: BLOCK_CONTEXT_DONE,
    false, // 0x23: CS_CONTEXT_DONE,
    false, // 0x24: VGT_FLUSH,
    false, // 0x25: TGID_ROLLOVER,
    false, // 0x26: SQ_NON_EVENT,
    false, // 0x27: SC_SEND_DB_VPZ,
    true,  // 0x28: BOTTOM_OF_PIPE_TS,
    true,  // 0x29: FLUSH_SX_TS,
    false, // 0x2a: DB_CACHE_FLUSH_AND_INV,
    true,  // 0x2b: FLUSH_AND_INV_DB_DATA_TS,
    false, // 0x2c: FLUSH_AND_INV_DB_META,
    true,  // 0x2d: FLUSH_AND_INV_CB_DATA_TS,
    false, // 0x2e: FLUSH_AND_INV_CB_META,
    false, // 0x2f: CS_DONE,
    false, // 0x30: PS_DONE,
    false, // 0x31: FLUSH_AND_INV_CB_PIXEL_DATA,
    false, // 0x32: SX_CB_RAT_ACK_REQUEST,
    false, // 0x33: THREAD_TRACE_START,
    false, // 0x34: THREAD_TRACE_STOP,
    false, // 0x35: THREAD_TRACE_MARKER,
    false, // 0x36: THREAD_TRACE_FLUSH,
    false, // 0x37: THREAD_TRACE_FINISH,
    false, // 0x38: PIXEL_PIPE_STAT_CONTROL,
    false, // 0x39: PIXEL_PIPE_STAT_DUMP,
    false, // 0x3a: PIXEL_PIPE_STAT_RESET,
    false, // 0x3b: CONTEXT_SUSPEND,
    false, // 0x3c: OFFCHIP_HS_DEALLOC,
    false, // 0x3d: ENABLE_NGG_PIPELINE,
    false, // 0x3e: ENABLE_LEGACY_PIPELINE,
    false, // 0x3f: Reserved_0x3f,
};

// =====================================================================================================================
// Returns a 32-bit quantity that corresponds to a type-3 packet header.  "count" is the actual size of the packet in
// terms of DWORDs, including the header.
//
// The shaderType argument doesn't matter (can be left at its default) for all packets except the following:
// - load_sh_reg
// - set_base
// - set_sh_reg
// - set_sh_reg_offset
// - write_gds
static PM4_ME_TYPE_3_HEADER Type3Header(
    IT_OpCodeType  opCode,
    uint32         count,
    bool           resetFilterCam = false,
    Pm4ShaderType  shaderType     = ShaderGraphics,
    Pm4Predicate   predicate      = PredDisable)
{
    PM4_ME_TYPE_3_HEADER header = {};

    header.predicate      = predicate;
    header.shaderType     = shaderType;
    header.type           = 3; // type-3 packet
    header.opcode         = opCode;
    header.count          = (count - 2);
    header.resetFilterCam = resetFilterCam;

    return header;
}

// =====================================================================================================================
// True if the specified register is in context reg space, false otherwise.
static bool IsContextReg(
    uint32 regAddr)
{
    const bool isContextReg = ((regAddr >= CONTEXT_SPACE_START) && (regAddr <= CONTEXT_SPACE_END));
    return isContextReg;
}

// =====================================================================================================================
// True if the specified register is in user-config reg space, false otherwise.
bool CmdUtil::IsUserConfigReg(
    uint32 regAddr)
{
    return (((regAddr >= UCONFIG_SPACE_START) && (regAddr <= UConfigRangeEnd)) ||
            ((regAddr >= UConfigPerfStart)    && (regAddr <= UconfigPerfEnd)));
}

// =====================================================================================================================
// True if the specified register is in persistent data space, false otherwise.
static bool IsShReg(
    uint32 regAddr)
{
    const bool isShReg = ((regAddr >= PERSISTENT_SPACE_START) && (regAddr <= PERSISTENT_SPACE_END));
    return isShReg;
}

// =====================================================================================================================
// Note that this constructor is invoked before settings have been committed.
CmdUtil::CmdUtil(
    const Device& device)
    :
    m_device(device),
    m_chipProps(device.Parent()->ChipProperties())
{
}

// =====================================================================================================================
size_t CmdUtil::BuildContextControl(
    const PM4_PFP_CONTEXT_CONTROL& contextControl,
    void*                          pBuffer)
{
    static_assert(PM4_PFP_CONTEXT_CONTROL_SIZEDW__CORE == PM4_ME_CONTEXT_CONTROL_SIZEDW__CORE,
                  "Context control packet doesn't match between PFP and ME!");

    constexpr uint32 PacketSize = PM4_PFP_CONTEXT_CONTROL_SIZEDW__CORE;
    auto* const      pPacket    = static_cast<PM4_PFP_CONTEXT_CONTROL*>(pBuffer);

    PM4_PFP_CONTEXT_CONTROL packet = contextControl;
    packet.ordinal1.header.u32All = Type3Header(IT_CONTEXT_CONTROL, PacketSize).u32All;

    *pPacket = packet;

    return PacketSize;
}

// =====================================================================================================================
size_t CmdUtil::BuildNop(
    uint32  numDwords,
    void*   pBuffer)
{
    static_assert((PM4_PFP_NOP_SIZEDW__CORE  == PM4_MEC_NOP_SIZEDW__CORE),
                  "graphics and compute versions of the NOP packet don't match!");

    PM4_PFP_NOP* pPacket = static_cast<PM4_PFP_NOP*>(pBuffer);

    if (numDwords == 0)
    {
        // No padding required.
    }
    else if (numDwords == 1)
    {
        // NOP packets with a maxed-out size field (0x3FFF) are one dword long (i.e., header only).  The "Type3Header"
        // function will subtract two from the size field, so add two here.
        pPacket->ordinal1.header.u32All = (Type3Header(IT_NOP, 0x3FFF + 2)).u32All;
    }
    else
    {
        pPacket->ordinal1.header.u32All = (Type3Header(IT_NOP, static_cast<uint32>(numDwords))).u32All;
    }

    return numDwords;
}

// =====================================================================================================================
// Builds an NOP PM4 packet with the payload data embedded inside.
size_t CmdUtil::BuildNopPayload(
    const void* pPayload,
    uint32      payloadSize,
    void*       pBuffer)
{
    static_assert((PM4_PFP_NOP_SIZEDW__CORE  == PM4_MEC_NOP_SIZEDW__CORE),
                  "graphics and compute versions of the NOP packet don't match!");

    const size_t packetSize = NopPayloadSizeDwords(payloadSize);
    auto*const   pPacket    = static_cast<PM4_PFP_NOP*>(pBuffer);
    uint32*      pData      = reinterpret_cast<uint32*>(pPacket + 1);

    // Build header (NOP, signature, size, type)
    pPacket->ordinal1.header.u32All = (Type3Header(IT_NOP, static_cast<uint32>(packetSize))).u32All;

    // Append data
    memcpy(pData, pPayload, payloadSize * sizeof(uint32));

    return packetSize;
}

// =====================================================================================================================
// Builds an NOP PM4 packet with the ASCII string comment embedded inside. The comment is preceeded by a signature
// that analysis tools can use to tell that this is a comment.
size_t CmdUtil::BuildCommentString(
    const char*   pComment,
    Pm4ShaderType type,
    void*         pBuffer)
{
    const size_t stringLength    = strlen(pComment) + 1;
    const size_t payloadSize     = (PM4_PFP_NOP_SIZEDW__CORE * sizeof(uint32)) + stringLength;
    const size_t packetSize      = (Util::RoundUpToMultiple(payloadSize, sizeof(uint32)) / sizeof(uint32)) + 3;
    PM4_PFP_NOP*        pPacket  = static_cast<PM4_PFP_NOP*>(pBuffer);
    CmdBufferPayload*   pData    = reinterpret_cast<CmdBufferPayload*>(pPacket + 1);

    PAL_ASSERT(stringLength < MaxPayloadSize);

    // Build header (NOP, signature, size, type)
    pPacket->ordinal1.header.u32All = (Type3Header(IT_NOP, static_cast<uint32>(packetSize), false, type)).u32All;
    pData->signature                = CmdBufferPayloadSignature;
    pData->payloadSize              = static_cast<uint32>(packetSize);
    pData->type                     = CmdBufferPayloadType::String;

    // Append data
    memcpy(&pData->payload[0], pComment, stringLength);

    return packetSize;
}

// =====================================================================================================================
template <Pm4ShaderType ShaderType, bool ResetFilterCam>
size_t CmdUtil::BuildSetShPairsHeader(
    uint32 numPairsTotal,
    void** ppDataStart, // [out] Pointer to where the register pair data starts.
    void*  pBuffer)
{
    // The packet itself contains at least one pair.
    PAL_ASSERT(numPairsTotal > 0);
    const uint32 packetSize = PM4_PFP_SET_SH_REG_PAIRS_SIZEDW__CORE + ((numPairsTotal - 1) * 2);

    PM4_ME_TYPE_3_HEADER header = Type3Header(IT_SET_SH_REG_PAIRS, packetSize, ResetFilterCam, ShaderType);

    uint32* pPacket = static_cast<uint32*>(pBuffer);
    pPacket[0] = header.u32All;

    *ppDataStart = &pPacket[1];

    return packetSize;
}

template size_t CmdUtil::BuildSetShPairsHeader<ShaderGraphics, true>(
    uint32 numPairsTotal,
    void** ppDataStart,
    void*  pBuffer);
template size_t CmdUtil::BuildSetShPairsHeader<ShaderGraphics, false>(
    uint32 numPairsTotal,
    void** ppDataStart,
    void*  pBuffer);
template size_t CmdUtil::BuildSetShPairsHeader<ShaderCompute, true>(
    uint32 numPairsTotal,
    void** ppDataStart,
    void*  pBuffer);
template size_t CmdUtil::BuildSetShPairsHeader<ShaderCompute, false>(
    uint32 numPairsTotal,
    void** ppDataStart,
    void*  pBuffer);

// =====================================================================================================================
template <Pm4ShaderType ShaderType, bool ResetFilterCam>
size_t CmdUtil::BuildSetShPairs(
    const RegisterValuePair* pPairs,
    uint32                   numPairs,
    void*                    pBuffer)
{
#if _DEBUG
    for (uint32 i = 0; i < numPairs; i++)
    {
        PAL_ASSERT(IsShReg(pPairs[i].offset + PERSISTENT_SPACE_START));
    }
#endif

    // The packet itself contains at least one pair.
    PAL_ASSERT(numPairs > 0);
    void* pPairsStart = nullptr;
    const size_t packetSize = BuildSetShPairsHeader(numPairs, &pPairsStart, pBuffer);

    PAL_ASSERT(pPairsStart != nullptr);
    memcpy(pPairsStart, pPairs, numPairs * sizeof(RegisterValuePair));
    return packetSize;
}

template size_t CmdUtil::BuildSetShPairs<ShaderGraphics, true>(
    const RegisterValuePair* pPairs,
    uint32                   numPairs,
    void*                    pBuffer);
template size_t CmdUtil::BuildSetShPairs<ShaderGraphics, false>(
    const RegisterValuePair* pPairs,
    uint32                   numPairs,
    void*                    pBuffer);
template size_t CmdUtil::BuildSetShPairs<ShaderCompute, true>(
    const RegisterValuePair* pPairs,
    uint32                   numPairs,
    void*                    pBuffer);
template size_t CmdUtil::BuildSetShPairs<ShaderCompute, false>(
    const RegisterValuePair* pPairs,
    uint32                   numPairs,
    void*                    pBuffer);

// =====================================================================================================================
size_t CmdUtil::BuildSetContextPairsHeader(
    uint32 numPairsTotal,
    void** ppDataStart, // [out] Pointer to where the register pair data starts.
    void*  pBuffer)
{
    // The packet itself contains at least one pair.
    PAL_ASSERT(numPairsTotal > 0);
    const uint32 packetSize = SetContextPairsSizeDwords(numPairsTotal);

    PM4_ME_TYPE_3_HEADER header = Type3Header(IT_SET_CONTEXT_REG_PAIRS, packetSize);

    uint32* pPacket = static_cast<uint32*>(pBuffer);
    pPacket[0] = header.u32All;

    *ppDataStart = &pPacket[1];

    return packetSize;
}

// =====================================================================================================================
size_t CmdUtil::BuildSetContextPairs(
    const RegisterValuePair* pPairs,
    uint32                   numPairs,
    void*                    pBuffer)
{
    // The packet itself contains at least one pair.
    PAL_ASSERT(numPairs > 0);

#if _DEBUG
    for (uint32 i = 0; i < numPairs; i++)
    {
        PAL_ASSERT(IsContextReg(pPairs[i].offset + CONTEXT_SPACE_START));
    }
#endif

    void* pPairsStart = nullptr;
    const size_t packetSize = BuildSetContextPairsHeader(numPairs, &pPairsStart, pBuffer);

    PAL_ASSERT(pPairsStart != nullptr);
    memcpy(pPairsStart, pPairs, numPairs * sizeof(RegisterValuePair));

    return packetSize;
}

// =====================================================================================================================
size_t CmdUtil::BuildContextRegRmw(
    uint32  regAddr,
    uint32  regMask,
    uint32  regData,
    void*   pBuffer)
{
    PM4_ME_CONTEXT_REG_RMW* pPacket = static_cast<PM4_ME_CONTEXT_REG_RMW*>(pBuffer);

    constexpr uint32 PacketSize = PM4_ME_CONTEXT_REG_RMW_SIZEDW__CORE;

    pPacket->ordinal1.header               = Type3Header(IT_CONTEXT_REG_RMW, PacketSize);
    pPacket->ordinal2.bitfields.reg_offset = regAddr - CONTEXT_SPACE_START;
    pPacket->ordinal3.reg_mask             = regMask;
    pPacket->ordinal4.reg_data             = regData;

    return PacketSize;
}

// =====================================================================================================================
// Builds a LOAD_CONTEXT_REG_INDEX packet with only direct_addr index and offset_and_size data format usage.
// Fetches up to 8 context-configuration register data. Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildLoadContextRegsIndex(
    gpusize gpuVirtAddr,
    uint32  startRegAddr,
    uint32  count,
    void*   pBuffer)
{
#if _DEBUG
    for (uint32 i = 0; i < count; i++)
    {
        PAL_ASSERT(IsContextReg(startRegAddr + i));
    }
#endif
    PAL_ASSERT(IsPow2Aligned(gpuVirtAddr, 4));
    PAL_ASSERT(count <= 8);

    constexpr uint32 PacketSize = PM4_PFP_LOAD_CONTEXT_REG_INDEX_SIZEDW__CORE;

    PM4_PFP_LOAD_CONTEXT_REG_INDEX packet = {};
    packet.ordinal1.header.u32All = (Type3Header(IT_LOAD_CONTEXT_REG_INDEX, PacketSize, false, ShaderGraphics)).u32All;

    // This version only uses the direct_addr index, which uses the gpuVirtAddr as a read address.
    packet.ordinal2.bitfields.index       = index__pfp_load_context_reg_index__direct_addr;
    packet.ordinal2.bitfields.mem_addr_lo = (LowPart(gpuVirtAddr) >> 2);
    packet.ordinal3.mem_addr_hi           = HighPart(gpuVirtAddr);

    // This version only uses the offset_and_size data format, which reads and writes count DWORDs consecutively
    // from the gpuVirtAddr and startRegAddr respectively.
    packet.ordinal4.bitfields.reg_offset = (startRegAddr - CONTEXT_SPACE_START);
    packet.ordinal4.bitfields.data_format = data_format__pfp_load_context_reg_index__offset_and_size;
    packet.ordinal5.bitfields.num_dwords  = count;

    memcpy(pBuffer, &packet, PacketSize * sizeof(uint32));

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 packet which sets one config register. Returns the size of the PM4 command assembled, in DWORDs.
template <bool ResetFilterCam>
size_t CmdUtil::BuildSetOneUConfigReg(
    uint32 offset,
    uint32 value,
    void*  pBuffer)
{
    PAL_ASSERT(IsUserConfigReg(offset));

    constexpr uint32 PacketSize = SetOneUConfigRegSizeDwords;

    uint32* pPacket = static_cast<uint32*>(pBuffer);
    pPacket[0] = Type3Header(IT_SET_UCONFIG_REG, PacketSize, ResetFilterCam).u32All;
    pPacket[1] = offset - UCONFIG_SPACE_START;
    pPacket[2] = value;

    return PacketSize;
}

template size_t CmdUtil::BuildSetOneUConfigReg<true>(uint32 offset, uint32 value, void* pBuffer);
template size_t CmdUtil::BuildSetOneUConfigReg<false>(uint32 offset, uint32 value, void* pBuffer);

// =====================================================================================================================
// Builds a PM4 packet which sets a sequence of user config registers starting with startRegAddr and ending with
// endRegAddr (inclusive). Returns the size of the PM4 command assembled, in DWORDs.
template <bool ResetFilterCam>
size_t CmdUtil::BuildSetSeqUConfigRegs(
    uint32 startRegAddr,
    uint32 endRegAddr,
    void*  pBuffer)      // [out] Build the PM4 packet in this buffer.
{
    PAL_ASSERT(IsUserConfigReg(startRegAddr) && IsUserConfigReg(endRegAddr) && (endRegAddr >= startRegAddr));

    const uint32 packetSize  = SetSeqUConfigRegsSizeDwords(startRegAddr, endRegAddr);
    auto*const   pPacket     = static_cast<PM4_PFP_SET_UCONFIG_REG*>(pBuffer);

    PM4_PFP_TYPE_3_HEADER header;
    header.u32All            = (Type3Header(IT_SET_UCONFIG_REG, packetSize, ResetFilterCam)).u32All;
    pPacket->ordinal1.header = header;
    pPacket->ordinal2.u32All = startRegAddr - UCONFIG_SPACE_START;

    return packetSize;
}

template size_t CmdUtil::BuildSetSeqUConfigRegs<true>(uint32 startRegAddr, uint32 endRegAddr, void* pBuffer);
template size_t CmdUtil::BuildSetSeqUConfigRegs<false>(uint32 startRegAddr, uint32 endRegAddr, void* pBuffer);

// =====================================================================================================================
size_t CmdUtil::BuildSetUConfigPairsHeader(
    uint32 numPairsTotal,
    void** ppDataStart, // [out] Pointer to where the register pair data starts.
    void*  pBuffer)
{
    // The packet itself contains at least one pair.
    PAL_ASSERT(numPairsTotal > 0);
    const uint32 packetSize = PM4_PFP_SET_UCONFIG_REG_PAIRS_SIZEDW__CORE + ((numPairsTotal - 1) * 2);

    PM4_ME_TYPE_3_HEADER header = Type3Header(IT_SET_UCONFIG_REG_PAIRS, packetSize);

    uint32* pPacket = static_cast<uint32*>(pBuffer);
    pPacket[0] = header.u32All;

    *ppDataStart = &pPacket[1];

    return packetSize;
}

// =====================================================================================================================
size_t CmdUtil::BuildSetUConfigPairs(
    const RegisterValuePair* pPairs,
    uint32                   numPairs,
    void*                    pBuffer)
{
    // The packet itself contains at least one pair.
    PAL_ASSERT(numPairs > 0);

#if _DEBUG
    for (uint32 i = 0; i < numPairs; i++)
    {
        PAL_ASSERT(IsUserConfigReg(pPairs[i].offset + UCONFIG_SPACE_START));
    }
#endif

    void* pPairsStart = nullptr;
    const size_t packetSize = BuildSetUConfigPairsHeader(numPairs, &pPairsStart, pBuffer);

    PAL_ASSERT(pPairsStart != nullptr);
    memcpy(pPairsStart, pPairs, numPairs * sizeof(RegisterValuePair));

    return packetSize;
}

// =====================================================================================================================
size_t CmdUtil::BuildLoadShRegsIndex(
    PFP_LOAD_SH_REG_INDEX_index_enum       index,
    PFP_LOAD_SH_REG_INDEX_data_format_enum dataFormat,
    gpusize                                gpuVirtAddr,
    uint32                                 startRegAddr,
    uint32                                 count,
    Pm4ShaderType                          shaderType,
    void*                                  pBuffer)
{
    // startRegAddr is a register address, not a relative offset.
    PAL_ASSERT(IsShReg(startRegAddr) && IsShReg(startRegAddr + count - 1));

    constexpr uint32 PacketSize = PM4_PFP_LOAD_SH_REG_INDEX_SIZEDW__CORE;

    PM4_PFP_LOAD_SH_REG_INDEX packet = {};

    packet.ordinal1.header.u32All   = (Type3Header(IT_LOAD_SH_REG_INDEX, PacketSize, false, shaderType)).u32All;
    packet.ordinal2.bitfields.index = index;

    if (index == index__pfp_load_sh_reg_index__offset)
    {
        packet.ordinal3.addr_offset = LowPart(gpuVirtAddr);

        // The offset is only 32 bits.
        PAL_ASSERT(HighPart(gpuVirtAddr) == 0);
    }
    else
    {
        packet.ordinal2.bitfields.mem_addr_lo = LowPart(gpuVirtAddr) >> 2;
        packet.ordinal3.mem_addr_hi           = HighPart(gpuVirtAddr);

        // Only the low 16 bits are honored for the high portion of the GPU virtual address!
        PAL_ASSERT((HighPart(gpuVirtAddr) & 0xFFFF0000) == 0);
    }

    packet.ordinal4.bitfields.data_format = dataFormat;

    if (dataFormat == data_format__pfp_load_sh_reg_index__offset_and_size)
    {
        packet.ordinal4.bitfields.reg_offset = startRegAddr - PERSISTENT_SPACE_START;
    }

    packet.ordinal5.bitfields.num_dwords = count;

    *static_cast<PM4_PFP_LOAD_SH_REG_INDEX*>(pBuffer) = packet;

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 packet which issues a "num instances" command into the given DE command stream. Returns the Size of the
// PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildNumInstances(
    uint32 instanceCount,
    void*  pBuffer)       // [out] Build the PM4 packet in this buffer.
{
    constexpr uint32 PacketSize = PM4_PFP_NUM_INSTANCES_SIZEDW__CORE;
    auto*const       pPacket    = static_cast<PM4_PFP_NUM_INSTANCES*>(pBuffer);

    PM4_ME_TYPE_3_HEADER header = Type3Header(IT_NUM_INSTANCES, PacketSize);

    pPacket->ordinal1.header.u32All = header.u32All;
    pPacket->ordinal2.num_instances = instanceCount;

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 packet which issues a "index base" command into the given DE command stream. Return the size of the PM4
// command assembled, in DWORDs.
size_t CmdUtil::BuildIndexBase(
    gpusize baseAddr, // Base address of index buffer (w/ offset).
    void*   pBuffer)  // [out] Build the PM4 packet in this buffer.
{
    // Address must be 2 byte aligned
    PAL_ASSERT(IsPow2Aligned(baseAddr, 2));

    constexpr uint32 PacketSize = PM4_PFP_INDEX_BASE_SIZEDW__CORE;
    auto*const       pPacket    = static_cast<PM4_PFP_INDEX_BASE*>(pBuffer);

    pPacket->ordinal1.header.u32All = (Type3Header(IT_INDEX_BASE, PacketSize)).u32All;
    pPacket->ordinal2.u32All        = LowPart(baseAddr);
    PAL_ASSERT(pPacket->ordinal2.bitfields.reserved1 == 0);
    pPacket->ordinal3.index_base_hi = HighPart(baseAddr);

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 packet which issues a "index buffer size" command into the given DE command stream. Returns the size of
// the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildIndexBufferSize(
    uint32 indexCount,
    void*  pBuffer)     // [out] Build the PM4 packet in this buffer.
{
    constexpr uint32 PacketSize = PM4_PFP_INDEX_BUFFER_SIZE_SIZEDW__CORE;
    auto*const       pPacket    = static_cast<PM4_PFP_INDEX_BUFFER_SIZE*>(pBuffer);

    pPacket->ordinal1.header.u32All     = (Type3Header(IT_INDEX_BUFFER_SIZE, PacketSize)).u32All;
    pPacket->ordinal2.index_buffer_size = indexCount;

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 packet which issues a "index type" command into the given DE command stream. Returns the size of the PM4
// command assembled, in DWORDs.
size_t CmdUtil::BuildIndexType(
    uint32   vgtDmaIndexType, // value associated with the VGT_DMA_INDEX_TYPE register
    void*    pBuffer)         // [out] Build the PM4 packet in this buffer.
{
    const size_t PacketSize     = IndexTypeSizeDwords;

    PM4_PFP_SET_UCONFIG_REG_INDEX setReg{};
    setReg.ordinal1.header.u32All        = Type3Header(IT_SET_UCONFIG_REG_INDEX, PacketSize).u32All;
    setReg.ordinal2.bitfields.reg_offset = mmVGT_INDEX_TYPE - UCONFIG_SPACE_START;
    setReg.ordinal2.bitfields.index      = index__pfp_set_uconfig_reg_index__index_type;

    uint32* pPacket         = static_cast<uint32*>(pBuffer);
    pPacket[0] = setReg.ordinal1.u32All;
    pPacket[1] = setReg.ordinal2.u32All;
    pPacket[2] = vgtDmaIndexType;

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 packet which issues a non-indexed draw. Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildDrawIndexAuto(
    uint32       indexCount,
    bool         useOpaque,
    Pm4Predicate predicate,
    void*        pBuffer)    // [out] Build the PM4 packet in this buffer.
{
    PAL_ASSERT((indexCount == 0) || (useOpaque == false));

    PM4_PFP_DRAW_INDEX_AUTO packet     = {};
    constexpr uint32        PacketSize = PM4_PFP_DRAW_INDEX_AUTO_SIZEDW__CORE;

    packet.ordinal1.header.u32All =
        Type3Header(IT_DRAW_INDEX_AUTO, PacketSize, false, ShaderGraphics, predicate).u32All;
    packet.ordinal2.index_count   = indexCount;

    VGT_DRAW_INITIATOR drawInitiator = {};
    drawInitiator.bits.SOURCE_SELECT = DI_SRC_SEL_AUTO_INDEX;
    drawInitiator.bits.USE_OPAQUE    = useOpaque;
    packet.ordinal3.draw_initiator   = drawInitiator.u32All;

    *static_cast<PM4_PFP_DRAW_INDEX_AUTO*>(pBuffer) = packet;

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 packet which issues an indexed draw.. Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildDrawIndex2(
    uint32       indexCount,
    uint32       indexBufSize,
    gpusize      indexBufAddr,
    Pm4Predicate predicate,
    void*        pBuffer)
{
    PM4_PFP_DRAW_INDEX_2 packet     = {};
    constexpr uint32     PacketSize = PM4_PFP_DRAW_INDEX_2_SIZEDW__CORE;

    packet.ordinal1.header.u32All =
        Type3Header(IT_DRAW_INDEX_2, PacketSize, false, ShaderGraphics, predicate).u32All;
    packet.ordinal2.max_size      = indexBufSize;
    packet.ordinal3.index_base_lo = LowPart(indexBufAddr);
    packet.ordinal4.index_base_hi = HighPart(indexBufAddr);
    packet.ordinal5.index_count   = indexCount;

    VGT_DRAW_INITIATOR drawInitiator = {};
    drawInitiator.bits.SOURCE_SELECT = DI_SRC_SEL_DMA;
    packet.ordinal6.draw_initiator   = drawInitiator.u32All;

    *static_cast<PM4_PFP_DRAW_INDEX_2*>(pBuffer) = packet;

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 packet which issues a draw indirect multi command into the given DE command stream. Returns the size of
// the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildDrawIndirectMulti(
    gpusize      offset,          // Byte offset to the indirect args data.
    uint16       baseVtxLoc,      // Register VS expects to read baseVtxLoc from.
    uint16       startInstLoc,    // Register VS expects to read startInstLoc from.
    uint16       drawIndexLoc,    // Register VS expects to read drawIndex from.
    uint32       stride,          // Stride from one indirect args data structure to the next.
    uint32       count,           // Number of draw calls to loop through, or max draw calls if count is in GPU memory.
    gpusize      countGpuAddr,    // GPU address containing the count.
    Pm4Predicate predicate,       // Predication enable control.
    bool         issueSqttMarker, // If SQTT Marker bit on the PM4 needs to be set.
    void*        pBuffer)         // [out] Build the PM4 packet in this buffer.
{
    // Draw argument offset in the buffer has to be 4-byte aligned.
    PAL_ASSERT(IsPow2Aligned(offset, 4));

    PM4_PFP_DRAW_INDIRECT_MULTI packet     = {};
    constexpr uint32            PacketSize = PM4_PFP_DRAW_INDIRECT_MULTI_SIZEDW__CORE;

    packet.ordinal1.header.u32All = Type3Header(
        IT_DRAW_INDIRECT_MULTI, PacketSize, false, ShaderGraphics, predicate).u32All;
    packet.ordinal2.data_offset              = LowPart(offset);
    packet.ordinal3.bitfields.start_vtx_loc  = baseVtxLoc;
    packet.ordinal4.bitfields.start_inst_loc = startInstLoc;

    if (drawIndexLoc != UserDataNotMapped)
    {
        packet.ordinal5.bitfields.draw_index_enable = 1;
        packet.ordinal5.bitfields.draw_index_loc    = drawIndexLoc;
    }
#if (PAL_BUILD_BRANCH >= 2410)
    if (issueSqttMarker)
    {
        packet.ordinal5.bitfields.thread_trace_marker_enable = 1;
    }
#endif
    packet.ordinal5.bitfields.count_indirect_enable = (countGpuAddr != 0);

    packet.ordinal6.count         = count;
    packet.ordinal7.u32All        = LowPart(countGpuAddr);
    packet.ordinal8.count_addr_hi = HighPart(countGpuAddr);
    packet.ordinal9.stride        = stride;

    VGT_DRAW_INITIATOR drawInitiator = {};
    drawInitiator.bits.SOURCE_SELECT = DI_SRC_SEL_AUTO_INDEX;
    packet.ordinal10.u32All          = drawInitiator.u32All;

    *static_cast<PM4_PFP_DRAW_INDIRECT_MULTI*>(pBuffer) = packet;

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 packet which issues a indirect draw command into the given DE command stream. Returns the
// size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildDrawIndirect(
    gpusize      offset,        // Byte offset to the indirect args data.
    uint32       baseVtxLoc,    // Register VS expects to read baseVtxLoc from.
    uint32       startInstLoc,  // Register VS expects to read startInstLoc from.
    Pm4Predicate predicate,
    void*        pBuffer)       // [out] Build the PM4 packet in this buffer.
{
    // Draw argument offset in the buffer has to be 4-byte aligned.
    PAL_ASSERT(IsPow2Aligned(offset, 4));

    constexpr size_t PacketSize = PM4_PFP_DRAW_INDIRECT_SIZEDW__CORE;

    PM4_PFP_DRAW_INDIRECT packet             = {};
    packet.ordinal1.header.u32All            =
        (Type3Header(IT_DRAW_INDIRECT, PacketSize, false, ShaderGraphics, predicate)).u32All;
    packet.ordinal2.data_offset = LowPart(offset);
    packet.ordinal3.bitfields.start_vtx_loc  = baseVtxLoc;

    packet.ordinal4.bitfields.start_inst_loc = startInstLoc;

    VGT_DRAW_INITIATOR drawInitiator = {};
    drawInitiator.bits.SOURCE_SELECT = DI_SRC_SEL_AUTO_INDEX;
    packet.ordinal5.u32All           = drawInitiator.u32All;

    *static_cast<PM4_PFP_DRAW_INDIRECT*>(pBuffer) = packet;

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 packet which issues a multi indexed, indirect draw command into the given DE command stream. Returns the
// size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildDrawIndexIndirect(
    gpusize      offset,       // Byte offset to the indirect args data.
    uint32       baseVtxLoc,   // Register VS expects to read baseVtxLoc from.
    uint32       startInstLoc, // Register VS expects to read startInstLoc from.
    Pm4Predicate predicate,
    void*        pBuffer)      // [out] Build the PM4 packet in this buffer.
{
    // Draw argument offset in the buffer has to be 4-byte aligned.
    PAL_ASSERT(IsPow2Aligned(offset, 4));

    constexpr uint32 PacketSize = PM4_PFP_DRAW_INDEX_INDIRECT_SIZEDW__CORE;

    PM4_PFP_DRAW_INDEX_INDIRECT packet       = {};
    packet.ordinal1.header.u32All            =
        (Type3Header(IT_DRAW_INDEX_INDIRECT, PacketSize, false, ShaderGraphics, predicate)).u32All;
    packet.ordinal2.data_offset              = LowPart(offset);
    packet.ordinal3.bitfields.base_vtx_loc   = baseVtxLoc;
    packet.ordinal4.bitfields.start_inst_loc = startInstLoc;

    VGT_DRAW_INITIATOR drawInitiator = {};
    drawInitiator.bits.SOURCE_SELECT = DI_SRC_SEL_DMA;
    packet.ordinal5.u32All           = drawInitiator.u32All;

    *static_cast<PM4_PFP_DRAW_INDEX_INDIRECT*>(pBuffer) = packet;

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 packet which issues an indexed, indirect draw command into the given DE command stream. Returns the size
// of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildDrawIndexIndirectMulti(
    gpusize      offset,          // Byte offset to the indirect args data.
    uint16       baseVtxLoc,      // Register VS expects to read baseVtxLoc from.
    uint16       startInstLoc,    // Register VS expects to read startInstLoc from.
    uint16       drawIndexLoc,    // Register VS expects to read drawIndex from.
    uint32       stride,          // Stride from one indirect args data structure to the next.
    uint32       count,           // Number of draw calls to loop through, or max draw calls if count is in GPU memory.
    gpusize      countGpuAddr,    // GPU address containing the count.
    Pm4Predicate predicate,       // Predication enable control.
    bool         issueSqttMarker, // If SQTT Marker bit on the PM4 needs to be set.
    void*        pBuffer)         // [out] Build the PM4 packet in this buffer.
{
    // Draw argument offset in the buffer has to be 4-byte aligned.
    PAL_ASSERT(IsPow2Aligned(offset, 4));

    PM4_PFP_DRAW_INDEX_INDIRECT_MULTI packet     = {};
    constexpr uint32                  PacketSize = PM4_PFP_DRAW_INDEX_INDIRECT_MULTI_SIZEDW__CORE;
    packet.ordinal1.header.u32All =
        Type3Header(IT_DRAW_INDEX_INDIRECT_MULTI, PacketSize, false, ShaderGraphics, predicate).u32All;
    packet.ordinal2.data_offset              = LowPart(offset);
    packet.ordinal3.bitfields.base_vtx_loc   = baseVtxLoc;
    packet.ordinal4.bitfields.start_inst_loc = startInstLoc;

    if (drawIndexLoc != UserDataNotMapped)
    {
        packet.ordinal5.bitfields.draw_index_enable = 1;
        packet.ordinal5.bitfields.draw_index_loc    = drawIndexLoc;
    }
#if (PAL_BUILD_BRANCH >= 2410)
    if (issueSqttMarker)
    {
        packet.ordinal5.bitfields.thread_trace_marker_enable = 1;
    }
#endif
    packet.ordinal5.bitfields.count_indirect_enable = (countGpuAddr != 0);

    packet.ordinal6.count         = count;
    packet.ordinal7.u32All        = LowPart(countGpuAddr);
    packet.ordinal8.count_addr_hi = HighPart(countGpuAddr);
    packet.ordinal9.stride        = stride;

    VGT_DRAW_INITIATOR drawInitiator = {};
    drawInitiator.bits.SOURCE_SELECT = DI_SRC_SEL_DMA;
    packet.ordinal10.u32All          = drawInitiator.u32All;

    *static_cast<PM4_PFP_DRAW_INDEX_INDIRECT_MULTI*>(pBuffer) = packet;

    return PacketSize;
}

// =====================================================================================================================
// Builds a DISPATCH_DIRECT packet. Returns the size of the PM4 command assembled, in DWORDs.
template <bool DimInThreads, bool ForceStartAt000>
size_t CmdUtil::BuildDispatchDirect(
    DispatchDims size,                   // Thread groups (or threads) to launch.
    Pm4Predicate predicate,              // Predication enable control. Must be PredDisable on the Compute Engine.
    bool         isWave32,               // Set if wave-size is 32 for bound compute shader
    bool         useTunneling,           // Set if dispatch tunneling should be used (VR)
    bool         disablePartialPreempt,  // Avoid preemption at thread group level without CWSR.
    bool         pingPongEn,             // Enable ping pong (reverse workgroup walk order)
    bool         is2dDispatchInterleave, // Is 2D Dispatch interleave?
    void*        pBuffer)                // [out] Build the PM4 packet in this buffer.
{
    static_assert((PM4_PFP_DISPATCH_DIRECT_SIZEDW__CORE == PM4_MEC_DISPATCH_DIRECT_SIZEDW__CORE) &&
                  (PM4_PFP_DISPATCH_DIRECT_SIZEDW__CORE == PM4_PFP_DISPATCH_DIRECT_INTERLEAVED_SIZEDW__CORE),
                  "DISPATCH_DIRECT packet definition has been updated, fix this!");

    PM4_MEC_DISPATCH_DIRECT packet     = {};
    constexpr uint32        PacketSize = PM4_MEC_DISPATCH_DIRECT_SIZEDW__CORE;
    const IT_OpCodeType     opCode     = is2dDispatchInterleave ? IT_DISPATCH_DIRECT_INTERLEAVED : IT_DISPATCH_DIRECT;

    packet.ordinal1.header.u32All   = Type3Header(opCode, PacketSize, false, ShaderCompute, predicate).u32All;
    packet.ordinal2.dim_x           = size.x;
    packet.ordinal3.dim_y           = size.y;
    packet.ordinal4.dim_z           = size.z;

    auto* pDispatchInitiator = reinterpret_cast<regCOMPUTE_DISPATCH_INITIATOR*>(&(packet.ordinal5.dispatch_initiator));

    pDispatchInitiator->bits.COMPUTE_SHADER_EN      = 1;
    pDispatchInitiator->bits.FORCE_START_AT_000     = ForceStartAt000;
    pDispatchInitiator->bits.USE_THREAD_DIMENSIONS  = DimInThreads;
    pDispatchInitiator->bits.CS_W32_EN              = isWave32;
    pDispatchInitiator->bits.TUNNEL_ENABLE          = useTunneling;
    // This flag in COMPUTE_DISPATCH_INITIATOR tells the CP to not preempt mid-dispatch when CWSR is disabled.
    pDispatchInitiator->bits.DISABLE_DISP_PREMPT_EN = disablePartialPreempt;
    // Set unordered mode to allow waves launch faster. This bit is related to the QoS (Quality of service) feature and
    // should be safe to set by default as the feature gets enabled only when allowed by the KMD. This bit also only
    // applies to asynchronous compute pipe and the graphics pipe simply ignores it.
    pDispatchInitiator->bits.ORDER_MODE             = 1;

    if (DimInThreads == false)
    {
        pDispatchInitiator->bits.INTERLEAVE_2D_EN = is2dDispatchInterleave;
        pDispatchInitiator->bits.PING_PONG_EN     = pingPongEn;
    }

    // INTERLEAVE_2D_EN requires that USE_THREAD_DIMENSIONS=0, PARTIAL_TG_EN=0 and ORDERED_APPEND_ENBL = 0
    PAL_ASSERT((pDispatchInitiator->bits.INTERLEAVE_2D_EN == 0) ||
               ((pDispatchInitiator->bits.USE_THREAD_DIMENSIONS == 0) &&
                (pDispatchInitiator->bits.PARTIAL_TG_EN         == 0) &&
                (pDispatchInitiator->bits.ORDERED_APPEND_ENBL   == 0)));

    // PING_PONG_EN is not compatible with PARTIAL_TG_EN or USE_THREAD_DIMENSIONS!
    PAL_ASSERT((pDispatchInitiator->bits.PING_PONG_EN == 0) ||
               ((pDispatchInitiator->bits.USE_THREAD_DIMENSIONS == 0) &&
                (pDispatchInitiator->bits.PARTIAL_TG_EN         == 0)));

    *static_cast<PM4_MEC_DISPATCH_DIRECT*>(pBuffer) = packet;

    return PacketSize;
}

template size_t CmdUtil::BuildDispatchDirect<true, true>(
    DispatchDims, Pm4Predicate , bool, bool, bool, bool, bool, void*);
template size_t CmdUtil::BuildDispatchDirect<false, false>(
    DispatchDims, Pm4Predicate , bool, bool, bool, bool, bool, void*);
template size_t CmdUtil::BuildDispatchDirect<false, true>(
    DispatchDims, Pm4Predicate , bool, bool, bool, bool, bool, void*);

// =====================================================================================================================
// Builds a DISPATCH_INDIRECT packet for the GFX engine. Returns the size of the PM4 command assembled, in DWORDs.
// This packet has different sizes between ME compute and ME gfx.
size_t CmdUtil::BuildDispatchIndirectGfx(
    gpusize      offset,                 // Offset from the address specified by the set-base packet where the compute params are
    Pm4Predicate predicate,              // Predication enable control
    bool         isWave32,               // Set if wave-size is 32 for bound compute shader
    bool         pingPongEn,             // Enable ping pong (reverse workgroup walk order)
    bool         is2dDispatchInterleave, // Is 2D Dispatch interleave?
    void*        pBuffer)                // [out] Build the PM4 packet in this buffer.
{
    static_assert(PM4_PFP_DISPATCH_INDIRECT_SIZEDW__CORE == PM4_PFP_DISPATCH_INDIRECT_INTERLEAVED_SIZEDW__CORE,
                  "DISPATCH_INDIRECT packet definition has been updated, fix this!");

    // We accept a 64-bit offset but the packet can only handle a 32-bit offset.
    PAL_ASSERT(HighPart(offset) == 0);

    PM4_PFP_DISPATCH_INDIRECT packet     = {};
    constexpr uint32          PacketSize = PM4_PFP_DISPATCH_INDIRECT_SIZEDW__CORE;
    const IT_OpCodeType       opCode     = is2dDispatchInterleave ? IT_DISPATCH_INDIRECT_INTERLEAVED :
                                                                    IT_DISPATCH_INDIRECT;

    packet.ordinal1.header.u32All = Type3Header(opCode, PacketSize, false, ShaderCompute, predicate).u32All;
    packet.ordinal2.data_offset   = LowPart(offset);

    auto* pDispatchInitiator = reinterpret_cast<regCOMPUTE_DISPATCH_INITIATOR*>(&(packet.ordinal3.dispatch_initiator));

    pDispatchInitiator->bits.COMPUTE_SHADER_EN  = 1;
    pDispatchInitiator->bits.FORCE_START_AT_000 = 1;
    pDispatchInitiator->bits.CS_W32_EN          = isWave32;
    pDispatchInitiator->bits.PING_PONG_EN       = pingPongEn;
    pDispatchInitiator->bits.INTERLEAVE_2D_EN   = is2dDispatchInterleave;

    // INTERLEAVE_2D_EN requires that USE_THREAD_DIMENSIONS=0, PARTIAL_TG_EN=0 and ORDERED_APPEND_ENBL = 0
    PAL_ASSERT((pDispatchInitiator->bits.INTERLEAVE_2D_EN == 0) ||
               ((pDispatchInitiator->bits.USE_THREAD_DIMENSIONS == 0) &&
                (pDispatchInitiator->bits.PARTIAL_TG_EN         == 0) &&
                (pDispatchInitiator->bits.ORDERED_APPEND_ENBL   == 0)));

    // PING_PONG_EN is not compatible with PARTIAL_TG_EN or USE_THREAD_DIMENSIONS!
    PAL_ASSERT((pDispatchInitiator->bits.PING_PONG_EN == 0) ||
               ((pDispatchInitiator->bits.USE_THREAD_DIMENSIONS == 0) &&
                (pDispatchInitiator->bits.PARTIAL_TG_EN         == 0)));

    *static_cast<PM4_PFP_DISPATCH_INDIRECT*>(pBuffer) = packet;

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4_ME_DISPATCH_MESH_DIRECT packet for the PFP & ME engines.
size_t CmdUtil::BuildDispatchMeshDirect(
    DispatchDims size,
    Pm4Predicate predicate,
    void*        pBuffer)
{
    PM4_PFP_DISPATCH_MESH_DIRECT packet     = {};
    constexpr uint32             PacketSize = PM4_PFP_DISPATCH_MESH_DIRECT_SIZEDW__CORE;

    packet.ordinal1.header.u32All =
        Type3Header(IT_DISPATCH_MESH_DIRECT, PacketSize, false, ShaderGraphics, predicate).u32All;
    packet.ordinal2.dim_x = size.x;
    packet.ordinal3.dim_y = size.y;
    packet.ordinal4.dim_z = size.z;

    VGT_DRAW_INITIATOR drawInitiator = {};
    drawInitiator.bits.SOURCE_SELECT = DI_SRC_SEL_AUTO_INDEX;
    packet.ordinal5.draw_initiator   = drawInitiator.u32All;

    *static_cast<PM4_PFP_DISPATCH_MESH_DIRECT*>(pBuffer) = packet;

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4_ME_DISPATCH_MESH_INDIRECT_MULTI packet for the PFP & ME engines.
size_t CmdUtil::BuildDispatchMeshIndirectMulti(
    gpusize      dataOffset,             // Byte offset of the indirect buffer.
    uint16       xyzDimLoc,              // First of three consecutive user-SGPRs specifying the dimension.
    uint16       drawIndexLoc,           // Draw index user-SGPR offset.
    uint32       count,                  // Number of draw calls to loop through, or max draw calls if count is in GPU
                                         // memory.
    uint32       stride,                 // Stride from one indirect args data structure to the next.
    gpusize      countGpuAddr,           // GPU address containing the count.
    Pm4Predicate predicate,              // Predication enable control.
    bool         issueSqttMarker,        // If SQTT Marker bit on the PM4 needs to be set.
    void*        pBuffer)                // [out] Build the PM4 packet in this buffer.
{
    // Draw argument offset in the buffer has to be 4-byte aligned.
    PAL_ASSERT(IsPow2Aligned(dataOffset, 4));
    // The count address must be Dword aligned.
    PAL_ASSERT(IsPow2Aligned(countGpuAddr, 4));

    PM4_PFP_DISPATCH_MESH_INDIRECT_MULTI packet     = {};
    constexpr uint32                     PacketSize = PM4_PFP_DISPATCH_MESH_INDIRECT_MULTI_SIZEDW__CORE;

    packet.ordinal1.header.u32All =
        Type3Header(IT_DISPATCH_MESH_INDIRECT_MULTI, PacketSize, true, ShaderGraphics, predicate).u32All;

    packet.ordinal2.data_offset = LowPart(dataOffset);

    if (drawIndexLoc != UserDataNotMapped)
    {
        packet.ordinal4.bitfields.draw_index_enable = 1;
        packet.ordinal3.bitfields.draw_index_loc    = drawIndexLoc;
    }

    if (xyzDimLoc != UserDataNotMapped)
    {
        packet.ordinal4.bitfields.xyz_dim_enable = 1;
        packet.ordinal3.bitfields.xyz_dim_loc    = xyzDimLoc;
    }

    if (countGpuAddr != 0)
    {
        packet.ordinal4.bitfields.count_indirect_enable = 1;
        packet.ordinal6.u32All                          = LowPart(countGpuAddr);
        PAL_ASSERT(packet.ordinal6.bitfields.reserved1 == 0);
        packet.ordinal7.count_addr_hi                   = HighPart(countGpuAddr);
    }

#if (PAL_BUILD_BRANCH >= 2410)
    if (issueSqttMarker)
    {
        packet.ordinal4.bitfields.thread_trace_marker_enable = 1;
    }
#endif

    packet.ordinal5.count  = count;
    packet.ordinal8.stride = stride;

    VGT_DRAW_INITIATOR drawInitiator = {};
    drawInitiator.bits.SOURCE_SELECT = DI_SRC_SEL_AUTO_INDEX;
    packet.ordinal9.draw_initiator   = drawInitiator.u32All;

    *static_cast<PM4_PFP_DISPATCH_MESH_INDIRECT_MULTI*>(pBuffer) = packet;

    return PacketSize;
}

// =====================================================================================================================
// Builds a DISPATCH_TASKMESH_GFX packet for ME & PFP engines, which consumes data produced by the CS shader and CS
// dispatches that are launched by DISPATCH_TASKMESH_DIRECT_ACE or DISPATCH_TASKMESH_INDIRECT_MULTI_ACE packets by ACE.
// The ME issues multiple sub-draws with the data fetched.
size_t CmdUtil::BuildDispatchTaskMeshGfx(
    uint16       xyzDimLoc,              // First of three consecutive user-SGPRs specifying the dimension.
    uint16       ringEntryLoc,           // User-SGPR offset for the ring entry value received for the draw.
    Pm4Predicate predicate,              // Predication enable control.
    bool         issueSqttMarker,        // If SQTT Marker bit on the PM4 needs to be set.
    bool         linearDispatch,         // Use linear dispatch.
    void*        pBuffer)                // [out] Build the PM4 packet in this buffer.
{
    PAL_ASSERT(ringEntryLoc != UserDataNotMapped);

    PM4_PFP_DISPATCH_TASKMESH_GFX packet = {};
    constexpr uint32              PacketSize = PM4_PFP_DISPATCH_TASKMESH_GFX_SIZEDW__CORE;

    packet.ordinal1.header.u32All =
        Type3Header(IT_DISPATCH_TASKMESH_GFX, PacketSize, true, ShaderGraphics, predicate).u32All;

    packet.ordinal2.bitfields.ring_entry_loc = ringEntryLoc;
    if (xyzDimLoc != UserDataNotMapped)
    {
        packet.ordinal3.bitfields.xyz_dim_enable = 1;
        packet.ordinal2.bitfields.xyz_dim_loc    = xyzDimLoc;
    }

#if (PAL_BUILD_BRANCH >= 2410)
    if (issueSqttMarker)
    {
        packet.ordinal3.bitfields.thread_trace_marker_enable = 1;
    }
#endif

    packet.ordinal3.bitfields.linear_dispatch_enable = linearDispatch ? 1 : 0;

    VGT_DRAW_INITIATOR drawInitiator = {};
    drawInitiator.bits.SOURCE_SELECT = DI_SRC_SEL_AUTO_INDEX;
    packet.ordinal4.draw_initiator   = drawInitiator.u32All;

    *static_cast<PM4_PFP_DISPATCH_TASKMESH_GFX*>(pBuffer) = packet;

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4_MEC_DISPATCH_TASKMESH_DIRECT_ACE packet for the compute engine, which directly starts the task/mesh
// workload.
size_t CmdUtil::BuildDispatchTaskMeshDirectMec(
    DispatchDims size,          // Thread groups (or threads) to launch.
    uint16       ringEntryLoc,  // User data offset where CP writes the payload WPTR.
    Pm4Predicate predicate,     // Predication enable control. Must be PredDisable on the Compute Engine.
    bool         isWave32,      // If wave-size is 32 for bound compute shader
    void*        pBuffer)       // [out] Build the PM4 packet in this buffer.
{
    PM4_MEC_DISPATCH_TASKMESH_DIRECT_ACE packet     = {};
    constexpr uint32                     PacketSize = PM4_MEC_DISPATCH_TASKMESH_DIRECT_ACE_SIZEDW__CORE;

    packet.ordinal1.header.u32All =
        Type3Header(IT_DISPATCH_TASKMESH_DIRECT_ACE, PacketSize, false, ShaderCompute, predicate).u32All;

    packet.ordinal2.x_dim = size.x;
    packet.ordinal3.y_dim = size.y;
    packet.ordinal4.z_dim = size.z;

    regCOMPUTE_DISPATCH_INITIATOR dispatchInitiator = {};
    dispatchInitiator.bitfields.COMPUTE_SHADER_EN      = 1;
    dispatchInitiator.bitfields.ORDER_MODE             = 1;
    dispatchInitiator.bitfields.CS_W32_EN              = isWave32 ? 1 : 0;
    dispatchInitiator.bitfields.AMP_SHADER_EN          = 1;
    dispatchInitiator.bitfields.DISABLE_DISP_PREMPT_EN = 1;
    packet.ordinal5.dispatch_initiator = dispatchInitiator.u32All;

    packet.ordinal6.bitfields.ring_entry_loc = ringEntryLoc;

    *static_cast<PM4_MEC_DISPATCH_TASKMESH_DIRECT_ACE*>(pBuffer) = packet;

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4_MEC_DISPATCH_TASKMESH_INDIRECT_MULTI_ACE packet for the compute engine.
size_t CmdUtil::BuildDispatchTaskMeshIndirectMultiMec(
    gpusize      dataOffset,       // Byte offset of the indirect buffer.
    uint16       ringEntryLoc,     // Offset of user-SGPR where the CP writes the ring entry WPTR.
    uint16       xyzDimLoc,        // First of three consecutive user-SGPR for the compute dispatch dimensions.
    uint16       dispatchIndexLoc, // User-SGPR offset where the dispatch index is written.
    uint32       count,            // Number of draw calls to loop through, or max draw calls if count is in GPU memory.
    uint32       stride,           // Stride from one indirect args data structure to the next.
    gpusize      countGpuAddr,     // GPU address containing the count.
    bool         isWave32,         // If wave-size is 32 for bound compute shader.
    Pm4Predicate predicate,        // Predication enable control.
    bool         issueSqttMarker,  // If SQTT Marker bit on the PM4 needs to be set.
    void*        pBuffer)          // [out] Build the PM4 packet in this buffer.
{
    // Draw argument offset in the buffer has to be 4-byte aligned.
    PAL_ASSERT(IsPow2Aligned(dataOffset, 4));
    // The count address must be Dword aligned.
    PAL_ASSERT(IsPow2Aligned(countGpuAddr, 4));

    PM4_MEC_DISPATCH_TASKMESH_INDIRECT_MULTI_ACE packet = {};
    constexpr uint32                             PacketSize = PM4_MEC_DISPATCH_TASKMESH_INDIRECT_MULTI_ACE_SIZEDW__CORE;

    packet.ordinal1.header.u32All =
        Type3Header(IT_DISPATCH_TASKMESH_INDIRECT_MULTI_ACE, PacketSize, false, ShaderCompute, predicate).u32All;

    packet.ordinal2.bitfields.data_addr_lo = LowPart(dataOffset) >> 2;
    packet.ordinal3.data_addr_hi           = HighPart(dataOffset);

    packet.ordinal4.bitfields.ring_entry_loc = ringEntryLoc;

    if (dispatchIndexLoc != UserDataNotMapped)
    {
        packet.ordinal5.bitfields.draw_index_enable  = 1;
        packet.ordinal5.bitfields.dispatch_index_loc = dispatchIndexLoc;
    }

    if (xyzDimLoc != UserDataNotMapped)
    {
        packet.ordinal5.bitfields.compute_xyz_dim_enable = 1;
        packet.ordinal6.bitfields.compute_xyz_dim_loc    = xyzDimLoc;
    }

    if (countGpuAddr != 0)
    {
        packet.ordinal5.bitfields.count_indirect_enable = 1;
        packet.ordinal8.bitfields.count_addr_lo         = LowPart(countGpuAddr) >> 2;
        packet.ordinal9.count_addr_hi                   = HighPart(countGpuAddr);
    }

#if (PAL_BUILD_BRANCH >= 2410)
    if (issueSqttMarker)
    {
        packet.ordinal5.bitfields.thread_trace_marker_enable = 1;
    }
#endif

    packet.ordinal7.count   = count;
    packet.ordinal10.stride = stride;

    regCOMPUTE_DISPATCH_INITIATOR dispatchInitiator = {};
    dispatchInitiator.bitfields.COMPUTE_SHADER_EN      = 1;
    dispatchInitiator.bitfields.ORDER_MODE             = 1;
    dispatchInitiator.bitfields.CS_W32_EN              = isWave32 ? 1 : 0;
    dispatchInitiator.bitfields.AMP_SHADER_EN          = 1;
    dispatchInitiator.bitfields.DISABLE_DISP_PREMPT_EN = 1;
    packet.ordinal11.dispatch_initiator = dispatchInitiator.u32All;

    *static_cast<PM4_MEC_DISPATCH_TASKMESH_INDIRECT_MULTI_ACE*>(pBuffer) = packet;

    return PacketSize;
}

// =====================================================================================================================
// Builds a DISPATCH_INDIRECT packet for the MEC. Returns the size of the PM4 command assembled, in DWORDs.
// This packet has different sizes between ME compute and ME gfx.
size_t CmdUtil::BuildDispatchIndirectMec(
    gpusize       address,               // Address of the indirect args data.
    bool          isWave32,              // Set if wave-size is 32 for bound compute shader
    bool          useTunneling,          // Set if dispatch tunneling should be used (VR)
    bool          disablePartialPreempt, // Avoid preemption at thread group level without CWSR.
    void*         pBuffer)               // [out] Build the PM4 packet in this buffer.
{
    // Address must be 32-bit aligned
    PAL_ASSERT ((address & 0x3) == 0);

    constexpr uint32 PacketSize = PM4_MEC_DISPATCH_INDIRECT_SIZEDW__CORE;
    PM4_MEC_DISPATCH_INDIRECT packet = {};

    packet.ordinal1.header.u32All      = (Type3Header(IT_DISPATCH_INDIRECT, PacketSize)).u32All;
    packet.ordinal2.addr_lo            = LowPart(address);
    packet.ordinal3.addr_hi            = HighPart(address);

    auto* pDispatchInitiator = reinterpret_cast<regCOMPUTE_DISPATCH_INITIATOR*>(&(packet.ordinal4.dispatch_initiator));

    pDispatchInitiator->bits.COMPUTE_SHADER_EN  = 1;
    pDispatchInitiator->bits.FORCE_START_AT_000 = 1;
    pDispatchInitiator->bits.ORDER_MODE         = 1;
    pDispatchInitiator->bits.CS_W32_EN          = isWave32;
    pDispatchInitiator->bits.TUNNEL_ENABLE      = useTunneling;

    if (disablePartialPreempt)
    {
        pDispatchInitiator->bits.DISABLE_DISP_PREMPT_EN = 1;
    }

    memcpy(pBuffer, &packet, PacketSize * sizeof(uint32));

    return PacketSize;
}

// =====================================================================================================================
// Returns the number of dwords required to chain two pm4 packet chunks together.
uint32 CmdUtil::ChainSizeInDwords(
    EngineType engineType)
{
    uint32 size = 0;

    // The packet used for chaining indirect-buffers together differs based on the queue we're executing on.
    if (Pal::Device::EngineSupportsGraphics(engineType))
    {
        size = PM4_PFP_INDIRECT_BUFFER_SIZEDW__CORE;
    }
    else if (engineType == EngineTypeCompute)
    {
        size = PM4_MEC_INDIRECT_BUFFER_SIZEDW__CORE;
    }
    else
    {
        // Other engine types do not support chaining.
    }

    return size;
}

// =====================================================================================================================
// Builds an indirect-buffer packet for graphics with optional chaining support.
// Returns the size of the packet, in DWORDs
size_t CmdUtil::BuildIndirectBuffer(
    EngineType engineType, // queue this IB will be executed on
    gpusize    ibAddr,     // gpu virtual address of the indirect buffer
    uint32     ibSize,     // size of indirect buffer in dwords
    bool       chain,
    bool       enablePreemption,
    void*      pBuffer)    // space to place the newly-generated PM4 packet into
{
    static_assert(PM4_PFP_INDIRECT_BUFFER_SIZEDW__CORE == PM4_MEC_INDIRECT_BUFFER_SIZEDW__CORE,
                  "Indirect buffer packets are not the same size between GFX and compute!");

    PM4_PFP_INDIRECT_BUFFER packet = {};
    constexpr uint32 PacketSize = PM4_MEC_INDIRECT_BUFFER_SIZEDW__CORE;
    const IT_OpCodeType opCode = IT_INDIRECT_BUFFER;

    packet.ordinal1.header.u32All = (Type3Header(opCode, PacketSize)).u32All;
    packet.ordinal2.u32All        = LowPart(ibAddr);
    packet.ordinal3.ib_base_hi    = HighPart(ibAddr);

    // Make sure our address is properly ali`gned
    PAL_ASSERT(packet.ordinal2.bitfields.reserved1 == 0);

    packet.ordinal4.bitfields.ib_size = ibSize;
    packet.ordinal4.bitfields.chain   = chain;

    if (engineType == EngineTypeCompute)
    {
        // This bit only exists on the compute version of this packet.
        auto pMecPacket = reinterpret_cast<PM4_MEC_INDIRECT_BUFFER*>(&packet);
        pMecPacket->ordinal4.bitfields.valid = 1;
        PAL_ASSERT(enablePreemption == false);
    }
    else
    {
        packet.ordinal4.bitfields.pre_ena = enablePreemption;
    }

    static_assert(PacketSize * sizeof(uint32) == sizeof(packet), "");
    memcpy(pBuffer, &packet, sizeof(packet));
    return PacketSize;
}

// =====================================================================================================================
// Build an EVENT_WRITE packet.  Not to be used for any EOP or EOS type events.  Return the number of DWORDs taken up
// by this packet.
size_t CmdUtil::BuildSampleEventWrite(
    VGT_EVENT_TYPE                           vgtEvent,
    ME_EVENT_WRITE_event_index_enum          eventIndex,
    EngineType                               engineType,
    MEC_EVENT_WRITE_samp_plst_cntr_mode_enum counterMode,
    gpusize                                  gpuAddr,
    void*                                    pBuffer)     // [out] Build the PM4 packet in this buffer.
{
    // Verify the event index enumerations match between the ME and MEC engines.  Note that ME (gfx) has more
    // events than MEC does.  We assert below if this packet is meant for compute and a gfx-only index is selected.
    static_assert(
        ((static_cast<uint32>(event_index__mec_event_write__other)                  ==
          static_cast<uint32>(event_index__me_event_write__other))                  &&
         (static_cast<uint32>(event_index__mec_event_write__cs_partial_flush)       ==
          static_cast<uint32>(event_index__me_event_write__cs_vs_ps_partial_flush)) &&
         (static_cast<uint32>(event_index__mec_event_write__sample_pipelinestat)    ==
          static_cast<uint32>(event_index__me_event_write__sample_pipelinestat))),
        "event index enumerations don't match between gfx and compute!");

#if PAL_ENABLE_PRINTS_ASSERTS
    // Make sure the supplied VGT event is legal.
    PAL_ASSERT(vgtEvent < (sizeof(VgtEventIndex) / sizeof(VGT_EVENT_TYPE)));

    PAL_ASSERT((vgtEvent == PIXEL_PIPE_STAT_CONTROL) ||
               (vgtEvent == PIXEL_PIPE_STAT_DUMP)    ||
               (vgtEvent == SAMPLE_PIPELINESTAT)     ||
               (vgtEvent == SAMPLE_STREAMOUTSTATS)   ||
               (vgtEvent == SAMPLE_STREAMOUTSTATS1)  ||
               (vgtEvent == SAMPLE_STREAMOUTSTATS2)  ||
               (vgtEvent == SAMPLE_STREAMOUTSTATS3)  ||
               (vgtEvent == VS_PARTIAL_FLUSH));

    PAL_ASSERT((VgtEventIndex[vgtEvent] == event_index__me_event_write__pixel_pipe_stat_control_or_dump) ||
               (VgtEventIndex[vgtEvent] == event_index__me_event_write__sample_pipelinestat)             ||
               (VgtEventIndex[vgtEvent] == event_index__me_event_write__cs_vs_ps_partial_flush));

    // Event-write packets destined for the compute queue can only use some events.
    PAL_ASSERT((engineType != EngineTypeCompute) ||
               (static_cast<uint32>(eventIndex) ==
                static_cast<uint32>(event_index__mec_event_write__sample_pipelinestat)));

    // All samples are 64-bit and must meet that address alignment.
    PAL_ASSERT(IsPow2Aligned(gpuAddr, sizeof(uint64)));
#endif

    // Here's where packet building actually starts.
    uint32 packetSize;

    if ((vgtEvent   == PIXEL_PIPE_STAT_DUMP) &&
        (eventIndex == event_index__me_event_write__pixel_pipe_stat_control_or_dump))
    {
        packetSize = SampleEventWriteZpassSizeDwords;

        PM4_ME_EVENT_WRITE_ZPASS packet = {};
        packet.ordinal1.header = Type3Header(IT_EVENT_WRITE_ZPASS, packetSize);
        packet.ordinal2.u32All = LowPart(gpuAddr);
        packet.ordinal3.u32All = HighPart(gpuAddr);

        memcpy(pBuffer, &packet, sizeof(packet));
    }
    else
    {
        packetSize = SampleEventWriteSizeDwords;

        PM4_ME_EVENT_WRITE packet = {};
        packet.ordinal1.header                = Type3Header(IT_EVENT_WRITE, packetSize);
        packet.ordinal2.u32All                = 0;
        packet.ordinal2.bitfields.event_type  = vgtEvent;
        packet.ordinal2.bitfields.event_index = eventIndex;

        if ((engineType == EngineTypeCompute) && (vgtEvent == SAMPLE_PIPELINESTAT))
        {
            auto*const pPacketMec = reinterpret_cast<PM4_MEC_EVENT_WRITE*>(&packet);
            pPacketMec->ordinal2.bitfields.samp_plst_cntr_mode = counterMode;
        }

        packet.ordinal3.u32All = LowPart(gpuAddr);
        packet.ordinal4.u32All = HighPart(gpuAddr);

        memcpy(pBuffer, &packet, sizeof(packet));
    }

    return packetSize;
}

// =====================================================================================================================
// Build an EVENT_WRITE packet.  Not to be used for any EOP, EOS or SAMPLE_XXXXX type events.  Return the number of
// DWORDs taken up by this packet.
size_t CmdUtil::BuildNonSampleEventWrite(
    VGT_EVENT_TYPE  vgtEvent,
    EngineType      engineType,
    void*           pBuffer)   // [out] Build the PM4 packet in this buffer.
{
    return BuildNonSampleEventWrite(vgtEvent, engineType, PredDisable, pBuffer);
}

// =====================================================================================================================
// Build an EVENT_WRITE packet.  Not to be used for any EOP, EOS or SAMPLE_XXXXX type events.  Return the number of
// DWORDs taken up by this packet.
size_t CmdUtil::BuildNonSampleEventWrite(
    VGT_EVENT_TYPE  vgtEvent,
    EngineType      engineType,
    Pm4Predicate    predicate,
    void*           pBuffer)   // [out] Build the PM4 packet in this buffer.
{
    // Verify the event index enumerations match between the ME and MEC engines.  Note that ME (gfx) has more
    // events than MEC does.  We assert below if this packet is meant for compute and a gfx-only index is selected.
    static_assert(
        ((static_cast<uint32>(event_index__mec_event_write__other)                  ==
          static_cast<uint32>(event_index__me_event_write__other))                  &&
         (static_cast<uint32>(event_index__mec_event_write__cs_partial_flush)       ==
          static_cast<uint32>(event_index__me_event_write__cs_vs_ps_partial_flush)) &&
         (static_cast<uint32>(event_index__mec_event_write__sample_pipelinestat)    ==
          static_cast<uint32>(event_index__me_event_write__sample_pipelinestat))),
        "event index enumerations don't match between gfx and compute!");

    // Make sure the supplied VGT event is legal.
    PAL_ASSERT(vgtEvent < (sizeof(VgtEventIndex) / sizeof(VGT_EVENT_TYPE)));

    // Event-write packets destined for the compute queue can only use some events.
    PAL_ASSERT(Pal::Device::EngineSupportsGraphics(engineType)                       ||
               (static_cast<uint32>(VgtEventIndex[vgtEvent])                         ==
                static_cast<uint32>(event_index__mec_event_write__other))            ||
               (static_cast<uint32>(VgtEventIndex[vgtEvent])                         ==
                static_cast<uint32>(event_index__mec_event_write__cs_partial_flush)) ||
               (static_cast<uint32>(VgtEventIndex[vgtEvent])                         ==
                static_cast<uint32>(event_index__mec_event_write__sample_pipelinestat)));

    // The CP team says you risk hanging the GPU if you use a TS event with event_write.
    PAL_ASSERT(VgtEventHasTs[vgtEvent] == false);

    // Don't use PM4_ME_EVENT_WRITE_SIZEDW__CORE here!  The official packet definition contains extra dwords
    // for functionality that is only required for "sample" type events.
    constexpr uint32   PacketSize = NonSampleEventWriteSizeDwords;
    PM4_ME_EVENT_WRITE packet;
    packet.ordinal1.header                = Type3Header(IT_EVENT_WRITE, PacketSize, false, ShaderGraphics, predicate);
    packet.ordinal2.u32All                = 0;
    packet.ordinal2.bitfields.event_type  = vgtEvent;
    packet.ordinal2.bitfields.event_index = VgtEventIndex[vgtEvent];

    // Enable offload compute queue until EOP queue goes empty to increase multi-queue concurrency
    if ((engineType == EngineTypeCompute) && (vgtEvent == CS_PARTIAL_FLUSH))
    {
        auto*const pPacketMec = reinterpret_cast<PM4_MEC_EVENT_WRITE*>(&packet);

        pPacketMec->ordinal2.bitfields.offload_enable = 1;
    }

    memcpy(pBuffer, &packet, PacketSize * sizeof(uint32));

    return PacketSize;
}

// For AcquireMemGcrCntl and ReleaseMemGcrCntl below,
// Only need set seq=1 if write back lower level caches (glkWb/wbRbCache) and write back GL2 together.
// Note that we don't need set seq=1 at both Release (EopGcrCntl) and Acquire (GcrCntl) for below reason,
//     (1) glkWb is always 0 since shader compiler doesn't use SMEM_WRITE so no need glkWb.
//     (2) wbRbCache is always done via an EOP event at Release and no wbRbCache at Acquire. At Release point,
//         EopGcrCntl wouldn't be processed until EOP event (and its corresponding cache flush) done signal is
//         received, so no need worry the write back ordering issue.

// GCR_CNTL bit fields for ACQUIRE_MEM.
union AcquireMemGcrCntl
{
    struct
    {
        uint32 gliInv      : 2;
        uint32 gl1Range    : 2; // Range control for K$ and V$.
        uint32 reserved4_5 : 2;
        uint32 glkWb       : 1;
        uint32 glkInv      : 1;
        uint32 glvInv      : 1;
        uint32 reserved9   : 1;
        uint32 gl2Us       : 1;
        uint32 gl2Range    : 2;
        uint32 gl2Discard  : 1;
        uint32 gl2Inv      : 1;
        uint32 gl2Wb       : 1;
        uint32 seq         : 2;
        uint32 rangeIsPa   : 1;
        uint32             : 13;
    } bits;

    uint32  u32All;
};

// GCR_CNTL bit fields for RELEASE_MEM.
union ReleaseMemGcrCntl
{
    struct
    {
        uint32 reserved0_1 : 2;
        uint32 glvInv      : 1;
        uint32 reserved4   : 1;
        uint32 gl2Us       : 1;
        uint32 gl2Range    : 2;
        uint32 gl2Discard  : 1;
        uint32 gl2Inv      : 1;
        uint32 gl2Wb       : 1;
        uint32 seq         : 2;
        uint32 glkWb       : 1;
        uint32             : 19;
    } bits;

    uint32  u32All;
};

// =====================================================================================================================
// A helper to set and return the AcquireMemGcrCntl bits.
static uint32 GetAcquireMemGcrCntlBits(
    SyncGlxFlags cacheSync)
{
    // Note that glmWb is unimplemented in HW so we don't bother setting it. Everything else we want zeroed.
    //
    // We always prefer parallel cache ops but must force sequential (L0->L1->L2) mode when we're writing back a
    // non-write-through L0 before an L2 writeback. The only writable L0 that a PWS acquire can flush is the K$
    // but K$ is read only now and we don't use K$ writeback.
    //
    // Note that we default-initialize the "range" fields to 0 which means "ALL"/"entire cache". This is on purpose,
    // range-based invalidates require a series of page walks in HW which makes barriers run slowly. Even for smaller
    // allocations we don't think it's worth the hassle.
    AcquireMemGcrCntl cntl = {};
    cntl.bits.gliInv = TestAnyFlagSet(cacheSync, SyncGliInv);
    cntl.bits.glkInv = TestAnyFlagSet(cacheSync, SyncGlkInv);
    cntl.bits.glvInv = TestAnyFlagSet(cacheSync, SyncGlvInv);
    cntl.bits.gl2Inv = TestAnyFlagSet(cacheSync, SyncGl2Inv);
    cntl.bits.gl2Wb  = TestAnyFlagSet(cacheSync, SyncGl2Wb);

    return cntl.u32All;
}

// =====================================================================================================================
size_t CmdUtil::BuildAcquireMemGfxPws(
    const AcquireMemGfxPws& info,
    void*                   pBuffer)
{
    // There are a couple of cases where we need to modify the caller's stage select before applying it.
    ME_ACQUIRE_MEM_pws_stage_sel_enum stageSel = info.stageSel;

    // We need to wait at one of the CP stages if we want it to do a GCR after waiting. Rather than force the caller
    // to get this right we just silently handle it. It can't cause any correctness issues, it's just a perf hit.
    if ((info.cacheSync != 0) &&
        (stageSel != pws_stage_sel__me_acquire_mem__cp_me) &&
        (stageSel != pws_stage_sel__me_acquire_mem__cp_pfp))
    {
        stageSel = pws_stage_sel__me_acquire_mem__cp_me;
    }

    constexpr uint32   PacketSize = PM4_ME_ACQUIRE_MEM_SIZEDW__CORE;
    PM4_ME_ACQUIRE_MEM packet     = {};

    packet.ordinal1.header                     = Type3Header(IT_ACQUIRE_MEM, PacketSize);
    packet.ordinal2.bitfieldsB.pws_stage_sel   = stageSel;
    packet.ordinal2.bitfieldsB.pws_counter_sel = info.counterSel;
    packet.ordinal2.bitfieldsB.pws_ena2        = pws_ena2__me_acquire_mem__pixel_wait_sync_enable;
    packet.ordinal2.bitfieldsB.pws_count       = info.syncCount;

    // The GCR base and size are in units of 128 bytes. For a full range acquire, we're required to set every bit in
    // base to '0' and every bit in size to '1'. We only support full-range acquires so we just hard-code that state.
    // Note that we zeroed the base earlier so we only need to program the size here.
    packet.ordinal3.gcr_size               = UINT32_MAX;
    packet.ordinal4.bitfieldsB.gcr_size_hi = BitfieldGenMask(25u); // gcr_size_hi is only 25 bits.
    packet.ordinal7.bitfieldsB.pws_ena     = pws_ena__me_acquire_mem__pixel_wait_sync_enable;

    if (info.cacheSync != 0)
    {
        packet.ordinal8.bitfields.gcr_cntl = GetAcquireMemGcrCntlBits(info.cacheSync);
    }

    memcpy(pBuffer, &packet, PacketSize * sizeof(uint32));

    return PacketSize;
}

static_assert(PM4_MEC_ACQUIRE_MEM_SIZEDW__CORE == PM4_ME_ACQUIRE_MEM_SIZEDW__CORE,
              "ACQUIRE_MEM packet size is different between ME compute and ME graphics!");

// =====================================================================================================================
// Used for sync GCR caches only
size_t CmdUtil::BuildAcquireMemGeneric(
    const AcquireMemGeneric& info,
    void*                    pBuffer)
{
    constexpr uint32   PacketSize = PM4_ME_ACQUIRE_MEM_SIZEDW__CORE;
    PM4_ME_ACQUIRE_MEM packet     = {};

    packet.ordinal1.header = Type3Header(IT_ACQUIRE_MEM, PacketSize);

    // Note that this field isn't used on ACE.
    if (Pal::Device::EngineSupportsGraphics(info.engineType))
    {
        packet.ordinal2.bitfieldsA.engine_sel = engine_sel__me_acquire_mem__micro_engine;
    }

    // The coher base and size are in units of 256 bytes. For a full range acquire, we're required to set every bit in
    // base to '0' and every bit in size to '1'. We only support full-range acquires so we just hard-code that state.
    // Note that we zeroed the base earlier so we only need to program the size here.
    packet.ordinal3.coher_size               = UINT32_MAX;
    packet.ordinal4.bitfieldsA.coher_size_hi = BitfieldGenMask(24u); // coher_size_hi is only 24 bits.
    packet.ordinal7.bitfieldsA.poll_interval = Pal::Device::PollInterval;

    if (info.cacheSync != 0)
    {
        packet.ordinal8.bitfields.gcr_cntl = GetAcquireMemGcrCntlBits(info.cacheSync);
    }

    memcpy(pBuffer, &packet, PacketSize * sizeof(uint32));

    return PacketSize;
}

// =====================================================================================================================
// Constructs a PM4 packet which issues a sync command instructing the PFP to stall until the ME is no longer busy. This
// packet will hang on the compute queue; it is the caller's responsibility to ensure that this function is called
// safely. Returns the size of the PM4 command built, in DWORDs.
size_t CmdUtil::BuildPfpSyncMe(
    void* pBuffer) // [out] Build the PM4 packet in this buffer.
{
    constexpr uint32 PacketSize = PM4_PFP_PFP_SYNC_ME_SIZEDW__CORE;
    auto*const       pPacket    = static_cast<PM4_PFP_PFP_SYNC_ME*>(pBuffer);

    pPacket->ordinal1.header.u32All = (Type3Header(IT_PFP_SYNC_ME, PacketSize)).u32All;
    pPacket->ordinal2.dummy_data    = 0;

    return PacketSize;
}

// =====================================================================================================================
// Call this to pick an appropriate graphics EOP_TS event for a release_mem.
VGT_EVENT_TYPE CmdUtil::SelectEopEvent(
    SyncRbFlags rbSync)
{
    VGT_EVENT_TYPE vgtEvent;

    // We start with the most specific events which touch the fewest caches and walk the list until we get
    // CACHE_FLUSH_AND_INV_TS_EVENT which hits all of them.
    if (rbSync == SyncRbNone)
    {
        // No flags so don't flush or invalidate anything.
        vgtEvent = BOTTOM_OF_PIPE_TS;
    }
    else if (rbSync == SyncCbDataWbInv)
    {
        // Just CB data caches.
        vgtEvent = FLUSH_AND_INV_CB_DATA_TS;
    }
    else if (rbSync == SyncDbDataWbInv)
    {
        // Just DB data caches.
        vgtEvent = FLUSH_AND_INV_DB_DATA_TS;
    }
    else if (TestAnyFlagSet(rbSync, SyncRbInv) == false)
    {
        // Flush everything, no invalidates.
        vgtEvent = CACHE_FLUSH_TS;
    }
    else
    {
        // Flush and invalidate everything.
        vgtEvent = CACHE_FLUSH_AND_INV_TS_EVENT;
    }

    return vgtEvent;
}

// =====================================================================================================================
// Returns a ReleaseMemCaches that applies as many flags from pGlxSync as it can, masking off the consumed flags.
// The caller is expected to forward the remaining flags to an acquire_mem.
ReleaseMemCaches CmdUtil::SelectReleaseMemCaches(
    SyncGlxFlags* pGlxSync)
{
    // First, split the syncs into a release set and an acquire set.
    constexpr SyncGlxFlags ReleaseMask = SyncGl2WbInv | SyncGlvInv | SyncGlkInv;

    SyncGlxFlags releaseSyncs = *pGlxSync & ReleaseMask;
    SyncGlxFlags acquireSyncs = *pGlxSync & ~ReleaseMask;

    ReleaseMemCaches caches = {};
    caches.gl2Inv = TestAnyFlagSet(releaseSyncs, SyncGl2Inv);
    caches.gl2Wb  = TestAnyFlagSet(releaseSyncs, SyncGl2Wb);
    caches.glvInv = TestAnyFlagSet(releaseSyncs, SyncGlvInv);
    caches.glkInv = TestAnyFlagSet(releaseSyncs, SyncGlkInv);

    // Pass the extra flags back out to the caller so they know they need to handle them in an acquire_mem.
    *pGlxSync = acquireSyncs;

    // Make sure all SyncGlxFlags have been converted to ReleaseMemCaches. The only possible sync bit here is
    // SyncGliInv but it's only used in submit preamble via BuildAcquireMemGeneric() call.
    PAL_ASSERT(acquireSyncs == SyncGlxNone);

    return caches;
}

// =====================================================================================================================
size_t CmdUtil::BuildReleaseMemGeneric(
    const ReleaseMemGeneric& info,
    void*                    pBuffer
    ) const
{
    VGT_EVENT_TYPE vgtEvent = info.vgtEvent;

    const bool isEop = VgtEventHasTs[vgtEvent];

    // The release_mem packet only supports EOS events or EOP TS events.
    PAL_ASSERT(isEop || (vgtEvent == PS_DONE) || (vgtEvent == CS_DONE));

    // This function only supports Glx cache syncs on EOP events. This restriction comes from the graphics engine,
    // where EOS releases don't support cache flushes but can still issue timestamps. On compute engines we could
    // support EOS cache syncs but it's not useful practically speaking because the ACE treats CS_DONE events exactly
    // the same as EOP timestamp events. If we force the caller to use a BOTTOM_OF_PIPE_TS on ACE they lose nothing.
    PAL_ASSERT(isEop || (info.cacheSync.u8All == 0));

    // The EOS path also only supports constant timestamps; that's right, it doesn't support "none".
    // Yes, that means you have to provide a valid dstAddr even when using PWS if the event is an EOS event.
    PAL_ASSERT(isEop || (info.dataSel == data_sel__me_release_mem__send_32_bit_low)
                     || (info.dataSel == data_sel__me_release_mem__send_64_bit_data));

    constexpr uint32   PacketSize = PM4_ME_RELEASE_MEM_SIZEDW__CORE;
    PM4_ME_RELEASE_MEM packet     = {};

    packet.ordinal1.header                = Type3Header(IT_RELEASE_MEM, PacketSize);
    packet.ordinal2.bitfields.event_type  = vgtEvent;
    packet.ordinal2.bitfields.event_index = isEop ? event_index__me_release_mem__end_of_pipe
                                                  : event_index__me_release_mem__shader_done;
    packet.ordinal3.bitfields.data_sel    = static_cast<ME_RELEASE_MEM_data_sel_enum>(info.dataSel);
    packet.ordinal3.bitfields.dst_sel     = dst_sel__me_release_mem__tc_l2;
    packet.ordinal4.u32All                = LowPart(info.dstAddr);
    packet.ordinal5.address_hi            = HighPart(info.dstAddr);
    packet.ordinal6.data_lo               = LowPart(info.data);
    packet.ordinal7.data_hi               = HighPart(info.data);

    if (info.dataSel != data_sel__me_release_mem__none)
    {
        // dstAddr must be properly aligned. 4 bytes for a 32-bit write or 8 bytes for a 64-bit write.
        PAL_ASSERT((info.dstAddr != 0) &&
                   (((info.dataSel == data_sel__me_release_mem__send_32_bit_low) && IsPow2Aligned(info.dstAddr, 4)) ||
                    IsPow2Aligned(info.dstAddr, 8)));

        if (info.noConfirmWr == false)
        {
            // This won't send an interrupt but will wait for write confirm before indicating completion
            packet.ordinal3.bitfields.int_sel = int_sel__me_release_mem__send_data_and_write_confirm;
        }
    }

    // Clients must query EnableReleaseMemWaitCpDma() to make sure ReleaseMem packet supports waiting CP DMA before
    // setting info.waitCpDma to true here.
    PAL_ASSERT((info.waitCpDma == false) || (m_device.Settings().enableReleaseMemWaitCpDma));

    packet.ordinal2.bitfields.pws_enable = info.usePws;
    packet.ordinal2.bitfields.wait_sync  = info.waitCpDma;

    if (info.cacheSync.u8All != 0)
    {
        // Note that glmWb is unimplemented in HW so we don't bother setting it. Everything else we want zeroed.
        //
        // We always prefer parallel cache ops but must force sequential (L0->L1->L2) mode when we're writing
        // back one of the non-write-through L0s before an L2 writeback. Any L0 flush/inv ops in our release_mem's
        // event are already sequential with the CP's GCR request so we only have to worry about K$ writes.
        ReleaseMemGcrCntl cntl = {};
        cntl.bits.glvInv     = info.cacheSync.glvInv;
        cntl.bits.gl2Inv     = info.cacheSync.gl2Inv;
        cntl.bits.gl2Wb      = info.cacheSync.gl2Wb;
        cntl.bits.seq        = info.cacheSync.gl2Wb & info.cacheSync.glkWb;
        cntl.bits.glkWb      = info.cacheSync.glkWb;

        packet.ordinal2.bitfields.gcr_cntl = cntl.u32All;
        packet.ordinal2.bitfields.glk_inv  = info.cacheSync.glkInv;
    }

    // Write the release_mem packet and return the packet size in DWORDs.
    memcpy(pBuffer, &packet, PacketSize * sizeof(uint32));

    return PacketSize;
}

// =====================================================================================================================
// Builds a set of PM4 commands that writes a PWS-enabled EOP event then waits for the event to complete.
// Requested cache operations trigger after the release but before the wait clears. The actual wait point may be more
// strict (e.g., ME wait instead of pre_color wait) if PAL needs to adjust things to make the cache operations work.
// An ME wait and EOP release would emulate a non-PWS wait for idle.
//
// Returns the size of the PM4 command built, in DWORDs. Only supported on gfx11+.
size_t CmdUtil::BuildWaitEopPws(
    AcquirePoint waitPoint,
    bool         waitCpDma,
    SyncGlxFlags glxSync,
    SyncRbFlags  rbSync,
    void*        pBuffer
    ) const
{
    size_t totalSize = 0;

    // Clamp waitPoint if PWS late acquire point is disabled.
    if ((waitPoint > AcquirePointMe)   &&
        (waitPoint != AcquirePointEop) &&
        (m_device.Parent()->UsePwsLateAcquirePoint(EngineTypeUniversal) == false))
    {
        waitPoint = AcquirePointMe;
    }

    // Issue explicit waitCpDma packet if ReleaseMem doesn't support it.
    if (waitCpDma && (m_device.Settings().enableReleaseMemWaitCpDma == false))
    {
        totalSize += CmdUtil::BuildWaitDmaData(pBuffer);
        waitCpDma = false;
    }

    ReleaseMemGeneric releaseInfo = {};
    releaseInfo.vgtEvent  = SelectEopEvent(rbSync);
    releaseInfo.cacheSync = SelectReleaseMemCaches(&glxSync);
    releaseInfo.dataSel   = data_sel__me_release_mem__none;
    releaseInfo.usePws    = true;
    releaseInfo.waitCpDma = waitCpDma;

    totalSize += BuildReleaseMemGeneric(releaseInfo, VoidPtrInc(pBuffer, totalSize * sizeof(uint32)));

    // We define an "EOP" wait to mean a release without an acquire.
    // If glxSync still has some flags left over we still need an acquire to issue the GCR.
    if ((waitPoint != AcquirePointEop) || (glxSync != SyncGlxNone))
    {
        // This will set syncCount = 0 to wait for the most recent PWS release_mem (the one we just wrote).
        AcquireMemGfxPws acquireInfo = {};

        // Practically speaking, SelectReleaseMemCaches should consume all of our cache flags on gfx11. If the caller
        // asked for an I$ invalidate then it will get passed to the acquire_mem here but that sync should be rare.
        acquireInfo.cacheSync  = glxSync;
        acquireInfo.counterSel = pws_counter_sel__me_acquire_mem__ts_select;

        switch (waitPoint)
        {
        case AcquirePointPfp:
            acquireInfo.stageSel = pws_stage_sel__me_acquire_mem__cp_pfp;
            break;
        case AcquirePointMe:
            acquireInfo.stageSel = pws_stage_sel__me_acquire_mem__cp_me;
            break;
        case AcquirePointPreDepth:
        case AcquirePointEop:
            acquireInfo.stageSel = pws_stage_sel__me_acquire_mem__pre_depth;
            break;
        default:
            // What is this?
            PAL_ASSERT_ALWAYS();
            break;
        }

        totalSize += BuildAcquireMemGfxPws(acquireInfo, VoidPtrInc(pBuffer, totalSize * sizeof(uint32)));
    }

    return totalSize;
}

// =====================================================================================================================
// Builds a WAIT_REG_MEM PM4 packet. Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildWaitRegMem(
    EngineType    engineType,
    uint32        memSpace,
    uint32        function,
    uint32        engine,
    gpusize       addr,
    uint32        reference,
    uint32        mask,
    void*         pBuffer,   // [out] Build the PM4 packet in this buffer.
    uint32        operation)
{
    static_assert(PM4_ME_WAIT_REG_MEM_SIZEDW__CORE == PM4_MEC_WAIT_REG_MEM_SIZEDW__CORE,
                  "WAIT_REG_MEM has different sizes between compute and gfx!");
    static_assert(
        ((static_cast<uint32>(function__me_wait_reg_mem__always_pass)                               ==
          static_cast<uint32>(function__mec_wait_reg_mem__always_pass))                             &&
         (static_cast<uint32>(function__me_wait_reg_mem__less_than_ref_value)                       ==
          static_cast<uint32>(function__mec_wait_reg_mem__less_than_ref_value))                     &&
         (static_cast<uint32>(function__me_wait_reg_mem__less_than_equal_to_the_ref_value)          ==
          static_cast<uint32>(function__mec_wait_reg_mem__less_than_equal_to_the_ref_value))        &&
         (static_cast<uint32>(function__me_wait_reg_mem__equal_to_the_reference_value)              ==
          static_cast<uint32>(function__mec_wait_reg_mem__equal_to_the_reference_value))            &&
         (static_cast<uint32>(function__me_wait_reg_mem__not_equal_reference_value)                 ==
          static_cast<uint32>(function__mec_wait_reg_mem__not_equal_reference_value))               &&
         (static_cast<uint32>(function__me_wait_reg_mem__greater_than_or_equal_reference_value)     ==
          static_cast<uint32>(function__mec_wait_reg_mem__greater_than_or_equal_reference_value))   &&
         (static_cast<uint32>(function__me_wait_reg_mem__greater_than_reference_value)              ==
          static_cast<uint32>(function__mec_wait_reg_mem__greater_than_reference_value))),
        "Function enumerations don't match between ME and MEC!");
    static_assert(
        ((static_cast<uint32>(mem_space__me_wait_reg_mem__register_space)   ==
          static_cast<uint32>(mem_space__mec_wait_reg_mem__register_space)) &&
         (static_cast<uint32>(mem_space__me_wait_reg_mem__memory_space)     ==
          static_cast<uint32>(mem_space__mec_wait_reg_mem__memory_space))),
        "Memory space enumerations don't match between ME and MEC!");
    static_assert(
        ((static_cast<uint32>(operation__me_wait_reg_mem__wait_reg_mem)         ==
          static_cast<uint32>(operation__mec_wait_reg_mem__wait_reg_mem))       &&
         (static_cast<uint32>(operation__me_wait_reg_mem__wait_mem_preemptable) ==
          static_cast<uint32>(operation__mec_wait_reg_mem__wait_mem_preemptable))),
        "Operation enumerations don't match between ME and MEC!");

    // We build the packet with the ME definition, but the MEC definition is identical, so it should work...
    constexpr uint32    PacketSize = WaitRegMemSizeDwords;
    auto*const          pPacket    = static_cast<PM4_ME_WAIT_REG_MEM*>(pBuffer);
    PM4_ME_WAIT_REG_MEM packet     = { 0 };

    packet.ordinal1.header              = Type3Header(IT_WAIT_REG_MEM, PacketSize);
    packet.ordinal2.u32All              = 0;
    packet.ordinal2.bitfields.function  = static_cast<ME_WAIT_REG_MEM_function_enum>(function);
    packet.ordinal2.bitfields.mem_space = static_cast<ME_WAIT_REG_MEM_mem_space_enum>(memSpace);
    packet.ordinal2.bitfields.operation = static_cast<ME_WAIT_REG_MEM_operation_enum>(operation);
    packet.ordinal3.u32All              = LowPart(addr);

    if (memSpace == mem_space__me_wait_reg_mem__memory_space)
    {
        PAL_ASSERT(packet.ordinal3.bitfieldsA.reserved1 == 0);
    }
    else if (memSpace == mem_space__mec_wait_reg_mem__register_space)
    {
        PAL_ASSERT(packet.ordinal3.bitfieldsB.reserved2 == 0);
    }

    packet.ordinal4.mem_poll_addr_hi        = HighPart(addr);
    packet.ordinal5.reference               = reference;
    packet.ordinal6.mask                    = mask;
    packet.ordinal7.u32All                  = 0;
    packet.ordinal7.bitfields.poll_interval = Pal::Device::PollInterval;

    if (Pal::Device::EngineSupportsGraphics(engineType))
    {
        packet.ordinal2.bitfields.engine_sel = static_cast<ME_WAIT_REG_MEM_engine_sel_enum>(engine);
        *pPacket = packet;
    }
    else
    {
        auto*const pPacketMecOnly     = static_cast<PM4_MEC_WAIT_REG_MEM*>(pBuffer);
        PM4_MEC_WAIT_REG_MEM* pMecPkt = reinterpret_cast<PM4_MEC_WAIT_REG_MEM*>(&packet);

        // Similarly to engine_sel in ME, this ACE offload optimization is only for MEC and a reserved bit for ME.
        pMecPkt->ordinal7.bitfields.optimize_ace_offload_mode = 1;
        *pPacketMecOnly = *pMecPkt;
    }

    return PacketSize;
}

// =====================================================================================================================
// Builds a WRITE-DATA packet for either the MEC or ME engine.  Writes the data in "pData" into the GPU memory
// address "dstAddr".
static size_t BuildWriteDataInternal(
    const WriteDataInfo& info,
    size_t               dwordsToWrite,
    void*                pBuffer)       // [out] Build the PM4 packet in this buffer.
{
    static_assert(PM4_MEC_WRITE_DATA_SIZEDW__CORE == PM4_ME_WRITE_DATA_SIZEDW__CORE,
        "write_data packet has different sizes between compute and gfx!");
    static_assert(
        ((static_cast<uint32>(dst_sel__mec_write_data__mem_mapped_register) ==
          static_cast<uint32>(dst_sel__me_write_data__mem_mapped_register)) &&
         (static_cast<uint32>(dst_sel__mec_write_data__tc_l2)               ==
          static_cast<uint32>(dst_sel__me_write_data__tc_l2))               &&
         (static_cast<uint32>(dst_sel__mec_write_data__memory)              ==
          static_cast<uint32>(dst_sel__me_write_data__memory))),
        "DST_SEL enumerations don't match between MEC and ME!");
    static_assert(
        ((static_cast<uint32>(wr_confirm__mec_write_data__do_not_wait_for_write_confirmation) ==
          static_cast<uint32>(wr_confirm__me_write_data__do_not_wait_for_write_confirmation)) &&
         (static_cast<uint32>(wr_confirm__mec_write_data__wait_for_write_confirmation)        ==
          static_cast<uint32>(wr_confirm__me_write_data__wait_for_write_confirmation))),
         "WR_CONFIRM enumerations don't match between MEC and ME!");
    static_assert(
        ((static_cast<uint32>(addr_incr__me_write_data__do_not_increment_address)   ==
          static_cast<uint32>(addr_incr__mec_write_data__do_not_increment_address)) &&
         (static_cast<uint32>(addr_incr__me_write_data__increment_address)          ==
          static_cast<uint32>(addr_incr__mec_write_data__increment_address))),
         "ADDR_INCR enumerations don't match between MEC and ME!");

    // We build the packet with the ME definition, but the MEC definition is identical, so it should work...
    const uint32 packetSize = PM4_ME_WRITE_DATA_SIZEDW__CORE + static_cast<uint32>(dwordsToWrite);

    PM4_ME_WRITE_DATA packet = { 0 };

    packet.ordinal1.header                 = Type3Header(IT_WRITE_DATA,
                                                         packetSize,
                                                         false,
                                                         ShaderGraphics,
                                                         info.predicate);
    packet.ordinal2.u32All                 = 0;
    packet.ordinal2.bitfields.addr_incr    = info.dontIncrementAddr
                                               ? addr_incr__me_write_data__do_not_increment_address
                                               : addr_incr__me_write_data__increment_address;
    // Carlos: figure out new bitfield, mode/aid_id/temporal.
    packet.ordinal2.bitfields.dst_sel      = static_cast<ME_WRITE_DATA_dst_sel_enum>(info.dstSel);
    packet.ordinal2.bitfields.wr_confirm   = info.dontWriteConfirm
                                               ? wr_confirm__me_write_data__do_not_wait_for_write_confirmation
                                               : wr_confirm__me_write_data__wait_for_write_confirmation;
    if (Pal::Device::EngineSupportsGraphics(info.engineType))
    {
        // This field only exists on graphics engines.
        packet.ordinal2.bitfields.engine_sel = static_cast<ME_WRITE_DATA_engine_sel_enum>(info.engineSel);
    }
    packet.ordinal3.u32All                 = LowPart(info.dstAddr);
    packet.ordinal4.dst_mem_addr_hi        = HighPart(info.dstAddr);

    switch (info.dstSel)
    {
    case dst_sel__me_write_data__mem_mapped_register:
        PAL_ASSERT(packet.ordinal4.bitfieldsA.reserved1 == 0);
        break;

    case dst_sel__me_write_data__memory:
    case dst_sel__me_write_data__tc_l2:
        PAL_ASSERT(packet.ordinal3.bitfieldsB.reserved1 == 0);
        break;

    case dst_sel__me_write_data__memory_sync_across_grbm:
        PAL_ASSERT(Pal::Device::EngineSupportsGraphics(info.engineType));
        PAL_NOT_IMPLEMENTED();
        break;

    case dst_sel__mec_write_data__memory_mapped_adc_persistent_state:
        PAL_ASSERT(info.engineType == EngineTypeCompute);
        PAL_NOT_IMPLEMENTED();
        break;

    default:
        PAL_NOT_IMPLEMENTED();
        break;
    }

    auto* const   pPacket = static_cast<PM4_ME_WRITE_DATA*>(pBuffer);
    *pPacket = packet;

    return packetSize;
}

// =====================================================================================================================
// Builds a PM4 packet that writes a single data DWORD into the GPU memory address "dstAddr"
size_t CmdUtil::BuildWriteData(
    const WriteDataInfo& info,
    uint32               data,
    void*                pBuffer) // [out] Build the PM4 packet in this buffer.
{
    // Fill out a packet that writes a single DWORD, get a pointer to the embedded data payload, and fill it out.
    const size_t packetSize   = BuildWriteDataInternal(info, 1, pBuffer);
    uint32*const pDataPayload = static_cast<uint32*>(pBuffer) + packetSize - 1;

    *pDataPayload = data;

    return packetSize;
}

// =====================================================================================================================
// Builds a PM4 packet that writes the data in "pData" into the GPU memory address "dstAddr"
size_t CmdUtil::BuildWriteData(
    const WriteDataInfo& info,
    size_t               dwordsToWrite,
    const uint32*        pData,
    void*                pBuffer)       // [out] Build the PM4 packet in this buffer.
{
    const size_t packetSizeWithWrittenDwords = BuildWriteDataInternal(info, dwordsToWrite, pBuffer);

    // If this is null, the caller is just interested in the final packet size
    if (pData != nullptr)
    {
        const size_t packetSizeInBytes = (packetSizeWithWrittenDwords - dwordsToWrite) * sizeof(uint32);
        memcpy(VoidPtrInc(pBuffer, packetSizeInBytes), pData, dwordsToWrite * sizeof(uint32));
    }

    return packetSizeWithWrittenDwords;
}

// =====================================================================================================================
// This packet substitutes a COPY_DATA + RELEASE_MEM (cache flush) to copy the gpu/Soc clock counter to the dst Memory,
// and uses the GRBM bus to write&sync for gfx pipe instead of MALL/LLC.
// For compute pipe, it still goes through the MALL/LLC.
size_t CmdUtil::BuildWriteTimestamp(
    const TimestampInfo& info,
    void*                pBuffer)
{
    const size_t PacketSize = PM4_ME_TIMESTAMP_SIZEDW__CORE;
    auto*const   pPacket    = static_cast<PM4_ME_TIMESTAMP*>(pBuffer);

    PM4_ME_TIMESTAMP packetGfx = {};
    packetGfx.ordinal1.header = Type3Header(IT_TIMESTAMP, PacketSize, false, info.shaderType);
    packetGfx.ordinal2.bitfields.clock_sel = static_cast<ME_TIMESTAMP_clock_sel_enum>(info.clkSel);

    if (info.enableBottom)
    {
        packetGfx.ordinal3.u32All           = LowPart(info.dstAddr);
        packetGfx.ordinal4.pipe_bot_addr_hi = HighPart(info.dstAddr);
        packetGfx.ordinal2.bitfields.enable_bottom = 1;
    }
    else
    {
        packetGfx.ordinal5.u32All           = LowPart(info.dstAddr);
        packetGfx.ordinal6.pipe_top_addr_hi = HighPart(info.dstAddr);
        packetGfx.ordinal2.bitfields.enable_top = 1;
    }

   *pPacket = packetGfx;
    return PacketSize;
}

// =====================================================================================================================
size_t CmdUtil::BuildCopyData(
    const CopyDataInfo& info,
    void*               pBuffer)
{

    static_assert(PM4_ME_COPY_DATA_SIZEDW__CORE == PM4_MEC_COPY_DATA_SIZEDW__CORE,
                  "CopyData packet size is different between ME and MEC!");

    static_assert(((static_cast<uint32>(src_sel__mec_copy_data__mem_mapped_register)           ==
                    static_cast<uint32>(src_sel__me_copy_data__mem_mapped_register))           &&
                   (static_cast<uint32>(src_sel__mec_copy_data__tc_l2)                         ==
                    static_cast<uint32>(src_sel__me_copy_data__tc_l2))                         &&
                   (static_cast<uint32>(src_sel__mec_copy_data__perfcounters)                  ==
                    static_cast<uint32>(src_sel__me_copy_data__perfcounters))                  &&
                   (static_cast<uint32>(src_sel__mec_copy_data__immediate_data)                ==
                    static_cast<uint32>(src_sel__me_copy_data__immediate_data))                &&
                   (static_cast<uint32>(src_sel__mec_copy_data__atomic_return_data)            ==
                    static_cast<uint32>(src_sel__me_copy_data__atomic_return_data))            &&
                   (static_cast<uint32>(src_sel__mec_copy_data__gpu_clock_count)               ==
                    static_cast<uint32>(src_sel__me_copy_data__gpu_clock_count))),
                  "CopyData srcSel enum is different between ME and MEC!");

    static_assert(((static_cast<uint32>(dst_sel__mec_copy_data__mem_mapped_register) ==
                    static_cast<uint32>(dst_sel__me_copy_data__mem_mapped_register)) &&
                   (static_cast<uint32>(dst_sel__mec_copy_data__tc_l2)               ==
                    static_cast<uint32>(dst_sel__me_copy_data__tc_l2))               &&
                   (static_cast<uint32>(dst_sel__mec_copy_data__perfcounters)        ==
                    static_cast<uint32>(dst_sel__me_copy_data__perfcounters))),
                   "CopyData dstSel enum is different between ME and MEC!");

    static_assert(((static_cast<uint32>(count_sel__mec_copy_data__32_bits_of_data) ==
                    static_cast<uint32>(count_sel__me_copy_data__32_bits_of_data)) &&
                   (static_cast<uint32>(count_sel__mec_copy_data__64_bits_of_data) ==
                    static_cast<uint32>(count_sel__me_copy_data__64_bits_of_data))),
                  "CopyData countSel enum is different between ME and MEC!");

    static_assert(((static_cast<uint32>(wr_confirm__mec_copy_data__do_not_wait_for_confirmation) ==
                    static_cast<uint32>(wr_confirm__me_copy_data__do_not_wait_for_confirmation)) &&
                   (static_cast<uint32>(wr_confirm__mec_copy_data__wait_for_confirmation)        ==
                    static_cast<uint32>(wr_confirm__me_copy_data__wait_for_confirmation))),
                   "CopyData wrConfirm enum is different between ME and MEC!");

    constexpr uint32  PacketSize      = PM4_ME_COPY_DATA_SIZEDW__CORE;
    PM4_ME_COPY_DATA   packetGfx      = {};
    PM4_MEC_COPY_DATA* pPacketCompute = reinterpret_cast<PM4_MEC_COPY_DATA*>(&packetGfx);
    const bool        gfxSupported    = Pal::Device::EngineSupportsGraphics(info.engineType);
    const bool        isCompute       = (info.engineType == EngineTypeCompute);

    packetGfx.ordinal1.header         = Type3Header(IT_COPY_DATA, PacketSize);

    packetGfx.ordinal2.bitfields.src_sel    = static_cast<ME_COPY_DATA_src_sel_enum>(info.srcSel);
    packetGfx.ordinal2.bitfields.dst_sel    = static_cast<ME_COPY_DATA_dst_sel_enum>(info.dstSel);
    packetGfx.ordinal2.bitfields.count_sel  = static_cast<ME_COPY_DATA_count_sel_enum>(info.countSel);
    packetGfx.ordinal2.bitfields.wr_confirm = static_cast<ME_COPY_DATA_wr_confirm_enum>(info.wrConfirm);

    if (isCompute)
    {
        // Set these to their "zero" equivalents...  Enumerating these here explicitly to provide reminders that these
        // fields do exist.
        pPacketCompute->ordinal2.bitfields.src_temporal  = src_temporal__mec_copy_data__rt;
        pPacketCompute->ordinal2.bitfields.dst_temporal  = dst_temporal__mec_copy_data__rt;
        pPacketCompute->ordinal2.bitfields.pq_exe_status = pq_exe_status__mec_copy_data__default;
    }
    else
    {
        PAL_ASSERT(Pal::Device::EngineSupportsGraphics(info.engineType));

        // Set these to their "zero" equivalents...  Enumerating these here explicitly to provide reminders that these
        // fields do exist.
        packetGfx.ordinal2.bitfields.src_temporal = src_temporal__me_copy_data__rt;
        packetGfx.ordinal2.bitfields.dst_temporal = dst_temporal__me_copy_data__rt;
        packetGfx.ordinal2.bitfields.engine_sel   = static_cast<ME_COPY_DATA_engine_sel_enum>(info.engineSel);
    }

    switch (info.srcSel)
    {
    case src_sel__me_copy_data__perfcounters:
    case src_sel__me_copy_data__mem_mapped_register:
        packetGfx.ordinal3.u32All = LowPart(info.srcAddr);
        packetGfx.ordinal4.bitfieldsA.src_reg_offset_hi = HighPart(info.srcAddr);
        break;

    case src_sel__me_copy_data__immediate_data:
        packetGfx.ordinal3.imm_data = LowPart(info.srcAddr);

        // Really only meaningful if countSel==count_sel__me_copy_data__64_bits_of_data, but shouldn't hurt to
        // write it regardless.
        packetGfx.ordinal4.src_imm_data = HighPart(info.srcAddr);
        break;

    case src_sel__me_copy_data__tc_l2:
        packetGfx.ordinal3.u32All            = LowPart(info.srcAddr);
        packetGfx.ordinal4.src_memtc_addr_hi = HighPart(info.srcAddr);

        // Make sure our srcAddr is properly aligned.  The alignment differs based on how much data is being written
        PAL_ASSERT(((info.countSel == count_sel__mec_copy_data__64_bits_of_data) &&
                    ((isCompute    && (pPacketCompute->ordinal3.bitfieldsC.reserved2 == 0)) ||
                     (gfxSupported && (packetGfx.ordinal3.bitfieldsC.reserved2 == 0)))) ||
                   ((info.countSel == count_sel__mec_copy_data__32_bits_of_data) &&
                    ((isCompute    && (pPacketCompute->ordinal3.bitfieldsB.reserved1 == 0))  ||
                     (gfxSupported && (packetGfx.ordinal3.bitfieldsB.reserved1 == 0)))));
        break;

    case src_sel__me_copy_data__gpu_clock_count:
        // Nothing to worry about here?
        break;

    default:
        // Feel free to implement this.  :-)
        PAL_NOT_IMPLEMENTED();
        break;
    }

    switch (info.dstSel)
    {
    case dst_sel__me_copy_data__perfcounters:
    case dst_sel__me_copy_data__mem_mapped_register:
        packetGfx.ordinal5.u32All = LowPart(info.dstAddr);
        packetGfx.ordinal6.bitfieldsA.dst_reg_offset_hi = HighPart(info.dstAddr);
        break;

    case dst_sel__me_copy_data__memory_sync_across_grbm:
        // sync memory destination is only available with ME engine on universal queue
        PAL_ASSERT(gfxSupported && (info.engineSel == engine_sel__me_copy_data__micro_engine));
        // break intentionally left out
        [[fallthrough]];
    case dst_sel__me_copy_data__tc_l2:
        packetGfx.ordinal5.u32All      = LowPart(info.dstAddr);
        packetGfx.ordinal6.dst_addr_hi = HighPart(info.dstAddr);

        // Make sure our dstAddr is properly aligned.  The alignment differs based on how much data is being written
        PAL_ASSERT(((info.countSel == count_sel__mec_copy_data__64_bits_of_data) &&
                    ((isCompute    && (pPacketCompute->ordinal5.bitfieldsC.reserved2 == 0)) ||
                     (gfxSupported && (packetGfx.ordinal5.bitfieldsC.reserved2 == 0)))) ||
                   ((info.countSel == count_sel__mec_copy_data__32_bits_of_data) &&
                    ((isCompute    && (pPacketCompute->ordinal5.bitfieldsB.reserved1 == 0)) ||
                     (gfxSupported && (packetGfx.ordinal5.bitfieldsB.reserved1 == 0)))));
        break;

    default:
        // Feel free to implement this.  :-)
        PAL_NOT_IMPLEMENTED();
        break;
    }

    memcpy(pBuffer, &packetGfx, PacketSize * sizeof(uint32));

    return PacketSize;
}

// =====================================================================================================================
// Constructs a DMA_DATA packet for any engine (PFP, ME, MEC).  Copies data from the source (can be immediate 32-bit
// data or a memory location) to a destination (either memory or a register).
template<bool indirectAddress>
size_t CmdUtil::BuildDmaData(
    const DmaDataInfo&  dmaDataInfo,
    void*               pBuffer) // [out] Build the PM4 packet in this buffer.
{
    static_assert((static_cast<uint32>(sas__mec_dma_data__memory) == static_cast<uint32>(sas__pfp_dma_data__memory)),
                  "MEC and PFP sas dma_data enumerations don't match!");

    static_assert((static_cast<uint32>(das__mec_dma_data__memory) == static_cast<uint32>(das__pfp_dma_data__memory)),
                  "MEC and PFP das dma_data enumerations don't match!");

    static_assert(
        ((static_cast<uint32>(dst_sel__mec_dma_data__dst_addr_using_das)  ==
          static_cast<uint32>(dst_sel__pfp_dma_data__dst_addr_using_das)) &&
         (static_cast<uint32>(dst_sel__mec_dma_data__dst_nowhere)         ==
          static_cast<uint32>(dst_sel__pfp_dma_data__dst_nowhere))        &&
         (static_cast<uint32>(dst_sel__mec_dma_data__dst_addr_using_l2)   ==
          static_cast<uint32>(dst_sel__pfp_dma_data__dst_addr_using_l2))),
        "MEC and PFP dst sel dma_data enumerations don't match!");

    static_assert(
        ((static_cast<uint32>(src_sel__mec_dma_data__src_addr_using_sas)  ==
          static_cast<uint32>(src_sel__pfp_dma_data__src_addr_using_sas)) &&
         (static_cast<uint32>(src_sel__mec_dma_data__data)                ==
          static_cast<uint32>(src_sel__pfp_dma_data__data))               &&
         (static_cast<uint32>(src_sel__mec_dma_data__src_addr_using_l2)   ==
          static_cast<uint32>(src_sel__pfp_dma_data__src_addr_using_l2))),
        "MEC and PFP src sel dma_data enumerations don't match!");

    static_assert(PM4_PFP_DMA_DATA_SIZEDW__CORE == PM4_ME_DMA_DATA_SIZEDW__CORE,
                  "PFP, ME and MEC versions of the DMA_DATA packet are not the same size!");

    // The "byte_count" field only has 26 bits (numBytes must be less than 64MB).
    PAL_ASSERT(dmaDataInfo.numBytes < (1 << 26));

    constexpr uint32 PacketSize = PM4_PFP_DMA_DATA_SIZEDW__CORE;
    auto*const       pPacket    = static_cast<PM4_PFP_DMA_DATA*>(pBuffer);
    PM4_PFP_DMA_DATA packet     = { 0 };

    packet.ordinal1.header.u32All        =
        (Type3Header(IT_DMA_DATA, PacketSize, false, ShaderGraphics, dmaDataInfo.predicate)).u32All;
    packet.ordinal2.u32All               = 0;
    packet.ordinal2.bitfields.engine_sel = static_cast<PFP_DMA_DATA_engine_sel_enum>(dmaDataInfo.usePfp
                                              ? static_cast<uint32>(engine_sel__pfp_dma_data__prefetch_parser)
                                              : static_cast<uint32>(engine_sel__me_dma_data__micro_engine));
    packet.ordinal2.bitfields.dst_sel    = dmaDataInfo.dstSel;
    packet.ordinal2.bitfields.src_sel    = dmaDataInfo.srcSel;
    packet.ordinal2.bitfields.cp_sync    = (dmaDataInfo.sync ? 1 : 0);

    if (dmaDataInfo.srcSel == src_sel__pfp_dma_data__data)
    {
        packet.ordinal3.src_addr_lo_or_data = dmaDataInfo.srcData;
        packet.ordinal4.src_addr_hi         = 0; // ignored for data
    }
    else if (indirectAddress)
    {
        packet.ordinal2.bitfields.src_indirect = 1;
        packet.ordinal2.bitfields.dst_indirect = 1;
        packet.ordinal3.src_addr_lo_or_data    = dmaDataInfo.srcOffset;
        packet.ordinal4.src_addr_hi            = 0; // ignored for data
    }
    else
    {
        packet.ordinal3.src_addr_lo_or_data = LowPart(dmaDataInfo.srcAddr);
        packet.ordinal4.src_addr_hi         = HighPart(dmaDataInfo.srcAddr);
    }

    packet.ordinal5.dst_addr_lo         = LowPart(dmaDataInfo.dstAddr);
    packet.ordinal6.dst_addr_hi         = HighPart(dmaDataInfo.dstAddr);
    if (indirectAddress)
    {
        packet.ordinal5.dst_addr_lo                      = dmaDataInfo.dstOffset;
        packet.ordinal6.dst_addr_hi                      = 0; // ignored for data
    }

    packet.ordinal7.u32All               = 0;
    packet.ordinal7.bitfields.byte_count = dmaDataInfo.numBytes;
    packet.ordinal7.bitfields.sas        = dmaDataInfo.srcAddrSpace;
    packet.ordinal7.bitfields.das        = dmaDataInfo.dstAddrSpace;
    packet.ordinal7.bitfields.raw_wait   = (dmaDataInfo.rawWait ? 1 : 0);
    packet.ordinal7.bitfields.dis_wc     = (dmaDataInfo.disWc   ? 1 : 0);

    *pPacket = packet;

    return PacketSize;
}

template size_t CmdUtil::BuildDmaData<true>(const DmaDataInfo& dmaDataInfo, void* pBuffer);
template size_t CmdUtil::BuildDmaData<false>(const DmaDataInfo& dmaDataInfo, void* pBuffer);

// =====================================================================================================================
// Builds a PM4 command to stall the CP ME until the CP's DMA engine has finished all previous DMA_DATA commands.
// Returns the size of the PM4 command written, in DWORDs.
size_t CmdUtil::BuildWaitDmaData(
    void* pBuffer) // [out] Build the PM4 packet in this buffer.
{
    // The most efficient way to do this is to issue a dummy DMA that copies zero bytes.
    // The DMA engine will see that there's no work to do and skip this DMA request, however, the ME microcode will
    // see the sync flag and still wait for all DMAs to complete.
    DmaDataInfo dmaDataInfo = {};
    dmaDataInfo.dstSel   = dst_sel__pfp_dma_data__dst_nowhere;
    dmaDataInfo.srcSel   = src_sel__pfp_dma_data__src_addr_using_sas;
    dmaDataInfo.dstAddr  = 0;
    dmaDataInfo.srcAddr  = 0;
    dmaDataInfo.numBytes = 0;
    dmaDataInfo.sync     = true;
    dmaDataInfo.usePfp   = false;

    return BuildDmaData<false>(dmaDataInfo, pBuffer);
}

// =====================================================================================================================
// Builds a PM4 packet which sets a sequence of context registers starting with startRegAddr and ending with endRegAddr
// (inclusive). All context registers are for graphics. Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildSetSeqContextRegs(
    uint32 startRegAddr,
    uint32 endRegAddr,
    void*  pBuffer) // [out] Build the PM4 packet in this buffer.
{
    PAL_ASSERT(IsContextReg(startRegAddr) && IsContextReg(endRegAddr) && (endRegAddr >= startRegAddr));

    static_assert(PM4_PFP_SET_CONTEXT_REG_SIZEDW__CORE == PM4_ME_SET_CONTEXT_REG_SIZEDW__CORE,
                  "PFP and ME SET_CONTEXT_REG size don't match!");

    const uint32 packetSize  = SetSeqContextRegsSizeDwords(startRegAddr, endRegAddr);
    auto*const   pPacket     = static_cast<PM4_PFP_SET_CONTEXT_REG*>(pBuffer);

    PM4_PFP_TYPE_3_HEADER header;
    header.u32All            = (Type3Header(IT_SET_CONTEXT_REG, packetSize)).u32All;
    pPacket->ordinal1.header = header;
    pPacket->ordinal2.u32All = startRegAddr - CONTEXT_SPACE_START;

    return packetSize;
}

// =====================================================================================================================
// Builds a PM4 packet which sets a sequence of Graphics SH registers starting with startRegAddr and ending with
// endRegAddr (inclusive). Returns the size of the PM4 command assembled, in DWORDs.
template <Pm4ShaderType ShaderType>
size_t CmdUtil::BuildSetSeqShRegs(
    uint32 startRegAddr,
    uint32 endRegAddr,
    void*  pBuffer) // [out] Build the PM4 packet in this buffer.
{
    PAL_ASSERT(IsShReg(startRegAddr) && IsShReg(endRegAddr) && (endRegAddr >= startRegAddr));

    const uint32 packetSize  = SetSeqShRegsSizeDwords(startRegAddr, endRegAddr);
    auto*const   pPacket     = static_cast<PM4_PFP_SET_SH_REG*>(pBuffer);

    PM4_PFP_TYPE_3_HEADER header;
    header.u32All            = (Type3Header(IT_SET_SH_REG, packetSize, false, ShaderType)).u32All;
    pPacket->ordinal1.header = header;
    pPacket->ordinal2.u32All = startRegAddr - PERSISTENT_SPACE_START;

    return packetSize;
}

template size_t CmdUtil::BuildSetSeqShRegs<ShaderGraphics>(uint32, uint32, void*);
template size_t CmdUtil::BuildSetSeqShRegs<ShaderCompute >(uint32, uint32, void*);

// =====================================================================================================================
// Builds a REWIND packet for telling compute queues to reload the command buffer data after this packet. Returns the
// size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildRewind(
    bool  offloadEnable,
    bool  valid,
    void* pBuffer)
{
    // This packet in PAL is only supported on compute queues.
    // The packet is supported on the PFP engine (PM4_PFP_REWIND) but offload_enable is not defined for PFP.
    constexpr size_t PacketSize = PM4_MEC_REWIND_SIZEDW__CORE;
    PM4_MEC_REWIND   packet     = {};

    packet.ordinal1.header.u32All            = (Type3Header(IT_REWIND, PacketSize, false, ShaderCompute)).u32All;
    packet.ordinal2.bitfields.offload_enable = offloadEnable;
    packet.ordinal2.bitfields.valid          = valid;

    static_assert(PacketSize * sizeof(uint32) == sizeof(packet), "");
    memcpy(pBuffer, &packet, sizeof(packet));
    return PacketSize;
}

// =====================================================================================================================
// Translates between the API compare func and the WaitRegMem comparison enumerations.
ME_WAIT_REG_MEM_function_enum CmdUtil::WaitRegMemFunc(
    CompareFunc compareFunc)
{
    constexpr ME_WAIT_REG_MEM_function_enum XlateCompareFunc[]=
    {
        function__me_wait_reg_mem__always_pass, // Never, not supported need to define something here
        function__me_wait_reg_mem__less_than_ref_value,
        function__me_wait_reg_mem__equal_to_the_reference_value,
        function__me_wait_reg_mem__less_than_equal_to_the_ref_value,
        function__me_wait_reg_mem__greater_than_reference_value,
        function__me_wait_reg_mem__not_equal_reference_value,
        function__me_wait_reg_mem__greater_than_or_equal_reference_value,
        function__me_wait_reg_mem__always_pass
    };

    const uint32  compareFunc32 = static_cast<uint32>(compareFunc);

    PAL_ASSERT(compareFunc != CompareFunc::Never);
    PAL_ASSERT(compareFunc32 < ArrayLen(XlateCompareFunc));

    return XlateCompareFunc[compareFunc32];
}

// =====================================================================================================================
// Builds a SET_BASE packet.  Returns the number of DWORDs taken by this packet.
template <Pm4ShaderType ShaderType>
size_t CmdUtil::BuildSetBase(
    gpusize                      address,
    PFP_SET_BASE_base_index_enum baseIndex,
    void*                        pBuffer)
{
    constexpr uint32 PacketSize = PM4_PFP_SET_BASE_SIZEDW__CORE;
    PM4_PFP_SET_BASE packet = {};

    packet.ordinal1.header.u32All        = (Type3Header(IT_SET_BASE, PacketSize, false, ShaderType)).u32All;
    packet.ordinal2.bitfields.base_index = baseIndex;
    packet.ordinal3.u32All               = LowPart(address);
    packet.ordinal4.address_hi           = HighPart(address);

    // Make sure our address was aligned properly
    PAL_ASSERT(packet.ordinal3.bitfields.reserved1 == 0);

    // For EI global spill buffer, requires base address to be aligned with EiSpillTblStrideAlignmentBytes.
    PAL_ASSERT((baseIndex != base_index__pfp_set_base__execute_indirect_v2) ||
               IsPow2Aligned(address, EiSpillTblStrideAlignmentBytes));

    static_assert(PacketSize * sizeof(uint32) == sizeof(packet), "");
    memcpy(pBuffer, &packet, sizeof(packet));
    return PacketSize;
}

template size_t CmdUtil::BuildSetBase<ShaderGraphics>(gpusize, PFP_SET_BASE_base_index_enum, void*);
template size_t CmdUtil::BuildSetBase<ShaderCompute >(gpusize, PFP_SET_BASE_base_index_enum, void*);

// =====================================================================================================================
// True if the specified atomic operation acts on 32-bit values.
static bool Is32BitAtomicOp(
    AtomicOp atomicOp)
{
    // AddInt64 is the first 64-bit operation.
    return (static_cast<uint32>(atomicOp) < static_cast<uint32>(AtomicOp::AddInt64));
}

// =====================================================================================================================
// Builds an ATOMIC_MEM packet. The caller should make sure that atomicOp is valid. This method assumes that pPacket has
// been initialized to zeros. Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildAtomicMem(
    AtomicOp atomicOp,
    gpusize  dstMemAddr,
    uint64   srcData,    // Constant operand for the atomic operation.
    void*    pBuffer)    // [out] Build the PM4 packet in this buffer.
{
    // Lookup table for converting a AtomicOp index into a TC_OP on Gfx9 hardware.
    constexpr TC_OP AtomicOpConversionTable[] =
    {
        TC_OP_ATOMIC_ADD_RTN_32,  // AddInt32
        TC_OP_ATOMIC_SUB_RTN_32,  // SubInt32
        TC_OP_ATOMIC_UMIN_RTN_32, // MinUint32
        TC_OP_ATOMIC_UMAX_RTN_32, // MaxUint32
        TC_OP_ATOMIC_SMIN_RTN_32, // MinSint32
        TC_OP_ATOMIC_SMAX_RTN_32, // MaxSing32
        TC_OP_ATOMIC_AND_RTN_32,  // AndInt32
        TC_OP_ATOMIC_OR_RTN_32,   // OrInt32
        TC_OP_ATOMIC_XOR_RTN_32,  // XorInt32
        TC_OP_ATOMIC_INC_RTN_32,  // IncUint32
        TC_OP_ATOMIC_DEC_RTN_32,  // DecUint32
        TC_OP_ATOMIC_ADD_RTN_64,  // AddInt64
        TC_OP_ATOMIC_SUB_RTN_64,  // SubInt64
        TC_OP_ATOMIC_UMIN_RTN_64, // MinUint64
        TC_OP_ATOMIC_UMAX_RTN_64, // MaxUint64
        TC_OP_ATOMIC_SMIN_RTN_64, // MinSint64
        TC_OP_ATOMIC_SMAX_RTN_64, // MaxSint64
        TC_OP_ATOMIC_AND_RTN_64,  // AndInt64
        TC_OP_ATOMIC_OR_RTN_64,   // OrInt64
        TC_OP_ATOMIC_XOR_RTN_64,  // XorInt64
        TC_OP_ATOMIC_INC_RTN_64,  // IncUint64
        TC_OP_ATOMIC_DEC_RTN_64,  // DecUint64
    };

    // Size of the AtomicOp conversion table, in entries.
    constexpr size_t AtomicOpConversionTableSize = ArrayLen(AtomicOpConversionTable);

    // The AtomicOp table should contain one entry for each AtomicOp.
    static_assert((AtomicOpConversionTableSize == static_cast<size_t>(AtomicOp::Count)),
                  "AtomicOp conversion table has too many/few entries");

    static_assert(PM4_ME_ATOMIC_MEM_SIZEDW__CORE == PM4_MEC_ATOMIC_MEM_SIZEDW__CORE,
                  "Atomic Mem packets don't match between ME and MEC!");

    static_assert(((static_cast<uint32>(command__me_atomic_mem__single_pass_atomic)           ==
                    static_cast<uint32>(command__mec_atomic_mem__single_pass_atomic))         &&
                   (static_cast<uint32>(command__me_atomic_mem__loop_until_compare_satisfied) ==
                    static_cast<uint32>(command__mec_atomic_mem__loop_until_compare_satisfied))),
                  "Atomic Mem command enum is different between ME and MEC!");

    // The destination address must be aligned to the size of the operands.
    PAL_ASSERT((dstMemAddr != 0) && IsPow2Aligned(dstMemAddr, (Is32BitAtomicOp(atomicOp) ? 4 : 8)));

    constexpr uint32 PacketSize = AtomicMemSizeDwords;
    PM4_ME_ATOMIC_MEM packet = {};

    packet.ordinal1.header                 = Type3Header(IT_ATOMIC_MEM, PacketSize);
    packet.ordinal2.bitfields.atomic       = static_cast<ME_ATOMIC_MEM_atomic_enum>
                                                (AtomicOpConversionTable[static_cast<uint32>(atomicOp)]);
    packet.ordinal2.bitfields.command      = command__me_atomic_mem__single_pass_atomic;
    packet.ordinal2.bitfields.temporal     = temporal__me_atomic_mem__rt;
    packet.ordinal2.bitfields.engine_sel   = engine_sel__me_atomic_mem__micro_engine;
    packet.ordinal3.addr_lo                = LowPart(dstMemAddr);
    packet.ordinal4.addr_hi                = HighPart(dstMemAddr);
    packet.ordinal5.src_data_lo            = LowPart(srcData);
    packet.ordinal6.src_data_hi            = HighPart(srcData);

    static_assert(PacketSize * sizeof(uint32) == sizeof(packet), "");
    memcpy(pBuffer, &packet, sizeof(packet));
    return PacketSize;
}

// =====================================================================================================================
// Generates a basic "COND_INDIRECT_BUFFER" packet.  The branch locations must be filled in later.  Returns the
// size, in DWORDs, of the generated packet.
size_t CmdUtil::BuildCondIndirectBuffer(
    CompareFunc compareFunc,
    gpusize     compareGpuAddr,
    uint64      data,
    uint64      mask,
    void*       pBuffer)
{
    static_assert(PM4_PFP_COND_INDIRECT_BUFFER_SIZEDW__CORE == PM4_MEC_COND_INDIRECT_BUFFER_SIZEDW__CORE,
                  "Conditional indirect buffer packets don't match between GFX and compute!");

    // The CP doesn't implement a "never" compare function.  It is the caller's responsibility to detect
    // this case and work around it.  The "funcTranslation" table defines an entry for "never" only to
    // make indexing into it easy.
    PAL_ASSERT(compareFunc != CompareFunc::Never);

    constexpr static PFP_COND_INDIRECT_BUFFER_function_enum  FuncTranslation[]=
    {
        function__pfp_cond_indirect_buffer__always_pass,                           // Never
        function__pfp_cond_indirect_buffer__less_than_ref_value,                   // Less
        function__pfp_cond_indirect_buffer__equal_to_the_reference_value,          // Equal
        function__pfp_cond_indirect_buffer__less_than_equal_to_the_ref_value,      // LessEqual
        function__pfp_cond_indirect_buffer__greater_than_reference_value,          // Greater
        function__pfp_cond_indirect_buffer__not_equal_reference_value,             // NotEqual
        function__pfp_cond_indirect_buffer__greater_than_or_equal_reference_value, // GreaterEqual
        function__pfp_cond_indirect_buffer__always_pass                            // _Always
    };

    constexpr uint32 PacketSize = PM4_PFP_COND_INDIRECT_BUFFER_SIZEDW__CORE;
    PM4_PFP_COND_INDIRECT_BUFFER packet = {};

    packet.ordinal1.header.u32All      = (Type3Header(IT_COND_INDIRECT_BUFFER, PacketSize)).u32All;
    packet.ordinal2.bitfields.function = FuncTranslation[static_cast<uint32>(compareFunc)];

    // We always implement both a "then" and an "else" clause
    packet.ordinal2.bitfields.mode = mode__pfp_cond_indirect_buffer__if_then_else;

    // Make sure our comparison address is aligned properly
    // Note that the packet definition makes it seem like 8 byte alignment is required, but only 4 is actually
    // necessary.
    PAL_ASSERT(IsPow2Aligned(compareGpuAddr, 4));
    packet.ordinal3.u32All          = LowPart(compareGpuAddr);
    packet.ordinal4.compare_addr_hi = HighPart(compareGpuAddr);

    packet.ordinal5.mask_lo      = LowPart(mask);
    packet.ordinal6.mask_hi      = HighPart(mask);
    packet.ordinal7.reference_lo = LowPart(data);
    packet.ordinal8.reference_hi = HighPart(data);

    static_assert(PacketSize * sizeof(uint32) == sizeof(packet), "");
    memcpy(pBuffer, &packet, sizeof(packet));
    // Size and locations of the IB are not yet known, will be patched later.

    return PacketSize;
}

// =====================================================================================================================
// Generates a basic "COND_EXEC" packet. Returns the size, in DWORDs, of the generated packet.
size_t CmdUtil::BuildCondExec(
    gpusize gpuVirtAddr,
    uint32  sizeInDwords,
    void*   pBuffer)
{
    static_assert(PM4_PFP_COND_EXEC_SIZEDW__CORE == PM4_MEC_COND_EXEC_SIZEDW__CORE,
                  "Conditional execute packets don't match between GFX and compute!");

    constexpr uint32 PacketSize = PM4_MEC_COND_EXEC_SIZEDW__CORE;
    PM4_MEC_COND_EXEC packet = {};

    packet.ordinal1.header.u32All        = (Type3Header(IT_COND_EXEC, PacketSize)).u32All;
    packet.ordinal2.u32All               = LowPart(gpuVirtAddr);
    PAL_ASSERT(packet.ordinal2.bitfields.reserved1 == 0);
    packet.ordinal3.addr_hi              = HighPart(gpuVirtAddr);
    packet.ordinal5.bitfields.exec_count = sizeInDwords;

    static_assert(PacketSize * sizeof(uint32) == sizeof(packet), "");
    memcpy(pBuffer, &packet, sizeof(packet));
    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 packet which issues a "prime UtcL2" command into the given command stream. Returns the size of the PM4
// command assembled, in DWORDs.
size_t CmdUtil::BuildPrimeUtcL2(
    gpusize gpuAddr,
    uint32  cachePerm,      // Bitmask of permissions. Bits: 0 - read, 1 - write, 2 - execute
    uint32  primeMode,      // XXX_PRIME_UTCL2_prime_mode_enum
    uint32  engineSel,      // XXX_PRIME_UTCL2_engine_sel_enum
    size_t  requestedPages, // Number of 4KB pages to prefetch.
    void*   pBuffer)
{
    static_assert((PM4_PFP_PRIME_UTCL2_SIZEDW__CORE == PM4_MEC_PRIME_UTCL2_SIZEDW__CORE),
                  "PRIME_UTCL2 packet is different between PFP and MEC!");

    static_assert(((static_cast<uint32>(prime_mode__pfp_prime_utcl2__dont_wait_for_xack)  ==
                    static_cast<uint32>(prime_mode__mec_prime_utcl2__dont_wait_for_xack)) &&
                   (static_cast<uint32>(prime_mode__pfp_prime_utcl2__wait_for_xack)       ==
                    static_cast<uint32>(prime_mode__mec_prime_utcl2__wait_for_xack))),
                   "Prime mode enum is different between PFP and MEC!");

    constexpr uint32 PacketSize = PM4_PFP_PRIME_UTCL2_SIZEDW__CORE;

    PM4_PFP_PRIME_UTCL2 packet = {};

    packet.ordinal1.header.u32All              = (Type3Header(IT_PRIME_UTCL2, PacketSize)).u32All;
    packet.ordinal2.bitfields.cache_perm       = cachePerm;
    packet.ordinal2.bitfields.prime_mode       = static_cast<PFP_PRIME_UTCL2_prime_mode_enum>(primeMode);
    packet.ordinal2.bitfields.engine_sel       = static_cast<PFP_PRIME_UTCL2_engine_sel_enum>(engineSel);
    PAL_ASSERT(packet.ordinal2.bitfields.reserved1 == 0);
    packet.ordinal3.addr_lo                    = LowPart(gpuAddr);
    // Address must be 4KB aligned.
    PAL_ASSERT((packet.ordinal3.addr_lo & (PrimeUtcL2MemAlignment - 1)) == 0);
    packet.ordinal4.addr_hi                    = HighPart(gpuAddr);
    packet.ordinal5.bitfields.requested_pages  = static_cast<uint32>(requestedPages);
    PAL_ASSERT(packet.ordinal5.bitfields.reserved1 == 0);

    static_assert(PacketSize * sizeof(uint32) == sizeof(packet), "");
    memcpy(pBuffer, &packet, sizeof(packet));
    return PacketSize;
}

// =====================================================================================================================
size_t CmdUtil::BuildPrimeGpuCaches(
    const PrimeGpuCacheRange& primeGpuCacheRange,
    gpusize                   clampSize,
    EngineType                engineType,
    void*                     pBuffer)
{
    gpusize prefetchSize = primeGpuCacheRange.size;

    if (clampSize != 0)
    {
        prefetchSize = Min(prefetchSize, clampSize);
    }

    size_t packetSize = 0;

    // Examine the usageFlags to check if GL2 is relevant to that usage's data path.
    if ((TestAnyFlagSet(primeGpuCacheRange.usageMask, CoherCpu | CoherMemory) == false) &&
        (primeGpuCacheRange.addrTranslationOnly == false))
    {
        PAL_ASSERT(prefetchSize <= UINT32_MAX);

        // DMA DATA to "nowhere" should be performed, ideally using the PFP.
        DmaDataInfo dmaDataInfo  = { };
        dmaDataInfo.dstAddr      = 0;
        dmaDataInfo.dstAddrSpace = das__pfp_dma_data__memory;
        dmaDataInfo.dstSel       = dst_sel__pfp_dma_data__dst_nowhere;
        dmaDataInfo.srcAddr      = primeGpuCacheRange.gpuVirtAddr;
        dmaDataInfo.srcAddrSpace = sas__pfp_dma_data__memory;
        // CP headers haven't updated this since Gfx11, so L2 here actually refers to MALL.
        dmaDataInfo.srcSel       = src_sel__pfp_dma_data__src_addr_using_l2;
        dmaDataInfo.numBytes     = static_cast<uint32>(prefetchSize);
        dmaDataInfo.usePfp       = (engineType == EngineTypeUniversal);
        dmaDataInfo.disWc        = true;

        packetSize = BuildDmaData<false>(dmaDataInfo, pBuffer);
    }
    else
    {
        // A PRIME_UTCL2 should be performed.
        const gpusize firstPage = Pow2AlignDown(primeGpuCacheRange.gpuVirtAddr, PrimeUtcL2MemAlignment);
        const gpusize lastPage  = Pow2AlignDown(primeGpuCacheRange.gpuVirtAddr + prefetchSize - 1,
                                                PrimeUtcL2MemAlignment);

        const size_t  numPages = 1 + static_cast<size_t>((lastPage - firstPage) / PrimeUtcL2MemAlignment);

        packetSize = BuildPrimeUtcL2(firstPage,
                                     2,
                                     prime_mode__pfp_prime_utcl2__dont_wait_for_xack,
                                     engine_sel__pfp_prime_utcl2__prefetch_parser,
                                     numPages,
                                     pBuffer);
    }

    return packetSize;
}

// =====================================================================================================================
template <Pm4ShaderType ShaderType>
size_t CmdUtil::BuildSetSeqShRegsIndex(
    uint32                          startRegAddr,
    uint32                          endRegAddr,
    PFP_SET_SH_REG_INDEX_index_enum index,
    void*                           pBuffer)
{
    const size_t packetSize = ShRegIndexSizeDwords + endRegAddr - startRegAddr + 1;

    PM4_PFP_SET_SH_REG_INDEX packet      = {};
    packet.ordinal1.header.u32All        = (Type3Header(IT_SET_SH_REG_INDEX,
                                                        static_cast<uint32>(packetSize),
                                                        false,
                                                        ShaderType)).u32All;
    packet.ordinal2.bitfields.index      = index;
    packet.ordinal2.bitfields.reg_offset = startRegAddr - PERSISTENT_SPACE_START;

    static_assert(ShRegIndexSizeDwords * sizeof(uint32) == sizeof(packet), "");
    memcpy(pBuffer, &packet, sizeof(packet));

    return packetSize;
}

template size_t CmdUtil::BuildSetSeqShRegsIndex<ShaderGraphics>(uint32, uint32, PFP_SET_SH_REG_INDEX_index_enum, void*);
template size_t CmdUtil::BuildSetSeqShRegsIndex<ShaderCompute >(uint32, uint32, PFP_SET_SH_REG_INDEX_index_enum, void*);

// =====================================================================================================================
// Builds a SET_PREDICATION packet. Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildSetPredication(
    gpusize       gpuVirtAddr,
    bool          predicationBool,    // Controls the polarity of the predication test. E.g., for occlusion predicates,
                                      // true indicates to draw if any pixels passed the Z-test while false indicates
                                      // to draw if no pixels passed the Z-test.
    bool          occlusionHint,      // Controls whether the hardware should wait for all ZPASS data to be written by
                                      // the DB's before proceeding. True chooses to assume that the draw should not be
                                      // skipped if the ZPASS data is not ready yet, false chooses to wait until all
                                      // ZPASS data is ready.
    PredicateType predType,
    bool          continuePredicate,  // Controls how data is accumulated across cmd buffer boundaries. True indicates
                                      // that this predicate is a continuation of the previous one, accumulating data
                                      // between them.
    void*         pBuffer)
{
    static_assert(
        (static_cast<PFP_SET_PREDICATION_pred_op_enum>(PredicateType::Zpass)     ==
         pred_op__pfp_set_predication__set_zpass_predicate)     &&
        (static_cast<PFP_SET_PREDICATION_pred_op_enum>(PredicateType::PrimCount) ==
         pred_op__pfp_set_predication__set_primcount_predicate) &&
        (static_cast<PFP_SET_PREDICATION_pred_op_enum>(PredicateType::Boolean64) ==
         pred_op__pfp_set_predication__DX12)                    &&
        (static_cast<PFP_SET_PREDICATION_pred_op_enum>(PredicateType::Boolean32) ==
         pred_op__pfp_set_predication__Vulkan),
        "Unexpected values for the PredicateType enum.");

    constexpr uint32 PacketSize = PM4_PFP_SET_PREDICATION_SIZEDW__CORE;
    PM4_PFP_SET_PREDICATION packet = {};

    // The predication memory address cannot be wider than 40 bits.
    PAL_ASSERT(gpuVirtAddr <= ((1uLL << 40) - 1));

    // Verify the address meets the CP's alignment requirement for the predicate type.
    if (predType == PredicateType::Boolean32)
    {
        PAL_ASSERT(IsPow2Aligned(gpuVirtAddr, 4));
    }
    else if (predType == PredicateType::Boolean64)
    {
        PAL_ASSERT(IsPow2Aligned(gpuVirtAddr, 8));
    }
    else
    {
        PAL_ASSERT(IsPow2Aligned(gpuVirtAddr, 16));
    }

    // The predicate type has to be valid.
    PAL_ASSERT(predType <= PredicateType::Boolean32);

    packet.ordinal1.header.u32All = (Type3Header(IT_SET_PREDICATION, PacketSize)).u32All;
    packet.ordinal3.u32All        = LowPart(gpuVirtAddr);
    packet.ordinal4.start_addr_hi = (HighPart(gpuVirtAddr) & 0xFF);

    const bool continueSupported = (predType == PredicateType::Zpass) || (predType == PredicateType::PrimCount);
    PAL_ASSERT(continueSupported || (continuePredicate == false));
    packet.ordinal2.bitfields.pred_bool    = (predicationBool
                                            ? pred_bool__pfp_set_predication__draw_if_visible_or_no_overflow
                                            : pred_bool__pfp_set_predication__draw_if_not_visible_or_overflow);
    packet.ordinal2.bitfields.hint         = (((predType == PredicateType::Zpass) && occlusionHint)
                                            ? hint__pfp_set_predication__draw_if_not_final_zpass_written
                                            : hint__pfp_set_predication__wait_until_final_zpass_written);
    packet.ordinal2.bitfields.pred_op      = static_cast<PFP_SET_PREDICATION_pred_op_enum>(predType);
    packet.ordinal2.bitfields.continue_bit = ((continueSupported && continuePredicate)
                                            ? continue_bit__pfp_set_predication__continue_set_predication
                                            : continue_bit__pfp_set_predication__new_set_predication);

    static_assert(PacketSize * sizeof(uint32) == sizeof(packet), "");
    memcpy(pBuffer, &packet, sizeof(packet));
    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 command to add the differences in the given set of ZPASS begin and end counts. Returns the size of the
// PM4 command built, in DWORDs.
size_t CmdUtil::BuildOcclusionQuery(
    gpusize queryMemAddr, // DB0 start address, 16-byte aligned
    gpusize dstMemAddr,   // Accumulated ZPASS count destination, 4-byte aligned
    void*   pBuffer)      // [out] Build the PM4 packet in this buffer.
{
    // Note that queryAddr means "zpass query sum address" and not "query pool counters address". Instead startAddr is
    // the "query pool counters address".
    constexpr size_t PacketSize = PM4_PFP_OCCLUSION_QUERY_SIZEDW__CORE;
    PM4_PFP_OCCLUSION_QUERY packet = {};

    packet.ordinal1.header.u32All = (Type3Header(IT_OCCLUSION_QUERY, PacketSize)).u32All;
    packet.ordinal2.u32All        = LowPart(queryMemAddr);
    packet.ordinal3.start_addr_hi = HighPart(queryMemAddr);
    packet.ordinal4.u32All        = LowPart(dstMemAddr);
    packet.ordinal5.query_addr_hi = HighPart(dstMemAddr);

    // The query address should be 16-byte aligned.
    PAL_ASSERT(IsPow2Aligned(queryMemAddr, 16));

    // The destination address should be 4-byte aligned.
    PAL_ASSERT(IsPow2Aligned(dstMemAddr, 4));

    static_assert(PacketSize * sizeof(uint32) == sizeof(packet), "");
    memcpy(pBuffer, &packet, sizeof(packet));
    return PacketSize;
}

// =====================================================================================================================
// Builds execute indirect V2 packet for the PFP + ME engine. Returns the size of the PM4 command assembled, in DWORDs.
// This function only supports Universal Queue usage.
size_t CmdUtil::BuildExecuteIndirectV2Gfx(
    Pm4Predicate                     predicate,
    const bool                       isGfx,
    const ExecuteIndirectPacketInfo& packetInfo,
    ExecuteIndirectMeta*             pMeta,
    void*                            pBuffer)
{
    const Pm4ShaderType shaderType = isGfx ? ShaderGraphics : ShaderCompute;
    ExecuteIndirectMetaData* pMetaData = pMeta->GetMetaData();

    PM4_PFP_EXECUTE_INDIRECT_V2 packet = {};

    constexpr uint32 PacketDwSize = PM4_PFP_EXECUTE_INDIRECT_V2_SIZEDW__CORE;

    packet.ordinal2.bitfields.count_indirect_enable      = (packetInfo.countBufferAddr != 0);
    packet.ordinal2.bitfields.userdata_dw_count          = pMetaData->userDataDwCount;
    packet.ordinal2.bitfields.command_index_enable       = pMetaData->commandIndexEnable;
    packet.ordinal2.bitfields.init_mem_copy_count        = pMetaData->initMemCopy.count;
    packet.ordinal2.bitfields.build_srd_count            = pMetaData->buildSrd.count;
    packet.ordinal2.bitfields.update_mem_copy_count      = pMetaData->updateMemCopy.count;
    packet.ordinal2.bitfields.operation                  =
        static_cast<PFP_EXECUTE_INDIRECT_V2_operation_enum>(pMetaData->opType);
    packet.ordinal2.bitfields.fetch_index_attributes     = pMetaData->fetchIndexAttributes;
    packet.ordinal2.bitfields.userdata_scatter_mode      =
        static_cast<PFP_EXECUTE_INDIRECT_V2_userdata_scatter_mode_enum>(pMetaData->userDataScatterMode);
    packet.ordinal2.bitfields.vertex_offset_mode_enabled = pMetaData->vertexOffsetModeEnable;
    packet.ordinal2.bitfields.vertex_bounds_check_enable = pMetaData->vertexBoundsCheckEnable;
    packet.ordinal2.bitfields.thread_trace_enable        = pMetaData->threadTraceEnable;
    packet.ordinal3.bitfields.count_addr_lo              = LowPart(packetInfo.countBufferAddr) >> 2;
    packet.ordinal4.bitfields.count_addr_hi              = HighPart(packetInfo.countBufferAddr);
    packet.ordinal5.max_count                            = packetInfo.maxCount;
    packet.ordinal6.stride                               = packetInfo.argumentBufferStrideBytes;
    packet.ordinal7.bitfields.data_addr_lo               = LowPart(packetInfo.argumentBufferAddr) >> 2;
    packet.ordinal8.bitfields.data_addr_hi               = HighPart(packetInfo.argumentBufferAddr);
    packet.ordinal8.bitfields.index_attributes_offset    = pMetaData->indexAttributesOffset;
    if (packetInfo.vbTableRegOffset != 0)
    {
        packet.ordinal9.bitfields.userdata_gfx_register        = packetInfo.vbTableRegOffset;
        packet.ordinal2.bitfields.userdata_gfx_register_enable = 1;
    }
    packet.ordinal9.bitfields.userdata_offset            = pMetaData->userDataOffset;
    packet.ordinal10.bitfields.spill_table_addr_lo       = LowPart(packetInfo.spillTableAddr) >> 2;
    packet.ordinal11.bitfields.spill_table_addr_hi       = HighPart(packetInfo.spillTableAddr);

    uint32 numSpillRegsActive = 0;
    if (packetInfo.spillTableAddr != 0)
    {
        if (isGfx)
        {
            const GraphicsUserDataLayout* pGfxUserData =
                static_cast<const GraphicsUserDataLayout*>(packetInfo.pUserDataLayout);

            // Graphics Registers are 8-bits wide.  We do the following ops to store up to 3 GraphicsRegs' data and
            // then extract it into the PM4 ordinal. Ordinal13 contains the regs for the 3 possible shader stages.
            if (pGfxUserData->GetSpillTable().regOffset0)
            {
                PAL_ASSERT(pGfxUserData->GetSpillTable().regOffset0 <= 0xFF);
                packet.ordinal13.bitfieldsA.spill_graphics_reg0 = pGfxUserData->GetSpillTable().regOffset0;
                numSpillRegsActive++;
            }
            if (pGfxUserData->GetSpillTable().regOffset1)
            {
                PAL_ASSERT(pGfxUserData->GetSpillTable().regOffset1 <= 0xFF);
                packet.ordinal13.bitfieldsA.spill_graphics_reg1 = pGfxUserData->GetSpillTable().regOffset1;
                numSpillRegsActive++;
            }
            if (pGfxUserData->GetSpillTable().regOffset2)
            {
                PAL_ASSERT(pGfxUserData->GetSpillTable().regOffset2 <= 0xFF);
                packet.ordinal13.bitfieldsA.spill_graphics_reg2 = pGfxUserData->GetSpillTable().regOffset2;
                numSpillRegsActive++;
            }

            packet.ordinal12.bitfields.vb_table_size = packetInfo.vbTableSizeDwords * sizeof(uint32);
        }
        else
        {
            const ComputeUserDataLayout* pCsUserData =
                static_cast<const ComputeUserDataLayout*>(packetInfo.pUserDataLayout);
            // Compute Registers are 16-bits wide with 10-bits of useful data. We do the following ops to store the
            // ComputeRegs' data and then extract it into the PM4 ordinal.
            if (pCsUserData->GetSpillTable().regOffset)
            {
                packet.ordinal13.bitfieldsB.spill_compute_reg0 = pCsUserData->GetSpillTable().regOffset;
                numSpillRegsActive++;
            }
        }

        packet.ordinal2.bitfields.num_spill_regs = numSpillRegsActive;

        PAL_ASSERT(IsPow2Aligned(packetInfo.spillTableStrideBytes, EiSpillTblStrideAlignmentBytes));
        packet.ordinal12.bitfields.spill_table_stride = packetInfo.spillTableStrideBytes;
    }

    uint32* pOut   = reinterpret_cast<uint32*>(pBuffer);
    uint32  offset = PacketDwSize;

    // As part of the ExecuteIndirectV2 PM4's function the CP performs the operation of copying over Spilled UserData
    // entries, adding SRDs for the VertexBuffer Data/Table into the reserved GlobalSpillBuffer and updating UserData
    // entries to mapped SGPRs. We update and append required information for these operations to the base PM4.
    if (pMetaData->initMemCopy.count != 0)
    {
        // 16 BitsPerComponent for RegPacked writing in initMemCpyCount, updateMemCpyCount and buildSrdCount structs.
        offset += ExecuteIndirectMeta::ExecuteIndirectWritePacked(&pOut[offset],
                                                                  16,
                                                                  pMetaData->initMemCopy.count,
                                                                  pMetaData->initMemCopy.srcOffsets,
                                                                  pMetaData->initMemCopy.dstOffsets,
                                                                  pMetaData->initMemCopy.sizes);
    }

    if (pMetaData->updateMemCopy.count != 0)
    {
        offset += ExecuteIndirectMeta::ExecuteIndirectWritePacked(&pOut[offset],
                                                                  16,
                                                                  pMetaData->updateMemCopy.count,
                                                                  pMetaData->updateMemCopy.srcOffsets,
                                                                  pMetaData->updateMemCopy.dstOffsets,
                                                                  pMetaData->updateMemCopy.sizes);
    }

    // SRD build, typically the VBTable.
    if (pMetaData->buildSrd.count != 0)
    {
        offset += ExecuteIndirectMeta::ExecuteIndirectWritePacked(&pOut[offset],
                                                                  16,
                                                                  pMetaData->buildSrd.count,
                                                                  pMetaData->buildSrd.srcOffsets,
                                                                  pMetaData->buildSrd.dstOffsets);
    }

    // UserDataEntries to be updated in Registers.
    if (pMetaData->userDataDwCount != 0)
    {
        uint32* pInputs[EiMaxStages] = {};
        static_assert((EiMaxStages == 3),"EiMaxStages != 3");

        const uint32 count = pMetaData->stageUsageCount;
        // For Graphics, pInputs[i]'s will store the address of modified UserData Entry array for each stage which have
        // up to 32 entries per active stage. eg. pInputs[0] for GS userData[0-31], pInputs[1] for PS userData[32-63].
        // Since userData[] marks every modified entry, it needs to stride by NumUserDataRegisters (32) here.
        // For Compute, only pInputs[0] will contain the address to the modified CS userDataEntry array.
        for (uint32 i = 0; i < count; i++)
        {
            pInputs[i] = &pMetaData->userData[i * NumUserDataRegisters];
        }
        offset += ExecuteIndirectMeta::ExecuteIndirectWritePacked(&pOut[offset],
                                                                  isGfx ? 8 : 16,
                                                                  pMetaData->userDataDwCount,
                                                                  pInputs[0],
                                                                  pInputs[1],
                                                                  pInputs[2]);
    }

    // Copy Op MetaData at an offset the base PM4.
    memcpy(&pOut[offset], pMeta->GetOp(), EiOpDwSize * sizeof(uint32));
    offset += EiOpDwSize;

    // Update header when we have final Packet+Op Dword size as offset.
    packet.ordinal1.header.u32All =
        (Type3Header(IT_EXECUTE_INDIRECT_V2, offset, true, shaderType, predicate)).u32All;

    memcpy(pBuffer, &packet, sizeof(packet));

    return offset;
}

// =====================================================================================================================
// Builds execute indirect V2 packet for the ACE engine. Returns the size of the PM4 command assembled, in DWORDs.
// This function only supports Compute Queue usage.
size_t CmdUtil::BuildExecuteIndirectV2Ace(
    Pm4Predicate                     predicate,
    const ExecuteIndirectPacketInfo& packetInfo,
    ExecuteIndirectMeta*             pMeta,
    void*                            pBuffer)
{
    ExecuteIndirectMetaData* pMetaData = pMeta->GetMetaData();
    PM4_MEC_EXECUTE_INDIRECT_V2 packet = {};

    constexpr uint32 PacketDwSize = PM4_MEC_EXECUTE_INDIRECT_V2_SIZEDW__CORE;

    packet.ordinal2.bitfields.count_indirect_enable  = (packetInfo.countBufferAddr != 0);
    packet.ordinal2.bitfields.command_index_enable   = pMetaData->commandIndexEnable;
    packet.ordinal2.bitfields.init_mem_copy_count    = pMetaData->initMemCopy.count;
    packet.ordinal2.bitfields.update_mem_copy_count  = pMetaData->updateMemCopy.count;
    packet.ordinal2.bitfields.operation              =
        static_cast<MEC_EXECUTE_INDIRECT_V2_operation_enum>(pMetaData->opType);
    packet.ordinal2.bitfields.userdata_scatter_mode  =
        static_cast<MEC_EXECUTE_INDIRECT_V2_userdata_scatter_mode_enum>(pMetaData->userDataScatterMode);
    packet.ordinal2.bitfields.thread_trace_enable    = pMetaData->threadTraceEnable;
    packet.ordinal3.bitfields.count_addr_lo          = LowPart(packetInfo.countBufferAddr) >> 2;
    packet.ordinal4.bitfields.count_addr_hi          = HighPart(packetInfo.countBufferAddr);
    packet.ordinal5.max_count                        = packetInfo.maxCount;
    packet.ordinal6.stride                           = packetInfo.argumentBufferStrideBytes;
    packet.ordinal7.bitfields.data_addr_lo           = LowPart(packetInfo.argumentBufferAddr) >> 2;
    packet.ordinal8.bitfields.data_addr_hi           = HighPart(packetInfo.argumentBufferAddr);
    packet.ordinal9.bitfields.userdata_offset        = pMetaData->userDataOffset;
    packet.ordinal10.bitfields.spill_table_addr_lo   = LowPart(packetInfo.spillTableAddr) >> 2;
    packet.ordinal11.bitfields.spill_table_addr_hi   = HighPart(packetInfo.spillTableAddr);

    uint32 numSpillRegsActive = 0;
    if (packetInfo.spillTableAddr != 0)
    {
        const ComputeUserDataLayout* pCsUserData =
            static_cast<const ComputeUserDataLayout*>(packetInfo.pUserDataLayout);
        // Compute Registers are 16-bits wide with 10-bits of useful data. We do the following ops to store the
        // ComputeRegs' data and then extract it into the PM4 ordinal.
        if (pCsUserData->GetSpillTable().regOffset)
        {
            packet.ordinal13.bitfields.spill_compute_reg0 = pCsUserData->GetSpillTable().regOffset;
            numSpillRegsActive++;
        }

        packet.ordinal2.bitfields.num_spill_regs = numSpillRegsActive;

        PAL_ASSERT(IsPow2Aligned(packetInfo.spillTableStrideBytes, EiSpillTblStrideAlignmentBytes));
        packet.ordinal12.bitfields.spill_table_stride = packetInfo.spillTableStrideBytes;
    }

    uint32* pOut = reinterpret_cast<uint32*>(pBuffer);

    uint32 offset = PacketDwSize;

    // Init and Update MemCopy are the CP MemCopy structs that decide slots on how to copy Spilled UserData Entries
    // from the ArgBuffer into the reserved queue specific VB+Spill Buffer.
    // 16 BitsPerComponent for RegPacked writing in initMemCpyCount, updateMemCpyCount and buildSrdCount structs.
    if (pMetaData->initMemCopy.count != 0)
    {
        offset += ExecuteIndirectMeta::ExecuteIndirectWritePacked(&pOut[offset],
                                                                  16,
                                                                  pMetaData->initMemCopy.count,
                                                                  pMetaData->initMemCopy.srcOffsets,
                                                                  pMetaData->initMemCopy.dstOffsets,
                                                                  pMetaData->initMemCopy.sizes);
    }

    if (pMetaData->updateMemCopy.count != 0)
    {
        offset += ExecuteIndirectMeta::ExecuteIndirectWritePacked(&pOut[offset],
                                                                  16,
                                                                  pMetaData->updateMemCopy.count,
                                                                  pMetaData->updateMemCopy.srcOffsets,
                                                                  pMetaData->updateMemCopy.dstOffsets,
                                                                  pMetaData->updateMemCopy.sizes);
    }

    // UserDataEntries to be updated in Registers.
    if (pMetaData->userDataDwCount != 0)
    {
        const uint32 userDataDwords = ExecuteIndirectMeta::AppendUserDataMec(&pOut[offset],
                                                                             pMetaData->userDataDwCount,
                                                                             &pMetaData->userData[0]);
        offset += userDataDwords;
        packet.ordinal2.bitfields.userdata_dw_count = userDataDwords;
    }

    // Copy Op MetaData at an offset to the base PM4.
    memcpy(&pOut[offset], pMeta->GetOp(), EiOpDwSize * sizeof(uint32));
    offset += EiOpDwSize;

    // Update header when we have final Packet+Op Dword size as offset.
    packet.ordinal1.header.u32All =
        (Type3Header(IT_EXECUTE_INDIRECT_V2, offset, false, ShaderCompute, predicate)).u32All;

    memcpy(pBuffer, &packet, sizeof(packet));

    return offset;
}

// =====================================================================================================================
// Builds a PERFMON_CONTROL packet. Returns the size of the PM4 command assembled, in DWORDs.
// This packet is to control Data Fabric (DF) perfmon events by writing the PerfMonCtlLo/Hi registers and is only
// supported on the graphics engine.
size_t CmdUtil::BuildPerfmonControl(
    uint32     perfMonCtlId,  // PerfMonCtl id to be configured (0-7)
    bool       enable,        // Perfmon enabling: 0=disable, 1=enable
    uint32     eventSelect,   // If enabling, the event selection to configure for this perfMonId
    uint32     eventUnitMask, // If enabling, this is event specific configuration data.
    void*      pBuffer)       // [out] Build the PM4 packet in this buffer.
{
    constexpr uint32       PacketSize = PM4_ME_PERFMON_CONTROL_SIZEDW__CORE;
    PM4_ME_PERFMON_CONTROL packet     = {};

    packet.ordinal1.header.u32All           = (Type3Header(IT_PERFMON_CONTROL, PacketSize)).u32All;
    packet.ordinal2.bitfields.pmc_id        = perfMonCtlId;
    packet.ordinal2.bitfields.pmc_en        = static_cast<ME_PERFMON_CONTROL_pmc_en_enum>(enable);
    packet.ordinal2.bitfields.pmc_unit_mask = eventUnitMask;
    packet.ordinal3.bitfields.pmc_event     = eventSelect;

    memcpy(pBuffer, &packet, PacketSize * sizeof(uint32));

    return PacketSize;
}

// Builds a LOAD_BUFFER_FILLED_SIZES packet. Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildLoadBufferFilledSizes(
    const gpusize  streamoutCtrlBuf,
    const gpusize* pStreamoutTargets,
    void*          pBuffer)
{
    PAL_ASSERT_MSG(MaxStreamOutTargets == 4,
                   "MaxStreamOutTargets is no longer 4 so we need to update the firmware packets!");
    constexpr uint32 PacketSize             = PM4_PFP_LOAD_BUFFER_FILLED_SIZES_SIZEDW__CORE;
    PM4_PFP_LOAD_BUFFER_FILLED_SIZES packet = {};

    packet.ordinal1.u32All                          = (Type3Header(IT_LOAD_BUFFER_FILLED_SIZE, PacketSize)).u32All;
    packet.ordinal2.bitfields.streamout_ctrl_buf_lo = LowPart(streamoutCtrlBuf) >> SoCtrlBufAlignShift;
    packet.ordinal3.streamout_ctrl_buf_hi           = HighPart(streamoutCtrlBuf);

    packet.ordinal4.bitfields.streamout_target0_lo  = LowPart(pStreamoutTargets[0]) >> SoTargetAlignShift;
    packet.ordinal5.streamout_target0_hi            = HighPart(pStreamoutTargets[0]);

    packet.ordinal6.bitfields.streamout_target1_lo  = LowPart(pStreamoutTargets[1]) >> SoTargetAlignShift;
    packet.ordinal7.streamout_target1_hi            = HighPart(pStreamoutTargets[1]);

    packet.ordinal8.bitfields.streamout_target2_lo  = LowPart(pStreamoutTargets[2]) >> SoTargetAlignShift;
    packet.ordinal9.streamout_target2_hi            = HighPart(pStreamoutTargets[2]);

    packet.ordinal10.bitfields.streamout_target3_lo = LowPart(pStreamoutTargets[3]) >> SoTargetAlignShift;
    packet.ordinal11.streamout_target3_hi           = HighPart(pStreamoutTargets[3]);

    memcpy(pBuffer, &packet, sizeof(packet));

    return PacketSize;
}

// =====================================================================================================================
// Builds a SET_BUFFER_FILLED_SIZE packet. Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildSetBufferFilledSize(
    const gpusize streamoutCtrlBuf,
    const uint32  bufferId,
    const uint32  bufferOffset,
    void*         pBuffer)
{
    constexpr uint32 PacketSize           = PM4_PFP_SET_BUFFER_FILLED_SIZE_SIZEDW__CORE;
    PM4_PFP_SET_BUFFER_FILLED_SIZE packet = {};

    packet.ordinal1.header.u32All                   = (Type3Header(IT_SET_BUFFER_FILLED_SIZE, PacketSize)).u32All;
    packet.ordinal2.bitfields.streamout_ctrl_buf_lo = LowPart(streamoutCtrlBuf) >> SoCtrlBufAlignShift;
    packet.ordinal3.streamout_ctrl_buf_hi           = HighPart(streamoutCtrlBuf);
    packet.ordinal4.bitfields.buffer_id             = bufferId;
    packet.ordinal5.buffer_offset                   = bufferOffset;

    memcpy(pBuffer, &packet, sizeof(packet));

    return PacketSize;
}

// =====================================================================================================================
// Builds a SAVE_BUFFER_FILLED_SIZES packet. Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildSaveBufferFilledSizes(
    const gpusize  streamoutCtrlBuf,
    const gpusize* pStreamoutTargets,
    void*          pBuffer)
{
    PAL_ASSERT_MSG(MaxStreamOutTargets == 4,
                   "MaxStreamOutTargets is no longer 4 so we need to update the firmware packets!");
    constexpr uint32 PacketSize             = PM4_PFP_SAVE_BUFFER_FILLED_SIZES_SIZEDW__CORE;
    PM4_PFP_SAVE_BUFFER_FILLED_SIZES packet = {};

    packet.ordinal1.header.u32All                   = (Type3Header(IT_SAVE_BUFFER_FILLED_SIZE, PacketSize)).u32All;
    packet.ordinal2.bitfields.streamout_ctrl_buf_lo = LowPart(streamoutCtrlBuf) >> SoCtrlBufAlignShift;
    packet.ordinal3.streamout_ctrl_buf_hi           = HighPart(streamoutCtrlBuf);

    packet.ordinal4.bitfields.streamout_target0_lo  = LowPart(pStreamoutTargets[0]) >> SoTargetAlignShift;
    packet.ordinal5.streamout_target0_hi            = HighPart(pStreamoutTargets[0]);

    packet.ordinal6.bitfields.streamout_target1_lo  = LowPart(pStreamoutTargets[1]) >> SoTargetAlignShift;
    packet.ordinal7.streamout_target1_hi            = HighPart(pStreamoutTargets[1]);

    packet.ordinal8.bitfields.streamout_target2_lo  = LowPart(pStreamoutTargets[2]) >> SoTargetAlignShift;
    packet.ordinal9.streamout_target2_hi            = HighPart(pStreamoutTargets[2]);

    packet.ordinal10.bitfields.streamout_target3_lo = LowPart(pStreamoutTargets[3]) >> SoTargetAlignShift;
    packet.ordinal11.streamout_target3_hi           = HighPart(pStreamoutTargets[3]);

    memcpy(pBuffer, &packet, sizeof(packet));

    return PacketSize;
}

// =====================================================================================================================
// Builds a STRMOUT_STATS_QUERY packet. Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildStreamoutStatsQuery(
    const gpusize streamoutCtrlBuf,
    const uint32  streamIndex,
    const gpusize streamoutDst,
    void*         pBuffer)
{
    constexpr uint32 PacketSize          = PM4_PFP_STREAMOUT_STATS_QUERY_SIZEDW__CORE;
    PM4_PFP_STREAMOUT_STATS_QUERY packet = {};

    packet.ordinal1.header.u32All                   = (Type3Header(IT_STRMOUT_STATS_QUERY, PacketSize)).u32All;
    packet.ordinal2.bitfields.streamout_ctrl_buf_lo = LowPart(streamoutCtrlBuf) >> SoCtrlBufAlignShift;
    packet.ordinal3.streamout_ctrl_buf_hi           = HighPart(streamoutCtrlBuf);
    packet.ordinal4.bitfields.stream_index          = streamIndex;
    packet.ordinal5.bitfields.streamout_dst_lo      = LowPart(streamoutDst) >> QueryPoolAlignShift;
    packet.ordinal6.streamout_dst_hi                = HighPart(streamoutDst);

    memcpy(pBuffer, &packet, sizeof(packet));

    return PacketSize;
}

// =====================================================================================================================
// Builds a DISPATCH_TASK_STATE_INIT packet which provides the virtual address with which CP can access the control
// buffer.
size_t CmdUtil::BuildTaskStateInit(
    gpusize       controlBufferAddr, // [In] Address of the control buffer.
    Pm4Predicate  predicate,         // Predication enable control.
    Pm4ShaderType shaderType,        // This packet is set for ME (Graphics) or MEC (Compute)
    void*         pBuffer)           // [Out] Build the PM4 packet in this buffer.
{
    // The control buffer address must be 256-byte aligned.
    PAL_ASSERT(IsPow2Aligned(controlBufferAddr, 256u));

    static_assert(PM4_MEC_DISPATCH_TASK_STATE_INIT_SIZEDW__CORE == PM4_PFP_DISPATCH_TASK_STATE_INIT_SIZEDW__CORE,
                  "PFP, MEC versions of PM4_ME_DISPATCH_TASK_STATE_INIT are not the same!");

    constexpr uint32 PacketSize = PM4_PFP_DISPATCH_TASK_STATE_INIT_SIZEDW__CORE;
    PM4_PFP_DISPATCH_TASK_STATE_INIT packet = {};

    packet.ordinal1.header.u32All =
        (Type3Header(IT_DISPATCH_TASK_STATE_INIT, PacketSize, false, shaderType, predicate)).u32All;
    packet.ordinal2.bitfields.control_buf_addr_lo = LowPart(controlBufferAddr) >> 8;
    packet.ordinal3.control_buf_addr_hi           = HighPart(controlBufferAddr);

    memcpy(pBuffer, &packet, sizeof(packet));

    return PacketSize;
}

// =====================================================================================================================
// Builds a PERF_COUNTER_WINDOW packet. Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildPerfCounterWindow(
    EngineType engineType,
    bool       enableWindow,
    void*      pBuffer
    ) const
{
    PFP_PERF_COUNTER_WINDOW_op_enum operation = enableWindow ? op__pfp_perf_counter_window__start_window :
                                                               op__pfp_perf_counter_window__stop_window;

    static_assert((uint32(op__pfp_perf_counter_window__start_window) ==
                   uint32(op__mec_perf_counter_window__start_window)) &&
                  (uint32(op__pfp_perf_counter_window__stop_window) ==
                   uint32(op__mec_perf_counter_window__stop_window)) &&
                  (PM4_PFP_PERF_COUNTER_WINDOW_SIZEDW__CORE == PM4_MEC_PERF_COUNTER_WINDOW_SIZEDW__CORE));

    // Minimum FW version required to use PERF_COUNTER_WINDOW packet
    constexpr uint32 MinPfpPerfCounterWindowVersion = 2410;
    constexpr uint32 MinMecPerfCounterWindowVersion = 2500;

    bool   supported  = false;
    uint32 packetSize = 0;

    if (engineType == EngineTypeCompute)
    {
        if (m_chipProps.mecUcodeVersion >= MinMecPerfCounterWindowVersion)
        {
            supported = true;
        }
    }
    else if (m_chipProps.pfpUcodeVersion >= MinPfpPerfCounterWindowVersion)
    {
        supported = true;
    }

    if (supported)
    {
        packetSize = PM4_PFP_PERF_COUNTER_WINDOW_SIZEDW__CORE;

        PM4_PFP_PERF_COUNTER_WINDOW packet = {};

        packet.ordinal1.header.u32All = (Type3Header(IT_PERF_COUNTER_WINDOW, packetSize).u32All);
        packet.ordinal2.bitfields.op  = operation;

        memcpy(pBuffer, &packet, sizeof(packet));
    }
    else
    {
        packetSize = uint32(BuildNop(PM4_PFP_PERF_COUNTER_WINDOW_SIZEDW__CORE, pBuffer));
    }

    return packetSize;
}

// =====================================================================================================================
bool CmdUtil::IsIndexedRegister(
    uint32 addr)
{
    return ((addr == mmCOMPUTE_DISPATCH_INTERLEAVE) ||
            (addr == mmSPI_SHADER_PGM_RSRC3_HS) ||
            (addr == mmSPI_SHADER_PGM_RSRC3_GS) ||
            (addr == mmSPI_SHADER_PGM_RSRC3_PS) ||
            (addr == mmCOMPUTE_STATIC_THREAD_MGMT_SE0) ||
            (addr == mmCOMPUTE_STATIC_THREAD_MGMT_SE1) ||
            (addr == mmCOMPUTE_STATIC_THREAD_MGMT_SE2) ||
            (addr == mmCOMPUTE_STATIC_THREAD_MGMT_SE3) ||
            (addr == mmCOMPUTE_STATIC_THREAD_MGMT_SE4) ||
            (addr == mmCOMPUTE_STATIC_THREAD_MGMT_SE5) ||
            (addr == mmCOMPUTE_STATIC_THREAD_MGMT_SE6) ||
            (addr == mmCOMPUTE_STATIC_THREAD_MGMT_SE7) ||
            (addr == mmCOMPUTE_STATIC_THREAD_MGMT_SE8) ||
            (addr == mmVGT_INDEX_TYPE));
}

// =====================================================================================================================
// Builds a HDP_FLUSH packet for the compute engine. Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildHdpFlush(
    void* pBuffer
    ) const
{
    uint32 packetSize = PM4_MEC_HDP_FLUSH_SIZEDW__CORE;

    PM4_MEC_HDP_FLUSH packet = { };

    packet.ordinal1.u32All = (Type3Header(IT_HDP_FLUSH, packetSize)).u32All;

    memcpy(pBuffer, &packet, sizeof(packet));

    return packetSize;
}

// =====================================================================================================================
size_t CmdUtil::BuildUpdateDbSummarizerTimeouts(
    uint32 timeout,
    void*  pBuffer)
{
    constexpr uint32 PacketSize = PM4_PFP_UPDATE_DB_SUMMARIZER_TIMEOUTS_SIZEDW__CORE;

    PM4_PFP_UPDATE_DB_SUMMARIZER_TIMEOUTS packet = {};
    packet.ordinal1.u32All    = Type3Header(IT_UPDATE_DB_SUMMARIZER_TIMEOUT, PacketSize).u32All;
    packet.ordinal2.reg_value = timeout;

    PM4_PFP_UPDATE_DB_SUMMARIZER_TIMEOUTS* pPacket = static_cast<PM4_PFP_UPDATE_DB_SUMMARIZER_TIMEOUTS*>(pBuffer);
    *pPacket = packet;

    return PacketSize;
}

} // namespace Gfx12
} // namespace Pal
