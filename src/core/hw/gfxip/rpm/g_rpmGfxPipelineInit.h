/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2024 Advanced Micro Devices, Inc. All Rights Reserved.
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
    SlowColorClear_32ABGR        = 44,
    SlowColorClear_32GR          = 45,
    SlowColorClear_32R           = 46,
    SlowColorClear_FP16          = 47,
    SlowColorClear_SINT16        = 48,
    SlowColorClear_SNORM16       = 49,
    SlowColorClear_UINT16        = 50,
    SlowColorClear_UNORM16       = 51,
    ResolveDepth                 = 52,
    ResolveDepthCopy             = 53,
    ResolveStencil               = 54,
    ResolveStencilCopy           = 55,
    ScaledCopyDepth              = 56,
    ScaledCopyDepthStencil       = 57,
    ScaledCopyImageColorKey      = 58,
    ScaledCopyMsaaDepth          = 59,
    ScaledCopyMsaaDepthStencil   = 60,
    ScaledCopyMsaaStencil        = 61,
    ScaledCopyStencil            = 62,
    Gfx11ResolveGraphics_32ABGR  = 63,
    Gfx11ResolveGraphics_32GR    = 64,
    Gfx11ResolveGraphics_32R     = 65,
    Gfx11ResolveGraphics_FP16    = 66,
    Gfx11ResolveGraphics_SINT16  = 67,
    Gfx11ResolveGraphics_SNORM16 = 68,
    Gfx11ResolveGraphics_UINT16  = 69,
    Gfx11ResolveGraphics_UNORM16 = 70,
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
