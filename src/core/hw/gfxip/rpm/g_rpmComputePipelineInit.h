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
    GenerateMipmaps                       = 40,
    HtileCopyAndFixUp                     = 41,
    HtileSR4xUpdate                       = 42,
    HtileSRUpdate                         = 43,
    MsaaFmaskCopyImage                    = 44,
    MsaaFmaskCopyImageOptimized           = 45,
    MsaaFmaskExpand2x                     = 46,
    MsaaFmaskExpand4x                     = 47,
    MsaaFmaskExpand8x                     = 48,
    MsaaFmaskResolve1xEqaa                = 49,
    MsaaFmaskResolve2x                    = 50,
    MsaaFmaskResolve2xEqaa                = 51,
    MsaaFmaskResolve2xEqaaMax             = 52,
    MsaaFmaskResolve2xEqaaMin             = 53,
    MsaaFmaskResolve2xMax                 = 54,
    MsaaFmaskResolve2xMin                 = 55,
    MsaaFmaskResolve4x                    = 56,
    MsaaFmaskResolve4xEqaa                = 57,
    MsaaFmaskResolve4xEqaaMax             = 58,
    MsaaFmaskResolve4xEqaaMin             = 59,
    MsaaFmaskResolve4xMax                 = 60,
    MsaaFmaskResolve4xMin                 = 61,
    MsaaFmaskResolve8x                    = 62,
    MsaaFmaskResolve8xEqaa                = 63,
    MsaaFmaskResolve8xEqaaMax             = 64,
    MsaaFmaskResolve8xEqaaMin             = 65,
    MsaaFmaskResolve8xMax                 = 66,
    MsaaFmaskResolve8xMin                 = 67,
    MsaaFmaskScaledCopy                   = 68,
    MsaaResolve2x                         = 69,
    MsaaResolve2xMax                      = 70,
    MsaaResolve2xMin                      = 71,
    MsaaResolve4x                         = 72,
    MsaaResolve4xMax                      = 73,
    MsaaResolve4xMin                      = 74,
    MsaaResolve8x                         = 75,
    MsaaResolve8xMax                      = 76,
    MsaaResolve8xMin                      = 77,
    MsaaResolveStencil2xMax               = 78,
    MsaaResolveStencil2xMin               = 79,
    MsaaResolveStencil4xMax               = 80,
    MsaaResolveStencil4xMin               = 81,
    MsaaResolveStencil8xMax               = 82,
    MsaaResolveStencil8xMin               = 83,
    PackedPixelComposite                  = 84,
    ResolveOcclusionQuery                 = 85,
    ResolvePipelineStatsQuery             = 86,
    ResolveStreamoutStatsQuery            = 87,
    RgbToYuvPacked                        = 88,
    RgbToYuvPlanar                        = 89,
    ScaledCopyImage2d                     = 90,
    ScaledCopyImage3d                     = 91,
    YuvIntToRgb                           = 92,
    YuvToRgb                              = 93,
    Gfx6GenerateCmdDispatch               = 94,
    Gfx6GenerateCmdDraw                   = 95,
    Gfx9BuildHtileLookupTable             = 96,
    Gfx9ClearDccMultiSample2d             = 97,
    Gfx9ClearDccOptimized2d               = 98,
    Gfx9ClearDccSingleSample2d            = 99,
    Gfx9ClearDccSingleSample3d            = 100,
    Gfx9ClearHtileFast                    = 101,
    Gfx9ClearHtileMultiSample             = 102,
    Gfx9ClearHtileOptimized2d             = 103,
    Gfx9ClearHtileSingleSample            = 104,
    Gfx9Fill4x4Dword                      = 105,
    Gfx9GenerateCmdDispatch               = 106,
    Gfx9GenerateCmdDraw                   = 107,
    Gfx9HtileCopyAndFixUp                 = 110,
    Gfx9InitCmaskSingleSample             = 111,
    Gfx10ClearDccComputeSetFirstPixel     = 112,
    Gfx10ClearDccComputeSetFirstPixelMsaa = 113,
    Gfx10GenerateCmdDispatch              = 114,
    Gfx10GenerateCmdDraw                  = 115,
    Count
};

Result CreateRpmComputePipelines(GfxDevice* pDevice, ComputePipeline** pPipelineMem);

} // Pal
