/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

class ComputePipeline;
class GfxDevice;

// RPM Compute Pipelines. Used to index into RsrcProcMgr::m_pComputePipelines array
enum class RpmComputePipeline : uint32
{
    ClearBuffer                       = 0,
    ClearImage1d                      = 1,
    ClearImage2d                      = 2,
    ClearImage3d                      = 3,
    CopyBufferByte                    = 4,
    CopyBufferDword                   = 5,
    CopyImage2d                       = 6,
    CopyImage2dms2x                   = 7,
    CopyImage2dms4x                   = 8,
    CopyImage2dms8x                   = 9,
    CopyImage2dShaderMipLevel         = 10,
    CopyImageGammaCorrect2d           = 11,
    CopyImgToMem1d                    = 12,
    CopyImgToMem2d                    = 13,
    CopyImgToMem2dms2x                = 14,
    CopyImgToMem2dms4x                = 15,
    CopyImgToMem2dms8x                = 16,
    CopyImgToMem3d                    = 17,
    CopyMemToImg1d                    = 18,
    CopyMemToImg2d                    = 19,
    CopyMemToImg2dms2x                = 20,
    CopyMemToImg2dms4x                = 21,
    CopyMemToImg2dms8x                = 22,
    CopyMemToImg3d                    = 23,
    CopyTypedBuffer1d                 = 24,
    CopyTypedBuffer2d                 = 25,
    CopyTypedBuffer3d                 = 26,
    ExpandMaskRam                     = 27,
    ExpandMaskRamMs2x                 = 28,
    ExpandMaskRamMs4x                 = 29,
    ExpandMaskRamMs8x                 = 30,
    FastDepthClear                    = 31,
    FastDepthExpClear                 = 32,
    FastDepthStExpClear               = 33,
    FillMem4xDword                    = 34,
    FillMemDword                      = 35,
    HtileCopyAndFixUp                 = 36,
    MsaaFmaskCopyImage                = 37,
    MsaaFmaskCopyImageOptimized       = 38,
    MsaaFmaskExpand2x                 = 39,
    MsaaFmaskExpand4x                 = 40,
    MsaaFmaskExpand8x                 = 41,
    MsaaFmaskResolve1xEqaa            = 42,
    MsaaFmaskResolve2x                = 43,
    MsaaFmaskResolve2xEqaa            = 44,
    MsaaFmaskResolve2xEqaaMax         = 45,
    MsaaFmaskResolve2xEqaaMin         = 46,
    MsaaFmaskResolve2xMax             = 47,
    MsaaFmaskResolve2xMin             = 48,
    MsaaFmaskResolve4x                = 49,
    MsaaFmaskResolve4xEqaa            = 50,
    MsaaFmaskResolve4xEqaaMax         = 51,
    MsaaFmaskResolve4xEqaaMin         = 52,
    MsaaFmaskResolve4xMax             = 53,
    MsaaFmaskResolve4xMin             = 54,
    MsaaFmaskResolve8x                = 55,
    MsaaFmaskResolve8xEqaa            = 56,
    MsaaFmaskResolve8xEqaaMax         = 57,
    MsaaFmaskResolve8xEqaaMin         = 58,
    MsaaFmaskResolve8xMax             = 59,
    MsaaFmaskResolve8xMin             = 60,
    MsaaFmaskScaledCopy               = 61,
    MsaaResolve2x                     = 62,
    MsaaResolve2xMax                  = 63,
    MsaaResolve2xMin                  = 64,
    MsaaResolve4x                     = 65,
    MsaaResolve4xMax                  = 66,
    MsaaResolve4xMin                  = 67,
    MsaaResolve8x                     = 68,
    MsaaResolve8xMax                  = 69,
    MsaaResolve8xMin                  = 70,
    PackedPixelComposite              = 71,
    ResolveOcclusionQuery             = 72,
    ResolvePipelineStatsQuery         = 73,
    ResolveStreamoutStatsQuery        = 74,
    RgbToYuvPacked                    = 75,
    RgbToYuvPlanar                    = 76,
    ScaledCopyImage2d                 = 77,
    ScaledCopyImage3d                 = 78,
    YuvIntToRgb                       = 79,
    YuvToRgb                          = 80,
    Gfx6GenerateCmdDispatch           = 81,
    Gfx6GenerateCmdDraw               = 82,
#if PAL_BUILD_GFX9
    Gfx9BuildHtileLookupTable         = 86,
    Gfx9ClearDccMultiSample2d         = 87,
    Gfx9ClearDccOptimized2d           = 88,
    Gfx9ClearDccSingleSample2d        = 89,
    Gfx9ClearDccSingleSample3d        = 90,
    Gfx9ClearHtileFast                = 91,
    Gfx9ClearHtileMultiSample         = 92,
    Gfx9ClearHtileOptimized2d         = 93,
    Gfx9ClearHtileSingleSample        = 94,
    Gfx9Fill4x4Dword                  = 95,
    Gfx9GenerateCmdDispatch           = 96,
    Gfx9GenerateCmdDraw               = 97,
    Gfx9HtileCopyAndFixUp             = 98,
    Gfx9InitCmaskSingleSample         = 99,
#endif
    Count
};

Result CreateRpmComputePipelines(GfxDevice* pDevice, ComputePipeline** pPipelineMem);

} // Pal
