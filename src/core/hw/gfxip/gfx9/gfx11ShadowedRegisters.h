/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

namespace Pal
{
namespace Gfx9
{

// Defines the set of ranges of GFX SH registers we shadow when mid command buffer preemption is enabled.
const RegisterRange Gfx11ShShadowRange[] =
{
    {
        (Apu09_1xPlus::mmSPI_SHADER_PGM_CHKSUM_PS - PERSISTENT_SPACE_START),                          // 0x2C06
        1
    },
    {
        (mmSPI_SHADER_PGM_LO_PS - PERSISTENT_SPACE_START),                                            // 0x2C08 - 0x2C2B
        (mmSPI_SHADER_USER_DATA_PS_31 - mmSPI_SHADER_PGM_LO_PS + 1),
    },
    {
        (Gfx10Plus::mmSPI_SHADER_USER_ACCUM_PS_0 - PERSISTENT_SPACE_START),                           // 0x2C32 - 0x2C35
        (Gfx10Plus::mmSPI_SHADER_USER_ACCUM_PS_3 - Gfx10Plus::mmSPI_SHADER_USER_ACCUM_PS_0 + 1),
    },
    {
        (Gfx10Plus::mmSPI_SHADER_PGM_LO_ES - PERSISTENT_SPACE_START),                                 // 0x2CC8 - 0x2CC9
        (Gfx10Plus::mmSPI_SHADER_PGM_HI_ES - Gfx10Plus::mmSPI_SHADER_PGM_LO_ES + 1),
    },
    {
        (Gfx10Plus::mmSPI_SHADER_PGM_LO_LS - PERSISTENT_SPACE_START),                                 // 0x2D48 - 0x2D49
        (Gfx10Plus::mmSPI_SHADER_PGM_HI_LS - Gfx10Plus::mmSPI_SHADER_PGM_LO_LS + 1),
    },
    {
        (Apu09_1xPlus::mmSPI_SHADER_PGM_CHKSUM_GS - PERSISTENT_SPACE_START),                          // 0x2C80
        1,
    },
    {
        (mmSPI_SHADER_PGM_LO_GS - PERSISTENT_SPACE_START),                                            // 0x2C88 - 0x2CAD
        (Gfx11::mmSPI_SHADER_GS_MESHLET_EXP_ALLOC - mmSPI_SHADER_PGM_LO_GS + 1),
    },
    {
        (Gfx10Plus::mmSPI_SHADER_USER_ACCUM_ESGS_0 - PERSISTENT_SPACE_START),                         // 0x2CB2 - 0x2CB5
        (Gfx10Plus::mmSPI_SHADER_USER_ACCUM_ESGS_3 - Gfx10Plus::mmSPI_SHADER_USER_ACCUM_ESGS_0 + 1),
    },
    {
        (Apu09_1xPlus::mmSPI_SHADER_PGM_CHKSUM_HS - PERSISTENT_SPACE_START),                          // 0x2D00
        1,
    },
    {
        (mmSPI_SHADER_PGM_LO_HS - PERSISTENT_SPACE_START),                                            // 0x2D08 - 0x2D2B
        (Gfx10Plus::mmSPI_SHADER_USER_DATA_HS_31 - mmSPI_SHADER_PGM_LO_HS + 1),
    },
    {
        (Gfx10Plus::mmSPI_SHADER_USER_ACCUM_LSHS_0 - PERSISTENT_SPACE_START),                         // 0x2D32 - 0x2D35
        (Gfx10Plus::mmSPI_SHADER_USER_ACCUM_LSHS_3 - Gfx10Plus::mmSPI_SHADER_USER_ACCUM_LSHS_0 + 1),
    },
    {
        (Gfx10Plus::mmSPI_SHADER_REQ_CTRL_PS - PERSISTENT_SPACE_START),                               // 0x2C30
        1,
    },
};

// Defines the set of ranges of CS SH registers we shadow when mid command buffer preemption is enabled.
const RegisterRange Gfx11CsShShadowRange[] =
{
    {
        (mmCOMPUTE_START_X - PERSISTENT_SPACE_START),                           // 0x2E04 - 0x2E09
        (mmCOMPUTE_NUM_THREAD_Z - mmCOMPUTE_START_X + 1),
    },
    {
        (mmCOMPUTE_PERFCOUNT_ENABLE - PERSISTENT_SPACE_START),                  // 0x2E0B - 0x2E0D
        (mmCOMPUTE_PGM_HI - mmCOMPUTE_PERFCOUNT_ENABLE + 1),
    },
    {
        (mmCOMPUTE_DISPATCH_SCRATCH_BASE_LO - PERSISTENT_SPACE_START),          // 0x2E10 - 0x2E13
        (mmCOMPUTE_PGM_RSRC2 - mmCOMPUTE_DISPATCH_SCRATCH_BASE_LO + 1),
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
        (Gfx10Plus::mmCOMPUTE_USER_ACCUM_0 - PERSISTENT_SPACE_START),               // 0x2E24 - 0x2E28
        (Gfx10Plus::mmCOMPUTE_PGM_RSRC3 - Gfx10Plus::mmCOMPUTE_USER_ACCUM_0 + 1),
    },
    {
        (Gfx10Plus::mmCOMPUTE_SHADER_CHKSUM - PERSISTENT_SPACE_START),              // 0x2E2A
        1
    },
    {
        (Gfx11::mmCOMPUTE_DISPATCH_INTERLEAVE - PERSISTENT_SPACE_START),            // 0x2E2F
        1,
    },
    {
        (mmCOMPUTE_USER_DATA_0 - PERSISTENT_SPACE_START),                       // 0x2E40 - 0x2E4F
        (mmCOMPUTE_USER_DATA_15 - mmCOMPUTE_USER_DATA_0 + 1),
    },
    {
        (Gfx10Plus::mmCOMPUTE_DISPATCH_TUNNEL - PERSISTENT_SPACE_START),            // 0x2E7D
        1,
    },
};

// Defines the set of ranges of context registers we shadow when mid command buffer preemption is enabled.
const RegisterRange Gfx11ContextShadowRange[] =
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
        (Gfx11::mmPA_SC_VRS_OVERRIDE_CNTL - CONTEXT_SPACE_START),               // 0xA0F4 - 0xA0F9
        (Gfx11::mmPA_SC_VRS_RATE_CACHE_CNTL - Gfx11::mmPA_SC_VRS_OVERRIDE_CNTL + 1),
    },
    {
        (Gfx11::mmPA_SC_VRS_RATE_BASE - CONTEXT_SPACE_START),                   // 0xA0FC - 0xA0FE
        (Gfx11::mmPA_SC_VRS_RATE_SIZE_XY - Gfx11::mmPA_SC_VRS_RATE_BASE + 1),
    },
    {
        (mmVGT_MULTI_PRIM_IB_RESET_INDX - CONTEXT_SPACE_START),                 // 0xA103 - 0xA186
        (mmPA_CL_UCP_5_W - mmVGT_MULTI_PRIM_IB_RESET_INDX + 1),
    },
    {
        (mmSPI_PS_INPUT_CNTL_0 - CONTEXT_SPACE_START),                          // 0xA191 - 0xA1BC
        (Gfx11::mmSPI_GFX_SCRATCH_BASE_HI - mmSPI_PS_INPUT_CNTL_0 + 1),
    },
    {
        (Gfx10Plus::mmSPI_SHADER_IDX_FORMAT - CONTEXT_SPACE_START),             // 0xA1C2 - 0xA1C5
        (mmSPI_SHADER_COL_FORMAT - Gfx10Plus::mmSPI_SHADER_IDX_FORMAT + 1),
    },
    {
        (Gfx103PlusExclusive::mmSX_PS_DOWNCONVERT_CONTROL - CONTEXT_SPACE_START),        // 0xA1D4 - 0xA1E7
        (mmCB_BLEND7_CONTROL - Gfx103PlusExclusive::mmSX_PS_DOWNCONVERT_CONTROL + 1),
    },
    {
        (mmPA_CL_POINT_X_RAD - CONTEXT_SPACE_START),                            // 0xA1F5 - 0xA1F8
        (mmPA_CL_POINT_CULL_RAD - mmPA_CL_POINT_X_RAD + 1),
    },
    {
        (Gfx10Plus::mmGE_MAX_OUTPUT_PER_SUBGROUP - CONTEXT_SPACE_START),        // 0xA1FF - 0xA212
        (Gfx103Plus::mmPA_CL_VRS_CNTL - Gfx10Plus::mmGE_MAX_OUTPUT_PER_SUBGROUP + 1),
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
        (mmPA_SC_MODE_CNTL_0 - CONTEXT_SPACE_START),                           // 0xA292 - 0xA29B
        (mmVGT_ENHANCE - mmPA_SC_MODE_CNTL_0 + 1),
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
        (mmVGT_DRAW_PAYLOAD_CNTL - CONTEXT_SPACE_START),                        // 0xA2A6
        1,
    },
    {
        (mmVGT_ESGS_RING_ITEMSIZE - CONTEXT_SPACE_START),                       // 0xA2AB - 0xA2B1
        (mmDB_SRESULTS_COMPARE_STATE1 - mmVGT_ESGS_RING_ITEMSIZE + 1),
    },
    {
        (mmVGT_STRMOUT_DRAW_OPAQUE_OFFSET - CONTEXT_SPACE_START),               // 0xA2CA - 0xA2CE
        (mmVGT_GS_MAX_VERT_OUT - mmVGT_STRMOUT_DRAW_OPAQUE_OFFSET + 1),
    },
    {
        (Gfx10Plus::mmGE_NGG_SUBGRP_CNTL - CONTEXT_SPACE_START),                // 0xA2D3 - 0xA2D6
        (mmVGT_LS_HS_CONFIG - Gfx10Plus::mmGE_NGG_SUBGRP_CNTL + 1),
    },
    {
        (mmVGT_TF_PARAM - CONTEXT_SPACE_START),                                 // 0xA2DB - 0xA2E4
        (mmVGT_GS_INSTANCE_CNT - mmVGT_TF_PARAM + 1),
    },
    {
        (mmPA_SC_CENTROID_PRIORITY_0 - CONTEXT_SPACE_START),                    // 0xA2F5 - 0xA3BF
        (Gfx104Plus::mmPA_SC_BINNER_CNTL_2 - mmPA_SC_CENTROID_PRIORITY_0 + 1),
    },
    {
        (mmCB_COLOR0_BASE - CONTEXT_SPACE_START),                               // 0xA318
        1,
    },
    {
        (mmCB_COLOR0_VIEW - CONTEXT_SPACE_START),                               // 0xA31B - 0xA31E
        (mmCB_COLOR0_DCC_CONTROL - mmCB_COLOR0_VIEW + 1),
    },
    {
        (mmCB_COLOR0_DCC_BASE - CONTEXT_SPACE_START),                           // 0xA325 - 0xA327
        (mmCB_COLOR1_BASE - mmCB_COLOR0_DCC_BASE + 1),
    },
    {
        (mmCB_COLOR1_VIEW - CONTEXT_SPACE_START),                               // 0xA32A - 0xA32D
        (mmCB_COLOR1_DCC_CONTROL - mmCB_COLOR1_VIEW + 1),
    },
    {
        (mmCB_COLOR1_DCC_BASE - CONTEXT_SPACE_START),                           // 0xA334 - 0xA336
        (mmCB_COLOR2_BASE - mmCB_COLOR1_DCC_BASE + 1),
    },
    {
        (mmCB_COLOR2_VIEW - CONTEXT_SPACE_START),                               // 0xA339 - 0xA33C
        (mmCB_COLOR2_DCC_CONTROL - mmCB_COLOR2_VIEW + 1),
    },
    {
        (mmCB_COLOR2_DCC_BASE - CONTEXT_SPACE_START),                           // 0xA343 - 0xA345
        (mmCB_COLOR3_BASE - mmCB_COLOR2_DCC_BASE + 1),
    },
    {
        (mmCB_COLOR3_VIEW - CONTEXT_SPACE_START),                               // 0xA348 - 0xA34B
        (mmCB_COLOR3_DCC_CONTROL - mmCB_COLOR3_VIEW + 1),
    },
    {
        (mmCB_COLOR3_DCC_BASE - CONTEXT_SPACE_START),                           // 0xA352 - 0xA354
        (mmCB_COLOR4_BASE - mmCB_COLOR3_DCC_BASE + 1),
    },
    {
        (mmCB_COLOR4_VIEW - CONTEXT_SPACE_START),                               // 0xA357 - 0xA35A
        (mmCB_COLOR4_DCC_CONTROL - mmCB_COLOR4_VIEW + 1),
    },
    {
        (mmCB_COLOR4_DCC_BASE - CONTEXT_SPACE_START),                           // 0xA361 - 0xA363
        (mmCB_COLOR5_BASE - mmCB_COLOR4_DCC_BASE + 1),
    },
    {
        (mmCB_COLOR5_VIEW - CONTEXT_SPACE_START),                               // 0xA366 - 0xA369
        (mmCB_COLOR5_DCC_CONTROL - mmCB_COLOR5_VIEW + 1),
    },
    {
        (mmCB_COLOR5_DCC_BASE - CONTEXT_SPACE_START),                           // 0xA370 - 0xA372
        (mmCB_COLOR6_BASE - mmCB_COLOR5_DCC_BASE + 1),
    },
    {
        (mmCB_COLOR6_VIEW - CONTEXT_SPACE_START),                               // 0xA375 - 0xA378
        (mmCB_COLOR6_DCC_CONTROL - mmCB_COLOR6_VIEW + 1),
    },
    {
        (mmCB_COLOR6_DCC_BASE - CONTEXT_SPACE_START),                           // 0xA37F - 0xA381
        (mmCB_COLOR7_BASE - mmCB_COLOR6_DCC_BASE + 1),
    },
    {
        (mmCB_COLOR7_VIEW - CONTEXT_SPACE_START),                               // 0xA384 - 0xA387
        (mmCB_COLOR7_DCC_CONTROL - mmCB_COLOR7_VIEW + 1),
    },
    {
        (mmCB_COLOR7_DCC_BASE - CONTEXT_SPACE_START),                           // 0xA38E - 0xA397
        (Gfx10Plus::mmCB_COLOR7_BASE_EXT - mmCB_COLOR7_DCC_BASE + 1),
    },
    {
        (Gfx10Plus::mmCB_COLOR0_DCC_BASE_EXT - CONTEXT_SPACE_START),            // 0xA3A8 - 0xA3BF
        (Gfx10Plus::mmCB_COLOR7_ATTRIB3 - Gfx10Plus::mmCB_COLOR0_DCC_BASE_EXT + 1),
    },
};

