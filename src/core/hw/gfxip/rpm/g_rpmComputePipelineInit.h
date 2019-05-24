/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
    ClearBuffer                           = 0,
    ClearImage1d                          = 1,
    ClearImage1dTexelScale                = 2,
    ClearImage2d                          = 3,
    ClearImage2dTexelScale                = 4,
    ClearImage3d                          = 5,
    ClearImage3dTexelScale                = 6,
    CopyBufferByte                        = 7,
    CopyBufferDword                       = 8,
    CopyImage2d                           = 9,
    CopyImage2dms2x                       = 10,
    CopyImage2dms4x                       = 11,
    CopyImage2dms8x                       = 12,
    CopyImage2dShaderMipLevel             = 13,
    CopyImageGammaCorrect2d               = 14,
    CopyImgToMem1d                        = 15,
    CopyImgToMem2d                        = 16,
    CopyImgToMem2dms2x                    = 17,
    CopyImgToMem2dms4x                    = 18,
    CopyImgToMem2dms8x                    = 19,
    CopyImgToMem3d                        = 20,
    CopyMemToImg1d                        = 21,
    CopyMemToImg2d                        = 22,
    CopyMemToImg2dms2x                    = 23,
    CopyMemToImg2dms4x                    = 24,
    CopyMemToImg2dms8x                    = 25,
    CopyMemToImg3d                        = 26,
    CopyTypedBuffer1d                     = 27,
    CopyTypedBuffer2d                     = 28,
    CopyTypedBuffer3d                     = 29,
    ExpandMaskRam                         = 30,
    ExpandMaskRamMs2x                     = 31,
    ExpandMaskRamMs4x                     = 32,
    ExpandMaskRamMs8x                     = 33,
    FastDepthClear                        = 34,
    FastDepthExpClear                     = 35,
    FastDepthStExpClear                   = 36,
    FillMem4xDword                        = 37,
    FillMemDword                          = 38,
    HtileCopyAndFixUp                     = 39,
    MsaaFmaskCopyImage                    = 40,
    MsaaFmaskCopyImageOptimized           = 41,
    MsaaFmaskExpand2x                     = 42,
    MsaaFmaskExpand4x                     = 43,
    MsaaFmaskExpand8x                     = 44,
    MsaaFmaskResolve1xEqaa                = 45,
    MsaaFmaskResolve2x                    = 46,
    MsaaFmaskResolve2xEqaa                = 47,
    MsaaFmaskResolve2xEqaaMax             = 48,
    MsaaFmaskResolve2xEqaaMin             = 49,
    MsaaFmaskResolve2xMax                 = 50,
    MsaaFmaskResolve2xMin                 = 51,
    MsaaFmaskResolve4x                    = 52,
    MsaaFmaskResolve4xEqaa                = 53,
    MsaaFmaskResolve4xEqaaMax             = 54,
    MsaaFmaskResolve4xEqaaMin             = 55,
    MsaaFmaskResolve4xMax                 = 56,
    MsaaFmaskResolve4xMin                 = 57,
    MsaaFmaskResolve8x                    = 58,
    MsaaFmaskResolve8xEqaa                = 59,
    MsaaFmaskResolve8xEqaaMax             = 60,
    MsaaFmaskResolve8xEqaaMin             = 61,
    MsaaFmaskResolve8xMax                 = 62,
    MsaaFmaskResolve8xMin                 = 63,
    MsaaFmaskScaledCopy                   = 64,
    MsaaResolve2x                         = 65,
    MsaaResolve2xMax                      = 66,
    MsaaResolve2xMin                      = 67,
    MsaaResolve4x                         = 68,
    MsaaResolve4xMax                      = 69,
    MsaaResolve4xMin                      = 70,
    MsaaResolve8x                         = 71,
    MsaaResolve8xMax                      = 72,
    MsaaResolve8xMin                      = 73,
    MsaaResolveStencil2xMax               = 74,
    MsaaResolveStencil2xMin               = 75,
    MsaaResolveStencil4xMax               = 76,
    MsaaResolveStencil4xMin               = 77,
    MsaaResolveStencil8xMax               = 78,
    MsaaResolveStencil8xMin               = 79,
    PackedPixelComposite                  = 80,
    ResolveOcclusionQuery                 = 81,
    ResolvePipelineStatsQuery             = 82,
    ResolveStreamoutStatsQuery            = 83,
    RgbToYuvPacked                        = 84,
    RgbToYuvPlanar                        = 85,
    ScaledCopyImage2d                     = 86,
    ScaledCopyImage3d                     = 87,
    YuvIntToRgb                           = 88,
    YuvToRgb                              = 89,
    Gfx6GenerateCmdDispatch               = 92,
    Gfx6GenerateCmdDraw                   = 93,
#if PAL_BUILD_GFX9
    Gfx9BuildHtileLookupTable             = 94,
    Gfx9ClearDccMultiSample2d             = 95,
    Gfx9ClearDccOptimized2d               = 96,
    Gfx9ClearDccSingleSample2d            = 97,
    Gfx9ClearDccSingleSample3d            = 98,
    Gfx9ClearHtileFast                    = 99,
    Gfx9ClearHtileMultiSample             = 100,
    Gfx9ClearHtileOptimized2d             = 101,
    Gfx9ClearHtileSingleSample            = 102,
    Gfx9Fill4x4Dword                      = 103,
    Gfx9GenerateCmdDispatch               = 104,
    Gfx9GenerateCmdDraw                   = 105,
    Gfx9HtileCopyAndFixUp                 = 106,
    Gfx9InitCmaskSingleSample             = 107,
#endif
    Count
};

Result CreateRpmComputePipelines(GfxDevice* pDevice, ComputePipeline** pPipelineMem);

} // Pal
