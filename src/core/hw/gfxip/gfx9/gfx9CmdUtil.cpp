/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/gfx9/gfx9CmdUtil.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "g_gfx9Settings.h"
#include "palInlineFuncs.h"
#include "palIterator.h"
#include "palMath.h"

using namespace Util;

namespace Pal
{
namespace Gfx9
{

static constexpr ME_EVENT_WRITE_event_index_enum VgtEventIndex[]=
{
    event_index__me_event_write__other,                                  // 0x0: Reserved_0x00,
    event_index__me_event_write__sample_streamoutstats__GFX09_10,        // 0x1: SAMPLE_STREAMOUTSTATS1,
    event_index__me_event_write__sample_streamoutstats__GFX09_10,        // 0x2: SAMPLE_STREAMOUTSTATS2,
    event_index__me_event_write__sample_streamoutstats__GFX09_10,        // 0x3: SAMPLE_STREAMOUTSTATS3,
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
    event_index__me_event_write__sample_streamoutstats__GFX09_10,        // 0x20: SAMPLE_STREAMOUTSTATS,
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

#if PAL_BUILD_GFX11
constexpr size_t PackedRegPairPacketSize = PM4_PFP_SET_SH_REG_PAIRS_PACKED_SIZEDW__GFX11;
static_assert((PackedRegPairPacketSize == PM4_PFP_SET_SH_REG_PAIRS_PACKED_SIZEDW__GFX11)      &&
              (PackedRegPairPacketSize == PM4_PFP_SET_CONTEXT_REG_PAIRS_PACKED_SIZEDW__GFX11) &&
              (PackedRegPairPacketSize == PM4_PFP_SET_SH_REG_PAIRS_PACKED_N_SIZEDW__GFX11),
              "PAIR_PACKED packet sizes do not match!");
// Maximum number of registers that may be written with a fixed length packed register pair packet.
constexpr uint32 MaxNumPackedFixLengthRegs            = 8;
// Minimum FW version required to use the expanded fixed length range. Prior FW versions only support up to 8 registers.
constexpr uint32 MinExpandedPackedFixLengthPfpVersion = 1463;
constexpr uint32 MaxNumPackedFixLengthRegsExpanded    = 14;
// Minimum number of registers that may be written with a fixed length packed register pair packet.
constexpr uint32 MinNumPackedFixLengthRegs            = 2;
#endif

// GCR_CNTL bit fields for ACQUIRE_MEM and RELEASE_MEM are slightly different.
union Gfx10AcquireMemGcrCntl
{
    struct
    {
        uint32  gliInv     :  2;
        uint32  gl1Range   :  2;
        uint32  glmWb      :  1;
        uint32  glmInv     :  1;
        uint32  glkWb      :  1;
        uint32  glkInv     :  1;
        uint32  glvInv     :  1;
        uint32  gl1Inv     :  1;
        uint32  gl2Us      :  1;
        uint32  gl2Range   :  2;
        uint32  gl2Discard :  1;
        uint32  gl2Inv     :  1;
        uint32  gl2Wb      :  1;
        uint32  seq        :  2;
        uint32  reserved   : 14;
    } bits;

    uint32  u32All;
};

union Gfx10ReleaseMemGcrCntl
{
    struct
    {
        uint32  glmWb      :  1;
        uint32  glmInv     :  1;
        uint32  glvInv     :  1;
        uint32  gl1Inv     :  1;
        uint32  gl2Us      :  1;
        uint32  gl2Range   :  2;
        uint32  gl2Discard :  1;
        uint32  gl2Inv     :  1;
        uint32  gl2Wb      :  1;
        uint32  seq        :  2;
#if PAL_BUILD_GFX11
        uint32  gfx11GlkWb :  1;
#else
        uint32  reserved1  :  1;
#endif
        uint32  reserved   : 19;
    } bits;

    uint32  u32All;
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
    // PFP and ME headers are the same structure...  doesn't really matter which one we use.
    PM4_ME_TYPE_3_HEADER  header = {};

    header.predicate      = predicate;
    header.shaderType     = shaderType;
    header.type           = 3; // type-3 packet
    header.opcode         = opCode;
    header.count          = (count - 2);
    header.resetFilterCam = resetFilterCam;

    return header;
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
// } PM4_PFP_SET_CONTEXT_REG, *PPM4_PFP_SET_CONTEXT_REG;
// This is done with shifts to avoid a read-modify-write of the destination memory.
static uint32 Type3Ordinal2(
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
    m_chipProps(device.Parent()->ChipProperties())
#if PAL_ENABLE_PRINTS_ASSERTS
    , m_verifyShadowedRegisters(device.Parent()->Settings().cmdUtilVerifyShadowedRegRanges)
#endif
{
    const Pal::Device&  parent = *(device.Parent());

    memset(&m_registerInfo, 0, sizeof(m_registerInfo));

    if (m_chipProps.gfxLevel == GfxIpLevel::GfxIp9)
    {
        if ((IsVega10(parent) == false) && (IsRaven(parent) == false))
        {
            m_registerInfo.mmComputeShaderChksum = Gfx09_1x::mmCOMPUTE_SHADER_CHKSUM;

            if (IsVega12(parent) || IsVega20(parent))
            {
                m_registerInfo.mmPaStereoCntl   = Vg12_Vg20::mmPA_STEREO_CNTL;
                m_registerInfo.mmPaStateStereoX = Vg12_Vg20::mmPA_STATE_STEREO_X;
            }
        }

        m_registerInfo.mmRlcPerfmonClkCntl          = Gfx09::mmRLC_PERFMON_CLK_CNTL;
        m_registerInfo.mmRlcSpmGlobalMuxselAddr     = Gfx09::mmRLC_SPM_GLOBAL_MUXSEL_ADDR;
        m_registerInfo.mmRlcSpmGlobalMuxselData     = Gfx09::mmRLC_SPM_GLOBAL_MUXSEL_DATA;
        m_registerInfo.mmRlcSpmSeMuxselAddr         = Gfx09::mmRLC_SPM_SE_MUXSEL_ADDR;
        m_registerInfo.mmRlcSpmSeMuxselData         = Gfx09::mmRLC_SPM_SE_MUXSEL_DATA;
        m_registerInfo.mmSpiShaderPgmLoLs           = Gfx09::mmSPI_SHADER_PGM_LO_LS;
        m_registerInfo.mmSpiShaderPgmLoEs           = Gfx09::mmSPI_SHADER_PGM_LO_ES;
        m_registerInfo.mmVgtGsMaxPrimsPerSubGroup   = Gfx09::mmVGT_GS_MAX_PRIMS_PER_SUBGROUP;
        m_registerInfo.mmDbDfsmControl              = Gfx09::mmDB_DFSM_CONTROL;
        m_registerInfo.mmUserDataStartHsShaderStage = Gfx09::mmSPI_SHADER_USER_DATA_LS_0;
        m_registerInfo.mmUserDataStartGsShaderStage = Gfx09::mmSPI_SHADER_USER_DATA_ES_0;
    }
    else
    {
        m_registerInfo.mmVgtGsMaxPrimsPerSubGroup = Gfx10Plus::mmGE_MAX_OUTPUT_PER_SUBGROUP;
        m_registerInfo.mmComputeShaderChksum      = Gfx10Plus::mmCOMPUTE_SHADER_CHKSUM;
        m_registerInfo.mmPaStereoCntl             = Gfx10Plus::mmPA_STEREO_CNTL;
        m_registerInfo.mmPaStateStereoX           = Gfx10Plus::mmPA_STATE_STEREO_X;

        // GFX10 provides a "PGM_{LO,HI}_ES_GS" and a "PGM_{LO,HI}_LS_HS" register that you would think is
        // what you want to use for the merged shader stages.  You'd be wrong.  According to
        // Those registers are for internal use only.
        m_registerInfo.mmSpiShaderPgmLoLs = Gfx10Plus::mmSPI_SHADER_PGM_LO_LS;
        m_registerInfo.mmSpiShaderPgmLoEs = Gfx10Plus::mmSPI_SHADER_PGM_LO_ES;

        // The "LS" and "ES" user-data registers (that GFX9 utilizes) do exist on GFX10, but they are only
        // meaningful in non-GEN-TWO mode.  We get 32 of these which is what we want.
        m_registerInfo.mmUserDataStartHsShaderStage = Gfx10Plus::mmSPI_SHADER_USER_DATA_HS_0;
        m_registerInfo.mmUserDataStartGsShaderStage = Gfx10Plus::mmSPI_SHADER_USER_DATA_GS_0;

        if (IsGfx10(parent))
        {
            m_registerInfo.mmRlcSpmGlobalMuxselAddr     = Gfx10::mmRLC_SPM_GLOBAL_MUXSEL_ADDR;
            m_registerInfo.mmRlcSpmGlobalMuxselData     = Gfx10::mmRLC_SPM_GLOBAL_MUXSEL_DATA;
            m_registerInfo.mmRlcSpmSeMuxselAddr         = Gfx10::mmRLC_SPM_SE_MUXSEL_ADDR;
            m_registerInfo.mmRlcSpmSeMuxselData         = Gfx10::mmRLC_SPM_SE_MUXSEL_DATA;
            m_registerInfo.mmRlcPerfmonClkCntl          = Gfx10::mmRLC_PERFMON_CLK_CNTL;

            if (IsGfx101(parent))
            {
                m_registerInfo.mmDbDfsmControl          = Gfx10Core::mmDB_DFSM_CONTROL;
            }
            else if (IsGfx103(parent))
            {
                m_registerInfo.mmDbDfsmControl          = Gfx10Core::mmDB_DFSM_CONTROL;
            }
        }
#if PAL_BUILD_GFX11
        else if (IsGfx11(parent))
        {
            m_registerInfo.mmRlcSpmGlobalMuxselAddr = Gfx11::mmRLC_SPM_GLOBAL_MUXSEL_ADDR;
            m_registerInfo.mmRlcSpmGlobalMuxselData = Gfx11::mmRLC_SPM_GLOBAL_MUXSEL_DATA;
            m_registerInfo.mmRlcSpmSeMuxselAddr     = Gfx11::mmRLC_SPM_SE_MUXSEL_ADDR;
            m_registerInfo.mmRlcSpmSeMuxselData     = Gfx11::mmRLC_SPM_SE_MUXSEL_DATA;
        }
#endif
        else
        {
            PAL_ASSERT_ALWAYS();
        }
    }
}

// =====================================================================================================================
// Returns if we can use CS_PARTIAL_FLUSH events on the given engine.
bool CmdUtil::CanUseCsPartialFlush(
    EngineType engineType
    ) const
{
    // There is a CP ucode bug which causes CS_PARTIAL_FLUSH to return early if compute wave save restore (CWSR) is
    // enabled. CWSR was added in gfx8 and the bug was undetected for a few generations. The bug has been fixed in
    // certain versions of the gfx9+ CP ucode. Thus, in the long term we can enable cspf for all ASICs on the gfx9
    // HWL but we still need a fallback if someone runs with old CP ucode.
    bool useCspf = true;

    // We will only try to disable cspf if this is an async compute engine on an ASIC that at some point had the bug.
    if ((Pal::Device::EngineSupportsGraphics(engineType) == false) && (m_chipProps.gfxLevel <= GfxIpLevel::GfxIp10_3))
    {
        if (m_device.Settings().disableAceCsPartialFlush)
        {
            // Always disable ACE support if someone set the debug setting.
            useCspf = false;
        }
        else if (m_chipProps.gfxLevel == GfxIpLevel::GfxIp9)
        {
            // Disable ACE support on gfx9 if the ucode doesn't have the fix.
            constexpr uint32 MinUcodeVerForCsPartialFlushGfx9 = 52;

            useCspf = (m_chipProps.cpUcodeVersion >= MinUcodeVerForCsPartialFlushGfx9);
        }
        else if (m_chipProps.gfxLevel == GfxIpLevel::GfxIp10_1)
        {
            // Disable ACE support on gfx10.1 if the ucode doesn't have the fix.
            constexpr uint32 MinUcodeVerForCsPartialFlushGfx10_1 = 32;

            useCspf = (m_chipProps.cpUcodeVersion >= MinUcodeVerForCsPartialFlushGfx10_1);
        }
        else if (m_chipProps.gfxLevel == GfxIpLevel::GfxIp10_3)
        {
            // Disable ACE support on gfx10.3 if the ucode doesn't have the fix.
            constexpr uint32 MinUcodeVerForCsPartialFlushGfx10_3 = 35;

            useCspf = (m_chipProps.cpUcodeVersion >= MinUcodeVerForCsPartialFlushGfx10_3);
        }
        else
        {
            // Otherwise, assume the bug exists and wasn't fixed.
            useCspf = false;
        }
    }

    return useCspf;
}

// =====================================================================================================================
// If we have support for the indirect_addr index and compute engines.
bool CmdUtil::HasEnhancedLoadShRegIndex() const
{
    bool hasEnhancedLoadShRegIndex = false;
#if PAL_BUILD_GFX11
    if (IsGfx11(m_chipProps.gfxLevel))
    {
        // This function should return true for Gfx11 by default.
        hasEnhancedLoadShRegIndex = true;
    }
    else
#endif
    {
        // This was only implemented on gfx10.3+.
        hasEnhancedLoadShRegIndex =
            ((m_chipProps.cpUcodeVersion >= Gfx103UcodeVersionLoadShRegIndexIndirectAddr) &&
             IsGfx103CorePlus(m_chipProps.gfxLevel));
    }

    return hasEnhancedLoadShRegIndex;
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
// True if the specified register is in context reg space, false otherwise.
bool CmdUtil::IsContextReg(
    uint32 regAddr)
{
    const bool isContextReg = ((regAddr >= CONTEXT_SPACE_START) && (regAddr <= Gfx09_10::CONTEXT_SPACE_END));

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
// If AcquireMem packet supports flush or invalidate requested RB cache sync flags.
bool CmdUtil::CanUseAcquireMem(
    SyncRbFlags rbSync
    ) const
{
    bool canUse = true;

    // Can't flush or invalidate CB metadata using an ACQUIRE_MEM as not supported.
    if (TestAnyFlagSet(rbSync, SyncCbMetaWbInv)
#if PAL_BUILD_GFX11
        // GFX11 doesn't support phase-II RB cache flush.
        || (IsGfx11(m_chipProps.gfxLevel) && (rbSync != 0))
#endif
        )
    {
        canUse = false;
    }

    return canUse;
}

// =====================================================================================================================
// A helper function to translate some of the given CacheSyncFlags into a gfx9 TC cache op. The caller is expected
// to call this function in a loop until the flags mask is set to zero. By studying the code below, we expect that:
// - If you set SyncGl2WbInv, no matter what your other flags are, you only need one cache op.
// - SyncGl2Inv | SyncGlmInv always gets rolled into one op.
// - The worst case flag combination is SyncGl2Wb | SyncGlmInv | SyncGlvInv, which requires three cache ops.
//   Maybe we should consider promoting that to a single SyncGl2WbInv | SyncGlmInv | SyncGlvInv cache op?
// - The cases that require two cache ops are:
//   1. SyncGl2Wb  | SyncGlmInv
//   2. SyncGl2Wb  | SyncGlvInv
//   3. SyncGl2Inv | SyncGlvInv
//   4. SyncGlmInv | SyncGlvInv
static regCP_COHER_CNTL SelectGfx9CacheOp(
    SyncGlxFlags* pGlxFlags)
{
    regCP_COHER_CNTL   cpCoherCntl = {};
    const SyncGlxFlags curFlags    = *pGlxFlags;

    // This function can't handle any flags outside of this set. The caller needs to mask them off first.
    // Note that SyncGl1Inv is always ignored on gfx9 so it's not really an error to pass it into this function.
    PAL_ASSERT(TestAnyFlagSet(curFlags, ~(SyncGl2WbInv | SyncGlmInv | SyncGl1Inv | SyncGlvInv)) == false);

    // Each branch in this function corresponds to one of the special "TC cache op" encodings supported by the CP.
    //
    // The first two cases are shortcuts for flushing and invalidating many caches in one operation. We prefer to use
    // them whenever it wouldn't cause us to sync extra caches as this should reduce the number of releases or acquires
    // we need to send to the CP.
    //
    // Also, note that any request which invalidates the GL2 also invalidates the metadata cache. That's why we
    // ignore the SyncGlmInv flag when selecting between most GL2 cache operations.
    if (TestAllFlagsSet(curFlags, SyncGl2WbInv | SyncGlvInv))
    {
        *pGlxFlags = SyncGlxNone;
        cpCoherCntl.bits.TC_ACTION_ENA    = 1;
        cpCoherCntl.bits.TC_WB_ACTION_ENA = 1;
    }
    else if (TestAllFlagsSet(curFlags, SyncGl2WbInv))
    {
        // We can set this to None because we would have taken the first branch if SyncGlvInv was set.
        *pGlxFlags = SyncGlxNone;
        cpCoherCntl.bits.TC_ACTION_ENA    = 1;
        cpCoherCntl.bits.TC_WB_ACTION_ENA = 1;
        cpCoherCntl.bits.TC_NC_ACTION_ENA = 1;
    }
    else if (TestAnyFlagSet(curFlags, SyncGl2Wb))
    {
        // As above, we can assume SyncGl2Inv is not set. We also need to keep SyncGlmInv as this is the only GL2
        // cache operation that doesn't automatically invalidate it.
        *pGlxFlags &= SyncGlmInv | SyncGlvInv;

        // This assumes PAL will never use the write_confirm MTYPE.
        cpCoherCntl.bits.TC_WB_ACTION_ENA = 1;
        cpCoherCntl.bits.TC_NC_ACTION_ENA = 1;
    }
    else if (TestAnyFlagSet(curFlags, SyncGl2Inv))
    {
        // As above, we can assume SyncGl2Wb is not set.
        *pGlxFlags &= SyncGlvInv;
        cpCoherCntl.bits.TC_ACTION_ENA    = 1;
        cpCoherCntl.bits.TC_NC_ACTION_ENA = 1;
    }
    else if (TestAnyFlagSet(curFlags, SyncGlmInv))
    {
        // If we've gotten here it means none of the other GL2 flags were set, only a SyncGlvInv could left.
        *pGlxFlags &= SyncGlvInv;
        cpCoherCntl.bits.TC_ACTION_ENA              = 1;
        cpCoherCntl.bits.TC_INV_METADATA_ACTION_ENA = 1;
    }
    else if (TestAnyFlagSet(curFlags, SyncGlvInv))
    {
        // If we didn't take any of the other branches this has to be the last flag remaining.
        *pGlxFlags = SyncGlxNone;
        cpCoherCntl.bits.TCL1_ACTION_ENA = 1;
    }

    // We'll loop forever in the caller if this function didn't remove at least one flag from pGlxFlags.
    PAL_ASSERT((curFlags == 0) || (*pGlxFlags != curFlags));

    return cpCoherCntl;
}

// =====================================================================================================================
size_t CmdUtil::BuildAcquireMemGeneric(
    const AcquireMemGeneric& info,
    void*                    pBuffer
    ) const
{
    size_t totalSize;

    if (IsGfx10Plus(m_chipProps.gfxLevel))
    {
        totalSize = BuildAcquireMemInternalGfx10(info, info.engineType, {}, pBuffer);
    }
    else
    {
        totalSize = BuildAcquireMemInternalGfx9(info, info.engineType, {}, pBuffer);
    }

    return totalSize;
}

// =====================================================================================================================
size_t CmdUtil::BuildAcquireMemGfxSurfSync(
    const AcquireMemGfxSurfSync& info,
    void*                        pBuffer
    ) const
{
    size_t totalSize;

    if (IsGfx10Plus(m_chipProps.gfxLevel))
    {
        totalSize = BuildAcquireMemInternalGfx10(info, EngineTypeUniversal, info.flags, pBuffer);
    }
    else
    {
        totalSize = BuildAcquireMemInternalGfx9(info, EngineTypeUniversal, info.flags, pBuffer);
    }

    return totalSize;
}

static_assert(PM4_MEC_ACQUIRE_MEM_SIZEDW__CORE == PM4_ME_ACQUIRE_MEM_SIZEDW__CORE,
              "GFX9: ACQUIRE_MEM packet size is different between ME compute and ME graphics!");

// Mask of CP_ME_COHER_CNTL bits which stall based on all CB base addresses.
static constexpr uint32 CpMeCoherCntlStallCb = CP_ME_COHER_CNTL__CB0_DEST_BASE_ENA_MASK |
                                               CP_ME_COHER_CNTL__CB1_DEST_BASE_ENA_MASK |
                                               CP_ME_COHER_CNTL__CB2_DEST_BASE_ENA_MASK |
                                               CP_ME_COHER_CNTL__CB3_DEST_BASE_ENA_MASK |
                                               CP_ME_COHER_CNTL__CB4_DEST_BASE_ENA_MASK |
                                               CP_ME_COHER_CNTL__CB5_DEST_BASE_ENA_MASK |
                                               CP_ME_COHER_CNTL__CB6_DEST_BASE_ENA_MASK |
                                               CP_ME_COHER_CNTL__CB7_DEST_BASE_ENA_MASK;

// Mask of CP_ME_COHER_CNTL bits which stall based on all DB base addresses (depth and stencil).
static constexpr uint32 CpMeCoherCntlStallDb = CP_ME_COHER_CNTL__DB_DEST_BASE_ENA_MASK |
                                               CP_ME_COHER_CNTL__DEST_BASE_0_ENA_MASK;

// Mask of CP_ME_COHER_CNTL bits which stall based on all base addresses. (CB + DB + unused)
static constexpr uint32 CpMeCoherCntlStallAll = CP_ME_COHER_CNTL__CB0_DEST_BASE_ENA_MASK |
                                                CP_ME_COHER_CNTL__CB1_DEST_BASE_ENA_MASK |
                                                CP_ME_COHER_CNTL__CB2_DEST_BASE_ENA_MASK |
                                                CP_ME_COHER_CNTL__CB3_DEST_BASE_ENA_MASK |
                                                CP_ME_COHER_CNTL__CB4_DEST_BASE_ENA_MASK |
                                                CP_ME_COHER_CNTL__CB5_DEST_BASE_ENA_MASK |
                                                CP_ME_COHER_CNTL__CB6_DEST_BASE_ENA_MASK |
                                                CP_ME_COHER_CNTL__CB7_DEST_BASE_ENA_MASK |
                                                CP_ME_COHER_CNTL__DB_DEST_BASE_ENA_MASK  |
                                                CP_ME_COHER_CNTL__DEST_BASE_0_ENA_MASK   |
                                                CP_ME_COHER_CNTL__DEST_BASE_1_ENA_MASK   |
                                                CP_ME_COHER_CNTL__DEST_BASE_2_ENA_MASK   |
                                                CP_ME_COHER_CNTL__DEST_BASE_3_ENA_MASK;

// =====================================================================================================================
size_t CmdUtil::BuildAcquireMemInternalGfx9(
    const AcquireMemCore& info,
    EngineType            engineType,
    SurfSyncFlags         surfSyncFlags,
    void*                 pBuffer
    ) const
{
    // This path only works on gfx9.
    PAL_ASSERT(IsGfx10Plus(m_chipProps.gfxLevel) == false);

    // The surf sync dest_base stalling feature is only supported on graphics engines. ACE acquires are immediate.
    // The RB caches can only be flushed and invalidated on graphics queues as well. This assert should never fire
    // because the public functions that call this function hard code their arguments such that it will never be false.
    PAL_ASSERT(Pal::Device::EngineSupportsGraphics(engineType) || (surfSyncFlags.u8All == 0));

    size_t totalSize = 0;

    constexpr uint32   PacketSize = PM4_ME_ACQUIRE_MEM_SIZEDW__CORE;
    PM4_ME_ACQUIRE_MEM packet     = {};

    packet.ordinal1.header = Type3Header(IT_ACQUIRE_MEM, PacketSize);

    // The DEST_BASE bits in CP_ME_COHER_CNTL control the surf sync context stalling feature.
    const bool cbStall = surfSyncFlags.cbTargetStall != 0;
    const bool dbStall = surfSyncFlags.dbTargetStall != 0;

    const uint32 cpMeCoherCntl = (cbStall && dbStall) ? CpMeCoherCntlStallAll :
                                 cbStall              ? CpMeCoherCntlStallCb  :
                                 dbStall              ? CpMeCoherCntlStallDb  : 0;

    // Gfx9 doesn't have GCR support. Instead, we have to break the input flags down into one or more supported
    // TC cache ops. To make it easier to share code, we convert our packet-specific flags into CacheSyncFlags.
    // Note that gfx9 has no GL1 cache so we ignore that bit.
    SyncGlxFlags     glxFlags    = info.cacheSync & (SyncGl2WbInv | SyncGlmInv | SyncGlvInv);
    regCP_COHER_CNTL cpCoherCntl = SelectGfx9CacheOp(&glxFlags);

    // Add in the L0 flags that SelectGfx9CacheOp doesn't handle. These flags can be set independently of the TC ops.
    cpCoherCntl.bits.CB_ACTION_ENA           = surfSyncFlags.gfx9Gfx10CbDataWbInv;
    cpCoherCntl.bits.DB_ACTION_ENA           = surfSyncFlags.gfx9Gfx10DbWbInv;
    cpCoherCntl.bits.SH_KCACHE_ACTION_ENA    = TestAnyFlagSet(info.cacheSync, SyncGlkInv);
    cpCoherCntl.bits.SH_ICACHE_ACTION_ENA    = TestAnyFlagSet(info.cacheSync, SyncGliInv);
    cpCoherCntl.bits.SH_KCACHE_WB_ACTION_ENA = TestAnyFlagSet(info.cacheSync, SyncGlkWb);

    // Both COHER_CNTL registers get combined into our packet's coher_cntl field.
    packet.ordinal2.bitfieldsA.coher_cntl = cpCoherCntl.u32All | cpMeCoherCntl;

    // Note that this field isn't used on ACE.
    if (Pal::Device::EngineSupportsGraphics(engineType))
    {
        packet.ordinal2.bitfieldsA.engine_sel = (surfSyncFlags.pfpWait != 0)
                                         ? ME_ACQUIRE_MEM_engine_sel_enum(engine_sel__pfp_acquire_mem__prefetch_parser)
                                         : ME_ACQUIRE_MEM_engine_sel_enum(engine_sel__me_acquire_mem__micro_engine);
    }

    // The coher base and size are in units of 256 bytes. Rather than require the caller to align them to 256 bytes we
    // just expand the base and size to the next 256-byte multiple if they're not already aligned.
    //
    // Note that we're required to set every bit in base to '0' and every bit in size to '1' for a full range acquire.
    // AcquireMemCore requires the caller to use base = 0 and size = 0 for a full range acquire so the math just works
    // for coher_base, but coher_size requires us to substitute a special constant.
    const gpusize coherBase = Pow2AlignDown(info.rangeBase, 256);
    const gpusize padSize   = info.rangeSize + info.rangeBase % 256;
    const gpusize coherSize = (info.rangeSize == 0) ? Pow2AlignDown(UINT64_MAX, 256) : Pow2Align(padSize, 256);

    packet.ordinal3.coher_size                        = Get256BAddrLo(coherSize);
    packet.ordinal4.bitfieldsA.gfx09_10.coher_size_hi = Get256BAddrHi(coherSize);
    packet.ordinal5.coher_base_lo                     = Get256BAddrLo(coherBase);
    packet.ordinal6.bitfieldsA.coher_base_hi          = Get256BAddrHi(coherBase);
    packet.ordinal7.bitfieldsA.poll_interval          = Pal::Device::PollInterval;

    // Write the first acquire_mem. Hopefully we only need this one.
    memcpy(pBuffer, &packet, PacketSize * sizeof(uint32));

    pBuffer = VoidPtrInc(pBuffer, PacketSize * sizeof(uint32));
    totalSize += PacketSize;

    // But if the first SelectGfx9CacheOp call didn't use all of the GCR flags we need more packets. The first packet
    // will handle the I$, K$, and RB caches. These follow-up packets just need to poke the remaining TC cache ops.
    // No more waiting is required, the first packet already did whatever surf-sync waiting was required.
    while (glxFlags != SyncGlxNone)
    {
        const regCP_COHER_CNTL cntl = SelectGfx9CacheOp(&glxFlags);

        packet.ordinal2.bitfieldsA.coher_cntl = cntl.u32All;

        memcpy(pBuffer, &packet, PacketSize * sizeof(uint32));

        pBuffer = VoidPtrInc(pBuffer, PacketSize * sizeof(uint32));
        totalSize += PacketSize;
    }

    return totalSize;
}

// =====================================================================================================================
// A helper heuristic used to program the "range" fields in acquire_mem packets.
static bool UseRangeBasedGcr(
    gpusize base,
    gpusize size)
{
    // The L1 / L2 caches are physical address based. When specifying the range, the GCR will perform virtual address
    // to physical address translation before the wb / inv. If the acquired op is full sync, we must ignore the range,
    // otherwise page fault may occur because page table cannot cover full range virtual address.
    //    When the source address is virtual , the GCR block will have to perform the virtual address to physical
    //    address translation before the wb / inv. Since the pages in memory are a collection of fragments, you can't
    //    specify the full range without walking into a page that has no PTE triggering a fault. In the cases where
    //    the driver wants to wb / inv the entire cache, you should not use range based method, and instead flush the
    //    entire cache without it. The range based method is not meant to be used this way, it is for selective page
    //    invalidation.
    //
    // So that's a good reason to return false if the base or size are the special "full" values. It's also a good idea
    // to disable range-based GCRs if the sync range is too big, as walking a large VA range has a large perf cost.
    return ((base != 0) && (size != 0) && (size <= CmdUtil::Gfx10AcquireMemGl1Gl2RangedCheckMaxSurfaceSizeBytes));
}

static_assert(PM4_MEC_ACQUIRE_MEM_SIZEDW__GFX10PLUS == PM4_ME_ACQUIRE_MEM_SIZEDW__GFX10PLUS,
              "GFX10: ACQUIRE_MEM packet size is different between ME compute and ME graphics!");

// =====================================================================================================================
size_t CmdUtil::BuildAcquireMemInternalGfx10(
    const AcquireMemCore& info,
    EngineType            engineType,
    SurfSyncFlags         surfSyncFlags,
    void*                 pBuffer
    ) const
{
    // This function is named "BuildGfx10..." so don't call it on gfx9.
    PAL_ASSERT(IsGfx10Plus(m_chipProps.gfxLevel));

    // The surf sync dest_base stalling feature is only supported on graphics engines. ACE acquires are immediate.
    // The RB caches can only be flushed and invalidated on graphics queues as well. This assert should never fire
    // because the public functions that call this function hard code their arguments such that it will never be false.
    PAL_ASSERT(Pal::Device::EngineSupportsGraphics(engineType) || (surfSyncFlags.u8All == 0));

    // These are such long names... some temps will help.
    const bool cbDataWbInv = surfSyncFlags.gfx9Gfx10CbDataWbInv != 0;
    const bool dbWbInv     = surfSyncFlags.gfx9Gfx10DbWbInv != 0;

#if PAL_BUILD_GFX11
    // Gfx11 removed support for flushing and invalidating RB caches in an acquire_mem.
    PAL_ASSERT((IsGfx11(m_chipProps.gfxLevel) == false) || ((cbDataWbInv == false) && (dbWbInv == false)));
#endif

    constexpr uint32   PacketSize = PM4_ME_ACQUIRE_MEM_SIZEDW__GFX10PLUS;
    PM4_ME_ACQUIRE_MEM packet     = {};

    packet.ordinal1.header = Type3Header(IT_ACQUIRE_MEM, PacketSize);

    // The DEST_BASE bits in CP_ME_COHER_CNTL control the surf sync context stalling feature.
    const bool cbStall = surfSyncFlags.cbTargetStall != 0;
    const bool dbStall = surfSyncFlags.dbTargetStall != 0;

    const uint32 cpMeCoherCntl = (cbStall && dbStall) ? CpMeCoherCntlStallAll :
                                 cbStall              ? CpMeCoherCntlStallCb  :
                                 dbStall              ? CpMeCoherCntlStallDb  : 0;

    // Note that the other ACTION_ENA flags are not used on gfx10+, they go in the gcr_cntl instead.
    regCP_COHER_CNTL cpCoherCntl = {};
    cpCoherCntl.bits.CB_ACTION_ENA = cbDataWbInv;
    cpCoherCntl.bits.DB_ACTION_ENA = dbWbInv;

    // Both COHER_CNTL registers get combined into our packet's coher_cntl field.
    packet.ordinal2.bitfieldsA.coher_cntl = cpCoherCntl.u32All | cpMeCoherCntl;

    // Note that this field isn't used on ACE.
    if (Pal::Device::EngineSupportsGraphics(engineType))
    {
        packet.ordinal2.bitfieldsA.engine_sel = (surfSyncFlags.pfpWait != 0)
                                         ? ME_ACQUIRE_MEM_engine_sel_enum(engine_sel__pfp_acquire_mem__prefetch_parser)
                                         : ME_ACQUIRE_MEM_engine_sel_enum(engine_sel__me_acquire_mem__micro_engine);
    }

    // The coher base and size are in units of 256 bytes. Rather than require the caller to align them to 256 bytes we
    // just expand the base and size to the next 256-byte multiple if they're not already aligned.
    //
    // Note that we're required to set every bit in base to '0' and every bit in size to '1' for a full range acquire.
    // AcquireMemCore requires the caller to use base = 0 and size = 0 for a full range acquire so the math just works
    // for coher_base, but coher_size requires us to substitute a special constant.
    const gpusize coherBase = Pow2AlignDown(info.rangeBase, 256);
    const gpusize padSize   = info.rangeSize + info.rangeBase % 256;
    const gpusize coherSize = (info.rangeSize == 0) ? Pow2AlignDown(UINT64_MAX, 256) : Pow2Align(padSize, 256);

    packet.ordinal3.coher_size = Get256BAddrLo(coherSize);

#if PAL_BUILD_GFX11
    if (IsGfx11(m_chipProps.gfxLevel))
    {
        packet.ordinal4.bitfieldsA.gfx11.coher_size_hi = Get256BAddrHi(coherSize);
    }
    else
#endif
    {
        packet.ordinal4.bitfieldsA.gfx09_10.coher_size_hi = Get256BAddrHi(coherSize);
    }

    packet.ordinal5.coher_base_lo            = Get256BAddrLo(coherBase);
    packet.ordinal6.bitfieldsA.coher_base_hi = Get256BAddrHi(coherBase);
    packet.ordinal7.bitfieldsA.poll_interval = Pal::Device::PollInterval;

    if (info.cacheSync != 0)
    {
        // Note that glmWb is unimplemented in HW so we don't bother setting it. Everything else we want zeroed.
        //
        // We always prefer parallel cache ops but must force sequential (L0->L1->L2) mode when we're writing back a
        // non-write-through L0 before an L2 writeback.
        Gfx10AcquireMemGcrCntl cntl = {};
        cntl.bits.gliInv = TestAnyFlagSet(info.cacheSync, SyncGliInv);
        cntl.bits.glmInv = TestAnyFlagSet(info.cacheSync, SyncGlmInv);
        cntl.bits.glkWb  = TestAnyFlagSet(info.cacheSync, SyncGlkWb);
        cntl.bits.glkInv = TestAnyFlagSet(info.cacheSync, SyncGlkInv);
        cntl.bits.glvInv = TestAnyFlagSet(info.cacheSync, SyncGlvInv);
        cntl.bits.gl1Inv = TestAnyFlagSet(info.cacheSync, SyncGl1Inv);
        cntl.bits.gl2Inv = TestAnyFlagSet(info.cacheSync, SyncGl2Inv);
        cntl.bits.gl2Wb  = TestAnyFlagSet(info.cacheSync, SyncGl2Wb);
        cntl.bits.seq    = cntl.bits.gl2Wb & cntl.bits.glkWb;

        // We default to whole-cache operations unless this heuristic says we should do a range-based GCR.
        if (UseRangeBasedGcr(info.rangeBase, info.rangeSize))
        {
            cntl.bits.gl1Range = 2;
            cntl.bits.gl2Range = 2;
        }

        packet.ordinal8.bitfields.gfx10Plus.gcr_cntl = cntl.u32All;
    }

    memcpy(pBuffer, &packet, PacketSize * sizeof(uint32));

    return PacketSize;
}

#if PAL_BUILD_GFX11
// =====================================================================================================================
size_t CmdUtil::BuildAcquireMemGfxPws(
    const AcquireMemGfxPws& info,
    void*                   pBuffer
    ) const
{
    // PWS isn't going to work on pre-gfx11 hardware.
    PAL_ASSERT(IsGfx11(m_chipProps.gfxLevel));

    // There are a couple of cases where we need to modify the caller's stage select before applying it.
    ME_ACQUIRE_MEM_pws_stage_sel_enum stageSel = info.stageSel;

    if (m_device.Settings().waForcePrePixShaderWaitPoint &&
        (stageSel == pws_stage_sel__me_acquire_mem__pre_color__HASPWS))
    {
        stageSel = pws_stage_sel__me_acquire_mem__pre_pix_shader__HASPWS;
    }

    // We need to wait at one of the CP stages if we want it to do a GCR after waiting. Rather than force the caller
    // to get this right we just silently handle it. It can't cause any correctness issues, it's just a perf hit.
    if ((info.cacheSync != 0) &&
        (stageSel != pws_stage_sel__me_acquire_mem__cp_me__HASPWS) &&
        (stageSel != pws_stage_sel__me_acquire_mem__cp_pfp__HASPWS))
    {
        stageSel = pws_stage_sel__me_acquire_mem__cp_me__HASPWS;
    }

    constexpr uint32   PacketSize = PM4_ME_ACQUIRE_MEM_SIZEDW__GFX10PLUS;
    PM4_ME_ACQUIRE_MEM packet     = {};

    packet.ordinal1.header                           = Type3Header(IT_ACQUIRE_MEM, PacketSize);
    packet.ordinal2.bitfieldsB.gfx11.pws_stage_sel   = stageSel;
    packet.ordinal2.bitfieldsB.gfx11.pws_counter_sel = info.counterSel;
    packet.ordinal2.bitfieldsB.gfx11.pws_ena2        = pws_ena2__me_acquire_mem__pixel_wait_sync_enable__HASPWS;
    packet.ordinal2.bitfieldsB.gfx11.pws_count       = info.syncCount;

    // The GCR base and size are in units of 128 bytes. Rather than require the caller to align them to 128 bytes we
    // just expand the base and size to the next 128-byte multiple if they're not already aligned.
    //
    // Note that we're required to set every bit in base to '0' and every bit in size to '1' for a full range acquire.
    // AcquireMemCore requires the caller to use base = 0 and size = 0 for a full range acquire so the math just works
    // for gcr_base, but gcr_size requires us to substitute a special constant.
    const gpusize gcrBase = Pow2AlignDown(info.rangeBase, 128);
    const gpusize padSize = info.rangeSize + info.rangeBase % 128;
    const gpusize gcrSize = (info.rangeSize == 0) ? Pow2AlignDown(UINT64_MAX, 128) : Pow2Align(padSize, 128);

    packet.ordinal3.gcr_size                     = Get128BAddrLo(gcrSize);
    packet.ordinal4.bitfieldsB.gfx11.gcr_size_hi = Get128BAddrHi(gcrSize);
    packet.ordinal5.gcr_base_lo                  = Get128BAddrLo(gcrBase);
    packet.ordinal6.bitfieldsB.gfx11.gcr_base_hi = Get128BAddrHi(gcrBase);
    packet.ordinal7.bitfieldsB.gfx11.pws_ena     = pws_ena__me_acquire_mem__pixel_wait_sync_enable__HASPWS;

    if (info.cacheSync != 0)
    {
        // Note that glmWb is unimplemented in HW so we don't bother setting it. Everything else we want zeroed.
        //
        // We always prefer parallel cache ops but must force sequential (L0->L1->L2) mode when we're writing back a
        // non-write-through L0 before an L2 writeback. The only writeable L0 that a PWS acquire can flush is the K$.
        Gfx10AcquireMemGcrCntl cntl = {};
        cntl.bits.gliInv = TestAnyFlagSet(info.cacheSync, SyncGliInv);
        cntl.bits.glmInv = TestAnyFlagSet(info.cacheSync, SyncGlmInv);
        cntl.bits.glkWb  = TestAnyFlagSet(info.cacheSync, SyncGlkWb);
        cntl.bits.glkInv = TestAnyFlagSet(info.cacheSync, SyncGlkInv);
        cntl.bits.glvInv = TestAnyFlagSet(info.cacheSync, SyncGlvInv);
        cntl.bits.gl1Inv = TestAnyFlagSet(info.cacheSync, SyncGl1Inv);
        cntl.bits.gl2Inv = TestAnyFlagSet(info.cacheSync, SyncGl2Inv);
        cntl.bits.gl2Wb  = TestAnyFlagSet(info.cacheSync, SyncGl2Wb);
        cntl.bits.seq    = cntl.bits.gl2Wb & cntl.bits.glkWb;

        // We default to whole-cache operations unless this heuristic says we should do a range-based GCR.
        if (UseRangeBasedGcr(info.rangeBase, info.rangeSize))
        {
            cntl.bits.gl1Range = 2;
            cntl.bits.gl2Range = 2;
        }

        packet.ordinal8.bitfields.gfx10Plus.gcr_cntl = cntl.u32All;
    }

    memcpy(pBuffer, &packet, PacketSize * sizeof(uint32));

    return PacketSize;
}
#endif

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
    static_assert(PM4_ME_ATOMIC_MEM_SIZEDW__CORE == PM4_MEC_ATOMIC_MEM_SIZEDW__CORE,
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

    static_assert(((static_cast<uint32>(cache_policy__me_atomic_mem__noa__GFX10PLUS)    ==
                    static_cast<uint32>(cache_policy__mec_atomic_mem__noa__GFX10PLUS))  &&
                   (static_cast<uint32>(cache_policy__me_atomic_mem__bypass__GFX10PLUS) ==
                    static_cast<uint32>(cache_policy__mec_atomic_mem__bypass__GFX10PLUS))),
                  "Atomic Mem cache policy enum is different between ME and MEC!");

    // The destination address must be aligned to the size of the operands.
    PAL_ASSERT((dstMemAddr != 0) && IsPow2Aligned(dstMemAddr, (Is32BitAtomicOp(atomicOp) ? 4 : 8)));

    constexpr uint32 PacketSize = PM4_ME_ATOMIC_MEM_SIZEDW__CORE;
    PM4_ME_ATOMIC_MEM packet = {};

    packet.ordinal1.header                 = Type3Header(IT_ATOMIC_MEM, PacketSize);
    packet.ordinal2.bitfields.atomic       = AtomicOpConversionTable[static_cast<uint32>(atomicOp)];
    packet.ordinal2.bitfields.command      = command__me_atomic_mem__single_pass_atomic;
    packet.ordinal2.bitfields.cache_policy = cache_policy__me_atomic_mem__lru;
    packet.ordinal3.addr_lo                = LowPart(dstMemAddr);
    packet.ordinal4.addr_hi                = HighPart(dstMemAddr);
    packet.ordinal5.src_data_lo            = LowPart(srcData);
    packet.ordinal6.src_data_hi            = HighPart(srcData);

    static_assert(PacketSize * sizeof(uint32) == sizeof(packet), "");
    memcpy(pBuffer, &packet, sizeof(packet));
    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 packet which issues a clear state command. Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildClearState(
    PFP_CLEAR_STATE_cmd_enum command,
    void*                    pBuffer) // [out] Build the PM4 packet in this buffer.
{
    static_assert(PM4_PFP_CLEAR_STATE_SIZEDW__HASCLEARSTATE == PM4_ME_CLEAR_STATE_SIZEDW__HASCLEARSTATE,
                  "Clear state packets don't match between PFP and ME!");

    constexpr uint32 PacketSize = PM4_PFP_CLEAR_STATE_SIZEDW__HASCLEARSTATE;
    PM4_PFP_CLEAR_STATE packet = {};

    packet.ordinal1.header.u32All = (Type3Header(IT_CLEAR_STATE__HASCLEARSTATE, PacketSize)).u32All;
    packet.ordinal2.bitfields.hasClearState.cmd = command;

    static_assert(PacketSize * sizeof(uint32) == sizeof(packet), "");
    memcpy(pBuffer, &packet, sizeof(packet));
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

    constexpr uint32    PacketSize = PM4_PFP_COND_INDIRECT_BUFFER_SIZEDW__CORE;
    PM4_PFP_COND_INDIRECT_BUFFER packet = {};

    // There is no separate op-code for conditional indirect buffers.  The CP figures it out
    const IT_OpCodeType opCode     = constantEngine ? IT_INDIRECT_BUFFER_CNST : IT_INDIRECT_BUFFER;

    packet.ordinal1.header.u32All      = (Type3Header(opCode, PacketSize)).u32All;
    packet.ordinal2.bitfields.function = FuncTranslation[static_cast<uint32>(compareFunc)];

    // We always implement both a "then" and an "else" clause
    packet.ordinal2.bitfields.mode = mode__pfp_cond_indirect_buffer__if_then_else;

    // Make sure our comparison address is aligned properly
    packet.ordinal3.u32All          = LowPart(compareGpuAddr);
    packet.ordinal4.compare_addr_hi = HighPart(compareGpuAddr);
    PAL_ASSERT(packet.ordinal3.bitfields.reserved1 == 0);

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
// Builds a CONTEXT_CONTROL packet with both load and shadowing disabled.  Returns the size, in DWORDs, of the
// generated packet.
size_t CmdUtil::BuildContextControl(
    const PM4_PFP_CONTEXT_CONTROL& contextControl,
    void*                          pBuffer)   // [out] Build the PM4 packet in this buffer.
{
    static_assert(PM4_PFP_CONTEXT_CONTROL_SIZEDW__CORE == PM4_ME_CONTEXT_CONTROL_SIZEDW__CORE,
                  "Context control packet doesn't match between PFP and ME!");

    constexpr uint32 PacketSize = PM4_PFP_CONTEXT_CONTROL_SIZEDW__CORE;
    auto*const       pPacket    = static_cast<PM4_PFP_CONTEXT_CONTROL*>(pBuffer);

    pPacket->ordinal1.header.u32All = (Type3Header(IT_CONTEXT_CONTROL, PacketSize)).u32All;
    pPacket->ordinal2.u32All        = contextControl.ordinal2.u32All;
    pPacket->ordinal3.u32All        = contextControl.ordinal3.u32All;

    return PacketSize;
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
    void*      pBuffer    // [out] Build the PM4 packet in this buffer.
    ) const
{
    static_assert(PM4_ME_COPY_DATA_SIZEDW__CORE == PM4_MEC_COPY_DATA_SIZEDW__CORE,
                  "CopyData packet size is different between ME and MEC!");

    static_assert(((static_cast<uint32>(src_sel__mec_copy_data__mem_mapped_register)           ==
                    static_cast<uint32>(src_sel__me_copy_data__mem_mapped_register))           &&
                   (static_cast<uint32>(src_sel__mec_copy_data__memory__GFX09)                 ==
                    static_cast<uint32>(src_sel__me_copy_data__memory__GFX09))                 &&
                   (static_cast<uint32>(src_sel__mec_copy_data__tc_l2)                         ==
                    static_cast<uint32>(src_sel__me_copy_data__tc_l2))                         &&
                   (static_cast<uint32>(src_sel__mec_copy_data__gds__CORE)                     ==
                    static_cast<uint32>(src_sel__me_copy_data__gds__CORE))                     &&
                   (static_cast<uint32>(src_sel__mec_copy_data__perfcounters)                  ==
                    static_cast<uint32>(src_sel__me_copy_data__perfcounters))                  &&
                   (static_cast<uint32>(src_sel__mec_copy_data__immediate_data)                ==
                    static_cast<uint32>(src_sel__me_copy_data__immediate_data))                &&
                   (static_cast<uint32>(src_sel__mec_copy_data__atomic_return_data)            ==
                    static_cast<uint32>(src_sel__me_copy_data__atomic_return_data))            &&
                   (static_cast<uint32>(src_sel__mec_copy_data__gds_atomic_return_data0__CORE) ==
                    static_cast<uint32>(src_sel__me_copy_data__gds_atomic_return_data0__CORE)) &&
                   (static_cast<uint32>(src_sel__mec_copy_data__gds_atomic_return_data1__CORE) ==
                    static_cast<uint32>(src_sel__me_copy_data__gds_atomic_return_data1__CORE)) &&
                   (static_cast<uint32>(src_sel__mec_copy_data__gpu_clock_count)               ==
                    static_cast<uint32>(src_sel__me_copy_data__gpu_clock_count))),
                  "CopyData srcSel enum is different between ME and MEC!");

    static_assert(((static_cast<uint32>(dst_sel__mec_copy_data__mem_mapped_register) ==
                    static_cast<uint32>(dst_sel__me_copy_data__mem_mapped_register)) &&
                   (static_cast<uint32>(dst_sel__mec_copy_data__tc_l2)               ==
                    static_cast<uint32>(dst_sel__me_copy_data__tc_l2))               &&
                   (static_cast<uint32>(dst_sel__mec_copy_data__gds__CORE)           ==
                    static_cast<uint32>(dst_sel__me_copy_data__gds__CORE))           &&
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

    static_assert((static_cast<uint32>(src_sel__pfp_copy_data__tc_l2_obsolete__GFX10PLUS) ==
                   static_cast<uint32>(src_sel__pfp_copy_data__memory__GFX09)),
                  "CopyData memory destination enumerations have changed between GFX9 and GFX10");

    static_assert((static_cast<uint32>(dst_sel__pfp_copy_data__tc_l2_obsolete__GFX10PLUS) ==
                   static_cast<uint32>(dst_sel__pfp_copy_data__memory__GFX09)),
                  "CopyData memory destination enumerations have changed between GFX9 and GFX10");

    constexpr uint32  PacketSize     = PM4_ME_COPY_DATA_SIZEDW__CORE;
    PM4_ME_COPY_DATA   packetGfx;
    PM4_MEC_COPY_DATA* pPacketCompute = reinterpret_cast<PM4_MEC_COPY_DATA*>(&packetGfx);
    const bool        gfxSupported   = Pal::Device::EngineSupportsGraphics(engineType);
    const bool        isCompute      = (engineType == EngineTypeCompute);

    packetGfx.ordinal1.header        = Type3Header(IT_COPY_DATA, PacketSize);
    packetGfx.ordinal2.u32All        = 0;
    packetGfx.ordinal3.u32All        = 0;
    packetGfx.ordinal4.u32All        = 0;
    packetGfx.ordinal5.u32All        = 0;

    packetGfx.ordinal2.bitfields.src_sel    = static_cast<ME_COPY_DATA_src_sel_enum>(srcSel);
    packetGfx.ordinal2.bitfields.dst_sel    = static_cast<ME_COPY_DATA_dst_sel_enum>(dstSel);
    packetGfx.ordinal2.bitfields.count_sel  = static_cast<ME_COPY_DATA_count_sel_enum>(countSel);
    packetGfx.ordinal2.bitfields.wr_confirm = static_cast<ME_COPY_DATA_wr_confirm_enum>(wrConfirm);

    if (isCompute)
    {
        // Set these to their "zero" equivalents...  Enumerating these here explicitly to provide reminders that these
        // fields do exist.
        pPacketCompute->ordinal2.bitfields.src_cache_policy = src_cache_policy__mec_copy_data__lru;
        pPacketCompute->ordinal2.bitfields.dst_cache_policy = dst_cache_policy__mec_copy_data__lru;
        pPacketCompute->ordinal2.bitfields.pq_exe_status    = pq_exe_status__mec_copy_data__default;
    }
    else
    {
        PAL_ASSERT(Pal::Device::EngineSupportsGraphics(engineType));

        // Set these to their "zero" equivalents...  Enumerating these here explicitly to provide reminders that these
        // fields do exist.
        packetGfx.ordinal2.bitfields.src_cache_policy = src_cache_policy__me_copy_data__lru;
        packetGfx.ordinal2.bitfields.dst_cache_policy = dst_cache_policy__me_copy_data__lru;
        packetGfx.ordinal2.bitfields.engine_sel       = static_cast<ME_COPY_DATA_engine_sel_enum>(engineSel);
    }

    switch (srcSel)
    {
    case src_sel__me_copy_data__perfcounters:
    case src_sel__me_copy_data__mem_mapped_register:
        packetGfx.ordinal3.u32All = LowPart(srcAddr);

        // Make sure we didn't get an illegal register offset
        PAL_ASSERT(CanUseCopyDataRegOffset(srcAddr));
        PAL_ASSERT((gfxSupported && (packetGfx.ordinal3.bitfieldsA.reserved1 == 0)) ||
                   (isCompute    && (pPacketCompute->ordinal3.bitfieldsA.reserved1 == 0)));
        break;

    case src_sel__me_copy_data__immediate_data:
        packetGfx.ordinal3.imm_data = LowPart(srcAddr);

        // Really only meaningful if countSel==count_sel__me_copy_data__64_bits_of_data, but shouldn't hurt to
        // write it regardless.
        packetGfx.ordinal4.src_imm_data = HighPart(srcAddr);
        break;

    case src_sel__me_copy_data__memory__GFX09:
    case src_sel__me_copy_data__tc_l2:
        packetGfx.ordinal3.u32All            = LowPart(srcAddr);
        packetGfx.ordinal4.src_memtc_addr_hi = HighPart(srcAddr);

        // Make sure our srcAddr is properly aligned.  The alignment differs based on how much data is being written
        PAL_ASSERT(((countSel == count_sel__mec_copy_data__64_bits_of_data) &&
                    ((isCompute    && (pPacketCompute->ordinal3.bitfieldsC.reserved3 == 0)) ||
                     (gfxSupported && (packetGfx.ordinal3.bitfieldsC.reserved3 == 0)))) ||
                   ((countSel == count_sel__mec_copy_data__32_bits_of_data) &&
                    ((isCompute    && (pPacketCompute->ordinal3.bitfieldsB.reserved2 == 0))  ||
                     (gfxSupported && (packetGfx.ordinal3.bitfieldsB.reserved2 == 0)))));
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
        packetGfx.ordinal5.u32All = LowPart(dstAddr);

        // Make sure we didn't get an illegal register offset.
        PAL_ASSERT(CanUseCopyDataRegOffset(dstAddr));
        PAL_ASSERT((isCompute    && (pPacketCompute->ordinal5.bitfieldsA.reserved1 == 0)) ||
                   (gfxSupported && (packetGfx.ordinal5.bitfieldsA.reserved1 == 0)));
        break;

    case dst_sel__me_copy_data__memory_sync_across_grbm:
        // sync memory destination is only available with ME engine on universal queue
        PAL_ASSERT(gfxSupported && (engineSel == engine_sel__me_copy_data__micro_engine));
        // break intentionally left out

    case dst_sel__me_copy_data__memory__GFX09:
    case dst_sel__me_copy_data__tc_l2:
        packetGfx.ordinal5.u32All      = LowPart(dstAddr);
        packetGfx.ordinal6.dst_addr_hi = HighPart(dstAddr);

        // Make sure our dstAddr is properly aligned.  The alignment differs based on how much data is being written
        PAL_ASSERT(((countSel == count_sel__mec_copy_data__64_bits_of_data) &&
                    ((isCompute    && (pPacketCompute->ordinal5.bitfieldsC.reserved3 == 0)) ||
                     (gfxSupported && (packetGfx.ordinal5.bitfieldsC.reserved3 == 0)))) ||
                   ((countSel == count_sel__mec_copy_data__32_bits_of_data) &&
                    ((isCompute    && (pPacketCompute->ordinal5.bitfieldsB.reserved2 == 0)) ||
                     (gfxSupported && (packetGfx.ordinal5.bitfieldsB.reserved2 == 0)))));
        break;

    case dst_sel__me_copy_data__gds__CORE:
        packetGfx.ordinal5.u32All = LowPart(dstAddr);
        PAL_ASSERT((isCompute    && (pPacketCompute->ordinal5.bitfieldsD.core.reserved4 == 0)) ||
                   (gfxSupported && (packetGfx.ordinal5.bitfieldsD.core.reserved4 == 0)));
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
// Builds a PERFMON_CONTROL packet. Returns the size of the PM4 command assembled, in DWORDs.
// This packet is to control Data Fabric (DF) perfmon events by writing the PerfMonCtlLo/Hi registers.
size_t CmdUtil::BuildPerfmonControl(
    uint32     perfMonCtlId,  // PerfMonCtl id to be configured (0-7)
    bool       enable,        // Perfmon enabling: 0=disable, 1=enable
    uint32     eventSelect,   // If enabling, the event selection to configure for this perfMonId
    uint32     eventUnitMask, // If enabling, this is event specific configuration data.
    void*      pBuffer)       // [out] Build the PM4 packet in this buffer.
{
    constexpr uint32       PacketSize = PM4_ME_PERFMON_CONTROL_SIZEDW__GFX103COREPLUS;
    PM4_ME_PERFMON_CONTROL packetGfx;

    packetGfx.ordinal1.header        = Type3Header(IT_PERFMON_CONTROL__GFX103COREPLUS, PacketSize);

    packetGfx.ordinal2.u32All                                 = 0;
    packetGfx.ordinal2.bitfields.gfx103CorePlus.pmc_id        = perfMonCtlId;
    packetGfx.ordinal2.bitfields.gfx103CorePlus.pmc_en        = static_cast<ME_PERFMON_CONTROL_pmc_en_enum>(enable);
    packetGfx.ordinal2.bitfields.gfx103CorePlus.pmc_unit_mask = eventUnitMask;
    packetGfx.ordinal3.u32All                                 = 0;
    packetGfx.ordinal3.bitfields.gfx103CorePlus.pmc_event     = eventSelect;

    memcpy(pBuffer, &packetGfx, PacketSize * sizeof(uint32));
    return PacketSize;
}

// =====================================================================================================================
// Builds a DISPATCH_DIRECT packet. Returns the size of the PM4 command assembled, in DWORDs.
template <bool dimInThreads, bool forceStartAt000>
size_t CmdUtil::BuildDispatchDirect(
    DispatchDims size,                  // Thread groups (or threads) to launch.
    Pm4Predicate predicate,             // Predication enable control. Must be PredDisable on the Compute Engine.
    bool         isWave32,              // Meaningful for GFX10 only, set if wave-size is 32 for bound compute shader
    bool         useTunneling,          // Meaningful for GFX10 only, set if dispatch tunneling should be used (VR)
    bool         disablePartialPreempt, // Avoid preemption at thread group level without CWSR. Only affects GFX10.
    void*        pBuffer                // [out] Build the PM4 packet in this buffer.
    ) const
{
    regCOMPUTE_DISPATCH_INITIATOR dispatchInitiator;
    dispatchInitiator.u32All                     = 0;
    dispatchInitiator.bits.COMPUTE_SHADER_EN     = 1;
    dispatchInitiator.bits.FORCE_START_AT_000    = forceStartAt000;
    dispatchInitiator.bits.USE_THREAD_DIMENSIONS = dimInThreads;
    dispatchInitiator.gfx10Plus.CS_W32_EN        = isWave32;
    if (IsGfx10Plus(m_chipProps.gfxLevel))
    {
        dispatchInitiator.gfx10Plus.TUNNEL_ENABLE = useTunneling;
    }
    if (disablePartialPreempt)
    {
        dispatchInitiator.u32All |= ComputeDispatchInitiatorDisablePartialPreemptMask;
    }

    // Set unordered mode to allow waves launch faster. This bit is related to the QoS (Quality of service) feature and
    // should be safe to set by default as the feature gets enabled only when allowed by the KMD. This bit also only
    // applies to asynchronous compute pipe and the graphics pipe simply ignores it.
    dispatchInitiator.bits.ORDER_MODE = 1;

    static_assert(PM4_MEC_DISPATCH_DIRECT_SIZEDW__CORE == PM4_ME_DISPATCH_DIRECT_SIZEDW__CORE,
                  "MEC_DISPATCH_DIRECT packet definition has been updated, fix this!");

    constexpr uint32 PacketSize = PM4_ME_DISPATCH_DIRECT_SIZEDW__CORE;
    auto*const       pPacket    = static_cast<PM4_MEC_DISPATCH_DIRECT*>(pBuffer);

    pPacket->ordinal1.header.u32All      = (Type3Header(IT_DISPATCH_DIRECT, PacketSize,
                                                        false, ShaderCompute, predicate)).u32All;
    pPacket->ordinal2.dim_x              = size.x;
    pPacket->ordinal3.dim_y              = size.y;
    pPacket->ordinal4.dim_z              = size.z;
    pPacket->ordinal5.dispatch_initiator = dispatchInitiator.u32All;

    return PacketSize;
}

template
size_t CmdUtil::BuildDispatchDirect<true, true>(
    DispatchDims size,
    Pm4Predicate predicate,
    bool         isWave32,
    bool         useTunneling,
    bool         disablePartialPreempt,
    void*        pBuffer) const;
template
size_t CmdUtil::BuildDispatchDirect<false, false>(
    DispatchDims size,
    Pm4Predicate predicate,
    bool         isWave32,
    bool         useTunneling,
    bool         disablePartialPreempt,
    void*        pBuffer) const;
template
size_t CmdUtil::BuildDispatchDirect<false, true>(
    DispatchDims size,
    Pm4Predicate predicate,
    bool         isWave32,
    bool         useTunneling,
    bool         disablePartialPreempt,
    void*        pBuffer) const;

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
    dispatchInitiator.gfx10Plus.CS_W32_EN      = isWave32;

    constexpr uint32 PacketSize = PM4_ME_DISPATCH_INDIRECT_SIZEDW__CORE;
    auto*const       pPacket    = static_cast<PM4_ME_DISPATCH_INDIRECT*>(pBuffer);

    pPacket->ordinal1.header             =
        Type3Header(IT_DISPATCH_INDIRECT, PacketSize, false, ShaderCompute, predicate);
    pPacket->ordinal2.data_offset        = LowPart(byteOffset);
    pPacket->ordinal3.dispatch_initiator = dispatchInitiator.u32All;

    return PacketSize;
}

// =====================================================================================================================
// Builds execute indirect packet for the GFX engine. Returns the size of the PM4 command assembled, in DWORDs.
// This function only supports Graphics Queue usage.
size_t CmdUtil::BuildExecuteIndirect(
    Pm4Predicate                     predicate,
    const bool                       isGfx,
    const ExecuteIndirectPacketInfo& packetInfo,
    const bool                       resetPktFilter,
    void*                            pBuffer)       // [out] Build the PM4 packet in this buffer.
{
    constexpr uint32 PacketSize                         = PM4_PFP_EXECUTE_INDIRECT_SIZEDW__CORE;
    const GraphicsPipelineSignature* pGraphicsSignature = packetInfo.pipelineSignature.pSignatureGfx;
    const ComputePipelineSignature*  pComputeSignature  = packetInfo.pipelineSignature.pSignatureCs;
    PM4_PFP_EXECUTE_INDIRECT  packet                    = {};

    packet.ordinal1.header.u32All =
        (Type3Header(IT_EXECUTE_INDIRECT__EXECINDIRECT, PacketSize, resetPktFilter, ShaderGraphics, predicate)).u32All;
    packet.ordinal2.bitfields.core.cmd_base_lo           = LowPart(packetInfo.commandBufferAddr) >> 2;
    packet.ordinal3.cmd_base_hi                          = HighPart(packetInfo.commandBufferAddr);
    packet.ordinal4.bitfields.core.count_indirect_enable = (packetInfo.countBufferAddr != 0);
    packet.ordinal4.bitfields.core.ib_size               = packetInfo.commandBufferSizeDwords;
    packet.ordinal5.max_count                            = packetInfo.maxCount;
    packet.ordinal6.bitfields.core.count_addr_lo         = LowPart(packetInfo.countBufferAddr) >> 2;
    packet.ordinal7.count_addr_hi                        = HighPart(packetInfo.countBufferAddr);
    packet.ordinal8.stride                               = packetInfo.argumentBufferStrideBytes;
    packet.ordinal9.data_addr_lo                         = LowPart(packetInfo.argumentBufferAddr);
    packet.ordinal10.bitfields.core.data_addr_hi         = HighPart(packetInfo.argumentBufferAddr);
    packet.ordinal10.bitfields.core.spill_table_stride   = packetInfo.spillTableStrideBytes;
    packet.ordinal11.spill_table_addr_lo                 = LowPart(packetInfo.spillTableAddr);
    packet.ordinal12.bitfields.core.spill_table_addr_hi  = HighPart(packetInfo.spillTableAddr);
    if (packetInfo.spillTableAddr != 0)
    {
        if (isGfx)
        {
            packet.ordinal12.bitfields.core.spill_table_reg_offset0 =
                ShRegOffset(pGraphicsSignature->stage[0].spillTableRegAddr);
            packet.ordinal13.bitfields.core.spill_table_reg_offset1 =
                ShRegOffset(pGraphicsSignature->stage[1].spillTableRegAddr);
            packet.ordinal13.bitfields.core.spill_table_reg_offset2 =
                ShRegOffset(pGraphicsSignature->stage[2].spillTableRegAddr);
            packet.ordinal14.bitfields.core.spill_table_reg_offset3 =
                ShRegOffset(pGraphicsSignature->stage[3].spillTableRegAddr);
        }
        else
        {
            packet.ordinal12.bitfields.core.spill_table_reg_offset0 =
                ShRegOffset(pComputeSignature->stage.spillTableRegAddr);
        }
        packet.ordinal14.bitfields.core.spill_table_instance_count = packetInfo.spillTableInstanceCnt;
    }
    packet.ordinal15.bitfields.core.vb_table_reg_offset = ShRegOffset(packetInfo.vbTableRegOffset);
    packet.ordinal15.bitfields.core.vb_table_size       = packetInfo.vbTableSize;

    memcpy(pBuffer, &packet, PacketSize * sizeof(uint32));
    return PacketSize;
}

// =====================================================================================================================
// Builds a DISPATCH_INDIRECT packet for the MEC. Returns the size of the PM4 command assembled, in DWORDs.
// This packet has different sizes between ME compute and ME gfx.
size_t CmdUtil::BuildDispatchIndirectMec(
    gpusize       address,      // Address of the indirect args data.
    bool          isWave32,     // Meaningful for GFX10 only, set if wave-size is 32 for bound compute shader
    bool          useTunneling, // Meaningful for GFX10 only, set if dispatch tunneling should be used (VR)
    bool          disablePartialPreempt, // Avoid preemption at thread group level without CWSR. Only affects GFX10.
    void*         pBuffer                // [out] Build the PM4 packet in this buffer.
    ) const
{
    // Address must be 32-bit aligned
    PAL_ASSERT ((address & 0x3) == 0);

    constexpr uint32               PacketSize        = PM4_MEC_DISPATCH_INDIRECT_SIZEDW__CORE;
    auto*const                     pPacket           = static_cast<PM4_MEC_DISPATCH_INDIRECT*>(pBuffer);
    regCOMPUTE_DISPATCH_INITIATOR  dispatchInitiator = {};

    dispatchInitiator.u32All                   = 0;
    dispatchInitiator.bits.COMPUTE_SHADER_EN   = 1;
    dispatchInitiator.bits.FORCE_START_AT_000  = 1;
    dispatchInitiator.bits.ORDER_MODE          = 1;
    dispatchInitiator.gfx10Plus.CS_W32_EN      = isWave32;
    if (IsGfx10Plus(m_chipProps.gfxLevel))
    {
        dispatchInitiator.gfx10Plus.TUNNEL_ENABLE = useTunneling;
    }
    if (disablePartialPreempt)
    {
        dispatchInitiator.u32All |= ComputeDispatchInitiatorDisablePartialPreemptMask;
    }

    pPacket->ordinal1.header.u32All      = (Type3Header(IT_DISPATCH_INDIRECT, PacketSize)).u32All;
    pPacket->ordinal2.addr_lo            = LowPart(address);
    pPacket->ordinal3.addr_hi            = HighPart(address);
    pPacket->ordinal4.dispatch_initiator = dispatchInitiator.u32All;

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
    constexpr uint32 PacketSize = PM4_PFP_DRAW_INDEX_2_SIZEDW__CORE;
    auto*const       pPacket    = static_cast<PM4_PFP_DRAW_INDEX_2*>(pBuffer);

    pPacket->ordinal1.header.u32All = (Type3Header(IT_DRAW_INDEX_2, PacketSize,
                                                   false, ShaderGraphics, predicate)).u32All;
    pPacket->ordinal2.max_size      = indexBufSize;
    pPacket->ordinal3.index_base_lo = LowPart(indexBufAddr);
    pPacket->ordinal4.index_base_hi = HighPart(indexBufAddr);
    pPacket->ordinal5.index_count   = indexCount;

    regVGT_DRAW_INITIATOR drawInitiator;
    drawInitiator.u32All                = 0;
    drawInitiator.bits.SOURCE_SELECT    = DI_SRC_SEL_DMA;
    drawInitiator.bits.MAJOR_MODE       = DI_MAJOR_MODE_0;

    pPacket->ordinal6.draw_initiator = drawInitiator.u32All;
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
    constexpr uint32 PacketSize = PM4_PFP_DRAW_INDEX_OFFSET_2_SIZEDW__CORE;
    auto*const       pPacket    = static_cast<PM4_PFP_DRAW_INDEX_OFFSET_2*>(pBuffer);

    pPacket->ordinal1.header.u32All = (Type3Header(IT_DRAW_INDEX_OFFSET_2, PacketSize,
                                                   false, ShaderGraphics, predicate)).u32All;
    pPacket->ordinal2.max_size      = indexBufSize;
    pPacket->ordinal3.index_offset  = indexOffset;
    pPacket->ordinal4.index_count   = indexCount;

    regVGT_DRAW_INITIATOR drawInitiator;
    drawInitiator.u32All                = 0;
    drawInitiator.bits.SOURCE_SELECT    = DI_SRC_SEL_DMA;
    drawInitiator.bits.MAJOR_MODE       = DI_MAJOR_MODE_0;

    pPacket->ordinal5.draw_initiator = drawInitiator.u32All;
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

    constexpr uint32 PacketSize = PM4_PFP_DRAW_INDEX_AUTO_SIZEDW__CORE;
    auto*const       pPacket    = static_cast<PM4_PFP_DRAW_INDEX_AUTO*>(pBuffer);

    pPacket->ordinal1.header.u32All = (Type3Header(IT_DRAW_INDEX_AUTO, PacketSize,
                                                   false, ShaderGraphics, predicate)).u32All;
    pPacket->ordinal2.index_count   = indexCount;

    regVGT_DRAW_INITIATOR drawInitiator;
    drawInitiator.u32All                = 0;
    drawInitiator.bits.SOURCE_SELECT    = DI_SRC_SEL_AUTO_INDEX;
    drawInitiator.bits.MAJOR_MODE       = DI_MAJOR_MODE_0;
    drawInitiator.bits.USE_OPAQUE       = useOpaque ? 1 : 0;

    pPacket->ordinal3.draw_initiator = drawInitiator.u32All;
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
    void*        pBuffer        // [out] Build the PM4 packet in this buffer.
) const
{
    // Draw argument offset in the buffer has to be 4-byte aligned.
    PAL_ASSERT(IsPow2Aligned(offset, 4));

    constexpr size_t PacketSize = PM4_PFP_DRAW_INDIRECT_SIZEDW__CORE;
    PM4_PFP_DRAW_INDIRECT packet = {};

    packet.ordinal1.header.u32All =
        (Type3Header(IT_DRAW_INDIRECT, PacketSize, false, ShaderGraphics, predicate)).u32All;
    packet.ordinal2.data_offset = LowPart(offset);
    packet.ordinal3.bitfields.start_vtx_loc = baseVtxLoc - PERSISTENT_SPACE_START;

    packet.ordinal4.bitfields.start_inst_loc = startInstLoc - PERSISTENT_SPACE_START;

    auto*const pDrawInitiator = reinterpret_cast<regVGT_DRAW_INITIATOR *>(&packet.ordinal5.u32All);
    pDrawInitiator->bits.SOURCE_SELECT = DI_SRC_SEL_AUTO_INDEX;
    pDrawInitiator->bits.MAJOR_MODE = DI_MAJOR_MODE_0;

    static_assert(PacketSize * sizeof(uint32) == sizeof(packet), "");
    memcpy(pBuffer, &packet, sizeof(packet));
    return PacketSize;
}

// =====================================================================================================================
// Returns the size needed by BuildDrawIndexIndirect in DWORDs.
uint32 CmdUtil::DrawIndexIndirectSize() const
{
    uint32 packetSize = PM4_PFP_DRAW_INDEX_INDIRECT_SIZEDW__CORE;

    return packetSize;
}

// =====================================================================================================================
// Builds a PM4 packet which issues a multi indexed, indirect draw command into the given DE command stream. Returns the
// size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildDrawIndexIndirect(
    gpusize      offset,        // Byte offset to the indirect args data.
    uint32       baseVtxLoc,    // Register VS expects to read baseVtxLoc from.
    uint32       startInstLoc,  // Register VS expects to read startInstLoc from.
    Pm4Predicate predicate,
    void*        pBuffer        // [out] Build the PM4 packet in this buffer.
    ) const
{
    // Draw argument offset in the buffer has to be 4-byte aligned.
    PAL_ASSERT(IsPow2Aligned(offset, 4));

    constexpr size_t DrawIndexIndirectPacketSize = PM4_PFP_DRAW_INDEX_INDIRECT_SIZEDW__CORE;
    size_t packetSize = DrawIndexIndirectPacketSize;

    PM4_PFP_DRAW_INDEX_INDIRECT packet = {};
    packet.ordinal1.header.u32All            =
        (Type3Header(IT_DRAW_INDEX_INDIRECT, DrawIndexIndirectPacketSize, false, ShaderGraphics, predicate)).u32All;
    packet.ordinal2.data_offset              = LowPart(offset);
    packet.ordinal3.bitfields.base_vtx_loc   = baseVtxLoc - PERSISTENT_SPACE_START;

    decltype(PM4_PFP_DRAW_INDEX_INDIRECT::ordinal4) ordinal4 = {};
    ordinal4.bitfields.start_inst_loc = startInstLoc - PERSISTENT_SPACE_START;
    packet.ordinal4 = ordinal4;

    auto*const pDrawInitiator = reinterpret_cast<regVGT_DRAW_INITIATOR*>(&packet.ordinal5.u32All);
    pDrawInitiator->bits.SOURCE_SELECT = DI_SRC_SEL_DMA;
    pDrawInitiator->bits.MAJOR_MODE    = DI_MAJOR_MODE_0;

    static_assert(DrawIndexIndirectPacketSize * sizeof(uint32) == sizeof(packet), "");
    memcpy(pBuffer, &packet, sizeof(packet));
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
    uint32       stride,        // Stride from one indirect args data structure to the next.
    uint32       count,         // Number of draw calls to loop through, or max draw calls if count is in GPU memory.
    gpusize      countGpuAddr,  // GPU address containing the count.
    Pm4Predicate predicate,
    void*        pBuffer        // [out] Build the PM4 packet in this buffer.
    ) const
{
    // Draw argument offset in the buffer has to be 4-byte aligned.
    PAL_ASSERT(IsPow2Aligned(offset, 4));

    constexpr size_t DrawIndexIndirectMultiPacketSize = PM4_PFP_DRAW_INDEX_INDIRECT_MULTI_SIZEDW__CORE;
    size_t packetSize = DrawIndexIndirectMultiPacketSize;

    PM4_PFP_DRAW_INDEX_INDIRECT_MULTI packet = {};
    packet.ordinal1.header.u32All            = (Type3Header(IT_DRAW_INDEX_INDIRECT_MULTI,
                                                              DrawIndexIndirectMultiPacketSize,
                                                              false,
                                                              ShaderGraphics,
                                                              predicate)).u32All;
    packet.ordinal2.data_offset              = LowPart(offset);
    packet.ordinal3.bitfields.base_vtx_loc   = baseVtxLoc - PERSISTENT_SPACE_START;
    packet.ordinal4.bitfields.start_inst_loc = startInstLoc - PERSISTENT_SPACE_START;

    decltype(PM4_PFP_DRAW_INDEX_INDIRECT_MULTI::ordinal5) ordinal5 = {};
    if (drawIndexLoc != UserDataNotMapped)
    {
        ordinal5.bitfields.draw_index_enable = 1;
        ordinal5.bitfields.draw_index_loc    = drawIndexLoc - PERSISTENT_SPACE_START;
    }
    ordinal5.bitfields.count_indirect_enable = (countGpuAddr != 0);

    packet.ordinal5               = ordinal5;
    packet.ordinal6.count         = count;
    packet.ordinal7.u32All        = LowPart(countGpuAddr);
    packet.ordinal8.count_addr_hi = HighPart(countGpuAddr);
    packet.ordinal9.stride        = stride;

    auto*const pDrawInitiator = reinterpret_cast<regVGT_DRAW_INITIATOR*>(&packet.ordinal10.u32All);
    pDrawInitiator->bits.SOURCE_SELECT = DI_SRC_SEL_DMA;
    pDrawInitiator->bits.MAJOR_MODE    = DI_MAJOR_MODE_0;

    static_assert(DrawIndexIndirectMultiPacketSize * sizeof(uint32) == sizeof(packet), "");
    memcpy(pBuffer, &packet, sizeof(packet));
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

    constexpr uint32 PacketSize = PM4_PFP_DRAW_INDIRECT_MULTI_SIZEDW__CORE;
    PM4_PFP_DRAW_INDIRECT_MULTI packet = {};

    packet.ordinal1.header.u32All            = (Type3Header(IT_DRAW_INDIRECT_MULTI, PacketSize,
                                                              false, ShaderGraphics, predicate)).u32All;
    packet.ordinal2.data_offset              = LowPart(offset);
    packet.ordinal3.bitfields.start_vtx_loc  = baseVtxLoc - PERSISTENT_SPACE_START;
    packet.ordinal4.bitfields.start_inst_loc = startInstLoc - PERSISTENT_SPACE_START;

    decltype(PM4_PFP_DRAW_INDIRECT_MULTI::ordinal5) ordinal5 = {};
    if (drawIndexLoc != UserDataNotMapped)
    {
        ordinal5.bitfields.draw_index_enable = 1;
        ordinal5.bitfields.draw_index_loc    = drawIndexLoc - PERSISTENT_SPACE_START;
    }
    ordinal5.bitfields.count_indirect_enable = (countGpuAddr != 0);

    packet.ordinal5               = ordinal5;
    packet.ordinal6.count         = count;
    packet.ordinal7.u32All        = LowPart(countGpuAddr);
    packet.ordinal8.count_addr_hi = HighPart(countGpuAddr);
    packet.ordinal9.stride        = stride;

    auto*const pDrawInitiator = reinterpret_cast<regVGT_DRAW_INITIATOR*>(&packet.ordinal10.u32All);
    pDrawInitiator->bits.SOURCE_SELECT = DI_SRC_SEL_AUTO_INDEX;
    pDrawInitiator->bits.MAJOR_MODE    = DI_MAJOR_MODE_0;

    static_assert(PacketSize * sizeof(uint32) == sizeof(packet), "");
    memcpy(pBuffer, &packet, sizeof(packet));
    return PacketSize;
}

// =====================================================================================================================
// Builds a DISPATCH_TASK_STATE_INIT packet for any engine (ME or MEC) which provides the virtual address with which
// CP can access the control buffer.
size_t CmdUtil::BuildTaskStateInit(
    Pm4ShaderType shaderType,
    gpusize       controlBufferAddr, // [In] Address of the control buffer.
    Pm4Predicate  predicate,         // Predication enable control.
    void*         pBuffer)           // [Out] Build the PM4 packet in this buffer.
{
    // The control buffer address must be 256-byte aligned.
    PAL_ASSERT(IsPow2Aligned(controlBufferAddr, 256));

    static_assert(PM4_MEC_DISPATCH_TASK_STATE_INIT_SIZEDW__GFX10COREPLUS ==
                  PM4_ME_DISPATCH_TASK_STATE_INIT_SIZEDW__GFX10COREPLUS,
                  "ME, MEC versions of PM4_ME_DISPATCH_TASK_STATE_INIT are not the same!");

    constexpr uint32 PacketSize = PM4_ME_DISPATCH_TASK_STATE_INIT_SIZEDW__GFX10COREPLUS;
    PM4_ME_DISPATCH_TASK_STATE_INIT packet = {};

    packet.ordinal1.header = Type3Header(IT_DISPATCH_TASK_STATE_INIT__GFX101,
                                           PacketSize,
                                           false,
                                           shaderType,
                                           predicate);

    packet.ordinal2.u32All              = LowPart(controlBufferAddr);
    PAL_ASSERT(packet.ordinal2.bitfields.gfx10CorePlus.reserved1 == 0);

    packet.ordinal3.control_buf_addr_hi = HighPart(controlBufferAddr);

    static_assert(PacketSize * sizeof(uint32) == sizeof(packet), "");
    memcpy(pBuffer, &packet, sizeof(packet));
    return PacketSize;
}

// =====================================================================================================================
// Builds a DISPATCH_TASKMESH_GFX packet for ME & PFP engines, which consumes data produced by the CS shader and CS
// dispatches that are launched by DISPATCH_TASKMESH_DIRECT_ACE or DISPATCH_TASKMESH_INDIRECT_MULTI_ACE packets by ACE.
// The ME issues multiple sub-draws with the data fetched.
template <bool IssueSqttMarkerEvent>
size_t CmdUtil::BuildDispatchTaskMeshGfx(
    uint32       tgDimOffset,            // First of 3 user-SGPRs where the thread group dimensions (x, y, z) are
                                         // written.
    uint32       ringEntryLoc,           // User-SGPR offset for the ring entry value received for the draw.
    Pm4Predicate predicate,              // Predication enable control.
#if PAL_BUILD_GFX11
    bool         usesLegacyMsFastLaunch, // Use legacy MS fast launch.
    bool         linearDispatch,         // Use linear dispatch.
#endif
    void*        pBuffer                 // [out] Build the PM4 packet in this buffer.
    ) const
{
    static_assert(PM4_ME_DISPATCH_TASKMESH_GFX_SIZEDW__GFX10COREPLUS ==
                  PM4_PFP_DISPATCH_TASKMESH_GFX_SIZEDW__GFX10COREPLUS,
                  "PFP, ME versions of PM4_ME_DISPATCH_TASKMESH_GFX are not the same!");

    PAL_ASSERT(ringEntryLoc != UserDataNotMapped);

    constexpr uint32 PacketSize = PM4_ME_DISPATCH_TASKMESH_GFX_SIZEDW__GFX10COREPLUS;
    PM4_ME_DISPATCH_TASKMESH_GFX packet = {};

    packet.ordinal1.header = Type3Header(IT_DISPATCH_TASKMESH_GFX__GFX101,
                                           PacketSize,
                                           true,
                                           ShaderGraphics,
                                           predicate);

    packet.ordinal2.bitfields.gfx10CorePlus.xyz_dim_loc                = (tgDimOffset != UserDataNotMapped)?
                                                                           tgDimOffset - PERSISTENT_SPACE_START : 0;
    packet.ordinal2.bitfields.gfx10CorePlus.ring_entry_loc             = ringEntryLoc - PERSISTENT_SPACE_START;
    packet.ordinal3.bitfields.gfx10CorePlus.thread_trace_marker_enable = (IssueSqttMarkerEvent) ? 1 : 0;

#if PAL_BUILD_GFX11
    if (IsGfx11(m_chipProps.gfxLevel) && (tgDimOffset != UserDataNotMapped))
    {
        packet.ordinal3.bitfields.gfx11.xyz_dim_enable = 1;
    }

    packet.ordinal3.bitfields.gfx11.mode1_enable = (usesLegacyMsFastLaunch) ? 1 : 0;

    packet.ordinal3.bitfields.gfx11.linear_dispatch_enable = (linearDispatch) ? 1 : 0;
#endif

    auto*const pDrawInitiator = reinterpret_cast<regVGT_DRAW_INITIATOR*>(&packet.ordinal4.u32All);
    pDrawInitiator->bits.SOURCE_SELECT    = DI_SRC_SEL_AUTO_INDEX;
    pDrawInitiator->bits.MAJOR_MODE       = DI_MAJOR_MODE_0;

    static_assert(PacketSize * sizeof(uint32) == sizeof(packet), "");
    memcpy(pBuffer, &packet, sizeof(packet));
    return PacketSize;
}

template
size_t CmdUtil::BuildDispatchTaskMeshGfx<true>(
    uint32       tgDimOffset,
    uint32       ringEntryLoc,
    Pm4Predicate predicate,
#if PAL_BUILD_GFX11
    bool         usesLegacyMsFastLaunch,
    bool         linearDispatch,
#endif
    void*        pBuffer) const;
template
size_t CmdUtil::BuildDispatchTaskMeshGfx<false>(
    uint32       tgDimOffset,
    uint32       ringEntryLoc,
    Pm4Predicate predicate,
#if PAL_BUILD_GFX11
    bool         usesLegacyMsFastLaunch,
    bool         linearDispatch,
#endif
    void*        pBuffer) const;

#if PAL_BUILD_GFX11
// =====================================================================================================================
// Builds a PM4_ME_DISPATCH_MESH_DIRECT packet for the PFP & ME engines.
size_t CmdUtil::BuildDispatchMeshDirect(
    DispatchDims size,
    Pm4Predicate predicate,
    void*        pBuffer)
{
    constexpr uint32 PacketSize = PM4_ME_DISPATCH_MESH_DIRECT_SIZEDW__GFX11;
    auto* const pPacket         = static_cast<PM4_ME_DISPATCH_MESH_DIRECT*>(pBuffer);

    pPacket->ordinal1.header = Type3Header(IT_DISPATCH_MESH_DIRECT__GFX11,
                                           PacketSize,
                                           false,
                                           ShaderGraphics,
                                           predicate);

    pPacket->ordinal2.dim_x = size.x;
    pPacket->ordinal3.dim_y = size.y;
    pPacket->ordinal4.dim_z = size.z;

    regVGT_DRAW_INITIATOR drawInitiator = {};
    drawInitiator.bits.SOURCE_SELECT    = DI_SRC_SEL_AUTO_INDEX;
    drawInitiator.bits.MAJOR_MODE       = DI_MAJOR_MODE_0;
    pPacket->ordinal5.draw_initiator    = drawInitiator.u32All;

    return PacketSize;
}
#endif

// =====================================================================================================================
// Builds a PM4_ME_DISPATCH_MESH_INDIRECT_MULTI packet for the PFP & ME engines.
size_t CmdUtil::BuildDispatchMeshIndirectMulti(
    gpusize      dataOffset,             // Byte offset of the indirect buffer.
    uint32       xyzOffset,              // First of three consecutive user-SGPRs specifying the dimension.
    uint32       drawIndexOffset,        // Draw index user-SGPR offset.
    uint32       count,                  // Number of draw calls to loop through, or max draw calls if count is in GPU
                                         // memory.
    uint32       stride,                 // Stride from one indirect args data structure to the next.
    gpusize      countGpuAddr,           // GPU address containing the count.
    Pm4Predicate predicate,              // Predication enable control.
#if PAL_BUILD_GFX11
    bool         usesLegacyMsFastLaunch, // Use legacy MS fast launch.
#endif
    void*        pBuffer                 // [out] Build the PM4 packet in this buffer.
    ) const
{
    static_assert(PM4_ME_DISPATCH_MESH_INDIRECT_MULTI_SIZEDW__GFX10COREPLUS ==
                  PM4_PFP_DISPATCH_MESH_INDIRECT_MULTI_SIZEDW__GFX10COREPLUS,
                  "PFP, ME versions of PM4_ME_DISPATCH_MESH_INDIRECT_MULTI are not the same!");

    // Draw argument offset in the buffer has to be 4-byte aligned.
    PAL_ASSERT(IsPow2Aligned(dataOffset, 4));
    // The count address must be Dword aligned.
    PAL_ASSERT(IsPow2Aligned(countGpuAddr, 4));

    PM4_ME_DISPATCH_MESH_INDIRECT_MULTI packet     = {};
    constexpr uint32                    PacketSize = PM4_ME_DISPATCH_MESH_INDIRECT_MULTI_SIZEDW__GFX10COREPLUS;

    packet.ordinal1.header = Type3Header(IT_DISPATCH_MESH_INDIRECT_MULTI__GFX101,
                                         PacketSize,
                                         true,
                                         ShaderGraphics,
                                         predicate);

    packet.ordinal2.data_offset                         = LowPart(dataOffset);
    packet.ordinal3.bitfields.gfx10CorePlus.xyz_dim_loc = (xyzOffset != UserDataNotMapped) ?
                                                          xyzOffset - PERSISTENT_SPACE_START : 0;

    if (drawIndexOffset != UserDataNotMapped)
    {
        packet.ordinal3.bitfields.gfx10CorePlus.draw_index_loc    = drawIndexOffset - PERSISTENT_SPACE_START;
        packet.ordinal4.bitfields.gfx10CorePlus.draw_index_enable = 1;
    }

#if PAL_BUILD_GFX11
    if (IsGfx11(m_chipProps.gfxLevel) && (xyzOffset != UserDataNotMapped))
    {
        packet.ordinal4.bitfields.gfx11.xyz_dim_enable = 1;
    }

    packet.ordinal4.bitfields.gfx11.mode1_enable = (usesLegacyMsFastLaunch) ? 1 : 0;
#endif

    if (countGpuAddr != 0)
    {
        packet.ordinal4.bitfields.gfx10CorePlus.count_indirect_enable = 1;
        packet.ordinal6.u32All                                        = LowPart(countGpuAddr);
        PAL_ASSERT(packet.ordinal6.bitfields.gfx10CorePlus.reserved1 == 0);

        packet.ordinal7.count_addr_hi = HighPart(countGpuAddr);
    }

    packet.ordinal5.count  = count;
    packet.ordinal8.stride = stride;

    regVGT_DRAW_INITIATOR drawInitiator = {};
    drawInitiator.bits.SOURCE_SELECT    = DI_SRC_SEL_AUTO_INDEX;
    drawInitiator.bits.MAJOR_MODE       = DI_MAJOR_MODE_0;
    packet.ordinal9.draw_initiator      = drawInitiator.u32All;

    *static_cast<PM4_ME_DISPATCH_MESH_INDIRECT_MULTI*>(pBuffer) = packet;

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4_ME_DISPATCH_MESH_INDIRECT_MULTI_ACE packet for the compute engine.
size_t CmdUtil::BuildDispatchTaskMeshIndirectMultiAce(
    gpusize      dataOffset,       // Byte offset of the indirect buffer.
    uint32       ringEntryLoc,     // Offset of user-SGPR where the CP writes the ring entry WPTR.
    uint32       xyzDimLoc,        // First of three consecutive user-SGPR for the compute dispatch dimensions.
    uint32       dispatchIndexLoc, // User-SGPR offset where the dispatch index is written.
    uint32       count,            // Number of draw calls to loop through, or max draw calls if count is in GPU memory.
    uint32       stride,           // Stride from one indirect args data structure to the next.
    gpusize      countGpuAddr,     // GPU address containing the count.
    bool         isWave32,         // Meaningful for GFX10 only, set if wave-size is 32 for bound compute shader.
    Pm4Predicate predicate,        // Predication enable control.
    void*        pBuffer)          // [out] Build the PM4 packet in this buffer.
{
    // Draw argument offset in the buffer has to be 4-byte aligned.
    PAL_ASSERT(IsPow2Aligned(dataOffset, 4));
    // The count address must be Dword aligned.
    PAL_ASSERT(IsPow2Aligned(countGpuAddr, 4));

    constexpr uint32 PacketSize = CmdUtil::DispatchTaskMeshIndirectMecSize;
    PM4_MEC_DISPATCH_TASKMESH_INDIRECT_MULTI_ACE packet = {};

    packet.ordinal1.header.u32All = (Type3Header(IT_DISPATCH_TASKMESH_INDIRECT_MULTI_ACE__GFX101,
                                                   PacketSize,
                                                   false,
                                                   ShaderCompute,
                                                   predicate)).u32All;

    packet.ordinal2.u32All = LowPart(dataOffset);
    PAL_ASSERT(packet.ordinal2.bitfields.gfx10CorePlus.reserved1 == 0);

    packet.ordinal3.data_addr_hi                           = HighPart(dataOffset);
    packet.ordinal4.bitfields.gfx10CorePlus.ring_entry_loc = ringEntryLoc - PERSISTENT_SPACE_START;

    if (dispatchIndexLoc != UserDataNotMapped)
    {
        packet.ordinal5.bitfields.gfx10CorePlus.dispatch_index_loc = dispatchIndexLoc - PERSISTENT_SPACE_START;
        packet.ordinal5.bitfields.gfx10CorePlus.draw_index_enable  = 1;
    }

    if (xyzDimLoc != UserDataNotMapped)
    {
        packet.ordinal5.bitfields.gfx10CorePlus.compute_xyz_dim_enable = 1;
        packet.ordinal6.bitfields.gfx10CorePlus.compute_xyz_dim_loc    = xyzDimLoc - PERSISTENT_SPACE_START;
    }

    if (countGpuAddr != 0)
    {
        packet.ordinal5.bitfields.gfx10CorePlus.count_indirect_enable = 1;
        packet.ordinal8.u32All                                        = LowPart(countGpuAddr);
        PAL_ASSERT(packet.ordinal6.bitfields.gfx10CorePlus.reserved1 == 0);

        packet.ordinal9.count_addr_hi = HighPart(countGpuAddr);
    }
    else
    {
        packet.ordinal9.count_addr_hi = 0;
    }

    packet.ordinal7.count   = count;
    packet.ordinal10.stride = stride;

    auto*const pDispatchInitiator = reinterpret_cast<regCOMPUTE_DISPATCH_INITIATOR*>(&packet.ordinal11.u32All);
    pDispatchInitiator->bits.COMPUTE_SHADER_EN         = 1;
    pDispatchInitiator->bits.FORCE_START_AT_000        = 0;
    pDispatchInitiator->bits.ORDER_MODE                = 1;
    pDispatchInitiator->gfx10Plus.CS_W32_EN            = isWave32;
#if PAL_BUILD_GFX11
    pDispatchInitiator->gfx11.AMP_SHADER_EN            = 1;
#endif
    pDispatchInitiator->u32All                        |= ComputeDispatchInitiatorDisablePartialPreemptMask;

    static_assert(PacketSize * sizeof(uint32) == sizeof(packet), "");
    memcpy(pBuffer, &packet, sizeof(packet));
    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4_MEC_DISPATCH_TASKMESH_DIRECT_ACE packet for the compute engine, which directly starts the task/mesh
// workload.
size_t CmdUtil::BuildDispatchTaskMeshDirectAce(
    DispatchDims size,         // Thread groups (or threads) to launch.
    uint32       ringEntryLoc, // User data offset where CP writes the payload WPTR.
    Pm4Predicate predicate,    // Predication enable control. Must be PredDisable on the Compute Engine.
    bool         isWave32,     // Meaningful for GFX10 only, set if wave-size is 32 for bound compute shader
    void*        pBuffer)      // [out] Build the PM4 packet in this buffer.
{
    constexpr uint32 PacketSize = CmdUtil::DispatchTaskMeshDirectMecSize;
    PM4_MEC_DISPATCH_TASKMESH_DIRECT_ACE packet = {};

    packet.ordinal1.header.u32All = (Type3Header(IT_DISPATCH_TASKMESH_DIRECT_ACE__GFX101,
                                                   PacketSize,
                                                   false,
                                                   ShaderCompute,
                                                   predicate)).u32All;

    packet.ordinal2.x_dim                                  = size.x;
    packet.ordinal3.y_dim                                  = size.y;
    packet.ordinal4.z_dim                                  = size.z;
    packet.ordinal6.bitfields.gfx10CorePlus.ring_entry_loc = ringEntryLoc - PERSISTENT_SPACE_START;

    auto*const pDispatchInitiator = reinterpret_cast<regCOMPUTE_DISPATCH_INITIATOR*>(&packet.ordinal5.u32All);
    pDispatchInitiator->bits.COMPUTE_SHADER_EN         = 1;
    pDispatchInitiator->bits.FORCE_START_AT_000        = 0;
    pDispatchInitiator->bits.ORDER_MODE                = 1;
    pDispatchInitiator->gfx10Plus.CS_W32_EN            = isWave32;
#if PAL_BUILD_GFX11
    pDispatchInitiator->gfx11.AMP_SHADER_EN            = 1;
#endif
    pDispatchInitiator->u32All                        |= ComputeDispatchInitiatorDisablePartialPreemptMask;

    static_assert(PacketSize * sizeof(uint32) == sizeof(packet), "");
    memcpy(pBuffer, &packet, sizeof(packet));
    return PacketSize;
}

// =====================================================================================================================
// Constructs a DMA_DATA packet for any engine (PFP, ME, MEC).  Copies data from the source (can be immediate 32-bit
// data or a memory location) to a destination (either memory or a register).
template<bool indirectAddress>
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
         (static_cast<uint32>(dst_sel__mec_dma_data__gds__CORE)           ==
          static_cast<uint32>(dst_sel__pfp_dma_data__gds__CORE))          &&
         (static_cast<uint32>(dst_sel__mec_dma_data__dst_nowhere)         ==
          static_cast<uint32>(dst_sel__pfp_dma_data__dst_nowhere))        &&
         (static_cast<uint32>(dst_sel__mec_dma_data__dst_addr_using_l2)   ==
          static_cast<uint32>(dst_sel__pfp_dma_data__dst_addr_using_l2))),
        "MEC and PFP dst sel dma_data enumerations don't match!");

    static_assert(
        ((static_cast<uint32>(src_sel__mec_dma_data__src_addr_using_sas)  ==
          static_cast<uint32>(src_sel__pfp_dma_data__src_addr_using_sas)) &&
         (static_cast<uint32>(src_sel__mec_dma_data__gds__CORE)           ==
          static_cast<uint32>(src_sel__pfp_dma_data__gds__CORE))          &&
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
        packet.ordinal2.bitfields.core.src_indirect     = 1;
        packet.ordinal2.bitfields.core.dst_indirect     = 1;
        packet.ordinal3.src_addr_offset                  = dmaDataInfo.srcOffset;
        packet.ordinal4.src_addr_hi                      = 0; // ignored for data
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

template
size_t CmdUtil::BuildDmaData<true>(
    DmaDataInfo& dmaDataInfo,
    void*        pBuffer);

template
size_t CmdUtil::BuildDmaData<false>(
    DmaDataInfo& dmaDataInfo,
    void*        pBuffer);

//=====================================================================================================================
// Constructs a PM4 packet for the PFP with information to build an untyped Shader Resource Descriptor. This SRD will
// typically be used to store the VertexBuffer table in IndirectDrawing (ExecuteIndirect).
size_t CmdUtil::BuildUntypedSrd(
    Pm4Predicate               predicate,
    const BuildUntypedSrdInfo* pSrdInfo,
    Pm4ShaderType              shaderType,
    void*                      pBuffer)
{
    const uint32 PacketSize = PM4_PFP_BUILD_UNTYPED_SRD_SIZEDW__CORE;
    PM4_PFP_BUILD_UNTYPED_SRD packet = {};

#if PAL_BUILD_GFX11
    static_assert(IT_BUILD_UNTYPED_SRD__GFX101 == IT_BUILD_UNTYPED_SRD__GFX11,
                  "The BuildUntyped SRD opcodes for Gfx10 and Gfx11 are supposed to be the same by definition.");
#endif

    packet.ordinal1.header.u32All =
        (Type3Header(IT_BUILD_UNTYPED_SRD__GFX101, PacketSize, predicate, shaderType)).u32All;
    // For ExecuteIndirect CP will fetch the Vertex Data from ArgumentBuffer which has index data, set index = 1.
    packet.ordinal2.bitfields.core.index       = 1;
    packet.ordinal2.bitfields.core.src_addr_lo = LowPart(pSrdInfo->srcGpuVirtAddress);
    packet.ordinal3.src_addr_hi                 = HighPart(pSrdInfo->srcGpuVirtAddress);
    packet.ordinal4.src_offset                  = pSrdInfo->srcGpuVirtAddressOffset;
    packet.ordinal5.bitfields.core.dst_addr_lo = LowPart(pSrdInfo->dstGpuVirtAddress);
    packet.ordinal6.dst_addr_hi                 = HighPart(pSrdInfo->dstGpuVirtAddress);
    packet.ordinal7.dst_offset                  = pSrdInfo->dstGpuVirtAddressOffset;
    packet.ordinal8.dword3                      = pSrdInfo->srdDword3;

    static_assert(PacketSize * sizeof(uint32) == sizeof(packet), "");
    memcpy(pBuffer, &packet, sizeof(packet));
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

    constexpr uint32 PacketSize = PM4_CE_DUMP_CONST_RAM_SIZEDW__HASCE;
    PM4_CE_DUMP_CONST_RAM packet = {};

    DumpConstRamOrdinal2 ordinal2 = { };
    ordinal2.bits.hasCe.offset    = ramByteOffset;

    packet.ordinal1.header.u32All          = (Type3Header(IT_DUMP_CONST_RAM, PacketSize)).u32All;
    packet.ordinal2.u32All                 = ordinal2.u32All;
    packet.ordinal3.bitfields.hasCe.num_dw = dwordSize;
    packet.ordinal4.addr_lo                = LowPart(dstGpuAddr);
    packet.ordinal5.addr_hi                = HighPart(dstGpuAddr);

    static_assert(PacketSize * sizeof(uint32) == sizeof(packet), "");
    memcpy(pBuffer, &packet, sizeof(packet));
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

    constexpr uint32 PacketSize = PM4_CE_DUMP_CONST_RAM_OFFSET_SIZEDW__HASCE;
    PM4_CE_DUMP_CONST_RAM_OFFSET packet = {};

    DumpConstRamOrdinal2 ordinal2 = { };
    ordinal2.bits.hasCe.offset    = ramByteOffset;

    packet.ordinal1.header.u32All          = (Type3Header(IT_DUMP_CONST_RAM_OFFSET, PacketSize)).u32All;
    packet.ordinal2.u32All                 = ordinal2.u32All;
    packet.ordinal3.bitfields.hasCe.num_dw = dwordSize;
    packet.ordinal4.addr_offset            = dstAddrOffset;

    static_assert(PacketSize * sizeof(uint32) == sizeof(packet), "");
    memcpy(pBuffer, &packet, sizeof(packet));
    return PacketSize;
}

// =====================================================================================================================
// Build an EVENT_WRITE packet.  Not to be used for any EOP, EOS or SAMPLE_XXXXX type events.  Return the number of
// DWORDs taken up by this packet.
size_t CmdUtil::BuildNonSampleEventWrite(
    VGT_EVENT_TYPE  vgtEvent,
    EngineType      engineType,
    void*           pBuffer    // [out] Build the PM4 packet in this buffer.
    ) const
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

    // If this trips, the caller needs to use the BuildSampleEventWrite() routine instead.
    PAL_ASSERT(VgtEventIndex[vgtEvent] != event_index__me_event_write__sample_streamoutstats__GFX09_10);

    // The CP team says you risk hanging the GPU if you use a TS event with event_write.
    PAL_ASSERT(VgtEventHasTs[vgtEvent] == false);

    size_t totalSize = 0;

#if PAL_BUILD_NAVI3X
    if (Pal::Device::EngineSupportsGraphics(engineType) &&
        m_device.Settings().waReplaceEventsWithTsEvents &&
        ((vgtEvent == CACHE_FLUSH_AND_INV_EVENT) ||
         (vgtEvent == FLUSH_AND_INV_DB_META)     ||
         (vgtEvent == DB_CACHE_FLUSH_AND_INV)    ||
         (vgtEvent == CACHE_FLUSH)))
    {
        // There are a few events which flush DB caches which must not be used when this workaround is active.
        // Instead, we must use an event that does a flush and invalidate with an EOP TS signal. The timestamp
        // doesn't actually need to be written, it just needs to be a TS event (the DB doesn't know the difference).
        // We should use a release_mem packet to handle this because event_write doesn't support TS events. Note that:
        // 1. This is limited to graphics engines because only they can touch the DB caches.
        // 2. Despite being a heavy hammer, CACHE_FLUSH_AND_INV_TS_EVENT is the smallest impact event that covers
        //    the necessary DB caches in all cases.
        ReleaseMemGfx releaseInfo = {};
        releaseInfo.vgtEvent = CACHE_FLUSH_AND_INV_TS_EVENT;
        releaseInfo.dataSel  = data_sel__me_release_mem__none;

        totalSize = BuildReleaseMemGfx(releaseInfo, pBuffer);
    }
    else
#endif
    {
        // Don't use PM4_ME_EVENT_WRITE_SIZEDW__CORE here!  The official packet definition contains extra dwords
        // for functionality that is only required for "sample" type events.
        constexpr uint32   PacketSize = WriteNonSampleEventDwords;
        PM4_ME_EVENT_WRITE packet;
        packet.ordinal1.header                = Type3Header(IT_EVENT_WRITE, PacketSize);
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

        totalSize = PacketSize;
    }

    return totalSize;
}

// =====================================================================================================================
// Build an EVENT_WRITE packet.  Not to be used for any EOP or EOS type events.  Return the number of DWORDs taken up
// by this packet.
size_t CmdUtil::BuildSampleEventWrite(
    VGT_EVENT_TYPE                           vgtEvent,
    ME_EVENT_WRITE_event_index_enum          eventIndex,
    EngineType                               engineType,
#if PAL_BUILD_GFX11
    MEC_EVENT_WRITE_samp_plst_cntr_mode_enum counterMode,
#endif
    gpusize                                  gpuAddr,
    void*                                    pBuffer     // [out] Build the PM4 packet in this buffer.
    ) const
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

#if ( PAL_BUILD_GFX11)
    const bool vsPartialFlushValid = (vgtEvent == VS_PARTIAL_FLUSH) && (m_chipProps.gfxip.supportsSwStrmout != 0);
#else
    const bool vsPartialFlushValid = false;
#endif

    // Note that ZPASS_DONE is marked as deprecated in gfx9 but still works and is required for at least one workaround.
    PAL_ASSERT((vgtEvent == PIXEL_PIPE_STAT_CONTROL) ||
               (vgtEvent == PIXEL_PIPE_STAT_DUMP)    ||
               (vgtEvent == SAMPLE_PIPELINESTAT)     ||
               (vgtEvent == SAMPLE_STREAMOUTSTATS)   ||
               (vgtEvent == SAMPLE_STREAMOUTSTATS1)  ||
               (vgtEvent == SAMPLE_STREAMOUTSTATS2)  ||
               (vgtEvent == SAMPLE_STREAMOUTSTATS3)  ||
               (vgtEvent == ZPASS_DONE__GFX09_10)    ||
               vsPartialFlushValid);

    PAL_ASSERT(vgtEvent != 0x9);

#if ( PAL_BUILD_GFX11)
    const bool vsPartialFlushEventIndexValid =
        ((VgtEventIndex[vgtEvent] == event_index__me_event_write__cs_vs_ps_partial_flush) &&
         (m_chipProps.gfxip.supportsSwStrmout != 0));
#else
    const bool vsPartialFlushEventIndexValid = false;
#endif

    PAL_ASSERT((VgtEventIndex[vgtEvent] == event_index__me_event_write__pixel_pipe_stat_control_or_dump) ||
               (VgtEventIndex[vgtEvent] == event_index__me_event_write__sample_pipelinestat)             ||
               (VgtEventIndex[vgtEvent] == event_index__me_event_write__sample_streamoutstats__GFX09_10) ||
               vsPartialFlushEventIndexValid);

    // Event-write packets destined for the compute queue can only use some events.
    PAL_ASSERT((engineType != EngineTypeCompute) ||
               (static_cast<uint32>(eventIndex) ==
                static_cast<uint32>(event_index__mec_event_write__sample_pipelinestat)));

    // All samples are 64-bit and must meet that address alignment.
    PAL_ASSERT(IsPow2Aligned(gpuAddr, sizeof(uint64)));
#endif

    // Here's where packet building actually starts.
    uint32 packetSize;

#if PAL_BUILD_GFX11
    if ((vgtEvent          == PIXEL_PIPE_STAT_DUMP)                                         &&
        (eventIndex        == event_index__me_event_write__pixel_pipe_stat_control_or_dump) &&
        m_device.Settings().gfx11EnableZpassPacketOptimization)
    {
        packetSize = PM4_ME_EVENT_WRITE_ZPASS_SIZEDW__GFX11;

        PM4_ME_EVENT_WRITE_ZPASS packet = {};
        packet.ordinal1.header = Type3Header(IT_EVENT_WRITE_ZPASS__GFX11, packetSize);
        packet.ordinal2.u32All = LowPart(gpuAddr);
        packet.ordinal3.u32All = HighPart(gpuAddr);

        memcpy(pBuffer, &packet, sizeof(packet));
    }
    else
#endif
    {
        packetSize = PM4_ME_EVENT_WRITE_SIZEDW__CORE;

        PM4_ME_EVENT_WRITE packet = {};
        packet.ordinal1.header                = Type3Header(IT_EVENT_WRITE, packetSize);
        packet.ordinal2.u32All                = 0;
        packet.ordinal2.bitfields.event_type  = vgtEvent;
        packet.ordinal2.bitfields.event_index = eventIndex;

#if PAL_BUILD_GFX11
        if ((engineType == EngineTypeCompute) && IsGfx11(m_chipProps.gfxLevel) && (vgtEvent == SAMPLE_PIPELINESTAT))
        {
            auto*const pPacketMec = reinterpret_cast<PM4_MEC_EVENT_WRITE*>(&packet);
            pPacketMec->ordinal2.bitfields.gfx11.samp_plst_cntr_mode = counterMode;
        }
#endif

        packet.ordinal3.u32All = LowPart(gpuAddr);
        packet.ordinal4.u32All = HighPart(gpuAddr);

        memcpy(pBuffer, &packet, sizeof(packet));
    }

    return packetSize;
}

// =====================================================================================================================
// Builds a PM4 constant engine command to increment the CE counter. Returns the size of the PM4 command built, in
// DWORDs.
size_t CmdUtil::BuildIncrementCeCounter(
    void* pBuffer) // [out] Build the PM4 packet in this buffer.
{
    constexpr uint32 PacketSize = PM4_CE_INCREMENT_CE_COUNTER_SIZEDW__HASCE;
    PM4_CE_INCREMENT_CE_COUNTER packet = {};

    packet.ordinal1.header.u32All           = (Type3Header(IT_INCREMENT_CE_COUNTER, PacketSize)).u32All;
    packet.ordinal2.bitfields.hasCe.cntrsel = cntrsel__ce_increment_ce_counter__increment_ce_counter__HASCE;

    static_assert(PacketSize * sizeof(uint32) == sizeof(packet), "");
    memcpy(pBuffer, &packet, sizeof(packet));
    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 command to increment the DE counter. Returns the size of the PM4 command built, in DWORDs.
size_t CmdUtil::BuildIncrementDeCounter(
    void* pBuffer) // [out] Build the PM4 packet in this buffer.
{
    constexpr uint32 PacketSize  = PM4_ME_INCREMENT_DE_COUNTER_SIZEDW__CORE;
    auto*const       pPacket     = static_cast<PM4_ME_INCREMENT_DE_COUNTER*>(pBuffer);

    pPacket->ordinal1.header     = Type3Header(IT_INCREMENT_DE_COUNTER, PacketSize);
    pPacket->ordinal2.dummy_data = 0;

    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 packet which issues an "index attributes indirect" command into the given DE stream. Return the size of
// the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildIndexAttributesIndirect(
    gpusize baseAddr,   // Base address of an array of index attributes
    uint16  index,       // Index into the array of index attributes to load
    bool    hasIndirectAddress,
    void*   pBuffer)   // [out] Build the PM4 packet in this buffer.
{
    constexpr size_t PacketSize = PM4_PFP_INDEX_ATTRIBUTES_INDIRECT_SIZEDW__CORE;
    PM4_PFP_INDEX_ATTRIBUTES_INDIRECT packet = {};

    packet.ordinal1.header.u32All = (Type3Header(IT_INDEX_ATTRIBUTES_INDIRECT, PacketSize)).u32All;
    if (hasIndirectAddress)
    {
        packet.ordinal2.bitfields.hasCe.indirect_mode =
            mode__pfp_index_attributes_indirect_indirect_offset__GFX09_GFX10CORE;
        packet.ordinal3.addr_offset                       = LowPart(baseAddr);
    }
    else
    {
        packet.ordinal2.u32All            = LowPart(baseAddr);
        PAL_ASSERT(packet.ordinal2.bitfields.reserved1 == 0); // Address must be 4-DWORD aligned
        packet.ordinal3.attribute_base_hi = HighPart(baseAddr);
    }

    packet.ordinal4.bitfields.attribute_index = index;

    static_assert(PacketSize * sizeof(uint32) == sizeof(packet), "");
    memcpy(pBuffer, &packet, sizeof(packet));
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
    static_assert(PM4_PFP_INDIRECT_BUFFER_SIZEDW__CORE == PM4_MEC_INDIRECT_BUFFER_SIZEDW__CORE,
                  "Indirect buffer packets are not the same size between GFX and compute!");

    PM4_PFP_INDIRECT_BUFFER packet = {};
    constexpr uint32 PacketSize = PM4_MEC_INDIRECT_BUFFER_SIZEDW__CORE;
    const IT_OpCodeType opCode = constantEngine ? IT_INDIRECT_BUFFER_CNST : IT_INDIRECT_BUFFER;

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

    constexpr uint32 PacketSize = PM4_CE_LOAD_CONST_RAM_SIZEDW__HASCE;
    PM4_CE_LOAD_CONST_RAM packet = {};

    packet.ordinal1.header.u32All              = (Type3Header(IT_LOAD_CONST_RAM, PacketSize)).u32All;
    packet.ordinal2.addr_lo                    = LowPart(srcGpuAddr);
    packet.ordinal3.addr_hi                    = HighPart(srcGpuAddr);
    packet.ordinal4.bitfields.hasCe.num_dw     = dwordSize;
    packet.ordinal5.bitfields.hasCe.start_addr = ramByteOffset;

    static_assert(PacketSize * sizeof(uint32) == sizeof(packet), "");
    memcpy(pBuffer, &packet, sizeof(packet));
    return PacketSize;
}

// =====================================================================================================================
// Builds a NOP command as long as the specified number of DWORDs. Returns the size of the PM4 command built, in DWORDs
size_t CmdUtil::BuildNop(
    size_t numDwords,
    void*  pBuffer)   // [out] Build the PM4 packet in this buffer.
{
    static_assert((PM4_PFP_NOP_SIZEDW__CORE  == PM4_MEC_NOP_SIZEDW__CORE) &&
                   (PM4_PFP_NOP_SIZEDW__CORE == PM4_CE_NOP_SIZEDW__HASCE),
                  "graphics, compute and constant versions of the NOP packet don't match!");

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
// Builds a PM4 packet which issues a "num instances" command into the given DE command stream. Returns the Size of the
// PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildNumInstances(
    uint32 instanceCount,
    void* pBuffer        // [out] Build the PM4 packet in this buffer.
    ) const
{
    constexpr uint32 PacketSize = PM4_PFP_NUM_INSTANCES_SIZEDW__CORE;
    auto*const       pPacket    = static_cast<PM4_PFP_NUM_INSTANCES*>(pBuffer);

    PM4_ME_TYPE_3_HEADER header = Type3Header(IT_NUM_INSTANCES, PacketSize);

    pPacket->ordinal1.header.u32All = header.u32All;
    pPacket->ordinal2.num_instances = instanceCount;

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
    PM4_PFP_OCCLUSION_QUERY packet = {};

    packet.ordinal1.header.u32All = (Type3Header(IT_OCCLUSION_QUERY, PacketSize)).u32All;
    packet.ordinal2.u32All        = LowPart(queryMemAddr);
    packet.ordinal3.start_addr_hi = HighPart(queryMemAddr);
    packet.ordinal4.u32All        = LowPart(dstMemAddr);
    packet.ordinal5.query_addr_hi = HighPart(dstMemAddr);

    // The query address should be 16-byte aligned.
    PAL_ASSERT((packet.ordinal2.bitfields.reserved1 == 0) && (queryMemAddr != 0));

    // The destination address should be 4-byte aligned.
    PAL_ASSERT((packet.ordinal4.bitfields.reserved1 == 0) && (dstMemAddr != 0));

    static_assert(PacketSize * sizeof(uint32) == sizeof(packet), "");
    memcpy(pBuffer, &packet, sizeof(packet));
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
    static_assert(((PM4_PFP_PRIME_UTCL2_SIZEDW__CORE == PM4_MEC_PRIME_UTCL2_SIZEDW__CORE)  &&
                   (PM4_PFP_PRIME_UTCL2_SIZEDW__CORE == PM4_CE_PRIME_UTCL2_SIZEDW__HASCE)),
                   "PRIME_UTCL2 packet is different between PFP, MEC, and CE!");

    static_assert(((static_cast<uint32>(cache_perm__pfp_prime_utcl2__read)                  ==
                    static_cast<uint32>(cache_perm__mec_prime_utcl2__read))                 &&
                   (static_cast<uint32>(cache_perm__pfp_prime_utcl2__read)                  ==
                    static_cast<uint32>(cache_perm__ce_prime_utcl2__read__HASCE))           &&
                   (static_cast<uint32>(cache_perm__pfp_prime_utcl2__write)                 ==
                    static_cast<uint32>(cache_perm__mec_prime_utcl2__write))                &&
                   (static_cast<uint32>(cache_perm__pfp_prime_utcl2__write)                 ==
                    static_cast<uint32>(cache_perm__ce_prime_utcl2__write__HASCE))          &&
                   (static_cast<uint32>(cache_perm__pfp_prime_utcl2__execute)               ==
                    static_cast<uint32>(cache_perm__mec_prime_utcl2__execute))              &&
                   (static_cast<uint32>(cache_perm__pfp_prime_utcl2__execute)               ==
                    static_cast<uint32>(cache_perm__ce_prime_utcl2__execute__HASCE))),
                  "Cache permissions enum is different between PFP, MEC, and CE!");

    static_assert(((static_cast<uint32>(prime_mode__pfp_prime_utcl2__dont_wait_for_xack)                 ==
                    static_cast<uint32>(prime_mode__mec_prime_utcl2__dont_wait_for_xack))                &&
                   (static_cast<uint32>(prime_mode__pfp_prime_utcl2__dont_wait_for_xack)                 ==
                    static_cast<uint32>(prime_mode__ce_prime_utcl2__dont_wait_for_xack__HASCE))          &&
                   (static_cast<uint32>(prime_mode__pfp_prime_utcl2__wait_for_xack)                      ==
                    static_cast<uint32>(prime_mode__mec_prime_utcl2__wait_for_xack))                     &&
                   (static_cast<uint32>(prime_mode__pfp_prime_utcl2__wait_for_xack)                      ==
                    static_cast<uint32>(prime_mode__ce_prime_utcl2__wait_for_xack__HASCE))),
                  "Prime mode enum is different between PFP, MEC, and CE!");

    constexpr uint32 PacketSize = PM4_PFP_PRIME_UTCL2_SIZEDW__CORE;
    PM4_PFP_PRIME_UTCL2 packet = {};

    packet.ordinal1.header.u32All              = (Type3Header(IT_PRIME_UTCL2, PacketSize)).u32All;
    packet.ordinal2.bitfields.cache_perm       = static_cast<PFP_PRIME_UTCL2_cache_perm_enum>(cachePerm);
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
    PM4_ME_CONTEXT_REG_RMW packet = {};

    packet.ordinal1.header               = Type3Header(IT_CONTEXT_REG_RMW, PacketSize);
    packet.ordinal2.bitfields.reg_offset = regAddr - CONTEXT_SPACE_START;
    packet.ordinal3.reg_mask             = regMask;
    packet.ordinal4.reg_data             = regData;

    static_assert(PacketSize * sizeof(uint32) == sizeof(packet), "");
    memcpy(pBuffer, &packet, sizeof(packet));
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
    PM4_ME_REG_RMW packet = {};

    packet.ordinal1.header                    = Type3Header(IT_REG_RMW, PacketSize);
    packet.ordinal2.bitfields.mod_addr        = regAddr;
    packet.ordinal2.bitfields.shadow_base_sel = shadow_base_sel__me_reg_rmw__no_shadow;
    packet.ordinal2.bitfields.or_mask_src     = or_mask_src__me_reg_rmw__immediate;
    packet.ordinal2.bitfields.and_mask_src    = and_mask_src__me_reg_rmw__immediate;
    packet.ordinal4.or_mask                   = orMask;
    packet.ordinal3.and_mask                  = andMask;

    static_assert(PacketSize * sizeof(uint32) == sizeof(packet), "");
    memcpy(pBuffer, &packet, sizeof(packet));
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

    const uint32 packetSize = PM4_PFP_LOAD_CONFIG_REG_SIZEDW__CORE + (2 * (rangeCount - 1));
    PM4_PFP_LOAD_CONFIG_REG packet = {};

    packet.ordinal1.header.u32All          = (Type3Header(IT_LOAD_CONFIG_REG, packetSize)).u32All;
    packet.ordinal2.bitfields.base_addr_lo = (LowPart(gpuVirtAddr) >> 2);
    packet.ordinal3.base_addr_hi           = HighPart(gpuVirtAddr);

    static_assert(PM4_PFP_LOAD_CONFIG_REG_SIZEDW__CORE * sizeof(uint32) == sizeof(packet), "");
    memcpy(pBuffer, &packet, offsetof(PM4_PFP_LOAD_CONFIG_REG, ordinal4));
    // Note: This is a variable-length packet. The PM4_PFP_LOAD_CONFIG_REG packet contains space for the first register
    // range, but not the others (though they are expected to immediately follow in the command buffer).
    auto*const pPacket = static_cast<PM4_PFP_LOAD_CONFIG_REG*>(pBuffer);
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

    constexpr uint32 PacketSize = PM4_PFP_LOAD_CONTEXT_REG_SIZEDW__CORE;
    PM4_PFP_LOAD_CONTEXT_REG packet = {};

    packet.ordinal1.header.u32All          = (Type3Header(IT_LOAD_CONTEXT_REG, PacketSize)).u32All;
    packet.ordinal2.bitfields.base_addr_lo = (LowPart(gpuVirtAddr) >> 2);
    packet.ordinal3.base_addr_hi           = HighPart(gpuVirtAddr);
    packet.ordinal4.bitfields.reg_offset   = (startRegAddr - CONTEXT_SPACE_START);
    packet.ordinal5.bitfields.num_dwords   = count;

    static_assert(PacketSize * sizeof(uint32) == sizeof(packet), "");
    memcpy(pBuffer, &packet, sizeof(packet));
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

    const uint32 packetSize = PM4_PFP_LOAD_CONTEXT_REG_SIZEDW__CORE + (2 * (rangeCount - 1));
    PM4_PFP_LOAD_CONTEXT_REG packet = {};

    packet.ordinal1.header.u32All          = (Type3Header(IT_LOAD_CONTEXT_REG, packetSize)).u32All;
    packet.ordinal2.bitfields.base_addr_lo = (LowPart(gpuVirtAddr) >> 2);
    packet.ordinal3.base_addr_hi           = HighPart(gpuVirtAddr);

    static_assert(PM4_PFP_LOAD_CONTEXT_REG_SIZEDW__CORE * sizeof(uint32) == sizeof(packet), "");
    memcpy(pBuffer, &packet, offsetof(PM4_PFP_LOAD_CONTEXT_REG, ordinal4));

    // Note: This is a variable-length packet. The PM4_PFP_LOAD_CONTEXT_REG packet contains space for the first register
    // range, but not the others (though they are expected to immediately follow in the command buffer).
    auto*const pPacket = static_cast<PM4_PFP_LOAD_CONTEXT_REG*>(pBuffer);
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

    constexpr uint32 PacketSize = PM4_PFP_LOAD_CONTEXT_REG_INDEX_SIZEDW__CORE;
    auto*const       pPacket    = static_cast<PM4_PFP_LOAD_CONTEXT_REG_INDEX*>(pBuffer);
    PM4_PFP_LOAD_CONTEXT_REG_INDEX packet = { 0 };
    PM4_PFP_TYPE_3_HEADER header;
    header.u32All         = (Type3Header(IT_LOAD_CONTEXT_REG_INDEX, PacketSize)).u32All;
    packet.ordinal1.header                = header;
    packet.ordinal2.u32All                = 0;
    if (directAddress)
    {
        // Only the low 16 bits of addrOffset are honored for the high portion of the GPU virtual address!
        PAL_ASSERT((HighPart(gpuVirtAddrOrAddrOffset) & 0xFFFF0000) == 0);

        packet.ordinal2.bitfields.index       = index__pfp_load_context_reg_index__direct_addr;
        packet.ordinal2.bitfields.mem_addr_lo = (LowPart(gpuVirtAddrOrAddrOffset) >> 2);
        packet.ordinal3.mem_addr_hi           = HighPart(gpuVirtAddrOrAddrOffset);
    }
    else
    {
        // The high part of the offset is ignored when not using direct-address mode because the offset is only
        // specified to the packet using 32 bits.
        PAL_ASSERT(HighPart(gpuVirtAddrOrAddrOffset) == 0);

        packet.ordinal2.bitfields.index       = index__pfp_load_context_reg_index__offset;
        packet.ordinal3.addr_offset           = LowPart(gpuVirtAddrOrAddrOffset);
    }
    packet.ordinal4.u32All                = 0;
    packet.ordinal4.bitfields.reg_offset  = (startRegAddr - CONTEXT_SPACE_START);
    packet.ordinal4.bitfields.data_format = data_format__pfp_load_context_reg_index__offset_and_size;
    packet.ordinal5.u32All                = 0;
    packet.ordinal5.bitfields.num_dwords  = count;

    *pPacket = packet;

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
    void*   pBuffer       // [out] Build the PM4 packet in this buffer.
    ) const
{
    constexpr uint32 PacketSize = PM4_PFP_LOAD_CONTEXT_REG_INDEX_SIZEDW__CORE;
    auto*const       pPacket    = static_cast<PM4_PFP_LOAD_CONTEXT_REG_INDEX*>(pBuffer);
    PM4_PFP_LOAD_CONTEXT_REG_INDEX packet = { 0 };

    PM4_PFP_TYPE_3_HEADER header;
    header.u32All          = (Type3Header(IT_LOAD_CONTEXT_REG_INDEX, PacketSize)).u32All;
    packet.ordinal1.header = header;

    packet.ordinal2.u32All                = 0;
    packet.ordinal2.bitfields.index       = index__pfp_load_context_reg_index__direct_addr;
    packet.ordinal2.bitfields.mem_addr_lo = LowPart(gpuVirtAddr) >> 2;
    packet.ordinal3.mem_addr_hi           = HighPart(gpuVirtAddr);
    // Only the low 16 bits are honored for the high portion of the GPU virtual address!
    PAL_ASSERT((HighPart(gpuVirtAddr) & 0xFFFF0000) == 0);

    packet.ordinal4.u32All                = 0;
    packet.ordinal4.bitfields.data_format = data_format__pfp_load_context_reg_index__offset_and_data;

    packet.ordinal5.u32All                = 0;
    packet.ordinal5.bitfields.num_dwords  = count;

    *pPacket = packet;

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

    constexpr uint32 PacketSize = PM4_PFP_LOAD_SH_REG_SIZEDW__CORE;
    PM4_PFP_LOAD_SH_REG packet = {};

    packet.ordinal1.header.u32All             = (Type3Header(IT_LOAD_SH_REG, PacketSize, false, shaderType)).u32All;
    packet.ordinal2.bitfields.base_address_lo = (LowPart(gpuVirtAddr) >> 2);
    packet.ordinal3.base_address_hi           = HighPart(gpuVirtAddr);
    packet.ordinal4.bitfields.reg_offset      = (startRegAddr - PERSISTENT_SPACE_START);
    packet.ordinal5.bitfields.num_dword       = count;

    static_assert(PacketSize * sizeof(uint32) == sizeof(packet), "");
    memcpy(pBuffer, &packet, sizeof(packet));
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

    const uint32 packetSize = PM4_PFP_LOAD_SH_REG_SIZEDW__CORE + (2 * (rangeCount - 1));
    PM4_PFP_LOAD_SH_REG packet = {};

    packet.ordinal1.header.u32All             = (Type3Header(IT_LOAD_SH_REG, packetSize, false, shaderType)).u32All;
    packet.ordinal2.bitfields.base_address_lo = (LowPart(gpuVirtAddr) >> 2);
    packet.ordinal3.base_address_hi           = HighPart(gpuVirtAddr);

    static_assert(PM4_PFP_LOAD_SH_REG_SIZEDW__CORE * sizeof(uint32) == sizeof(packet), "");
    memcpy(pBuffer, &packet, offsetof(PM4_PFP_LOAD_SH_REG, ordinal4));

    // Note: This is a variable-length packet. The PM4_PFP_LOAD_SH_REG packet contains space for the first register
    // range, but not the others (though they are expected to immediately follow in the command buffer).
    auto*const pPacket = static_cast<PM4_PFP_LOAD_SH_REG*>(pBuffer);
    memcpy(&pPacket->ordinal4, pRanges, (sizeof(RegisterRange) * rangeCount));

    return packetSize;
}

// =====================================================================================================================
// Builds a PM4 packet which issues a load_sh_reg_index command to load a series of individual persistent-state
// registers stored in GPU memory.  Returns the size of the PM4 command assembled, in DWORDs.
//
// The index controls how the CP finds the memory to read from. The data_format controls the layout of that memory.
// - offset_and_size: read count DWORDs and write them to the sequential registers starting at startRegAddr.
// - offset_and_data: read count pairs of relative offset and value pairs, write at each indicated offset.
size_t CmdUtil::BuildLoadShRegsIndex(
    PFP_LOAD_SH_REG_INDEX_index_enum       index,
    PFP_LOAD_SH_REG_INDEX_data_format_enum dataFormat,
    gpusize                                gpuVirtAddr,  // Actually an offset in "offset" mode.
    uint32                                 startRegAddr, // Only used if dataFormat is offset_and_data.
    uint32                                 count,        // This changes meaning depending on dataFormat.
    Pm4ShaderType                          shaderType,
    void*                                  pBuffer       // [out] Build the PM4 packet in this buffer.
    ) const
{
    static_assert(((static_cast<uint32>(index__pfp_load_sh_reg_index__direct_addr)   ==
                    static_cast<uint32>(index__mec_load_sh_reg_index__direct_addr__GFX103COREPLUS)) &&
                   (static_cast<uint32>(index__pfp_load_sh_reg_index__indirect_addr__GFX103COREPLUS)   ==
                    static_cast<uint32>(index__mec_load_sh_reg_index__indirect_addr__GFX103COREPLUS))),
                  "LOAD_SH_REG_INDEX index enumerations don't match between PFP and MEC!");

    static_assert(((static_cast<uint32>(data_format__pfp_load_sh_reg_index__offset_and_size)   ==
                    static_cast<uint32>(data_format__mec_load_sh_reg_index__offset_and_size__GFX103COREPLUS)) &&
                   (static_cast<uint32>(data_format__pfp_load_sh_reg_index__offset_and_data)   ==
                    static_cast<uint32>(data_format__mec_load_sh_reg_index__offset_and_data__GFX103COREPLUS))),
                  "LOAD_SH_REG_INDEX data format enumerations don't match between PFP and MEC!");

    constexpr uint32 PacketSize = PM4_PFP_LOAD_SH_REG_INDEX_SIZEDW__CORE;
    PM4_PFP_LOAD_SH_REG_INDEX packet = {};

    packet.ordinal1.header.u32All = Type3Header(IT_LOAD_SH_REG_INDEX, PacketSize, false, shaderType).u32All;
    packet.ordinal2.u32All = 0;

    if (HasEnhancedLoadShRegIndex())
    {
        packet.ordinal2.bitfields.gfx103CorePlus.index = index;
    }
    else
    {
        packet.ordinal2.bitfields.gfx09.index = index;
    }

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

    packet.ordinal4.u32All                = 0;
    packet.ordinal4.bitfields.data_format = dataFormat;

    if (dataFormat == data_format__pfp_load_sh_reg_index__offset_and_size)
    {
        PAL_ASSERT(IsShReg(startRegAddr));
        packet.ordinal4.bitfields.reg_offset = startRegAddr - PERSISTENT_SPACE_START;
    }

    packet.ordinal5.u32All                = 0;
    packet.ordinal5.bitfields.num_dwords  = count;

    *static_cast<PM4_PFP_LOAD_SH_REG_INDEX*>(pBuffer) = packet;

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

    const uint32 packetSize = PM4_PFP_LOAD_UCONFIG_REG_SIZEDW__CORE + (2 * (rangeCount - 1));
    PM4_PFP_LOAD_UCONFIG_REG packet = {};

    packet.ordinal1.header.u32All             = (Type3Header(IT_LOAD_UCONFIG_REG, packetSize)).u32All;
    packet.ordinal2.bitfields.base_address_lo = (LowPart(gpuVirtAddr) >> 2);
    packet.ordinal3.base_address_hi           = HighPart(gpuVirtAddr);

    static_assert(PM4_PFP_LOAD_UCONFIG_REG_SIZEDW__CORE * sizeof(uint32) == sizeof(packet), "");
    memcpy(pBuffer, &packet, offsetof(PM4_PFP_LOAD_UCONFIG_REG, ordinal4));

    // Note: This is a variable-length packet. The PM4_PFP_LOAD_UCONFIG_REG packet contains space for the first register
    // range, but not the others (though they are expected to immediately follow in the command buffer).
    auto*const   pPacket    = static_cast<PM4_PFP_LOAD_UCONFIG_REG*>(pBuffer);
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
    constexpr uint32 PacketSize = PM4_PFP_PFP_SYNC_ME_SIZEDW__CORE;
    auto*const       pPacket    = static_cast<PM4_PFP_PFP_SYNC_ME*>(pBuffer);

    pPacket->ordinal1.header.u32All = (Type3Header(IT_PFP_SYNC_ME, PacketSize)).u32All;
    pPacket->ordinal2.dummy_data    = 0;

    return PacketSize;
}

// =====================================================================================================================
// Call this to pick an appropriate graphics EOP_TS event for a release_mem.
VGT_EVENT_TYPE CmdUtil::SelectEopEvent(
    SyncRbFlags rbSync
    ) const
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
    SyncGlxFlags* pGlxSync
    ) const
{
    // First, split the syncs into a release set and an acquire set.
    constexpr SyncGlxFlags ReleaseMask = SyncGl2WbInv | SyncGlmInv | SyncGl1Inv | SyncGlvInv;

    SyncGlxFlags releaseSyncs = *pGlxSync & ReleaseMask;
    SyncGlxFlags acquireSyncs = *pGlxSync & ~ReleaseMask;

    if (IsGfx9(m_chipProps.gfxLevel))
    {
        // Gfx9 has restrictions on which combinations of flags it can issue in one cache operation. It would be
        // legal to fill out ReleaseMemCaches with every flag on gfx9, but CmdUtil would internally unroll that into
        // multiple release_mem packets. Given that this function assumes the caller will issue an acquire_mem after
        // the release_mem, we can optimize gfx9 by deferring extra cache syncs to the acquire_mem. We should end
        // up with a single release_mem, a wait, and then 0-2 acquire_mems to invalidate the remaining caches.
        // SelectGfx9CacheOp is meant to build packets but we can reuse its SyncGlxFlags masking logic here.
        SyncGlxFlags deferredSyncs = releaseSyncs;
        SelectGfx9CacheOp(&deferredSyncs);

        // SelectGfx9CacheOp clears the bits it can handle in one release_mem, so we remove the remaining bits it
        // can't process from our release mask and move them into the acquire mask.
        releaseSyncs &= ~deferredSyncs;
        acquireSyncs |= deferredSyncs;
    }

    ReleaseMemCaches caches = {};
    caches.gl2Inv = TestAnyFlagSet(releaseSyncs, SyncGl2Inv);
    caches.gl2Wb  = TestAnyFlagSet(releaseSyncs, SyncGl2Wb);
    caches.glmInv = TestAnyFlagSet(releaseSyncs, SyncGlmInv);
    caches.gl1Inv = TestAnyFlagSet(releaseSyncs, SyncGl1Inv);
    caches.glvInv = TestAnyFlagSet(releaseSyncs, SyncGlvInv);

#if PAL_BUILD_GFX11
    if (IsGfx11(m_chipProps.gfxLevel))
    {
        // Gfx11 added release_mem support for the glk, pull them back out of the acquire mask.
        caches.gfx11GlkInv = TestAnyFlagSet(acquireSyncs, SyncGlkInv);
        caches.gfx11GlkWb  = TestAnyFlagSet(acquireSyncs, SyncGlkWb);

        acquireSyncs &= ~(SyncGlkInv | SyncGlkWb);
    }
#endif

    // Pass the extra flags back out to the caller so they know they need to handle them in an acquire_mem.
    *pGlxSync = acquireSyncs;

    return caches;
}

// =====================================================================================================================
// Convert from ReleaseMemCaches to SyncGlxFlags. ReleaseMemCaches is a subset of SyncGlxFlags.
SyncGlxFlags CmdUtil::GetSyncGlxFlagsFromReleaseMemCaches(
    ReleaseMemCaches releaseCaches
    ) const
{
    SyncGlxFlags syncGlx = {};

    syncGlx |= releaseCaches.gl2Inv ? SyncGl2Inv : SyncGlxNone;
    syncGlx |= releaseCaches.gl2Wb  ? SyncGl2Wb  : SyncGlxNone;
    syncGlx |= releaseCaches.glmInv ? SyncGlmInv : SyncGlxNone;
    syncGlx |= releaseCaches.gl1Inv ? SyncGl1Inv : SyncGlxNone;
    syncGlx |= releaseCaches.glvInv ? SyncGlvInv : SyncGlxNone;

#if PAL_BUILD_GFX11
    if (IsGfx11(m_chipProps.gfxLevel))
    {
        syncGlx |= releaseCaches.gfx11GlkInv ? SyncGlkInv : SyncGlxNone;
        syncGlx |= releaseCaches.gfx11GlkWb  ? SyncGlkWb : SyncGlxNone;
    }
#endif

    return syncGlx;
}

// =====================================================================================================================
// Builds a release_mem packet for compute or graphics. The feature set is restricted to what compute engines and
// graphics engines both support.
//
// Note that ACE does support EOS releases using CS_DONE events but the CP treats them exactly the same as an EOP
// release using a timestamp event. Further, none of the graphics specific timestamp events are meaningful on ACE
// so essentially every ACE release_mem boils down to just a BOTTOM_OF_PIPE_TS event.
//
// On the graphics side of things, EOS releases don't support cache flushes but can issue timestamps. This makes
// graphics EOS releases more restricted than ACE releases.
//
// Thus, this generic implementation only supports EOP releases using BOTTOM_OF_PIPE_TS. In theory it could also
// support CS_DONE events with no cache syncs but we have no current use for that so it seems like a waste of time.
size_t CmdUtil::BuildReleaseMemGeneric(
    const ReleaseMemGeneric& info,
    void*                    pBuffer
    ) const
{
    size_t totalSize;

    if (IsGfx10Plus(m_chipProps.gfxLevel))
    {
#if PAL_BUILD_GFX11
        totalSize = BuildReleaseMemInternalGfx10(info, BOTTOM_OF_PIPE_TS, false, pBuffer);
#else
        totalSize = BuildReleaseMemInternalGfx10(info, BOTTOM_OF_PIPE_TS, pBuffer);
#endif
    }
    else
    {
        totalSize = BuildReleaseMemInternalGfx9(info, info.engineType, BOTTOM_OF_PIPE_TS, pBuffer);
    }

    return totalSize;
}

// =====================================================================================================================
// Graphics engines have some extra release_mem features which BuildReleaseMemGeneric lacks.
size_t CmdUtil::BuildReleaseMemGfx(
    const ReleaseMemGfx& info,
    void*                pBuffer
    ) const
{
    size_t totalSize;

    if (IsGfx10Plus(m_chipProps.gfxLevel))
    {
#if PAL_BUILD_GFX11
        totalSize = BuildReleaseMemInternalGfx10(info, info.vgtEvent, info.usePws, pBuffer);
#else
        totalSize = BuildReleaseMemInternalGfx10(info, info.vgtEvent, pBuffer);
#endif
    }
    else
    {
#if PAL_BUILD_GFX11
        // PWS is only supported on gfx11.
        PAL_ASSERT(info.usePws == false);
#endif

        totalSize = BuildReleaseMemInternalGfx9(info, EngineTypeUniversal, info.vgtEvent, pBuffer);
    }

    return totalSize;
}

// Common assumptions between all RELEASE_MEM packet builders.
static_assert(((static_cast<uint32>(event_index__me_release_mem__end_of_pipe)   ==
                static_cast<uint32>(event_index__mec_release_mem__end_of_pipe)) &&
               (static_cast<uint32>(event_index__me_release_mem__shader_done)   ==
                static_cast<uint32>(event_index__mec_release_mem__shader_done))),
              "RELEASE_MEM event index enumerations don't match between ME and MEC!");
static_assert(((static_cast<uint32>(data_sel__me_release_mem__none)                           ==
                static_cast<uint32>(data_sel__mec_release_mem__none))                         &&
               (static_cast<uint32>(data_sel__me_release_mem__send_32_bit_low)                ==
                static_cast<uint32>(data_sel__mec_release_mem__send_32_bit_low))              &&
               (static_cast<uint32>(data_sel__me_release_mem__send_64_bit_data)               ==
                static_cast<uint32>(data_sel__mec_release_mem__send_64_bit_data))             &&
               (static_cast<uint32>(data_sel__me_release_mem__send_gpu_clock_counter)         ==
                static_cast<uint32>(data_sel__mec_release_mem__send_gpu_clock_counter))       &&
               (static_cast<uint32>(data_sel__me_release_mem__store_gds_data_to_memory__CORE) ==
                static_cast<uint32>(data_sel__mec_release_mem__store_gds_data_to_memory__CORE))),
              "RELEASE_MEM data sel enumerations don't match between ME and MEC!");
static_assert(dst_sel__me_release_mem__tc_l2 == dst_sel__me_release_mem__tc_l2,
              "RELEASE_MEM dst sel enums don't match between ME and MEC!");
static_assert((uint32(int_sel__me_release_mem__none) == uint32(int_sel__mec_release_mem__none)) &&
              (uint32(int_sel__me_release_mem__send_data_and_write_confirm) ==
               uint32(int_sel__mec_release_mem__send_data_and_write_confirm)),
              "RELEASE_MEM int sel enums don't match between ME and MEC!");
static_assert(PM4_MEC_RELEASE_MEM_SIZEDW__CORE == PM4_ME_RELEASE_MEM_SIZEDW__CORE,
              "RELEASE_MEM is different sizes between ME and MEC!");

// =====================================================================================================================
size_t CmdUtil::BuildReleaseMemInternalGfx9(
    const ReleaseMemCore& info,
    EngineType            engineType,
    VGT_EVENT_TYPE        vgtEvent,
    void*                 pBuffer
    ) const
{
    // This path only works on gfx9.
    PAL_ASSERT(IsGfx10Plus(m_chipProps.gfxLevel) == false);

    size_t totalSize = 0;
    const bool isEop = VgtEventHasTs[vgtEvent];

    // The release_mem packet only supports EOS events or EOP TS events.
    PAL_ASSERT(isEop || (vgtEvent == PS_DONE) || (vgtEvent == CS_DONE));

    // This function only supports Glx cache syncs on EOP events. This restriction comes from the graphics engine,
    // where EOS releases don't support cache flushes but can still issue timestamps. On compute engines we could
    // support EOS cache syncs but it's not useful practically speaking because the ACE treats CS_DONE events exactly
    // the same as EOP timestamp events. If we force the caller to use a BOTTOM_OF_PIPE_TS on ACE they lose nothing.
    PAL_ASSERT(isEop || (info.cacheSync.u8All == 0));

    // The EOS path also only supports constant timestamps; that's right, it doesn't support "none".
    PAL_ASSERT(isEop || (info.dataSel == data_sel__me_release_mem__send_32_bit_low)
                     || (info.dataSel == data_sel__me_release_mem__send_64_bit_data));

#if PAL_BUILD_GFX11
    // These bits are only supported on gfx11+.
    PAL_ASSERT((info.cacheSync.gfx11GlkWb == 0) && (info.cacheSync.gfx11GlkInv == 0));
#endif

    // Add a dummy ZPASS_DONE event before EOP timestamp events to avoid a DB hang.
    if (isEop && Pal::Device::EngineSupportsGraphics(engineType) && m_device.Settings().waDummyZpassDoneBeforeTs)
    {
        const BoundGpuMemory& dummyMemory = m_device.DummyZpassDoneMem();
        PAL_ASSERT(dummyMemory.IsBound());

        const size_t size = BuildSampleEventWrite(ZPASS_DONE__GFX09_10,
                                                  event_index__me_event_write__pixel_pipe_stat_control_or_dump,
                                                  engineType,
#if PAL_BUILD_GFX11
                                                  samp_plst_cntr_mode__mec_event_write__legacy_mode__GFX11,
#endif
                                                  dummyMemory.GpuVirtAddr(),
                                                  pBuffer);

        pBuffer = VoidPtrInc(pBuffer, size * sizeof(uint32));
        totalSize += size;
    }

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
        // PAL doesn't support GDS.
        PAL_ASSERT(info.dataSel != data_sel__me_release_mem__store_gds_data_to_memory__CORE);

        // dstAddr must be properly aligned. 4 bytes for a 32-bit write or 8 bytes for a 64-bit write.
        PAL_ASSERT((info.dstAddr != 0) &&
                   (((info.dataSel == data_sel__me_release_mem__send_32_bit_low) && IsPow2Aligned(info.dstAddr, 4)) ||
                    IsPow2Aligned(info.dstAddr, 8)));

        // This won't send an interrupt but will wait for write confirm before writing the data to memory.
        packet.ordinal3.bitfields.int_sel = int_sel__me_release_mem__send_data_and_write_confirm;
    }

    // Gfx9 doesn't have GCR support. Instead, we have to break the input flags down into one or more supported
    // TC cache ops. To make it easier to share code, we convert our packet-specific flags into CacheSyncFlags.
    // Note that gfx9 has no GL1 cache so we ignore that bit.
    SyncGlxFlags glxFlags = (((info.cacheSync.glmInv != 0) ? SyncGlmInv : SyncGlxNone) |
                             ((info.cacheSync.glvInv != 0) ? SyncGlvInv : SyncGlxNone) |
                             ((info.cacheSync.gl2Inv != 0) ? SyncGl2Inv : SyncGlxNone) |
                             ((info.cacheSync.gl2Wb  != 0) ? SyncGl2Wb  : SyncGlxNone));

    while (glxFlags != SyncGlxNone)
    {
        const regCP_COHER_CNTL cntl = SelectGfx9CacheOp(&glxFlags);

        packet.ordinal2.bitfields.gfx09.tcl1_vol_action_ena = cntl.bits.TCL1_VOL_ACTION_ENA;
        packet.ordinal2.bitfields.gfx09.tc_wb_action_ena    = cntl.bits.TC_WB_ACTION_ENA;
        packet.ordinal2.bitfields.gfx09.tcl1_action_ena     = cntl.bits.TCL1_ACTION_ENA;
        packet.ordinal2.bitfields.gfx09.tc_action_ena       = cntl.bits.TC_ACTION_ENA;
        packet.ordinal2.bitfields.gfx09.tc_nc_action_ena    = cntl.bits.TC_NC_ACTION_ENA;
        packet.ordinal2.bitfields.gfx09.tc_wc_action_ena    = cntl.bits.TC_WC_ACTION_ENA;
        packet.ordinal2.bitfields.gfx09.tc_md_action_ena    = cntl.bits.TC_INV_METADATA_ACTION_ENA;

        // If SelectGfx9CacheOp used up all of our flags we can break out and write the final release_mem
        // packet which will write the callers selected data and so on.
        if (glxFlags == SyncGlxNone)
        {
            break;
        }

        // If SelectGfx9CacheOp didn't clear all of our flags we need to issue multiple packets to satisfy all
        // of our requested cache flags without over-syncing by flushing and invalidating all caches.
        //
        // We can break a release_mem into N sequential TC cache ops by setting data_sel = none for the first
        // N-1 packets. Only the Nth packet will write the caller's selected data to the destination memory.
        // Note that we only need to fill out the first two ordinals to get a piplined cache op. We want
        // everything else to be zeroed out (e.g., data_sel = 0).
        PM4_ME_RELEASE_MEM cachesOnly = {};
        cachesOnly.ordinal1.u32All = packet.ordinal1.u32All;
        cachesOnly.ordinal2.u32All = packet.ordinal2.u32All;

        memcpy(pBuffer, &cachesOnly, PacketSize * sizeof(uint32));

        pBuffer = VoidPtrInc(pBuffer, PacketSize * sizeof(uint32));
        totalSize += PacketSize;

        // One last thing, if the caller uses something like CACHE_FLUSH_AND_INV_TS_EVENT we only want to issue that
        // event in the first release_mem. It has to happen first so that the RB caches flush to GL2 before we issue
        // any GL2 syncs and we don't want it to happen again in the next release_mem to avoid wasting time. Recall
        // that this function only supports cache syncs with EOP events so we can just force BOTTOM_OF_PIPE_TS.
        packet.ordinal2.bitfields.event_type = BOTTOM_OF_PIPE_TS;
    }

    // Finally, we write the last release_mem packet and return the total written size in DWORDs.
    memcpy(pBuffer, &packet, PacketSize * sizeof(uint32));

    return totalSize + PacketSize;
}

// =====================================================================================================================
size_t CmdUtil::BuildReleaseMemInternalGfx10(
    const ReleaseMemCore& info,
    VGT_EVENT_TYPE        vgtEvent,
#if PAL_BUILD_GFX11
    bool                  usePws,
#endif
    void*                 pBuffer
    ) const
{
    // This function is named "BuildGfx10..." so don't call it on gfx9.
    PAL_ASSERT(IsGfx10Plus(m_chipProps.gfxLevel));

#if PAL_BUILD_NAVI3X
    if ((vgtEvent == CACHE_FLUSH_TS) && m_device.Settings().waReplaceEventsWithTsEvents)
    {
        // If this workaround is enabled we need to upgrade to a flush and invalidate to avoid a hang.
        vgtEvent = CACHE_FLUSH_AND_INV_TS_EVENT;
    }
#endif

    const bool isEop = VgtEventHasTs[vgtEvent];

    // The release_mem packet only supports EOS events or EOP TS events.
    PAL_ASSERT(isEop || (vgtEvent == PS_DONE) || (vgtEvent == CS_DONE));

    // This function only supports Glx cache syncs on EOP events. This restriction comes from the graphics engine,
    // where EOS releases don't support cache flushes but can still issue timestamps. On compute engines we could
    // support EOS cache syncs but it's not useful practically speaking because the ACE treats CS_DONE events exactly
    // the same as EOP timestamp events. If we force the caller to use a BOTTOM_OF_PIPE_TS on ACE they lose nothing.
    PAL_ASSERT(isEop || (info.cacheSync.u8All == 0));

    // The EOS path also only supports constant timestamps; that's right, it doesn't support "none".
#if PAL_BUILD_GFX11
    // Yes, that means you have to provide a valid dstAddr even when using PWS if the event is an EOS event.
#endif
    PAL_ASSERT(isEop || (info.dataSel == data_sel__me_release_mem__send_32_bit_low)
                     || (info.dataSel == data_sel__me_release_mem__send_64_bit_data));

    // We don't expect this workaround to be enabled on gfx10+ so it's not implemented.
    PAL_ASSERT(m_device.Settings().waDummyZpassDoneBeforeTs == false);

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
        // PAL doesn't support GDS.
        PAL_ASSERT(info.dataSel != data_sel__me_release_mem__store_gds_data_to_memory__CORE);

        // dstAddr must be properly aligned. 4 bytes for a 32-bit write or 8 bytes for a 64-bit write.
        PAL_ASSERT((info.dstAddr != 0) &&
                   (((info.dataSel == data_sel__me_release_mem__send_32_bit_low) && IsPow2Aligned(info.dstAddr, 4)) ||
                    IsPow2Aligned(info.dstAddr, 8)));

        // This won't send an interrupt but will wait for write confirm before writing the data to memory.
        packet.ordinal3.bitfields.int_sel = int_sel__me_release_mem__send_data_and_write_confirm;
    }

#if PAL_BUILD_GFX11
    if (IsGfx11(m_chipProps.gfxLevel))
    {
        packet.ordinal2.bitfields.gfx11.pws_enable = usePws;

        if (info.cacheSync.u8All != 0)
        {
            // Note that glmWb is unimplemented in HW so we don't bother setting it. Everything else we want zeroed.
            //
            // We always prefer parallel cache ops but must force sequential (L0->L1->L2) mode when we're writing
            // back one of the non-write-through L0s before an L2 writeback. Any L0 flush/inv ops in our release_mem's
            // event are already sequential with the CP's GCR request so we only have to worry about K$ writes.
            Gfx10ReleaseMemGcrCntl cntl = {};
            cntl.bits.glmInv     = info.cacheSync.glmInv;
            cntl.bits.glvInv     = info.cacheSync.glvInv;
            cntl.bits.gl1Inv     = info.cacheSync.gl1Inv;
            cntl.bits.gl2Inv     = info.cacheSync.gl2Inv;
            cntl.bits.gl2Wb      = info.cacheSync.gl2Wb;
            cntl.bits.seq        = info.cacheSync.gl2Wb & info.cacheSync.gfx11GlkWb;
            cntl.bits.gfx11GlkWb = info.cacheSync.gfx11GlkWb;

            packet.ordinal2.bitfields.gfx11.gcr_cntl = cntl.u32All;
            packet.ordinal2.bitfields.gfx11.glk_inv  = info.cacheSync.gfx11GlkInv;
        }
    }
    else
    {
        // These bits are only supported on gfx11+.
        PAL_ASSERT((usePws == false) && (info.cacheSync.gfx11GlkWb == 0) && (info.cacheSync.gfx11GlkInv == 0));
#else
    {
#endif
        if (info.cacheSync.u8All != 0)
        {
            // Note that glmWb is unimplemented in HW so we don't bother setting it. Everything else we want zeroed.
            // On gfx10, there are no cases where a release_mem would require seq = 1, we can always run in parallel.
            Gfx10ReleaseMemGcrCntl cntl = {};
            cntl.bits.glmInv = info.cacheSync.glmInv;
            cntl.bits.glvInv = info.cacheSync.glvInv;
            cntl.bits.gl1Inv = info.cacheSync.gl1Inv;
            cntl.bits.gl2Inv = info.cacheSync.gl2Inv;
            cntl.bits.gl2Wb  = info.cacheSync.gl2Wb;

            packet.ordinal2.bitfields.gfx10.gcr_cntl = cntl.u32All;
        }
    }

    // Write the release_mem packet and return the packet size in DWORDs.
    memcpy(pBuffer, &packet, PacketSize * sizeof(uint32));

    return PacketSize;
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
    constexpr size_t PacketSize = PM4_MEC_REWIND_SIZEDW__CORE;
    PM4_MEC_REWIND packet = {};

    packet.ordinal1.header.u32All            = (Type3Header(IT_REWIND, PacketSize, false, ShaderCompute)).u32All;
    packet.ordinal2.bitfields.offload_enable = offloadEnable;
    packet.ordinal2.bitfields.valid          = valid;

    static_assert(PacketSize * sizeof(uint32) == sizeof(packet), "");
    memcpy(pBuffer, &packet, sizeof(packet));
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
    constexpr uint32 PacketSize = PM4_PFP_SET_BASE_SIZEDW__CORE;
    PM4_PFP_SET_BASE packet = {};

    packet.ordinal1.header.u32All        = (Type3Header(IT_SET_BASE, PacketSize, false, shaderType)).u32All;
    packet.ordinal2.bitfields.base_index = baseIndex;
    packet.ordinal3.u32All               = LowPart(address);
    packet.ordinal4.address_hi           = HighPart(address);

    // Make sure our address was aligned properly
    PAL_ASSERT(packet.ordinal3.bitfields.reserved1 == 0);

    static_assert(PacketSize * sizeof(uint32) == sizeof(packet), "");
    memcpy(pBuffer, &packet, sizeof(packet));
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
    constexpr uint32 PacketSize = PM4_CE_SET_BASE_SIZEDW__HASCE;
    PM4_CE_SET_BASE packet = {};

    packet.ordinal1.header.u32All              = (Type3Header(IT_SET_BASE, PacketSize, false, shaderType)).u32All;
    packet.ordinal2.bitfields.hasCe.base_index = baseIndex;
    packet.ordinal3.u32All                     = LowPart(address);
    packet.ordinal4.address_hi                 = HighPart(address);

    // Make sure our address was aligned properly
    PAL_ASSERT(packet.ordinal3.bitfieldsA.hasCe.reserved1 == 0);

    static_assert(PacketSize * sizeof(uint32) == sizeof(packet), "");
    memcpy(pBuffer, &packet, sizeof(packet));
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

    PAL_ASSERT((m_chipProps.gfxLevel != GfxIpLevel::GfxIp9) ||
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
    PAL_ASSERT((m_chipProps.gfxLevel != GfxIpLevel::GfxIp9) || (resetFilterCam == false));

    const uint32 packetSize = ConfigRegSizeDwords + endRegAddr - startRegAddr + 1;
    auto*const   pPacket    = static_cast<PM4_PFP_SET_UCONFIG_REG*>(pBuffer);

    IT_OpCodeType opCode = IT_SET_UCONFIG_REG;
    if (index != index__pfp_set_uconfig_reg_index__default)
    {
        // GFX9 started supporting uconfig-reg-index as of ucode version 26.
        if ((m_chipProps.cpUcodeVersion >= 26)
            || IsGfx10Plus(m_chipProps.gfxLevel)
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
                            static_cast<uint32>(index__pfp_set_uconfig_reg__prim_type__GFX09))            &&
                           (static_cast<uint32>(index__pfp_set_uconfig_reg_index__index_type)             ==
                            static_cast<uint32>(index__pfp_set_uconfig_reg__index_type__GFX09))           &&
                           (static_cast<uint32>(index__pfp_set_uconfig_reg_index__num_instances)          ==
                            static_cast<uint32>(index__pfp_set_uconfig_reg__num_instances__GFX09))        &&
                           (static_cast<uint32>(index__pfp_set_uconfig_reg_index__multi_vgt_param__GFX09) ==
                            static_cast<uint32>(index__pfp_set_uconfig_reg__multi_vgt_param__GFX09))),
                          "uconfig index enumerations have changed across old and new packets!");
        }
    }
    PM4_PFP_TYPE_3_HEADER header;
    header.u32All = (Type3Header(opCode, packetSize, resetFilterCam)).u32All;
    pPacket->ordinal1.header                = header;
    pPacket->ordinal2.u32All                = Type3Ordinal2((startRegAddr - UCONFIG_SPACE_START), index);

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

#if PAL_BUILD_GFX11
// =====================================================================================================================
// Builds a Type 3 header for various packed register pair packets and places it in pPacket's first DWORD + places count
// of registers in the second. Also updates the packet size passed in. Returns the next unused DWORD in pPacket.
template <Pm4ShaderType ShaderType>
uint32* CmdUtil::FillPackedRegPairsHeaderAndCount(
    uint32  numRegs,
    bool    isShReg,
    size_t* pPacketSize,
    uint32* pPacket
    ) const
{
    // Every 2 registers comprises a pair with:
    //  - 1 DW containing both offsets
    //  - 1 DW containing data for offset0
    //  - 1 DW containing data for offset1
    const uint32 roundedNumRegs      = Pow2Align(numRegs, 2);
    const uint32 numPackedPairDwords = ((roundedNumRegs / 2) - 1) * 3;
    *pPacketSize                     = numPackedPairDwords + PackedRegPairPacketSize;
    // Currently the fixed length optimization for packed register packets is only supported for SH regs. This and
    // following checks must be updated when fixed length support is either made generic or expanded.
    const uint32 maxFixedLengthRange = (m_chipProps.pfpUcodeVersion >= MinExpandedPackedFixLengthPfpVersion)
                                       ? MaxNumPackedFixLengthRegsExpanded : MaxNumPackedFixLengthRegs;
    const bool  isFixedLength        = isShReg && (roundedNumRegs >= MinNumPackedFixLengthRegs)
                                               && (roundedNumRegs <= maxFixedLengthRange);

    const IT_OpCodeType packetOpcode = isFixedLength ? IT_SET_SH_REG_PAIRS_PACKED_N__GFX11 :
                                       isShReg       ? IT_SET_SH_REG_PAIRS_PACKED__GFX11   :
                                                       IT_SET_CONTEXT_REG_PAIRS_PACKED__GFX11;

    *pPacket = (Type3Header(packetOpcode,
                            uint32(*pPacketSize),
                            true,                 // Required as this is handled entirely in ucode.
                            ShaderType)).u32All;

    // Packed reg pair packets require the raw count of packed registers be placed in the DWORD following the header.
    (*++pPacket) = roundedNumRegs;

    return ++pPacket;
}

// =====================================================================================================================
// Builds a PM4 packet which sets a sequence of context/SH registers as ([offset1 << 16 | offset0], val0, val1) groups,
// skipping those not set in the associated mask. *The mask must be nonzero*. It is expected this function is only used
// when PM4 optimization is enabled. Returns the size of the PM4 command assembled, in DWORDs.
template <Pm4ShaderType ShaderType, size_t N>
size_t CmdUtil::BuildSetMaskedPackedRegPairs(
    const PackedRegisterPair* pRegPairs,
    uint32                    (&validMask)[N],
    bool                      isShReg,
    void*                     pBuffer          // [out] Build the PM4 packet in this buffer.
    ) const
{
    WideBitIter<uint32, N> validIter(validMask);
    const uint32 numRegs = validIter.Size();

    PAL_ASSERT(numRegs > 0);

    size_t packetSize = 0;
    if (numRegs >= 2)
    {
        uint32* pPacket = static_cast<uint32*>(pBuffer);
        pPacket = FillPackedRegPairsHeaderAndCount<ShaderType>(numRegs, isShReg, &packetSize, pPacket);

        uint32 i = 0;
        while (validIter.IsValid())
        {
            const uint32 index = validIter.Get();

            const uint32 pairIndex = index / 2;
            const auto&  regPair   = pRegPairs[pairIndex];

            const uint16 offset = ((index % 2) == 0) ? regPair.offset0 : regPair.offset1;
            const uint32 value  = ((index % 2) == 0) ? regPair.value0  : regPair.value1;

            PackedRegisterPair* pPacketPair = &reinterpret_cast<PackedRegisterPair*>(pPacket)[i / 2];

            if ((i % 2) == 0)
            {
                pPacketPair->offset0 = offset;
                pPacketPair->value0  = value;
            }
            else
            {
                pPacketPair->offset1 = offset;
                pPacketPair->value1  = value;
            }

            i++;
            validIter.Next();
        }

        // We have one extra we have to handle.
        // We have been advised that if we have an odd number of registers to write, we should reuse the first one
        // to avoid corrupting random registers.
        if ((i % 2) != 0)
        {
            uint32     index = 0;
            const bool found = WideBitMaskScanForward(&index, validMask);
            PAL_ASSERT(found);

            const uint32 pairIndex = index / 2;
            const auto&  regPair   = pRegPairs[pairIndex];

            const uint16 offset = regPair.offset0;
            const uint32 value  = regPair.value0;

            PackedRegisterPair* pPacketPair = &reinterpret_cast<PackedRegisterPair*>(pPacket)[i / 2];

            pPacketPair->offset1 = offset;
            pPacketPair->value1  = value;

            i++;
        }

        // Ensure the odd case is handled.
        PAL_ASSERT(i == Pow2Align(numRegs, 2));
    }
    else
    {
        // We only have a single register to write, use the normal SET_*_REG packet.
        uint32     index = 0;
        const bool found = WideBitMaskScanForward(&index, validMask);
        PAL_ASSERT(found);

        const uint32 pairIndex = index / 2;
        const auto&  regPair   = pRegPairs[pairIndex];

        const uint16 offset = ((index % 2) == 0) ? regPair.offset0 : regPair.offset1;
        const uint32 value  = ((index % 2) == 0) ? regPair.value0  : regPair.value1;

        uint32* pPacket = static_cast<uint32*>(pBuffer);
        packetSize = isShReg ? BuildSetOneShReg(offset + PERSISTENT_SPACE_START, ShaderType, pPacket)
                             : BuildSetOneContextReg(offset + CONTEXT_SPACE_START, pPacket);

        static_assert(ContextRegSizeDwords == ShRegSizeDwords, "Context and Sh packet sizes do not match!");

        pPacket[ShRegSizeDwords] = value;
    }

    return packetSize;
}

template
size_t CmdUtil::BuildSetMaskedPackedRegPairs<ShaderGraphics, Gfx11NumRegPairSupportedStagesGfx>(
    const PackedRegisterPair* pRegPairs,
    uint32                    (&validMask)[Gfx11NumRegPairSupportedStagesGfx],
    bool                      isShReg,
    void*                     pBuffer
    ) const;
template
size_t CmdUtil::BuildSetMaskedPackedRegPairs<ShaderCompute, Gfx11NumRegPairSupportedStagesCs>(
    const PackedRegisterPair* pRegPairs,
    uint32                    (&validMask)[Gfx11NumRegPairSupportedStagesCs],
    bool                      isShReg,
    void*                     pBuffer
    ) const;

// =====================================================================================================================
// Builds a PM4 packet which sets a sequence of context/SH registers as ([offset1 << 16 | offset0], val0, val1) groups.
// Note, if given an odd number of 'numRegs', the function will modify pRegPairs to place pRegPairs[0].offset1/val1
// into the last regpair's offset1/val1.
// Returns the size of the PM4 command assembled, in DWORDs.
template <Pm4ShaderType ShaderType>
size_t CmdUtil::BuildSetPackedRegPairs(
    PackedRegisterPair* pRegPairs,
    uint32              numRegs,
    bool                isShReg,
    void*               pBuffer    // [out] Build the PM4 packet in this buffer.
    ) const
{
    PAL_ASSERT(numRegs > 0);

    size_t packetSize = 0;

    if (numRegs >= 2)
    {
        uint32* pPacket = static_cast<uint32*>(pBuffer);
        pPacket = FillPackedRegPairsHeaderAndCount<ShaderType>(numRegs, isShReg, &packetSize, pPacket);

        if ((numRegs % 2) != 0)
        {
            // We have one extra we have to handle.
            // We have been advised that if we have an odd number of registers to write, we should reuse the first one
            // to avoid corrupting random registers.
            pRegPairs[numRegs / 2].offset1 = pRegPairs[0].offset0;
            pRegPairs[numRegs / 2].value1  = pRegPairs[0].value0;
        }

        memcpy(pPacket, pRegPairs, (packetSize - 2) * sizeof(uint32));
    }
    else
    {
        // We only have a single register to write, use the normal SET_*_REG packet.
        const uint32 offset = pRegPairs[0].offset0;
        const uint32 value  = pRegPairs[0].value0;

        uint32* pPacket = static_cast<uint32*>(pBuffer);
        packetSize = isShReg ? BuildSetOneShReg(offset + PERSISTENT_SPACE_START, ShaderType, pPacket)
                             : BuildSetOneContextReg(offset + CONTEXT_SPACE_START, pPacket);

        static_assert(ContextRegSizeDwords == ShRegSizeDwords, "Context and Sh packet sizes do not match!");

        pPacket[ShRegSizeDwords] = value;
    }

    return packetSize;
}

template
size_t CmdUtil::BuildSetPackedRegPairs<ShaderGraphics>(
    PackedRegisterPair* pRegPairs,
    uint32              numRegs,
    bool                isShReg,
    void*               pBuffer
    ) const;
template
size_t CmdUtil::BuildSetPackedRegPairs<ShaderCompute>(
    PackedRegisterPair* pRegPairs,
    uint32              numRegs,
    bool                isShReg,
    void*               pBuffer
    ) const;

//=====================================================================================================================
// Builds a PM4 packet which sets a sequence of SH reg using the optimized pairs packed packet.
// Returns the size of the PM4 command assembled, in DWORDs.
template <Pm4ShaderType ShaderType>
size_t CmdUtil::BuildSetShRegPairsPacked(
    PackedRegisterPair* pRegPairs,
    uint32              numRegs,
    void*               pBuffer
    ) const
{
    return BuildSetPackedRegPairs<ShaderType>(pRegPairs, numRegs, true, pBuffer);
}

template
size_t CmdUtil::BuildSetShRegPairsPacked<ShaderGraphics>(
    PackedRegisterPair* pRegPairs,
    uint32              numRegs,
    void* pBuffer
    ) const;
template
size_t CmdUtil::BuildSetShRegPairsPacked<ShaderCompute>(
    PackedRegisterPair* pRegPairs,
    uint32              numRegs,
    void* pBuffer
    ) const;

// =====================================================================================================================
// Builds a PM4 packet which sets a sequence of context regs using the optimized SET_CONTEXT_REG_PAIRS_PACKED packet.
// Returns the size of the PM4 command assembled, in DWORDs.
size_t CmdUtil::BuildSetContextRegPairsPacked(
    PackedRegisterPair* pRegPairs,
    uint32              numRegs,
    void*               pBuffer
    ) const
{
    return BuildSetPackedRegPairs<ShaderGraphics>(pRegPairs, numRegs, false, pBuffer);
}
#endif

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

    const uint32      packetSize = ShRegSizeDwords + endRegAddr - startRegAddr + 1;
    PM4_ME_SET_SH_REG packet = {};

    PM4_ME_TYPE_3_HEADER header = Type3Header(IT_SET_SH_REG, packetSize, false, shaderType);
    packet.ordinal1.header    = header;

    packet.ordinal2.bitfields.reg_offset  = startRegAddr - PERSISTENT_SPACE_START;

    static_assert(ShRegSizeDwords * sizeof(uint32) == sizeof(packet), "");
    memcpy(pBuffer, &packet , sizeof(packet));
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
    CheckShadowedShRegs(shaderType, startRegAddr, endRegAddr,
                        (index != index__pfp_set_sh_reg_index__apply_kmd_cu_and_mask));
#endif

    // Minimum microcode feature version required by gfx-9 hardware to support the packet SET_SH_REG_INDEX
    constexpr uint32 MinUcodeFeatureVersionForSetShRegIndex = 26;
    size_t packetSize = 0;

    // Switch to the SET_SH_REG opcode for setting the registers if SET_SH_REG_INDEX opcode is not supported.
    if ((m_chipProps.gfxLevel == GfxIpLevel::GfxIp9) &&
        (m_chipProps.cpUcodeVersion < MinUcodeFeatureVersionForSetShRegIndex))
    {
        packetSize = BuildSetSeqShRegs(startRegAddr, endRegAddr, shaderType, pBuffer);
    }
    else
    {
        packetSize = ShRegIndexSizeDwords + endRegAddr - startRegAddr + 1;
        PM4_PFP_SET_SH_REG_INDEX packet = {};

        packet.ordinal1.header.u32All        = (Type3Header(IT_SET_SH_REG_INDEX,
                                                            static_cast<uint32>(packetSize),
                                                            false,
                                                            shaderType)).u32All;
        packet.ordinal2.bitfields.index       = index;
        packet.ordinal2.bitfields.reg_offset  = startRegAddr - PERSISTENT_SPACE_START;

        static_assert(ShRegIndexSizeDwords * sizeof(uint32) == sizeof(packet), "");
        memcpy(pBuffer, &packet, sizeof(packet));
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

    const uint32 packetSize  = ContextRegSizeDwords + endRegAddr - startRegAddr + 1;
    auto*const   pPacket     = static_cast<PM4_PFP_SET_CONTEXT_REG*>(pBuffer);

    PM4_PFP_TYPE_3_HEADER header;
    header.u32All            = (Type3Header(IT_SET_CONTEXT_REG, packetSize)).u32All;
    pPacket->ordinal1.header = header;
    pPacket->ordinal2.u32All = Type3Ordinal2((startRegAddr - CONTEXT_SPACE_START), index);

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
        (static_cast<PFP_SET_PREDICATION_pred_op_enum>(PredicateType::Boolean64)   ==
                pred_op__pfp_set_predication__DX12) &&
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
#if PAL_BUILD_GFX11
    gpusize controlBufAddr, // On ASICs with software streamout, this is the GPU virtual address of the streamout
                            // control buffer which contains the offsets and buffer-filled-sizes for the different
                            // buffers.
#endif
    void*   pBuffer)        // [out] Build the PM4 packet in this buffer.
{
    static_assert(PM4_PFP_STRMOUT_BUFFER_UPDATE_SIZEDW__CORE == PM4_ME_STRMOUT_BUFFER_UPDATE_SIZEDW__CORE,
                  "STRMOUT_BUFFER_UPDATE packet differs between PFP and ME!");

    static_assert(
        ((static_cast<uint32>(source_select__pfp_strmout_buffer_update__use_buffer_offset)                   ==
          static_cast<uint32>(source_select__me_strmout_buffer_update__use_buffer_offset))                   &&
         (static_cast<uint32>(source_select__pfp_strmout_buffer_update__read_strmout_buffer_filled_size)     ==
          static_cast<uint32>(source_select__me_strmout_buffer_update__read_strmout_buffer_filled_size))     &&
         (static_cast<uint32>(source_select__pfp_strmout_buffer_update__from_src_address)                    ==
          static_cast<uint32>(source_select__me_strmout_buffer_update__from_src_address))                    &&
         (static_cast<uint32>(source_select__pfp_strmout_buffer_update__none__GFX09_10)                      ==
          static_cast<uint32>(source_select__me_strmout_buffer_update__none__GFX09_10))),
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

    constexpr uint32 PacketSize = PM4_PFP_STRMOUT_BUFFER_UPDATE_SIZEDW__CORE;
    PM4_PFP_STRMOUT_BUFFER_UPDATE packet = {};

    packet.ordinal1.header.u32All           = (Type3Header(IT_STRMOUT_BUFFER_UPDATE, PacketSize)).u32All;
    packet.ordinal2.bitfields.update_memory = update_memory__pfp_strmout_buffer_update__dont_update_memory;
    packet.ordinal2.bitfields.source_select = static_cast<PFP_STRMOUT_BUFFER_UPDATE_source_select_enum>(sourceSelect);
    packet.ordinal2.bitfields.buffer_select = static_cast<PFP_STRMOUT_BUFFER_UPDATE_buffer_select_enum>(bufferId);

    constexpr PFP_STRMOUT_BUFFER_UPDATE_data_type_enum DataType = data_type__pfp_strmout_buffer_update__bytes;

#if PAL_BUILD_GFX11
    // We can make the assumption that if the streamout control buffer address is non-zero that we need to utilize
    // the newer version of the packet that requires the control buffer address.
    if (controlBufAddr != 0)
    {
        packet.ordinal5.u32All             = LowPart(controlBufAddr);
#if PAL_BUILD_GFX11
        PAL_ASSERT(packet.ordinal5.bitfields.gfx11.reserved1 == 0);
#endif
        packet.ordinal6.control_address_hi = HighPart(controlBufAddr);

        switch (sourceSelect)
        {
        case source_select__pfp_strmout_buffer_update__use_buffer_offset:
            packet.ordinal3.offset = explicitOffset;
            break;
        case source_select__pfp_strmout_buffer_update__read_strmout_buffer_filled_size:
            // No additional members need to be set for this operation.
            break;
        case source_select__pfp_strmout_buffer_update__from_src_address:
            packet.ordinal3.u32All               = LowPart(srcGpuVirtAddr);
#if PAL_BUILD_GFX11
            PAL_ASSERT(packet.ordinal3.bitfieldsB.gfx11.reserved2 == 0);
#endif
            packet.ordinal4.src_address_hi       = HighPart(srcGpuVirtAddr);
            packet.ordinal2.bitfields.data_type  = DataType;
            break;
        case source_select__pfp_strmout_buffer_update__none__GFX09_10:
            packet.ordinal2.bitfields.update_memory =
                update_memory__pfp_strmout_buffer_update__update_memory_at_dst_address;
            packet.ordinal3.u32All                  = LowPart(dstGpuVirtAddr);
#if PAL_BUILD_GFX11
            PAL_ASSERT(packet.ordinal3.bitfieldsC.gfx11.reserved3 == 0);
#endif
            packet.ordinal4.dst_address_hi          = HighPart(dstGpuVirtAddr);
            packet.ordinal2.bitfields.data_type     = DataType;
            break;
        default:
            PAL_ASSERT_ALWAYS();
            break;
        }
    }
    else
#endif
    {
        switch (sourceSelect)
        {
        case source_select__pfp_strmout_buffer_update__use_buffer_offset:
            packet.ordinal5.offset_or_address_lo = explicitOffset;
            break;
        case source_select__pfp_strmout_buffer_update__read_strmout_buffer_filled_size:
            // No additional members need to be set for this operation.
            break;
        case source_select__pfp_strmout_buffer_update__from_src_address:
            packet.ordinal5.offset_or_address_lo = LowPart(srcGpuVirtAddr);
            packet.ordinal6.src_address_hi       = HighPart(srcGpuVirtAddr);
            packet.ordinal2.bitfields.data_type  = DataType;
            break;
        case source_select__pfp_strmout_buffer_update__none__GFX09_10:
            packet.ordinal2.bitfields.update_memory =
                update_memory__pfp_strmout_buffer_update__update_memory_at_dst_address;
            packet.ordinal3.u32All                  = LowPart(dstGpuVirtAddr);
            PAL_ASSERT(packet.ordinal3.bitfields.gfx09_10.reserved1 == 0);
            packet.ordinal4.dst_address_hi          = HighPart(dstGpuVirtAddr);
            packet.ordinal2.bitfields.data_type     = DataType;
            break;
        default:
            PAL_ASSERT_ALWAYS();
            break;
        }
    }

    static_assert(PacketSize * sizeof(uint32) == sizeof(packet), "");
    memcpy(pBuffer, &packet, sizeof(packet));
    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 command to stall the CP (ME or MEC) until all prior dispatches have finished. Note that we only need to
// call this helper function on async compute engines; graphics engines can directly issue CS_PARTIAL_FLUSH events.
// Returns the size of the PM4 command written, in DWORDs.
size_t CmdUtil::BuildWaitCsIdle(
    EngineType engineType,
    gpusize    timestampGpuAddr, // This function may write a temporary EOP timestamp to this address.
    void*      pBuffer           // [out] Build the PM4 packet in this buffer.
    ) const
{
    size_t totalSize;

    // Fall back to a EOP TS wait-for-idle if we can't safely use a CS_PARTIAL_FLUSH.
    if (CanUseCsPartialFlush(engineType))
    {
        totalSize = BuildNonSampleEventWrite(CS_PARTIAL_FLUSH, engineType, pBuffer);
    }
    else
    {
        constexpr uint32 ClearedTimestamp   = 0x11111111;
        constexpr uint32 CompletedTimestamp = 0x22222222;

        // Write a known value to the timestamp.
        WriteDataInfo writeData = {};
        writeData.engineType = engineType;
        writeData.dstAddr    = timestampGpuAddr;
        writeData.engineSel  = engine_sel__me_write_data__micro_engine;
        writeData.dstSel     = dst_sel__me_write_data__tc_l2;

        totalSize = BuildWriteData(writeData, ClearedTimestamp, pBuffer);

        // Issue an EOP timestamp event.
        ReleaseMemGeneric releaseInfo = {};
        releaseInfo.engineType = engineType;
        releaseInfo.dstAddr    = timestampGpuAddr;
        releaseInfo.dataSel    = data_sel__me_release_mem__send_32_bit_low;
        releaseInfo.data       = CompletedTimestamp;

        totalSize += BuildReleaseMemGeneric(releaseInfo, static_cast<uint32*>(pBuffer) + totalSize);

        // Wait on the timestamp value.
        totalSize += BuildWaitRegMem(engineType,
                                     mem_space__me_wait_reg_mem__memory_space,
                                     function__me_wait_reg_mem__equal_to_the_reference_value,
                                     engine_sel__me_wait_reg_mem__micro_engine,
                                     timestampGpuAddr,
                                     CompletedTimestamp,
                                     UINT32_MAX,
                                     static_cast<uint32*>(pBuffer) + totalSize);
    }

    return totalSize;
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

    return BuildDmaData<false>(dmaDataInfo, pBuffer);
}

// =====================================================================================================================
// Builds a PM4 command to stall the DE until the CE counter is positive, then decrements the CE counter. Returns the
// size of the PM4 command written, in DWORDs.
size_t CmdUtil::BuildWaitOnCeCounter(
    bool  invalidateKcache,
    void* pBuffer) // [out] Build the PM4 packet in this buffer.
{
    constexpr uint32 PacketSize = PM4_ME_WAIT_ON_CE_COUNTER_SIZEDW__CORE;
    PM4_ME_WAIT_ON_CE_COUNTER packet = {};

    packet.ordinal1.header                           = Type3Header(IT_WAIT_ON_CE_COUNTER, PacketSize);
    packet.ordinal2.bitfields.core.cond_surface_sync = invalidateKcache;

    static_assert(PacketSize * sizeof(uint32) == sizeof(packet), "");
    memcpy(pBuffer, &packet, sizeof(packet));
    return PacketSize;
}

// =====================================================================================================================
// Builds a PM4 command to stall the CE until it is less than the specified number of draws ahead of the DE. Returns
// the size of the PM4 command written, in DWORDs.
size_t CmdUtil::BuildWaitOnDeCounterDiff(
    uint32 counterDiff,
    void*  pBuffer)     // [out] Build the PM4 packet in this buffer.
{
    constexpr uint32 PacketSize = PM4_CE_WAIT_ON_DE_COUNTER_DIFF_SIZEDW__HASCE;
    auto*const       pPacket    = static_cast<PM4_CE_WAIT_ON_DE_COUNTER_DIFF*>(pBuffer);

    pPacket->ordinal1.header.u32All = (Type3Header(IT_WAIT_ON_DE_COUNTER_DIFF, PacketSize)).u32All;
    pPacket->ordinal2.diff          = counterDiff;

    return PacketSize;
}

#if PAL_BUILD_GFX11
// =====================================================================================================================
// Builds a set of PM4 commands that writes a PWS-enabled EOP event then waits for the event to complete.
// Requested cache operations trigger after the release but before the wait clears. The actual wait point may be more
// strict (e.g., ME wait instead of pre_color wait) if PAL needs to adjust things to make the cache operations work.
// An ME wait and EOP release would emulate a non-PWS wait for idle.
//
// Returns the size of the PM4 command built, in DWORDs. Only supported on gfx11+.
size_t CmdUtil::BuildWaitEopPws(
    HwPipePoint  waitPoint,
    SyncGlxFlags glxSync,
    SyncRbFlags  rbSync,
    void*        pBuffer
    ) const
{
    ReleaseMemGfx releaseInfo = {};
    releaseInfo.vgtEvent  = SelectEopEvent(rbSync);
    releaseInfo.cacheSync = SelectReleaseMemCaches(&glxSync);
    releaseInfo.dataSel   = data_sel__me_release_mem__none;
    releaseInfo.usePws    = true;

    size_t totalSize = BuildReleaseMemGfx(releaseInfo, pBuffer);

    // This will set syncCount = 0 to wait for the most recent PWS release_mem (the one we just wrote).
    AcquireMemGfxPws acquireInfo = {};

    // Practically speaking, SelectReleaseMemCaches should consume all of our cache flags on gfx11. If the caller
    // asked for an I$ invalidate then it will get passed to the acquire_mem here but that sync should be rare.
    acquireInfo.cacheSync  = glxSync;
    acquireInfo.counterSel = pws_counter_sel__me_acquire_mem__ts_select__HASPWS;

    switch (waitPoint)
    {
    case HwPipeTop:
        acquireInfo.stageSel = pws_stage_sel__me_acquire_mem__cp_pfp__HASPWS;
        break;
    case HwPipePostPrefetch:
    case HwPipePostCs:
    case HwPipePostBlt:
        // HwPipePostPrefetch, HwPipePreCs, HwPipePreBlt, even though implies more specific destination states, share
        // the same wait stage enum. HwPipePostCs has to go here too because there is no place to wait after compute
        // shaders, we have to upgrade it to a CP wait. HwPipePostBlt needs to wait after draws and dispatches, the
        // most conservative of those are dispatches so it goes here with HwPipePostCs.
        acquireInfo.stageSel = pws_stage_sel__me_acquire_mem__cp_me__HASPWS;
        break;
    case HwPipePreRasterization:
        acquireInfo.stageSel = m_device.Parent()->UsePwsLateAcquirePoint(EngineTypeUniversal)
                               ? pws_stage_sel__me_acquire_mem__pre_depth__HASPWS
                               : pws_stage_sel__me_acquire_mem__cp_me__HASPWS;
        break;
    case HwPipePostPs:
    case HwPipePreColorTarget:
    case HwPipeBottom:
        // HwPipePostPs and HwPipePreColorTarget are essentially the same pipe point with only a minor semantic
        // difference. They both map to pre_color. The last wait stage we can get is pre_color so that's also the best
        // choice for bottom of pipe waits.
        acquireInfo.stageSel = m_device.Parent()->UsePwsLateAcquirePoint(EngineTypeUniversal)
                               ? pws_stage_sel__me_acquire_mem__pre_color__HASPWS
                               : pws_stage_sel__me_acquire_mem__cp_me__HASPWS;
        break;
    default:
        // What is this?
        PAL_ASSERT_ALWAYS();
        break;
    }

    totalSize += BuildAcquireMemGfxPws(acquireInfo, VoidPtrInc(pBuffer, totalSize * sizeof(uint32)));

    return totalSize;
}
#endif

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
    constexpr uint32 PacketSize           = PM4_ME_WAIT_REG_MEM_SIZEDW__CORE;
    auto*const pPacket                    = static_cast<PM4_ME_WAIT_REG_MEM*>(pBuffer);
    auto*const pPacketMecOnly             = static_cast<PM4_MEC_WAIT_REG_MEM*>(pBuffer);
    PM4_ME_WAIT_REG_MEM packet            = { 0 };
    PM4_MEC_WAIT_REG_MEM* pMecPkt         = reinterpret_cast<PM4_MEC_WAIT_REG_MEM*>(&packet);

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
        // Similarily to engine_sel in ME, this ACE offload optimization is only for MEC and a reserved bit for ME.
        pMecPkt->ordinal7.bitfields.optimize_ace_offload_mode = 1;
        *pPacketMecOnly = *pMecPkt;
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
    static_assert(PM4_ME_WAIT_REG_MEM64_SIZEDW__CORE == PM4_MEC_WAIT_REG_MEM64_SIZEDW__CORE,
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
    constexpr uint32 PacketSize  = PM4_ME_WAIT_REG_MEM64_SIZEDW__CORE;
    PM4_ME_WAIT_REG_MEM64 packet = {};

    packet.ordinal1.header                  = Type3Header(IT_WAIT_REG_MEM64, PacketSize);
    packet.ordinal2.bitfields.function      = static_cast<ME_WAIT_REG_MEM64_function_enum>(function);
    packet.ordinal2.bitfields.mem_space     = static_cast<ME_WAIT_REG_MEM64_mem_space_enum>(memSpace);
    packet.ordinal2.bitfields.operation     = operation__me_wait_reg_mem64__wait_reg_mem;
    if (Pal::Device::EngineSupportsGraphics(engineType))
    {
        packet.ordinal2.bitfields.engine_sel = static_cast<ME_WAIT_REG_MEM64_engine_sel_enum>(engine);
    }
    packet.ordinal3.u32All                  = LowPart(addr);
    PAL_ASSERT(packet.ordinal3.bitfieldsA.reserved1 == 0);
    packet.ordinal4.mem_poll_addr_hi        = HighPart(addr);
    packet.ordinal5.reference               = LowPart(reference);
    packet.ordinal6.reference_hi            = HighPart(reference);
    packet.ordinal7.mask                    = LowPart(mask);
    packet.ordinal8.mask_hi                 = HighPart(mask);
    packet.ordinal9.bitfields.poll_interval = Pal::Device::PollInterval;
    if (Pal::Device::EngineSupportsGraphics(engineType) == false)
    {
        auto*const pPacketMecOnly = reinterpret_cast<PM4_MEC_WAIT_REG_MEM64*>(&packet);
        // Similarily to engine_sel in ME, this ACE offload optimization is only for MEC and a reserved bit for ME.
        pPacketMecOnly->ordinal9.bitfields.optimize_ace_offload_mode = 1;
    }

    static_assert(PacketSize * sizeof(uint32) == sizeof(packet), "");
    memcpy(pBuffer, &packet, sizeof(packet));
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
    const uint32 packetSize = PM4_CE_WRITE_CONST_RAM_SIZEDW__HASCE + dwordSize;
    PM4_CE_WRITE_CONST_RAM packet = {};

    packet.ordinal1.header.u32All          = (Type3Header(IT_WRITE_CONST_RAM, packetSize)).u32All;
    packet.ordinal2.bitfields.hasCe.offset = ramByteOffset;

    static_assert(PM4_CE_WRITE_CONST_RAM_SIZEDW__HASCE * sizeof(uint32) == sizeof(packet), "");
    memcpy(pBuffer, &packet, sizeof(packet));

    // Copy the data into the buffer after the packet.
    auto*const pPacket = static_cast<PM4_CE_WRITE_CONST_RAM*>(pBuffer);
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
    static_assert(PM4_MEC_WRITE_DATA_SIZEDW__CORE == PM4_ME_WRITE_DATA_SIZEDW__CORE,
        "write_data packet has different sizes between compute and gfx!");
    static_assert(
        ((static_cast<uint32>(dst_sel__mec_write_data__mem_mapped_register) ==
          static_cast<uint32>(dst_sel__me_write_data__mem_mapped_register)) &&
         (static_cast<uint32>(dst_sel__mec_write_data__tc_l2)               ==
          static_cast<uint32>(dst_sel__me_write_data__tc_l2))               &&
         (static_cast<uint32>(dst_sel__mec_write_data__gds__CORE)           ==
          static_cast<uint32>(dst_sel__me_write_data__gds__CORE))           &&
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
    auto*const   pPacket    = static_cast<PM4_ME_WRITE_DATA*>(pBuffer);
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
    packet.ordinal2.bitfields.cache_policy = cache_policy__me_write_data__lru;
    packet.ordinal2.bitfields.dst_sel      = static_cast<ME_WRITE_DATA_dst_sel_enum>(info.dstSel);
    packet.ordinal2.bitfields.wr_confirm   = info.dontWriteConfirm
                                               ? wr_confirm__me_write_data__do_not_wait_for_write_confirmation
                                               : wr_confirm__me_write_data__wait_for_write_confirmation;

    if (Pal::Device::EngineSupportsGraphics(info.engineType))
    {
        // This field only exists on graphics engines.
        packet.ordinal2.bitfields.engine_sel = static_cast<ME_WRITE_DATA_engine_sel_enum>(info.engineSel);
    }

    packet.ordinal3.u32All          = LowPart(info.dstAddr);
    packet.ordinal4.dst_mem_addr_hi = HighPart(info.dstAddr);

    switch (info.dstSel)
    {
    case dst_sel__me_write_data__mem_mapped_register:
        PAL_ASSERT(packet.ordinal3.bitfieldsA.reserved1 == 0);
        break;

    case dst_sel__me_write_data__memory:
    case dst_sel__me_write_data__tc_l2:
        PAL_ASSERT(packet.ordinal3.bitfieldsC.core.reserved4 == 0);
        break;

    case dst_sel__me_write_data__gds__CORE:
        PAL_ASSERT(packet.ordinal3.bitfieldsB.core.reserved2 == 0);
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

    *pPacket = packet;

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
// Builds an NOP PM4 packet with the payload data embedded inside.
size_t CmdUtil::BuildNopPayload(
    const void* pPayload,
    uint32      payloadSize,
    void*       pBuffer
    ) const
{
    const size_t packetSize = payloadSize + PM4_PFP_NOP_SIZEDW__CORE;
    auto*const   pPacket    = static_cast<PM4_PFP_NOP*>(pBuffer);
    uint32*      pData      = reinterpret_cast<uint32*>(pPacket + 1);

    // Build header (NOP, signature, size, type)
    pPacket->ordinal1.header.u32All = (Type3Header(IT_NOP, static_cast<uint32>(packetSize))).u32All;

    // Append data
    memcpy(pData, pPayload, payloadSize * sizeof(uint32));

    return packetSize;
}

// =====================================================================================================================
size_t CmdUtil::BuildPrimeGpuCaches(
    const PrimeGpuCacheRange& primeGpuCacheRange,
    EngineType                engineType,
    void*                     pBuffer
    ) const
{
    const gpusize clampSize    = m_device.CoreSettings().prefetchClampSize;
    gpusize       prefetchSize = primeGpuCacheRange.size;

    if (clampSize != 0)
    {
        prefetchSize = Min(prefetchSize, clampSize);
    }

    size_t packetSize = 0;

    // examine the usageFlags to determine if GL2 is relevant to that usage's data path, and addrTranslationOnly
    // is false
    // DDN said, the mask of GL2 usages for GFX9 should be everything but CoherCpu and CoherMemory.
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
        dmaDataInfo.srcSel       = src_sel__pfp_dma_data__src_addr_using_l2;
        dmaDataInfo.numBytes     = static_cast<uint32>(prefetchSize);
        dmaDataInfo.usePfp       = (engineType == EngineTypeUniversal);
        dmaDataInfo.disWc        = true;

        packetSize = BuildDmaData<false>(dmaDataInfo, pBuffer);
    }
    else
    {
        // a PRIME_UTCL2 should be performed
        const gpusize firstPage = Pow2AlignDown(primeGpuCacheRange.gpuVirtAddr, PrimeUtcL2MemAlignment);
        const gpusize lastPage  = Pow2AlignDown(primeGpuCacheRange.gpuVirtAddr + prefetchSize - 1,
                                                PrimeUtcL2MemAlignment);

        const size_t  numPages  = 1 + static_cast<size_t>((lastPage - firstPage) / PrimeUtcL2MemAlignment);

        packetSize = BuildPrimeUtcL2(firstPage,
                                     cache_perm__pfp_prime_utcl2__execute,
                                     prime_mode__pfp_prime_utcl2__dont_wait_for_xack,
                                     engine_sel__pfp_prime_utcl2__prefetch_parser,
                                     numPages,
                                     pBuffer);
    }

    return packetSize;
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

// =====================================================================================================================
bool CmdUtil::IsIndexedRegister(
    uint32 regAddr)
{
    return
        (regAddr == mmVGT_LS_HS_CONFIG) ||
        (regAddr == mmSPI_SHADER_PGM_RSRC3_GS) ||
        (regAddr == mmSPI_SHADER_PGM_RSRC4_GS) ||
        (regAddr == mmSPI_SHADER_PGM_RSRC3_HS) ||
        (regAddr == mmSPI_SHADER_PGM_RSRC4_HS) ||
        (regAddr == mmSPI_SHADER_PGM_RSRC3_PS) ||
        (regAddr == Gfx10Plus::mmSPI_SHADER_PGM_RSRC4_PS) ||
        (regAddr == HasHwVs::mmSPI_SHADER_PGM_RSRC3_VS) ||
        (regAddr == Gfx10::mmSPI_SHADER_PGM_RSRC4_VS) ||
        (regAddr == mmVGT_PRIMITIVE_TYPE) ||
        (regAddr == mmVGT_INDEX_TYPE) ||
        (regAddr == mmVGT_NUM_INSTANCES) ||
        (regAddr == Gfx09::mmIA_MULTI_VGT_PARAM);
}

#if PAL_ENABLE_PRINTS_ASSERTS

// =====================================================================================================================
// Helper function which determines if a range of sequential register addresses fall within any of the specified
// register ranges.
inline bool AreRegistersInRangeList(
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

        if ((false == AreRegistersInRangeList(startRegAddr, endRegAddr, pRange, numEntries))
            )
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
    uint32        regAddr,
    bool          shouldBeShadowed
    ) const
{
    CheckShadowedShRegs(shaderType, regAddr, regAddr, shouldBeShadowed);
}

// =====================================================================================================================
// Helper function which verifies that the specified set of sequential SH registers falls within one of the ranges which
// are shadowed when mid command buffer preemption is enabled.
void CmdUtil::CheckShadowedShRegs(
    Pm4ShaderType shaderType,
    uint32        startRegAddr,
    uint32        endRegAddr,
    bool          shouldBeShadowed
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
                                                   numEntries) == shouldBeShadowed);
            }
            else
            {
                pRange = m_device.GetRegisterRange(RegRangeCsSh, &numEntries);

                PAL_ASSERT(AreRegistersInRangeList((startRegAddr - PERSISTENT_SPACE_START),
                                                   (endRegAddr - PERSISTENT_SPACE_START),
                                                   pRange,
                                                   numEntries) == shouldBeShadowed);
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
