/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/pipeline.h"
#include "core/hw/gfxip/gfx9/gfx9Chip.h"
#include "core/hw/gfxip/gfx9/gfx9CmdUtil.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/g_gfx9PalSettings.h"
#include "marker_payload.h"
#include "palMath.h"

using namespace Util;

namespace Pal
{
namespace Gfx9
{

static constexpr ME_EVENT_WRITE_event_index_enum VgtEventIndex[]=
{
    event_index__me_event_write__other,                                 // Reserved_0x00,
    event_index__me_event_write__sample_streamoutstats,                 // SAMPLE_STREAMOUTSTATS1,
    event_index__me_event_write__sample_streamoutstats,                 // SAMPLE_STREAMOUTSTATS2,
    event_index__me_event_write__sample_streamoutstats,                 // SAMPLE_STREAMOUTSTATS3,
    event_index__me_event_write__other,                                 // CACHE_FLUSH_TS,
    event_index__me_event_write__other,                                 // CONTEXT_DONE,
    event_index__me_event_write__other,                                 // CACHE_FLUSH,
    event_index__me_event_write__cs_vs_ps_partial_flush,                // CS_PARTIAL_FLUSH,
    event_index__me_event_write__other,                                 // VGT_STREAMOUT_SYNC,
    event_index__me_event_write__other,                                 // Reserved_0x09,
    event_index__me_event_write__other,                                 // VGT_STREAMOUT_RESET,
    event_index__me_event_write__other,                                 // END_OF_PIPE_INCR_DE,
    event_index__me_event_write__other,                                 // END_OF_PIPE_IB_END,
    event_index__me_event_write__other,                                 // RST_PIX_CNT,
    event_index__me_event_write__other,                                 // BREAK_BATCH,
    event_index__me_event_write__cs_vs_ps_partial_flush,                // VS_PARTIAL_FLUSH,
    event_index__me_event_write__cs_vs_ps_partial_flush,                // PS_PARTIAL_FLUSH,
    event_index__me_event_write__other,                                 // FLUSH_HS_OUTPUT,
    event_index__me_event_write__other,                                 // FLUSH_DFSM,
    event_index__me_event_write__other,                                 // RESET_TO_LOWEST_VGT,
    event_index__me_event_write__other,                                 // CACHE_FLUSH_AND_INV_TS_EVENT,
    event_index__me_event_write__zpass_pixel_pipe_stat_control_or_dump, // ZPASS_DONE,
    event_index__me_event_write__other,                                 // CACHE_FLUSH_AND_INV_EVENT,
    event_index__me_event_write__other,                                 // PERFCOUNTER_START,
    event_index__me_event_write__other,                                 // PERFCOUNTER_STOP,
    event_index__me_event_write__other,                                 // PIPELINESTAT_START,
    event_index__me_event_write__other,                                 // PIPELINESTAT_STOP,
    event_index__me_event_write__other,                                 // PERFCOUNTER_SAMPLE,
    event_index__me_event_write__other,                                 // Available_0x1c,
    event_index__me_event_write__other,                                 // Available_0x1d,
    event_index__me_event_write__sample_pipelinestat,                   // SAMPLE_PIPELINESTAT,
    event_index__me_event_write__other,                                 // SO_VGTSTREAMOUT_FLUSH,
    event_index__me_event_write__sample_streamoutstats,                 // SAMPLE_STREAMOUTSTATS,
    event_index__me_event_write__other,                                 // RESET_VTX_CNT,
    event_index__me_event_write__other,                                 // BLOCK_CONTEXT_DONE,
    event_index__me_event_write__other,                                 // CS_CONTEXT_DONE,
    event_index__me_event_write__other,                                 // VGT_FLUSH,
    event_index__me_event_write__other,                                 // TGID_ROLLOVER,
    event_index__me_event_write__other,                                 // SQ_NON_EVENT,
    event_index__me_event_write__other,                                 // SC_SEND_DB_VPZ,
    event_index__me_event_write__other,                                 // BOTTOM_OF_PIPE_TS,
    event_index__me_event_write__other,                                 // FLUSH_SX_TS,
    event_index__me_event_write__other,                                 // DB_CACHE_FLUSH_AND_INV,
    event_index__me_event_write__other,                                 // FLUSH_AND_INV_DB_DATA_TS,
    event_index__me_event_write__other,                                 // FLUSH_AND_INV_DB_META,
    event_index__me_event_write__other,                                 // FLUSH_AND_INV_CB_DATA_TS,
    event_index__me_event_write__other,                                 // FLUSH_AND_INV_CB_META,
    event_index__me_event_write__other,                                 // CS_DONE,
    event_index__me_event_write__other,                                 // PS_DONE,
    event_index__me_event_write__other,                                 // FLUSH_AND_INV_CB_PIXEL_DATA,
    event_index__me_event_write__other,                                 // SX_CB_RAT_ACK_REQUEST,
    event_index__me_event_write__other,                                 // THREAD_TRACE_START,
    event_index__me_event_write__other,                                 // THREAD_TRACE_STOP,
    event_index__me_event_write__other,                                 // THREAD_TRACE_MARKER,
    event_index__me_event_write__other,                                 // THREAD_TRACE_FLUSH,
    event_index__me_event_write__other,                                 // THREAD_TRACE_FINISH,
    event_index__me_event_write__zpass_pixel_pipe_stat_control_or_dump, // PIXEL_PIPE_STAT_CONTROL,
    event_index__me_event_write__zpass_pixel_pipe_stat_control_or_dump, // PIXEL_PIPE_STAT_DUMP,
    event_index__me_event_write__other,                                 // PIXEL_PIPE_STAT_RESET,
    event_index__me_event_write__other,                                 // CONTEXT_SUSPEND,
    event_index__me_event_write__other,                                 // OFFCHIP_HS_DEALLOC,
    event_index__me_event_write__other,                                 // ENABLE_NGG_PIPELINE,
    event_index__me_event_write__other,                                 // ENABLE_LEGACY_PIPELINE,
    event_index__me_event_write__other,                                 // Reserved_0x3f,
};

static constexpr bool VgtEventHasTs[]=
{
    false, // Reserved_0x00,
    false, // SAMPLE_STREAMOUTSTATS1,
    false, // SAMPLE_STREAMOUTSTATS2,
    false, // SAMPLE_STREAMOUTSTATS3,
    true,  // CACHE_FLUSH_TS,
    false, // CONTEXT_DONE,
    false, // CACHE_FLUSH,
    false, // CS_PARTIAL_FLUSH,
    false, // VGT_STREAMOUT_SYNC,
    false, // Reserved_0x09,
    false, // VGT_STREAMOUT_RESET,
    false, // END_OF_PIPE_INCR_DE,
    false, // END_OF_PIPE_IB_END,
    false, // RST_PIX_CNT,
    false, // BREAK_BATCH,
    false, // VS_PARTIAL_FLUSH,
    false, // PS_PARTIAL_FLUSH,
    false, // FLUSH_HS_OUTPUT,
    false, // FLUSH_DFSM,
    false, // RESET_TO_LOWEST_VGT,
    true,  // CACHE_FLUSH_AND_INV_TS_EVENT,
    false, // ZPASS_DONE,
    false, // CACHE_FLUSH_AND_INV_EVENT,
    false, // PERFCOUNTER_START,
    false, // PERFCOUNTER_STOP,
    false, // PIPELINESTAT_START,
    false, // PIPELINESTAT_STOP,
    false, // PERFCOUNTER_SAMPLE,
    false, // Available_0x1c,
    false, // Available_0x1d,
    false, // SAMPLE_PIPELINESTAT,
    false, // SO_VGTSTREAMOUT_FLUSH,
    false, // SAMPLE_STREAMOUTSTATS,
    false, // RESET_VTX_CNT,
    false, // BLOCK_CONTEXT_DONE,
    false, // CS_CONTEXT_DONE,
    false, // VGT_FLUSH,
    false, // TGID_ROLLOVER,
    false, // SQ_NON_EVENT,
    false, // SC_SEND_DB_VPZ,
    true,  // BOTTOM_OF_PIPE_TS,
    true,  // FLUSH_SX_TS,
    false, // DB_CACHE_FLUSH_AND_INV,
    true,  // FLUSH_AND_INV_DB_DATA_TS,
    false, // FLUSH_AND_INV_DB_META,
    true,  // FLUSH_AND_INV_CB_DATA_TS,
    false, // FLUSH_AND_INV_CB_META,
    false, // CS_DONE,
    false, // PS_DONE,
    false, // FLUSH_AND_INV_CB_PIXEL_DATA,
    false, // SX_CB_RAT_ACK_REQUEST,
    false, // THREAD_TRACE_START,
    false, // THREAD_TRACE_STOP,
    false, // THREAD_TRACE_MARKER,
    false, // THREAD_TRACE_FLUSH,
    false, // THREAD_TRACE_FINISH,
    false, // PIXEL_PIPE_STAT_CONTROL,
    false, // PIXEL_PIPE_STAT_DUMP,
    false, // PIXEL_PIPE_STAT_RESET,
    false, // CONTEXT_SUSPEND,
    false, // OFFCHIP_HS_DEALLOC,
    false, // ENABLE_NGG_PIPELINE,
    false, // ENABLE_LEGACY_PIPELINE,
    false, // Reserved_0x3f,
};

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

constexpr size_t TcCacheOpConversionTableSize = ArrayLen(Gfx9TcCacheOpConversionTable);
static_assert(ArrayLen(Gfx9TcCacheOpConversionTable) == static_cast<size_t>(TcCacheOp::Count),
              "TcCacheOp conversion table has too many/few entries");

static uint32 Type3Header(
    IT_OpCodeType  opCode,
    uint32         count,
    bool           resetFilterCam = false,
    Pm4ShaderType  shaderType     = ShaderGraphics,
    Pm4Predicate   predicate      = PredDisable);

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
static PAL_INLINE uint32 Type3Header(
    IT_OpCodeType  opCode,
    uint32         count,
    bool           resetFilterCam,
    Pm4ShaderType  shaderType,
    Pm4Predicate   predicate)
{
    // PFP and ME headers are the same structure...  doesn't really matter which one we use.
    PM4_ME_TYPE_3_HEADER  header = {};

    header.predicate      = predicate;
    header.shaderType     = shaderType;
    header.type           = 3; // type-3 packet
    header.opcode         = opCode;
    header.count          = (count - 2);
    header.resetFilterCam = resetFilterCam;

    return header.u32All;
}

// =====================================================================================================================
// Returns a 32-bit quantity that corresponds to a ordinal 2 of packets that are similar to
// typedef struct PM4_PFP_SET_CONTEXT_REG
// {
//     union
//     {
//         PM4_PFP_TYPE_3_HEADER   header;            ///header
//         uint32_t            ordinal1;
//     };
//
//     union
//     {
//         struct
//         {
//             uint32_t reg_offset:16;
//             uint32_t reserved1:12;
//             PFP_SET_CONTEXT_REG_index_enum index:4;
//         } bitfields2;
//         uint32_t ordinal2;
//     };
//
// //  uint32_t reg_data[];  // N-DWords
//
// } PM4PFP_SET_CONTEXT_REG, *PPM4PFP_SET_CONTEXT_REG;
// This is done with shifts to avoid a read-modify-write of the destination memory.
static PAL_INLINE uint32 Type3Ordinal2(
    uint32 regOffset,
    uint32 index)
{
    const uint32 IndexShift = 28;

    return regOffset |
           (index << IndexShift);
}

// =====================================================================================================================
// Note that this constructor is invoked before settings have been committed.
CmdUtil::CmdUtil(
    const Device& device)
    :
    m_device(device),
    m_gfxIpLevel(device.Parent()->ChipProperties().gfxLevel),
    m_cpUcodeVersion(device.Parent()->EngineProperties().cpUcodeVersion)
#if PAL_ENABLE_PRINTS_ASSERTS
    , m_verifyShadowedRegisters(device.Parent()->Settings().cmdUtilVerifyShadowedRegRanges)
#endif
{
    const Pal::Device&  parent = *(device.Parent());

    memset(&m_registerInfo, 0, sizeof(m_registerInfo));

    if (m_gfxIpLevel == GfxIpLevel::GfxIp9)
    {
        if (IsVega10(parent) || IsRaven(parent))
        {
            m_registerInfo.mmEaPerfResultCntl = Gfx09_0::mmGCEA_PERFCOUNTER_RSLT_CNTL;
        }
        else
        {
            m_registerInfo.mmEaPerfResultCntl    = Gfx09_1x::mmGCEA_PERFCOUNTER_RSLT_CNTL;
            m_registerInfo.mmComputeShaderChksum = Gfx09_1x::mmCOMPUTE_SHADER_CHKSUM;

            if (IsVega12(parent))
            {
                m_registerInfo.mmPaStereoCntl   = Vg12::mmPA_STEREO_CNTL;
                m_registerInfo.mmPaStateStereoX = Vg12::mmPA_STATE_STEREO_X;
            }
            else if (IsVega20(parent))
            {
                m_registerInfo.mmPaStereoCntl   = Vg20::mmPA_STEREO_CNTL;
                m_registerInfo.mmPaStateStereoX = Vg20::mmPA_STATE_STEREO_X;
            }
        }

        m_registerInfo.mmRlcPerfmonClkCntl      = Gfx09::mmRLC_PERFMON_CLK_CNTL;
        m_registerInfo.mmRlcSpmGlobalMuxselAddr = Gfx09::mmRLC_SPM_GLOBAL_MUXSEL_ADDR;
        m_registerInfo.mmRlcSpmGlobalMuxselData = Gfx09::mmRLC_SPM_GLOBAL_MUXSEL_DATA;
        m_registerInfo.mmRlcSpmSeMuxselAddr     = Gfx09::mmRLC_SPM_SE_MUXSEL_ADDR;
        m_registerInfo.mmRlcSpmSeMuxselData     = Gfx09::mmRLC_SPM_SE_MUXSEL_DATA;
        m_registerInfo.mmAtcPerfResultCntl      = Gfx09::mmATC_PERFCOUNTER_RSLT_CNTL;
        m_registerInfo.mmAtcL2PerfResultCntl    = Gfx09::mmATC_L2_PERFCOUNTER_RSLT_CNTL;
        m_registerInfo.mmMcVmL2PerfResultCntl   = Gfx09::mmMC_VM_L2_PERFCOUNTER_RSLT_CNTL;

        {
            m_registerInfo.mmRpbPerfResultCntl = Vega::mmRPB_PERFCOUNTER_RSLT_CNTL;
        }

        m_registerInfo.mmSpiShaderPgmLoLs           = Gfx09::mmSPI_SHADER_PGM_LO_LS;
        m_registerInfo.mmSpiShaderPgmLoEs           = Gfx09::mmSPI_SHADER_PGM_LO_ES;
        m_registerInfo.mmVgtGsMaxPrimsPerSubGroup   = Gfx09::mmVGT_GS_MAX_PRIMS_PER_SUBGROUP;
        m_registerInfo.mmDbDfsmControl              = Gfx09::mmDB_DFSM_CONTROL;
        m_registerInfo.mmUserDataStartHsShaderStage = Gfx09::mmSPI_SHADER_USER_DATA_LS_0;
        m_registerInfo.mmUserDataStartGsShaderStage = Gfx09::mmSPI_SHADER_USER_DATA_ES_0;
    }
    else
    {
        if (IsGfx101(parent))
        {
            m_registerInfo.mmEaPerfResultCntl               = Gfx101::mmGCEA_PERFCOUNTER_RSLT_CNTL;
            m_registerInfo.mmDbDepthInfo                    = Nv10::mmDB_RESERVED_REG_2;
            m_registerInfo.mmAtcPerfResultCntl              = Gfx101::mmATC_PERFCOUNTER_RSLT_CNTL;
            m_registerInfo.mmAtcL2PerfResultCntl            = Gfx101::mmGC_ATC_L2_PERFCOUNTER_RSLT_CNTL;
            m_registerInfo.mmDbDfsmControl                  = Gfx10Core::mmDB_DFSM_CONTROL;

        }
        else
        {
            PAL_ASSERT_ALWAYS();
        }

        m_registerInfo.mmRlcPerfmonClkCntl              = Gfx10::mmRLC_PERFMON_CLK_CNTL;
        m_registerInfo.mmRlcSpmGlobalMuxselAddr         = Gfx10::mmRLC_SPM_GLOBAL_MUXSEL_ADDR;
        m_registerInfo.mmRlcSpmGlobalMuxselData         = Gfx10::mmRLC_SPM_GLOBAL_MUXSEL_DATA;
        m_registerInfo.mmRlcSpmSeMuxselAddr             = Gfx10::mmRLC_SPM_SE_MUXSEL_ADDR;
        m_registerInfo.mmRlcSpmSeMuxselData             = Gfx10::mmRLC_SPM_SE_MUXSEL_DATA;
        m_registerInfo.mmMcVmL2PerfResultCntl           = Gfx10::mmGCMC_VM_L2_PERFCOUNTER_RSLT_CNTL;
        m_registerInfo.mmRpbPerfResultCntl              = Gfx10::mmRPB_PERFCOUNTER_RSLT_CNTL;
        m_registerInfo.mmVgtGsMaxPrimsPerSubGroup       = Gfx10::mmGE_MAX_OUTPUT_PER_SUBGROUP;
        m_registerInfo.mmComputeShaderChksum            = Gfx10::mmCOMPUTE_SHADER_CHKSUM;
        m_registerInfo.mmPaStereoCntl                   = Gfx10::mmPA_STEREO_CNTL;
        m_registerInfo.mmPaStateStereoX                 = Gfx10::mmPA_STATE_STEREO_X;

        // GFX10 provides a "PGM_{LO,HI}_ES_GS" and a "PGM_{LO,HI}_LS_HS" register that you would think is
        // what you want to use for the merged shader stages.  You'd be wrong.  According to
        // Those registers are for internal use only.
        m_registerInfo.mmSpiShaderPgmLoLs = Gfx10::mmSPI_SHADER_PGM_LO_LS;
        m_registerInfo.mmSpiShaderPgmLoEs = Gfx10::mmSPI_SHADER_PGM_LO_ES;

        // The "LS" and "ES" user-data registers (that GFX9 utilizes) do exist on GFX10, but they are only
        // meaningful in non-GEN-TWO mode.  We get 32 of these which is what we want.
        m_registerInfo.mmUserDataStartHsShaderStage = Gfx10::mmSPI_SHADER_USER_DATA_HS_0;
        m_registerInfo.mmUserDataStartGsShaderStage = Gfx10::mmSPI_SHADER_USER_DATA_GS_0;
    }
}

// =====================================================================================================================
// Returns the number of dwords required to chain two pm4 packet chunks together.
uint32 CmdUtil::ChainSizeInDwords(
    EngineType engineType)
{
    uint32  sizeInBytes = 0;

    // The packet used for chaining indirect-buffers together differs based on the queue we're executing on.
    if (Pal::Device::EngineSupportsGraphics(engineType))
    {
        sizeInBytes = sizeof(PM4PFP_INDIRECT_BUFFER);
    }
    else if (engineType == EngineTypeCompute
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 530
             || engineType == EngineTypeExclusiveCompute
#endif
             )
    {
        sizeInBytes = sizeof(PM4MEC_INDIRECT_BUFFER);
    }
    else
    {
        // Other engine types do not support chaining.
    }

    return (sizeInBytes / sizeof(uint32));
}

// =====================================================================================================================
// True if the specified register is in context reg space, false otherwise.
bool CmdUtil::IsContextReg(
    uint32 regAddr)
{
    const bool isContextReg = ((regAddr >= CONTEXT_SPACE_START) && (regAddr <= CONTEXT_SPACE_END));

    // Assert if we need to extend our internal range of context registers we actually set.
    PAL_ASSERT((isContextReg == false) || ((regAddr - CONTEXT_SPACE_START) < CntxRegUsedRangeSize));

    return isContextReg;
}

// =====================================================================================================================
// True if the specified register is in user-config reg space, false otherwise.
bool CmdUtil::IsUserConfigReg(
    uint32 regAddr)
{
    return ((regAddr >= UCONFIG_SPACE_START) && (regAddr <= UCONFIG_SPACE_END));
}

// =====================================================================================================================
// True if the specified register is in persistent data space, false otherwise.
bool CmdUtil::IsShReg(
    uint32 regAddr)
{
    const bool isShReg = ((regAddr >= PERSISTENT_SPACE_START) && (regAddr <= PERSISTENT_SPACE_END));

    // Assert if we need to extend our internal range of SH registers we actually set.
    PAL_ASSERT((isShReg == false) || ((regAddr - PERSISTENT_SPACE_START) < ShRegUsedRangeSize));

    return isShReg;
}

// =====================================================================================================================
// Builds the common aspects of the acquire-mem packet into the supplied pPacket ptr.
template <typename AcquireMemPacketType>
uint32 CmdUtil::BuildAcquireMemInternal(
    const ExplicitAcquireMemInfo& acquireMemInfo,
    AcquireMemPacketType*         pPacket         // [out] Build the PM4 packet in this buffer.
    ) const
{
    constexpr uint32  PacketSize = sizeof(AcquireMemPacketType) / sizeof(uint32);
    pPacket->header.u32All       = Type3Header(IT_ACQUIRE_MEM, PacketSize);
    pPacket->ordinal2            = 0;

    pPacket->bitfields2.coher_cntl = acquireMemInfo.coherCntl;

    if (Pal::Device::EngineSupportsGraphics(acquireMemInfo.engineType))
    {
        pPacket->bitfields2.engine_sel = static_cast<ME_ACQUIRE_MEM_engine_sel_enum>(acquireMemInfo.flags.usePfp
                                            ? static_cast<uint32>(engine_sel__pfp_acquire_mem__prefetch_parser)
                                            : static_cast<uint32>(engine_sel__me_acquire_mem__micro_engine));
    }

    // Need to align-down the given base address and then add the difference to the size, and align that new size.
    // Note that if sizeBytes is equal to FullSyncSize we should clamp it to the max virtual address.
    constexpr gpusize Alignment = 256;
    constexpr gpusize SizeShift = 8;

    const gpusize alignedAddress = Pow2AlignDown(acquireMemInfo.baseAddress, Alignment);
    const gpusize alignedSize = (acquireMemInfo.sizeBytes == FullSyncSize)
                        ? m_device.Parent()->MemoryProperties().vaUsableEnd
                        : Pow2Align(acquireMemInfo.sizeBytes + acquireMemInfo.baseAddress - alignedAddress, Alignment);

    pPacket->coher_size = LowPart(alignedSize >> SizeShift);
    pPacket->ordinal4   = HighPart(alignedSize >> SizeShift);

    // Make sure that the size field doesn't overflow
    PAL_ASSERT (pPacket->bitfields4.reserved1 == 0);

    pPacket->coher_base_lo = Get256BAddrLo(alignedAddress);
    pPacket->ordinal6      = Get256BAddrHi(alignedAddress);

    // Make sure that the address field doesn't overflow
    PAL_ASSERT (pPacket->bitfields6.reserved1 == 0);

    pPacket->ordinal7                 = 0;
    pPacket->bitfields7.poll_interval = Pal::Device::PollInterval;

    return PacketSize;
}

// =====================================================================================================================
// GFX10 adds a new GCR_CNTL field that takes over flush/inv control on most caches. Only CB and DB are still
// controlled by CP_COHER_CNTL field. This function essentially converts GFX9 style cache sync info to GFX10 style one.
uint32 CmdUtil::Gfx10CalcAcquireMemGcrCntl(
    const AcquireMemInfo&  acquireMemInfo
    ) const
{
    Gfx10AcquireMemGcrCntl gcrCntl = {};

    // The L1 / L2 caches are physical address based. When specify the range, the GCR will perform virtual address
    // to physical address translation before the wb / inv. If the acquired op is full sync, we must ignore the range,
    // otherwise page fault may occur because page table cannot cover full range virtual address.
    //    When the source address is virtual , the GCR block will have to perform the virtual address to physical
    //    address translation before the wb / inv. Since the pages in memory are a collection of fragments, you can't
    //    specify the full range without walking into a page that has no PTE triggering a fault. In the cases where
    //    the driver wants to wb / inv the entire cache, you should not use range based method, and instead flush the
    //    entire cache without it. The range based method is not meant to be used this way, it is for selective page
    //    invalidation.
    //
    // GL1_RANGE[1:0] - range control for L0 / L1 physical caches(K$, V$, M$, GL1)
    //  0:ALL         - wb / inv op applies to entire physical cache (ignore range)
    //  1:reserved
    //  2:RANGE       - wb / inv op applies to just the base / limit virtual address range
    //  3:FIRST_LAST  - wb / inv op applies to 128B at BASE_VA and 128B at LIMIT_VA
    //
    // GL2_RANGE[1:0]
    //  0:ALL         - wb / inv op applies to entire physical cache (ignore range)
    //  1:VOL         - wb / inv op applies to all volatile tagged lines in the GL2 (ignore range)
    //  2:RANGE       - wb / inv op applies to just the base/limit virtual address range
    //  3:FIRST_LAST  - wb / inv op applies to 128B at BASE_VA and 128B at LIMIT_VA
    if (((acquireMemInfo.baseAddress == FullSyncBaseAddr) && (acquireMemInfo.sizeBytes == FullSyncSize)) ||
        (acquireMemInfo.sizeBytes > CmdUtil::Gfx10AcquireMemGl1Gl2RangedCheckMaxSurfaceSizeBytes))
    {
        gcrCntl.bits.gl1Range = 0;
        gcrCntl.bits.gl2Range = 0;
    }
    else
    {
        gcrCntl.bits.gl1Range = 2;
        gcrCntl.bits.gl2Range = 2;
    }

    // #1. Setup TC cache operations.
    switch (acquireMemInfo.tcCacheOp)
    {
    case TcCacheOp::WbInvL1L2:
        // GLM_WB[0]  - write-back control for the meta-data cache of GL2. L2MD is write-through, ignore this bit.
        // GLM_INV[0] - invalidate enable for the meta-data cache of GL2
        // GLV_INV[0] - invalidate enable for shader vector L0 cache
        // GL1_INV[0] - invalidate enable for GL1
        // GL2_INV[0] - invalidate enable for GL2
        // GL2_WB[0]  - writeback enable for GL2
        gcrCntl.bits.glmInv = 1;
        gcrCntl.bits.glvInv = 1;
        gcrCntl.bits.gl1Inv = 1;
        gcrCntl.bits.gl2Inv = 1;
        gcrCntl.bits.gl2Wb  = 1;
        break;

    case TcCacheOp::WbInvL2Nc:
        // GL2_INV[0] - invalidate enable for GL2
        // GL2_WB[0]  - writeback enable for GL2
        gcrCntl.bits.gl2Inv = 1;
        gcrCntl.bits.gl2Wb  = 1;
        break;

        // GFX10TODO: GCR cannot differentiate Nc(non-coherent MTYPE) and Wc(write-combined MTYPE)?
    case TcCacheOp::WbL2Nc:
    case TcCacheOp::WbL2Wc:
        // GL2_WB[0] - writeback enable for GL2
        gcrCntl.bits.gl2Wb = 1;
        break;

    case TcCacheOp::InvL2Nc:
        // GL2_INV[0] - invalidate enable for GL2
        gcrCntl.bits.gl2Inv = 1;

    case TcCacheOp::InvL2Md:
        // GLM_INV[0] - invalidate enable for the meta-data cache of GL2
        gcrCntl.bits.glmInv = 1;
        break;

    case TcCacheOp::InvL1:
        // GLV_INV[0] - invalidate enable for shader vector L0 cache
        gcrCntl.bits.glvInv = 1;
        gcrCntl.bits.gl1Inv = 1;
        break;

    case TcCacheOp::InvL1Vol:
        // GL2_RANGE[1:0]
        // GL1_INV[0] - invalidate enable for GL1
        // GLV_INV[0] - invalidate enable for shader vector L0 cache
        gcrCntl.bits.gl2Range = 1;
        gcrCntl.bits.gl1Inv   = 1;
        gcrCntl.bits.glvInv   = 1;
        break;

    default:
        PAL_ASSERT(acquireMemInfo.tcCacheOp == TcCacheOp::Nop);
        break;
    }

    // #2. Setup extra cache operations.
    // GLI_INV[1:0]   control for the virtual tagged instruction cache (I$)
    //      0:NOP          no invalidation of I$
    //      1:ALL          entire I$ is invalidated
    //      2:RANGE        invalidate base -> limit virtual address range of the I$.
    //                     Overrides to NOP if RANGE_IS_PA == 1
    //      3:FIRST_LAST   invalidate just 128B at BASE_VA and 128B at LIMIT_VA.
    //                     Overrides to NOP if RANGE_IS_PA == 1
    //
    gcrCntl.bits.gliInv = acquireMemInfo.flags.invSqI$;

    // GLK_WB[0]  - write-back control for shaded scalar L0 cache
    // GLK_INV[0] - invalidate enable for shader scalar L0 cache
    gcrCntl.bits.glkWb  = acquireMemInfo.flags.flushSqK$;
    gcrCntl.bits.glkInv = acquireMemInfo.flags.invSqK$;

    // SEQ[1:0]   controls the sequence of operations on the cache hierarchy (L0/L1/L2)
    //      0: PARALLEL   initiate wb/inv ops on specified caches at same time
    //      1: FORWARD    L0 then L1/L2, complete L0 ops then initiate L1/L2
    //                    Typically only needed when doing WB of L0 K$, M$, or RB w/ WB of GL2
    //      2: REVERSE    L2 -> L1 -> L0
    //                    Typically only used for post-unaligned-DMA operation (invalidate only)
    switch (acquireMemInfo.tcCacheOp)
    {
    case TcCacheOp::WbInvL1L2:
    case TcCacheOp::WbInvL2Nc:
    case TcCacheOp::WbL2Nc:
    case TcCacheOp::WbL2Wc:
        // CbMd flush only happens with CACHE_FLUSH_AND_INV_XX event, in this case we expect the hardware to
        // take care of the flush sequence.
        if (acquireMemInfo.flags.flushSqK$ || acquireMemInfo.flags.wbInvCbData || acquireMemInfo.flags.wbInvDb)
        {
            gcrCntl.bits.seq = 1;
        }
        break;

    default:
        break;
    }

    return gcrCntl.u32All;
}

// =====================================================================================================================
// Builds the the ACQUIRE_MEM command.  Returns the size, in DWORDs, of the assembled PM4 command
size_t CmdUtil::BuildAcquireMem(
    const AcquireMemInfo& acquireMemInfo,
    void*                 pBuffer         // [out] Build the PM4 packet in this buffer.
    ) const
{
    if (Pal::Device::EngineSupportsGraphics(acquireMemInfo.engineType) == false)
    {
        // If there's no graphics support on this engine then disable various gfx-specific requests
        PAL_ASSERT(acquireMemInfo.cpMeCoherCntl.u32All == 0);
        PAL_ASSERT(acquireMemInfo.flags.wbInvCbData == 0);
        PAL_ASSERT(acquireMemInfo.flags.wbInvDb == 0);
    }

    // Translate AcquireMemInfo to an explicit AcquireMemInfo type.
    ExplicitAcquireMemInfo explicitAcquireMemInfo = {};
    explicitAcquireMemInfo.flags.usePfp = acquireMemInfo.flags.usePfp;
    explicitAcquireMemInfo.engineType   = acquireMemInfo.engineType;
    explicitAcquireMemInfo.baseAddress  = acquireMemInfo.baseAddress;
    explicitAcquireMemInfo.sizeBytes    = acquireMemInfo.sizeBytes;

    // The CP_COHER_CNTL bits either belong to the set of mutually exclusive TC cache ops or can be set
    // independently.
    if (m_gfxIpLevel == GfxIpLevel::GfxIp9)
    {
        const uint32  tcCacheOp = static_cast<uint32>(acquireMemInfo.tcCacheOp);

        regCP_COHER_CNTL cpCoherCntl;
        cpCoherCntl.u32All                       = Gfx9TcCacheOpConversionTable[tcCacheOp];
        cpCoherCntl.bits.CB_ACTION_ENA           = acquireMemInfo.flags.wbInvCbData;
        cpCoherCntl.bits.DB_ACTION_ENA           = acquireMemInfo.flags.wbInvDb;
        cpCoherCntl.bits.SH_KCACHE_ACTION_ENA    = acquireMemInfo.flags.invSqK$;
        cpCoherCntl.bits.SH_ICACHE_ACTION_ENA    = acquireMemInfo.flags.invSqI$;
        cpCoherCntl.bits.SH_KCACHE_WB_ACTION_ENA = acquireMemInfo.flags.flushSqK$;

        // There shouldn't be any shared bits between CP_ME_COHER_CNTL and CP_COHER_CNTL.
        PAL_ASSERT((cpCoherCntl.u32All & acquireMemInfo.cpMeCoherCntl.u32All) == 0);

        explicitAcquireMemInfo.coherCntl = cpCoherCntl.u32All | acquireMemInfo.cpMeCoherCntl.u32All;
    }
    else if (IsGfx10(m_gfxIpLevel))
    {
        if (Pal::Device::EngineSupportsGraphics(acquireMemInfo.engineType))
        {
            // K$ and I$ and all previous tcCacheOp controlled caches are moved to GCR fields,
            // set in CalcAcquireMemGcrCntl().
            regCP_COHER_CNTL cpCoherCntl = {};
            cpCoherCntl.bits.CB_ACTION_ENA = acquireMemInfo.flags.wbInvCbData;
            cpCoherCntl.bits.DB_ACTION_ENA = acquireMemInfo.flags.wbInvDb;

            // There shouldn't be any shared bits between CP_ME_COHER_CNTL and CP_COHER_CNTL.
            PAL_ASSERT((cpCoherCntl.u32All & acquireMemInfo.cpMeCoherCntl.u32All) == 0);

            explicitAcquireMemInfo.coherCntl = cpCoherCntl.u32All | acquireMemInfo.cpMeCoherCntl.u32All;
        }

        explicitAcquireMemInfo.gcrCntl.u32All = Gfx10CalcAcquireMemGcrCntl(acquireMemInfo);
    }

    // Call a more explicit function.
    return ExplicitBuildAcquireMem(explicitAcquireMemInfo, pBuffer);
}

// =====================================================================================================================
// Builds the the ACQUIRE_MEM command.  Returns the size, in DWORDs, of the assembled PM4 command
size_t CmdUtil::ExplicitBuildAcquireMem(
    const ExplicitAcquireMemInfo& acquireMemInfo,
    void*                         pBuffer         // [out] Build the PM4 packet in this buffer.
    ) const
{
    uint32  packetSize = 0;

    if (m_gfxIpLevel == GfxIpLevel::GfxIp9)
    {
        static_assert(sizeof(PM4MEC_ACQUIRE_MEM__GFX09) == sizeof(PM4ME_ACQUIRE_MEM__GFX09),
                      "GFX9:  ACQUIRE_MEM packet size is different between ME compute and ME graphics!");

        auto*const pPacket = static_cast<PM4ME_ACQUIRE_MEM__GFX09*>(pBuffer);

        packetSize = BuildAcquireMemInternal(acquireMemInfo, pPacket);
    }
    else if (IsGfx10(m_gfxIpLevel))
    {
        static_assert(sizeof(PM4MEC_ACQUIRE_MEM__GFX10) == sizeof(PM4ME_ACQUIRE_MEM__GFX10),
                      "GFX10: ACQUIRE_MEM packet size is different between ME compute and ME graphics!");

        auto*const pPacket = static_cast<PM4ME_ACQUIRE_MEM__GFX10*>(pBuffer);

        packetSize = BuildAcquireMemInternal(acquireMemInfo, pPacket);

        // Handle the GFX-specific aspects of a release-mem packet.
        pPacket->ordinal8            = 0;
        pPacket->bitfields8.gcr_cntl = acquireMemInfo.gcrCntl.u32All;
        PAL_ASSERT(pPacket->bitfields8.reserved1 == 0);
    }

    return packetSize;
}

// =====================================================================================================================
// True if the specified atomic operation acts on 32-bit values.
static bool Is32BitAtomicOp(
    AtomicOp atomicOp)
{
    // AddInt64 is the first 64-bit operation.
    return (static_cast<int32>(atomicOp) < static_cast<int32>(AtomicOp::AddInt64));
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
    static_assert((sizeof(PM4_ME_ATOMIC_MEM) == sizeof(PM4_MEC_ATOMIC_MEM)),
                  "Atomic Mem packets don't match between ME and MEC!");

    static_assert(((static_cast<uint32>(command__me_atomic_mem__single_pass_atomic)           ==
                    static_cast<uint32>(command__mec_atomic_mem__single_pass_atomic))         &&
                   (static_cast<uint32>(command__me_atomic_mem__loop_until_compare_satisfied) ==
                    static_cast<uint32>(command__mec_atomic_mem__loop_until_compare_satisfied))),
                  "Atomic Mem command enum is different between ME and MEC!");

    static_assert(((static_cast<uint32>(cache_policy__me_atomic_mem__lru)    ==
                    static_cast<uint32>(cache_policy__mec_atomic_mem__lru))  &&
                   (static_cast<uint32>(cache_policy__me_atomic_mem__stream) ==
                    static_cast<uint32>(cache_policy__mec_atomic_mem__stream))),
                  "Atomic Mem cache policy enum is different between ME and MEC!");

    static_assert(((static_cast<uint32>(cache_policy__me_atomic_mem__noa__GFX10) ==
                    static_cast<uint32>(cache_policy__mec_atomic_mem__noa__GFX10))  &&
                   (static_cast<uint32>(cache_policy__me_atomic_mem__bypass__GFX10) ==
                    static_cast<uint32>(cache_policy__mec_atomic_mem__bypass__GFX10))),
                  "Atomic Mem cache policy enum is different between ME and MEC!");

    // The destination address must be aligned to the size of the operands.
    PAL_ASSERT((dstMemAddr != 0) && IsPow2Aligned(dstMemAddr, (Is32BitAtomicOp(atomicOp) ? 4 : 8)));

    constexpr uint32 PacketSize = (sizeof(PM4_ME_ATOMIC_MEM) / sizeof(uint32));
    auto*const       pPacket    = static_cast<PM4_ME_ATOMIC_MEM*>(pBuffer);

    pPacket->header.u32All            = Type3Header(IT_ATOMIC_MEM, PacketSize);
    pPacket->ordinal2                 = 0;
    pPacket->bitfields2.atomic        = AtomicOpConversionTable[static_cast<uint32>(atomicOp)];
    pPacket->bitfields2.command       = command__me_atomic_mem__single_pass_atomic;
    pPacket->bitfields2.cache_policy  = cache_policy__me_atomic_mem__lru;
    pPacket->addr_lo                  = LowPart(dstMemAddr);
    pPacket->addr_hi                  = HighPart(dstMemAddr);
    pPacket->src_data_lo              = LowPart(srcData);
    pPacket->src_data_hi              = HighPart(srcData);
    pPacket->cmp_data_lo              = 0;
    pPacket->cmp_data_hi              = 0;
    pPacket->ordinal9                 = 0;

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 packet which issues a clear state command. Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildClearState(
    PFP_CLEAR_STATE_cmd_enum command,
    void*                    pBuffer) // [out] Build the PM4 packet in this buffer.
{
    static_assert((sizeof(PM4_PFP_CLEAR_STATE) == sizeof(PM4_ME_CLEAR_STATE)),
                  "Clear state packets don't match between PFP and ME!");

    constexpr uint32 PacketSize = (sizeof(PM4_PFP_CLEAR_STATE) / sizeof(uint32));
    auto*const       pPacket    = static_cast<PM4_PFP_CLEAR_STATE*>(pBuffer);

    pPacket->header.u32All  = Type3Header(IT_CLEAR_STATE, PacketSize);
    pPacket->ordinal2       = 0;
    pPacket->bitfields2.cmd = command;

    return PacketSize;
}

// =====================================================================================================================
// Generates a basic "COND_EXEC" packet. Returns the size, in DWORDs, of the generated packet.
size_t CmdUtil::BuildCondExec(
    gpusize gpuVirtAddr,
    uint32  sizeInDwords,
    void*   pBuffer)
{
    static_assert((sizeof(PM4PFP_COND_EXEC) == sizeof(PM4MEC_COND_EXEC)),
                  "Conditional execute packets don't match between GFX and compute!");

    constexpr uint32 PacketSize = (sizeof(PM4MEC_COND_EXEC) / sizeof(uint32));
    auto*const       pPacket    = static_cast<PM4MEC_COND_EXEC*>(pBuffer);

    memset(pPacket, 0, sizeof(PM4MEC_COND_EXEC));
    pPacket->header.u32All         = Type3Header(IT_COND_EXEC, PacketSize);
    pPacket->ordinal2              = LowPart(gpuVirtAddr);
    PAL_ASSERT(pPacket->bitfields2.reserved1 == 0);
    pPacket->addr_hi               = HighPart(gpuVirtAddr);
    pPacket->bitfields5.exec_count = sizeInDwords;

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
    bool        constantEngine,
    void*       pBuffer)
{
    static_assert((sizeof(PM4PFP_COND_INDIRECT_BUFFER) == sizeof(PM4MEC_COND_INDIRECT_BUFFER)),
                  "Conditional indirect buffer packets don't match between GFX and compute!");

    // The CP doesn't implement a "never" compare function.  It is the caller's responsibility to detect
    // this case and work around it.  The "funcTranslation" table defines an entry for "never" only to
    // make indexing into it easy.
    PAL_ASSERT (compareFunc != CompareFunc::Never);

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

    constexpr uint32    PacketSize = (sizeof(PM4PFP_COND_INDIRECT_BUFFER) / sizeof(uint32));
    auto*const          pPacket    = static_cast<PM4PFP_COND_INDIRECT_BUFFER*>(pBuffer);
    // There is no separate op-code for conditional indirect buffers.  The CP figures it out
    const IT_OpCodeType opCode     = constantEngine ? IT_INDIRECT_BUFFER_CNST : IT_INDIRECT_BUFFER;

    memset(pPacket, 0, sizeof(PM4PFP_COND_INDIRECT_BUFFER));

    pPacket->header.u32All       = Type3Header(opCode, PacketSize);
    pPacket->bitfields2.function = FuncTranslation[static_cast<uint32>(compareFunc)];

    // We always implement both a "then" and an "else" clause
    pPacket->bitfields2.mode = mode__pfp_cond_indirect_buffer__if_then_else;

    // Make sure our comparison address is aligned properly
    pPacket->ordinal3        = LowPart(compareGpuAddr);
    pPacket->compare_addr_hi = HighPart(compareGpuAddr);
    PAL_ASSERT (pPacket->bitfields3.reserved1 == 0);

    pPacket->mask_lo      = LowPart(mask);
    pPacket->mask_hi      = HighPart(mask);
    pPacket->reference_lo = LowPart(data);
    pPacket->reference_hi = HighPart(data);

    // Size and locations of the IB are not yet known, will be patched later.

    return PacketSize;
}

// =====================================================================================================================
// Builds a CONTEXT_CONTROL packet with both load and shadowing disabled.  Returns the size, in DWORDs, of the
// generated packet.
size_t CmdUtil::BuildContextControl(
    const PM4PFP_CONTEXT_CONTROL& contextControl,
    void*                         pBuffer)   // [out] Build the PM4 packet in this buffer.
{
    static_assert((sizeof(PM4PFP_CONTEXT_CONTROL) == sizeof(PM4ME_CONTEXT_CONTROL)),
                  "Context control packet doesn't match between PFP and ME!");

    constexpr uint32 PacketSize = (sizeof(PM4PFP_CONTEXT_CONTROL) / sizeof(uint32));
    auto*const       pPacket    = static_cast<PM4PFP_CONTEXT_CONTROL*>(pBuffer);

    pPacket->header.u32All            = Type3Header(IT_CONTEXT_CONTROL, PacketSize);
    pPacket->ordinal2                 = contextControl.ordinal2;
    pPacket->ordinal3                 = contextControl.ordinal3;

    return PacketSize;
}

// =====================================================================================================================
size_t CmdUtil::BuildCopyDataGraphics(
    uint32                        engineSel,
    ME_COPY_DATA_dst_sel_enum     dstSel,
    gpusize                       dstAddr,
    ME_COPY_DATA_src_sel_enum     srcSel,
    gpusize                       srcAddr,
    ME_COPY_DATA_count_sel_enum   countSel,
    ME_COPY_DATA_wr_confirm_enum  wrConfirm,
    void*                         pBuffer)
{
    return BuildCopyData(
        EngineTypeUniversal, engineSel, dstSel, dstAddr, srcSel, srcAddr, countSel, wrConfirm, pBuffer);
}

// =====================================================================================================================
size_t CmdUtil::BuildCopyDataCompute(
    MEC_COPY_DATA_dst_sel_enum     dstSel,
    gpusize                        dstAddr,
    MEC_COPY_DATA_src_sel_enum     srcSel,
    gpusize                        srcAddr,
    MEC_COPY_DATA_count_sel_enum   countSel,
    MEC_COPY_DATA_wr_confirm_enum  wrConfirm,
    void*                          pBuffer)
{
    return BuildCopyData(EngineTypeCompute, 0, dstSel, dstAddr, srcSel, srcAddr, countSel, wrConfirm, pBuffer);
}
// =====================================================================================================================
// Builds a COPY_DATA packet for the compute/ graphics engine. Returns the size, in DWORDs, of the assembled PM4 command
size_t CmdUtil::BuildCopyData(
    EngineType engineType,
    uint32     engineSel, // Ignored on async compute
    uint32     dstSel,
    gpusize    dstAddr,   // Dest addr of the copy, see dstSel for exact meaning
    uint32     srcSel,
    gpusize    srcAddr,   // Source address (or value) of the copy, see srcSel for exact meaning
    uint32     countSel,
    uint32     wrConfirm,
    void*      pBuffer)   // [out] Build the PM4 packet in this buffer.
{
    static_assert((sizeof(PM4ME_COPY_DATA) == sizeof(PM4MEC_COPY_DATA)),
                  "CopyData packet size is different between ME and MEC!");

    static_assert(((static_cast<uint32>(src_sel__mec_copy_data__mem_mapped_register)     ==
                    static_cast<uint32>(src_sel__me_copy_data__mem_mapped_register))     &&
                   (static_cast<uint32>(src_sel__mec_copy_data__memory__GFX09)           ==
                    static_cast<uint32>(src_sel__me_copy_data__memory__GFX09))           &&
                   (static_cast<uint32>(src_sel__mec_copy_data__tc_l2)                   ==
                    static_cast<uint32>(src_sel__me_copy_data__tc_l2))                   &&
                   (static_cast<uint32>(src_sel__mec_copy_data__gds)                     ==
                    static_cast<uint32>(src_sel__me_copy_data__gds))                     &&
                   (static_cast<uint32>(src_sel__mec_copy_data__perfcounters)            ==
                    static_cast<uint32>(src_sel__me_copy_data__perfcounters))            &&
                   (static_cast<uint32>(src_sel__mec_copy_data__immediate_data)          ==
                    static_cast<uint32>(src_sel__me_copy_data__immediate_data))          &&
                   (static_cast<uint32>(src_sel__mec_copy_data__atomic_return_data)      ==
                    static_cast<uint32>(src_sel__me_copy_data__atomic_return_data))      &&
                   (static_cast<uint32>(src_sel__mec_copy_data__gds_atomic_return_data0) ==
                    static_cast<uint32>(src_sel__me_copy_data__gds_atomic_return_data0)) &&
                   (static_cast<uint32>(src_sel__mec_copy_data__gds_atomic_return_data1) ==
                    static_cast<uint32>(src_sel__me_copy_data__gds_atomic_return_data1)) &&
                   (static_cast<uint32>(src_sel__mec_copy_data__gpu_clock_count)         ==
                    static_cast<uint32>(src_sel__me_copy_data__gpu_clock_count))),
                  "CopyData srcSel enum is different between ME and MEC!");

    static_assert(((static_cast<uint32>(dst_sel__mec_copy_data__mem_mapped_register) ==
                    static_cast<uint32>(dst_sel__me_copy_data__mem_mapped_register)) &&
                   (static_cast<uint32>(dst_sel__mec_copy_data__tc_l2)               ==
                    static_cast<uint32>(dst_sel__me_copy_data__tc_l2))               &&
                   (static_cast<uint32>(dst_sel__mec_copy_data__gds)                 ==
                    static_cast<uint32>(dst_sel__me_copy_data__gds))                 &&
                   (static_cast<uint32>(dst_sel__mec_copy_data__perfcounters)        ==
                    static_cast<uint32>(dst_sel__me_copy_data__perfcounters))        &&
                   (static_cast<uint32>(dst_sel__mec_copy_data__memory__GFX09)       ==
                    static_cast<uint32>(dst_sel__me_copy_data__memory__GFX09))),
                   "CopyData dstSel enum is different between ME and MEC!");

    static_assert(((static_cast<uint32>(src_cache_policy__mec_copy_data__lru)    ==
                    static_cast<uint32>(src_cache_policy__me_copy_data__lru))    &&
                   (static_cast<uint32>(src_cache_policy__mec_copy_data__stream) ==
                    static_cast<uint32>(src_cache_policy__me_copy_data__stream))),
                  "CopyData srcCachePolicy enum is different between ME and MEC!");

    static_assert(((static_cast<uint32>(dst_cache_policy__mec_copy_data__lru)    ==
                    static_cast<uint32>(dst_cache_policy__me_copy_data__lru))    &&
                   (static_cast<uint32>(dst_cache_policy__mec_copy_data__stream) ==
                    static_cast<uint32>(dst_cache_policy__me_copy_data__stream))),
                  "CopyData dstCachePolicy enum is different between ME and MEC!");

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

    static_assert((static_cast<uint32>(src_sel__pfp_copy_data__tc_l2_obsolete__GFX10) ==
                   static_cast<uint32>(src_sel__pfp_copy_data__memory__GFX09)),
                  "CopyData memory destination enumerations have changed between GFX9 and GFX10");

    static_assert((static_cast<uint32>(dst_sel__pfp_copy_data__tc_l2_obsolete__GFX10) ==
                   static_cast<uint32>(dst_sel__pfp_copy_data__memory__GFX09)),
                  "CopyData memory destination enumerations have changed between GFX9 and GFX10");

    constexpr uint32  PacketSize     = (sizeof(PM4ME_COPY_DATA) / sizeof(uint32));
    PM4ME_COPY_DATA   packetGfx;
    PM4MEC_COPY_DATA* pPacketCompute = reinterpret_cast<PM4MEC_COPY_DATA*>(&packetGfx);
    const bool        gfxSupported   = Pal::Device::EngineSupportsGraphics(engineType);
    const bool        isCompute      = (engineType == EngineTypeCompute);

    packetGfx.header.u32All = Type3Header(IT_COPY_DATA, PacketSize);
    packetGfx.ordinal2      = 0;
    packetGfx.ordinal3      = 0;
    packetGfx.ordinal4      = 0;
    packetGfx.ordinal5      = 0;

    packetGfx.bitfields2.src_sel    = static_cast<ME_COPY_DATA_src_sel_enum>(srcSel);
    packetGfx.bitfields2.dst_sel    = static_cast<ME_COPY_DATA_dst_sel_enum>(dstSel);
    packetGfx.bitfields2.count_sel  = static_cast<ME_COPY_DATA_count_sel_enum>(countSel);
    packetGfx.bitfields2.wr_confirm = static_cast<ME_COPY_DATA_wr_confirm_enum>(wrConfirm);

    if (isCompute)
    {
        // Set these to their "zero" equivalents...  Enumerating these here explicitly to provide reminders that these
        // fields do exist.
        pPacketCompute->bitfields2.src_cache_policy = src_cache_policy__mec_copy_data__lru;
        pPacketCompute->bitfields2.dst_cache_policy = dst_cache_policy__mec_copy_data__lru;
        pPacketCompute->bitfields2.pq_exe_status    = pq_exe_status__mec_copy_data__default;
    }
    else
    {
        PAL_ASSERT(Pal::Device::EngineSupportsGraphics(engineType));

        // Set these to their "zero" equivalents...  Enumerating these here explicitly to provide reminders that these
        // fields do exist.
        packetGfx.bitfields2.src_cache_policy = src_cache_policy__me_copy_data__lru;
        packetGfx.bitfields2.dst_cache_policy = dst_cache_policy__me_copy_data__lru;
        packetGfx.bitfields2.engine_sel       = static_cast<ME_COPY_DATA_engine_sel_enum>(engineSel);
    }

    switch (srcSel)
    {
    case src_sel__me_copy_data__perfcounters:
    case src_sel__me_copy_data__mem_mapped_register:
        packetGfx.ordinal3 = LowPart(srcAddr);

        // Make sure we didn't get an illegal register offset
        PAL_ASSERT ((gfxSupported && (packetGfx.bitfields3a.reserved1 == 0)) ||
                    (isCompute    && (pPacketCompute->bitfields3a.reserved1 == 0)));
        PAL_ASSERT (HighPart(srcAddr) == 0);
        break;

    case src_sel__me_copy_data__immediate_data:
        packetGfx.imm_data     = LowPart(srcAddr);

        // Really only meaningful if countSel==count_sel__me_copy_data__64_bits_of_data, but shouldn't hurt to
        // write it regardless.
        packetGfx.src_imm_data = HighPart(srcAddr);
        break;

    case src_sel__me_copy_data__memory__GFX09:
    case src_sel__me_copy_data__tc_l2:
        packetGfx.ordinal3          = LowPart(srcAddr);
        packetGfx.src_memtc_addr_hi = HighPart(srcAddr);

        // Make sure our srcAddr is properly aligned.  The alignment differs based on how much data is being written
        PAL_ASSERT (((countSel == count_sel__mec_copy_data__64_bits_of_data) &&
                     ((isCompute    && (pPacketCompute->bitfields3c.reserved1 == 0)) ||
                      (gfxSupported && (packetGfx.bitfields3c.reserved1 == 0)))) ||
                    ((countSel == count_sel__mec_copy_data__32_bits_of_data) &&
                     ((isCompute    && (pPacketCompute->bitfields3b.reserved1 == 0))  ||
                      (gfxSupported && (packetGfx.bitfields3b.reserved1 == 0)))));
        break;

    case src_sel__me_copy_data__gpu_clock_count:
        // Nothing to worry about here?
        break;

    default:
        // Feel free to implement this.  :-)
        PAL_NOT_IMPLEMENTED();
        break;
    }

    switch (dstSel)
    {
    case dst_sel__me_copy_data__perfcounters:
    case dst_sel__me_copy_data__mem_mapped_register:
        packetGfx.ordinal5 = LowPart(dstAddr);
        PAL_ASSERT ((isCompute    && (pPacketCompute->bitfields5a.reserved1 == 0)) ||
                    (gfxSupported && (packetGfx.bitfields5a.reserved1 == 0)));
        break;

    case dst_sel__me_copy_data__memory_sync_across_grbm:
        // sync memory destination is only available with ME engine on universal queue
        PAL_ASSERT (gfxSupported && (engineSel == engine_sel__me_copy_data__micro_engine));
        // break intentionally left out

    case dst_sel__me_copy_data__memory__GFX09:
    case dst_sel__me_copy_data__tc_l2:
        packetGfx.ordinal5    = LowPart(dstAddr);
        packetGfx.dst_addr_hi = HighPart(dstAddr);

        // Make sure our dstAddr is properly aligned.  The alignment differs based on how much data is being written
        PAL_ASSERT (((countSel == count_sel__mec_copy_data__64_bits_of_data) &&
                     ((isCompute    && (pPacketCompute->bitfields5c.reserved1 == 0)) ||
                      (gfxSupported && (packetGfx.bitfields5c.reserved1 == 0)))) ||
                    ((countSel == count_sel__mec_copy_data__32_bits_of_data) &&
                     ((isCompute    && (pPacketCompute->bitfields5b.reserved1 == 0)) ||
                      (gfxSupported && (packetGfx.bitfields5b.reserved1 == 0)))));
        break;

    case dst_sel__me_copy_data__gds:
        packetGfx.ordinal5 = LowPart(dstAddr);
        PAL_ASSERT ((isCompute    && (pPacketCompute->bitfields5d.reserved1 == 0)) ||
                    (gfxSupported && (packetGfx.bitfields5d.reserved1 == 0)));
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
// Builds a DISPATCH_DIRECT packet. Returns the size of the PM4 command assembled, in DWORDs.
template <bool dimInThreads, bool forceStartAt000>
size_t CmdUtil::BuildDispatchDirect(
    uint32          xDim,         // Thread groups (or threads) to launch (X dimension).
    uint32          yDim,         // Thread groups (or threads) to launch (Y dimension).
    uint32          zDim,         // Thread groups (or threads) to launch (Z dimension).
    Pm4Predicate    predicate,    // Predication enable control. Must be PredDisable on the Compute Engine.
    bool            isWave32,     // Meaningful for GFX10 only, set if wave-size is 32 for bound compute shader
    bool            useTunneling, // Meaningful for GFX10 only, set if dispatch tunneling should be used (VR)
    void*           pBuffer       // [out] Build the PM4 packet in this buffer.
    ) const
{
    regCOMPUTE_DISPATCH_INITIATOR dispatchInitiator;
    dispatchInitiator.u32All                     = 0;
    dispatchInitiator.bits.COMPUTE_SHADER_EN     = 1;
    dispatchInitiator.bits.FORCE_START_AT_000    = forceStartAt000;
    dispatchInitiator.bits.USE_THREAD_DIMENSIONS = dimInThreads;
    dispatchInitiator.gfx10.CS_W32_EN            = isWave32;
    if (IsGfx10(m_gfxIpLevel))
    {
        dispatchInitiator.gfx10.TUNNEL_ENABLE    = useTunneling;
    }

    // Set unordered mode to allow waves launch faster. This bit is related to the QoS (Quality of service) feature and
    // should be safe to set by default as the feature gets enabled only when allowed by the KMD. This bit also only
    // applies to asynchronous compute pipe and the graphics pipe simply ignores it.
    dispatchInitiator.bits.ORDER_MODE            = 1;

    static_assert((sizeof(PM4MEC_DISPATCH_DIRECT) == sizeof(PM4ME_DISPATCH_DIRECT)),
                  "MEC_DISPATCH_DIRECT packet definition has been updated, fix this!");

    constexpr uint32 PacketSize = (sizeof(PM4ME_DISPATCH_DIRECT) / sizeof(uint32));
    auto*const       pPacket    = static_cast<PM4MEC_DISPATCH_DIRECT*>(pBuffer);

    pPacket->header.u32All      = Type3Header(IT_DISPATCH_DIRECT, PacketSize, false, ShaderCompute, predicate);
    pPacket->dim_x              = xDim;
    pPacket->dim_y              = yDim;
    pPacket->dim_z              = zDim;
    pPacket->dispatch_initiator = dispatchInitiator.u32All;

    return PacketSize;
}

template
size_t CmdUtil::BuildDispatchDirect<true, true>(
    uint32          xDim,
    uint32          yDim,
    uint32          zDim,
    Pm4Predicate    predicate,
    bool            isWave32,
    bool            useTunneling,
    void*           pBuffer) const;
template
size_t CmdUtil::BuildDispatchDirect<false, false>(
    uint32          xDim,
    uint32          yDim,
    uint32          zDim,
    Pm4Predicate    predicate,
    bool            isWave32,
    bool            useTunneling,
    void*           pBuffer) const;
template
size_t CmdUtil::BuildDispatchDirect<false, true>(
    uint32          xDim,
    uint32          yDim,
    uint32          zDim,
    Pm4Predicate    predicate,
    bool            isWave32,
    bool            useTunneling,
    void*           pBuffer) const;

// =====================================================================================================================
// Builds a DISPATCH_INDIRECT packet for the GFX engine. Returns the size of the PM4 command assembled, in DWORDs.
// This packet has different sizes between ME compute and ME gfx.
size_t CmdUtil::BuildDispatchIndirectGfx(
    gpusize      byteOffset, // Offset from the address specified by the set-base packet where the compute params are
    Pm4Predicate predicate,  // Predication enable control
    bool         isWave32,   // Meaningful for GFX10 only, set if wave-size is 32 for bound compute shader
    void*        pBuffer)    // [out] Build the PM4 packet in this buffer.
{
    // We accept a 64-bit offset but the packet can only handle a 32-bit offset.
    PAL_ASSERT(HighPart(byteOffset) == 0);

    regCOMPUTE_DISPATCH_INITIATOR  dispatchInitiator;
    dispatchInitiator.u32All                   = 0;
    dispatchInitiator.bits.COMPUTE_SHADER_EN   = 1;
    dispatchInitiator.bits.FORCE_START_AT_000  = 1;
    dispatchInitiator.gfx10.CS_W32_EN          = isWave32;

    constexpr uint32 PacketSize = (sizeof(PM4ME_DISPATCH_INDIRECT) / sizeof(uint32));
    auto*const       pPacket    = static_cast<PM4ME_DISPATCH_INDIRECT*>(pBuffer);

    pPacket->header.u32All      = Type3Header(IT_DISPATCH_INDIRECT, PacketSize, false, ShaderCompute, predicate);
    pPacket->data_offset        = LowPart(byteOffset);
    pPacket->dispatch_initiator = dispatchInitiator.u32All;

    return PacketSize;
}

// =====================================================================================================================
// Builds a DISPATCH_INDIRECT packet for the MEC. Returns the size of the PM4 command assembled, in DWORDs.
// This packet has different sizes between ME compute and ME gfx.
size_t CmdUtil::BuildDispatchIndirectMec(
    gpusize       address,      // Address of the indirect args data.
    bool          isWave32,     // Meaningful for GFX10 only, set if wave-size is 32 for bound compute shader
    bool          useTunneling, // Meaningful for GFX10 only, set if dispatch tunneling should be used (VR)
    void*         pBuffer       // [out] Build the PM4 packet in this buffer.
    ) const
{
    // Address must be 32-bit aligned
    PAL_ASSERT ((address & 0x3) == 0);

    constexpr uint32               PacketSize        = (sizeof(PM4MEC_DISPATCH_INDIRECT) / sizeof(uint32));
    auto*const                     pPacket           = static_cast<PM4MEC_DISPATCH_INDIRECT*>(pBuffer);
    regCOMPUTE_DISPATCH_INITIATOR  dispatchInitiator = {};

    dispatchInitiator.u32All                   = 0;
    dispatchInitiator.bits.COMPUTE_SHADER_EN   = 1;
    dispatchInitiator.bits.FORCE_START_AT_000  = 1;
    dispatchInitiator.bits.ORDER_MODE          = 1;
    dispatchInitiator.gfx10.CS_W32_EN          = isWave32;
    if (IsGfx10(m_gfxIpLevel))
    {
        dispatchInitiator.gfx10.TUNNEL_ENABLE  = useTunneling;
    }

    pPacket->header.u32All      = Type3Header(IT_DISPATCH_INDIRECT, PacketSize);
    pPacket->addr_lo            = LowPart(address);
    pPacket->addr_hi            = HighPart(address);
    pPacket->dispatch_initiator = dispatchInitiator.u32All;

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
    constexpr uint32 PacketSize = (sizeof(PM4_PFP_DRAW_INDEX_2) / sizeof(uint32));
    auto*const       pPacket    = static_cast<PM4_PFP_DRAW_INDEX_2*>(pBuffer);

    pPacket->header.u32All = Type3Header(IT_DRAW_INDEX_2, PacketSize, false, ShaderGraphics, predicate);
    pPacket->max_size      = indexBufSize;
    pPacket->index_base_lo = LowPart(indexBufAddr);
    pPacket->index_base_hi = HighPart(indexBufAddr);
    pPacket->index_count   = indexCount;

    regVGT_DRAW_INITIATOR drawInitiator;
    drawInitiator.u32All                = 0;
    drawInitiator.bits.SOURCE_SELECT    = DI_SRC_SEL_DMA;
    drawInitiator.bits.MAJOR_MODE       = DI_MAJOR_MODE_0;

    pPacket->draw_initiator = drawInitiator.u32All;
    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 packet which issues an indexed draw using DRAW_INDEX_OFFSET_2. Returns the size of the PM4 command
// assembled, in DWORDs.
size_t CmdUtil::BuildDrawIndexOffset2(
    uint32       indexCount,
    uint32       indexBufSize,
    uint32       indexOffset,
    Pm4Predicate predicate,
    void*        pBuffer)
{
    constexpr uint32 PacketSize = (sizeof(PM4_PFP_DRAW_INDEX_OFFSET_2) / sizeof(uint32));
    auto*const       pPacket    = static_cast<PM4_PFP_DRAW_INDEX_OFFSET_2*>(pBuffer);

    pPacket->header.u32All = Type3Header(IT_DRAW_INDEX_OFFSET_2, PacketSize, false, ShaderGraphics, predicate);
    pPacket->max_size      = indexBufSize;
    pPacket->index_offset  = indexOffset;
    pPacket->index_count   = indexCount;

    regVGT_DRAW_INITIATOR drawInitiator;
    drawInitiator.u32All                = 0;
    drawInitiator.bits.SOURCE_SELECT    = DI_SRC_SEL_DMA;
    drawInitiator.bits.MAJOR_MODE       = DI_MAJOR_MODE_0;

    pPacket->draw_initiator = drawInitiator.u32All;
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

    constexpr uint32 PacketSize = (sizeof(PM4_PFP_DRAW_INDEX_AUTO) / sizeof(uint32));
    auto*const       pPacket    = static_cast<PM4_PFP_DRAW_INDEX_AUTO*>(pBuffer);

    pPacket->header.u32All = Type3Header(IT_DRAW_INDEX_AUTO, PacketSize, false, ShaderGraphics, predicate);
    pPacket->index_count   = indexCount;

    regVGT_DRAW_INITIATOR drawInitiator;
    drawInitiator.u32All                = 0;
    drawInitiator.bits.SOURCE_SELECT    = DI_SRC_SEL_AUTO_INDEX;
    drawInitiator.bits.MAJOR_MODE       = DI_MAJOR_MODE_0;
    drawInitiator.bits.USE_OPAQUE       = useOpaque ? 1 : 0;

    pPacket->draw_initiator = drawInitiator.u32All;
    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 packet which issues a multi indexed, indirect draw command into the given DE command stream. Returns the
// size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildDrawIndexIndirect(
    gpusize      offset,        // Byte offset to the indirect args data.
    uint32       baseVtxLoc,    // Register VS expects to read baseVtxLoc from.
    uint32       startInstLoc,  // Register VS expects to read startInstLoc from.
    uint32       startIndexLoc, // Register VS expects to read startIndexLoc from.
    Pm4Predicate predicate,
    void*        pBuffer        // [out] Build the PM4 packet in this buffer.
    ) const
{
    PAL_ASSERT(startIndexLoc == 0);

    // Draw argument offset in the buffer has to be 4-byte aligned.
    PAL_ASSERT(IsPow2Aligned(offset, 4));

    constexpr size_t DrawIndexIndirectPacketSize = sizeof(PM4PFP_DRAW_INDEX_INDIRECT) / sizeof(uint32);
    size_t packetSize = DrawIndexIndirectPacketSize;

    auto*const pPacket = static_cast<PM4PFP_DRAW_INDEX_INDIRECT*>(pBuffer);
    pPacket->header.u32All =
        Type3Header(IT_DRAW_INDEX_INDIRECT, DrawIndexIndirectPacketSize, false, ShaderGraphics, predicate);
    pPacket->data_offset               = LowPart(offset);
    pPacket->ordinal3                  = 0;
    pPacket->bitfields3.base_vtx_loc   = baseVtxLoc - PERSISTENT_SPACE_START;
    pPacket->ordinal4                  = 0;
    pPacket->bitfields4.start_inst_loc = startInstLoc - PERSISTENT_SPACE_START;

    if (startIndexLoc != UserDataNotMapped)
    {
        pPacket->bitfields4.start_indx_enable = 1;
        pPacket->bitfields3.start_indx_loc    = startIndexLoc - PERSISTENT_SPACE_START;
    }

    regVGT_DRAW_INITIATOR drawInitiator;
    drawInitiator.u32All             = 0;
    drawInitiator.bits.SOURCE_SELECT = DI_SRC_SEL_DMA;
    drawInitiator.bits.MAJOR_MODE    = DI_MAJOR_MODE_0;

    pPacket->draw_initiator = drawInitiator.u32All;
    return packetSize;
}

// =====================================================================================================================
// Builds a PM4 packet which issues an indexed, indirect draw command into the given DE command stream. Returns the size
// of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildDrawIndexIndirectMulti(
    gpusize      offset,        // Byte offset to the indirect args data.
    uint32       baseVtxLoc,    // Register VS expects to read baseVtxLoc from.
    uint32       startInstLoc,  // Register VS expects to read startInstLoc from.
    uint32       drawIndexLoc,  // Register VS expects to read drawIndex from.
    uint32       startIndexLoc, // Register VS expects to read startIndexLoc from.
    uint32       stride,        // Stride from one indirect args data structure to the next.
    uint32       count,         // Number of draw calls to loop through, or max draw calls if count is in GPU memory.
    gpusize      countGpuAddr,  // GPU address containing the count.
    Pm4Predicate predicate,
    void*        pBuffer        // [out] Build the PM4 packet in this buffer.
    ) const
{
    PAL_ASSERT(startIndexLoc == 0);

    // Draw argument offset in the buffer has to be 4-byte aligned.
    PAL_ASSERT(IsPow2Aligned(offset, 4));

    constexpr size_t DrawIndexIndirectMultiPacketSize = sizeof(PM4_PFP_DRAW_INDEX_INDIRECT_MULTI) / sizeof(uint32);
    size_t packetSize = DrawIndexIndirectMultiPacketSize;

    auto*const pPacket = static_cast<PM4_PFP_DRAW_INDEX_INDIRECT_MULTI*>(pBuffer);
    pPacket->header.u32All =
        Type3Header(IT_DRAW_INDEX_INDIRECT_MULTI, DrawIndexIndirectMultiPacketSize, false, ShaderGraphics, predicate);
    pPacket->data_offset               = LowPart(offset);
    pPacket->ordinal3                  = 0;
    pPacket->bitfields3.base_vtx_loc   = baseVtxLoc - PERSISTENT_SPACE_START;
    pPacket->ordinal4                  = 0;
    pPacket->bitfields4.start_inst_loc = startInstLoc - PERSISTENT_SPACE_START;
    pPacket->ordinal5                  = 0;
    pPacket->ordinal7                  = 0;

    if (drawIndexLoc != UserDataNotMapped)
    {
        pPacket->bitfields5.draw_index_enable = 1;
        pPacket->bitfields5.draw_index_loc    = drawIndexLoc - PERSISTENT_SPACE_START;
    }
    if (startIndexLoc != UserDataNotMapped)
    {
        pPacket->bitfields5.start_indx_enable = 1;
        pPacket->bitfields3.start_indx_loc    = startIndexLoc - PERSISTENT_SPACE_START;
    }

    if (countGpuAddr != 0)
    {
        pPacket->bitfields5.count_indirect_enable = 1;
        pPacket->ordinal7                         = LowPart(countGpuAddr);
        pPacket->count_addr_hi                    = HighPart(countGpuAddr);
    }
    else
    {
        pPacket->count_addr_hi = 0;
    }

    pPacket->count  = count;
    pPacket->stride = stride;

    regVGT_DRAW_INITIATOR drawInitiator;
    drawInitiator.u32All             = 0;
    drawInitiator.bits.SOURCE_SELECT = DI_SRC_SEL_DMA;
    drawInitiator.bits.MAJOR_MODE    = DI_MAJOR_MODE_0;

    pPacket->draw_initiator = drawInitiator.u32All;
    return packetSize;
}

// =====================================================================================================================
// Builds a PM4 packet which issues a draw indirect multi command into the given DE command stream. Returns the size of
// the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildDrawIndirectMulti(
    gpusize      offset,       // Byte offset to the indirect args data.
    uint32       baseVtxLoc,   // Register VS expects to read baseVtxLoc from.
    uint32       startInstLoc, // Register VS expects to read startInstLoc from.
    uint32       drawIndexLoc, // Register VS expects to read drawIndex from.
    uint32       stride,       // Stride from one indirect args data structure to the next.
    uint32       count,        // Number of draw calls to loop through, or max draw calls if count is in GPU memory.
    gpusize      countGpuAddr, // GPU address containing the count.
    Pm4Predicate predicate,
    void*        pBuffer)      // [out] Build the PM4 packet in this buffer.
{
    // Draw argument offset in the buffer has to be 4-byte aligned.
    PAL_ASSERT(IsPow2Aligned(offset, 4));

    constexpr uint32 PacketSize = (sizeof(PM4_PFP_DRAW_INDIRECT_MULTI) / sizeof(uint32));
    auto*const       pPacket    = static_cast<PM4_PFP_DRAW_INDIRECT_MULTI*>(pBuffer);

    pPacket->header.u32All = Type3Header(IT_DRAW_INDIRECT_MULTI, PacketSize, false, ShaderGraphics, predicate);
    pPacket->data_offset               = LowPart(offset);
    pPacket->ordinal3                  = 0;
    pPacket->bitfields3.start_vtx_loc  = baseVtxLoc - PERSISTENT_SPACE_START;
    pPacket->ordinal4                  = 0;
    pPacket->bitfields4.start_inst_loc = startInstLoc - PERSISTENT_SPACE_START;
    pPacket->ordinal5                  = 0;
    pPacket->ordinal7                  = 0;

    if (drawIndexLoc != UserDataNotMapped)
    {
        pPacket->bitfields5.draw_index_enable = 1;
        pPacket->bitfields5.draw_index_loc    = drawIndexLoc - PERSISTENT_SPACE_START;
    }

    if (countGpuAddr != 0)
    {
        pPacket->bitfields5.count_indirect_enable = 1;
        pPacket->ordinal7                         = LowPart(countGpuAddr);
        pPacket->count_addr_hi                    = HighPart(countGpuAddr);
    }
    else
    {
        pPacket->count_addr_hi = 0;
    }

    pPacket->count  = count;
    pPacket->stride = stride;

    regVGT_DRAW_INITIATOR drawInitiator;
    drawInitiator.u32All             = 0;
    drawInitiator.bits.SOURCE_SELECT = DI_SRC_SEL_AUTO_INDEX;
    drawInitiator.bits.MAJOR_MODE    = DI_MAJOR_MODE_0;

    pPacket->draw_initiator = drawInitiator.u32All;
    return PacketSize;
}

// =====================================================================================================================
// Constructs a DMA_DATA packet for any engine (PFP, ME, MEC).  Copies data from the source (can be immediate 32-bit
// data or a memory location) to a destination (either memory or a register).
size_t CmdUtil::BuildDmaData(
    DmaDataInfo&  dmaDataInfo,
    void*         pBuffer) // [out] Build the PM4 packet in this buffer.
{
    static_assert((static_cast<uint32>(sas__mec_dma_data__memory) == static_cast<uint32>(sas__pfp_dma_data__memory)),
                  "MEC and PFP sas dma_data enumerations don't match!");

    static_assert((static_cast<uint32>(das__mec_dma_data__memory) == static_cast<uint32>(das__pfp_dma_data__memory)),
                  "MEC and PFP das dma_data enumerations don't match!");

    static_assert(
        ((static_cast<uint32>(dst_sel__mec_dma_data__dst_addr_using_das)  ==
          static_cast<uint32>(dst_sel__pfp_dma_data__dst_addr_using_das)) &&
         (static_cast<uint32>(dst_sel__mec_dma_data__gds)                 ==
          static_cast<uint32>(dst_sel__pfp_dma_data__gds))                &&
         (static_cast<uint32>(dst_sel__mec_dma_data__dst_nowhere)         ==
          static_cast<uint32>(dst_sel__pfp_dma_data__dst_nowhere))        &&
         (static_cast<uint32>(dst_sel__mec_dma_data__dst_addr_using_l2)   ==
          static_cast<uint32>(dst_sel__pfp_dma_data__dst_addr_using_l2))),
        "MEC and PFP dst sel dma_data enumerations don't match!");

    static_assert(
        ((static_cast<uint32>(src_sel__mec_dma_data__src_addr_using_sas)  ==
          static_cast<uint32>(src_sel__pfp_dma_data__src_addr_using_sas)) &&
         (static_cast<uint32>(src_sel__mec_dma_data__gds)                 ==
          static_cast<uint32>(src_sel__pfp_dma_data__gds))                &&
         (static_cast<uint32>(src_sel__mec_dma_data__data)                ==
          static_cast<uint32>(src_sel__pfp_dma_data__data))               &&
         (static_cast<uint32>(src_sel__mec_dma_data__src_addr_using_l2)   ==
          static_cast<uint32>(src_sel__pfp_dma_data__src_addr_using_l2))),
        "MEC and PFP src sel dma_data enumerations don't match!");

    static_assert((sizeof(PM4PFP_DMA_DATA) == sizeof(PM4ME_DMA_DATA)),
                  "PFP, ME and MEC versions of the DMA_DATA packet are not the same size!");

    // The "byte_count" field only has 26 bits (numBytes must be less than 64MB).
    PAL_ASSERT(dmaDataInfo.numBytes < (1 << 26));

    constexpr uint32 PacketSize = (sizeof(PM4PFP_DMA_DATA) / sizeof(uint32));
    auto*const       pPacket    = static_cast<PM4PFP_DMA_DATA*>(pBuffer);

    pPacket->header.u32All         = Type3Header(IT_DMA_DATA, PacketSize, false, ShaderGraphics, dmaDataInfo.predicate);
    pPacket->ordinal2              = 0;
    pPacket->bitfields2.engine_sel = static_cast<PFP_DMA_DATA_engine_sel_enum>(dmaDataInfo.usePfp
                                        ? static_cast<uint32>(engine_sel__pfp_dma_data__prefetch_parser)
                                        : static_cast<uint32>(engine_sel__me_dma_data__micro_engine));
    pPacket->bitfields2.dst_sel    = dmaDataInfo.dstSel;
    pPacket->bitfields2.src_sel    = dmaDataInfo.srcSel;
    pPacket->bitfields2.cp_sync    = (dmaDataInfo.sync ? 1 : 0);

    if (dmaDataInfo.srcSel == src_sel__pfp_dma_data__data)
    {
        pPacket->src_addr_lo_or_data = dmaDataInfo.srcData;
        pPacket->src_addr_hi         = 0; // ignored for data
    }
    else
    {
        pPacket->src_addr_lo_or_data = LowPart(dmaDataInfo.srcAddr);
        pPacket->src_addr_hi         = HighPart(dmaDataInfo.srcAddr);
    }

    pPacket->dst_addr_lo           = LowPart(dmaDataInfo.dstAddr);
    pPacket->dst_addr_hi           = HighPart(dmaDataInfo.dstAddr);
    pPacket->ordinal7              = 0;
    pPacket->bitfields7.byte_count = dmaDataInfo.numBytes;
    pPacket->bitfields7.sas        = dmaDataInfo.srcAddrSpace;
    pPacket->bitfields7.das        = dmaDataInfo.dstAddrSpace;
    pPacket->bitfields7.raw_wait   = (dmaDataInfo.rawWait ? 1 : 0);
    pPacket->bitfields7.dis_wc     = (dmaDataInfo.disWc   ? 1 : 0);

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 constant engine command to dump the specified amount of data from CE RAM into GPU memory through the L2
// cache. Returns the size of the PM4 command built, in DWORDs.
size_t CmdUtil::BuildDumpConstRam(
    gpusize dstGpuAddr,
    uint32  ramByteOffset,
    uint32  dwordSize,     // Amount of data to dump, in DWORDs.
    void*   pBuffer)       // [out] Build the PM4 packet in this buffer.
{
    PAL_ASSERT(IsPow2Aligned(dstGpuAddr, 4));
    PAL_ASSERT(IsPow2Aligned(ramByteOffset, 4));
    PAL_ASSERT(dwordSize != 0);

    constexpr uint32 PacketSize = (sizeof(PM4_CE_DUMP_CONST_RAM) / sizeof(uint32));
    auto*const       pPacket    = static_cast<PM4_CE_DUMP_CONST_RAM*>(pBuffer);

    DumpConstRamOrdinal2 ordinal2 = { };
    ordinal2.bits.offset = ramByteOffset;

    pPacket->header.u32All     = Type3Header(IT_DUMP_CONST_RAM, PacketSize);
    pPacket->ordinal2          = ordinal2.u32All;
    pPacket->ordinal3          = 0;
    pPacket->bitfields3.num_dw = dwordSize;
    pPacket->addr_lo           = LowPart(dstGpuAddr);
    pPacket->addr_hi           = HighPart(dstGpuAddr);

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 constant engine command to dump the specified amount of data from CE RAM into indirect GPU memory offset
// through the L2 cache. The base address is set via SET_BASE packet.
// Returns the size of the PM4 command built, in DWORDs.
size_t CmdUtil::BuildDumpConstRamOffset(
    uint32  dstAddrOffset,
    uint32  ramByteOffset,
    uint32  dwordSize,     // Amount of data to dump, in DWORDs.
    void*   pBuffer)       // [out] Build the PM4 packet in this buffer.
{
    PAL_ASSERT(IsPow2Aligned(dstAddrOffset, 4));
    PAL_ASSERT(IsPow2Aligned(ramByteOffset, 4));
    PAL_ASSERT(dwordSize != 0);

    constexpr uint32 PacketSize = (sizeof(PM4_CE_DUMP_CONST_RAM_OFFSET) / sizeof(uint32));
    auto*const       pPacket    = static_cast<PM4_CE_DUMP_CONST_RAM_OFFSET*>(pBuffer);

    DumpConstRamOrdinal2 ordinal2 = { };
    ordinal2.bits.offset = ramByteOffset;

    pPacket->header.u32All     = Type3Header(IT_DUMP_CONST_RAM_OFFSET, PacketSize);
    pPacket->ordinal2          = ordinal2.u32All;
    pPacket->ordinal3          = 0;
    pPacket->bitfields3.num_dw = dwordSize;
    pPacket->addr_offset       = dstAddrOffset;

    return PacketSize;
}

// =====================================================================================================================
// Build an EVENT_WRITE packet.  Not to be used for any EOP, EOS or SAMPLE_XXXXX type events.  Return the number of
// DWORDs taken up by this packet.
size_t CmdUtil::BuildNonSampleEventWrite(
    VGT_EVENT_TYPE  vgtEvent,
    EngineType      engineType,
    void*           pBuffer)    // [out] Build the PM4 packet in this buffer.
{
    // Verify the event index enumerations match between the ME and MEC engines.  Note that ME (gfx) has more
    // events than MEC does.  We assert below if this packet is meant for compute and a gfx-only index is selected.
    static_assert(
        ((static_cast<uint32>(event_index__mec_event_write__other)                  ==
          static_cast<uint32>(event_index__me_event_write__other))                  &&
         (static_cast<uint32>(event_index__mec_event_write__cs_partial_flush)       ==
          static_cast<uint32>(event_index__me_event_write__cs_vs_ps_partial_flush)) &&
         (static_cast<uint32>(event_index__mec_event_write__sample_pipelinestat)   ==
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

    // If this trips, the caller needs to use the BuildSampleEventWrite() routine instead.
    PAL_ASSERT(VgtEventIndex[vgtEvent] != event_index__me_event_write__sample_streamoutstats);

    // Don't use sizeof(PM4ME_EVENT_WRITE) here!  The official packet definition contains extra dwords
    // for functionality that is only required for "sample" type events.
    constexpr uint32 PacketSize = (sizeof(PM4ME_NON_SAMPLE_EVENT_WRITE) / sizeof(uint32));
    PM4ME_EVENT_WRITE packet;

    packet.header.u32All      = Type3Header(IT_EVENT_WRITE, PacketSize);
    packet.ordinal2           = 0;

    // CS_PARTIAL_FLUSH is only allowed on engines that support compute operations
    PAL_ASSERT((vgtEvent != CS_PARTIAL_FLUSH) || Pal::Device::EngineSupportsCompute(engineType));

    // Enable offload compute queue until EOP queue goes empty to increase multi-queue concurrency
    if ((engineType == EngineTypeCompute) && (vgtEvent == CS_PARTIAL_FLUSH))
    {
        auto*const pPacketMec = reinterpret_cast<PM4MEC_EVENT_WRITE*>(&packet);

        pPacketMec->bitfields2.offload_enable = 1;
    }

    packet.bitfields2.event_type  = vgtEvent;
    packet.bitfields2.event_index = VgtEventIndex[vgtEvent];

    memcpy(pBuffer, &packet, PacketSize * sizeof(uint32));
    return PacketSize;
}

// =====================================================================================================================
// Build an EVENT_WRITE packet.  Not to be used for any EOP or EOS type events.  Return the number of DWORDs taken up
// by this packet.
size_t CmdUtil::BuildSampleEventWrite(
    VGT_EVENT_TYPE  vgtEvent,
    EngineType      engineType,
    gpusize         gpuAddr,
    void*           pBuffer)    // [out] Build the PM4 packet in this buffer.
{
    // Verify the event index enumerations match between the ME and MEC engines.  Note that ME (gfx) has more
    // events than MEC does.  We assert below if this packet is meant for compute and a gfx-only index is selected.
    static_assert(
        ((static_cast<uint32>(event_index__mec_event_write__other)                  ==
          static_cast<uint32>(event_index__me_event_write__other))                  &&
         (static_cast<uint32>(event_index__mec_event_write__cs_partial_flush)       ==
          static_cast<uint32>(event_index__me_event_write__cs_vs_ps_partial_flush)) &&
         (static_cast<uint32>(event_index__mec_event_write__sample_pipelinestat)   ==
          static_cast<uint32>(event_index__me_event_write__sample_pipelinestat))),
        "event index enumerations don't match between gfx and compute!");

    // Make sure the supplied VGT event is legal.
    PAL_ASSERT(vgtEvent < (sizeof(VgtEventIndex) / sizeof(VGT_EVENT_TYPE)));

    // Note that ZPASS_DONE is marked as deprecated in gfx9 but still works and is required for at least one workaround.
    PAL_ASSERT((vgtEvent == PIXEL_PIPE_STAT_CONTROL) ||
               (vgtEvent == PIXEL_PIPE_STAT_DUMP)    ||
               (vgtEvent == SAMPLE_PIPELINESTAT)     ||
               (vgtEvent == SAMPLE_STREAMOUTSTATS)   ||
               (vgtEvent == SAMPLE_STREAMOUTSTATS1)  ||
               (vgtEvent == SAMPLE_STREAMOUTSTATS2)  ||
               (vgtEvent == SAMPLE_STREAMOUTSTATS3)  ||
               (vgtEvent == ZPASS_DONE));

    PAL_ASSERT((VgtEventIndex[vgtEvent] == event_index__me_event_write__zpass_pixel_pipe_stat_control_or_dump) ||
               (VgtEventIndex[vgtEvent] == event_index__me_event_write__sample_pipelinestat)                  ||
               (VgtEventIndex[vgtEvent] == event_index__me_event_write__sample_streamoutstats));

    // Event-write packets destined for the compute queue can only use some events.
    PAL_ASSERT((engineType != EngineTypeCompute) ||
               (static_cast<uint32>(VgtEventIndex[vgtEvent]) ==
                static_cast<uint32>(event_index__mec_event_write__sample_pipelinestat)));

    constexpr uint32 PacketSize = (sizeof(PM4_ME_EVENT_WRITE) / sizeof(uint32));
    auto*const       pPacket    = static_cast<PM4_ME_EVENT_WRITE*>(pBuffer);

    pPacket->header.u32All          = Type3Header(IT_EVENT_WRITE, PacketSize);
    pPacket->ordinal2               = 0;
    pPacket->bitfields2.event_type  = vgtEvent;
    pPacket->bitfields2.event_index = VgtEventIndex[vgtEvent];
    pPacket->ordinal3               = LowPart(gpuAddr);
    PAL_ASSERT(pPacket->bitfields3a.reserved1 == 0);
    pPacket->address_hi             = HighPart(gpuAddr);

    return PacketSize;
}

// =====================================================================================================================
size_t CmdUtil::BuildExecutionMarker(
    gpusize markerAddr,
    uint32  markerVal,
    uint64  clientHandle,
    uint32  markerType,
    void*   pBuffer
    ) const
{
    ReleaseMemInfo releaseInfo = {};
    releaseInfo.engineType     = EngineTypeUniversal;
    releaseInfo.vgtEvent       = BOTTOM_OF_PIPE_TS;
    releaseInfo.tcCacheOp      = TcCacheOp::Nop;
    releaseInfo.dstAddr        = markerAddr;
    releaseInfo.dataSel        = data_sel__me_release_mem__send_32_bit_low;
    releaseInfo.data           = markerVal;

    size_t packetSize = BuildReleaseMem(releaseInfo, pBuffer);
    pBuffer = VoidPtrInc(pBuffer, packetSize * sizeof(uint32));

    constexpr uint32 NopSizeDwords = sizeof(PM4PFP_NOP) / sizeof(uint32);

    if (markerType == RGD_EXECUTION_BEGIN_MARKER_GUARD)
    {
        constexpr size_t BeginPayloadSize = sizeof(RgdExecutionBeginMarker) / sizeof(uint32);
        packetSize += BuildNop(BeginPayloadSize + NopSizeDwords, pBuffer);

        auto* pPayload =
            reinterpret_cast<RgdExecutionBeginMarker*>(VoidPtrInc(pBuffer, NopSizeDwords * sizeof(uint32)));
        pPayload->guard         = RGD_EXECUTION_BEGIN_MARKER_GUARD;
        pPayload->marker_buffer = markerAddr;
        pPayload->client_handle = clientHandle;
        pPayload->counter       = markerVal;
    }
    else if (markerType == RGD_EXECUTION_MARKER_GUARD)
    {
        PAL_ASSERT(clientHandle == 0);
        constexpr size_t MarkerPayloadSize = sizeof(RgdExecutionMarker) / sizeof(uint32);
        packetSize += BuildNop(MarkerPayloadSize + NopSizeDwords, pBuffer);

        auto* pPayload =
            reinterpret_cast<RgdExecutionMarker*>(VoidPtrInc(pBuffer, NopSizeDwords * sizeof(uint32)));
        pPayload->guard   = RGD_EXECUTION_MARKER_GUARD;
        pPayload->counter = markerVal;
    }
    else if (markerType == RGD_EXECUTION_END_MARKER_GUARD)
    {
        PAL_ASSERT(clientHandle == 0);
        constexpr size_t EndPayloadSize = sizeof(RgdExecutionEndMarker) / sizeof(uint32);
        packetSize += BuildNop(EndPayloadSize + NopSizeDwords, pBuffer);

        auto* pPayload =
            reinterpret_cast<RgdExecutionEndMarker*>(VoidPtrInc(pBuffer, NopSizeDwords * sizeof(uint32)));
        pPayload->guard = RGD_EXECUTION_END_MARKER_GUARD;
        pPayload->counter = markerVal;
    }

    return packetSize;
}

// =====================================================================================================================
// Builds a PM4 constant engine command to increment the CE counter. Returns the size of the PM4 command built, in
// DWORDs.
size_t CmdUtil::BuildIncrementCeCounter(
    void* pBuffer) // [out] Build the PM4 packet in this buffer.
{
    constexpr uint32 PacketSize = (sizeof(PM4_CE_INCREMENT_CE_COUNTER) / sizeof(uint32));
    auto*const       pPacket    = static_cast<PM4_CE_INCREMENT_CE_COUNTER*>(pBuffer);

    pPacket->header.u32All      = Type3Header(IT_INCREMENT_CE_COUNTER, PacketSize);
    pPacket->ordinal2           = 0;
    pPacket->bitfields2.cntrsel = cntrsel__ce_increment_ce_counter__increment_ce_counter;

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 command to increment the DE counter. Returns the size of the PM4 command built, in DWORDs.
size_t CmdUtil::BuildIncrementDeCounter(
    void* pBuffer) // [out] Build the PM4 packet in this buffer.
{
    constexpr uint32 PacketSize = (sizeof(PM4_ME_INCREMENT_DE_COUNTER) / sizeof(uint32));
    auto*const       pPacket    = static_cast<PM4_ME_INCREMENT_DE_COUNTER*>(pBuffer);

    pPacket->header.u32All = Type3Header(IT_INCREMENT_DE_COUNTER, PacketSize);
    pPacket->dummy_data    = 0;

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 packet which issues an "index attributes indirect" command into the given DE stream. Return the size of
// the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildIndexAttributesIndirect(
    gpusize baseAddr,   // Base address of an array of index attributes
    uint16  index,      // Index into the array of index attributes to load
    void*   pBuffer)    // [out] Build the PM4 packet in this buffer.
{
    constexpr size_t PacketSize = sizeof(PM4_PFP_INDEX_ATTRIBUTES_INDIRECT) / sizeof(uint32);
    auto*const       pPacket    = static_cast<PM4_PFP_INDEX_ATTRIBUTES_INDIRECT*>(pBuffer);

    pPacket->header.u32All              = Type3Header(IT_INDEX_ATTRIBUTES_INDIRECT, PacketSize);
    pPacket->ordinal2                   = LowPart(baseAddr);
    PAL_ASSERT(pPacket->bitfields2.reserved1 == 0); // Address must be 4-DWORD aligned
    pPacket->attribute_base_hi          = HighPart(baseAddr);
    pPacket->ordinal4                   = 0;
    pPacket->bitfields4.attribute_index = index;

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

    constexpr uint32 PacketSize = (sizeof(PM4_PFP_INDEX_BASE) / sizeof(uint32));
    auto*const       pPacket    = static_cast<PM4_PFP_INDEX_BASE*>(pBuffer);

    pPacket->header.u32All            = Type3Header(IT_INDEX_BASE, PacketSize);
    pPacket->ordinal2                 = LowPart(baseAddr);
    PAL_ASSERT(pPacket->bitfields2.reserved1 == 0);
    pPacket->index_base_hi            = HighPart(baseAddr);

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 packet which issues a "index buffer size" command into the given DE command stream. Returns the size of
// the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildIndexBufferSize(
    uint32 indexCount,
    void*  pBuffer)     // [out] Build the PM4 packet in this buffer.
{
    constexpr uint32 PacketSize = (sizeof(PM4_PFP_INDEX_BUFFER_SIZE) / sizeof(uint32));
    auto*const       pPacket    = static_cast<PM4_PFP_INDEX_BUFFER_SIZE*>(pBuffer);

    pPacket->header.u32All     = Type3Header(IT_INDEX_BUFFER_SIZE, PacketSize);
    pPacket->index_buffer_size = indexCount;

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 packet which issues a "index type" command into the given DE command stream. Returns the size of the PM4
// command assembled, in DWORDs.
size_t CmdUtil::BuildIndexType(
    uint32   vgtDmaIndexType, // value associated with the VGT_DMA_INDEX_TYPE register
    void*    pBuffer          // [out] Build the PM4 packet in this buffer.
    ) const
{
    const size_t PacketSize     = BuildSetOneConfigReg(mmVGT_INDEX_TYPE,
                                                       pBuffer,
                                                       index__pfp_set_uconfig_reg_index__index_type);
    const size_t RegisterOffset = PacketSize - (sizeof(regVGT_INDEX_TYPE) / sizeof(uint32));

    uint32* pPacket         = static_cast<uint32*>(pBuffer);
    pPacket[RegisterOffset] = vgtDmaIndexType;

    return PacketSize;
}

// =====================================================================================================================
// Builds an indirect-buffer packet for graphics with optional chaining support.
// Returns the size of the packet, in DWORDs
size_t CmdUtil::BuildIndirectBuffer(
    EngineType engineType, // queue this IB will be executed on
    gpusize    ibAddr,     // gpu virtual address of the indirect buffer
    uint32     ibSize,     // size of indirect buffer in dwords
    bool       chain,
    bool       constantEngine,
    bool       enablePreemption,
    void*      pBuffer)    // space to place the newly-generated PM4 packet into
{
    static_assert((sizeof(PM4PFP_INDIRECT_BUFFER) == sizeof(PM4MEC_INDIRECT_BUFFER)),
                  "Indirect buffer packets are not the same size between GFX and compute!");

    constexpr uint32    PacketSize = (sizeof(PM4MEC_INDIRECT_BUFFER) / sizeof(uint32));
    auto*const          pMecPacket = static_cast<PM4MEC_INDIRECT_BUFFER*>(pBuffer);
    auto*const          pPfpPacket = static_cast<PM4PFP_INDIRECT_BUFFER*>(pBuffer);
    const IT_OpCodeType opCode     = constantEngine ? IT_INDIRECT_BUFFER_CNST : IT_INDIRECT_BUFFER;

    pPfpPacket->header.u32All = Type3Header(opCode, PacketSize);
    pPfpPacket->ordinal2      = LowPart(ibAddr);
    pPfpPacket->ib_base_hi    = HighPart(ibAddr);

    // Make sure our address is properly aligned
    PAL_ASSERT(pPfpPacket->bitfields2.reserved1 == 0);

    pPfpPacket->ordinal4           = 0;
    pPfpPacket->bitfields4.ib_size = ibSize;
    pPfpPacket->bitfields4.chain   = chain;

    if (engineType == EngineTypeCompute)
    {
        // This bit only exists on the compute version of this packet.
        pMecPacket->bitfields4.valid = 1;
        PAL_ASSERT(enablePreemption == false);
    }
    else
    {
        pPfpPacket->bitfields4.pre_ena = enablePreemption;
    }

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 constant engine command to load the specified amount of data from GPU memory into CE RAM. Returns the
// size of the PM4 command built, in DWORDs.
size_t CmdUtil::BuildLoadConstRam(
    gpusize srcGpuAddr,
    uint32  ramByteOffset,
    uint32  dwordSize,     // Amount of data to load, in DWORDs. Must be a multiple of 8
    void*   pBuffer)       // [out] Build the PM4 packet in this buffer.
{
    PAL_ASSERT(IsPow2Aligned(srcGpuAddr, 32));
    PAL_ASSERT(IsPow2Aligned(ramByteOffset, 32));
    PAL_ASSERT(IsPow2Aligned(dwordSize, 8));

    constexpr uint32 PacketSize = (sizeof(PM4_CE_LOAD_CONST_RAM) / sizeof(uint32));
    auto*const       pPacket    = static_cast<PM4_CE_LOAD_CONST_RAM*>(pBuffer);

    pPacket->header.u32All         = Type3Header(IT_LOAD_CONST_RAM, PacketSize);
    pPacket->addr_lo               = LowPart(srcGpuAddr);
    pPacket->addr_hi               = HighPart(srcGpuAddr);
    pPacket->ordinal4              = 0;
    pPacket->bitfields4.num_dw     = dwordSize;
    pPacket->ordinal5              = 0;
    pPacket->bitfields5.start_addr = ramByteOffset;

    return PacketSize;
}

// =====================================================================================================================
// Builds a NOP command as long as the specified number of DWORDs. Returns the size of the PM4 command built, in DWORDs
size_t CmdUtil::BuildNop(
    size_t numDwords,
    void*  pBuffer)   // [out] Build the PM4 packet in this buffer.
{
    static_assert(((sizeof(PM4PFP_NOP) == sizeof(PM4MEC_NOP)) &&
                   (sizeof(PM4PFP_NOP) == sizeof(PM4CE_NOP))),
                  "graphics, compute and constant versions of the NOP packet don't match!");

    PM4PFP_NOP* pPacket = static_cast<PM4PFP_NOP*>(pBuffer);

    if (numDwords == 0)
    {
        // No padding required.
    }
    else if (numDwords == 1)
    {
        // NOP packets with a maxed-out size field (0x3FFF) are one dword long (i.e., header only).  The "Type3Header"
        // function will subtract two from the size field, so add two here.
        pPacket->header.u32All = Type3Header(IT_NOP, 0x3FFF + 2);
    }
    else
    {
        pPacket->header.u32All = Type3Header(IT_NOP, static_cast<uint32>(numDwords));
    }

    return numDwords;
}

// =====================================================================================================================
// Builds a PM4 packet which issues a "num instances" command into the given DE command stream. Returns the Size of the
// PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildNumInstances(
    uint32 instanceCount,
    void*  pBuffer)       // [out] Build the PM4 packet in this buffer.
{
    constexpr uint32 PacketSize = (sizeof(PM4_PFP_NUM_INSTANCES) / sizeof(uint32));
    auto*const       pPacket    = static_cast<PM4_PFP_NUM_INSTANCES*>(pBuffer);

    pPacket->header.u32All = Type3Header(IT_NUM_INSTANCES, PacketSize);
    pPacket->num_instances = instanceCount;

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
    // the "query pool counters addess".
    constexpr size_t PacketSize = OcclusionQuerySizeDwords;
    auto*const       pPacket    = static_cast<PM4PFP_OCCLUSION_QUERY*>(pBuffer);

    pPacket->header.u32All = Type3Header(IT_OCCLUSION_QUERY, PacketSize);
    pPacket->ordinal2      = LowPart(queryMemAddr);
    pPacket->start_addr_hi = HighPart(queryMemAddr);
    pPacket->ordinal4      = LowPart(dstMemAddr);
    pPacket->query_addr_hi = HighPart(dstMemAddr);

    // The query address should be 16-byte aligned.
    PAL_ASSERT((pPacket->bitfields2.reserved1 == 0) && (queryMemAddr != 0));

    // The destination address should be 4-byte aligned.
    PAL_ASSERT((pPacket->bitfields4.reserved1 == 0) && (dstMemAddr != 0));

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 packet which issues a "prime UtcL2" command into the given command stream. Returns the size of the PM4
// command assembled, in DWORDs.
size_t CmdUtil::BuildPrimeUtcL2(
    gpusize gpuAddr,
    uint32  cachePerm,      // XXX_PRIME_UTCL2_cache_perm_enum
    uint32  primeMode,      // XXX_PRIME_UTCL2_prime_mode_enum
    uint32  engineSel,      // XXX_PRIME_UTCL2_engine_sel_enum
    size_t  requestedPages, // Number of 4KB pages to prefetch.
    void*   pBuffer)
{
    static_assert(((sizeof(PM4_PFP_PRIME_UTCL2) == sizeof(PM4_ME_PRIME_UTCL2))  &&
                   (sizeof(PM4_PFP_PRIME_UTCL2) == sizeof(PM4_MEC_PRIME_UTCL2)) &&
                   (sizeof(PM4_PFP_PRIME_UTCL2) == sizeof(PM4_CE_PRIME_UTCL2))),
                   "PRIME_UTCL2 packet is different between PFP, ME, MEC, and CE!");

    static_assert(((static_cast<uint32>(cache_perm__pfp_prime_utcl2__read)     ==
                    static_cast<uint32>(cache_perm__me_prime_utcl2__read))     &&
                   (static_cast<uint32>(cache_perm__pfp_prime_utcl2__read)     ==
                    static_cast<uint32>(cache_perm__mec_prime_utcl2__read))    &&
                   (static_cast<uint32>(cache_perm__pfp_prime_utcl2__read)     ==
                    static_cast<uint32>(cache_perm__ce_prime_utcl2__read))     &&
                   (static_cast<uint32>(cache_perm__pfp_prime_utcl2__write)    ==
                    static_cast<uint32>(cache_perm__me_prime_utcl2__write))    &&
                   (static_cast<uint32>(cache_perm__pfp_prime_utcl2__write)    ==
                    static_cast<uint32>(cache_perm__mec_prime_utcl2__write))   &&
                   (static_cast<uint32>(cache_perm__pfp_prime_utcl2__write)    ==
                    static_cast<uint32>(cache_perm__ce_prime_utcl2__write))    &&
                   (static_cast<uint32>(cache_perm__pfp_prime_utcl2__execute)  ==
                    static_cast<uint32>(cache_perm__me_prime_utcl2__execute))  &&
                   (static_cast<uint32>(cache_perm__pfp_prime_utcl2__execute)  ==
                    static_cast<uint32>(cache_perm__mec_prime_utcl2__execute)) &&
                   (static_cast<uint32>(cache_perm__pfp_prime_utcl2__execute)  ==
                    static_cast<uint32>(cache_perm__ce_prime_utcl2__execute))),
                  "Cache permissions enum is different between PFP, ME, MEC, and CE!");

    static_assert(((static_cast<uint32>(prime_mode__pfp_prime_utcl2__dont_wait_for_xack)  ==
                    static_cast<uint32>(prime_mode__me_prime_utcl2__dont_wait_for_xack))  &&
                   (static_cast<uint32>(prime_mode__pfp_prime_utcl2__dont_wait_for_xack)  ==
                    static_cast<uint32>(prime_mode__mec_prime_utcl2__dont_wait_for_xack)) &&
                   (static_cast<uint32>(prime_mode__pfp_prime_utcl2__dont_wait_for_xack)  ==
                    static_cast<uint32>(prime_mode__ce_prime_utcl2__dont_wait_for_xack))  &&
                   (static_cast<uint32>(prime_mode__pfp_prime_utcl2__wait_for_xack)       ==
                    static_cast<uint32>(prime_mode__me_prime_utcl2__wait_for_xack))       &&
                   (static_cast<uint32>(prime_mode__pfp_prime_utcl2__wait_for_xack)       ==
                    static_cast<uint32>(prime_mode__mec_prime_utcl2__wait_for_xack))      &&
                   (static_cast<uint32>(prime_mode__pfp_prime_utcl2__wait_for_xack)       ==
                    static_cast<uint32>(prime_mode__ce_prime_utcl2__wait_for_xack))),
                  "Prime mode enum is different between PFP, ME, MEC, and CE!");

    constexpr uint32 PacketSize = (sizeof(PM4_PFP_PRIME_UTCL2) / sizeof(uint32));
    auto*const       pPacket    = static_cast<PM4_PFP_PRIME_UTCL2*>(pBuffer);

    pPacket->header.u32All              = Type3Header(IT_PRIME_UTCL2, PacketSize);
    pPacket->ordinal2                   = 0;
    pPacket->bitfields2.cache_perm      = static_cast<PFP_PRIME_UTCL2_cache_perm_enum>(cachePerm);
    pPacket->bitfields2.prime_mode      = static_cast<PFP_PRIME_UTCL2_prime_mode_enum>(primeMode);
    pPacket->bitfields2.engine_sel      = static_cast<PFP_PRIME_UTCL2_engine_sel_enum>(engineSel);
    PAL_ASSERT(pPacket->bitfields2.reserved1 == 0);
    pPacket->addr_lo                    = LowPart(gpuAddr);
    // Address must be 4KB aligned.
    PAL_ASSERT((pPacket->addr_lo & (PrimeUtcL2MemAlignment - 1)) == 0);
    pPacket->addr_hi                    = HighPart(gpuAddr);
    pPacket->ordinal5                   = 0;
    pPacket->bitfields5.requested_pages = static_cast<uint32>(requestedPages);
    PAL_ASSERT(pPacket->bitfields5.reserved1 == 0);

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 packet which reads a context register, masks off a portion of it, then writes the provided data to the
// masked off fields. The register mask applies to the fields being written to, as follows:
//      newRegVal = (oldRegVal & ~regMask) | (regData & regMask)
// Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildContextRegRmw(
    uint32 regAddr,
    uint32 regMask,
    uint32 regData,
    void*  pBuffer  // [out] Build the PM4 packet in this buffer.
    ) const
{
#if PAL_ENABLE_PRINTS_ASSERTS
    CheckShadowedContextReg(regAddr);
#endif

    constexpr uint32 PacketSize = ContextRegRmwSizeDwords;
    auto*const       pPacket    = static_cast<PM4_ME_CONTEXT_REG_RMW*>(pBuffer);

    pPacket->header.u32All         = Type3Header(IT_CONTEXT_REG_RMW, PacketSize);
    pPacket->ordinal2              = 0;
    pPacket->bitfields2.reg_offset = regAddr - CONTEXT_SPACE_START;
    pPacket->reg_mask              = regMask;
    pPacket->reg_data              = regData;

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 packet which reads a config register, and performs immediate mode AND and OR operations on the regVal
// using the masks provided as follows:
//     newRegVal = (oldRegVal & andMask) | (orMask)
// Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildRegRmw(
    uint32 regAddr,
    uint32 orMask,
    uint32 andMask,
    void*  pBuffer  // [out] Build the PM4 packet in this buffer.
    ) const
{
    PAL_ASSERT(IsUserConfigReg(regAddr));

    constexpr size_t PacketSize = RegRmwSizeDwords;
    auto*const       pPacket    = static_cast<PM4_ME_REG_RMW*>(pBuffer);

    pPacket->header.u32All              = Type3Header(IT_REG_RMW, PacketSize);
    pPacket->ordinal2                   = 0;
    pPacket->bitfields2.mod_addr        = regAddr;
    pPacket->bitfields2.shadow_base_sel = shadow_base_sel__me_reg_rmw__no_shadow;
    pPacket->bitfields2.or_mask_src     = or_mask_src__me_reg_rmw__immediate;
    pPacket->bitfields2.and_mask_src    = and_mask_src__me_reg_rmw__immediate;
    pPacket->or_mask                    = orMask;
    pPacket->and_mask                   = andMask;

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 packet which issues a load_config_reg command to load multiple groups of consecutive config registers
// from video memory.  Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildLoadConfigRegs(
    gpusize              gpuVirtAddr,
    const RegisterRange* pRanges,
    uint32               rangeCount,
    void*                pBuffer)      // [out] Build the PM4 packet in this buffer.
{
    PAL_ASSERT((pRanges != nullptr) && (rangeCount >= 1));

    // The GPU virtual address must be DWORD-aligned and not use more than 48 bits.
    PAL_ASSERT(IsPow2Aligned(gpuVirtAddr, 4));
    PAL_ASSERT((HighPart(gpuVirtAddr) & 0xFFFF0000) == 0);

    const uint32 packetSize = (sizeof(PM4PFP_LOAD_CONFIG_REG) / sizeof(uint32)) + (2 * (rangeCount - 1));
    auto*const   pPacket    = static_cast<PM4PFP_LOAD_CONFIG_REG*>(pBuffer);

    pPacket->header.u32All           = Type3Header(IT_LOAD_CONFIG_REG, packetSize);
    pPacket->ordinal2                = 0;
    pPacket->bitfields2.base_addr_lo = (LowPart(gpuVirtAddr) >> 2);
    pPacket->base_addr_hi            = HighPart(gpuVirtAddr);

    // Note: This is a variable-length packet. The PM4PFP_LOAD_CONFIG_REG packet contains space for the first register
    // range, but not the others (though they are expected to immediately follow in the command buffer).
    memcpy(&pPacket->ordinal4, pRanges, (sizeof(RegisterRange) * rangeCount));

    return packetSize;
}

// =====================================================================================================================
// Builds a PM4 packet which issues a load_context_reg command to load a single group of consecutive context registers
// from video memory.  Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildLoadContextRegs(
    gpusize gpuVirtAddr,
    uint32  startRegAddr,
    uint32  count,
    void*   pBuffer)      // [out] Build the PM4 packet in this buffer.
{
    PAL_ASSERT(IsContextReg(startRegAddr));

    // The GPU virtual address must be DWORD-aligned and not use more than 48 bits.
    PAL_ASSERT(IsPow2Aligned(gpuVirtAddr, 4));
    PAL_ASSERT((HighPart(gpuVirtAddr) & 0xFFFF0000) == 0);

    constexpr uint32 PacketSize = (sizeof(PM4PFP_LOAD_CONTEXT_REG) / sizeof(uint32));
    auto*const       pPacket    = static_cast<PM4PFP_LOAD_CONTEXT_REG*>(pBuffer);

    pPacket->header.u32All           = Type3Header(IT_LOAD_CONTEXT_REG, PacketSize);
    pPacket->ordinal2                = 0;
    pPacket->bitfields2.base_addr_lo = (LowPart(gpuVirtAddr) >> 2);
    pPacket->base_addr_hi            = HighPart(gpuVirtAddr);
    pPacket->ordinal4                = 0;
    pPacket->bitfields4.reg_offset   = (startRegAddr - CONTEXT_SPACE_START);
    pPacket->ordinal5                = 0;
    pPacket->bitfields5.num_dwords   = count;

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 packet which issues a load_context_reg command to load multiple groups of consecutive context registers
// from video memory.  Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildLoadContextRegs(
    gpusize              gpuVirtAddr,
    const RegisterRange* pRanges,
    uint32               rangeCount,
    void*                pBuffer)      // [out] Build the PM4 packet in this buffer.
{
    PAL_ASSERT((pRanges != nullptr) && (rangeCount >= 1));

    // The GPU virtual address must be DWORD-aligned and not use more than 48 bits.
    PAL_ASSERT(IsPow2Aligned(gpuVirtAddr, 4));
    PAL_ASSERT((HighPart(gpuVirtAddr) & 0xFFFF0000) == 0);

    const uint32 packetSize = (sizeof(PM4PFP_LOAD_CONTEXT_REG) / sizeof(uint32)) + (2 * (rangeCount - 1));
    auto*const   pPacket    = static_cast<PM4PFP_LOAD_CONTEXT_REG*>(pBuffer);

    pPacket->header.u32All           = Type3Header(IT_LOAD_CONTEXT_REG, packetSize);
    pPacket->ordinal2                = 0;
    pPacket->bitfields2.base_addr_lo = (LowPart(gpuVirtAddr) >> 2);
    pPacket->base_addr_hi            = HighPart(gpuVirtAddr);

    // Note: This is a variable-length packet. The PM4PFP_LOAD_CONTEXT_REG packet contains space for the first register
    // range, but not the others (though they are expected to immediately follow in the command buffer).
    memcpy(&pPacket->ordinal4, pRanges, (sizeof(RegisterRange) * rangeCount));

    return packetSize;
}

// =====================================================================================================================
// Builds a PM4 packet which issues a load_context_reg_index command to load a single group of consecutive context
// registers from an indirect video memory offset.  Returns the size of the PM4 command assembled, in DWORDs.
template <bool directAddress>
size_t CmdUtil::BuildLoadContextRegsIndex(
    gpusize gpuVirtAddrOrAddrOffset,
    uint32  startRegAddr,
    uint32  count,
    void*   pBuffer       // [out] Build the PM4 packet in this buffer.
    ) const
{
    PAL_ASSERT(IsContextReg(startRegAddr));

    // The GPU virtual address and/or address offset gets added to a base address set via SET_BASE packet. CP then
    // loads the data from that address and it must be DWORD aligned.
    PAL_ASSERT(IsPow2Aligned(gpuVirtAddrOrAddrOffset, 4));

    constexpr uint32 PacketSize = (sizeof(PM4PFP_LOAD_CONTEXT_REG_INDEX) / sizeof(uint32));
    auto*const       pPacket = static_cast<PM4PFP_LOAD_CONTEXT_REG_INDEX*>(pBuffer);

    pPacket->header.u32All          = Type3Header(IT_LOAD_CONTEXT_REG_INDEX, PacketSize);
    pPacket->ordinal2               = 0;
    if (directAddress)
    {
        // Only the low 16 bits of addrOffset are honored for the high portion of the GPU virtual address!
        PAL_ASSERT((HighPart(gpuVirtAddrOrAddrOffset) & 0xFFFF0000) == 0);

        pPacket->bitfields2.index       = index__pfp_load_context_reg_index__direct_addr;
        pPacket->bitfields2.mem_addr_lo = (LowPart(gpuVirtAddrOrAddrOffset) >> 2);
        pPacket->mem_addr_hi            = HighPart(gpuVirtAddrOrAddrOffset);
    }
    else
    {
        // The high part of the offset is ignored when not using direct-address mode because the offset is only
        // specified to the packet using 32 bits.
        PAL_ASSERT(HighPart(gpuVirtAddrOrAddrOffset) == 0);

        pPacket->bitfields2.index       = index__pfp_load_context_reg_index__offset;
        pPacket->addr_offset            = LowPart(gpuVirtAddrOrAddrOffset);
    }
    pPacket->ordinal4               = 0;
    pPacket->bitfields4.reg_offset  = (startRegAddr - CONTEXT_SPACE_START);
    pPacket->bitfields4.data_format = data_format__pfp_load_context_reg_index__offset_and_size;
    pPacket->ordinal5               = 0;
    pPacket->bitfields5.num_dwords  = count;

    return PacketSize;
}

template
size_t CmdUtil::BuildLoadContextRegsIndex<true>(
    gpusize gpuVirtAddrOrAddrOffset,
    uint32  startRegAddr,
    uint32  count,
    void*   pBuffer
    ) const;

template
size_t CmdUtil::BuildLoadContextRegsIndex<false>(
    gpusize gpuVirtAddrOrAddrOffset,
    uint32  startRegAddr,
    uint32  count,
    void*   pBuffer
    ) const;

// =====================================================================================================================
// Builds a PM4 packet which issues a load_context_reg_index command to load a series of individual context registers
// stored in GPU memory.  Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildLoadContextRegsIndex(
    gpusize gpuVirtAddr,
    uint32  count,
    void*   pBuffer)      // [out] Build the PM4 packet in this buffer.
{
    constexpr uint32 PacketSize = (sizeof(PM4PFP_LOAD_CONTEXT_REG_INDEX) / sizeof(uint32));
    auto*const       pPacket    = static_cast<PM4PFP_LOAD_CONTEXT_REG_INDEX*>(pBuffer);

    pPacket->header.u32All = Type3Header(IT_LOAD_CONTEXT_REG_INDEX, PacketSize);

    pPacket->ordinal2               = 0;
    pPacket->bitfields2.index       = index__pfp_load_context_reg_index__direct_addr;
    pPacket->bitfields2.mem_addr_lo = LowPart(gpuVirtAddr) >> 2;
    pPacket->mem_addr_hi            = HighPart(gpuVirtAddr);
    // Only the low 16 bits are honored for the high portion of the GPU virtual address!
    PAL_ASSERT((HighPart(gpuVirtAddr) & 0xFFFF0000) == 0);

    pPacket->ordinal4               = 0;
    pPacket->bitfields4.data_format = data_format__pfp_load_context_reg_index__offset_and_data;

    pPacket->ordinal5               = 0;
    pPacket->bitfields5.num_dwords  = count;

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 packet which issues a load_sh_reg command to load a single group of consecutive persistent-state
// registers from video memory.  Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildLoadShRegs(
    gpusize       gpuVirtAddr,
    uint32        startRegAddr,
    uint32        count,
    Pm4ShaderType shaderType,
    void*         pBuffer)      // [out] Build the PM4 packet in this buffer.
{
    PAL_ASSERT(IsShReg(startRegAddr));

    // The GPU virtual address must be DWORD-aligned and not use more than 48 bits.
    PAL_ASSERT(IsPow2Aligned(gpuVirtAddr, 4));
    PAL_ASSERT((HighPart(gpuVirtAddr) & 0xFFFF0000) == 0);

    constexpr uint32 PacketSize = (sizeof(PM4PFP_LOAD_SH_REG) / sizeof(uint32));
    auto*const       pPacket    = static_cast<PM4PFP_LOAD_SH_REG*>(pBuffer);

    pPacket->header.u32All              = Type3Header(IT_LOAD_SH_REG, PacketSize, false, shaderType);
    pPacket->ordinal2                   = 0;
    pPacket->bitfields2.base_address_lo = (LowPart(gpuVirtAddr) >> 2);
    pPacket->base_address_hi            = HighPart(gpuVirtAddr);
    pPacket->ordinal4                   = 0;
    pPacket->bitfields4.reg_offset      = (startRegAddr - PERSISTENT_SPACE_START);
    pPacket->ordinal5                   = 0;
    pPacket->bitfields5.num_dword       = count;

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 packet which issues a load_sh_reg command to load multiple groups of consecutive persistent-state
// registers from video memory.  Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildLoadShRegs(
    gpusize              gpuVirtAddr,
    const RegisterRange* pRanges,
    uint32               rangeCount,
    Pm4ShaderType        shaderType,
    void*                pBuffer)      // [out] Build the PM4 packet in this buffer.
{
    PAL_ASSERT((pRanges != nullptr) && (rangeCount >= 1));

    // The GPU virtual address must be DWORD-aligned and not use more than 48 bits.
    PAL_ASSERT(IsPow2Aligned(gpuVirtAddr, 4));
    PAL_ASSERT((HighPart(gpuVirtAddr) & 0xFFFF0000) == 0);

    const uint32 packetSize = (sizeof(PM4PFP_LOAD_SH_REG) / sizeof(uint32)) + (2 * (rangeCount - 1));
    auto*const   pPacket    = static_cast<PM4PFP_LOAD_SH_REG*>(pBuffer);

    pPacket->header.u32All              = Type3Header(IT_LOAD_SH_REG, packetSize, false, shaderType);
    pPacket->ordinal2                   = 0;
    pPacket->bitfields2.base_address_lo = (LowPart(gpuVirtAddr) >> 2);
    pPacket->base_address_hi            = HighPart(gpuVirtAddr);

    // Note: This is a variable-length packet. The PM4PFP_LOAD_SH_REG packet contains space for the first register
    // range, but not the others (though they are expected to immediately follow in the command buffer).
    memcpy(&pPacket->ordinal4, pRanges, (sizeof(RegisterRange) * rangeCount));

    return packetSize;
}

// =====================================================================================================================
// Builds a PM4 packet which issues a load_sh_reg_index command to load a single group of consecutive persistent-state
// registers from indirect video memory offset.  Returns the size of the PM4 command assembled, in DWORDs.
template <bool directAddress>
size_t CmdUtil::BuildLoadShRegsIndex(
    gpusize       gpuVirtAddrOrAddrOffset,
    uint32        startRegAddr,
    uint32        count,
    Pm4ShaderType shaderType,
    void*         pBuffer       // [out] Build the PM4 packet in this buffer.
    ) const
{
#if PAL_ENABLE_PRINTS_ASSERTS
    CheckShadowedShReg(shaderType, startRegAddr);
#endif

    constexpr uint32 PacketSize = (sizeof(PM4PFP_LOAD_SH_REG_INDEX) / sizeof(uint32));
    auto*const       pPacket    = static_cast<PM4PFP_LOAD_SH_REG_INDEX*>(pBuffer);

    pPacket->header.u32All = Type3Header(IT_LOAD_SH_REG_INDEX, PacketSize, false, shaderType);
    pPacket->ordinal2      = 0;
    if (directAddress)
    {
        pPacket->bitfields2.index       = index__pfp_load_sh_reg_index__direct_addr;
        pPacket->bitfields2.mem_addr_lo = LowPart(gpuVirtAddrOrAddrOffset);
        pPacket->mem_addr_hi            = HighPart(gpuVirtAddrOrAddrOffset);
        // Only the low 16 bits of addrOffset are honored for the high portion of the GPU virtual address!
        PAL_ASSERT((HighPart(gpuVirtAddrOrAddrOffset) & 0xFFFF0000) == 0);
    }
    else
    {
        pPacket->bitfields2.index       = index__pfp_load_sh_reg_index__offset;
        pPacket->addr_offset            = LowPart(gpuVirtAddrOrAddrOffset);
    }
    pPacket->ordinal4               = 0;
    pPacket->bitfields4.reg_offset  = (startRegAddr - PERSISTENT_SPACE_START);
    pPacket->bitfields4.data_format = data_format__pfp_load_sh_reg_index__offset_and_size;
    pPacket->ordinal5               = 0;
    pPacket->bitfields5.num_dwords  = count;

    return PacketSize;
}

template
size_t CmdUtil::BuildLoadShRegsIndex<true>(
    gpusize       addrOffset,
    uint32        startRegAddr,
    uint32        count,
    Pm4ShaderType shaderType,
    void*         pBuffer
    ) const;

template
size_t CmdUtil::BuildLoadShRegsIndex<false>(
    gpusize       addrOffset,
    uint32        startRegAddr,
    uint32        count,
    Pm4ShaderType shaderType,
    void*         pBuffer
    ) const;

// =====================================================================================================================
// Builds a PM4 packet which issues a load_sh_reg_index command to load a series of individual persistent-state
// registers stored in GPU memory.  Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildLoadShRegsIndex(
    gpusize       gpuVirtAddr,
    uint32        count,
    Pm4ShaderType shaderType,
    void*         pBuffer)      // [out] Build the PM4 packet in this buffer.
{
    constexpr uint32 PacketSize = (sizeof(PM4PFP_LOAD_SH_REG_INDEX) / sizeof(uint32));
    auto*const       pPacket    = static_cast<PM4PFP_LOAD_SH_REG_INDEX*>(pBuffer);

    pPacket->header.u32All = Type3Header(IT_LOAD_SH_REG_INDEX, PacketSize, false, shaderType);

    pPacket->ordinal2               = 0;
    pPacket->bitfields2.index       = index__pfp_load_sh_reg_index__direct_addr;
    pPacket->bitfields2.mem_addr_lo = LowPart(gpuVirtAddr) >> 2;
    pPacket->mem_addr_hi            = HighPart(gpuVirtAddr);
    // Only the low 16 bits are honored for the high portion of the GPU virtual address!
    PAL_ASSERT((HighPart(gpuVirtAddr) & 0xFFFF0000) == 0);

    pPacket->ordinal4               = 0;
    pPacket->bitfields4.data_format = data_format__pfp_load_sh_reg_index__offset_and_data;

    pPacket->ordinal5               = 0;
    pPacket->bitfields5.num_dwords  = count;

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 packet which issues a load_uconfig_reg command to load multiple groups of consecutive user-config
// registers from video memory.  Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildLoadUserConfigRegs(
    gpusize              gpuVirtAddr,
    const RegisterRange* pRanges,
    uint32               rangeCount,
    void*                pBuffer)      // [out] Build the PM4 packet in this buffer.
{
    PAL_ASSERT((pRanges != nullptr) && (rangeCount >= 1));

    // The GPU virtual address must be DWORD-aligned and not use more than 48 bits.
    PAL_ASSERT(IsPow2Aligned(gpuVirtAddr, 4));
    PAL_ASSERT((HighPart(gpuVirtAddr) & 0xFFFF0000) == 0);

    const uint32 packetSize = (sizeof(PM4PFP_LOAD_UCONFIG_REG) / sizeof(uint32)) + (2 * (rangeCount - 1));
    auto*const   pPacket    = static_cast<PM4PFP_LOAD_UCONFIG_REG*>(pBuffer);

    pPacket->header.u32All              = Type3Header(IT_LOAD_UCONFIG_REG, packetSize);
    pPacket->ordinal2                   = 0;
    pPacket->bitfields2.base_address_lo = (LowPart(gpuVirtAddr) >> 2);
    pPacket->base_address_hi            = HighPart(gpuVirtAddr);

    // Note: This is a variable-length packet. The PM4PFP_LOAD_UCONFIG_REG packet contains space for the first register
    // range, but not the others (though they are expected to immediately follow in the command buffer).
    memcpy(&pPacket->ordinal4, pRanges, (sizeof(RegisterRange) * rangeCount));

    return packetSize;
}

// =====================================================================================================================
// Constructs a PM4 packet which issues a sync command instructing the PFP to stall until the ME is no longer busy. This
// packet will hang on the compute queue; it is the caller's responsibility to ensure that this function is called
// safely. Returns the size of the PM4 command built, in DWORDs.
size_t CmdUtil::BuildPfpSyncMe(
    void* pBuffer) // [out] Build the PM4 packet in this buffer.
{
    constexpr uint32 PacketSize = (sizeof(PM4PFP_PFP_SYNC_ME) / sizeof(uint32));
    auto*const       pPacket    = static_cast<PM4PFP_PFP_SYNC_ME*>(pBuffer);

    pPacket->header.u32All = Type3Header(IT_PFP_SYNC_ME, PacketSize);
    pPacket->dummy_data    = 0;

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 packet which marks the beginning or end of either a draw-engine preamble or the initialization of
// clear-state memory. Returns the size of the PM4 command build, in DWORDs.
size_t CmdUtil::BuildPreambleCntl(
    ME_PREAMBLE_CNTL_command_enum command,
    void*                         pBuffer)     // [out] Build the PM4 packet in this buffer.
{
    PAL_ASSERT((command == command__me_preamble_cntl__preamble_begin)                      ||
               (command == command__me_preamble_cntl__preamble_end)                        ||
               (command == command__me_preamble_cntl__begin_of_clear_state_initialization) ||
               (command == command__me_preamble_cntl__end_of_clear_state_initialization));

    constexpr size_t PacketSize = (sizeof(PM4ME_PREAMBLE_CNTL) / sizeof(uint32));
    auto*const       pPacket    = static_cast<PM4ME_PREAMBLE_CNTL*>(pBuffer);

    pPacket->header.u32All      = Type3Header(IT_PREAMBLE_CNTL, PacketSize);
    pPacket->ordinal2           = 0;
    pPacket->bitfields2.command = command;

    return PacketSize;
}

// =====================================================================================================================
// GFX10 adds a new GCR_CNTL field that takes over flush/inv control on most caches. Only CB and DB are still
// controlled by CP_COHER_CNTL field. This function essentially converts GFX9 style cache sync info to GFX10 style one.
uint32 CmdUtil::Gfx10CalcReleaseMemGcrCntl(
    const ReleaseMemInfo&  releaseMemInfo
    ) const
{
    Gfx10ReleaseMemGcrCntl gcrCntl = {};

    // GL2_RANGE[1:0]
    //  0:ALL          wb/inv op applies to entire physical cache (ignore range)
    //  1:VOL          wb/inv op applies to all volatile tagged lines in the GL2 (ignore range)
    //  2:RANGE      - wb/inv ops applies to just the base/limit virtual address range
    //  3:FIRST_LAST - wb/inv ops applies to 128B at BASE_VA and 128B at LIMIT_VA
    gcrCntl.bits.gl2Range = 0;

    switch(releaseMemInfo.tcCacheOp)
    {
    case TcCacheOp::WbInvL1L2:
        // GLM_WB[0]  - write-back control for the meta-data cache of GL2. L2MD is write-through, ignore this bit.
        // GLM_INV[0] - invalidate enable for the meta-data cache of GL2
        // GLV_INV[0] - invalidate enable for shader vector L0 cache
        // GL1_INV[0] - invalidate enable for GL1
        // GL2_INV[0] - invalidate enable for GL2
        // GL2_WB[0]  - writeback enable for GL2
        gcrCntl.bits.glmInv = 1;
        gcrCntl.bits.glvInv = 1;
        gcrCntl.bits.gl1Inv = 1;
        gcrCntl.bits.gl2Inv = 1;
        gcrCntl.bits.gl2Wb  = 1;
        break;

    case TcCacheOp::WbInvL2Nc:
        // GL2_INV[0] - invalidate enable for GL2
        // GL2_WB[0]  - writeback enable for GL2
        gcrCntl.bits.gl2Inv = 1;
        gcrCntl.bits.gl2Wb  = 1;
        break;

    // GFX10TODO: GCR cannot differentiate Nc(non-coherent MTYPE) and Wc(write-combined MTYPE)?
    case TcCacheOp::WbL2Nc:
    case TcCacheOp::WbL2Wc:
        // GL2_WB[0] - writeback enable for GL2
        gcrCntl.bits.gl2Wb  = 1;
        break;

    case TcCacheOp::InvL2Nc:
        // GL2_INV[0] - invalidate enable for GL2
        gcrCntl.bits.gl2Inv = 1;

    case TcCacheOp::InvL2Md:
        // GLM_INV[0] - invalidate enable for the meta-data cache of GL2
        gcrCntl.bits.glmInv = 1;
        break;

    case TcCacheOp::InvL1:
        // GLV_INV[0] - invalidate enable for shader vector L0 cache
        gcrCntl.bits.glvInv = 1;
        gcrCntl.bits.gl1Inv = 1;
        break;

    case TcCacheOp::InvL1Vol:
        // GL2_RANGE[1:0]
        // GL1_INV[0] - invalidate enable for GL1
        // GLV_INV[0] - invalidate enable for shader vector L0 cache
        gcrCntl.bits.gl2Range = 1;
        gcrCntl.bits.gl1Inv   = 1;
        gcrCntl.bits.glvInv   = 1;
        break;

    default:
        PAL_ASSERT(releaseMemInfo.tcCacheOp == TcCacheOp::Nop);
        break;
    }

    // SEQ[1:0]   controls the sequence of operations on the cache hierarchy (L0/L1/L2)
    //      0: PARALLEL   initiate wb/inv ops on specified caches at same time
    //      1: FORWARD    L0 then L1/L2, complete L0 ops then initiate L1/L2
    //                    Typically only needed when doing WB of L0 K$, M$, or RB w/ WB of GL2
    //      2: REVERSE    L2 -> L1 -> L0
    //                    Typically only used for post-unaligned-DMA operation (invalidate only)
    // For RELEASE_MEM, the only case when WB of L0 K$, RbMd and/or RB happens w/ WB of GL2 is when a
    // CACHE_FLUSH_AND_INV_XX event is issued, in this case we expect the hardware to take care of the flush sequence.
    gcrCntl.bits.seq = 0;

    return gcrCntl.u32All;
}

// =====================================================================================================================
// Builds the common aspects of a release-mem packet.
template <typename ReleaseMemPacketType>
size_t CmdUtil::BuildReleaseMemInternal(
    const ExplicitReleaseMemInfo& releaseMemInfo,
    ReleaseMemPacketType*         pPacket, // [out] Build the PM4 packet in this buffer.
    uint32                        gdsAddr, // dword offset,
                                           // ignored unless dataSel == release_mem__store_gds_data_to_memory
    uint32                        gdsSize  // ignored unless dataSel == release_mem__store_gds_data_to_memory
    ) const
{
    constexpr uint32 PacketSize = (sizeof(ReleaseMemPacketType) / sizeof(uint32));

    pPacket->header.u32All = Type3Header(IT_RELEASE_MEM, PacketSize);

    // If the asserts in this switch statement trip, you will almost certainly hang the GPU
    switch (releaseMemInfo.vgtEvent)
    {
    case FLUSH_SX_TS:
    case FLUSH_AND_INV_DB_DATA_TS:
    case FLUSH_AND_INV_CB_DATA_TS:
        PAL_ASSERT(Pal::Device::EngineSupportsGraphics(releaseMemInfo.engineType));
        // break intentionally left out!

    case CACHE_FLUSH_TS:
    case CACHE_FLUSH_AND_INV_TS_EVENT:
    case BOTTOM_OF_PIPE_TS:
        pPacket->bitfields2.event_index = event_index__mec_release_mem__end_of_pipe;
        break;

    case PS_DONE:
        PAL_ASSERT(Pal::Device::EngineSupportsGraphics(releaseMemInfo.engineType));
        pPacket->bitfields2.event_index = event_index__mec_release_mem__shader_done;
        break;

    case CS_DONE:
        PAL_ASSERT(Pal::Device::EngineSupportsCompute(releaseMemInfo.engineType));
        pPacket->bitfields2.event_index = event_index__mec_release_mem__shader_done;
        break;

    default:
        // Not all VGT events are legal with release-mem packets!
        PAL_ASSERT_ALWAYS();
        break;
    }

    pPacket->bitfields2.event_type = releaseMemInfo.vgtEvent;
    pPacket->ordinal3              = 0;
    pPacket->bitfields3.data_sel   = static_cast<MEC_RELEASE_MEM_data_sel_enum>(releaseMemInfo.dataSel);
    pPacket->bitfields3.dst_sel    = dst_sel__mec_release_mem__tc_l2;
    pPacket->ordinal4              = LowPart(releaseMemInfo.dstAddr);
    pPacket->address_hi            = HighPart(releaseMemInfo.dstAddr);  // ordinal5
    pPacket->data_lo               = LowPart(releaseMemInfo.data);      // ordinal6, overwritten below for gds
    pPacket->data_hi               = HighPart(releaseMemInfo.data);     // ordinal7, overwritten below for gds
    pPacket->int_ctxid             = 0;

    // This won't send an interrupt but will wait for write confirm before writing the data to memory.
    pPacket->bitfields3.int_sel    = (releaseMemInfo.dataSel == data_sel__mec_release_mem__none)
                                        ? int_sel__mec_release_mem__none
                                        : int_sel__mec_release_mem__send_data_and_write_confirm;

    // Make sure our dstAddr is properly aligned.  The alignment differs based on how much data is being written
    if (releaseMemInfo.dataSel == data_sel__mec_release_mem__store_gds_data_to_memory)
    {
        pPacket->bitfields6.dw_offset  = gdsAddr;
        pPacket->bitfields6.num_dwords = gdsSize;
        pPacket->data_hi               = 0;
    }

    return PacketSize;
}

// =====================================================================================================================
// Generic function for building a RELEASE_MEM packet on either computer or graphics engines.  Return the number of
// DWORDs taken up by this packet.
size_t CmdUtil::BuildReleaseMem(
    const ReleaseMemInfo& releaseMemInfo,
    void*                 pBuffer,   // [out] Build the PM4 packet in this buffer.
    uint32                gdsAddr,   // dword offset,
                                     // ignored unless dataSel == release_mem__store_gds_data_to_memory
    uint32                gdsSize    // ignored unless dataSel == release_mem__store_gds_data_to_memory
    ) const
{
    size_t totalSize = 0;

    // Add a dummy ZPASS_DONE event before EOP timestamp events to avoid a DB hang.
    if (VgtEventHasTs[releaseMemInfo.vgtEvent]                         &&
        Pal::Device::EngineSupportsGraphics(releaseMemInfo.engineType) &&
        m_device.Settings().waDummyZpassDoneBeforeTs)
    {
        const BoundGpuMemory& dummyMemory = m_device.DummyZpassDoneMem();
        PAL_ASSERT(dummyMemory.IsBound());

        totalSize += BuildSampleEventWrite(ZPASS_DONE,
                                           releaseMemInfo.engineType,
                                           dummyMemory.GpuVirtAddr(),
                                           VoidPtrInc(pBuffer, sizeof(uint32) * totalSize));
    }

    // Translate ReleaseMemInfo to a new ReleaseMemInfo type that's more universal.
    ExplicitReleaseMemInfo explicitReleaseMemInfo;
    explicitReleaseMemInfo.engineType = releaseMemInfo.engineType;
    explicitReleaseMemInfo.vgtEvent   = releaseMemInfo.vgtEvent;
    explicitReleaseMemInfo.dstAddr    = releaseMemInfo.dstAddr;
    explicitReleaseMemInfo.dataSel    = releaseMemInfo.dataSel;
    explicitReleaseMemInfo.data       = releaseMemInfo.data;

    if (m_gfxIpLevel == GfxIpLevel::GfxIp9)
    {
        regCP_COHER_CNTL cpCoherCntl = {};

        switch(releaseMemInfo.tcCacheOp)
        {
        case TcCacheOp::WbInvL1L2:
            cpCoherCntl.bits.TC_ACTION_ENA              = 1;
            cpCoherCntl.bits.TC_WB_ACTION_ENA           = 1;
            break;

        case TcCacheOp::WbInvL2Nc:
            cpCoherCntl.bits.TC_ACTION_ENA              = 1;
            cpCoherCntl.bits.TC_WB_ACTION_ENA           = 1;
            cpCoherCntl.bits.TC_NC_ACTION_ENA           = 1;
            break;

        case TcCacheOp::WbL2Nc:
            cpCoherCntl.bits.TC_WB_ACTION_ENA           = 1;
            cpCoherCntl.bits.TC_NC_ACTION_ENA           = 1;
            break;

        case TcCacheOp::WbL2Wc:
            cpCoherCntl.bits.TC_WB_ACTION_ENA           = 1;
            cpCoherCntl.bits.TC_WC_ACTION_ENA           = 1;
            break;

        case TcCacheOp::InvL2Nc:
            cpCoherCntl.bits.TC_ACTION_ENA              = 1;
            cpCoherCntl.bits.TC_NC_ACTION_ENA           = 1;
            break;

        case TcCacheOp::InvL2Md:
            cpCoherCntl.bits.TC_ACTION_ENA              = 1;
            cpCoherCntl.bits.TC_INV_METADATA_ACTION_ENA = 1;
            break;

        case TcCacheOp::InvL1:
            cpCoherCntl.bits.TCL1_ACTION_ENA            = 1;
            break;

        case TcCacheOp::InvL1Vol:
            cpCoherCntl.bits.TCL1_ACTION_ENA            = 1;
            cpCoherCntl.bits.TCL1_VOL_ACTION_ENA        = 1;
            break;

        default:
            PAL_ASSERT(releaseMemInfo.tcCacheOp == TcCacheOp::Nop);
            break;
        }

        explicitReleaseMemInfo.coherCntl = cpCoherCntl.u32All;
    }
    else if (IsGfx10(m_gfxIpLevel))
    {
        explicitReleaseMemInfo.coherCntl = 0;
        explicitReleaseMemInfo.gcrCntl   = Gfx10CalcReleaseMemGcrCntl(releaseMemInfo);
    }

    // Call a more explicit function.
    totalSize += ExplicitBuildReleaseMem(explicitReleaseMemInfo,
                                         VoidPtrInc(pBuffer, sizeof(uint32) * totalSize),
                                         gdsAddr,
                                         gdsSize);

    return totalSize;
}

// =====================================================================================================================
// Generic function for building a RELEASE_MEM packet on either computer or graphics engines.  Return the number of
// DWORDs taken up by this packet.
size_t CmdUtil::ExplicitBuildReleaseMem(
    const ExplicitReleaseMemInfo& releaseMemInfo,
    void*                         pBuffer,   // [out] Build the PM4 packet in this buffer.
    uint32                        gdsAddr,   // dword offset,
                                             // ignored unless dataSel == release_mem__store_gds_data_to_memory
    uint32                        gdsSize    // ignored unless dataSel == release_mem__store_gds_data_to_memory
    ) const
{
    static_assert(((static_cast<uint32>(event_index__me_release_mem__end_of_pipe)   ==
                    static_cast<uint32>(event_index__mec_release_mem__end_of_pipe)) &&
                   (static_cast<uint32>(event_index__me_release_mem__shader_done)   ==
                    static_cast<uint32>(event_index__mec_release_mem__shader_done))),
                  "RELEASE_MEM event index enumerations don't match between ME and MEC!");
    static_assert(((static_cast<uint32>(data_sel__me_release_mem__none)                        ==
                    static_cast<uint32>(data_sel__mec_release_mem__none))                      &&
                   (static_cast<uint32>(data_sel__me_release_mem__send_32_bit_low)             ==
                    static_cast<uint32>(data_sel__mec_release_mem__send_32_bit_low))           &&
                   (static_cast<uint32>(data_sel__me_release_mem__send_64_bit_data)            ==
                    static_cast<uint32>(data_sel__mec_release_mem__send_64_bit_data))          &&
                   (static_cast<uint32>(data_sel__me_release_mem__send_gpu_clock_counter)      ==
                    static_cast<uint32>(data_sel__mec_release_mem__send_gpu_clock_counter))    &&
                   (static_cast<uint32>(data_sel__me_release_mem__store_gds_data_to_memory)    ==
                    static_cast<uint32>(data_sel__mec_release_mem__store_gds_data_to_memory))),
                  "RELEASE_MEM data sel enumerations don't match between ME and MEC!");

    size_t packetSize = 0;

    if (m_gfxIpLevel == GfxIpLevel::GfxIp9)
    {
        static_assert(sizeof(PM4MEC_RELEASE_MEM__GFX09) == sizeof(PM4MEC_RELEASE_MEM__GFX09),
                      "RELEASE_MEM is different sizes between ME and MEC!");

        // This function is written with the MEC version of this packet, but we're assuming that the MEC and ME
        // versions are identical.
        PM4MEC_RELEASE_MEM__GFX09 packet = {};
        packetSize = BuildReleaseMemInternal(releaseMemInfo, &packet, gdsAddr, gdsSize);

        // Handle the GFX-specific aspects of a release-mem packet.
        regCP_COHER_CNTL cpCoherCntl;
        cpCoherCntl.u32All = releaseMemInfo.coherCntl;

        packet.bitfields2.tcl1_vol_action_ena = cpCoherCntl.bitfields.TCL1_VOL_ACTION_ENA;
        packet.bitfields2.tc_vol_action_ena   = 0;
        packet.bitfields2.tc_wb_action_ena    = cpCoherCntl.bitfields.TC_WB_ACTION_ENA;
        packet.bitfields2.tcl1_action_ena     = cpCoherCntl.bitfields.TCL1_ACTION_ENA;
        packet.bitfields2.tc_action_ena       = cpCoherCntl.bitfields.TC_ACTION_ENA;
        packet.bitfields2.tc_nc_action_ena    = cpCoherCntl.bitfields.TC_NC_ACTION_ENA;
        packet.bitfields2.tc_wc_action_ena    = cpCoherCntl.bitfields.TC_WC_ACTION_ENA;
        packet.bitfields2.tc_md_action_ena    = cpCoherCntl.bitfields.TC_INV_METADATA_ACTION_ENA;

        memcpy(pBuffer, &packet, packetSize * sizeof(uint32));
    }
    else if (IsGfx10(m_gfxIpLevel))
    {
        static_assert(sizeof(PM4MEC_RELEASE_MEM__GFX10) == sizeof(PM4ME_RELEASE_MEM__GFX10),
                      "RELEASE_MEM is different sizes between ME and MEC!");

        // This function is written with the MEC version of this packet, but we're assuming that the MEC and ME
        // versions are identical.
        PM4MEC_RELEASE_MEM__GFX10 packet = {};
        packetSize = BuildReleaseMemInternal(releaseMemInfo, &packet, gdsAddr, gdsSize);

        // Handle the GFX-specific aspects of a release-mem packet.
        packet.bitfields2.gcr_cntl = releaseMemInfo.gcrCntl;

        memcpy(pBuffer, &packet, packetSize * sizeof(uint32));
    }

    return packetSize;
}

// =====================================================================================================================
// Builds a REWIND packet for telling compute queues to reload the command buffer data after this packet. Returns the
// size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildRewind(
    bool  offloadEnable,
    bool  valid,
    void* pBuffer
    ) const
{
    // This packet in PAL is only supported on compute queues.
    // The packet is supported on the PFP engine (PM4_PFP_REWIND) but offload_enable is not defined for PFP.
    constexpr size_t PacketSize = sizeof(PM4_MEC_REWIND) / sizeof(uint32);
    auto*const       pPacket    = static_cast<PM4_MEC_REWIND*>(pBuffer);

    pPacket->header.u32All             = Type3Header(IT_REWIND__CORE, PacketSize, false, ShaderCompute);
    pPacket->ordinal2                  = 0;
    pPacket->bitfields2.offload_enable = offloadEnable;
    pPacket->bitfields2.valid          = valid;

    return PacketSize;
}

// =====================================================================================================================
// Builds a SET_BASE packet.  Returns the number of DWORDs taken by this packet.
size_t CmdUtil::BuildSetBase(
    gpusize                      address,
    PFP_SET_BASE_base_index_enum baseIndex,
    Pm4ShaderType                shaderType,
    void*                        pBuffer)    // [out] Build the PM4 packet in this buffer.
{
    constexpr uint32 PacketSize = (sizeof(PM4PFP_SET_BASE) / sizeof(uint32));
    auto*const       pPacket    = static_cast<PM4PFP_SET_BASE*>(pBuffer);

    pPacket->header.u32All         = Type3Header(IT_SET_BASE, PacketSize, false, shaderType);
    pPacket->ordinal2              = 0;
    pPacket->bitfields2.base_index = baseIndex;
    pPacket->ordinal3              = LowPart(address);
    pPacket->address_hi            = HighPart(address);

    // Make sure our address was aligned properly
    PAL_ASSERT (pPacket->bitfields3.reserved1 == 0);

    return PacketSize;
}

// =====================================================================================================================
// Builds a SET_BASE packet for constant engine.  Returns the number of DWORDs taken by this packet.
size_t CmdUtil::BuildSetBaseCe(
    gpusize                     address,
    CE_SET_BASE_base_index_enum baseIndex,
    Pm4ShaderType               shaderType,
    void*                       pBuffer)    // [out] Build the PM4 packet in this buffer.
{
    constexpr uint32 PacketSize = (sizeof(PM4CE_SET_BASE) / sizeof(uint32));
    auto*const       pPacket    = static_cast<PM4CE_SET_BASE*>(pBuffer);

    pPacket->header.u32All         = Type3Header(IT_SET_BASE, PacketSize, false, shaderType);
    pPacket->ordinal2              = 0;
    pPacket->bitfields2.base_index = baseIndex;
    pPacket->ordinal3              = LowPart(address);
    pPacket->address_hi            = HighPart(address);

    // Make sure our address was aligned properly
    PAL_ASSERT (pPacket->bitfields3a.reserved1 == 0);

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 packet which sets one config register. The index field is used to set special registers and should be
// set to zero except when setting one of those registers. Returns the size of the PM4 command assembled, in DWORDs.
template <bool resetFilterCam>
size_t CmdUtil::BuildSetOneConfigReg(
    uint32                               regAddr,
    void*                                pBuffer,  // [out] Build the PM4 packet in this buffer.
    PFP_SET_UCONFIG_REG_INDEX_index_enum index
    ) const
{
    PAL_ASSERT(((regAddr != mmVGT_INDEX_TYPE)    ||
                (index == index__pfp_set_uconfig_reg_index__index_type))    &&
               ((regAddr != mmVGT_NUM_INSTANCES) ||
                (index == index__pfp_set_uconfig_reg_index__num_instances)));

    PAL_ASSERT((m_gfxIpLevel != GfxIpLevel::GfxIp9) ||
                (((regAddr != mmVGT_PRIMITIVE_TYPE)        ||
                  (index == index__pfp_set_uconfig_reg_index__prim_type__GFX09))     &&
                 ((regAddr != Gfx09::mmIA_MULTI_VGT_PARAM) ||
                  (index == index__pfp_set_uconfig_reg_index__multi_vgt_param__GFX09))));

    return BuildSetSeqConfigRegs<resetFilterCam>(regAddr, regAddr, pBuffer, index);
}

template
size_t CmdUtil::BuildSetOneConfigReg<true>(
    uint32                               regAddr,
    void*                                pBuffer,
    PFP_SET_UCONFIG_REG_INDEX_index_enum index) const;
template
size_t CmdUtil::BuildSetOneConfigReg<false>(
    uint32                               regAddr,
    void*                                pBuffer,
    PFP_SET_UCONFIG_REG_INDEX_index_enum index) const;

// =====================================================================================================================
// Builds a PM4 packet which sets a sequence of config registers starting with startRegAddr and ending with endRegAddr
// (inclusive). Returns the size of the PM4 command assembled, in DWORDs.
template <bool resetFilterCam>
size_t CmdUtil::BuildSetSeqConfigRegs(
    uint32                                startRegAddr,
    uint32                                endRegAddr,
    void*                                 pBuffer,
    PFP_SET_UCONFIG_REG_INDEX_index_enum  index
    ) const
{
#if PAL_ENABLE_PRINTS_ASSERTS
    CheckShadowedUserConfigRegs(startRegAddr, endRegAddr);
#endif

    // resetFilterCam is not valid for Gfx9.
    PAL_ASSERT((m_gfxIpLevel != GfxIpLevel::GfxIp9) || (resetFilterCam == false));

    const uint32 packetSize = ConfigRegSizeDwords + endRegAddr - startRegAddr + 1;
    auto*const   pPacket    = static_cast<PM4_PFP_SET_UCONFIG_REG*>(pBuffer);

    IT_OpCodeType opCode = IT_SET_UCONFIG_REG;
    if (index != index__pfp_set_uconfig_reg_index__default)
    {
        // GFX9 started supporting uconfig-reg-index as of ucode version 26.
        if ((m_cpUcodeVersion >= 26)
            || IsGfx10(m_gfxIpLevel)
            )
        {
            //    SW needs to change from using the IT_SET_UCONFIG_REG to IT_SET_UCONFIG_REG_INDEX when using the
            //    "index" field to access the mmVGT_INDEX_TYPE and mmVGT_NUM_INSTANCE registers.
            //
            opCode = IT_SET_UCONFIG_REG_INDEX;
        }
        else
        {
            // Ok, we still have a non-zero index, but the device doesn't support the new and improved
            // uconfig-index packet.  This uses a different enumeration.  Verify that the "old" and "new"
            // enumerations match.
            static_assert(((static_cast<uint32>(index__pfp_set_uconfig_reg_index__prim_type__GFX09)       ==
                            static_cast<uint32>(index__pfp_set_uconfig_reg__prim_type__GFX09))       &&
                           (static_cast<uint32>(index__pfp_set_uconfig_reg_index__index_type)             ==
                            static_cast<uint32>(index__pfp_set_uconfig_reg__index_type__GFX09))      &&
                           (static_cast<uint32>(index__pfp_set_uconfig_reg_index__num_instances)          ==
                            static_cast<uint32>(index__pfp_set_uconfig_reg__num_instances__GFX09))   &&
                           (static_cast<uint32>(index__pfp_set_uconfig_reg_index__multi_vgt_param__GFX09) ==
                            static_cast<uint32>(index__pfp_set_uconfig_reg__multi_vgt_param__GFX09))),
                          "uconfig index enumerations have changed across old and new packets!");
        }
    }

    pPacket->header.u32All  = Type3Header(opCode, packetSize, resetFilterCam);
    pPacket->ordinal2       = Type3Ordinal2((startRegAddr - UCONFIG_SPACE_START), index);

    return packetSize;
}

template
size_t CmdUtil::BuildSetSeqConfigRegs<false>(
    uint32                                startRegAddr,
    uint32                                endRegAddr,
    void*                                 pBuffer,
    PFP_SET_UCONFIG_REG_INDEX_index_enum  index) const;
template
size_t CmdUtil::BuildSetSeqConfigRegs<true>(
    uint32                                startRegAddr,
    uint32                                endRegAddr,
    void*                                 pBuffer,
    PFP_SET_UCONFIG_REG_INDEX_index_enum  index) const;

// =====================================================================================================================
// Builds a PM4 packet which sets one SH register. Returns size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildSetOneShReg(
    uint32        regAddr,
    Pm4ShaderType shaderType,
    void*         pBuffer     // [out] Build the PM4 packet in this buffer.
    ) const
{
    return BuildSetSeqShRegs(regAddr, regAddr, shaderType, pBuffer);
}

// =====================================================================================================================
// Builds a PM4 IT_SET_SH_REG_INDEX packet using index provided. Returns size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildSetOneShRegIndex(
    uint32                          regAddr,
    Pm4ShaderType                   shaderType,
    PFP_SET_SH_REG_INDEX_index_enum index,
    void*                           pBuffer     // [out] Build the PM4 packet in this buffer.
    ) const
{
    return BuildSetSeqShRegsIndex(regAddr, regAddr, shaderType, index, pBuffer);
}

// =====================================================================================================================
// Builds a PM4 packet which sets a sequence of Graphics SH registers starting with startRegAddr and ending with
// endRegAddr (inclusive). Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildSetSeqShRegs(
    uint32        startRegAddr,
    uint32        endRegAddr,
    Pm4ShaderType shaderType,
    void*         pBuffer       // [out] Build the PM4 packet in this buffer.
    ) const
{
#if PAL_ENABLE_PRINTS_ASSERTS
    CheckShadowedShRegs(shaderType, startRegAddr, endRegAddr);
#endif

    const uint32         packetSize = ShRegSizeDwords + endRegAddr - startRegAddr + 1;
    auto*const           pPacket    = static_cast<PM4_ME_SET_SH_REG*>(pBuffer);

    pPacket->header.u32All         = Type3Header(IT_SET_SH_REG, packetSize, false, shaderType);
    pPacket->ordinal2              = 0;
    pPacket->bitfields2.reg_offset = startRegAddr - PERSISTENT_SPACE_START;

    return packetSize;
}

// =====================================================================================================================
// Builds a PM4 packet which sets a sequence of Graphics SH registers starting with startRegAddr and ending with
// endRegAddr (inclusive). Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildSetSeqShRegsIndex(
    uint32                          startRegAddr,
    uint32                          endRegAddr,
    Pm4ShaderType                   shaderType,
    PFP_SET_SH_REG_INDEX_index_enum index,
    void*                           pBuffer       // [out] Build the PM4 packet in this buffer.
    ) const
{
#if PAL_ENABLE_PRINTS_ASSERTS
    CheckShadowedShRegs(shaderType, startRegAddr, endRegAddr);
#endif

    // Minimum microcode feature version required by gfx-9 hardware to support the packet SET_SH_REG_INDEX
    constexpr uint32 MinUcodeFeatureVersionForSetShRegIndex = 26;
    size_t packetSize = 0;

    // Switch to the SET_SH_REG opcode for setting the registers if SET_SH_REG_INDEX opcode is not supported.
    if ((m_gfxIpLevel == GfxIpLevel::GfxIp9) && (m_cpUcodeVersion < MinUcodeFeatureVersionForSetShRegIndex))
    {
        packetSize = BuildSetSeqShRegs(startRegAddr, endRegAddr, shaderType, pBuffer);
    }
    else
    {
        packetSize         = ShRegIndexSizeDwords + endRegAddr - startRegAddr + 1;
        auto*const pPacket = static_cast<PM4_PFP_SET_SH_REG_INDEX*>(pBuffer);

        pPacket->header.u32All = Type3Header(IT_SET_SH_REG_INDEX, static_cast<uint32>(packetSize), false, shaderType);
        pPacket->ordinal2              = 0;
        pPacket->bitfields2.index      = index;
        pPacket->bitfields2.reg_offset = startRegAddr - PERSISTENT_SPACE_START;
    }

    return packetSize;
}

// =====================================================================================================================
// Builds a PM4 packet which sets one context register. Note that unlike R6xx/EG/NI, GCN has no compute contexts, so all
// context registers are for graphics. The index field is used to set special registers and should be set to zero except
// when setting one of those registers. Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildSetOneContextReg(
    uint32                               regAddr,
    void*                                pBuffer, // [out] Build the PM4 packet in this buffer.
    PFP_SET_CONTEXT_REG_INDEX_index_enum index
    ) const
{
    PAL_ASSERT((regAddr != mmVGT_LS_HS_CONFIG) || (index == index__pfp_set_context_reg_index__vgt_ls_hs_config__GFX09));
    return BuildSetSeqContextRegs(regAddr, regAddr, pBuffer, index);
}

// =====================================================================================================================
// Builds a PM4 packet which sets a sequence of context registers starting with startRegAddr and ending with endRegAddr
// (inclusive). All context registers are for graphics. Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildSetSeqContextRegs(
    uint32                               startRegAddr,
    uint32                               endRegAddr,
    void*                                pBuffer, // [out] Build the PM4 packet in this buffer.
    PFP_SET_CONTEXT_REG_INDEX_index_enum index
    ) const
{
#if PAL_ENABLE_PRINTS_ASSERTS
    CheckShadowedContextRegs(startRegAddr, endRegAddr);
#endif

    const uint32 packetSize = ContextRegSizeDwords + endRegAddr - startRegAddr + 1;
    auto*const   pPacket    = static_cast<PM4_PFP_SET_CONTEXT_REG*>(pBuffer);

    pPacket->header.u32All  = Type3Header(IT_SET_CONTEXT_REG, packetSize);
    pPacket->ordinal2       = Type3Ordinal2((startRegAddr - CONTEXT_SPACE_START), index);

    return packetSize;
}

// =====================================================================================================================
// Builds a SET_PREDICATION packet. Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildSetPredication(
    gpusize       gpuVirtAddr,
    bool          predicationBool,    // Controls the polarity of the predication test. E.g., for occlusion predicates,
                                      // true indicates to draw if any pixels passed the Z-test while false indicates
                                      // to draw if no pixels passed the Z-test.
    bool          occlusionHint,      // Controls whether the hardware should wait for all ZPASS data to be written by
                                      // the DB's before proceeding. True chooses to wait until all ZPASS data is ready,
                                      // false chooses to assume that the draw should not be skipped if the ZPASS data
                                      // is not ready yet.
    PredicateType predType,
    bool          continuePredicate,  // Contols how data is accumulated across cmd buffer boundaries. True indicates
                                      // that this predicate is a continuation of the previous one, accumulating data
                                      // between them.
    void*         pBuffer)
{
    static_assert(
        (static_cast<PFP_SET_PREDICATION_pred_op_enum>(PredicateType::Zpass)     ==
                pred_op__pfp_set_predication__set_zpass_predicate) &&
        (static_cast<PFP_SET_PREDICATION_pred_op_enum>(PredicateType::PrimCount) ==
                pred_op__pfp_set_predication__set_primcount_predicate) &&
        (static_cast<PFP_SET_PREDICATION_pred_op_enum>(PredicateType::Boolean)   ==
                pred_op__pfp_set_predication__DX12),
        "Unexpected values for the PredicateType enum.");

    constexpr uint32 PacketSize = (sizeof(PM4PFP_SET_PREDICATION) / sizeof(uint32));
    auto*const       pPacket    = static_cast<PM4PFP_SET_PREDICATION*>(pBuffer);

    // The predication memory address must be 16-byte aligned, and cannot be wider than 40 bits.
    PAL_ASSERT(((gpuVirtAddr & 0xF) == 0) && (gpuVirtAddr <= ((1uLL << 40) - 1)));

    pPacket->header.u32All = Type3Header(IT_SET_PREDICATION, PacketSize);
    pPacket->ordinal3      = LowPart(gpuVirtAddr);
    pPacket->start_addr_hi = (HighPart(gpuVirtAddr) & 0xFF);

    // Verify that the address is properly aligned
    PAL_ASSERT (pPacket->bitfields3.reserved1 == 0);

    pPacket->ordinal2                = 0;
    pPacket->bitfields2.pred_bool    = (predicationBool
                                            ? pred_bool__pfp_set_predication__draw_if_visible_or_no_overflow
                                            : pred_bool__pfp_set_predication__draw_if_not_visible_or_overflow);
    pPacket->bitfields2.hint         = (((predType == PredicateType::Zpass) && occlusionHint)
                                            ? hint__pfp_set_predication__draw_if_not_final_zpass_written
                                            : hint__pfp_set_predication__wait_until_final_zpass_written);
    pPacket->bitfields2.pred_op      = static_cast<PFP_SET_PREDICATION_pred_op_enum>(predType);
    pPacket->bitfields2.continue_bit = (((predType == PredicateType::Zpass) && continuePredicate)
                                            ? continue_bit__pfp_set_predication__continue_set_predication
                                            : continue_bit__pfp_set_predication__new_set_predication);

    return PacketSize;
}

// =====================================================================================================================
// Builds a STRMOUT_BUFFER_UPDATE packet. Returns the size of the PM4 command assembled, in DWORDs.
// All operations except STRMOUT_CNTL_OFFSET_SEL_NONE will internally issue a VGT_STREAMOUT_RESET.
size_t CmdUtil::BuildStrmoutBufferUpdate(
    uint32  bufferId,
    uint32  sourceSelect,   // Controls which streamout update operation to perform.
    uint32  explicitOffset, // When sourceSelect = EXPLICIT_OFFSET, this is the value to be written into the buffer
                            // filled size counter.
    gpusize dstGpuVirtAddr, // When sourceSelect = NONE, this is the GPU virtual address where the buffer filled size
                            // will be written-to.
    gpusize srcGpuVirtAddr, // When sourceSelect = READ_SRC_ADDRESS, this is the GPU virtual address where the buffer
                            // filled size will be read from.
    void*   pBuffer)        // [out] Build the PM4 packet in this buffer.
{
    static_assert((sizeof(PM4_PFP_STRMOUT_BUFFER_UPDATE) == sizeof(PM4_ME_STRMOUT_BUFFER_UPDATE)),
                  "STRMOUT_BUFFER_UPDATE packet differs between PFP and ME!");

    static_assert(
        ((static_cast<uint32>(source_select__pfp_strmout_buffer_update__use_buffer_offset)                   ==
          static_cast<uint32>(source_select__me_strmout_buffer_update__use_buffer_offset))                   &&
         (static_cast<uint32>(source_select__pfp_strmout_buffer_update__read_vgt_strmout_buffer_filled_size) ==
          static_cast<uint32>(source_select__me_strmout_buffer_update__read_vgt_strmout_buffer_filled_size)) &&
         (static_cast<uint32>(source_select__pfp_strmout_buffer_update__from_src_address)                    ==
          static_cast<uint32>(source_select__me_strmout_buffer_update__from_src_address))                    &&
         (static_cast<uint32>(source_select__pfp_strmout_buffer_update__none)                                ==
          static_cast<uint32>(source_select__me_strmout_buffer_update__none))),
         "source_select enum is different between PFP and ME!");

    static_assert(
        ((static_cast<uint32>(buffer_select__pfp_strmout_buffer_update__stream_out_buffer_0) ==
          static_cast<uint32>(buffer_select__me_strmout_buffer_update__stream_out_buffer_0)) &&
         (static_cast<uint32>(buffer_select__pfp_strmout_buffer_update__stream_out_buffer_1) ==
          static_cast<uint32>(buffer_select__me_strmout_buffer_update__stream_out_buffer_1)) &&
         (static_cast<uint32>(buffer_select__pfp_strmout_buffer_update__stream_out_buffer_2) ==
          static_cast<uint32>(buffer_select__me_strmout_buffer_update__stream_out_buffer_2)) &&
         (static_cast<uint32>(buffer_select__pfp_strmout_buffer_update__stream_out_buffer_3) ==
          static_cast<uint32>(buffer_select__me_strmout_buffer_update__stream_out_buffer_3))),
         "buffer_select enum is different between PFP and ME!");

    constexpr uint32 PacketSize = (sizeof(PM4_PFP_STRMOUT_BUFFER_UPDATE) / sizeof(uint32));
    auto*const       pPacket    = static_cast<PM4_PFP_STRMOUT_BUFFER_UPDATE*>(pBuffer);

    pPacket->header.u32All            = Type3Header(IT_STRMOUT_BUFFER_UPDATE, PacketSize);
    pPacket->ordinal2                 = 0;
    pPacket->bitfields2.update_memory = update_memory__pfp_strmout_buffer_update__dont_update_memory;
    pPacket->bitfields2.source_select = static_cast<PFP_STRMOUT_BUFFER_UPDATE_source_select_enum>(sourceSelect);
    pPacket->bitfields2.buffer_select = static_cast<PFP_STRMOUT_BUFFER_UPDATE_buffer_select_enum>(bufferId);
    pPacket->ordinal3                 = 0;
    pPacket->dst_address_hi           = 0;
    pPacket->offset_or_address_lo     = 0;
    pPacket->src_address_hi           = 0;

    constexpr PFP_STRMOUT_BUFFER_UPDATE_data_type_enum DataType = data_type__pfp_strmout_buffer_update__bytes;

    switch (sourceSelect)
    {
    case source_select__pfp_strmout_buffer_update__use_buffer_offset:
        pPacket->offset_or_address_lo = explicitOffset;
        break;
    case source_select__pfp_strmout_buffer_update__read_vgt_strmout_buffer_filled_size:
        // No additional members need to be set for this operation.
        break;
    case source_select__pfp_strmout_buffer_update__from_src_address:
        pPacket->offset_or_address_lo = LowPart(srcGpuVirtAddr);
        pPacket->src_address_hi       = HighPart(srcGpuVirtAddr);
        pPacket->bitfields2.data_type = DataType;
        break;
    case source_select__pfp_strmout_buffer_update__none:
        pPacket->bitfields2.update_memory = update_memory__pfp_strmout_buffer_update__update_memory_at_dst_address;
        pPacket->ordinal3                 = LowPart(dstGpuVirtAddr);
        PAL_ASSERT(pPacket->bitfields3.reserved1 == 0);
        pPacket->dst_address_hi           = HighPart(dstGpuVirtAddr);
        pPacket->bitfields2.data_type     = DataType;
        break;
    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    return PacketSize;
}

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

    return BuildDmaData(dmaDataInfo, pBuffer);
}

// =====================================================================================================================
// Builds a PM4 command to stall the DE until the CE counter is positive, then decrements the CE counter. Returns the
// size of the PM4 command written, in DWORDs.
size_t CmdUtil::BuildWaitOnCeCounter(
    bool  invalidateKcache,
    void* pBuffer) // [out] Build the PM4 packet in this buffer.
{
    constexpr uint32 PacketSize = (sizeof(PM4_ME_WAIT_ON_CE_COUNTER) / sizeof(uint32));
    auto*const       pPacket    = static_cast<PM4_ME_WAIT_ON_CE_COUNTER*>(pBuffer);

    pPacket->header.u32All                = Type3Header(IT_WAIT_ON_CE_COUNTER, PacketSize);
    pPacket->ordinal2                     = 0;
    pPacket->bitfields2.cond_surface_sync = invalidateKcache;

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 command to stall the CE until it is less than the specified number of draws ahead of the DE. Returns
// the size of the PM4 command written, in DWORDs.
size_t CmdUtil::BuildWaitOnDeCounterDiff(
    uint32 counterDiff,
    void*  pBuffer)     // [out] Build the PM4 packet in this buffer.
{
    constexpr uint32 PacketSize = (sizeof(PM4_CE_WAIT_ON_DE_COUNTER_DIFF) / sizeof(uint32));
    auto*const       pPacket    = static_cast<PM4_CE_WAIT_ON_DE_COUNTER_DIFF*>(pBuffer);

    pPacket->header.u32All = Type3Header(IT_WAIT_ON_DE_COUNTER_DIFF, PacketSize);
    pPacket->diff          = counterDiff;

    return PacketSize;
}

// =====================================================================================================================
// Builds a set of PM4 commands that update a timestamp value to a known value, writes an EOP timestamp event with a
// known different value then waits for the timestamp value to update. Returns the size of the PM4 command built, in
// DWORDs.
size_t CmdUtil::BuildWaitOnReleaseMemEvent(
    EngineType     engineType,
    VGT_EVENT_TYPE vgtEvent,
    TcCacheOp      tcCacheOp,
    gpusize        gpuAddr,
    void*          pBuffer
    ) const
{
    constexpr uint32 ClearedTimestamp   = 0x11111111;
    constexpr uint32 CompletedTimestamp = 0x22222222;

    // These are the only event types supported by this packet sequence.
    PAL_ASSERT((vgtEvent == PS_DONE) || (vgtEvent == CS_DONE) || VgtEventHasTs[vgtEvent]);

    // Write a known value to the timestamp.
    WriteDataInfo writeData = {};
    writeData.engineType = engineType;
    writeData.dstAddr    = gpuAddr;
    writeData.engineSel  = engine_sel__me_write_data__micro_engine;
    writeData.dstSel     = dst_sel__me_write_data__tc_l2;

    size_t totalSize = BuildWriteData(writeData, ClearedTimestamp, pBuffer);

    // Issue the specified timestamp event.
    ReleaseMemInfo releaseInfo = {};
    releaseInfo.engineType     = engineType;
    releaseInfo.vgtEvent       = vgtEvent;
    releaseInfo.tcCacheOp      = tcCacheOp;
    releaseInfo.dstAddr        = gpuAddr;
    releaseInfo.dataSel        = data_sel__me_release_mem__send_32_bit_low;
    releaseInfo.data           = CompletedTimestamp;

    totalSize += BuildReleaseMem(releaseInfo, static_cast<uint32*>(pBuffer) + totalSize);

    // Wait on the timestamp value.
    totalSize += BuildWaitRegMem(engineType,
                                 mem_space__me_wait_reg_mem__memory_space,
                                 function__me_wait_reg_mem__equal_to_the_reference_value,
                                 engine_sel__me_wait_reg_mem__micro_engine,
                                 gpuAddr,
                                 CompletedTimestamp,
                                 0xFFFFFFFF,
                                 static_cast<uint32*>(pBuffer) + totalSize);

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
    void*         pBuffer)   // [out] Build the PM4 packet in this buffer.
{
    static_assert((sizeof(PM4_ME_WAIT_REG_MEM) == sizeof(PM4_MEC_WAIT_REG_MEM)),
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
    constexpr uint32 PacketSize           = (sizeof(PM4_ME_WAIT_REG_MEM) / sizeof(uint32));
    auto*const pPacket                    = static_cast<PM4_ME_WAIT_REG_MEM*>(pBuffer);
    auto*const pPacketMecOnly             = static_cast<PM4_MEC_WAIT_REG_MEM*>(pBuffer);

    pPacket->header.u32All                = Type3Header(IT_WAIT_REG_MEM, PacketSize);
    pPacket->ordinal2                     = 0;
    pPacket->bitfields2.function          = static_cast<ME_WAIT_REG_MEM_function_enum>(function);
    pPacket->bitfields2.mem_space         = static_cast<ME_WAIT_REG_MEM_mem_space_enum>(memSpace);
    pPacket->bitfields2.operation         = operation__me_wait_reg_mem__wait_reg_mem;
    pPacket->ordinal3                     = LowPart(addr);

    if (memSpace == mem_space__me_wait_reg_mem__memory_space)
    {
        PAL_ASSERT(pPacket->bitfields3a.reserved1 == 0);
    }
    else if (memSpace == mem_space__mec_wait_reg_mem__register_space)
    {
        PAL_ASSERT(pPacket->bitfields3b.reserved1 == 0);
    }

    pPacket->mem_poll_addr_hi             = HighPart(addr);
    pPacket->reference                    = reference;
    pPacket->mask                         = mask;
    pPacket->ordinal7                     = 0;
    pPacket->bitfields7.poll_interval     = Pal::Device::PollInterval;

    if (Pal::Device::EngineSupportsGraphics(engineType))
    {
        pPacket->bitfields2.engine_sel = static_cast<ME_WAIT_REG_MEM_engine_sel_enum>(engine);
    }
    else
    {
        // Similarily to engine_sel in ME, this ACE offload optimization is only for MEC and a reserved bit for ME.
        pPacketMecOnly->bitfields7.optimize_ace_offload_mode = 1;
    }

    return PacketSize;
}

// =====================================================================================================================
// Builds a WAIT_REG_MEM64 PM4 packet. Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildWaitRegMem64(
    EngineType    engineType,
    uint32        memSpace,
    uint32        function,
    uint32        engine,
    gpusize       addr,
    uint64        reference,
    uint64        mask,
    void*         pBuffer)   // [out] Build the PM4 packet in this buffer.
{
    static_assert((sizeof(PM4_ME_WAIT_REG_MEM64) == sizeof(PM4_MEC_WAIT_REG_MEM64)),
                  "WAIT_REG_MEM64 has different sizes between compute and gfx!");
    static_assert(
        ((static_cast<uint32>(function__me_wait_reg_mem64__always_pass)                             ==
          static_cast<uint32>(function__mec_wait_reg_mem64__always_pass))                           &&
         (static_cast<uint32>(function__me_wait_reg_mem64__less_than_ref_value)                     ==
          static_cast<uint32>(function__mec_wait_reg_mem64__less_than_ref_value))                   &&
         (static_cast<uint32>(function__me_wait_reg_mem64__less_than_equal_to_the_ref_value)        ==
          static_cast<uint32>(function__mec_wait_reg_mem64__less_than_equal_to_the_ref_value))      &&
         (static_cast<uint32>(function__me_wait_reg_mem64__equal_to_the_reference_value)            ==
          static_cast<uint32>(function__mec_wait_reg_mem64__equal_to_the_reference_value))          &&
         (static_cast<uint32>(function__me_wait_reg_mem64__not_equal_reference_value)               ==
          static_cast<uint32>(function__mec_wait_reg_mem64__not_equal_reference_value))             &&
         (static_cast<uint32>(function__me_wait_reg_mem64__greater_than_or_equal_reference_value)   ==
          static_cast<uint32>(function__mec_wait_reg_mem64__greater_than_or_equal_reference_value)) &&
         (static_cast<uint32>(function__me_wait_reg_mem64__greater_than_reference_value)            ==
          static_cast<uint32>(function__mec_wait_reg_mem64__greater_than_reference_value))),
        "Function enumerations don't match between ME and MEC!");
    static_assert(
        ((static_cast<uint32>(mem_space__me_wait_reg_mem64__register_space)   ==
          static_cast<uint32>(mem_space__mec_wait_reg_mem64__register_space)) &&
         (static_cast<uint32>(mem_space__me_wait_reg_mem64__memory_space)     ==
          static_cast<uint32>(mem_space__mec_wait_reg_mem64__memory_space))),
        "Memory space enumerations don't match between ME and MEC!");
    static_assert(
        ((static_cast<uint32>(operation__me_wait_reg_mem64__wait_reg_mem)         ==
          static_cast<uint32>(operation__mec_wait_reg_mem64__wait_reg_mem))       &&
         (static_cast<uint32>(operation__me_wait_reg_mem64__wait_mem_preemptable) ==
          static_cast<uint32>(operation__mec_wait_reg_mem64__wait_mem_preemptable))),
        "Operation enumerations don't match between ME and MEC!");

    // We build the packet with the ME definition, but the MEC definition is identical, so it should work...
    constexpr uint32 PacketSize           = (sizeof(PM4_ME_WAIT_REG_MEM64) / sizeof(uint32));
    auto*const pPacket                    = static_cast<PM4_ME_WAIT_REG_MEM64*>(pBuffer);
    auto*const pPacketMecOnly             = static_cast<PM4_MEC_WAIT_REG_MEM64*>(pBuffer);

    pPacket->header.u32All                = Type3Header(IT_WAIT_REG_MEM64, PacketSize);
    pPacket->ordinal2                     = 0;
    pPacket->bitfields2.function          = static_cast<ME_WAIT_REG_MEM64_function_enum>(function);
    pPacket->bitfields2.mem_space         = static_cast<ME_WAIT_REG_MEM64_mem_space_enum>(memSpace);
    pPacket->bitfields2.operation         = operation__me_wait_reg_mem64__wait_reg_mem;
    pPacket->bitfields2.engine_sel        = static_cast<ME_WAIT_REG_MEM64_engine_sel_enum>(engine);
    pPacket->ordinal3                     = LowPart(addr);
    PAL_ASSERT(pPacket->bitfields3a.reserved1 == 0);
    pPacket->mem_poll_addr_hi             = HighPart(addr);
    pPacket->reference                    = LowPart(reference);
    pPacket->reference_hi                 = HighPart(reference);
    pPacket->mask                         = LowPart(mask);
    pPacket->mask_hi                      = HighPart(mask);
    pPacket->ordinal9                     = 0;
    pPacket->bitfields9.poll_interval     = Pal::Device::PollInterval;

    if (Pal::Device::EngineSupportsGraphics(engineType))
    {
        pPacket->bitfields2.engine_sel = static_cast<ME_WAIT_REG_MEM64_engine_sel_enum>(engine);
    }
    else
    {
        // Similarily to engine_sel in ME, this ACE offload optimization is only for MEC and a reserved bit for ME.
        pPacketMecOnly->bitfields9.optimize_ace_offload_mode = 1;
    }

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 constant engine command to write the specified amount of data from CPU memory into CE RAM. Returns the
// size of the PM4 command written, in DWORDs.
size_t CmdUtil::BuildWriteConstRam(
    const void* pSrcData,       // [in] Pointer to source data in CPU memory
    uint32      ramByteOffset,  // Offset into CE RAM. Must be 4-byte aligned.
    uint32      dwordSize,      // Amount of data to write, in DWORDs
    void*       pBuffer)        // [out] Build the PM4 packet in this buffer.
{
    const uint32 packetSize = (sizeof(PM4_CE_WRITE_CONST_RAM) / sizeof(uint32)) + dwordSize;
    auto*const   pPacket    = static_cast<PM4_CE_WRITE_CONST_RAM*>(pBuffer);

    pPacket->header.u32All     = Type3Header(IT_WRITE_CONST_RAM, packetSize);
    pPacket->ordinal2          = 0;
    pPacket->bitfields2.offset = ramByteOffset;

    // Copy the data into the buffer after the packet.
    memcpy(pPacket + 1, pSrcData, dwordSize * sizeof(uint32));

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
// Builds a WRITE-DATA packet for either the MEC or ME engine.  Writes the data in "pData" into the GPU memory
// address "dstAddr".
size_t CmdUtil::BuildWriteDataInternal(
    const WriteDataInfo& info,
    size_t               dwordsToWrite,
    void*                pBuffer)       // [out] Build the PM4 packet in this buffer.
{
    static_assert(sizeof(PM4MEC_WRITE_DATA) == sizeof(PM4ME_WRITE_DATA),
        "write_data packet has different sizes between compute and gfx!");
    static_assert(
        ((static_cast<uint32>(dst_sel__mec_write_data__mem_mapped_register) ==
          static_cast<uint32>(dst_sel__me_write_data__mem_mapped_register)) &&
         (static_cast<uint32>(dst_sel__mec_write_data__tc_l2)               ==
          static_cast<uint32>(dst_sel__me_write_data__tc_l2))               &&
         (static_cast<uint32>(dst_sel__mec_write_data__gds)                 ==
          static_cast<uint32>(dst_sel__me_write_data__gds))                 &&
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
        ((static_cast<uint32>(addr_incr__me_write_data__do_not_increment_address) ==
          static_cast<uint32>(addr_incr__mec_write_data__do_not_increment_address)) &&
         (static_cast<uint32>(addr_incr__me_write_data__increment_address)        ==
          static_cast<uint32>(addr_incr__mec_write_data__increment_address))),
         "ADDR_INCR enumerations don't match between MEC and ME!");

    // We build the packet with the ME definition, but the MEC definition is identical, so it should work...
    const uint32 packetSize = static_cast<uint32>((sizeof(PM4ME_WRITE_DATA) / sizeof(uint32)) + dwordsToWrite);
    auto*const   pPacket    = static_cast<PM4ME_WRITE_DATA*>(pBuffer);

    pPacket->header.u32All           = Type3Header(IT_WRITE_DATA, packetSize, false, ShaderGraphics, info.predicate);
    pPacket->ordinal2                = 0;
    pPacket->bitfields2.addr_incr    = info.dontIncrementAddr
                                            ? addr_incr__me_write_data__do_not_increment_address
                                            : addr_incr__me_write_data__increment_address;
    pPacket->bitfields2.cache_policy = cache_policy__me_write_data__lru;
    pPacket->bitfields2.dst_sel      = static_cast<ME_WRITE_DATA_dst_sel_enum>(info.dstSel);
    pPacket->bitfields2.wr_confirm   = info.dontWriteConfirm
                                            ? wr_confirm__me_write_data__do_not_wait_for_write_confirmation
                                            : wr_confirm__me_write_data__wait_for_write_confirmation;
    pPacket->bitfields2.engine_sel   = static_cast<ME_WRITE_DATA_engine_sel_enum>(info.engineSel);
    pPacket->ordinal3                = LowPart(info.dstAddr);
    pPacket->dst_mem_addr_hi         = HighPart(info.dstAddr);

    switch (info.dstSel)
    {
    case dst_sel__me_write_data__mem_mapped_register:
        PAL_ASSERT(pPacket->bitfields3a.reserved1 == 0);
        break;

    case dst_sel__me_write_data__memory:
    case dst_sel__me_write_data__tc_l2:
        PAL_ASSERT(pPacket->bitfields3c.reserved1 == 0);
        break;

    case dst_sel__me_write_data__gds:
        PAL_ASSERT(pPacket->bitfields3b.reserved1 == 0);
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

    return packetSize;
}

// =====================================================================================================================
// Builds a WRITE_DATA PM4 packet. If pPeriodData is non-null its contents (of length dwordsPerPeriod) will be copied
// into the data payload periodsToWrite times. Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildWriteDataPeriodic(
    const WriteDataInfo& info,
    size_t               dwordsPerPeriod,
    size_t               periodsToWrite,
    const uint32*        pPeriodData,
    void*                pBuffer)         // [out] Build the PM4 packet in this buffer.
{
    const size_t dwordsToWrite               = dwordsPerPeriod * periodsToWrite;
    const size_t packetSizeWithWrittenDwords = BuildWriteDataInternal(info, dwordsToWrite, pBuffer);
    const size_t packetSizeInBytes           = (packetSizeWithWrittenDwords - dwordsToWrite) * sizeof(uint32);

    PAL_ASSERT(pPeriodData != nullptr);

    // Copy the data into the buffer after the packet.
    const size_t bytesPerPeriod = sizeof(uint32) * dwordsPerPeriod;
    uint32*      pDataSection   = reinterpret_cast<uint32*>(VoidPtrInc(pBuffer, packetSizeInBytes));

    for (; periodsToWrite > 0; periodsToWrite--)
    {
        memcpy(pDataSection, pPeriodData, bytesPerPeriod);
        pDataSection += dwordsPerPeriod;
    }

    return packetSizeWithWrittenDwords;
}

// =====================================================================================================================
// Builds an NOP PM4 packet with the ASCII string comment embedded inside. The comment is preceeded by a signature
// that analysis tools can use to tell that this is a comment.
size_t CmdUtil::BuildCommentString(
    const char* pComment,
    void*       pBuffer)
{
    const size_t stringLength         = strlen(pComment) + 1;
    const size_t packetSize           =
        (Util::RoundUpToMultiple(sizeof(PM4PFP_NOP) + stringLength, sizeof(uint32)) / sizeof(uint32)) + 3;
    PM4PFP_NOP*  pPacket              = static_cast<PM4PFP_NOP*>(pBuffer);
    uint32*      pData                = reinterpret_cast<uint32*>(pPacket + 1);

    PAL_ASSERT(stringLength < CmdBuffer::MaxCommentStringLength);

    // Build header (NOP, signature, size, type)
    pPacket->header.u32All = Type3Header(IT_NOP, static_cast<uint32>(packetSize));
    pData[0]               = CmdBuffer::CommentSignature;
    pData[1]               = static_cast<uint32>(packetSize);
    pData[2]               = static_cast<uint32>(CmdBufferCommentType::String);

    // Append data
    memcpy(pData + 3, pComment, stringLength);

    return packetSize;
}

// =====================================================================================================================
// Builds an NOP PM4 packet with the payload data embedded inside.
size_t CmdUtil::BuildNopPayload(
    const void* pPayload,
    uint32      payloadSize,
    void*       pBuffer
    ) const
{
    const size_t packetSize = payloadSize + (sizeof(PM4PFP_NOP) / sizeof(uint32));
    auto*const   pPacket    = static_cast<PM4PFP_NOP*>(pBuffer);
    uint32*      pData      = reinterpret_cast<uint32*>(pPacket + 1);

    // Build header (NOP, signature, size, type)
    pPacket->header.u32All = Type3Header(IT_NOP, static_cast<uint32>(packetSize));

    // Append data
    memcpy(pData, pPayload, payloadSize * sizeof(uint32));

    return packetSize;
}

// =====================================================================================================================
// Issue commands to prime caches for access of a new pipeline.  This can be done with two methods:
//
// 1. Issue a CPDMA operation that will read the pipeline data through L2 then write it to "nowhere" (a new feature
//    with GFX9).  This will prime the VM translation cache (UTCL2) as well as the L2 data cache.
// 2. Issue a new packet that will only prime the VM translation cache (UTCL2).
void CmdUtil::BuildPipelinePrefetchPm4(
    const PipelineUploader& uploader,
    PipelinePrefetchPm4*    pOutput
    ) const
{
    const PalSettings&     coreSettings = m_device.Parent()->Settings();
    const Gfx9PalSettings& hwlSettings  = m_device.Settings();

    if (coreSettings.pipelinePrefetchEnable)
    {
        uint32 prefetchSize = static_cast<uint32>(uploader.PrefetchSize());

        if (coreSettings.shaderPrefetchClampSize != 0)
        {
            prefetchSize = Min(prefetchSize, coreSettings.shaderPrefetchClampSize);
        }

        if (hwlSettings.shaderPrefetchMethod == PrefetchCpDma)
        {
            DmaDataInfo dmaDataInfo  = { };
            dmaDataInfo.dstAddr      = 0;
            dmaDataInfo.dstAddrSpace = das__pfp_dma_data__memory;
            dmaDataInfo.dstSel       = dst_sel__pfp_dma_data__dst_nowhere;
            dmaDataInfo.srcAddr      = uploader.PrefetchAddr();
            dmaDataInfo.srcAddrSpace = sas__pfp_dma_data__memory;
            dmaDataInfo.srcSel       = src_sel__pfp_dma_data__src_addr_using_l2;
            dmaDataInfo.numBytes     = prefetchSize;
            dmaDataInfo.disWc        = true;

            BuildDmaData(dmaDataInfo, &pOutput->dmaData);
            pOutput->spaceNeeded = sizeof(pOutput->dmaData) / sizeof(uint32);
        }
        else
        {
            PAL_ASSERT(hwlSettings.shaderPrefetchMethod == PrefetchPrimeUtcL2);

            const gpusize firstPage = Pow2AlignDown(uploader.PrefetchAddr(), PrimeUtcL2MemAlignment);
            const gpusize lastPage  = Pow2AlignDown(uploader.PrefetchAddr() + prefetchSize - 1, PrimeUtcL2MemAlignment);
            const size_t  numPages  = 1 + static_cast<size_t>((lastPage - firstPage) / PrimeUtcL2MemAlignment);

            BuildPrimeUtcL2(firstPage,
                            cache_perm__pfp_prime_utcl2__execute,
                            prime_mode__pfp_prime_utcl2__dont_wait_for_xack,
                            engine_sel__pfp_prime_utcl2__prefetch_parser,
                            numPages,
                            &pOutput->primeUtcl2);
            pOutput->spaceNeeded = sizeof(pOutput->primeUtcl2) / sizeof(uint32);
        }
    }
    else
    {
        pOutput->spaceNeeded = 0;
    }
}

// =====================================================================================================================
// Translates between the API compare func and the WaitRegMem comparison enumerations.
ME_WAIT_REG_MEM_function_enum CmdUtil::WaitRegMemFunc(
    CompareFunc compareFunc)
{
    constexpr ME_WAIT_REG_MEM_function_enum xlateCompareFunc[]=
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
    PAL_ASSERT(compareFunc32 < sizeof(xlateCompareFunc)/sizeof(ME_WAIT_REG_MEM_function_enum));

    return xlateCompareFunc[compareFunc32];
}

#if PAL_ENABLE_PRINTS_ASSERTS

// =====================================================================================================================
// Helper function which determines if a range of sequential register addresses fall within any of the specified
// register ranges.
PAL_INLINE bool AreRegistersInRangeList(
    uint32               startRegAddr,
    uint32               endRegAddr,
    const RegisterRange* pRanges,
    uint32               count)
{
    bool found = false;

    const RegisterRange* pRange = pRanges;
    for (uint32 i = 0; i < count; ++i, ++pRange)
    {
        // This code makes the following assumption: any pair of register ranges in pRanges are separated by at least
        // one register. This implies that we are able to also assume that both the start and end register being checked
        // fall in the same register range, or that there are registers between startRegAddr and endRegAddr which aren't
        // contained in any of the range lists.
        if ((startRegAddr >= pRange->regOffset) && (startRegAddr < (pRange->regOffset + pRange->regCount)) &&
            (endRegAddr   >= pRange->regOffset) && (endRegAddr   < (pRange->regOffset + pRange->regCount)))
        {
            found = true;
            break;
        }
    }

    return found;
}

// =====================================================================================================================
// Helper function which verifies that the specified context register falls within one of the ranges which are shadowed
// when mid command buffer preemption is enabled.
void CmdUtil::CheckShadowedContextReg(
    uint32 regAddr
    ) const
{
    CheckShadowedContextRegs(regAddr, regAddr);
}

// =====================================================================================================================
// Helper function which verifies that the specified set of sequential context registers falls within one of the ranges
// which are shadowed when mid command buffer preemption is enabled.
void CmdUtil::CheckShadowedContextRegs(
    uint32 startRegAddr,
    uint32 endRegAddr
    ) const
{
    PAL_ASSERT(IsContextReg(startRegAddr) && IsContextReg(endRegAddr));

    if (m_verifyShadowedRegisters)
    {
        uint32               numEntries = 0;
        const RegisterRange* pRange     = m_device.GetRegisterRange(RegRangeNonShadowed, &numEntries);

        if (false == AreRegistersInRangeList(startRegAddr, endRegAddr, pRange, numEntries))
        {
            pRange = m_device.GetRegisterRange(RegRangeContext, &numEntries);

            PAL_ASSERT(AreRegistersInRangeList((startRegAddr - CONTEXT_SPACE_START),
                                               (endRegAddr - CONTEXT_SPACE_START),
                                               pRange,
                                               numEntries));
        }
    }
}

// =====================================================================================================================
// Helper function which verifies that the specified set of sequential SH registers falls within one of the ranges which
// are shadowed when mid command buffer preemption is enabled.
void CmdUtil::CheckShadowedShReg(
    Pm4ShaderType shaderType,
    uint32        regAddr
    ) const
{
    CheckShadowedShRegs(shaderType, regAddr, regAddr);
}

// =====================================================================================================================
// Helper function which verifies that the specified set of sequential SH registers falls within one of the ranges which
// are shadowed when mid command buffer preemption is enabled.
void CmdUtil::CheckShadowedShRegs(
    Pm4ShaderType shaderType,
    uint32        startRegAddr,
    uint32        endRegAddr
    ) const
{
    PAL_ASSERT(IsShReg(startRegAddr) && IsShReg(endRegAddr));

    if (m_verifyShadowedRegisters)
    {
        uint32               numEntries = 0;
        const RegisterRange* pRange     = m_device.GetRegisterRange(RegRangeNonShadowed, &numEntries);

        if (false == AreRegistersInRangeList(startRegAddr, endRegAddr, pRange, numEntries))
        {
            if (shaderType == ShaderGraphics)
            {
                pRange = m_device.GetRegisterRange(RegRangeSh, &numEntries);

                PAL_ASSERT(AreRegistersInRangeList((startRegAddr - PERSISTENT_SPACE_START),
                                                   (endRegAddr - PERSISTENT_SPACE_START),
                                                   pRange,
                                                   numEntries));
            }
            else
            {
                pRange = m_device.GetRegisterRange(RegRangeCsSh, &numEntries);

                PAL_ASSERT(AreRegistersInRangeList((startRegAddr - PERSISTENT_SPACE_START),
                                                   (endRegAddr - PERSISTENT_SPACE_START),
                                                   pRange,
                                                   numEntries));
            }
        }
    }
}

// =====================================================================================================================
// Helper function which verifies that the specified set of sequential user-config registers falls within one of the
// ranges which are shadowed when mid command buffer preemption is enabled.
void CmdUtil::CheckShadowedUserConfigRegs(
    uint32 startRegAddr,
    uint32 endRegAddr
    ) const
{
    PAL_ASSERT(IsUserConfigReg(startRegAddr) && IsUserConfigReg(endRegAddr));

    if (m_verifyShadowedRegisters)
    {
        uint32               numEntries = 0;
        const RegisterRange* pRange     = m_device.GetRegisterRange(RegRangeNonShadowed, &numEntries);

        if (false == AreRegistersInRangeList(startRegAddr, endRegAddr, pRange, numEntries))
        {
            pRange = m_device.GetRegisterRange(RegRangeUserConfig, &numEntries);

            PAL_ASSERT(AreRegistersInRangeList((startRegAddr - UCONFIG_SPACE_START),
                                               (endRegAddr - UCONFIG_SPACE_START),
                                               pRange,
                                               numEntries));
        }
    }
}
#endif

} // Gfx9
} // Pal
