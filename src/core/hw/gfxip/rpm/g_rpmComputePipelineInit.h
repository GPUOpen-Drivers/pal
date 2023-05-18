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
    CopyImage2dMorton2x                             = 11,
    CopyImage2dMorton4x                             = 12,
    CopyImage2dMorton8x                             = 13,
    CopyImage2dms2x                                 = 14,
    CopyImage2dms4x                                 = 15,
    CopyImage2dms8x                                 = 16,
    CopyImage2dShaderMipLevel                       = 17,
    CopyImageGammaCorrect2d                         = 18,
    CopyImgToMem1d                                  = 19,
    CopyImgToMem2d                                  = 20,
    CopyImgToMem2dms2x                              = 21,
    CopyImgToMem2dms4x                              = 22,
    CopyImgToMem2dms8x                              = 23,
    CopyImgToMem3d                                  = 24,
    CopyMemToImg1d                                  = 25,
    CopyMemToImg2d                                  = 26,
    CopyMemToImg2dms2x                              = 27,
    CopyMemToImg2dms4x                              = 28,
    CopyMemToImg2dms8x                              = 29,
    CopyMemToImg3d                                  = 30,
    CopyTypedBuffer1d                               = 31,
    CopyTypedBuffer2d                               = 32,
    CopyTypedBuffer3d                               = 33,
    ExpandMaskRam                                   = 34,
    ExpandMaskRamMs2x                               = 35,
    ExpandMaskRamMs4x                               = 36,
    ExpandMaskRamMs8x                               = 37,
    FastDepthClear                                  = 38,
    FastDepthExpClear                               = 39,
    FastDepthStExpClear                             = 40,
    FillMem4xDword                                  = 41,
    FillMemDword                                    = 42,
    GenerateMipmaps                                 = 43,
    GenerateMipmapsLowp                             = 44,
    HtileCopyAndFixUp                               = 45,
    HtileSR4xUpdate                                 = 46,
    HtileSRUpdate                                   = 47,
    MsaaFmaskCopyImage                              = 48,
    MsaaFmaskCopyImageOptimized                     = 49,
    MsaaFmaskCopyImgToMem                           = 50,
    MsaaFmaskExpand2x                               = 51,
    MsaaFmaskExpand4x                               = 52,
    MsaaFmaskExpand8x                               = 53,
    MsaaFmaskResolve1xEqaa                          = 54,
    MsaaFmaskResolve2x                              = 55,
    MsaaFmaskResolve2xEqaa                          = 56,
    MsaaFmaskResolve2xEqaaMax                       = 57,
    MsaaFmaskResolve2xEqaaMin                       = 58,
    MsaaFmaskResolve2xMax                           = 59,
    MsaaFmaskResolve2xMin                           = 60,
    MsaaFmaskResolve4x                              = 61,
    MsaaFmaskResolve4xEqaa                          = 62,
    MsaaFmaskResolve4xEqaaMax                       = 63,
    MsaaFmaskResolve4xEqaaMin                       = 64,
    MsaaFmaskResolve4xMax                           = 65,
    MsaaFmaskResolve4xMin                           = 66,
    MsaaFmaskResolve8x                              = 67,
    MsaaFmaskResolve8xEqaa                          = 68,
    MsaaFmaskResolve8xEqaaMax                       = 69,
    MsaaFmaskResolve8xEqaaMin                       = 70,
    MsaaFmaskResolve8xMax                           = 71,
    MsaaFmaskResolve8xMin                           = 72,
    MsaaFmaskScaledCopy                             = 73,
    MsaaResolve2x                                   = 74,
    MsaaResolve2xMax                                = 75,
    MsaaResolve2xMin                                = 76,
    MsaaResolve4x                                   = 77,
    MsaaResolve4xMax                                = 78,
    MsaaResolve4xMin                                = 79,
    MsaaResolve8x                                   = 80,
    MsaaResolve8xMax                                = 81,
    MsaaResolve8xMin                                = 82,
    MsaaResolveStencil2xMax                         = 83,
    MsaaResolveStencil2xMin                         = 84,
    MsaaResolveStencil4xMax                         = 85,
    MsaaResolveStencil4xMin                         = 86,
    MsaaResolveStencil8xMax                         = 87,
    MsaaResolveStencil8xMin                         = 88,
    MsaaScaledCopyImage2d                           = 89,
    ResolveOcclusionQuery                           = 90,
    ResolvePipelineStatsQuery                       = 91,
    ResolveStreamoutStatsQuery                      = 92,
    RgbToYuvPacked                                  = 93,
    RgbToYuvPlanar                                  = 94,
    ScaledCopyImage2d                               = 95,
    ScaledCopyImage3d                               = 96,
    YuvIntToRgb                                     = 97,
    YuvToRgb                                        = 98,
    Gfx6GenerateCmdDispatch                         = 99,
    Gfx6GenerateCmdDraw                             = 100,
    Gfx9BuildHtileLookupTable                       = 101,
    Gfx9ClearDccMultiSample2d                       = 102,
    Gfx9ClearDccOptimized2d                         = 103,
    Gfx9ClearDccSingleSample2d                      = 104,
    Gfx9ClearDccSingleSample3d                      = 105,
    Gfx9ClearHtileFast                              = 106,
    Gfx9ClearHtileMultiSample                       = 107,
    Gfx9ClearHtileOptimized2d                       = 108,
    Gfx9ClearHtileSingleSample                      = 109,
    Gfx9Fill4x4Dword                                = 110,
    Gfx9GenerateCmdDispatch                         = 112,
    Gfx9GenerateCmdDraw                             = 113,
    Gfx9HtileCopyAndFixUp                           = 116,
    Gfx9InitCmask                                   = 117,
    Gfx10BuildDccLookupTable                        = 118,
    Gfx10ClearDccComputeSetFirstPixel               = 119,
    Gfx10ClearDccComputeSetFirstPixelMsaa           = 120,
    Gfx10GenerateCmdDispatch                        = 121,
    Gfx10GenerateCmdDispatchTaskMesh                = 122,
    Gfx10GenerateCmdDraw                            = 123,
    Gfx10GfxDccToDisplayDcc                         = 126,
    Gfx10PrtPlusResolveResidencyMapDecode           = 129,
    Gfx10PrtPlusResolveResidencyMapEncode           = 130,
    Gfx10PrtPlusResolveSamplingStatusMap            = 131,
    Gfx10VrsHtile                                   = 132,
#if PAL_BUILD_NAVI31
    Gfx11GenerateCmdDispatchTaskMesh                = 135,
#endif
    Count
};

Result CreateRpmComputePipelines(GfxDevice* pDevice, ComputePipeline** pPipelineMem);

} // Pal
