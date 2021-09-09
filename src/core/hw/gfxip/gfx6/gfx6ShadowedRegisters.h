/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/gfx6/gfx6Chip.h"

namespace Pal
{
namespace Gfx6
{

// Defines the set of ranges of user-config registers we shadow when mid command buffer preemption is enabled. These
// registers are only valid on GFX7+ GPU's. (Preemption is only supported on GFX8+ GPU's.)
const RegisterRange UserConfigShadowRangeGfx7[] =
{
    {
        (mmCP_STRMOUT_CNTL__CI__VI - UCONFIG_SPACE_START__CI__VI),              // 0xC03F
        1,
    },
    {
        (mmVGT_ESGS_RING_SIZE__CI__VI - UCONFIG_SPACE_START__CI__VI),           // 0xC240 - 0xC242
        (mmVGT_PRIMITIVE_TYPE__CI__VI - mmVGT_ESGS_RING_SIZE__CI__VI + 1),
    },
    {
        (mmVGT_NUM_INSTANCES__CI__VI - UCONFIG_SPACE_START__CI__VI),            // 0xC24D - 0xC250
        (mmVGT_TF_MEMORY_BASE__CI__VI - mmVGT_NUM_INSTANCES__CI__VI + 1),
    },
    {
        (mmTA_CS_BC_BASE_ADDR__CI__VI - UCONFIG_SPACE_START__CI__VI),           //0xC380 - 0xC381
        (mmTA_CS_BC_BASE_ADDR_HI__CI__VI - mmTA_CS_BC_BASE_ADDR__CI__VI + 1),
    }
};
constexpr uint32 NumUserConfigShadowRangesGfx7 = static_cast<uint32>(Util::ArrayLen(UserConfigShadowRangeGfx7));

// Defines the set of ranges of context registers we shadow when mid command buffer
// preemption is enabled.(Preemption is only supported on GFX8+ GPU's.)
const RegisterRange ContextShadowRange[] =
{
    {
        (mmDB_RENDER_CONTROL - CONTEXT_SPACE_START),                            // 0xA000 - 0xA021
        (mmTA_BC_BASE_ADDR_HI__CI__VI - mmDB_RENDER_CONTROL + 1),
    },
    {
        (mmPA_SC_WINDOW_OFFSET - CONTEXT_SPACE_START),                          // 0xA080 - 0xA0D5
        (mmPA_SC_RASTER_CONFIG_1__CI__VI - mmPA_SC_WINDOW_OFFSET + 1),
    },
    {
        (mmCOHER_DEST_BASE_2 - CONTEXT_SPACE_START),                            // 0xA07E - 0xA07F
        (mmCOHER_DEST_BASE_3 - mmCOHER_DEST_BASE_2 + 1),
    },
    {
        (mmVGT_MAX_VTX_INDX - CONTEXT_SPACE_START),                             // 0xA100 - 0xA186
        (mmPA_CL_UCP_5_W - mmVGT_MAX_VTX_INDX + 1),
    },
    {
        (mmSPI_PS_INPUT_CNTL_0 - CONTEXT_SPACE_START),                          // 0xA191 - 0xA1C5
        (mmSPI_SHADER_COL_FORMAT - mmSPI_PS_INPUT_CNTL_0 + 1),
    },
    {
        (mmSX_PS_DOWNCONVERT__VI - CONTEXT_SPACE_START),                        // 0xA1D5 - 0xA1E7
        (mmCB_BLEND7_CONTROL - mmSX_PS_DOWNCONVERT__VI + 1),
    },
    {
        (mmDB_DEPTH_CONTROL - CONTEXT_SPACE_START),                             // 0xA200 - 0xA20A
        (mmPA_SU_LINE_STIPPLE_SCALE - mmDB_DEPTH_CONTROL + 1),
    },
    {
        (mmPA_SU_SMALL_PRIM_FILTER_CNTL__VI - CONTEXT_SPACE_START),             // 0xA20C
         1,
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
        (mmVGT_MULTI_PRIM_IB_RESET_EN - CONTEXT_SPACE_START),                   // 0xA2A5 - 0xA2B5
        (mmVGT_STRMOUT_VTX_STRIDE_0 - mmVGT_MULTI_PRIM_IB_RESET_EN + 1),
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
        (mmPA_SC_CENTROID_PRIORITY_0 - CONTEXT_SPACE_START),                    // 0xA2F5 - 0xA38E
        (mmCB_COLOR7_DCC_BASE__VI - mmPA_SC_CENTROID_PRIORITY_0 + 1),
    },
};
constexpr uint32 NumContextShadowRanges = static_cast<uint32>(Util::ArrayLen(ContextShadowRange));

// Special range with just PA_SC_RASTER_CONFIG removed from ContextShadowRange. This register should only be shadowed
// when RB reconfiguration is not enabled.
const RegisterRange ContextShadowRangeRbReconfig[] =
{
    {
        (mmDB_RENDER_CONTROL - CONTEXT_SPACE_START),                            // 0xA000 - 0xA021
        (mmTA_BC_BASE_ADDR_HI__CI__VI - mmDB_RENDER_CONTROL + 1),
    },
    {
        (mmPA_SC_WINDOW_OFFSET - CONTEXT_SPACE_START),                          // 0xA080 - 0xA0D3
        (mmPA_SC_VPORT_ZMAX_15 - mmPA_SC_WINDOW_OFFSET + 1),
    },
    {
        (mmPA_SC_RASTER_CONFIG_1__CI__VI - CONTEXT_SPACE_START),                // 0xA0D5 - 0xA0D5
        1,
    },
    {
        (mmCOHER_DEST_BASE_2 - CONTEXT_SPACE_START),                            // 0xA07E - 0xA07F
        (mmCOHER_DEST_BASE_3 - mmCOHER_DEST_BASE_2 + 1),
    },
    {
        (mmVGT_MAX_VTX_INDX - CONTEXT_SPACE_START),                             // 0xA100 - 0xA186
        (mmPA_CL_UCP_5_W - mmVGT_MAX_VTX_INDX + 1),
    },
    {
        (mmSPI_PS_INPUT_CNTL_0 - CONTEXT_SPACE_START),                          // 0xA191 - 0xA1C5
        (mmSPI_SHADER_COL_FORMAT - mmSPI_PS_INPUT_CNTL_0 + 1),
    },
    {
        (mmSX_PS_DOWNCONVERT__VI - CONTEXT_SPACE_START),                        // 0xA1D5 - 0xA1E7
        (mmCB_BLEND7_CONTROL - mmSX_PS_DOWNCONVERT__VI + 1),
    },
    {
        (mmDB_DEPTH_CONTROL - CONTEXT_SPACE_START),                             // 0xA200 - 0xA208
        (mmPA_CL_NANINF_CNTL - mmDB_DEPTH_CONTROL + 1),
    },
    {
        (mmPA_SU_SMALL_PRIM_FILTER_CNTL__VI - CONTEXT_SPACE_START),             // 0xA20C
         1,
    },
    {
        (mmPA_SU_POINT_SIZE - CONTEXT_SPACE_START),                             // 0xA280 - 0xA282
        (mmPA_SU_LINE_CNTL - mmPA_SU_POINT_SIZE + 1),
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
        (mmVGT_MULTI_PRIM_IB_RESET_EN - CONTEXT_SPACE_START),                   // 0xA2A5 - 0xA2B5
        (mmVGT_STRMOUT_VTX_STRIDE_0 - mmVGT_MULTI_PRIM_IB_RESET_EN + 1),
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
        (mmPA_SC_CENTROID_PRIORITY_0 - CONTEXT_SPACE_START),                    // 0xA2F5 - 0xA38E
        (mmCB_COLOR7_DCC_BASE__VI - mmPA_SC_CENTROID_PRIORITY_0 + 1),
    },
};
constexpr uint32 NumContextShadowRangesRbReconfig = static_cast<uint32>(Util::ArrayLen(ContextShadowRangeRbReconfig));

constexpr uint32 MaxNumContextShadowRanges = Util::Max(NumContextShadowRanges, NumContextShadowRangesRbReconfig);

// Defines the set of ranges of GFX SH registers we shadow when mid command buffer preemption is enabled. (Preemption
// is only supported on GFX8+ GPU's.)
const RegisterRange GfxShShadowRange[] =
{
    {
        (mmSPI_SHADER_TBA_LO_PS - PERSISTENT_SPACE_START),                      // 0x2C00 - 0x2C03
        (mmSPI_SHADER_TMA_HI_PS - mmSPI_SHADER_TBA_LO_PS + 1),
    },
    {
        (mmSPI_SHADER_PGM_LO_PS - PERSISTENT_SPACE_START),                      // 0x2C08 - 0x2C1B
        (mmSPI_SHADER_USER_DATA_PS_15 - mmSPI_SHADER_PGM_LO_PS + 1),
    },
    {
        (mmSPI_SHADER_TBA_LO_VS - PERSISTENT_SPACE_START),                      // 0x2C40 - 0x2C43
        (mmSPI_SHADER_TMA_HI_VS - mmSPI_SHADER_TBA_LO_VS + 1),
    },
    {
        (mmSPI_SHADER_LATE_ALLOC_VS__CI__VI - PERSISTENT_SPACE_START),          // 0x2C47 - 0x2C5B
        (mmSPI_SHADER_USER_DATA_VS_15 - mmSPI_SHADER_LATE_ALLOC_VS__CI__VI + 1),
    },
    {
        (mmSPI_SHADER_TBA_LO_GS - PERSISTENT_SPACE_START),                      // 0x2C80 - 0x2C83
        (mmSPI_SHADER_TMA_HI_GS - mmSPI_SHADER_TBA_LO_GS + 1),
    },
    {
        (mmSPI_SHADER_PGM_LO_GS - PERSISTENT_SPACE_START),                      // 0x2C88 - 0x2C9B
        (mmSPI_SHADER_USER_DATA_GS_15 - mmSPI_SHADER_PGM_LO_GS + 1),
    },
    {
        (mmSPI_SHADER_TBA_LO_ES - PERSISTENT_SPACE_START),                      // 0x2CC0 - 0x2CC3
        (mmSPI_SHADER_TMA_HI_ES - mmSPI_SHADER_TBA_LO_ES + 1),
    },
    {
        (mmSPI_SHADER_PGM_LO_ES - PERSISTENT_SPACE_START),                      // 0x2CC8 - 0x2CDB
        (mmSPI_SHADER_USER_DATA_ES_15 - mmSPI_SHADER_PGM_LO_ES + 1),
    },
    {
        (mmSPI_SHADER_TBA_LO_HS - PERSISTENT_SPACE_START),                      // 0x2D00 - 0x2D03
        (mmSPI_SHADER_TMA_HI_HS - mmSPI_SHADER_TBA_LO_HS + 1),
    },
    {
        (mmSPI_SHADER_PGM_RSRC3_HS__CI__VI - PERSISTENT_SPACE_START),           // 0x2D07 - 0x2D1B
        (mmSPI_SHADER_USER_DATA_HS_15 - mmSPI_SHADER_PGM_RSRC3_HS__CI__VI + 1),
    },
    {
        (mmSPI_SHADER_TBA_LO_LS - PERSISTENT_SPACE_START),                      // 0x2D40 - 0x2D43
        (mmSPI_SHADER_TMA_HI_LS - mmSPI_SHADER_TBA_LO_LS + 1),
    },
    {
        (mmSPI_SHADER_PGM_LO_LS - PERSISTENT_SPACE_START),                      // 0x2D48 - 0x2D5B
        (mmSPI_SHADER_USER_DATA_LS_15 - mmSPI_SHADER_PGM_LO_LS + 1),
    },
};
constexpr uint32 NumGfxShShadowRanges = static_cast<uint32>(Util::ArrayLen(GfxShShadowRange));

// Defines the set of ranges of CS SH registers we shadow when mid command buffer preemption is enabled. (Preemption
// is only supported on GFX8+ GPU's.)
const RegisterRange CsShShadowRange[] =
{
    {
        (mmCOMPUTE_START_X - PERSISTENT_SPACE_START),                           // 0x2E04 - 0x2E09
        (mmCOMPUTE_NUM_THREAD_Z - mmCOMPUTE_START_X + 1),
    },
    {
        (mmCOMPUTE_PERFCOUNT_ENABLE__CI__VI - PERSISTENT_SPACE_START),          // 0x2E0B - 0x2E13
        (mmCOMPUTE_PGM_RSRC2 - mmCOMPUTE_PERFCOUNT_ENABLE__CI__VI + 1),
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
        (mmCOMPUTE_USER_DATA_0 - PERSISTENT_SPACE_START),                       // 0x2E40 - 0x2E4F
        (mmCOMPUTE_USER_DATA_15 - mmCOMPUTE_USER_DATA_0 + 1),
    },
};
constexpr uint32 NumCsShShadowRanges = static_cast<uint32>(Util::ArrayLen(CsShShadowRange));

#if PAL_ENABLE_PRINTS_ASSERTS
// Defines the set of ranges of registers which cannot be shadowed for various reasons. Gfx6/7 have their own lists
// too, but we don't need to include them since we never enable state-shadowing for pre-Gfx8 GPU's.
const RegisterRange NonShadowedRangesGfx8[] =
{
    {
        mmSMC_MSG_ARG_11__VI,
        1
    },
    {
        mmMP_FPS_CNT__VI,
        1
    },
    {
        mmMC_CONFIG,
        1
    },
    {
        mmMC_CONFIG_MCD,
        1
    },
    {
        mmVGT_DMA_PRIMITIVE_TYPE__CI__VI,
        mmVGT_DMA_LS_HS_CONFIG__CI__VI - mmVGT_DMA_PRIMITIVE_TYPE__CI__VI + 1,
    },
    {
        mmSPI_CONFIG_CNTL,
        1
    },
    {
        mmCOMPUTE_VMID,
        1
    },
    {
        mmSPI_RESOURCE_RESERVE_CU_0__CI__VI,
        1
    },
    {
        mmSPI_RESOURCE_RESERVE_EN_CU_0__CI__VI,
        1
    },
    {
        mmSRBM_PERFMON_CNTL__VI,
        mmSRBM_PERFCOUNTER1_HI__VI - mmSRBM_PERFMON_CNTL__VI + 1
    },
    {
        mmVGT_DMA_NUM_INSTANCES,
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
        mmVGT_INDEX_TYPE__CI__VI,
        1
    },
    {
        mmCP_NUM_PRIM_WRITTEN_COUNT0_LO__CI__VI,
        mmCP_NUM_PRIM_NEEDED_COUNT3_HI__CI__VI - mmCP_NUM_PRIM_WRITTEN_COUNT0_LO__CI__VI + 1
    },
    {
        mmCP_VGT_IAVERT_COUNT_LO__CI__VI,
        mmCP_SC_PSINVOC_COUNT0_HI__CI__VI - mmCP_VGT_IAVERT_COUNT_LO__CI__VI + 1
    },
    {
        mmCP_VGT_CSINVOC_COUNT_LO__CI__VI,
        mmCP_VGT_CSINVOC_COUNT_HI__CI__VI - mmCP_VGT_CSINVOC_COUNT_LO__CI__VI + 1
    },
    {
        mmGRBM_GFX_INDEX__CI__VI,
        1
    },
    {
        mmVGT_STRMOUT_BUFFER_FILLED_SIZE_0__CI__VI,
        mmVGT_STRMOUT_BUFFER_FILLED_SIZE_3__CI__VI - mmVGT_STRMOUT_BUFFER_FILLED_SIZE_0__CI__VI + 1
    },
    {
        mmDB_OCCLUSION_COUNT0_LOW__CI__VI,
        mmDB_OCCLUSION_COUNT3_HI__CI__VI - mmDB_OCCLUSION_COUNT0_LOW__CI__VI + 1
    },
    {
        mmCP_DRAW_OBJECT_COUNTER__VI,
        1
    },
    {
        mmCPG_PERFCOUNTER1_SELECT__CI__VI,
        mmCPC_PERFCOUNTER0_SELECT__CI__VI - mmCPG_PERFCOUNTER1_SELECT__CI__VI + 1
    },
    {
        mmCB_PERFCOUNTER0_SELECT__CI__VI,
        mmCB_PERFCOUNTER3_SELECT__CI__VI - mmCB_PERFCOUNTER0_SELECT__CI__VI + 1
    },
    {
        mmDB_PERFCOUNTER0_SELECT__CI__VI,
        mmDB_PERFCOUNTER3_SELECT__CI__VI - mmDB_PERFCOUNTER0_SELECT__CI__VI + 1
    },
    {
        mmGRBM_PERFCOUNTER0_SELECT__CI__VI,
        mmGRBM_PERFCOUNTER1_SELECT__CI__VI - mmGRBM_PERFCOUNTER0_SELECT__CI__VI + 1
    },
    {
        mmGRBM_SE0_PERFCOUNTER_SELECT__CI__VI,
        mmGRBM_SE3_PERFCOUNTER_SELECT__CI__VI - mmGRBM_SE0_PERFCOUNTER_SELECT__CI__VI + 1
    },
    {
        mmRLC_PERFMON_CNTL__CI__VI,
        mmRLC_PERFCOUNTER1_SELECT__CI__VI - mmRLC_PERFMON_CNTL__CI__VI + 1
    },
    {
        mmPA_SU_PERFCOUNTER0_SELECT__CI__VI,
        mmPA_SU_PERFCOUNTER3_SELECT__CI__VI - mmPA_SU_PERFCOUNTER0_SELECT__CI__VI + 1,
    },
    {
        mmPA_SC_PERFCOUNTER0_SELECT__CI__VI,
        mmPA_SC_PERFCOUNTER7_SELECT__CI__VI - mmPA_SC_PERFCOUNTER0_SELECT__CI__VI + 1,
    },
    {
        mmSX_PERFCOUNTER0_SELECT__CI__VI,
        mmSX_PERFCOUNTER1_SELECT1__CI__VI - mmSX_PERFCOUNTER0_SELECT__CI__VI + 1,
    },
    {
        mmSPI_CONFIG_CNTL,
        1
    },
    {
        mmSPI_PERFCOUNTER0_SELECT__CI__VI,
        mmSPI_PERFCOUNTER_BINS__CI__VI - mmSPI_PERFCOUNTER0_SELECT__CI__VI + 1,
    },
    {
        mmSQ_PERFCOUNTER0_LO__CI__VI,
        mmSQ_PERFCOUNTER15_HI__CI__VI - mmSQ_PERFCOUNTER0_LO__CI__VI + 1,
    },
    {
        mmSQ_PERFCOUNTER0_SELECT__CI__VI,
        mmSQ_PERFCOUNTER15_SELECT__CI__VI - mmSQ_PERFCOUNTER0_SELECT__CI__VI + 1,
    },
    {
        mmSQ_PERFCOUNTER_CTRL__CI__VI,
        mmSQ_PERFCOUNTER_CTRL2__CI__VI - mmSQ_PERFCOUNTER_CTRL__CI__VI + 1,
    },
    {
        mmTA_PERFCOUNTER0_SELECT__CI__VI,
        mmTA_PERFCOUNTER1_SELECT__CI__VI - mmTA_PERFCOUNTER0_SELECT__CI__VI + 1,
    },
    {
        mmTD_PERFCOUNTER0_SELECT__CI__VI,
        mmTD_PERFCOUNTER1_SELECT__CI__VI - mmTD_PERFCOUNTER0_SELECT__CI__VI + 1,
    },
    {
        mmTCP_PERFCOUNTER0_SELECT__CI__VI,
        mmTCP_PERFCOUNTER3_SELECT__CI__VI - mmTCP_PERFCOUNTER0_SELECT__CI__VI + 1,
    },
    {
        mmTCC_PERFCOUNTER0_SELECT__CI__VI,
        mmTCC_PERFCOUNTER3_SELECT__CI__VI - mmTCC_PERFCOUNTER0_SELECT__CI__VI + 1,
    },
    {
        mmTCA_PERFCOUNTER0_SELECT__CI__VI,
        mmTCA_PERFCOUNTER3_SELECT__CI__VI - mmTCA_PERFCOUNTER0_SELECT__CI__VI + 1,
    },
    {
        mmGDS_PERFCOUNTER0_SELECT__CI__VI,
        mmGDS_PERFCOUNTER0_SELECT1__CI__VI - mmGDS_PERFCOUNTER0_SELECT__CI__VI + 1,
    },
    {
        mmVGT_PERFCOUNTER0_SELECT__CI__VI,
        mmVGT_PERFCOUNTER1_SELECT1__CI__VI - mmVGT_PERFCOUNTER0_SELECT__CI__VI + 1,
    },
    {
        mmIA_PERFCOUNTER0_SELECT__CI__VI,
        mmIA_PERFCOUNTER0_SELECT1__CI__VI - mmIA_PERFCOUNTER0_SELECT__CI__VI + 1,
    },
    {
        mmWD_PERFCOUNTER0_SELECT__CI__VI,
        mmWD_PERFCOUNTER3_SELECT__CI__VI - mmWD_PERFCOUNTER0_SELECT__CI__VI + 1,
    },
    {
        mmSDMA0_PERFMON_CNTL__VI,
        1,
    },
    {
        mmSDMA1_PERFMON_CNTL__VI,
        1,
    },
    {
        mmRLC_SPM_PERFMON_CNTL__CI__VI,
        mmRLC_SPM_SE_MUXSEL_ADDR__CI__VI - mmRLC_SPM_PERFMON_CNTL__CI__VI + 1,
    },
    {
        mmRLC_SPM_GLOBAL_MUXSEL_ADDR__CI__VI,
        mmRLC_SPM_RING_RDPTR__CI__VI - mmRLC_SPM_GLOBAL_MUXSEL_ADDR__CI__VI + 1,
    },
    {
        mmSQ_THREAD_TRACE_BASE__VI,
        mmSQ_THREAD_TRACE_HIWATER__VI - mmSQ_THREAD_TRACE_BASE__VI + 1
    },
    {
        mmRLC_PERFMON_CLK_CNTL__VI,
        1,
    },
    {
        mmSQ_THREAD_TRACE_USERDATA_0__CI__VI,
        mmSQ_THREAD_TRACE_USERDATA_3__CI__VI - mmSQ_THREAD_TRACE_USERDATA_0__CI__VI + 1
    },

    // The 7 pairs below are written in preamble when CU reservation is enabled.
    {
        mmSPI_SHADER_PGM_RSRC3_PS__CI__VI,
        1
    },
    {
        mmSPI_SHADER_PGM_RSRC3_VS__CI__VI,
        1
    },
    {
        mmSPI_SHADER_PGM_RSRC3_GS__CI__VI,
        1
    },
    {
        mmSPI_SHADER_PGM_RSRC3_ES__CI__VI,
        1
    },
    {
        mmSPI_SHADER_PGM_RSRC3_LS__CI__VI,
        1
    },
    {
        mmCOMPUTE_STATIC_THREAD_MGMT_SE0,
        mmCOMPUTE_STATIC_THREAD_MGMT_SE1 - mmCOMPUTE_STATIC_THREAD_MGMT_SE0 + 1
    },
    {
        mmCOMPUTE_STATIC_THREAD_MGMT_SE2__CI__VI,
        mmCOMPUTE_STATIC_THREAD_MGMT_SE3__CI__VI - mmCOMPUTE_STATIC_THREAD_MGMT_SE2__CI__VI + 1
    },
};
constexpr uint32 NumNonShadowedRangesGfx8 = static_cast<uint32>(Util::ArrayLen(NonShadowedRangesGfx8));

// =====================================================================================================================
// Helper function which determines if a register address falls within any of the specified register ranges.
inline bool IsRegisterInRangeList(
    uint32               regAddr,
    const RegisterRange* pRanges,
    uint32               count)
{
    bool found = false;

    const RegisterRange* pRange = pRanges;
    for (uint32 i = 0; i < count; ++i, ++pRange)
    {
        if ((regAddr >= pRange->regOffset) && (regAddr < (pRange->regOffset + pRange->regCount)))
        {
            found = true;
            break;
        }
    }

    return found;
}

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
#endif

} // Gfx6
} // Pal
