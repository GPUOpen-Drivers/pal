/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
    Copy2xMsaaDepth          = 0,
    Copy2xMsaaDepthStencil   = 1,
    Copy2xMsaaStencil        = 2,
    Copy4xMsaaDepth          = 3,
    Copy4xMsaaDepthStencil   = 4,
    Copy4xMsaaStencil        = 5,
    Copy8xMsaaDepth          = 6,
    Copy8xMsaaDepthStencil   = 7,
    Copy8xMsaaStencil        = 8,
    CopyDepth                = 9,
    CopyDepthStencil         = 10,
    CopyStencil              = 11,
    DccDecompress            = 12,
    DepthExpand              = 13,
    DepthResummarize         = 14,
    DepthSlowDraw            = 15,
    FastClearElim            = 16,
    FmaskDecompress          = 17,
    Copy_32ABGR              = 18,
    Copy_32GR                = 19,
    Copy_32R                 = 20,
    Copy_FP16                = 21,
    Copy_SINT16              = 22,
    Copy_SNORM16             = 23,
    Copy_UINT16              = 24,
    Copy_UNORM16             = 25,
    ResolveFixedFunc_32ABGR  = 26,
    ResolveFixedFunc_32GR    = 27,
    ResolveFixedFunc_32R     = 28,
    ResolveFixedFunc_FP16    = 29,
    ResolveFixedFunc_SINT16  = 30,
    ResolveFixedFunc_SNORM16 = 31,
    ResolveFixedFunc_UINT16  = 32,
    ResolveFixedFunc_UNORM16 = 33,
    ScaledCopy2d_32ABGR      = 34,
    ScaledCopy2d_32GR        = 35,
    ScaledCopy2d_32R         = 36,
    ScaledCopy2d_FP16        = 37,
    ScaledCopy2d_SINT16      = 38,
    ScaledCopy2d_SNORM16     = 39,
    ScaledCopy2d_UINT16      = 40,
    ScaledCopy2d_UNORM16     = 41,
    ScaledCopy3d_32ABGR      = 42,
    ScaledCopy3d_32GR        = 43,
    ScaledCopy3d_32R         = 44,
    ScaledCopy3d_FP16        = 45,
    ScaledCopy3d_SINT16      = 46,
    ScaledCopy3d_SNORM16     = 47,
    ScaledCopy3d_UINT16      = 48,
    ScaledCopy3d_UNORM16     = 49,
    SlowColorClear0_32ABGR   = 50,
    SlowColorClear0_32GR     = 51,
    SlowColorClear0_32R      = 52,
    SlowColorClear0_FP16     = 53,
    SlowColorClear0_SINT16   = 54,
    SlowColorClear0_SNORM16  = 55,
    SlowColorClear0_UINT16   = 56,
    SlowColorClear0_UNORM16  = 57,
    SlowColorClear1_32ABGR   = 58,
    SlowColorClear1_32GR     = 59,
    SlowColorClear1_32R      = 60,
    SlowColorClear1_FP16     = 61,
    SlowColorClear1_SINT16   = 62,
    SlowColorClear1_SNORM16  = 63,
    SlowColorClear1_UINT16   = 64,
    SlowColorClear1_UNORM16  = 65,
    SlowColorClear2_32ABGR   = 66,
    SlowColorClear2_32GR     = 67,
    SlowColorClear2_32R      = 68,
    SlowColorClear2_FP16     = 69,
    SlowColorClear2_SINT16   = 70,
    SlowColorClear2_SNORM16  = 71,
    SlowColorClear2_UINT16   = 72,
    SlowColorClear2_UNORM16  = 73,
    SlowColorClear3_32ABGR   = 74,
    SlowColorClear3_32GR     = 75,
    SlowColorClear3_32R      = 76,
    SlowColorClear3_FP16     = 77,
    SlowColorClear3_SINT16   = 78,
    SlowColorClear3_SNORM16  = 79,
    SlowColorClear3_UINT16   = 80,
    SlowColorClear3_UNORM16  = 81,
    SlowColorClear4_32ABGR   = 82,
    SlowColorClear4_32GR     = 83,
    SlowColorClear4_32R      = 84,
    SlowColorClear4_FP16     = 85,
    SlowColorClear4_SINT16   = 86,
    SlowColorClear4_SNORM16  = 87,
    SlowColorClear4_UINT16   = 88,
    SlowColorClear4_UNORM16  = 89,
    SlowColorClear5_32ABGR   = 90,
    SlowColorClear5_32GR     = 91,
    SlowColorClear5_32R      = 92,
    SlowColorClear5_FP16     = 93,
    SlowColorClear5_SINT16   = 94,
    SlowColorClear5_SNORM16  = 95,
    SlowColorClear5_UINT16   = 96,
    SlowColorClear5_UNORM16  = 97,
    SlowColorClear6_32ABGR   = 98,
    SlowColorClear6_32GR     = 99,
    SlowColorClear6_32R      = 100,
    SlowColorClear6_FP16     = 101,
    SlowColorClear6_SINT16   = 102,
    SlowColorClear6_SNORM16  = 103,
    SlowColorClear6_UINT16   = 104,
    SlowColorClear6_UNORM16  = 105,
    SlowColorClear7_32ABGR   = 106,
    SlowColorClear7_32GR     = 107,
    SlowColorClear7_32R      = 108,
    SlowColorClear7_FP16     = 109,
    SlowColorClear7_SINT16   = 110,
    SlowColorClear7_SNORM16  = 111,
    SlowColorClear7_UINT16   = 112,
    SlowColorClear7_UNORM16  = 113,
    ResolveDepth             = 114,
    ResolveDepthCopy         = 115,
    ResolveStencil           = 116,
    ResolveStencilCopy       = 117,
    ScaledCopyImageColorKey  = 118,
    RpmGfxPipelineCount
};

// We only care about 8 of the 10 export formats. We should never expect ZERO, and although 32_AR used by Pal, it
// appears in GetPsExportFormat we can't actually get to that export format with the image formats that Pal supports.
constexpr uint32 NumExportFormats = 8;

// Starting user-data entry which the RPM vertex shaders use for exporting a constant depth.
constexpr uint32 RpmVsDepthOut = 0;

// Starting user-data entry which the RPM vertex shaders use for exporting slice id.
constexpr uint32 RpmVsSliceOffset = RpmVsDepthOut + 1;

// Starting user-data entry which the RPM pixel shaders can use.
constexpr uint32 RpmPsFirstUserData = (RpmVsDepthOut + 1);

// Starting user-data entry which the RPM clear pixel shaders can use.
constexpr uint32 RpmPsClearFirstUserData = (RpmVsSliceOffset + 1);

// We support separate depth and stencil resolves
constexpr uint32 NumDepthStencilResolveTypes = 2;

Result CreateRpmGraphicsPipelines(GfxDevice* pDevice, GraphicsPipeline** pPipelineMem);

} // Pal
