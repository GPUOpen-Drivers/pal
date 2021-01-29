/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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

// Defines the set of ranges of context registers we shadow when mid command buffer preemption is enabled.
const RegisterRange Nv10ContextShadowRange[] =
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
        (mmVGT_MULTI_PRIM_IB_RESET_INDX - CONTEXT_SPACE_START),                 // 0xA103 - 0xA186
        (mmPA_CL_UCP_5_W - mmVGT_MULTI_PRIM_IB_RESET_INDX + 1),
    },
    {
        (mmSPI_PS_INPUT_CNTL_0 - CONTEXT_SPACE_START),                          // 0xA191 - 0xA1C5
        (mmSPI_SHADER_COL_FORMAT - mmSPI_PS_INPUT_CNTL_0 + 1),
    },
    {
        (mmSX_PS_DOWNCONVERT - CONTEXT_SPACE_START),                            // 0xA1D5 - 0xA1E7
        (mmCB_BLEND7_CONTROL - mmSX_PS_DOWNCONVERT + 1),
    },
    {
        (mmPA_CL_POINT_X_RAD - CONTEXT_SPACE_START),                            // 0xA1F5 - 0xA1F8
        (mmPA_CL_POINT_CULL_RAD - mmPA_CL_POINT_X_RAD + 1),
    },
    {
        (Gfx10Plus::mmGE_MAX_OUTPUT_PER_SUBGROUP - CONTEXT_SPACE_START),        // 0xA1FF - 0xA211
        (Gfx10Plus::mmPA_STATE_STEREO_X - Gfx10Plus::mmGE_MAX_OUTPUT_PER_SUBGROUP + 1),
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
        (Gfx09_10::mmVGT_GS_MODE - CONTEXT_SPACE_START),                        // 0xA290 - 0xA29B
        (Gfx09_10::mmVGT_GS_OUT_PRIM_TYPE - Gfx09_10::mmVGT_GS_MODE + 1),
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
        (mmVGT_DRAW_PAYLOAD_CNTL - CONTEXT_SPACE_START),                        // 0xA2A6 - 0xA2E6
        (Gfx09_10::mmVGT_STRMOUT_BUFFER_CONFIG - mmVGT_DRAW_PAYLOAD_CNTL + 1),
    },
    {
        (mmPA_SC_CENTROID_PRIORITY_0 - CONTEXT_SPACE_START),                    // 0xA2F5 - 0xA3BF
        (Gfx10Plus::mmCB_COLOR7_ATTRIB3 - mmPA_SC_CENTROID_PRIORITY_0 + 1),
    },
};

// Defines the set of ranges of user-config registers we shadow when mid command buffer preemption is enabled.
const RegisterRange Nv10UserConfigShadowRange[] =
{
    {
        (Gfx09_10::mmCP_STRMOUT_CNTL - UCONFIG_SPACE_START),
         1,
    },
    {
        (Gfx09_10::mmCP_COHER_START_DELAY - UCONFIG_SPACE_START),
         1,
    },
    {
        (Gfx101::mmVGT_GSVS_RING_SIZE_UMD - UCONFIG_SPACE_START),
        (mmVGT_PRIMITIVE_TYPE - Gfx101::mmVGT_GSVS_RING_SIZE_UMD + 1),
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
        (Gfx101::mmVGT_TF_MEMORY_BASE_UMD - mmVGT_NUM_INSTANCES + 1),
    },
    {
        (Gfx10Plus::mmGE_STEREO_CNTL - UCONFIG_SPACE_START),
         Gfx101::mmVGT_TF_MEMORY_BASE_HI_UMD - Gfx10Plus::mmGE_STEREO_CNTL + 1,
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
        (Gfx10Plus::mmGE_USER_VGPR_EN - UCONFIG_SPACE_START),
        1,
    },
    {
        (mmTA_CS_BC_BASE_ADDR - UCONFIG_SPACE_START),
        (mmTA_CS_BC_BASE_ADDR_HI - mmTA_CS_BC_BASE_ADDR + 1),
    },
};

#if PAL_ENABLE_PRINTS_ASSERTS
// Defines the set of ranges of registers which cannot be shadowed for various reasons.
const RegisterRange Navi10NonShadowedRanges[] =
{
    {
        Gfx09_10::mmVGT_DMA_PRIMITIVE_TYPE,
        Gfx09_10::mmVGT_DMA_LS_HS_CONFIG - Gfx09_10::mmVGT_DMA_PRIMITIVE_TYPE + 1
    },
    // mmVGT_INDEX_TYPE and mmVGT_DMA_INDEX_TYPE are a special case and neither of these should be shadowed.
    {
        mmVGT_DMA_INDEX_TYPE,
        1
    },
    {
        mmVGT_INDEX_TYPE,
        Gfx09_10::mmVGT_STRMOUT_BUFFER_FILLED_SIZE_3 - mmVGT_INDEX_TYPE + 1
    },
    {
        mmVGT_DMA_NUM_INSTANCES,
        1
    },
    {
        Gfx09_10::mmSPI_SHADER_PGM_RSRC3_VS,
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
        Gfx10::mmSPI_SHADER_PGM_RSRC4_VS,
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
        mmSPI_SHADER_PGM_RSRC3_GS,
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
    {
        mmGRBM_GFX_INDEX,
        1
    },
    {
        Gfx101::mmSPI_CONFIG_CNTL_REMAP,
        1
    },
    // SQ thread trace registers are always not shadowed.
    {
        Gfx10Core::mmSQ_THREAD_TRACE_BUF0_BASE,
        Gfx10Core::mmSQ_THREAD_TRACE_HP3D_MARKER_CNTR - Gfx10Core::mmSQ_THREAD_TRACE_BUF0_BASE + 1
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
        Gfx101::mmATC_PERFCOUNTER0_CFG,
        Gfx101::mmATC_PERFCOUNTER_HI - Gfx101::mmATC_PERFCOUNTER0_CFG + 1
    },
    {
        Gfx10Core::mmRPB_PERFCOUNTER_LO,
        Gfx10Core::mmRPB_PERFCOUNTER_RSLT_CNTL - Gfx10Core::mmRPB_PERFCOUNTER_LO + 1
    },
    {
        Oss50::mmSDMA0_PERFCOUNTER0_SELECT,
        Oss50::mmSDMA0_PERFCOUNTER1_HI - Oss50::mmSDMA0_PERFCOUNTER0_SELECT + 1
    },
    {
        Oss50::mmSDMA1_PERFCOUNTER0_SELECT,
        Oss50::mmSDMA1_PERFCOUNTER1_HI - Oss50::mmSDMA1_PERFCOUNTER0_SELECT + 1
    },
    {
        Gfx101::mmGCEA_PERFCOUNTER_LO,
        Gfx101::mmGCEA_PERFCOUNTER_RSLT_CNTL - Gfx101::mmGCEA_PERFCOUNTER_LO + 1
    },
    {
        Gfx101::mmGUS_PERFCOUNTER_LO,
        Gfx101::mmGUS_PERFCOUNTER_RSLT_CNTL - Gfx101::mmGUS_PERFCOUNTER_LO + 1
    },
};

constexpr uint32 Navi10NumNonShadowedRanges      = static_cast<uint32>(Util::ArrayLen(Navi10NonShadowedRanges));

#endif

constexpr uint32 Nv10NumUserConfigShadowRanges   = static_cast<uint32>(Util::ArrayLen(Nv10UserConfigShadowRange));
constexpr uint32 Nv10NumContextShadowRanges      = static_cast<uint32>(Util::ArrayLen(Nv10ContextShadowRange));

} // Gfx9
} // Pal