// Defines the set of ranges of context registers we shadow for emulation purposes.  For the most part,
// these are the registers used for read-modify-write operations (since the RMW packet accesses state
// shadowing instead of reading the actual register), but also includes the DB_*_CLEAR registers since
// those aren't always restored depending on the barrier states.
const RegisterRange Gfx11ContextEmuShadowRange[] =
{
    {
        (mmDB_COUNT_CONTROL - CONTEXT_SPACE_START),                             // 0xA001
        1,
    },
    {
        (mmDB_RENDER_OVERRIDE - CONTEXT_SPACE_START),                           // 0xA003
        1,
    },
    {
        (Gfx10Plus::mmDB_Z_INFO - CONTEXT_SPACE_START),                         // 0xA010
        1,
    },
    {
        (mmDB_STENCILREFMASK - CONTEXT_SPACE_START),                            // 0xA10C
        1,
    },
    {
        (mmDB_STENCILREFMASK_BF - CONTEXT_SPACE_START),                         // 0xA10D
        1,
    },
    {
        (mmPA_SU_SC_MODE_CNTL - CONTEXT_SPACE_START),                           // 0xA205
        1,
    },
    {
        (mmCB_COLOR0_INFO - CONTEXT_SPACE_START),                               // 0xA31C
        1,
    },
    {
        (mmCB_COLOR1_INFO - CONTEXT_SPACE_START),                               // 0xA32B
        1,
    },
    {
        (mmCB_COLOR2_INFO - CONTEXT_SPACE_START),                               // 0xA33A
        1,
    },
    {
        (mmCB_COLOR3_INFO - CONTEXT_SPACE_START),                               // 0xA349
        1,
    },
    {
        (mmCB_COLOR4_INFO - CONTEXT_SPACE_START),                               // 0xA358
        1,
    },
    {
        (mmCB_COLOR5_INFO - CONTEXT_SPACE_START),                               // 0xA367
        1,
    },
    {
        (mmCB_COLOR6_INFO - CONTEXT_SPACE_START),                               // 0xA376
        1,
    },
    {
        (mmCB_COLOR7_INFO - CONTEXT_SPACE_START),                               // 0xA385
        1,
    },
    {
        (mmDB_STENCIL_CLEAR - CONTEXT_SPACE_START),                             // 0xA00A
        2,
    },
};

