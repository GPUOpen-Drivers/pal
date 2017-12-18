/*
 *******************************************************************************
 *
 * Copyright (c) 2017 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!
//
// This code has been generated automatically. Do not hand-modify this code.
//
// When changes are needed, modify the tools generating this module in the tools\internal\MCBP directory.
//
// WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING! WARNING!  WARNING!  WARNING!  WARNING!
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9CmdStream.h"

namespace Pal
{
namespace Gfx9
{
// =====================================================================================================================
// Initialize several structures with the ClearState values, the Set registers are the same as ContextShadowRange.
void InitializeContextRegistersGfx9(
    CmdStream* pCmdStream)
{
    constexpr uint32 DbRenderControlGfx9[] = {
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x40004000,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0
    };
    constexpr uint32 PaScWindowOffsetGfx9[] = {
        0x0       ,
        0x80000000,
        0x40004000
    };
    constexpr uint32 CoherDestBaseHi0Gfx9[] = {
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0
    };
    constexpr uint32 PaScEdgeruleGfx9[] = {
        0xaa99aaaa,
        0x0       ,
        0xffffffff,
        0xffffffff,
        0x80000000,
        0x40004000,
        0x0       ,
        0x0       ,
        0x80000000,
        0x40004000,
        0x80000000,
        0x40004000,
        0x80000000,
        0x40004000,
        0x80000000,
        0x40004000,
        0x80000000,
        0x40004000,
        0x80000000,
        0x40004000,
        0x80000000,
        0x40004000,
        0x80000000,
        0x40004000,
        0x80000000,
        0x40004000,
        0x80000000,
        0x40004000,
        0x80000000,
        0x40004000,
        0x80000000,
        0x40004000,
        0x80000000,
        0x40004000,
        0x80000000,
        0x40004000,
        0x80000000,
        0x40004000,
        0x80000000,
        0x40004000,
        0x0       ,
        0x3f800000,
        0x0       ,
        0x3f800000,
        0x0       ,
        0x3f800000,
        0x0       ,
        0x3f800000,
        0x0       ,
        0x3f800000,
        0x0       ,
        0x3f800000,
        0x0       ,
        0x3f800000,
        0x0       ,
        0x3f800000,
        0x0       ,
        0x3f800000,
        0x0       ,
        0x3f800000,
        0x0       ,
        0x3f800000,
        0x0       ,
        0x3f800000,
        0x0       ,
        0x3f800000,
        0x0       ,
        0x3f800000,
        0x0       ,
        0x3f800000,
        0x0       ,
        0x3f800000,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0
    };
    constexpr uint32 PaScRightVertGridGfx9[] = {
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0
    };
    constexpr uint32 VgtMultiPrimIbResetIndxGfx9[] = {
        0x0
    };
    constexpr uint32 CbBlendRedGfx9[] = {
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x1000000 ,
        0x1000000 ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0
    };
    constexpr uint32 SpiPsInputCntl0Gfx9[] = {
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x2       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0
    };
    constexpr uint32 SxPsDownconvertGfx9[] = {
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0
    };
    constexpr uint32 DbDepthControlGfx9[] = {
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x90000   ,
        0x4       ,
        0x0       ,
        0x0       ,
        0x0
    };
    constexpr uint32 PaSuSmallPrimFilterCntlGfx9[] = {
        0x0       ,
        0x0       ,
        0x0       ,
        0x0
    };
    constexpr uint32 PaSuPointSizeGfx9[] = {
        0x0       ,
        0x0       ,
        0x0
    };
    constexpr uint32 VgtHosMaxTessLevelGfx9[] = {
        0x0       ,
        0x0
    };
    constexpr uint32 VgtGsModeGfx9[] = {
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x100     ,
        0x80      ,
        0x2       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0
    };
    constexpr uint32 VgtPrimitiveidEnGfx9[] = {
        0x0
    };
    constexpr uint32 VgtPrimitiveidResetGfx9[] = {
        0x0
    };
    constexpr uint32 VgtGsMaxPrimsPerSubgroupGfx09Gfx9[] = {
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0
    };
    constexpr uint32 VgtStrmoutBufferSize1Gfx9[] = {
        0x0       ,
        0x0
    };
    constexpr uint32 VgtStrmoutBufferSize2Gfx9[] = {
        0x0       ,
        0x0
    };
    constexpr uint32 VgtStrmoutBufferSize3Gfx9[] = {
        0x0       ,
        0x0
    };
    constexpr uint32 VgtGsMaxVertOutGfx9[] = {
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0
    };
    constexpr uint32 PaScCentroidPriority0Gfx9[] = {
        0x0       ,
        0x0       ,
        0x1000    ,
        0x0       ,
        0x5       ,
        0x3f800000,
        0x3f800000,
        0x3f800000,
        0x3f800000,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0xffffffff,
        0xffffffff,
        0x0       ,
        0x3       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x1e      ,
        0x20      ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0       ,
        0x0
    };
    uint32* pCmdSpace = pCmdStream->ReserveCommands();
    pCmdSpace = pCmdStream->WriteSetSeqContextRegs(mmDB_RENDER_CONTROL,mmTA_BC_BASE_ADDR_HI, DbRenderControlGfx9,pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetSeqContextRegs(mmPA_SC_WINDOW_OFFSET,mmPA_SC_WINDOW_SCISSOR_BR, PaScWindowOffsetGfx9,pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetSeqContextRegs(mmCOHER_DEST_BASE_HI_0,mmCOHER_DEST_BASE_3, CoherDestBaseHi0Gfx9,pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetSeqContextRegs(mmPA_SC_EDGERULE,mmPA_SC_TILE_STEERING_OVERRIDE, PaScEdgeruleGfx9,pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetSeqContextRegs(mmPA_SC_RIGHT_VERT_GRID,mmPA_SC_FOV_WINDOW_TB, PaScRightVertGridGfx9,pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetSeqContextRegs(mmVGT_MULTI_PRIM_IB_RESET_INDX,mmVGT_MULTI_PRIM_IB_RESET_INDX, VgtMultiPrimIbResetIndxGfx9,pCmdSpace);
    pCmdStream->CommitCommands(pCmdSpace);
    pCmdSpace = pCmdStream->ReserveCommands();
    pCmdSpace = pCmdStream->WriteSetSeqContextRegs(mmCB_BLEND_RED,mmPA_CL_UCP_5_W, CbBlendRedGfx9,pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetSeqContextRegs(mmSPI_PS_INPUT_CNTL_0,mmSPI_SHADER_COL_FORMAT, SpiPsInputCntl0Gfx9,pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetSeqContextRegs(mmSX_PS_DOWNCONVERT,mmCB_MRT7_EPITCH__GFX09, SxPsDownconvertGfx9,pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetSeqContextRegs(mmDB_DEPTH_CONTROL,mmPA_CL_NANINF_CNTL, DbDepthControlGfx9,pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetSeqContextRegs(mmPA_SU_SMALL_PRIM_FILTER_CNTL,mmPA_SU_OVER_RASTERIZATION_CNTL, PaSuSmallPrimFilterCntlGfx9,pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetSeqContextRegs(mmPA_SU_POINT_SIZE,mmPA_SU_LINE_CNTL, PaSuPointSizeGfx9,pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetSeqContextRegs(mmVGT_HOS_MAX_TESS_LEVEL,mmVGT_HOS_MIN_TESS_LEVEL, VgtHosMaxTessLevelGfx9,pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetSeqContextRegs(mmVGT_GS_MODE,mmVGT_GS_OUT_PRIM_TYPE, VgtGsModeGfx9,pCmdSpace);
    pCmdStream->CommitCommands(pCmdSpace);
    pCmdSpace = pCmdStream->ReserveCommands();
    pCmdSpace = pCmdStream->WriteSetSeqContextRegs(mmVGT_PRIMITIVEID_EN,mmVGT_PRIMITIVEID_EN, VgtPrimitiveidEnGfx9,pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetSeqContextRegs(mmVGT_PRIMITIVEID_RESET,mmVGT_PRIMITIVEID_RESET, VgtPrimitiveidResetGfx9,pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetSeqContextRegs(mmVGT_GS_MAX_PRIMS_PER_SUBGROUP__GFX09,mmVGT_STRMOUT_VTX_STRIDE_0, VgtGsMaxPrimsPerSubgroupGfx09Gfx9,pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetSeqContextRegs(mmVGT_STRMOUT_BUFFER_SIZE_1,mmVGT_STRMOUT_VTX_STRIDE_1, VgtStrmoutBufferSize1Gfx9,pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetSeqContextRegs(mmVGT_STRMOUT_BUFFER_SIZE_2,mmVGT_STRMOUT_VTX_STRIDE_2, VgtStrmoutBufferSize2Gfx9,pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetSeqContextRegs(mmVGT_STRMOUT_BUFFER_SIZE_3,mmVGT_STRMOUT_VTX_STRIDE_3, VgtStrmoutBufferSize3Gfx9,pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetSeqContextRegs(mmVGT_GS_MAX_VERT_OUT,mmVGT_STRMOUT_BUFFER_CONFIG, VgtGsMaxVertOutGfx9,pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetSeqContextRegs(mmPA_SC_CENTROID_PRIORITY_0,mmCB_COLOR7_DCC_BASE_EXT__GFX09, PaScCentroidPriority0Gfx9,pCmdSpace);
    pCmdStream->CommitCommands(pCmdSpace);
}

} // Gfx9
} // Pal
