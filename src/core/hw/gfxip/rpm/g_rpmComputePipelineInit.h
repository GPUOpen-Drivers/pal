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
    HtileSR4xUpdate                       = 40,
    HtileSRUpdate                         = 41,
    MsaaFmaskCopyImage                    = 42,
    MsaaFmaskCopyImageOptimized           = 43,
    MsaaFmaskExpand2x                     = 44,
    MsaaFmaskExpand4x                     = 45,
    MsaaFmaskExpand8x                     = 46,
    MsaaFmaskResolve1xEqaa                = 47,
    MsaaFmaskResolve2x                    = 48,
    MsaaFmaskResolve2xEqaa                = 49,
    MsaaFmaskResolve2xEqaaMax             = 50,
    MsaaFmaskResolve2xEqaaMin             = 51,
    MsaaFmaskResolve2xMax                 = 52,
    MsaaFmaskResolve2xMin                 = 53,
    MsaaFmaskResolve4x                    = 54,
    MsaaFmaskResolve4xEqaa                = 55,
    MsaaFmaskResolve4xEqaaMax             = 56,
    MsaaFmaskResolve4xEqaaMin             = 57,
    MsaaFmaskResolve4xMax                 = 58,
    MsaaFmaskResolve4xMin                 = 59,
    MsaaFmaskResolve8x                    = 60,
    MsaaFmaskResolve8xEqaa                = 61,
    MsaaFmaskResolve8xEqaaMax             = 62,
    MsaaFmaskResolve8xEqaaMin             = 63,
    MsaaFmaskResolve8xMax                 = 64,
    MsaaFmaskResolve8xMin                 = 65,
    MsaaFmaskScaledCopy                   = 66,
    MsaaResolve2x                         = 67,
    MsaaResolve2xMax                      = 68,
    MsaaResolve2xMin                      = 69,
    MsaaResolve4x                         = 70,
    MsaaResolve4xMax                      = 71,
    MsaaResolve4xMin                      = 72,
    MsaaResolve8x                         = 73,
    MsaaResolve8xMax                      = 74,
    MsaaResolve8xMin                      = 75,
    MsaaResolveStencil2xMax               = 76,
    MsaaResolveStencil2xMin               = 77,
    MsaaResolveStencil4xMax               = 78,
    MsaaResolveStencil4xMin               = 79,
    MsaaResolveStencil8xMax               = 80,
    MsaaResolveStencil8xMin               = 81,
    PackedPixelComposite                  = 82,
    ResolveOcclusionQuery                 = 83,
    ResolvePipelineStatsQuery             = 84,
    ResolveStreamoutStatsQuery            = 85,
    RgbToYuvPacked                        = 86,
    RgbToYuvPlanar                        = 87,
    ScaledCopyImage2d                     = 88,
    ScaledCopyImage3d                     = 89,
    YuvIntToRgb                           = 90,
    YuvToRgb                              = 91,
    Gfx6GenerateCmdDispatch               = 96,
    Gfx6GenerateCmdDraw                   = 97,
#if PAL_BUILD_GFX9
    Gfx9BuildHtileLookupTable             = 98,
    Gfx9ClearDccMultiSample2d             = 99,
    Gfx9ClearDccOptimized2d               = 100,
    Gfx9ClearDccSingleSample2d            = 101,
    Gfx9ClearDccSingleSample3d            = 102,
    Gfx9ClearHtileFast                    = 103,
    Gfx9ClearHtileMultiSample             = 104,
    Gfx9ClearHtileOptimized2d             = 105,
    Gfx9ClearHtileSingleSample            = 106,
    Gfx9Fill4x4Dword                      = 107,
    Gfx9GenerateCmdDispatch               = 108,
    Gfx9GenerateCmdDraw                   = 109,
    Gfx9HtileCopyAndFixUp                 = 110,
    Gfx9InitCmaskSingleSample             = 111,
#endif
    Gfx10ClearDccComputeSetFirstPixel     = 112,
    Gfx10ClearDccComputeSetFirstPixelMsaa = 113,
    Gfx10GenerateCmdDispatch              = 114,
    Gfx10GenerateCmdDraw                  = 115,
    Count
};

Result CreateRpmComputePipelines(GfxDevice* pDevice, ComputePipeline** pPipelineMem);

} // Pal