// Defines the set of ranges of user-config registers we shadow when mid command buffer preemption is enabled.
const RegisterRange Gfx11UserConfigShadowRange[] =
{
    {
        (mmVGT_PRIMITIVE_TYPE - UCONFIG_SPACE_START),
        1,
    },
    {
        (Gfx11::mmVGT_GS_OUT_PRIM_TYPE - UCONFIG_SPACE_START),
        1,
    },
    {
        (Gfx10Plus::mmGE_MAX_VTX_INDX - UCONFIG_SPACE_START),
        1,
    },
    {
        (Gfx10Plus::mmGE_MIN_VTX_INDX - UCONFIG_SPACE_START),
        (Gfx10Plus::mmGE_MULTI_PRIM_IB_RESET_EN - Gfx10Plus::mmGE_MIN_VTX_INDX + 1),
    },
    {
        (mmVGT_NUM_INSTANCES - UCONFIG_SPACE_START),
        (NotGfx10::mmVGT_TF_MEMORY_BASE - mmVGT_NUM_INSTANCES + 1),
    },
    {
        (Gfx10Plus::mmGE_STEREO_CNTL - UCONFIG_SPACE_START),
        Gfx104Plus::mmVGT_TF_MEMORY_BASE_HI - Gfx10Plus::mmGE_STEREO_CNTL + 1,
    },
    {
        (Gfx10Plus::mmGE_CNTL - UCONFIG_SPACE_START),
        1,
    },
    {
        (mmVGT_INSTANCE_BASE_ID - UCONFIG_SPACE_START),
        1,
    },
    {
        (mmTA_CS_BC_BASE_ADDR - UCONFIG_SPACE_START),
        (mmTA_CS_BC_BASE_ADDR_HI - mmTA_CS_BC_BASE_ADDR + 1),
    },
    {
        (Gfx10Plus::mmGE_USER_VGPR_EN - UCONFIG_SPACE_START),
        (Gfx103Plus::mmGE_VRS_RATE - Gfx10Plus::mmGE_USER_VGPR_EN + 1),
    },
    {
        (Gfx11::mmSPI_GS_THROTTLE_CNTL1 - UCONFIG_SPACE_START),             // 0xC444 - 0xC447
        (Gfx11::mmSPI_ATTRIBUTE_RING_SIZE - Gfx11::mmSPI_GS_THROTTLE_CNTL1 + 1),
    },
};

