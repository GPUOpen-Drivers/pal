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
    ClearImage3dAsThin                              = 6,
    ClearImage3dTexelScale                          = 7,
    CopyBufferByte                                  = 8,
    CopyBufferDqword                                = 9,
    CopyBufferDword                                 = 10,
    CopyImage2d                                     = 11,
    CopyImage2dMorton2x                             = 12,
    CopyImage2dMorton4x                             = 13,
    CopyImage2dMorton8x                             = 14,
    CopyImage2dms2x                                 = 15,
    CopyImage2dms4x                                 = 16,
    CopyImage2dms8x                                 = 17,
    CopyImage2dShaderMipLevel                       = 18,
    CopyImageGammaCorrect2d                         = 19,
    CopyImgToMem1d                                  = 20,
    CopyImgToMem2d                                  = 21,
    CopyImgToMem2dms2x                              = 22,
    CopyImgToMem2dms4x                              = 23,
    CopyImgToMem2dms8x                              = 24,
    CopyImgToMem3d                                  = 25,
    CopyMemToImg1d                                  = 26,
    CopyMemToImg2d                                  = 27,
    CopyMemToImg2dms2x                              = 28,
    CopyMemToImg2dms4x                              = 29,
    CopyMemToImg2dms8x                              = 30,
    CopyMemToImg3d                                  = 31,
    CopyTypedBuffer1d                               = 32,
    CopyTypedBuffer2d                               = 33,
    CopyTypedBuffer3d                               = 34,
    ExpandMaskRam                                   = 35,
    ExpandMaskRamMs2x                               = 36,
    ExpandMaskRamMs4x                               = 37,
    ExpandMaskRamMs8x                               = 38,
    FastDepthClear                                  = 39,
    FastDepthExpClear                               = 40,
    FastDepthStExpClear                             = 41,
    FillMem4xDword                                  = 42,
    FillMemDword                                    = 43,
    GenerateMipmaps                                 = 44,
    GenerateMipmapsLowp                             = 45,
    HtileCopyAndFixUp                               = 46,
    HtileSR4xUpdate                                 = 47,
    HtileSRUpdate                                   = 48,
    MsaaFmaskCopyImage                              = 49,
    MsaaFmaskCopyImageOptimized                     = 50,
    MsaaFmaskCopyImgToMem                           = 51,
    MsaaFmaskExpand2x                               = 52,
    MsaaFmaskExpand4x                               = 53,
    MsaaFmaskExpand8x                               = 54,
    MsaaFmaskResolve1xEqaa                          = 55,
    MsaaFmaskResolve2x                              = 56,
    MsaaFmaskResolve2xEqaa                          = 57,
    MsaaFmaskResolve2xEqaaMax                       = 58,
    MsaaFmaskResolve2xEqaaMin                       = 59,
    MsaaFmaskResolve2xMax                           = 60,
    MsaaFmaskResolve2xMin                           = 61,
    MsaaFmaskResolve4x                              = 62,
    MsaaFmaskResolve4xEqaa                          = 63,
    MsaaFmaskResolve4xEqaaMax                       = 64,
    MsaaFmaskResolve4xEqaaMin                       = 65,
    MsaaFmaskResolve4xMax                           = 66,
    MsaaFmaskResolve4xMin                           = 67,
    MsaaFmaskResolve8x                              = 68,
    MsaaFmaskResolve8xEqaa                          = 69,
    MsaaFmaskResolve8xEqaaMax                       = 70,
    MsaaFmaskResolve8xEqaaMin                       = 71,
    MsaaFmaskResolve8xMax                           = 72,
    MsaaFmaskResolve8xMin                           = 73,
    MsaaFmaskScaledCopy                             = 74,
    MsaaResolve2x                                   = 75,
    MsaaResolve2xMax                                = 76,
    MsaaResolve2xMin                                = 77,
    MsaaResolve4x                                   = 78,
    MsaaResolve4xMax                                = 79,
    MsaaResolve4xMin                                = 80,
    MsaaResolve8x                                   = 81,
    MsaaResolve8xMax                                = 82,
    MsaaResolve8xMin                                = 83,
    MsaaResolveStencil2xMax                         = 84,
    MsaaResolveStencil2xMin                         = 85,
    MsaaResolveStencil4xMax                         = 86,
    MsaaResolveStencil4xMin                         = 87,
    MsaaResolveStencil8xMax                         = 88,
    MsaaResolveStencil8xMin                         = 89,
    MsaaScaledCopyImage2d                           = 90,
    ResolveOcclusionQuery                           = 91,
    ResolvePipelineStatsQuery                       = 92,
    ResolveStreamoutStatsQuery                      = 93,
    RgbToYuvPacked                                  = 94,
    RgbToYuvPlanar                                  = 95,
    ScaledCopyImage2d                               = 96,
    ScaledCopyImage2dMorton2x                       = 97,
    ScaledCopyImage2dMorton4x                       = 98,
    ScaledCopyImage2dMorton8x                       = 99,
    ScaledCopyImage3d                               = 100,
    ScaledCopyTypedBufferToImg2D                    = 101,
    YuvIntToRgb                                     = 102,
    YuvToRgb                                        = 103,
    Gfx6GenerateCmdDispatch                         = 104,
    Gfx6GenerateCmdDraw                             = 105,
    Gfx9BuildHtileLookupTable                       = 106,
    Gfx9ClearDccMultiSample2d                       = 107,
    Gfx9ClearDccOptimized2d                         = 108,
    Gfx9ClearDccSingleSample2d                      = 109,
    Gfx9ClearDccSingleSample3d                      = 110,
    Gfx9ClearHtileFast                              = 111,
    Gfx9ClearHtileMultiSample                       = 112,
    Gfx9ClearHtileOptimized2d                       = 113,
    Gfx9ClearHtileSingleSample                      = 114,
    Gfx9Fill4x4Dword                                = 115,
    Gfx9GenerateCmdDispatch                         = 117,
    Gfx9GenerateCmdDraw                             = 118,
    Gfx9HtileCopyAndFixUp                           = 121,
    Gfx9InitCmask                                   = 122,
    Gfx10BuildDccLookupTable                        = 123,
    Gfx10ClearDccComputeSetFirstPixel               = 124,
    Gfx10ClearDccComputeSetFirstPixelMsaa           = 125,
    Gfx10GenerateCmdDispatch                        = 126,
    Gfx10GenerateCmdDispatchTaskMesh                = 127,
    Gfx10GenerateCmdDraw                            = 128,
    Gfx10GfxDccToDisplayDcc                         = 131,
    Gfx10PrtPlusResolveResidencyMapDecode           = 134,
    Gfx10PrtPlusResolveResidencyMapEncode           = 135,
    Gfx10PrtPlusResolveSamplingStatusMap            = 136,
    Gfx10VrsHtile                                   = 137,
#if PAL_BUILD_NAVI31|| PAL_BUILD_NAVI33|| PAL_BUILD_PHOENIX1
    Gfx11GenerateCmdDispatchTaskMesh                = 140,
#endif
    Count
};

Result CreateRpmComputePipelines(GfxDevice* pDevice, ComputePipeline** pPipelineMem);

} // Pal
