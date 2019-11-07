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
    FillMem4xDword                        = 38,
    FillMemDword                          = 39,
    HtileCopyAndFixUp                     = 40,
    HtileSR4xUpdate                       = 41,
    HtileSRUpdate                         = 42,
    MsaaFmaskCopyImage                    = 43,
    MsaaFmaskCopyImageOptimized           = 44,
    MsaaFmaskExpand2x                     = 45,
    MsaaFmaskExpand4x                     = 46,
    MsaaFmaskExpand8x                     = 47,
    MsaaFmaskResolve1xEqaa                = 48,
    MsaaFmaskResolve2x                    = 49,
    MsaaFmaskResolve2xEqaa                = 50,
    MsaaFmaskResolve2xEqaaMax             = 51,
    MsaaFmaskResolve2xEqaaMin             = 52,
    MsaaFmaskResolve2xMax                 = 53,
    MsaaFmaskResolve2xMin                 = 54,
    MsaaFmaskResolve4x                    = 55,
    MsaaFmaskResolve4xEqaa                = 56,
    MsaaFmaskResolve4xEqaaMax             = 57,
    MsaaFmaskResolve4xEqaaMin             = 58,
    MsaaFmaskResolve4xMax                 = 59,
    MsaaFmaskResolve4xMin                 = 60,
    MsaaFmaskResolve8x                    = 61,
    MsaaFmaskResolve8xEqaa                = 62,
    MsaaFmaskResolve8xEqaaMax             = 63,
    MsaaFmaskResolve8xEqaaMin             = 64,
    MsaaFmaskResolve8xMax                 = 65,
    MsaaFmaskResolve8xMin                 = 66,
    MsaaFmaskScaledCopy                   = 67,
    MsaaResolve2x                         = 68,
    MsaaResolve2xMax                      = 69,
    MsaaResolve2xMin                      = 70,
    MsaaResolve4x                         = 71,
    MsaaResolve4xMax                      = 72,
    MsaaResolve4xMin                      = 73,
    MsaaResolve8x                         = 74,
    MsaaResolve8xMax                      = 75,
    MsaaResolve8xMin                      = 76,
    MsaaResolveStencil2xMax               = 77,
    MsaaResolveStencil2xMin               = 78,
    MsaaResolveStencil4xMax               = 79,
    MsaaResolveStencil4xMin               = 80,
    MsaaResolveStencil8xMax               = 81,
    MsaaResolveStencil8xMin               = 82,
    PackedPixelComposite                  = 83,
    ResolveOcclusionQuery                 = 84,
    ResolvePipelineStatsQuery             = 85,
    ResolveStreamoutStatsQuery            = 86,
    RgbToYuvPacked                        = 87,
    RgbToYuvPlanar                        = 88,
    ScaledCopyImage2d                     = 89,
    ScaledCopyImage3d                     = 90,
    YuvIntToRgb                           = 91,
    YuvToRgb                              = 92,
    Gfx6GenerateCmdDispatch               = 93,
    Gfx6GenerateCmdDraw                   = 94,
    Gfx9BuildHtileLookupTable             = 95,
    Gfx9ClearDccMultiSample2d             = 96,
    Gfx9ClearDccOptimized2d               = 97,
    Gfx9ClearDccSingleSample2d            = 98,
    Gfx9ClearDccSingleSample3d            = 99,
    Gfx9ClearHtileFast                    = 100,
    Gfx9ClearHtileMultiSample             = 101,
    Gfx9ClearHtileOptimized2d             = 102,
    Gfx9ClearHtileSingleSample            = 103,
    Gfx9Fill4x4Dword                      = 104,
    Gfx9GenerateCmdDispatch               = 105,
    Gfx9GenerateCmdDraw                   = 106,
    Gfx9HtileCopyAndFixUp                 = 109,
    Gfx9InitCmaskSingleSample             = 110,
    Gfx10ClearDccComputeSetFirstPixel     = 111,
    Gfx10ClearDccComputeSetFirstPixelMsaa = 112,
    Gfx10GenerateCmdDispatch              = 113,
    Gfx10GenerateCmdDraw                  = 114,
    Count
};

Result CreateRpmComputePipelines(GfxDevice* pDevice, ComputePipeline** pPipelineMem);

} // Pal