#if PAL_ENABLE_PRINTS_ASSERTS
// Defines the set of ranges of registers which cannot be shadowed for various reasons.
const RegisterRange Gfx11NonShadowedRanges[] =
{
    // mmVGT_INDEX_TYPE and mmVGT_DMA_INDEX_TYPE are a special case and neither of these should be shadowed.
    {
        mmVGT_DMA_INDEX_TYPE,
        1
    },
    {
        mmVGT_INDEX_TYPE,
        1
    },
    {
        mmVGT_DMA_NUM_INSTANCES,
        1
    },
    {
        mmSPI_SHADER_PGM_RSRC3_PS,
        1
    },
    {
        Gfx10Plus::mmSPI_SHADER_PGM_RSRC4_PS,
        1
    },
    {
        mmSPI_SHADER_PGM_RSRC4_HS,
        1
    },
    {
        mmSPI_SHADER_PGM_RSRC4_GS,
        1
    },
    {
        mmCOMPUTE_STATIC_THREAD_MGMT_SE0, // 0x2E16 - 0x2E17
        mmCOMPUTE_STATIC_THREAD_MGMT_SE1 - mmCOMPUTE_STATIC_THREAD_MGMT_SE0 + 1
    },
    {
        mmCOMPUTE_STATIC_THREAD_MGMT_SE2, // 0x2E19 - 0x2E1A
        mmCOMPUTE_STATIC_THREAD_MGMT_SE3 - mmCOMPUTE_STATIC_THREAD_MGMT_SE2 + 1
    },
    {
        Gfx11::mmCOMPUTE_STATIC_THREAD_MGMT_SE4, // 0x2E2B - 0x2E2E
        Gfx11::mmCOMPUTE_STATIC_THREAD_MGMT_SE7 - Gfx11::mmCOMPUTE_STATIC_THREAD_MGMT_SE4 + 1
    },
    {
        mmGRBM_GFX_INDEX,
        1
    },
    {
        mmPA_SU_LINE_STIPPLE_VALUE,
        1
    },
    {
        mmPA_SC_LINE_STIPPLE_STATE,
        1
    },
    // SQ thread trace registers are always not shadowed.
    {
        Gfx104Plus::mmSQ_THREAD_TRACE_BUF0_BASE,
        Gfx104Plus::mmSQ_THREAD_TRACE_STATUS2 - Gfx104Plus::mmSQ_THREAD_TRACE_BUF0_BASE + 1
    },
    {
        mmSQ_THREAD_TRACE_USERDATA_0,
        Gfx10Plus::mmSQ_THREAD_TRACE_USERDATA_7 - mmSQ_THREAD_TRACE_USERDATA_0 + 1,
    },
    // Perf counter registers are always not shadowed. Most of them are in the perf register space but some legacy
    // registers are still outside of it. The SPM registers are in the perf range as well.
    {
        UserConfigRegPerfStart,
        UserConfigRegPerfEnd - UserConfigRegPerfStart + 1
    },
    {
        Gfx10Core::mmRPB_PERFCOUNTER_LO,
        Gfx10Core::mmRPB_PERFCOUNTER_RSLT_CNTL - Gfx10Core::mmRPB_PERFCOUNTER_LO + 1
    },
    {
        Gfx103CorePlus::mmSDMA0_PERFCOUNTER0_SELECT,
        Gfx103CorePlus::mmSDMA0_PERFCOUNTER1_SELECT1 - Gfx103CorePlus::mmSDMA0_PERFCOUNTER0_SELECT + 1
    },
#if PAL_BUILD_NAVI3X
    {
        Nv3x::mmSDMA1_PERFCOUNTER0_SELECT,
        Nv3x::mmSDMA1_PERFCOUNTER1_SELECT1 - Nv3x::mmSDMA1_PERFCOUNTER0_SELECT + 1
    },
    {
        Nv3x::mmSDMA1_PERFCNT_PERFCOUNTER_LO,
        Nv3x::mmSDMA1_PERFCOUNTER1_HI - Nv3x::mmSDMA1_PERFCNT_PERFCOUNTER_LO + 1
    },
#endif
    {
        Gfx103CorePlus::mmSDMA0_PERFCNT_PERFCOUNTER_LO,
        Gfx103CorePlus::mmSDMA0_PERFCOUNTER1_HI - Gfx103CorePlus::mmSDMA0_PERFCNT_PERFCOUNTER_LO + 1
    },
    {
        mmRLC_CGTT_MGCG_OVERRIDE,
        1
    },
    {
        NotGfx10::mmSPI_CONFIG_CNTL,
        1
    }
};

