/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/cmdStream.h"
#include "core/hw/gfxip/gfxCmdBuffer.h"
#include "core/hw/gfxip/rpm/g_rpmComputePipelineInit.h"
#include "core/hw/gfxip/rpm/g_rpmGfxPipelineInit.h"
#include "core/hw/gfxip/rpm/rpmUtil.h"
#include "palFormatInfo.h"
#include "palGpuMemory.h"
#include "palMath.h"

#include <limits.h>

using namespace Util;
using namespace Util::Math;

namespace Pal
{
namespace RpmUtil
{

// Lookup table containing the Pipeline and Image-View information for each (Y,Cb,Cr) component of a YUV image when
// doing color-space-conversion blits.
const ColorSpaceConversionInfo CscInfoTable[YuvFormatCount] =
{
    // Note: YUV packed formats are treated as YUV planar formats in the RPM shaders which convert YUV-to-RGB. The
    // reason for this is because they often have different sampling rates for Y and for UV, so we still need separate
    // SRD's for luminance and chrominance. These pseudo-planes are faked by creating Image-views of the whole Image,
    // but using the channel mappings to fake the behavior of separate image planes.

    // AYUV (4:4:4 packed)
    {
        RpmComputePipeline::YuvIntToRgb,
        {
            {   0,                          // Y pseudo-plane
                { ChNumFormat::X8Y8Z8W8_Unorm,
                  { ChannelSwizzle::Z, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::W },
                }
            },
            {   0,                          // CbCr pseudo-plane
                { ChNumFormat::X8Y8Z8W8_Unorm,
                  { ChannelSwizzle::Zero, ChannelSwizzle::Y, ChannelSwizzle::X, ChannelSwizzle::One },
                }
            },
        },
        // Note: For RGB to YUV conversions, we treat AYUV as a planar format with a single plane, because the luma
        // and chroma sampling rates are the same. The RgbToYuvPacked shader is intended to handle with macro-pixel
        // packed formats.
        RpmComputePipeline::RgbToYuvPlanar,
        {
            {   0,
                { ChNumFormat::X8Y8Z8W8_Unorm,
                  { ChannelSwizzle::Z, ChannelSwizzle::Y, ChannelSwizzle::X, ChannelSwizzle::W }, },
                0.5f, 0.5f,
                { 0, 1, 2, },
            },
        },
    },
    // UYVY (4:2:2 packed)
    {
        RpmComputePipeline::YuvIntToRgb,
        {
            {   0,                          // Y pseudo-plane
                { ChNumFormat::X8Y8_Z8Y8_Unorm,
                  { ChannelSwizzle::Y, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One },
                },
            },
            {   0,                          // CbCr pseudo-plane
                { ChNumFormat::X8Y8_Z8Y8_Unorm,
                  { ChannelSwizzle::Zero, ChannelSwizzle::X, ChannelSwizzle::Z, ChannelSwizzle::One },
                },
            },
        },
        RpmComputePipeline::RgbToYuvPacked,
        {
            {   0,
                { ChNumFormat::X16_Uint,
                  { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::Zero }, },
                0.25f, 0.5f,
                { 0, 1, 2, },
            },
        },
    },
    // VYUY (4:2:2 packed)
    {
        RpmComputePipeline::YuvIntToRgb,
        {
            {   0,                          // Y pseudo-plane
                { ChNumFormat::X8Y8_Z8Y8_Unorm,
                  { ChannelSwizzle::Y, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One },
                },
            },
            {   0,                          // CbCr pseudo-plane
                { ChNumFormat::X8Y8_Z8Y8_Unorm,
                  { ChannelSwizzle::Zero, ChannelSwizzle::Z, ChannelSwizzle::X, ChannelSwizzle::One },
                },
            },
        },
        RpmComputePipeline::RgbToYuvPacked,
        {
            {   0,
                { ChNumFormat::X16_Uint,
                  { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::Zero }, },
                0.25f, 0.5f,
                { 0, 2, 1, },
            },
        },
    },
    // YUY2 (4:2:2 packed)
    {
        RpmComputePipeline::YuvIntToRgb,
        {
            {   0,                          // Y pseudo-plane
                { ChNumFormat::Y8X8_Y8Z8_Unorm,
                  { ChannelSwizzle::Y, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One },
                },
            },
            {   0,                          // CbCr pseudo-plane
                { ChNumFormat::Y8X8_Y8Z8_Unorm,
                  { ChannelSwizzle::Zero, ChannelSwizzle::X, ChannelSwizzle::Z, ChannelSwizzle::One },
                },
            },
        },
        RpmComputePipeline::RgbToYuvPacked,
        {
            {   0,
                { ChNumFormat::X16_Uint,
                  { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::Zero }, },
                0.25f, 0.5f,
                { 0, 1, 2, },
            },
        },
    },
    // YVY2 (4:2:2 packed)
    {
        RpmComputePipeline::YuvIntToRgb,
        {
            {   0,                          // Y pseudo-plane
                { ChNumFormat::Y8X8_Y8Z8_Unorm,
                  { ChannelSwizzle::Y, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One },
                },
            },
            {   0,                          // CbCr pseudo-plane
                { ChNumFormat::Y8X8_Y8Z8_Unorm,
                  { ChannelSwizzle::Zero, ChannelSwizzle::Z, ChannelSwizzle::X, ChannelSwizzle::One },
                }
            },
        },
        RpmComputePipeline::RgbToYuvPacked,
        {
            {   0,
                { ChNumFormat::X16_Uint,
                  { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::Zero }, },
                0.25f, 0.5f,
                { 0, 2, 1, },
            },
        },
    },
    // YV12 (4:2:0 planar)
    {
        // YV12 has three planes in YVU (YCrCb) order. Our yuv to rgb conversion tables are expected to be in the
        // format YUV -> RGB, so the planes of the source image must be swizzled to produce the following
        // conversion:
        //         src                                          dst
        //   dot( [plane#0 Y plane#2 V plane#1 U], [row#0] ) = [plane#0 R]
        //   dot( [plane#0 Y plane#2 V plane#1 U], [row#1] ) = [plane#0 G]
        //   dot( [plane#0 Y plane#2 V plane#1 U], [row#2] ) = [plane#0 B]
        RpmComputePipeline::YuvToRgb,
        {
            {   0,                          // Y plane
                { ChNumFormat::X8_MM_Unorm,
                  { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One },
                },
            },
            {   2,                          // Cb plane
                { ChNumFormat::X8_MM_Unorm,
                  { ChannelSwizzle::Zero, ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::One },
                },
            },
            {   1,                          // Cr plane
                { ChNumFormat::X8_MM_Unorm,
                  { ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::X, ChannelSwizzle::One },
                },
            },
        },
        // YV12 has three planes in YVU (YCrCb) order. Our rgb to yuv conversion tables are expected to be in the
        // format RGB -> YUV, so the planes of the destination image must be swizzled to produce the following
        // conversion:
        //         src                     dst
        //   dot( [plane#0 R plane#0 G plane#0 B], [row#0] ) = [plane#0 Y]
        //   dot( [plane#0 R plane#0 G plane#0 B], [row#1] ) = [plane#2 U]
        //   dot( [plane#0 R plane#0 G plane#0 B], [row#2] ) = [plane#1 V]
        RpmComputePipeline::RgbToYuvPlanar,
        {
            {   0,                          // Y plane
                { ChNumFormat::X8_MM_Unorm,
                  { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::Zero }, },
                0.5f, 0.5f,
                { 0, USHRT_MAX, USHRT_MAX, },
            },
            {   2,                          // Cb plane
                { ChNumFormat::X8_MM_Unorm,
                  { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::Zero }, },
                0.25f, 0.5f,               // Mpeg-2 chroma subsampling location
                { 1, USHRT_MAX, USHRT_MAX, },
            },
            {   1,                          // Cr plane
                { ChNumFormat::X8_MM_Unorm,
                  { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::Zero }, },
                0.25f, 0.5f,               // Mpeg-2 chroma subsampling location
                { 2, USHRT_MAX, USHRT_MAX, },
            },
        },
    },
    // NV11 (4:1:1 planar)
    {
        RpmComputePipeline::YuvIntToRgb,
        {
            {   0,                          // Y plane
                { ChNumFormat::X8_MM_Unorm,
                  { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One },
                },
            },
            {   1,                          // CbCr plane
                { ChNumFormat::X8Y8_MM_Unorm,
                  { ChannelSwizzle::Zero, ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::One },
                }
            },
        },
        RpmComputePipeline::RgbToYuvPlanar,
        {
            {   0,                          // Y plane
                { ChNumFormat::X8_MM_Unorm,
                  { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::Zero }, },
                0.5f, 0.5f,
                { 0, USHRT_MAX, USHRT_MAX, },
            },
            {   1,                          // CbCr plane
                { ChNumFormat::X8Y8_MM_Unorm,
                  { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Zero, ChannelSwizzle::Zero }, },
                0.5f, 0.5f,
                { 1, 2, USHRT_MAX, },
            },
        },
    },
    // NV12 (4:2:0 planar)
    {
        RpmComputePipeline::YuvIntToRgb,
        {
            {   0,                          // Y plane
                { ChNumFormat::X8_MM_Unorm,
                  { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One },
                },
            },
            {   1,                          // CbCr plane
                { ChNumFormat::X8Y8_MM_Unorm,
                  { ChannelSwizzle::Zero, ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Zero },
                },
            },
        },
        RpmComputePipeline::RgbToYuvPlanar,
        {
            {   0,                          // Y plane
                { ChNumFormat::X8_MM_Unorm,
                  { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::Zero }, },
                0.5f, 0.5f,
                { 0, USHRT_MAX, USHRT_MAX, },
            },
            {   1,                          // CbCr plane
                { ChNumFormat::X8Y8_MM_Unorm,
                  { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Zero, ChannelSwizzle::Zero }, },
                0.25f, 0.5f,                // Mpeg-2 chroma subsampling location
                { 1, 2, USHRT_MAX, },
            },
        },
    },
    // NV21 (4:2:0 planar)
    {
        RpmComputePipeline::YuvIntToRgb,
        {
            {   0,                          // Y plane
                { ChNumFormat::X8_MM_Unorm,
                  { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One },
                },
            },
            {   1,                          // CbCr plane
                { ChNumFormat::X8Y8_MM_Unorm,
                  { ChannelSwizzle::Zero, ChannelSwizzle::Y, ChannelSwizzle::X, ChannelSwizzle::One },
                }
            },
        },
        RpmComputePipeline::RgbToYuvPlanar,
        {
            {   0,                          // Y plane
                { ChNumFormat::X8_MM_Unorm,
                  { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::Zero }, },
                0.5f, 0.5f,
                { 0, USHRT_MAX, USHRT_MAX, },
            },
            {   1,                          // CbCr plane
                { ChNumFormat::X8Y8_MM_Unorm,
                  { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Zero, ChannelSwizzle::Zero }, },
                0.25f, 0.5f,                // Mpeg-2 chroma subsampling location
                { 2, 1, USHRT_MAX, },
            },
        },
    },
    // P016 (4:2:0 planar)
    {
        RpmComputePipeline::YuvIntToRgb,
        {
            {   0,                          // Y plane
                { ChNumFormat::X16_MM10_Unorm,
                  { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One },
                },
            },
            {   1,                          // CbCr plane
                { ChNumFormat::X16Y16_MM10_Unorm,
                  { ChannelSwizzle::Zero, ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::One },
                },
            },
        },
        RpmComputePipeline::RgbToYuvPlanar,
        {
            {   0,                          // Y plane
                { ChNumFormat::X16_MM10_Unorm,
                  { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::Zero }, },
                0.5f, 0.5f,
                { 0, USHRT_MAX, USHRT_MAX, },
            },
            {   1,                          // CbCr plane
                { ChNumFormat::X16Y16_MM10_Unorm,
                  { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Zero, ChannelSwizzle::Zero }, },
                0.25f, 0.5f,                // Mpeg-2 chroma subsampling location
                { 1, 2, USHRT_MAX, },
            },
        },
    },
    // P010 (4:2:0 planar)
    {
        RpmComputePipeline::YuvIntToRgb,
        {
            {   0,                          // Y plane
                { ChNumFormat::X16_MM10_Unorm,
                  { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One },
                },
            },
            {   1,                          // CbCr plane
                { ChNumFormat::X16Y16_MM10_Unorm,
                  { ChannelSwizzle::Zero, ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::One },
                },
            },
        },
        RpmComputePipeline::RgbToYuvPlanar,
        {
            {   0,                          // Y plane
                { ChNumFormat::X16_MM10_Unorm,
                  { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::Zero }, },
                0.5f, 0.5f,
                { 0, USHRT_MAX, USHRT_MAX, },
            },
            {   1,                          // CbCr plane
                { ChNumFormat::X16Y16_MM10_Unorm,
                  { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Zero, ChannelSwizzle::Zero }, },
                0.25f, 0.5f,                // Mpeg-2 chroma subsampling location
                { 1, 2, USHRT_MAX, },
            },
        },
    },
    // P210 (4:2:2 planar)
    {
        RpmComputePipeline::YuvIntToRgb,
        {
            {   0,                          // Y plane
                { ChNumFormat::X16_MM10_Unorm,
                  { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One },
                },
            },
            {   1,                          // CbCr plane
                { ChNumFormat::X16Y16_MM10_Unorm,
                  { ChannelSwizzle::Zero, ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::One },
                },
            },
        },
        RpmComputePipeline::RgbToYuvPlanar,
        {
            {   0,                          // Y plane
                { ChNumFormat::X16_MM10_Unorm,
                  { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::Zero }, },
                0.5f, 0.5f,
                { 0, USHRT_MAX, USHRT_MAX, },
            },
            {   1,                          // CbCr plane
                { ChNumFormat::X16Y16_MM10_Unorm,
                  { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Zero, ChannelSwizzle::Zero }, },
                0.5f, 0.5f,                 // Mpeg-2 chroma subsampling location
                { 1, 2, USHRT_MAX, },
            },
        },
    },
    {}, // X8_MM_Unorm
    {}, // X8_MM_Uint
    {}, // X8Y8_MM_Unorm
    {}, // X8Y8_MM_Uint
    {}, // X16_MM10_Unorm
    {}, // X16_MM10_Uint
    {}, // X16Y16_MM10_Unorm
    {}, // X16Y16_MM10_Uint
    // P208 (4:2:2 planar)
    {
        RpmComputePipeline::YuvIntToRgb,
        {
            {   0,                          // Y plane
                { ChNumFormat::X8_MM_Unorm,
                  { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One },
                },
            },
            {   1,                          // CbCr plane
                { ChNumFormat::X8Y8_MM_Unorm,
                  { ChannelSwizzle::Zero, ChannelSwizzle::Y, ChannelSwizzle::X, ChannelSwizzle::One },
                }
            },
        },
        RpmComputePipeline::RgbToYuvPlanar,
        {
            {   0,                          // Y plane
                { ChNumFormat::X8_MM_Unorm,
                  { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::Zero }, },
                0.5f, 0.5f,
                { 0, USHRT_MAX, USHRT_MAX, },
            },
            {   1,                          // CbCr plane
                { ChNumFormat::X8Y8_MM_Unorm,
                  { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Zero, ChannelSwizzle::Zero }, },
                0.25f, 0.5f,                // Mpeg-2 chroma subsampling location
                { 1, 2, USHRT_MAX, },
            },
        },
    },
    {}, // X16_MM12_Unorm
    {}, // X16_MM12_Uint
    {}, // X16Y16_MM12_Unorm
    {}, // X16Y16_MM12_Uint
    // P012 (12-bit 4:2:0 planar)
    {
        RpmComputePipeline::YuvIntToRgb,
        {
            {   0,                          // Y plane
                { ChNumFormat::X16_MM12_Unorm,
                  { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One },
                },
            },
            {   1,                          // CbCr plane
                { ChNumFormat::X16Y16_MM12_Unorm,
                  { ChannelSwizzle::Zero, ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::One },
                },
            },
        },
        RpmComputePipeline::RgbToYuvPlanar,
        {
            {   0,                          // Y plane
                { ChNumFormat::X16_MM12_Unorm,
                  { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::Zero }, },
                0.5f, 0.5f,
                { 0, USHRT_MAX, USHRT_MAX, },
            },
            {   1,                          // CbCr plane
                { ChNumFormat::X16Y16_MM12_Unorm,
                  { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Zero, ChannelSwizzle::Zero }, },
                0.25f, 0.5f,                // Mpeg-2 chroma subsampling location
                { 1, 2, USHRT_MAX, },
            },
        },
    },
    // P212 (12-bit 4:2:2 planar)
    {
        RpmComputePipeline::YuvIntToRgb,
        {
            {   0,                          // Y plane
                { ChNumFormat::X16_MM12_Unorm,
                  { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One },
                },
            },
            {   1,                          // CbCr plane
                { ChNumFormat::X16Y16_MM12_Unorm,
                  { ChannelSwizzle::Zero, ChannelSwizzle::Y, ChannelSwizzle::X, ChannelSwizzle::One },
                }
            },
        },
        RpmComputePipeline::RgbToYuvPlanar,
        {
            {   0,                          // Y plane
                { ChNumFormat::X16_MM12_Unorm,
                  { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::Zero }, },
                0.5f, 0.5f,
                { 0, USHRT_MAX, USHRT_MAX, },
            },
            {   1,                          // CbCr plane
                { ChNumFormat::X16Y16_MM12_Unorm,
                  { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Zero, ChannelSwizzle::Zero }, },
                0.5f, 0.5f,                // SMPTE 4:2:2 chroma subsampling location
                { 2, 1, USHRT_MAX, },
            },
        },
    },
    // P412 (12-bit 4:4:4 planar)
    {
        RpmComputePipeline::YuvToRgb,
        {
            {   0,                          // Y plane
                { ChNumFormat::X16_MM12_Unorm,
                  { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One },
                },
            },
            {   1,                          // Cb plane
                { ChNumFormat::X16_MM12_Unorm,
                  { ChannelSwizzle::Zero, ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::One },
                }
            },
            {   2,                          // Cr plane
                { ChNumFormat::X16_MM12_Unorm,
                  { ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::X, ChannelSwizzle::One },
                }
            },
        },
        RpmComputePipeline::RgbToYuvPlanar,
        {
            {   0,                          // Y plane
                { ChNumFormat::X16_MM12_Unorm,
                  { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::Zero }, },
                0.5f, 0.5f,
                { 0, USHRT_MAX, USHRT_MAX, },
            },
            {   1,                          // Cb plane
                { ChNumFormat::X16_MM12_Unorm,
                  { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::Zero }, },
                0.5f, 0.5f,
                { 0, USHRT_MAX, USHRT_MAX, },
            },
            {   2,                          // Cr plane
                { ChNumFormat::X16_MM12_Unorm,
                  { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::Zero }, },
                0.5f, 0.5f,
                { 0, USHRT_MAX, USHRT_MAX, },
            },
        },
    },
};

// =====================================================================================================================
// Sets up a color-space conversion matrix for converting RGB data to YUV. The rows of the matrix are "swizzled" based
// on the supplied channel mapping -- this is due to the fact that the channel mappings aren't always honored for UAV
// store operations. But we can simulate a swizzled UAV store to the YUV image planes by swapping the rows of the matrix
// used to convert between color spaces.
void SetupRgbToYuvCscTable(
    ChNumFormat                      format,
    uint32                           pass,
    const ColorSpaceConversionTable& cscTable,
    RgbYuvConversionInfo*            pInfo)
{
    constexpr size_t RowBytes = (sizeof(float) * 4);
    constexpr uint32 RowCount = 3;

    const auto& cscViewInfo = RpmUtil::CscInfoTable[static_cast<uint32>(format) -
                                                    static_cast<uint32>(ChNumFormat::AYUV)].viewInfoRgbToYuv[pass];

    for (uint32 row = 0; row < RowCount; ++row)
    {
        const uint16 swizzledRow = cscViewInfo.matrixRowOrder[row];
        if (swizzledRow == USHRT_MAX)
        {
            memset(&pInfo->cscTable[row][0], 0, RowBytes);
        }
        else
        {
            memcpy(&pInfo->cscTable[row][0], &cscTable.table[swizzledRow][0], RowBytes);
        }
    }
}

//======================================================================================================================
// Swaps the default format used for YUV planes with MM formats
void SwapIncompatibleMMFormat(
    const Device*   pDevice,
    SwizzledFormat* pFormat)
{
    if (Formats::IsMmFormat(pFormat->format) && (pDevice->SupportsFormat(pFormat->format) == false))
    {
        switch (pFormat->format)
        {
        case ChNumFormat::X8_MM_Unorm:
            pFormat->format = ChNumFormat::X8_Unorm;
            break;
        case ChNumFormat::X8_MM_Uint:
            pFormat->format = ChNumFormat::X8_Uint;
            break;
        case ChNumFormat::X8Y8_MM_Unorm:
            pFormat->format = ChNumFormat::X8Y8_Unorm;
            break;
        case ChNumFormat::X8Y8_MM_Uint:
            pFormat->format = ChNumFormat::X8Y8_Uint;
            break;
        case ChNumFormat::X16_MM10_Unorm:
        case ChNumFormat::X16_MM12_Unorm:
            pFormat->format = ChNumFormat::X16_Unorm;
            break;
        case ChNumFormat::X16_MM10_Uint:
        case ChNumFormat::X16_MM12_Uint:
            pFormat->format = ChNumFormat::X16_Uint;
            break;
        case ChNumFormat::X16Y16_MM10_Unorm:
        case ChNumFormat::X16Y16_MM12_Unorm:
            pFormat->format = ChNumFormat::X16Y16_Unorm;
            break;
        case ChNumFormat::X16Y16_MM10_Uint:
        case ChNumFormat::X16Y16_MM12_Uint:
            pFormat->format = ChNumFormat::X16Y16_Uint;
            break;

        default:
            PAL_ASSERT_ALWAYS_MSG("Unrecognized MM format!");
        }
    }
}

// =====================================================================================================================
// Populates a raw BufferViewInfo that wraps the specified GPU memory address range.
void BuildRawBufferViewInfo(
    BufferViewInfo* pInfo,
    const Device&   device,
    gpusize         gpuVirtAddr,
    gpusize         sizeInBytes,
    bool            isCompressed)
{
    const auto* pPublicSettings = device.GetPublicSettings();

    pInfo->gpuAddr = gpuVirtAddr;
    pInfo->range   = sizeInBytes;
    pInfo->stride  = 1;
    pInfo->swizzledFormat = UndefinedSwizzledFormat;

    pInfo->flags.bypassMallRead =
        TestAnyFlagSet(pPublicSettings->rpmViewsBypassMall, RpmViewsBypassMallOnRead);
    pInfo->flags.bypassMallWrite =
        TestAnyFlagSet(pPublicSettings->rpmViewsBypassMall, RpmViewsBypassMallOnWrite);
}

// =====================================================================================================================
// Populates a raw BufferViewInfo that wraps the entire provided memory object.
void BuildRawBufferViewInfo(
    BufferViewInfo*   pInfo,
    const GpuMemory&  bufferMemory,
    gpusize           byteOffset)
{
    const auto& desc = bufferMemory.Desc();

    BuildRawBufferViewInfo(pInfo,
                           *bufferMemory.GetDevice(),
                           (desc.gpuVirtAddr + byteOffset),
                           (desc.size - byteOffset),
                           false
                           );
}

// =====================================================================================================================
// Populates a raw BufferViewInfo that wraps an explicit range of the provided memory object.
void BuildRawBufferViewInfo(
    BufferViewInfo*   pInfo,
    const GpuMemory&  bufferMemory,
    gpusize           byteOffset,
    gpusize           range)
{
    BuildRawBufferViewInfo(pInfo,
                           *bufferMemory.GetDevice(),
                           (bufferMemory.Desc().gpuVirtAddr + byteOffset),
                           range,
                           false
                           );
}

// =====================================================================================================================
// Populates an ImageViewInfo that wraps the given range of the provided image object.
void BuildImageViewInfo(
    ImageViewInfo*     pInfo,
    const Image&       image,
    const SubresRange& subresRange,
    SwizzledFormat     swizzledFormat,
    ImageLayout        imgLayout,
    ImageTexOptLevel   texOptLevel,
    bool               isShaderWriteable)
{
    // We will static cast ImageType to ImageViewType so verify that it will work as expected.
    static_assert(static_cast<uint32>(ImageType::Tex1d) == static_cast<uint32>(ImageViewType::Tex1d),
                  "RPM assumes that ImageType::Tex1d == ImageViewType::Tex1d");
    static_assert(static_cast<uint32>(ImageType::Tex2d) == static_cast<uint32>(ImageViewType::Tex2d),
                  "RPM assumes that ImageType::Tex2d == ImageViewType::Tex2d");
    static_assert(static_cast<uint32>(ImageType::Tex3d) == static_cast<uint32>(ImageViewType::Tex3d),
                  "RPM assumes that ImageType::Tex3d == ImageViewType::Tex3d");

    pInfo->pImage          = &image;
    pInfo->viewType        = static_cast<ImageViewType>(image.GetImageCreateInfo().imageType);
    pInfo->minLod          = 0;
    pInfo->subresRange     = subresRange;
    pInfo->swizzledFormat  = swizzledFormat;
    pInfo->texOptLevel     = texOptLevel;
    pInfo->possibleLayouts = imgLayout;

    pInfo->possibleLayouts.usages |= (isShaderWriteable ? LayoutShaderWrite : 0u);

    const PalPublicSettings& settings = *image.GetDevice()->GetPublicSettings();

    pInfo->flags.bypassMallRead  = TestAnyFlagSet(settings.rpmViewsBypassMall, RpmViewsBypassMallOnRead);
    pInfo->flags.bypassMallWrite = TestAnyFlagSet(settings.rpmViewsBypassMall, RpmViewsBypassMallOnWrite);
}

// =====================================================================================================================
// Gets a raw UINT format that matches the bit depth of the provided format. Some formats may not have such a format
// in which case a smaller format is selected and the caller must dispatch extra threads.
SwizzledFormat GetRawFormat(
    ChNumFormat format,
    uint32*     pTexelScale,   // [out] If non-null, each texel requires this many raw format texels in the X dimension.
    bool*       pSingleSubres) // [out] If non-null, check if the format needs to access a single subres at a time.
{
    SwizzledFormat rawFormat  = UndefinedSwizzledFormat;
    uint32         texelScale = 1;
    bool           singleSubres = false;

    switch(Formats::BitsPerPixel(format))
    {
    case 8:
        rawFormat.format  = ChNumFormat::X8_Uint;
        rawFormat.swizzle =
            { Pal::ChannelSwizzle::X, Pal::ChannelSwizzle::Zero, Pal::ChannelSwizzle::Zero, Pal::ChannelSwizzle::One };
        break;
    case 16:
        rawFormat.format  = Pal::ChNumFormat::X16_Uint;
        rawFormat.swizzle =
            { Pal::ChannelSwizzle::X, Pal::ChannelSwizzle::Zero, Pal::ChannelSwizzle::Zero, Pal::ChannelSwizzle::One };
        break;
    case 32:
        rawFormat.format  = Pal::ChNumFormat::X32_Uint;
        rawFormat.swizzle =
            { Pal::ChannelSwizzle::X, Pal::ChannelSwizzle::Zero, Pal::ChannelSwizzle::Zero, Pal::ChannelSwizzle::One };
        break;
    case 64:
        rawFormat.format  = Pal::ChNumFormat::X32Y32_Uint;
        rawFormat.swizzle =
            { Pal::ChannelSwizzle::X, Pal::ChannelSwizzle::Y,    Pal::ChannelSwizzle::Zero, Pal::ChannelSwizzle::One };
        break;
    case 96:
        // There is no 96-bit raw format; fall back to R32 and copy each channel separately.
        rawFormat.format  = Pal::ChNumFormat::X32_Uint;
        rawFormat.swizzle =
            { Pal::ChannelSwizzle::X, Pal::ChannelSwizzle::Zero, Pal::ChannelSwizzle::Zero, Pal::ChannelSwizzle::One };
        texelScale        = 3;
        // On GFX9+ for 96bpp images the base address needs to access the exact mip/slice so they must be handled one
        // at a time.
        singleSubres      = true;
        break;
    case 128:
        rawFormat.format  = Pal::ChNumFormat::X32Y32Z32W32_Uint;
        rawFormat.swizzle =
            { Pal::ChannelSwizzle::X, Pal::ChannelSwizzle::Y,    Pal::ChannelSwizzle::Z,    Pal::ChannelSwizzle::W   };
        break;
    default:
        // Unknown bit depth.
        PAL_ASSERT_ALWAYS_MSG("Unknown bit depth %d for format %d", Formats::BitsPerPixel(format), format);
        break;
    }

    if (pTexelScale != nullptr)
    {
        *pTexelScale = texelScale;
    }
    else
    {
        // The caller is going to assume that it doesn't need to worry about texelScale, hopefully it's right.
        PAL_ASSERT(texelScale == 1);
    }

    if (pSingleSubres != nullptr)
    {
        *pSingleSubres = singleSubres;
    }

    return rawFormat;
}

// =====================================================================================================================
// Allocates embedded command space for the given number of DWORDs with the specified alignment. The space can be used
// by RPM for SRDs, inline constants, or nested descriptor tables. The GPU virtual address is written to the two user
// data registers starting at "registerToBind" for each shader stage in the "shaderStages" mask. Returns a CPU pointer
// to the embedded space.
uint32* CreateAndBindEmbeddedUserData(
    GfxCmdBuffer*     pCmdBuffer,
    uint32            sizeInDwords,
    uint32            alignmentInDwords,
    PipelineBindPoint bindPoint,
    uint32            entryToBind)
{
    gpusize      gpuVirtAddr = 0;
    uint32*const pCmdSpace   = pCmdBuffer->CmdAllocateEmbeddedData(sizeInDwords, alignmentInDwords, &gpuVirtAddr);
    PAL_ASSERT(pCmdSpace != nullptr);

    const uint32 gpuVirtAddrLo = LowPart(gpuVirtAddr);
    pCmdBuffer->CmdSetUserData(bindPoint, entryToBind, 1, &gpuVirtAddrLo);

    return pCmdSpace;
}

// =====================================================================================================================
// Input data is the output of the ConvertColorToX9Y9Z9E5 function; output data is the equivalent color data expressed
// as 32-bit IEEE-1394 floating point numbers.
void ConvertX9YZ9E5ToFloat(
    const uint32*  pColorIn,
    uint32*        pColorOut)
{
    constexpr uint32 MantissaBits = 9;  // Number of mantissa bits per component
    constexpr uint32 ExponentBias = 15; // Exponent bias

    for (uint32 i = 0; i < 3; i++) // only have RGB data
    {
        uint32  mantissa   = pColorIn[i];
        float   multiplier = 0.0f;

        for (uint32 bit = MantissaBits; (mantissa != 0); bit--)
        {
            if ((mantissa & 0x1) != 0)
            {
                multiplier += (1.0f / (1 << bit));
            }

            mantissa = mantissa >> 1;
        }

        float  finalVal = multiplier * (1 << (pColorIn[3] - ExponentBias));
        pColorOut[i]    = *(reinterpret_cast<uint32*>(&finalVal));
    }

    pColorOut[3] = 0x3F800000;
}

// =====================================================================================================================
// Input data is the output of the ConvertColorToX10Y10Z10W2 function; output data is the equivalent color data expressed
// as 32-bit IEEE-1394 floating point numbers.
void ConvertX10Y10Z10W2ToFloat(
    const uint32*  pColorIn,
    uint32*        pColorOut)
{
    for (uint32 i = 0; i < 3; i++) // RGB coversion
    {
        pColorOut[i] = Float10_6e4ToFloat32(pColorIn[i]);
    }

    pColorOut[3] = UFixedToFloat(pColorIn[3], 2, 0); // Alpha
}
// =====================================================================================================================
// Converts a color from clearFormat to its native format. The color array must contain four DWORDs in RGBA order.
void ConvertClearColorToNativeFormat(
    SwizzledFormat baseFormat,
    SwizzledFormat clearFormat,
    uint32*        pColor)
{
    // The clear color passed in from the app may have more bits than the format can hold. In this case we want to mask
    // off the the appropriate number of bits for the format to avoid the clear color being clamped to max value. This
    // matches the behavior of the compute path.
    const auto& formatInfo = Formats::FormatInfoTable[static_cast<uint32>(clearFormat.format)];

    if (clearFormat.format == ChNumFormat::X9Y9Z9E5_Float)
    {
        ConvertX9YZ9E5ToFloat(pColor, pColor);
    }
    else if (clearFormat.format == ChNumFormat::X10Y10Z10W2_Float)
    {
        ConvertX10Y10Z10W2ToFloat(pColor, pColor);
    }
    else
    {
        for (uint32 rgbaIdx = 0; rgbaIdx < 4; ++rgbaIdx)
        {
            // Figure out which component on the data format (if any) this RGBA component maps to.
            const ChannelSwizzle compSwizzle = baseFormat.swizzle.swizzle[rgbaIdx];

            uint32 compIdx;

            // Map from component swizzle enum to component index.
            if ((compSwizzle == ChannelSwizzle::Zero) || (compSwizzle == ChannelSwizzle::One))
            {
                compIdx = rgbaIdx;
            }
            else
            {
                compIdx = static_cast<uint32>(compSwizzle) - static_cast<uint32>(ChannelSwizzle::X);
            }

            // Get the bit count using compIdx as there may be a swizzle (Only occurs for A8 format).
            const uint32 bitCount = formatInfo.bitCount[compIdx];

            if (bitCount > 0)
            {
                const uint32 signBitMask       = static_cast<uint32>(1ULL << (bitCount - 1));
                const uint32 maxComponentValue = static_cast<uint32>((1ULL << bitCount) - 1);

                // Get the valid range of values on the given input component
                const uint32 maskedColor       = pColor[rgbaIdx] & maxComponentValue;

                // Convert from format's data representation back to 32-bit float/uint/sint
                if (Formats::IsDepthStencilOnly(clearFormat.format) || Formats::IsFloat(clearFormat.format))
                {
                    // Shaders only understand 32-bit floats, so we need to convert the raw color (which is in the
                    // bitness of the format) to 32-bit IEEE format here.
                    pColor[rgbaIdx] = FloatToBits(FloatNumBitsToFloat32(maskedColor, bitCount));
                }
                else if (Formats::IsUint(clearFormat.format))
                {
                    // This is the easy case, uint color data came in, uint color data going out, so the input color
                    // was already in the correct format, etc.
                    pColor[rgbaIdx] = maskedColor;
                }
                else if (Formats::IsSrgb(clearFormat.format) || Formats::IsUnorm(clearFormat.format))
                {
                    // Convert from fixed point to floating point
                    float floatColor = UFixedToFloat(maskedColor, 0, bitCount);

                    // Convert the gamma-corrected value back to linear output from the shader; if the clear format
                    // is SRGB, gamma correction will be reapplied during color output.  No gamma correction on alpha.
                    if (Formats::IsSrgb(clearFormat.format) && (rgbaIdx != 3))
                    {
                        floatColor = Formats::GammaToLinear(floatColor);
                    }

                    pColor[rgbaIdx] = FloatToBits(floatColor);
                }
                else if (Formats::IsSnorm(clearFormat.format))
                {
                    float floatColor = SFixedToFloat(static_cast<int32>(maskedColor), 0, bitCount);

                    pColor[rgbaIdx] = FloatToBits(floatColor);
                }
                else if (Formats::IsSint(clearFormat.format))
                {
                    // If this is really a negative number and the channel isn't already 32-bits wide, then we need to
                    // sign-extend this value as the shader only understands 32-bit numbers
                    if (((pColor[compIdx] & signBitMask) != 0) && (bitCount != 32))
                    {
                        pColor[rgbaIdx] |= (~maxComponentValue);
                    }
                }
                else
                {
                    // What is this?
                    PAL_ASSERT_ALWAYS();
                }
            }
        }
    }

}

// =====================================================================================================================
// Converts a floating-point representation of a color value in RGBA order to the appropriate bit representation for
// each channel, swizzles the color, packs it to a single element of the provided format, and stores it in the memory
// provided. For YUV formats, this will just call ConvertYuvColor(). A helper function to consolidate calls to the clear
// color manipulation functions in palFormatInfo.h
void ConvertAndPackClearColor(
    const ClearColor&     color,
    SwizzledFormat        imgFormat,
    SwizzledFormat        clearFormat,
    const SwizzledFormat* pRawFormat,
    uint32                plane,
    bool                  clearWithRawFmt,
    uint32*               pPackedColor)
{
    // First, pack the clear color into the raw format and write it to user data 1-4. We also build the write-disabled
    // bitmasks while we're dealing with clear color bit representations.
    if (color.type == ClearColorType::Yuv)
    {
        // If clear color type is Yuv, the image format should used to determine the clear color swizzling and packing
        // for planar YUV formats since the baseFormat is subresource's format which is not a YUV format.
        // NOTE: if clear color type is Uint, the client is responsible for:
        //       1. packing and swizzling clear color for packed YUV formats (e.g. packing in YUYV order for YUY2)
        //       2. passing correct clear color for this plane for planar YUV formats (e.g. two uint32s for U and V if
        //          current plane is CbCr).

        Formats::ConvertYuvColor(imgFormat, plane, color.u32Color, pPackedColor);

        // Not implemented for Yuv clears.
        PAL_ASSERT(color.disabledChannelMask == 0);
    }
    else
    {
        uint32 convertedColor[4] = {};
        if (color.type == ClearColorType::Float)
        {
            Formats::ConvertColor(clearFormat, color.f32Color, convertedColor);
        }
        else
        {
            memcpy(convertedColor, color.u32Color, sizeof(convertedColor));
        }

        // At this point, "convertedColor" will contain the per-channel color data in its raw format. Compute clears
        // will prefer this in order to do a raw bit copy, but the RB requires shader outputs to be in their native
        // format for GFX draws. If the caller specifies the raw format for this function, then we need to convert the
        // color back to its native format.
        if (pRawFormat != nullptr)
        {
            RpmUtil::ConvertClearColorToNativeFormat(clearFormat, *pRawFormat, convertedColor);
        }

        // If we can clear with raw format replacement which is more efficient, swizzle it into the order
        // required and then pack it. As per the above comment, this should always be true for the CS case.
        if (clearWithRawFmt)
        {
            uint32 swizzledColor[4] = {};
            Formats::SwizzleColor(clearFormat, convertedColor, swizzledColor);
            Formats::PackRawClearColor(clearFormat, swizzledColor, pPackedColor);
        }
        else
        {
            memcpy(pPackedColor, convertedColor, sizeof(convertedColor));
        }
    }
}

// =====================================================================================================================
// Calculates the normalized form of the unsigned input data. Returns the input data as a uint32 which stores the IEEE
// bit format representation of the normalized form of the input data.
uint32 GetNormalizedData(
    uint32 inputData,         // input data
    uint32 maxComponentValue) // max possible value of "inputData"
{
    float normalized = static_cast<float>(inputData) / static_cast<float>(maxComponentValue);

    return FloatToBits(normalized);
}

// =====================================================================================================================
// Writes the user data register required to allow the RPM VS to export the supplied depth value.
void WriteVsZOut(
    GfxCmdBuffer* pCmdBuffer,
    float         depthValue)
{
    pCmdBuffer->CmdSetUserData(PipelineBindPoint::Graphics, RpmVsDepthOut, 1, reinterpret_cast<uint32*>(&depthValue));
}

// =====================================================================================================================
// Writes the user data register required to allow the RPM multi-layer VS to identify the first slice to render to
void WriteVsFirstSliceOffset(
    GfxCmdBuffer*   pCmdBuffer,
    uint32          firstSliceIndex)
{
    pCmdBuffer->CmdSetUserData(PipelineBindPoint::Graphics,
                               RpmVsSliceOffset,
                               1,
                               &firstSliceIndex);
}

// =====================================================================================================================
// Writes a simple, typical raster state for all RPM draws.
void BindBltRasterState(
    GfxCmdBuffer* pCmdBuffer)
{
    constexpr DepthBiasParams            DepthBias            = { 0.0f, 0.0f, 0.0f };
    constexpr PointLineRasterStateParams PointLineRasterState = { 1.0f, 1.0f };
    constexpr TriangleRasterStateParams  TriangleRasterState =
    {
        FillMode::Solid,        // frontface fillMode
        FillMode::Solid,        // backface fillMode
        CullMode::_None,        // cullMode
        FaceOrientation::Cw,    // frontFace
        ProvokingVertex::First  // provokingVertex
    };

    pCmdBuffer->CmdSetDepthBiasState(DepthBias);
    pCmdBuffer->CmdSetPointLineRasterState(PointLineRasterState);
    pCmdBuffer->CmdSetTriangleRasterState(TriangleRasterState);
}

// =====================================================================================================================
// Usually number of fmaskBits needed for a given fragmentCount = log2(fragmentCount)
// But the hardware can't work with 3 bits (for 8xMSAA) so it is padded to 4 bits
// For EQAA an extra bit is required to point to the colors for the extra samples.
// Chart below shows the number of bits required for each EQAA configuration
//      Frag Count | Sample Count | Fmask bits
//          2      |      2       |      1
//          2      |      4       |      2
//          2      |      8       |      2
//          2      |     16       |      2
//-----------------------------------------------
//          4      |      4       |      4
//          4      |      8       |      4
//          4      |     16       |      4
//-----------------------------------------------
//          8      |      8       |      4
uint32 CalculatNumFmaskBits(
    uint32 fragmentCount,
    uint32 sampleCount)
{
    uint32 fmaskBits = 4;
    if ((fragmentCount == 1) || (sampleCount == 2))
    {
        fmaskBits = 1;
    }
    else if ((sampleCount == 4) || (fragmentCount == 2))
    {
        fmaskBits = 2;
    }
    return fmaskBits;
}

// =====================================================================================================================
// Some RPM shaders work with lots of small constants (e.g., numSamples, numFragments) which we can bit-pack into
// individual bytes to save fast user-data space. The AMDIL unpack4u8/unpack4i8 instruction converts them back.
uint32 PackFourBytes(
    uint32 x,
    uint32 y,
    uint32 z,
    uint32 w)
{
    // This function only works when these values are all small enough to fit in a byte!
    PAL_ASSERT((x <= UINT8_MAX) && (y <= UINT8_MAX) && (z <= UINT8_MAX) && (w <= UINT8_MAX));

    return x | (y << 8) | (z << 16) | (w << 24);
}

} // RpmUtil
} // Pal
