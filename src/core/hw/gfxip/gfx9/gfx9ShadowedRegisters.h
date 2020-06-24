/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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

#pragma once

#include "palInlineFuncs.h"
#include "core/hw/gfxip/gfx9/gfx9Chip.h"
#include "core/hw/gfxip/gfx9/gfx10ShadowedRegisters.h"

namespace Pal
{
namespace Gfx9
{
static_assert((Vg12::mmPA_STEREO_CNTL == Vg20::mmPA_STEREO_CNTL),
              "mmPA_STEREO_CNTL has moved between Vega12 and Vega20!");
static_assert((Vg12::mmPA_STATE_STEREO_X == Vg20::mmPA_STATE_STEREO_X),
              "PA_STATE_STEREO_X has moved between Vega12 and Vega20!");

// Defines the set of ranges of user-config registers we shadow when mid command buffer preemption is enabled.
const RegisterRange Gfx9UserConfigShadowRange[] =
{
    {
        (mmCP_STRMOUT_CNTL - UCONFIG_SPACE_START),                        // 0xC03F
         1,
    },
    {
        (mmCP_COHER_START_DELAY - UCONFIG_SPACE_START),                   // 0xC07B
         1,
    },
    // Note: PAL should not state shadow mmVGT_INDEX_TYPE because CP will save and restore both mmVGT_INDEX_TYPE
    // and mmVGT_DMA_INDEX_TYPE. If those two registers have different values, VGT will make pipeline hang.
    // mmVGT_DMA_INDEX_TYPE cannot be shadowed, since it is a register that is sent to VGT by the PFP, so it will not
    // pass through the shadowing logic in ME.
    {
        (NotGfx10::mmVGT_GSVS_RING_SIZE - UCONFIG_SPACE_START),              // 0xC241 - 0xC242
        (mmVGT_PRIMITIVE_TYPE - NotGfx10::mmVGT_GSVS_RING_SIZE + 1),
    },
    {
        (Gfx09::mmVGT_MAX_VTX_INDX - UCONFIG_SPACE_START),                // 0xC248 - 0xC24B
        (Gfx09::mmVGT_MULTI_PRIM_IB_RESET_EN - Gfx09::mmVGT_MAX_VTX_INDX + 1),
    },
    {
        (mmVGT_NUM_INSTANCES - UCONFIG_SPACE_START),                      // 0xC24D - 0xC251
        (Gfx09::mmVGT_TF_MEMORY_BASE_HI - mmVGT_NUM_INSTANCES + 1),
    },
    // Note: PAL must not state shadow the following regs because they are written by KMD:
    // mmWD_POS_BUF_BASE, mmWD_POS_BUF_BASE_HI, mmWD_CNTL_SB_BUF_BASE, mmWD_CNTL_SB_BUF_BASE_HI,
    // mmWD_INDEX_BUF_BASE, mmWD_INDEX_BUF_BASE_HI (0xC252 - 0xC257)
    {
        (Gfx09::mmIA_MULTI_VGT_PARAM - UCONFIG_SPACE_START),              // 0xC258
         1,
    },
    {
        (mmVGT_INSTANCE_BASE_ID - UCONFIG_SPACE_START),                   // 0xC25A
         1,
    },
    {
        (mmTA_CS_BC_BASE_ADDR - UCONFIG_SPACE_START),                     // 0xC380 - 0xC381
        (mmTA_CS_BC_BASE_ADDR_HI - mmTA_CS_BC_BASE_ADDR + 1),
    },
    {
        (Vg12::mmPA_STATE_STEREO_X - UCONFIG_SPACE_START),                // 0xC2B5
        1,
    },

};
constexpr uint32 Gfx9NumUserConfigShadowRanges = static_cast<uint32>(Util::ArrayLen(Gfx9UserConfigShadowRange));

// Defines the set of ranges of context registers we shadow when mid command buffer preemption is enabled.
const RegisterRange Gfx9ContextShadowRange[] =
{
    {
        (mmDB_RENDER_CONTROL - CONTEXT_SPACE_START),                            // 0xA000 - 0xA021
        (mmTA_BC_BASE_ADDR_HI - mmDB_RENDER_CONTROL + 1),
    },
    {
        (mmCOHER_DEST_BASE_HI_0 - CONTEXT_SPACE_START),                         // 0xA07A - 0xA0D7
        (mmPA_SC_TILE_STEERING_OVERRIDE - mmCOHER_DEST_BASE_HI_0 + 1),
    },
    {
        (mmVGT_MULTI_PRIM_IB_RESET_INDX - CONTEXT_SPACE_START),                 // 0xA103
         1,
    },
    {
        (mmCB_BLEND_RED - CONTEXT_SPACE_START),                                 // 0xA105 - 0xA186
        (mmPA_CL_UCP_5_W - mmCB_BLEND_RED + 1),
    },
    {
        (mmSPI_PS_INPUT_CNTL_0 - CONTEXT_SPACE_START),                          // 0xA191 - 0xA1C5
        (mmSPI_SHADER_COL_FORMAT - mmSPI_PS_INPUT_CNTL_0 + 1),
    },
    {
        (mmSX_PS_DOWNCONVERT - CONTEXT_SPACE_START),                            // 0xA1D5 - 0xA1EF
        (Gfx09::mmCB_MRT7_EPITCH - mmSX_PS_DOWNCONVERT + 1),
    },
    {
        (mmDB_DEPTH_CONTROL - CONTEXT_SPACE_START),                             // 0xA200 - 0xA208
        (mmPA_CL_NANINF_CNTL - mmDB_DEPTH_CONTROL + 1),
    },
    {
        (mmPA_SU_SMALL_PRIM_FILTER_CNTL - CONTEXT_SPACE_START),
        (Vg12::mmPA_STEREO_CNTL - mmPA_SU_SMALL_PRIM_FILTER_CNTL + 1),          // 0xA20C - 0xA210
    },
    {
        (mmPA_SU_POINT_SIZE - CONTEXT_SPACE_START),                             // 0xA280 - 0xA283
        (mmPA_SC_LINE_STIPPLE - mmPA_SU_POINT_SIZE + 1),
    },
    {
        (mmVGT_HOS_MAX_TESS_LEVEL - CONTEXT_SPACE_START),                       // 0xA286 - 0xA287
        (mmVGT_HOS_MIN_TESS_LEVEL - mmVGT_HOS_MAX_TESS_LEVEL + 1),
    },
    {
        (mmVGT_GS_MODE - CONTEXT_SPACE_START),                                  // 0xA290 - 0xA29B
        (mmVGT_GS_OUT_PRIM_TYPE - mmVGT_GS_MODE + 1),
    },
    {
        (mmVGT_PRIMITIVEID_EN - CONTEXT_SPACE_START),                           // 0xA2A1
         1,
    },
    {
        (mmVGT_PRIMITIVEID_RESET - CONTEXT_SPACE_START),                        // 0xA2A3
         1,
    },
    {
        (Gfx09::mmVGT_GS_MAX_PRIMS_PER_SUBGROUP - CONTEXT_SPACE_START),         // 0xA2A5 - 0xA2B5
        (mmVGT_STRMOUT_VTX_STRIDE_0 - Gfx09::mmVGT_GS_MAX_PRIMS_PER_SUBGROUP + 1),
    },
    {
        (mmVGT_STRMOUT_BUFFER_SIZE_1 - CONTEXT_SPACE_START),                    // 0xA2B8 - 0xA2B9
        (mmVGT_STRMOUT_VTX_STRIDE_1 - mmVGT_STRMOUT_BUFFER_SIZE_1 + 1),
    },
    {
        (mmVGT_STRMOUT_BUFFER_SIZE_2 - CONTEXT_SPACE_START),                    // 0xA2BC - 0xA2BD
        (mmVGT_STRMOUT_VTX_STRIDE_2 - mmVGT_STRMOUT_BUFFER_SIZE_2 + 1),
    },
    {
        (mmVGT_STRMOUT_BUFFER_SIZE_3 - CONTEXT_SPACE_START),                    // 0xA2C0 - 0xA2C1
        (mmVGT_STRMOUT_VTX_STRIDE_3 - mmVGT_STRMOUT_BUFFER_SIZE_3 + 1),
    },
    {
        (mmVGT_STRMOUT_DRAW_OPAQUE_OFFSET - CONTEXT_SPACE_START),               // 0xA2CA - 0xA2CC
        (mmVGT_STRMOUT_DRAW_OPAQUE_VERTEX_STRIDE - mmVGT_STRMOUT_DRAW_OPAQUE_OFFSET + 1),
    },
    {
        (mmVGT_GS_MAX_VERT_OUT - CONTEXT_SPACE_START),                          // 0xA2CE - 0xA2E6
        (mmVGT_STRMOUT_BUFFER_CONFIG - mmVGT_GS_MAX_VERT_OUT + 1),
    },
    {
        (mmPA_SC_CENTROID_PRIORITY_0 - CONTEXT_SPACE_START),                    // 0xA2F5 - 0xA38F
        (Gfx09::mmCB_COLOR7_DCC_BASE_EXT - mmPA_SC_CENTROID_PRIORITY_0 + 1),
    },
};
constexpr uint32 Gfx9NumContextShadowRanges = static_cast<uint32>(Util::ArrayLen(Gfx9ContextShadowRange));

// Defines the set of ranges of GFX SH registers we shadow when mid command buffer preemption is enabled.
const RegisterRange Gfx9ShShadowRange[] =
{
    {
        (mmSPI_SHADER_PGM_LO_PS - PERSISTENT_SPACE_START),                      // 0x2C08 - 0x2C2B
        (mmSPI_SHADER_USER_DATA_PS_31 - mmSPI_SHADER_PGM_LO_PS + 1),
    },
    {
        (mmSPI_SHADER_LATE_ALLOC_VS - PERSISTENT_SPACE_START),                  // 0x2C47 - 0x2C6B
        (mmSPI_SHADER_USER_DATA_VS_31 - mmSPI_SHADER_LATE_ALLOC_VS + 1),
    },
    {
        (mmSPI_SHADER_PGM_RSRC4_GS - PERSISTENT_SPACE_START),                   // 0x2C81 - 0x2C85
        (Gfx09::mmSPI_SHADER_PGM_HI_ES - mmSPI_SHADER_PGM_RSRC4_GS + 1),
    },
    {
        (mmSPI_SHADER_PGM_LO_GS - PERSISTENT_SPACE_START),                      // 0x2C88 - 0x2C8B
        (mmSPI_SHADER_PGM_RSRC2_GS - mmSPI_SHADER_PGM_LO_GS + 1),
    },
    {
        (Gfx09::mmSPI_SHADER_USER_DATA_ES_0 - PERSISTENT_SPACE_START),          // 0x2CCC - 0x2CEB
        (Gfx09::mmSPI_SHADER_USER_DATA_ES_31 - Gfx09::mmSPI_SHADER_USER_DATA_ES_0 + 1),
    },
    {
        (mmSPI_SHADER_PGM_RSRC4_HS - PERSISTENT_SPACE_START),                   // 0x2D01 - 0x2D05
        (Gfx09::mmSPI_SHADER_PGM_HI_LS - mmSPI_SHADER_PGM_RSRC4_HS + 1),
    },
    {
        (mmSPI_SHADER_PGM_LO_HS - PERSISTENT_SPACE_START),                      // 0x2D08 - 0x2D2B
        (Gfx09::mmSPI_SHADER_USER_DATA_LS_31 - mmSPI_SHADER_PGM_LO_HS + 1),
    },
};
constexpr uint32 Gfx9NumShShadowRanges = static_cast<uint32>(Util::ArrayLen(Gfx9ShShadowRange));

// Defines the set of ranges of CS SH registers we shadow when mid command buffer preemption is enabled.
const RegisterRange Gfx9CsShShadowRange[] =
{
    {
        (mmCOMPUTE_START_X - PERSISTENT_SPACE_START),                           // 0x2E04 - 0x2E09
        (mmCOMPUTE_NUM_THREAD_Z - mmCOMPUTE_START_X + 1),
    },
    {
        (mmCOMPUTE_PERFCOUNT_ENABLE - PERSISTENT_SPACE_START),                  // 0x2E0B - 0x2EOD
        (mmCOMPUTE_PGM_HI - mmCOMPUTE_PERFCOUNT_ENABLE + 1),
    },
    {
        (mmCOMPUTE_PGM_RSRC1 - PERSISTENT_SPACE_START),                         // 0x2E12 - 0x2E13
        (mmCOMPUTE_PGM_RSRC2 - mmCOMPUTE_PGM_RSRC1 + 1),
    },
    {
        (mmCOMPUTE_RESOURCE_LIMITS - PERSISTENT_SPACE_START),                   // 0x2E15
        1,
    },
    {
        (mmCOMPUTE_TMPRING_SIZE - PERSISTENT_SPACE_START),                      // 0x2E18
        1,
    },
    {
        (mmCOMPUTE_THREAD_TRACE_ENABLE - PERSISTENT_SPACE_START),               // 0x2E1E
        1,
    },
    {
        (mmCOMPUTE_USER_DATA_0 - PERSISTENT_SPACE_START),                       // 0x2E40 - 0x2E4F
        (mmCOMPUTE_USER_DATA_15 - mmCOMPUTE_USER_DATA_0 + 1),
    },
};
constexpr uint32 Gfx9NumCsShShadowRanges = static_cast<uint32>(Util::ArrayLen(Gfx9CsShShadowRange));

// Defines the set of ranges of GFX SH registers we shadow when mid command buffer preemption is enabled.
const RegisterRange Gfx9ShShadowRangeRaven2[] =
{
    {
        (Apu09_1xPlus::mmSPI_SHADER_PGM_CHKSUM_PS - PERSISTENT_SPACE_START),    // 0x2C06
        1
    },
    {
        (mmSPI_SHADER_PGM_LO_PS - PERSISTENT_SPACE_START),                      // 0x2C08 - 0x2C2B
        (mmSPI_SHADER_USER_DATA_PS_31 - mmSPI_SHADER_PGM_LO_PS + 1),
    },
    {
        (Apu09_1xPlus::mmSPI_SHADER_PGM_CHKSUM_VS - PERSISTENT_SPACE_START),    // 0x2C45
        1
    },
    {
        (mmSPI_SHADER_LATE_ALLOC_VS - PERSISTENT_SPACE_START),                  // 0x2C47 - 0x2C6B
        (mmSPI_SHADER_USER_DATA_VS_31 - mmSPI_SHADER_LATE_ALLOC_VS + 1),
    },
    {
        (Apu09_1xPlus::mmSPI_SHADER_PGM_CHKSUM_GS - PERSISTENT_SPACE_START),    // 0x2C80 - 0x2C85
        (Gfx09::mmSPI_SHADER_PGM_HI_ES - Apu09_1xPlus::mmSPI_SHADER_PGM_CHKSUM_GS + 1),
    },
    {
        (mmSPI_SHADER_PGM_LO_GS - PERSISTENT_SPACE_START),                      // 0x2C88 - 0x2C8B
        (mmSPI_SHADER_PGM_RSRC2_GS - mmSPI_SHADER_PGM_LO_GS + 1),
    },
    {
        (Gfx09::mmSPI_SHADER_USER_DATA_ES_0 - PERSISTENT_SPACE_START),          // 0x2CCC - 0x2CEB
        (Gfx09::mmSPI_SHADER_USER_DATA_ES_31 - Gfx09::mmSPI_SHADER_USER_DATA_ES_0 + 1),
    },
    {
        (Apu09_1xPlus::mmSPI_SHADER_PGM_CHKSUM_HS - PERSISTENT_SPACE_START),    // 0x2D00 - 0x2D05
        (Gfx09::mmSPI_SHADER_PGM_HI_LS - Apu09_1xPlus::mmSPI_SHADER_PGM_CHKSUM_HS + 1),
    },
    {
        (mmSPI_SHADER_PGM_LO_HS - PERSISTENT_SPACE_START),                      // 0x2D08 - 0x2D2B
        (Gfx09::mmSPI_SHADER_USER_DATA_LS_31 - mmSPI_SHADER_PGM_LO_HS + 1),
    },
};
constexpr uint32 Gfx9NumShShadowRangesRaven2 = static_cast<uint32>(Util::ArrayLen(Gfx9ShShadowRangeRaven2));

// Defines the set of ranges of CS SH registers we shadow when mid command buffer preemption is enabled.
const RegisterRange Gfx9CsShShadowRangeRaven2[] =
{
    {
        (mmCOMPUTE_START_X - PERSISTENT_SPACE_START),                           // 0x2E04 - 0x2E09
        (mmCOMPUTE_NUM_THREAD_Z - mmCOMPUTE_START_X + 1),
    },
    {
        (mmCOMPUTE_PERFCOUNT_ENABLE - PERSISTENT_SPACE_START),                  // 0x2E0B - 0x2EOD
        (mmCOMPUTE_PGM_HI - mmCOMPUTE_PERFCOUNT_ENABLE + 1),
    },
    {
        (mmCOMPUTE_PGM_RSRC1 - PERSISTENT_SPACE_START),                         // 0x2E12 - 0x2E13
        (mmCOMPUTE_PGM_RSRC2 - mmCOMPUTE_PGM_RSRC1 + 1),
    },
    {
        (mmCOMPUTE_RESOURCE_LIMITS - PERSISTENT_SPACE_START),                   // 0x2E15
        1,
    },
    {
        (mmCOMPUTE_TMPRING_SIZE - PERSISTENT_SPACE_START),                      // 0x2E18
        1,
    },
    {
        (mmCOMPUTE_THREAD_TRACE_ENABLE - PERSISTENT_SPACE_START),               // 0x2E1E
        1,
    },
    {
        (Gfx09_1x::mmCOMPUTE_SHADER_CHKSUM - PERSISTENT_SPACE_START),           // 0x2E2A
        1
    },
    {
        (mmCOMPUTE_USER_DATA_0 - PERSISTENT_SPACE_START),                       // 0x2E40 - 0x2E4F
        (mmCOMPUTE_USER_DATA_15 - mmCOMPUTE_USER_DATA_0 + 1),
    },
};
constexpr uint32 Gfx9NumCsShShadowRangesRaven2 = static_cast<uint32>(Util::ArrayLen(Gfx9CsShShadowRangeRaven2));

#if PAL_ENABLE_PRINTS_ASSERTS
// Defines the set of ranges of registers which cannot be shadowed for various reasons. Gfx6/7 have their own lists
// too, but we don't need to include them since we never enable state-shadowing for pre-Gfx8 GPU's.
const RegisterRange Gfx90NonShadowedRanges[] =
{
    {
        mmVGT_DMA_PRIMITIVE_TYPE,
        mmVGT_DMA_LS_HS_CONFIG - mmVGT_DMA_PRIMITIVE_TYPE + 1
    },
    {
        mmCOMPUTE_VMID,
        1
    },
    {
        mmSPI_RESOURCE_RESERVE_CU_0,
        1
    },
    {
        Gfx09_10::mmSPI_RESOURCE_RESERVE_EN_CU_0,
        1
    },
    // mmVGT_INDEX_TYPE and mmVGT_DMA_INDEX_TYPE are a special case and neither of these should be shadowed.
    {
        mmVGT_DMA_INDEX_TYPE,
        1
    },
    {
        mmVGT_STRMOUT_BUFFER_OFFSET_0,
        1
    },
    {
        mmVGT_STRMOUT_BUFFER_OFFSET_1,
        1
    },
    {
        mmVGT_STRMOUT_BUFFER_OFFSET_2,
        1
    },
    {
        mmVGT_STRMOUT_BUFFER_OFFSET_3,
        1
    },
    {
        mmVGT_DMA_NUM_INSTANCES,
        1
    },
    {
        mmCP_NUM_PRIM_WRITTEN_COUNT0_LO,
        mmCP_NUM_PRIM_NEEDED_COUNT3_HI - mmCP_NUM_PRIM_WRITTEN_COUNT0_LO + 1
    },
    {
        mmCP_VGT_IAVERT_COUNT_LO,
        mmCP_SC_PSINVOC_COUNT0_HI - mmCP_VGT_IAVERT_COUNT_LO + 1
    },
    {
        mmCP_VGT_CSINVOC_COUNT_LO,
        mmCP_VGT_CSINVOC_COUNT_HI - mmCP_VGT_CSINVOC_COUNT_LO + 1
    },
    {
        mmGRBM_GFX_INDEX,
        1
    },
    {
        mmVGT_INDEX_TYPE,
        mmVGT_STRMOUT_BUFFER_FILLED_SIZE_3 - mmVGT_INDEX_TYPE + 1
    },
    {
        mmDB_OCCLUSION_COUNT0_LOW,
        mmDB_OCCLUSION_COUNT3_HI - mmDB_OCCLUSION_COUNT0_LOW + 1
    },
    {
        NotGfx10::mmSPI_CONFIG_CNTL,
        1
    },
    // SQ thread trace registers are always not shadowed.
    {
        Gfx09::mmSQ_THREAD_TRACE_BASE,
        Gfx09::mmSQ_THREAD_TRACE_HIWATER - Gfx09::mmSQ_THREAD_TRACE_BASE + 1
    },
    {
        mmSQ_THREAD_TRACE_USERDATA_0,
        mmSQ_THREAD_TRACE_USERDATA_3 - mmSQ_THREAD_TRACE_USERDATA_0 + 1
    },
    // The 6 pairs below are written in preamble when CU reservation is enabled.
    {
        mmSPI_SHADER_PGM_RSRC3_HS,
        1
    },
    {
        mmSPI_SHADER_PGM_RSRC3_GS,
        1
    },
    {
        mmSPI_SHADER_PGM_RSRC3_VS,
        1
    },
    {
        mmSPI_SHADER_PGM_RSRC3_PS,
        1
    },
    {
        mmCOMPUTE_STATIC_THREAD_MGMT_SE0,
        mmCOMPUTE_STATIC_THREAD_MGMT_SE1 - mmCOMPUTE_STATIC_THREAD_MGMT_SE0 + 1
    },
    {
        mmCOMPUTE_STATIC_THREAD_MGMT_SE2,
        mmCOMPUTE_STATIC_THREAD_MGMT_SE3 - mmCOMPUTE_STATIC_THREAD_MGMT_SE2 + 1
    },
    // Perf counter registers are always not shadowed. Most of them are in the perf register space but some legacy
    // registers are still outside of it. The SPM registers are in the perf range as well.
    {
        UserConfigRegPerfStart,
        UserConfigRegPerfEnd - UserConfigRegPerfStart + 1
    },
    {
        Gfx09::mmATC_PERFCOUNTER0_CFG,
        Gfx09::mmATC_PERFCOUNTER_HI - Gfx09::mmATC_PERFCOUNTER0_CFG + 1
    },
    // The "Vega" RPB offsets work for all gfx9.0 ASICs.
    {
        Vega::mmRPB_PERFCOUNTER_LO,
        Vega::mmRPB_PERFCOUNTER_RSLT_CNTL - Vega::mmRPB_PERFCOUNTER_LO + 1
    },
    {
        Gfx09::mmSDMA0_PERFMON_CNTL,
        Gfx09::mmSDMA0_PERFCOUNTER_TAG_DELAY_RANGE - Gfx09::mmSDMA0_PERFMON_CNTL + 1
    },
    // Raven family ASICs don't have SDMA1 but that's OK because the HW didn't reuse these register offsets.
    {
        Vega::mmSDMA1_PERFMON_CNTL,
        Vega::mmSDMA1_PERFCOUNTER_TAG_DELAY_RANGE - Vega::mmSDMA1_PERFMON_CNTL + 1
    },
    {
        Gfx09_0::mmGCEA_PERFCOUNTER0_CFG,
        Gfx09_0::mmGCEA_PERFCOUNTER_RSLT_CNTL - Gfx09_0::mmGCEA_PERFCOUNTER0_CFG + 1
    },
};
constexpr uint32 Gfx90NumNonShadowedRanges = static_cast<uint32>(Util::ArrayLen(Gfx90NonShadowedRanges));

const RegisterRange Gfx91NonShadowedRanges[] =
{
    {
        mmVGT_DMA_PRIMITIVE_TYPE,
        mmVGT_DMA_LS_HS_CONFIG - mmVGT_DMA_PRIMITIVE_TYPE + 1
    },
    {
        mmCOMPUTE_VMID,
        1
    },
    {
        mmSPI_RESOURCE_RESERVE_CU_0,
        1
    },
    {
        Gfx09_10::mmSPI_RESOURCE_RESERVE_EN_CU_0,
        1
    },
    // mmVGT_INDEX_TYPE and mmVGT_DMA_INDEX_TYPE are a special case and neither of these should be shadowed.
    {
        mmVGT_DMA_INDEX_TYPE,
        1
    },
    {
        mmVGT_STRMOUT_BUFFER_OFFSET_0,
        1
    },
    {
        mmVGT_STRMOUT_BUFFER_OFFSET_1,
        1
    },
    {
        mmVGT_STRMOUT_BUFFER_OFFSET_2,
        1
    },
    {
        mmVGT_STRMOUT_BUFFER_OFFSET_3,
        1
    },
    {
        mmVGT_DMA_NUM_INSTANCES,
        1
    },
    {
        mmCP_NUM_PRIM_WRITTEN_COUNT0_LO,
        mmCP_NUM_PRIM_NEEDED_COUNT3_HI - mmCP_NUM_PRIM_WRITTEN_COUNT0_LO + 1
    },
    {
        mmCP_VGT_IAVERT_COUNT_LO,
        mmCP_SC_PSINVOC_COUNT0_HI - mmCP_VGT_IAVERT_COUNT_LO + 1
    },
    {
        mmCP_VGT_CSINVOC_COUNT_LO,
        mmCP_VGT_CSINVOC_COUNT_HI - mmCP_VGT_CSINVOC_COUNT_LO + 1
    },
    {
        mmGRBM_GFX_INDEX,
        1
    },
    {
        mmVGT_INDEX_TYPE,
        mmVGT_STRMOUT_BUFFER_FILLED_SIZE_3 - mmVGT_INDEX_TYPE + 1
    },
    {
        mmDB_OCCLUSION_COUNT0_LOW,
        mmDB_OCCLUSION_COUNT3_HI - mmDB_OCCLUSION_COUNT0_LOW + 1
    },
    {
        NotGfx10::mmSPI_CONFIG_CNTL,
        1
    },
    // SQ thread trace registers are always not shadowed.
    {
        Gfx09::mmSQ_THREAD_TRACE_BASE,
        Gfx09::mmSQ_THREAD_TRACE_HIWATER - Gfx09::mmSQ_THREAD_TRACE_BASE + 1
    },
    {
        mmSQ_THREAD_TRACE_USERDATA_0,
        mmSQ_THREAD_TRACE_USERDATA_3 - mmSQ_THREAD_TRACE_USERDATA_0 + 1
    },
    // The 6 pairs below are written in preamble when CU reservation is enabled.
    {
        mmSPI_SHADER_PGM_RSRC3_HS,
        1
    },
    {
        mmSPI_SHADER_PGM_RSRC3_GS,
        1
    },
    {
        mmSPI_SHADER_PGM_RSRC3_VS,
        1
    },
    {
        mmSPI_SHADER_PGM_RSRC3_PS,
        1
    },
    {
        mmCOMPUTE_STATIC_THREAD_MGMT_SE0,
        mmCOMPUTE_STATIC_THREAD_MGMT_SE1 - mmCOMPUTE_STATIC_THREAD_MGMT_SE0 + 1
    },
    {
        mmCOMPUTE_STATIC_THREAD_MGMT_SE2,
        mmCOMPUTE_STATIC_THREAD_MGMT_SE3 - mmCOMPUTE_STATIC_THREAD_MGMT_SE2 + 1
    },
    // Perf counter registers are always not shadowed. Most of them are in the perf register space but some legacy
    // registers are still outside of it. The SPM registers are in the perf range as well.
    {
        UserConfigRegPerfStart,
        UserConfigRegPerfEnd - UserConfigRegPerfStart + 1
    },
    {
        Gfx09::mmATC_PERFCOUNTER0_CFG,
        Gfx09::mmATC_PERFCOUNTER_HI - Gfx09::mmATC_PERFCOUNTER0_CFG + 1
    },
    {
        Vega::mmRPB_PERFCOUNTER_LO,
        // Renoir moved these up one slot. We don't have a Renoir-specific table so don't shadow the union of both
        // RPB perfcounter ranges. We don't use the registers immediately before and after the perfcounters so it's
        // safe that we're including one extra register.
        Rn::mmRPB_PERFCOUNTER_RSLT_CNTL - Vega::mmRPB_PERFCOUNTER_LO + 1
    },
    {
        Gfx09::mmSDMA0_PERFMON_CNTL,
        Gfx09::mmSDMA0_PERFCOUNTER_TAG_DELAY_RANGE - Gfx09::mmSDMA0_PERFMON_CNTL + 1
    },
    // Raven family ASICs don't have SDMA1 but that's OK because the HW didn't reuse these register offsets.
    {
        Vega::mmSDMA1_PERFMON_CNTL,
        Vega::mmSDMA1_PERFCOUNTER_TAG_DELAY_RANGE - Vega::mmSDMA1_PERFMON_CNTL + 1
    },
    {
        Gfx09_1x::mmGCEA_PERFCOUNTER0_CFG,
        Gfx09_1x::mmGCEA_PERFCOUNTER_RSLT_CNTL - Gfx09_1x::mmGCEA_PERFCOUNTER0_CFG + 1
    },
};
constexpr uint32 Gfx91NumNonShadowedRanges = static_cast<uint32>(Util::ArrayLen(Gfx91NonShadowedRanges));

#endif

constexpr uint32 Gfx10NumShShadowRanges    = static_cast<uint32>(Util::ArrayLen(Gfx10ShShadowRange));
constexpr uint32 Gfx10NumCsShShadowRanges  = static_cast<uint32>(Util::ArrayLen(Gfx10CsShShadowRange));

constexpr uint32 Gfx10NumUserConfigShadowRanges = Util::Max(Nv10NumUserConfigShadowRanges
                                                            , 0u);
constexpr uint32 Gfx10NumContextShadowRanges    = Util::Max(Nv10NumContextShadowRanges
                                                            , 0u);

constexpr uint32 MaxNumUserConfigRanges  = Util::Max(Gfx9NumUserConfigShadowRanges, Gfx10NumUserConfigShadowRanges);
constexpr uint32 MaxNumContextRanges     = Util::Max(Gfx9NumContextShadowRanges,    Gfx10NumContextShadowRanges);
constexpr uint32 MaxNumShRanges          = Util::Max(Gfx9NumShShadowRanges,         Gfx10NumShShadowRanges);
constexpr uint32 MaxNumCsShRanges        = Util::Max(Gfx9NumCsShShadowRanges,       Gfx10NumCsShShadowRanges);
} // Gfx9
} // Pal