constexpr uint32 Gfx11NumNonShadowedRanges      = static_cast<uint32>(Util::ArrayLen(Gfx11NonShadowedRanges));
#endif

#if PAL_BUILD_GFX11
// Defines the set of ranges of GFX SH registers we want to initialize in per submit preamble.
const RegisterRange Gfx11CpRs64InitShRanges[] =
{
    {
        Apu09_1xPlus::mmSPI_SHADER_PGM_CHKSUM_PS,                          // 0x2C06
        1
    },
    {
        mmSPI_SHADER_PGM_LO_PS,                                            // 0x2C08 - 0x2C2B
        (mmSPI_SHADER_USER_DATA_PS_31 - mmSPI_SHADER_PGM_LO_PS + 1),
    },
    {
        Gfx10Plus::mmSPI_SHADER_USER_ACCUM_PS_0,                           // 0x2C32 - 0x2C35
        (Gfx10Plus::mmSPI_SHADER_USER_ACCUM_PS_3 - Gfx10Plus::mmSPI_SHADER_USER_ACCUM_PS_0 + 1),
    },
    {
        Gfx10Plus::mmSPI_SHADER_PGM_LO_ES,                                 // 0x2CC8 - 0x2CC9
        (Gfx10Plus::mmSPI_SHADER_PGM_HI_ES - Gfx10Plus::mmSPI_SHADER_PGM_LO_ES + 1),
    },
    {
        Gfx10Plus::mmSPI_SHADER_PGM_LO_LS,                                 // 0x2D48 - 0x2D49
        (Gfx10Plus::mmSPI_SHADER_PGM_HI_LS - Gfx10Plus::mmSPI_SHADER_PGM_LO_LS + 1),
    },
    {
        Apu09_1xPlus::mmSPI_SHADER_PGM_CHKSUM_GS,                          // 0x2C80
        1,
    },
    {
        mmSPI_SHADER_PGM_LO_GS,                                            // 0x2C88 - 0x2CAB
        (Gfx10Plus::mmSPI_SHADER_USER_DATA_GS_31 - mmSPI_SHADER_PGM_LO_GS + 1),
    },
    {
        Gfx10Plus::mmSPI_SHADER_USER_ACCUM_ESGS_0,                         // 0x2CB2 - 0x2CB5
        (Gfx10Plus::mmSPI_SHADER_USER_ACCUM_ESGS_3 - Gfx10Plus::mmSPI_SHADER_USER_ACCUM_ESGS_0 + 1),
    },
    {
        Apu09_1xPlus::mmSPI_SHADER_PGM_CHKSUM_HS,                          // 0x2D00
        1,
    },
    {
        mmSPI_SHADER_PGM_LO_HS,                                            // 0x2D08 - 0x2D2B
        (Gfx10Plus::mmSPI_SHADER_USER_DATA_HS_31 - mmSPI_SHADER_PGM_LO_HS + 1),
    },
    {
        Gfx10Plus::mmSPI_SHADER_USER_ACCUM_LSHS_0,                         // 0x2D32 - 0x2D35
        (Gfx10Plus::mmSPI_SHADER_USER_ACCUM_LSHS_3 - Gfx10Plus::mmSPI_SHADER_USER_ACCUM_LSHS_0 + 1),
    },
    {
        Gfx10Plus::mmSPI_SHADER_REQ_CTRL_PS,                               // 0x2C30
        1,
    },
};

