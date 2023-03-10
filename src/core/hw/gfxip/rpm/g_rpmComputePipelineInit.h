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

class ComputePipeline;
class GfxDevice;

// RPM Compute Pipelines. Used to index into RsrcProcMgr::m_pComputePipelines array
enum class RpmComputePipeline : uint32
{
    ClearBuffer                                     = 0,
    ClearImage1d                                    = 1,
    ClearImage1dTexelScale                          = 2,
    ClearImage2d                                    = 3,
    ClearImage2dTexelScale                          = 4,
    ClearImage3d                                    = 5,
    ClearImage3dTexelScale                          = 6,
    CopyBufferByte                                  = 7,
    CopyBufferDqword                                = 8,
    CopyBufferDword                                 = 9,
    CopyImage2d                                     = 10,
    CopyImage2dms2x                                 = 11,
    CopyImage2dms4x                                 = 12,
    CopyImage2dms8x                                 = 13,
    CopyImage2dShaderMipLevel                       = 14,
    CopyImageGammaCorrect2d                         = 15,
    CopyImgToMem1d                                  = 16,
    CopyImgToMem2d                                  = 17,
    CopyImgToMem2dms2x                              = 18,
    CopyImgToMem2dms4x                              = 19,
    CopyImgToMem2dms8x                              = 20,
    CopyImgToMem3d                                  = 21,
    CopyMemToImg1d                                  = 22,
    CopyMemToImg2d                                  = 23,
    CopyMemToImg2dms2x                              = 24,
    CopyMemToImg2dms4x                              = 25,
    CopyMemToImg2dms8x                              = 26,
    CopyMemToImg3d                                  = 27,
    CopyTypedBuffer1d                               = 28,
    CopyTypedBuffer2d                               = 29,
    CopyTypedBuffer3d                               = 30,
    ExpandMaskRam                                   = 31,
    ExpandMaskRamMs2x                               = 32,
    ExpandMaskRamMs4x                               = 33,
    ExpandMaskRamMs8x                               = 34,
    FastDepthClear                                  = 35,
    FastDepthExpClear                               = 36,
    FastDepthStExpClear                             = 37,
    FillMem4xDword                                  = 38,
    FillMemDword                                    = 39,
    GenerateMipmaps                                 = 40,
    GenerateMipmapsLowp                             = 41,
    HtileCopyAndFixUp                               = 42,
    HtileSR4xUpdate                                 = 43,
    HtileSRUpdate                                   = 44,
    MsaaFmaskCopyImage                              = 45,
    MsaaFmaskCopyImageOptimized                     = 46,
    MsaaFmaskCopyImgToMem                           = 47,
    MsaaFmaskExpand2x                               = 48,
    MsaaFmaskExpand4x                               = 49,
    MsaaFmaskExpand8x                               = 50,
    MsaaFmaskResolve1xEqaa                          = 51,
    MsaaFmaskResolve2x                              = 52,
    MsaaFmaskResolve2xEqaa                          = 53,
    MsaaFmaskResolve2xEqaaMax                       = 54,
    MsaaFmaskResolve2xEqaaMin                       = 55,
    MsaaFmaskResolve2xMax                           = 56,
    MsaaFmaskResolve2xMin                           = 57,
    MsaaFmaskResolve4x                              = 58,
    MsaaFmaskResolve4xEqaa                          = 59,
    MsaaFmaskResolve4xEqaaMax                       = 60,
    MsaaFmaskResolve4xEqaaMin                       = 61,
    MsaaFmaskResolve4xMax                           = 62,
    MsaaFmaskResolve4xMin                           = 63,
    MsaaFmaskResolve8x                              = 64,
    MsaaFmaskResolve8xEqaa                          = 65,
    MsaaFmaskResolve8xEqaaMax                       = 66,
    MsaaFmaskResolve8xEqaaMin                       = 67,
    MsaaFmaskResolve8xMax                           = 68,
    MsaaFmaskResolve8xMin                           = 69,
    MsaaFmaskScaledCopy                             = 70,
    MsaaResolve2x                                   = 71,
    MsaaResolve2xMax                                = 72,
    MsaaResolve2xMin                                = 73,
    MsaaResolve4x                                   = 74,
    MsaaResolve4xMax                                = 75,
    MsaaResolve4xMin                                = 76,
    MsaaResolve8x                                   = 77,
    MsaaResolve8xMax                                = 78,
    MsaaResolve8xMin                                = 79,
    MsaaResolveStencil2xMax                         = 80,
    MsaaResolveStencil2xMin                         = 81,
    MsaaResolveStencil4xMax                         = 82,
    MsaaResolveStencil4xMin                         = 83,
    MsaaResolveStencil8xMax                         = 84,
    MsaaResolveStencil8xMin                         = 85,
    MsaaScaledCopyImage2d                           = 86,
    ResolveOcclusionQuery                           = 87,
    ResolvePipelineStatsQuery                       = 88,
    ResolveStreamoutStatsQuery                      = 89,
    RgbToYuvPacked                                  = 90,
    RgbToYuvPlanar                                  = 91,
    ScaledCopyImage2d                               = 92,
    ScaledCopyImage3d                               = 93,
    YuvIntToRgb                                     = 94,
    YuvToRgb                                        = 95,
    Gfx6GenerateCmdDispatch                         = 96,
    Gfx6GenerateCmdDraw                             = 97,
    Gfx9BuildHtileLookupTable                       = 98,
    Gfx9ClearDccMultiSample2d                       = 99,
    Gfx9ClearDccOptimized2d                         = 100,
    Gfx9ClearDccSingleSample2d                      = 101,
    Gfx9ClearDccSingleSample3d                      = 102,
    Gfx9ClearHtileFast                              = 103,
    Gfx9ClearHtileMultiSample                       = 104,
    Gfx9ClearHtileOptimized2d                       = 105,
    Gfx9ClearHtileSingleSample                      = 106,
    Gfx9Fill4x4Dword                                = 107,
    Gfx9GenerateCmdDispatch                         = 109,
    Gfx9GenerateCmdDraw                             = 110,
    Gfx9HtileCopyAndFixUp                           = 113,
    Gfx9InitCmask                                   = 114,
    Gfx10BuildDccLookupTable                        = 115,
    Gfx10ClearDccComputeSetFirstPixel               = 116,
    Gfx10ClearDccComputeSetFirstPixelMsaa           = 117,
    Gfx10GenerateCmdDispatch                        = 118,
    Gfx10GenerateCmdDispatchTaskMesh                = 119,
    Gfx10GenerateCmdDraw                            = 120,
    Gfx10GfxDccToDisplayDcc                         = 123,
    Gfx10PrtPlusResolveResidencyMapDecode           = 126,
    Gfx10PrtPlusResolveResidencyMapEncode           = 127,
    Gfx10PrtPlusResolveSamplingStatusMap            = 128,
    Gfx10VrsHtile                                   = 129,
#if PAL_BUILD_NAVI31
    Gfx11GenerateCmdDispatchTaskMesh                = 132,
#endif
    Count
};

Result CreateRpmComputePipelines(GfxDevice* pDevice, ComputePipeline** pPipelineMem);

} // Pal
