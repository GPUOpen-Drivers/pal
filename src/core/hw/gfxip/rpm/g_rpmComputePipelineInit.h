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
    GenerateMipmaps                       = 39,
    GenerateMipmapsLowp                   = 40,
    HtileCopyAndFixUp                     = 41,
    HtileSR4xUpdate                       = 42,
    HtileSRUpdate                         = 43,
    MsaaFmaskCopyImage                    = 44,
    MsaaFmaskCopyImageOptimized           = 45,
    MsaaFmaskCopyImgToMem                 = 46,
    MsaaFmaskExpand2x                     = 47,
    MsaaFmaskExpand4x                     = 48,
    MsaaFmaskExpand8x                     = 49,
    MsaaFmaskResolve1xEqaa                = 50,
    MsaaFmaskResolve2x                    = 51,
    MsaaFmaskResolve2xEqaa                = 52,
    MsaaFmaskResolve2xEqaaMax             = 53,
    MsaaFmaskResolve2xEqaaMin             = 54,
    MsaaFmaskResolve2xMax                 = 55,
    MsaaFmaskResolve2xMin                 = 56,
    MsaaFmaskResolve4x                    = 57,
    MsaaFmaskResolve4xEqaa                = 58,
    MsaaFmaskResolve4xEqaaMax             = 59,
    MsaaFmaskResolve4xEqaaMin             = 60,
    MsaaFmaskResolve4xMax                 = 61,
    MsaaFmaskResolve4xMin                 = 62,
    MsaaFmaskResolve8x                    = 63,
    MsaaFmaskResolve8xEqaa                = 64,
    MsaaFmaskResolve8xEqaaMax             = 65,
    MsaaFmaskResolve8xEqaaMin             = 66,
    MsaaFmaskResolve8xMax                 = 67,
    MsaaFmaskResolve8xMin                 = 68,
    MsaaFmaskScaledCopy                   = 69,
    MsaaResolve2x                         = 70,
    MsaaResolve2xMax                      = 71,
    MsaaResolve2xMin                      = 72,
    MsaaResolve4x                         = 73,
    MsaaResolve4xMax                      = 74,
    MsaaResolve4xMin                      = 75,
    MsaaResolve8x                         = 76,
    MsaaResolve8xMax                      = 77,
    MsaaResolve8xMin                      = 78,
    MsaaResolveStencil2xMax               = 79,
    MsaaResolveStencil2xMin               = 80,
    MsaaResolveStencil4xMax               = 81,
    MsaaResolveStencil4xMin               = 82,
    MsaaResolveStencil8xMax               = 83,
    MsaaResolveStencil8xMin               = 84,
    PackedPixelComposite                  = 85,
    ResolveOcclusionQuery                 = 86,
    ResolvePipelineStatsQuery             = 87,
    ResolveStreamoutStatsQuery            = 88,
    RgbToYuvPacked                        = 89,
    RgbToYuvPlanar                        = 90,
    ScaledCopyImage2d                     = 91,
    ScaledCopyImage3d                     = 92,
    YuvIntToRgb                           = 93,
    YuvToRgb                              = 94,
    Gfx6GenerateCmdDispatch               = 95,
    Gfx6GenerateCmdDraw                   = 96,
    Gfx9BuildHtileLookupTable             = 97,
    Gfx9ClearDccMultiSample2d             = 98,
    Gfx9ClearDccOptimized2d               = 99,
    Gfx9ClearDccSingleSample2d            = 100,
    Gfx9ClearDccSingleSample3d            = 101,
    Gfx9ClearHtileFast                    = 102,
    Gfx9ClearHtileMultiSample             = 103,
    Gfx9ClearHtileOptimized2d             = 104,
    Gfx9ClearHtileSingleSample            = 105,
    Gfx9Fill4x4Dword                      = 106,
    Gfx9GenerateCmdDispatch               = 108,
    Gfx9GenerateCmdDraw                   = 109,
    Gfx9HtileCopyAndFixUp                 = 112,
    Gfx9InitCmaskSingleSample             = 113,
    Gfx10ClearDccComputeSetFirstPixel     = 114,
    Gfx10ClearDccComputeSetFirstPixelMsaa = 115,
    Gfx10GenerateCmdDispatch              = 116,
    Gfx10GenerateCmdDraw                  = 117,
    Count
};

Result CreateRpmComputePipelines(GfxDevice* pDevice, ComputePipeline** pPipelineMem);

} // Pal