constexpr uint32 Gfx11NumCpRs64InitShRanges = static_cast<uint32>(Util::ArrayLen(Gfx11CpRs64InitShRanges));

// Defines the set of ranges of Compute SH registers we want to initialize in per submit preamble.
const RegisterRange Gfx11CpRs64InitCsShRanges[] =
{
    {
        mmCOMPUTE_START_X,                           // 0x2E04 - 0x2E09
        (mmCOMPUTE_NUM_THREAD_Z - mmCOMPUTE_START_X + 1),
    },
    {
        mmCOMPUTE_PERFCOUNT_ENABLE,                  // 0x2E0B - 0x2E0D
        (mmCOMPUTE_PGM_HI - mmCOMPUTE_PERFCOUNT_ENABLE + 1),
    },
    {
        mmCOMPUTE_DISPATCH_SCRATCH_BASE_LO,          // 0x2E10 - 0x2E13
        (mmCOMPUTE_PGM_RSRC2 - mmCOMPUTE_DISPATCH_SCRATCH_BASE_LO + 1),
    },
    {
        mmCOMPUTE_RESOURCE_LIMITS,                   // 0x2E15
        1,
    },
    {
        mmCOMPUTE_TMPRING_SIZE,                      // 0x2E18
        1,
    },
    {
        mmCOMPUTE_THREAD_TRACE_ENABLE,               // 0x2E1E
        1,
    },
    {
        Gfx10Plus::mmCOMPUTE_USER_ACCUM_0,               // 0x2E24 - 0x2E28
        (Gfx10Plus::mmCOMPUTE_PGM_RSRC3 - Gfx10Plus::mmCOMPUTE_USER_ACCUM_0 + 1),
    },
    {
        Gfx10Plus::mmCOMPUTE_SHADER_CHKSUM,              // 0x2E2A
        1
    },
    {
        Gfx11::mmCOMPUTE_DISPATCH_INTERLEAVE,            // 0x2E2F
        1,
    },
    {
        mmCOMPUTE_USER_DATA_0,                       // 0x2E40 - 0x2E4F
        (mmCOMPUTE_USER_DATA_15 - mmCOMPUTE_USER_DATA_0 + 1),
    },
    {
        Gfx10Plus::mmCOMPUTE_DISPATCH_TUNNEL,            // 0x2E7D
        1,
    },
};

