/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "pal.h"

namespace Pal
{

class GraphicsPipeline;
class GfxDevice;

// RPM Graphics States. Used to index into RsrcProcMgr::m_pGraphicsStates array
enum RpmGfxPipeline : uint32
{
    CopyDepth                    = 0,
    CopyDepthStencil             = 1,
    CopyMsaaDepth                = 2,
    CopyMsaaDepthStencil         = 3,
    CopyMsaaStencil              = 4,
    CopyStencil                  = 5,
    DccDecompress                = 6,
    DepthExpand                  = 7,
    DepthResummarize             = 8,
    DepthSlowDraw                = 9,
    FastClearElim                = 10,
    FmaskDecompress              = 11,
    Copy_32ABGR                  = 12,
    Copy_32GR                    = 13,
    Copy_32R                     = 14,
    Copy_FP16                    = 15,
    Copy_SINT16                  = 16,
    Copy_SNORM16                 = 17,
    Copy_UINT16                  = 18,
    Copy_UNORM16                 = 19,
    ResolveFixedFunc_32ABGR      = 20,
    ResolveFixedFunc_32GR        = 21,
    ResolveFixedFunc_32R         = 22,
    ResolveFixedFunc_FP16        = 23,
    ResolveFixedFunc_SINT16      = 24,
    ResolveFixedFunc_SNORM16     = 25,
    ResolveFixedFunc_UINT16      = 26,
    ResolveFixedFunc_UNORM16     = 27,
    ScaledCopy2d_32ABGR          = 28,
    ScaledCopy2d_32GR            = 29,
    ScaledCopy2d_32R             = 30,
    ScaledCopy2d_FP16            = 31,
    ScaledCopy2d_SINT16          = 32,
    ScaledCopy2d_SNORM16         = 33,
    ScaledCopy2d_UINT16          = 34,
    ScaledCopy2d_UNORM16         = 35,
    ScaledCopy3d_32ABGR          = 36,
    ScaledCopy3d_32GR            = 37,
    ScaledCopy3d_32R             = 38,
    ScaledCopy3d_FP16            = 39,
    ScaledCopy3d_SINT16          = 40,
    ScaledCopy3d_SNORM16         = 41,
    ScaledCopy3d_UINT16          = 42,
    ScaledCopy3d_UNORM16         = 43,
    SlowColorClear0_32ABGR       = 44,
    SlowColorClear0_32GR         = 45,
    SlowColorClear0_32R          = 46,
    SlowColorClear0_FP16         = 47,
    SlowColorClear0_SINT16       = 48,
    SlowColorClear0_SNORM16      = 49,
    SlowColorClear0_UINT16       = 50,
    SlowColorClear0_UNORM16      = 51,
    SlowColorClear1_32ABGR       = 52,
    SlowColorClear1_32GR         = 53,
    SlowColorClear1_32R          = 54,
    SlowColorClear1_FP16         = 55,
    SlowColorClear1_SINT16       = 56,
    SlowColorClear1_SNORM16      = 57,
    SlowColorClear1_UINT16       = 58,
    SlowColorClear1_UNORM16      = 59,
    SlowColorClear2_32ABGR       = 60,
    SlowColorClear2_32GR         = 61,
    SlowColorClear2_32R          = 62,
    SlowColorClear2_FP16         = 63,
    SlowColorClear2_SINT16       = 64,
    SlowColorClear2_SNORM16      = 65,
    SlowColorClear2_UINT16       = 66,
    SlowColorClear2_UNORM16      = 67,
    SlowColorClear3_32ABGR       = 68,
    SlowColorClear3_32GR         = 69,
    SlowColorClear3_32R          = 70,
    SlowColorClear3_FP16         = 71,
    SlowColorClear3_SINT16       = 72,
    SlowColorClear3_SNORM16      = 73,
    SlowColorClear3_UINT16       = 74,
    SlowColorClear3_UNORM16      = 75,
    SlowColorClear4_32ABGR       = 76,
    SlowColorClear4_32GR         = 77,
    SlowColorClear4_32R          = 78,
    SlowColorClear4_FP16         = 79,
    SlowColorClear4_SINT16       = 80,
    SlowColorClear4_SNORM16      = 81,
    SlowColorClear4_UINT16       = 82,
    SlowColorClear4_UNORM16      = 83,
    SlowColorClear5_32ABGR       = 84,
    SlowColorClear5_32GR         = 85,
    SlowColorClear5_32R          = 86,
    SlowColorClear5_FP16         = 87,
    SlowColorClear5_SINT16       = 88,
    SlowColorClear5_SNORM16      = 89,
    SlowColorClear5_UINT16       = 90,
    SlowColorClear5_UNORM16      = 91,
    SlowColorClear6_32ABGR       = 92,
    SlowColorClear6_32GR         = 93,
    SlowColorClear6_32R          = 94,
    SlowColorClear6_FP16         = 95,
    SlowColorClear6_SINT16       = 96,
    SlowColorClear6_SNORM16      = 97,
    SlowColorClear6_UINT16       = 98,
    SlowColorClear6_UNORM16      = 99,
    SlowColorClear7_32ABGR       = 100,
    SlowColorClear7_32GR         = 101,
    SlowColorClear7_32R          = 102,
    SlowColorClear7_FP16         = 103,
    SlowColorClear7_SINT16       = 104,
    SlowColorClear7_SNORM16      = 105,
    SlowColorClear7_UINT16       = 106,
    SlowColorClear7_UNORM16      = 107,
    ResolveDepth                 = 108,
    ResolveDepthCopy             = 109,
    ResolveStencil               = 110,
    ResolveStencilCopy           = 111,
    ScaledCopyDepth              = 112,
    ScaledCopyDepthStencil       = 113,
    ScaledCopyImageColorKey      = 114,
    ScaledCopyMsaaDepth          = 115,
    ScaledCopyMsaaDepthStencil   = 116,
    ScaledCopyMsaaStencil        = 117,
    ScaledCopyStencil            = 118,
#if PAL_BUILD_NAVI31
    Gfx11ResolveGraphics_32ABGR  = 119,
    Gfx11ResolveGraphics_32GR    = 120,
    Gfx11ResolveGraphics_32R     = 121,
    Gfx11ResolveGraphics_FP16    = 122,
    Gfx11ResolveGraphics_SINT16  = 123,
    Gfx11ResolveGraphics_SNORM16 = 124,
    Gfx11ResolveGraphics_UINT16  = 125,
    Gfx11ResolveGraphics_UNORM16 = 126,
#endif
    RpmGfxPipelineCount
};

// We only care about 8 of the 10 export formats. We should never expect ZERO, and although 32_AR used by Pal, it
// appears in GetPsExportFormat we can't actually get to that export format with the image formats that Pal supports.
constexpr uint32 NumExportFormats = 8;

// Starting user-data entry which the RPM vertex shaders use for exporting a constant depth.
constexpr uint32 RpmVsDepthOut = 0;

// Starting user-data entry which the RPM vertex shaders use for exporting slice id.
constexpr uint32 RpmVsSliceOffset = RpmVsDepthOut + 1;

// Starting user-data entry which the RPM clear pixel shaders can use.
constexpr uint32 RpmPsClearFirstUserData = (RpmVsSliceOffset + 1);

// We support separate depth and stencil resolves
constexpr uint32 NumDepthStencilResolveTypes = 2;

Result CreateRpmGraphicsPipelines(GfxDevice* pDevice, GraphicsPipeline** pPipelineMem);

} // Pal