constexpr uint32 Gfx11NumCpRs64InitCsShRanges = static_cast<uint32>(Util::ArrayLen(Gfx11CpRs64InitCsShRanges));

// Defines the set of ranges of UserConfig registers we want to initialize in per submit preamble.
const RegisterRange Gfx11CpRs64InitUserConfigRanges[] =
{
    {
        mmVGT_PRIMITIVE_TYPE,
        1,
    },
    {
        Gfx11::mmVGT_GS_OUT_PRIM_TYPE,
        1,
    },
    {
        Gfx10Plus::mmGE_MAX_VTX_INDX,
        1,
    },
    {
        Gfx10Plus::mmGE_MIN_VTX_INDX,
        (Gfx10Plus::mmGE_MULTI_PRIM_IB_RESET_EN - Gfx10Plus::mmGE_MIN_VTX_INDX + 1),
    },
    {
        mmVGT_NUM_INSTANCES,
        (NotGfx10::mmVGT_TF_MEMORY_BASE - mmVGT_NUM_INSTANCES + 1),
    },
    {
        Gfx10Plus::mmGE_STEREO_CNTL,
        Gfx104Plus::mmVGT_TF_MEMORY_BASE_HI - Gfx10Plus::mmGE_STEREO_CNTL + 1,
    },
    {
        Gfx10Plus::mmGE_CNTL,
        1,
    },
    {
        mmVGT_INSTANCE_BASE_ID,
        1,
    },
    {
        mmTA_CS_BC_BASE_ADDR,
        (mmTA_CS_BC_BASE_ADDR_HI - mmTA_CS_BC_BASE_ADDR + 1),
    },
    {
        Gfx10Plus::mmGE_USER_VGPR_EN,
        (Gfx103Plus::mmGE_VRS_RATE - Gfx10Plus::mmGE_USER_VGPR_EN + 1),
    },
    {
        Gfx11::mmSPI_GS_THROTTLE_CNTL1,             // 0xC444 - 0xC447
        (Gfx11::mmSPI_ATTRIBUTE_RING_SIZE - Gfx11::mmSPI_GS_THROTTLE_CNTL1 + 1),
    },
};

constexpr uint32 Gfx11NumCpRs64InitUserConfigRanges
                                                = static_cast<uint32>(Util::ArrayLen(Gfx11CpRs64InitUserConfigRanges));

#endif

constexpr uint32 Gfx11NumUserConfigShadowRanges = static_cast<uint32>(Util::ArrayLen(Gfx11UserConfigShadowRange));
constexpr uint32 Gfx11NumContextShadowRanges    = static_cast<uint32>(Util::ArrayLen(Gfx11ContextShadowRange));
constexpr uint32 Gfx11NumContextEmuShadowRanges = static_cast<uint32>(Util::ArrayLen(Gfx11ContextEmuShadowRange));
constexpr uint32 Gfx11NumShShadowRanges         = static_cast<uint32>(Util::ArrayLen(Gfx11ShShadowRange));
constexpr uint32 Gfx11NumCsShShadowRanges       = static_cast<uint32>(Util::ArrayLen(Gfx11CsShShadowRange));

} // Gfx9
} // Pal
