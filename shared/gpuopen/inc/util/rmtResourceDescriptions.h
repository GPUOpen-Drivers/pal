/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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
/**
***********************************************************************************************************************
* @file  rmtResourceDescDefs.h
* @brief RMT resource description structure definitions
***********************************************************************************************************************
*/

#pragma once

#include <util/rmtCommon.h>

namespace DevDriver
{

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// RMT Types and Helper Functions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

inline uint64_t Log2(uint64_t x)
{
    DD_ASSERT(x > 0);

    uint64_t log2Val = 63;
    while (x != 0)
    {
        if (x & 0x8000000000000000LL)
        {
            break;
        }
        x = x << 1;
        log2Val--;
    }
    return log2Val;
}

// Bitfield defining the Image resource usage flags.
struct RMT_IMAGE_USAGE_FLAGS
{
    union {
        struct {
            uint16 SHADER_READ               : 1;
            uint16 SHADER_WRITE              : 1;
            uint16 RESOLVE_SOURCE            : 1;
            uint16 RESOLVE_DESTINATION       : 1;
            uint16 COLOR_TARGET              : 1;
            uint16 DEPTH_STENCIL             : 1;
            uint16 NO_STENCIL_SHADER_READ    : 1;
            uint16 HI_Z_NEVER_INVALID        : 1;
            uint16 DEPTH_AS_Z24              : 1;
            uint16 FIRST_SHADER_WRITABLE_MIP : 3;
            uint16 CORNER_SAMPLING           : 1;
            uint16 RESERVED                  : 2;
        };

       uint16 u16Val;
    };
};

// Bitfield defining the Image resource create flags.
struct RMT_IMAGE_CREATE_FLAGS
{
    union {
        struct {
            uint32 INVARIANT                       : 1;
            uint32 CLONEABLE                       : 1;
            uint32 SHAREABLE                       : 1;
            uint32 FLIPPABLE                       : 1;
            uint32 STEREO                          : 1;
            uint32 CUBEMAP                         : 1;
            uint32 PRT                             : 1;
            uint32 RESERVED_0                      : 1;
            uint32 READ_SWIZZLE_EQUATIONS          : 1;
            uint32 PER_SUBRESOURCE_INIT            : 1;
            uint32 SEPARATE_DEPTH_ASPECT_RATIO     : 1;
            uint32 COPY_FORMATS_MATCH              : 1;
            uint32 REPETITIVE_RESOLVE              : 1;
            uint32 PREFR_SWIZZLE_EQUATIONS         : 1;
            uint32 FIXED_TILE_SWIZZLE              : 1;
            uint32 VIDEO_REFERENCE_ONLY            : 1;
            uint32 OPTIMAL_SHAREABLE               : 1;
            uint32 SAMPLE_LOCATIONS_ALWAYS_KNOWN   : 1;
            uint32 FULL_RESOLVE_DESTINATION_ONLY   : 1;
            uint32 EXTERNAL_SHARED                 : 1;
            uint32 RESERVED                        : 12;
        };

        uint32 u32Val;
    };

};

// Enumeration of image types
enum RMT_IMAGE_TYPE
{
    RMT_IMAGE_TYPE_1D = 0,
    RMT_IMAGE_TYPE_2D = 1,
    RMT_IMAGE_TYPE_3D = 2,
};

// Image dimensions helper struct
struct RMT_IMAGE_DIMENSIONS
{
    uint16 dimension_X;
    uint16 dimension_Y;
    uint16 dimension_Z;
};

// Enumeration of image tiling types
enum RMT_IMAGE_TILING_TYPE
{
    RMT_IMAGE_TILING_TYPE_LINEAR           = 0,
    RMT_IMAGE_TILING_TYPE_OPTIMAL          = 1,
    RMT_IMAGE_TILING_TYPE_STANDARD_SWIZZLE = 2,
};

// Enumeration of image tiling opt modes
enum RMT_IMAGE_TILING_OPT_MODE
{
    RMT_IMAGE_TILING_OPT_MODE_BALANCED      = 0,
    RMT_IMAGE_TILING_OPT_MODE_OPT_FOR_SPACE = 1,
    RMT_IMAGE_TILING_OPT_MODE_OPT_FOR_SPEED = 2,
};

// Enumeration of image metadata modes
enum RMT_IMAGE_METADATA_MODE
{
    RMT_IMAGE_METADATA_MODE_DEFAULT              = 0,
    RMT_IMAGE_METADATA_MODE_OPT_FOR_TEX_PREFETCH = 1,
    RMT_IMAGE_METADATA_MODE_DISABLED             = 2,
};

// Bitfield defining the Buffer resource usage flags.
struct RMT_BUFFER_USAGE_FLAGS
{
    uint32 TRANSFER_SOURCE                   : 1;
    uint32 TRANSFER_DESTINATION              : 1;
    uint32 UNIFORM_TEXEL_BUFFER              : 1;
    uint32 STORAGE_TEXEL_BUFFER              : 1;
    uint32 UNIFORM_BUFFER                    : 1;
    uint32 STORAGE_BUFFER                    : 1;
    uint32 INDEX_BUFFER                      : 1;
    uint32 VERTEX_BUFFER                     : 1;
    uint32 INDIRECT_BUFFER                   : 1;
    uint32 TRANSFORM_FEEDBACK_BUFFER         : 1;
    uint32 TRANSFORM_FEEDBACK_COUNTER_BUFFER : 1;
    uint32 CONDITIONAL_RENDERING             : 1;
    uint32 RAY_TRACING                       : 1;
    uint32 SHADER_DEVICE_ADDRESS             : 1;
};

// Bitfield defining the Buffer resource create flags.
struct RMT_BUFFER_CREATE_FLAGS
{
    uint32 SPARSE_BINDING                 : 1;
    uint32 SPARSE_RESIDENCY               : 1;
    uint32 SPARSE_ALIASING                : 1;
    uint32 PROTECTED                      : 1;
    uint32 DEVICE_ADDRESS_CAPTURE_REPLAY  : 1;
};

// Enumeration of swizzle types
enum RMT_SWIZZLE
{
    RMT_SWIZZLE_0 = 0,
    RMT_SWIZZLE_1 = 1,
    RMT_SWIZZLE_X = 2,
    RMT_SWIZZLE_Y = 3,
    RMT_SWIZZLE_Z = 4,
    RMT_SWIZZLE_W = 5,
};

// Enumeration of image number formats
enum RMT_NUM_FORMAT
{
    RMT_FORMAT_UNDEFINED                               = 0,
    RMT_FORMAT_X1_UNORM                                = 1,
    RMT_FORMAT_X1_USCALED                              = 2,
    RMT_FORMAT_X4Y4_UNORM                              = 3,
    RMT_FORMAT_X4Y4_USCALED                            = 4,
    RMT_FORMAT_L4A4_UNORM                              = 5,
    RMT_FORMAT_X4Y4Z4W4_UNORM                          = 6,
    RMT_FORMAT_X4Y4Z4W4_USCALED                        = 7,
    RMT_FORMAT_X5Y6Z5_UNORM                            = 8,
    RMT_FORMAT_X5Y6Z5_USCALED                          = 9,
    RMT_FORMAT_X5Y5Z5W1_UNORM                          = 10,
    RMT_FORMAT_X5Y5Z5W1_USCALED                        = 11,
    RMT_FORMAT_X1Y5Z5W5_UNORM                          = 12,
    RMT_FORMAT_X1Y5Z5W5_USCALED                        = 13,
    RMT_FORMAT_X8_XNORM                                = 14,
    RMT_FORMAT_X8_SNORM                                = 15,
    RMT_FORMAT_X8_USCALED                              = 16,
    RMT_FORMAT_X8_SSCALED                              = 17,
    RMT_FORMAT_X8_UINT                                 = 18,
    RMT_FORMAT_X8_SINT                                 = 19,
    RMT_FORMAT_X8_SRGB                                 = 20,
    RMT_FORMAT_A8_UNORM                                = 21,
    RMT_FORMAT_L8_UNORM                                = 22,
    RMT_FORMAT_P8_UINT                                 = 23,
    RMT_FORMAT_X8Y8_UNORM                              = 24,
    RMT_FORMAT_X8Y8_SNORM                              = 25,
    RMT_FORMAT_X8Y8_USCALED                            = 26,
    RMT_FORMAT_X8Y8_SSCALED                            = 27,
    RMT_FORMAT_X8Y8_UINT                               = 28,
    RMT_FORMAT_X8Y8_SINT                               = 29,
    RMT_FORMAT_X8Y8_SRGB                               = 30,
    RMT_FORMAT_L8A8_UNORM                              = 31,
    RMT_FORMAT_X8Y8Z8W8_UNORM                          = 32,
    RMT_FORMAT_X8Y8Z8W8_SNORM                          = 33,
    RMT_FORMAT_X8Y8Z8W8_USCALED                        = 34,
    RMT_FORMAT_X8Y8Z8W8_SSCALED                        = 35,
    RMT_FORMAT_X8Y8Z8W8_UINT                           = 36,
    RMT_FORMAT_X8Y8Z8W8_SINT                           = 37,
    RMT_FORMAT_X8Y8Z8W8_SRGB                           = 38,
    RMT_FORMAT_U8V8_SNORM_L8W8_UNORM                   = 39,
    RMT_FORMAT_X10Y11Z11_FLOAT                         = 40,
    RMT_FORMAT_X11Y11Z10_FLOAT                         = 41,
    RMT_FORMAT_X10Y10Z10W2_UNORM                       = 42,
    RMT_FORMAT_X10Y10Z10W2_SNORM                       = 43,
    RMT_FORMAT_X10Y10Z10W2_USCALED                     = 44,
    RMT_FORMAT_X10Y10Z10W2_SSCALED                     = 45,
    RMT_FORMAT_X10Y10Z10W2_UINT                        = 46,
    RMT_FORMAT_X10Y10Z10W2_SINT                        = 47,
    RMT_FORMAT_X10Y10Z10W2BIAS_UNORM                   = 48,
    RMT_FORMAT_U10V10W10_SNORM_A2_UNORM                = 49,
    RMT_FORMAT_X16_UNORM                               = 50,
    RMT_FORMAT_X16_SNORM                               = 51,
    RMT_FORMAT_X16_USCALED                             = 52,
    RMT_FORMAT_X16_SSCALED                             = 53,
    RMT_FORMAT_X16_UINT                                = 54,
    RMT_FORMAT_X16_SINT                                = 55,
    RMT_FORMAT_X16_FLOAT                               = 56,
    RMT_FORMAT_L16_UNORM                               = 57,
    RMT_FORMAT_X16Y16_UNORM                            = 58,
    RMT_FORMAT_X16Y16_SNORM                            = 59,
    RMT_FORMAT_X16Y16_USCALED                          = 60,
    RMT_FORMAT_X16Y16_SSCALED                          = 61,
    RMT_FORMAT_X16Y16_UINT                             = 62,
    RMT_FORMAT_X16Y16_SINT                             = 63,
    RMT_FORMAT_X16Y16_FLOAT                            = 64,
    RMT_FORMAT_X16Y16Z16W16_UNORM                      = 65,
    RMT_FORMAT_X16Y16Z16W16_SNORM                      = 66,
    RMT_FORMAT_X16Y16Z16W16_USCALED                    = 67,
    RMT_FORMAT_X16Y16Z16W16_SSCALED                    = 68,
    RMT_FORMAT_X16Y16Z16W16_UINT                       = 69,
    RMT_FORMAT_X16Y16Z16W16_SINT                       = 70,
    RMT_FORMAT_X16Y16Z16W16_FLOAT                      = 71,
    RMT_FORMAT_X32_UINT                                = 72,
    RMT_FORMAT_X32_SINT                                = 73,
    RMT_FORMAT_X32_FLOAT                               = 74,
    RMT_FORMAT_X32Y32_UINT                             = 75,
    RMT_FORMAT_X32Y32_SINT                             = 76,
    RMT_FORMAT_X32Y32_FLOAT                            = 77,
    RMT_FORMAT_X32Y32Z32_UINT                          = 78,
    RMT_FORMAT_X32Y32Z32_SINT                          = 79,
    RMT_FORMAT_X32Y32Z32_FLOAT                         = 80,
    RMT_FORMAT_X32Y32Z32W32_UINT                       = 81,
    RMT_FORMAT_X32Y32Z32W32_SINT                       = 82,
    RMT_FORMAT_X32Y32Z32W32_FLOAT                      = 83,
    RMT_FORMAT_D16_UNORM_S8_UINT                       = 84,
    RMT_FORMAT_D32_UNORM_S8_UINT                       = 85,
    RMT_FORMAT_X9Y9Z9E5_FLOAT                          = 86,
    RMT_FORMAT_BC1_UNORM                               = 87,
    RMT_FORMAT_BC1_SRGB                                = 88,
    RMT_FORMAT_BC2_UNORM                               = 89,
    RMT_FORMAT_BC2_SRGB                                = 90,
    RMT_FORMAT_BC3_UNORM                               = 91,
    RMT_FORMAT_BC3_SRGB                                = 92,
    RMT_FORMAT_BC4_UNORM                               = 93,
    RMT_FORMAT_BC4_SRGB                                = 94,
    RMT_FORMAT_BC5_UNORM                               = 95,
    RMT_FORMAT_BC5_SRGB                                = 96,
    RMT_FORMAT_BC6_UNORM                               = 97,
    RMT_FORMAT_BC6_SRGB                                = 98,
    RMT_FORMAT_BC7_UNORM                               = 99,
    RMT_FORMAT_BC7_SRGB                                = 100,
    RMT_FORMAT_ETC2X8Y8Z8_UNORM                        = 101,
    RMT_FORMAT_ETC2X8Y8Z8_SRGB                         = 102,
    RMT_FORMAT_ETC2X8Y8Z8W1_UNORM                      = 103,
    RMT_FORMAT_ETC2X8Y8Z8W1_SRGB                       = 104,
    RMT_FORMAT_ETC2X8Y8Z8W8_UNORM                      = 105,
    RMT_FORMAT_ETC2X8Y8Z8W8_SRGB                       = 106,
    RMT_FORMAT_ETC2X11_UNORM                           = 107,
    RMT_FORMAT_ETC2X11_SNORM                           = 108,
    RMT_FORMAT_ETC2X11Y11_UNORM                        = 109,
    RMT_FORMAT_ETC2X11Y11_SNORM                        = 110,
    RMT_FORMAT_ASTCLDR4X4_UNORM                        = 111,
    RMT_FORMAT_ASTCLDR4X4_SRGB                         = 112,
    RMT_FORMAT_ASTCLDR5X4_UNORM                        = 113,
    RMT_FORMAT_ASTCLDR5X4_SRGB                         = 114,
    RMT_FORMAT_ASTCLDR5X5_UNORM                        = 115,
    RMT_FORMAT_ASTCLDR5X5_SRGB                         = 116,
    RMT_FORMAT_ASTCLDR6X5_UNORM                        = 117,
    RMT_FORMAT_ASTCLDR6X5_SRGB                         = 118,
    RMT_FORMAT_ASTCLDR6X6_UNORM                        = 119,
    RMT_FORMAT_ASTCLDR6X6_SRGB                         = 120,
    RMT_FORMAT_ASTCLDR8X5_UNORM                        = 121,
    RMT_FORMAT_ASTCLDR8X5_SRGB                         = 122,
    RMT_FORMAT_ASTCLDR8X6_UNORM                        = 123,
    RMT_FORMAT_ASTCLDR8X6_SRGB                         = 124,
    RMT_FORMAT_ASTCLDR8X8_UNORM                        = 125,
    RMT_FORMAT_ASTCLDR8X8_SRGB                         = 126,
    RMT_FORMAT_ASTCLDR10X5_UNORM                       = 127,
    RMT_FORMAT_ASTCLDR10X5_SRGB                        = 128,
    RMT_FORMAT_ASTCLDR10X6_UNORM                       = 129,
    RMT_FORMAT_ASTCLDR10X6_SRGB                        = 130,
    RMT_FORMAT_ASTCLDR10X8_UNORM                       = 131,
    RMT_FORMAT_ASTCLDR10X10_UNORM                      = 132,
    RMT_FORMAT_ASTCLDR12X10_UNORM                      = 133,
    RMT_FORMAT_ASTCLDR12X10_SRGB                       = 134,
    RMT_FORMAT_ASTCLDR12X12_UNORM                      = 135,
    RMT_FORMAT_ASTCLDR12X12_SRGB                       = 136,
    RMT_FORMAT_ASTCHDR4x4_FLOAT                        = 137,
    RMT_FORMAT_ASTCHDR5x4_FLOAT                        = 138,
    RMT_FORMAT_ASTCHDR5x5_FLOAT                        = 139,
    RMT_FORMAT_ASTCHDR6x5_FLOAT                        = 140,
    RMT_FORMAT_ASTCHDR6x6_FLOAT                        = 141,
    RMT_FORMAT_ASTCHDR8x5_FLOAT                        = 142,
    RMT_FORMAT_ASTCHDR8x6_FLOAT                        = 143,
    RMT_FORMAT_ASTCHDR8x8_FLOAT                        = 144,
    RMT_FORMAT_ASTCHDR10x5_FLOAT                       = 145,
    RMT_FORMAT_ASTCHDR10x6_FLOAT                       = 146,
    RMT_FORMAT_ASTCHDR10x8_FLOAT                       = 147,
    RMT_FORMAT_ASTCHDR10x10_FLOAT                      = 148,
    RMT_FORMAT_ASTCHDR12x10_FLOAT                      = 149,
    RMT_FORMAT_ASTCHDR12x12_FLOAT                      = 150,
    RMT_FORMAT_X8Y8_Z8Y8_UNORM                         = 151,
    RMT_FORMAT_X8Y8_Z8Y8_USCALED                       = 152,
    RMT_FORMAT_Y8X8_Y8Z8_UNORM                         = 153,
    RMT_FORMAT_Y8X8_Y8Z8_USCALED                       = 154,
    RMT_FORMAT_AYUV                                    = 155,
    RMT_FORMAT_UYVY                                    = 156,
    RMT_FORMAT_VYUY                                    = 157,
    RMT_FORMAT_YUY2                                    = 158,
    RMT_FORMAT_YVY2                                    = 159,
    RMT_FORMAT_YV12                                    = 160,
    RMT_FORMAT_NV11                                    = 161,
    RMT_FORMAT_NV12                                    = 162,
    RMT_FORMAT_NV21                                    = 163,
    RMT_FORMAT_P016                                    = 164,
    RMT_FORMAT_P010                                    = 165,
};

// Structure defining an image format
struct RMT_IMAGE_FORMAT
{
    union
    {
        struct
        {
            uint32    SWIZZLE_X  : 3;  // Encoded using RMT_SWIZZLE values
            uint32    SWIZZLE_Y  : 3;  // Encoded using RMT_SWIZZLE values
            uint32    SWIZZLE_Z  : 3;  // Encoded using RMT_SWIZZLE values
            uint32    SWIZZLE_W  : 3;  // Encoded using RMT_SWIZZLE values
            uint32    NUM_FORMAT : 8;  // Encoded using RMT_NUM_FORMAT values
            uint32    reserved   : 12;
        };

        uint32 dwordVal;
    };
};

// Enumeration of descriptor types
enum RMT_DESCRIPTOR_TYPE
{
    RMT_DESCRIPTOR_TYPE_CSV_SRV_UAV            = 0,
    RMT_DESCRIPTOR_TYPE_SAMPLER                = 1,
    RMT_DESCRIPTOR_TYPE_RTV                    = 2,
    RMT_DESCRIPTOR_TYPE_DSV                    = 3,
    RMT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER = 4,
    RMT_DESCRIPTOR_TYPE_SAMPLED_IMAGE          = 5,
    RMT_DESCRIPTOR_TYPE_STORAGE_IMAGE          = 6,
    RMT_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER   = 7,
    RMT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER   = 8,
    RMT_DESCRIPTOR_TYPE_UNIFORM_BUFFER         = 9,
    RMT_DESCRIPTOR_TYPE_STORAGE_BUFFER         = 10,
    RMT_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC = 11,
    RMT_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC = 12,
    RMT_DESCRIPTOR_TYPE_INPUT_ATTACHMENT       = 13,
    RMT_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK   = 14,
    RMT_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE = 15,
};

// Enumeration of Query Heap types
enum RMT_QUERY_HEAP_TYPE
{
    RMT_QUERY_HEAP_TYPE_OCCLUSION          = 0,
    RMT_QUERY_HEAP_TYPE_PIPELINE_STATS     = 1,
    RMT_QUERY_HEAP_TYPE_STREAMOUT_STATS    = 2,
    RMT_QUERY_HEAP_TYPE_VIDEO_DECODE_STATS = 3,
};

// Enumeration of engine types
enum RMT_ENGINE_TYPE
{
    RMT_ENGINE_TYPE_UNIVERSAL         = 0,
    RMT_ENGINE_TYPE_COMPUTE           = 1,
    RMT_ENGINE_TYPE_EXCLUSIVE_COMPUTE = 2,
    RMT_ENGINE_TYPE_DMA               = 3,
    RMT_ENGINE_TYPE_TIMER             = 4,
    RMT_ENGINE_TYPE_VCE_ENCODE        = 5,
    RMT_ENGINE_TYPE_UVD_DECODE        = 6,
    RMT_ENGINE_TYPE_UVD_ENCODE        = 7,
    RMT_ENGINE_TYPE_VCN_DECODE        = 8,
    RMT_ENGINE_TYPE_VCN_ENCODE        = 9,
    RMT_ENGINE_TYPE_HP3D              = 10,
};

// Enumeration of Video Decoder types
enum RMT_VIDEO_DECODER_TYPE
{
    RMT_VIDEO_DECODER_TYPE_H264      = 0,
    RMT_VIDEO_DECODER_TYPE_VC1       = 1,
    RMT_VIDEO_DECODER_TYPE_MPEG2IDCT = 2,
    RMT_VIDEO_DECODER_TYPE_MPEG2VLD  = 3,
    RMT_VIDEO_DECODER_TYPE_MPEG4     = 4,
    RMT_VIDEO_DECODER_TYPE_WMV9      = 5,
    RMT_VIDEO_DECODER_TYPE_MJPEG     = 6,
    RMT_VIDEO_DECODER_TYPE_HVEC      = 7,
    RMT_VIDEO_DECODER_TYPE_VP9       = 8,
    RMT_VIDEO_DECODER_TYPE_HEVC10BIT = 9,
    RMT_VIDEO_DECODER_TYPE_VP910BIT  = 10,
    RMT_VIDEO_DECODER_TYPE_AV1       = 11,
    RMT_VIDEO_DECODER_TYPE_AV112BIT  = 12
};

// Enumeration of Video Encoder types
enum RMT_VIDEO_ENCODER_TYPE
{
    RMT_VIDEO_ENCODER_TYPE_H264 = 0,
    RMT_VIDEO_ENCODER_TYPE_H265 = 1,
};

// Bitfield defining the values for Heap description flags
struct RMT_HEAP_FLAGS
{
    union
    {
        struct
        {
            uint8 NON_RT_DS_TEXTURES   : 1;
            uint8 BUFFERS              : 1;
            uint8 COHERENT_SYSTEM_WIDE : 1;
            uint8 PRIMARY              : 1;
            uint8 RT_DS_TEXTURES       : 1;
            uint8 DENY_L0_PROMOTION    : 1;
            uint8 reserved             : 3;
        };

        uint8 byteVal;
    };
};

// Bitfield defining the values for Pipeline creation flags
struct RMT_PIPELINE_CREATE_FLAGS
{
    union
    {
        struct
        {
            uint8 CLIENT_INTERNAL   : 1;
            uint8 OVERRIDE_GPU_HEAP : 1;
            uint8 reserved          : 6;
        };

        uint8 byteVal;
    };
};

// Structure defining a 128-bit pipeline hash
struct RMT_PIPELINE_HASH
{
    uint64 hashUpper;
    uint64 hashLower;
};

// Bitfield used to mark active pipeline shader stages
struct RMT_PIPELINE_STAGES
{
    uint8 PS_STAGE : 1;
    uint8 HS_STAGE : 1;
    uint8 DS_STAGE : 1;
    uint8 VS_STAGE : 1;
    uint8 GS_STAGE : 1;
    uint8 CS_STAGE : 1;
    uint8 TS_STAGE : 1;
    uint8 MS_STAGE : 1;
};

// Bitfield defining CmdAllocator create flags
struct RMT_CMD_ALLOCATOR_CREATE_FLAGS
{
    union
    {
        struct
        {
            uint8 AUTO_MEMORY_REUSE           : 1;
            uint8 DISABLE_BUSY_CHUNK_TRACKING : 1;
            uint8 THREAD_SAFE                 : 1;
            uint8 reserved                    : 5;
        };

        uint8 byteVal;
    };
};

// Enumeration of Misc internal resource types
enum RMT_MISC_INTERNAL_TYPE
{
    RMT_MISC_INTERNAL_TYPE_OCCLUSION_QUERY_RESET_DATA  = 0,
    RMT_MISC_INTERNAL_TYPE_CPDMA_PATCH                 = 1,
    RMT_MISC_INTERNAL_TYPE_OCCLUSION_QUERY_RESULT_PAIR = 2,
    RMT_MISC_INTERNAL_TYPE_SHADER_MEMORY               = 3,
    RMT_MISC_INTERNAL_TYPE_SHADER_RING                 = 4,
    RMT_MISC_INTERNAL_TYPE_SRD_TABLE                   = 5,
    RMT_MISC_INTERNAL_TYPE_DEBUG_STALL_MEMORY          = 6,
    RMT_MISC_INTERNAL_TYPE_FRAME_COUNT_MEMORY          = 7,
    RMT_MISC_INTERNAL_TYPE_PIPELINE_PERF_DATA          = 8,
    RMT_MISC_INTERNAL_TYPE_PAGE_FAULT_SRD              = 9,
    RMT_MISC_INTERNAL_TYPE_DUMMY_CHUNK                 = 10,
    RMT_MISC_INTERNAL_TYPE_DELAG_DEVICE                = 11,
    RMT_MISC_INTERNAL_TYPE_TILE_GRID_MEMORY            = 12,
    RMT_MISC_INTERNAL_TYPE_FMASK_MEMORY                = 13,
    RMT_MISC_INTERNAL_TYPE_VIDEO_DECODER_HEAP          = 14,
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Resource Description Structures
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// RMT_RESOURCE_TYPE_IMAGE
struct RMT_IMAGE_DESC_CREATE_INFO
{
    uint32                    createFlags;  // Encoded using RMT_IMAGE_CREATE_FLAGS bit values
    uint16                    usageFlags;   // Encoded using RMT_IMAGE_USAGE_FLAGS bit values
    RMT_IMAGE_TYPE            imageType;
    RMT_IMAGE_DIMENSIONS      dimensions;
    RMT_IMAGE_FORMAT          format;
    uint8                     mips;
    uint16                    slices;
    uint8                     samples;
    uint8                     fragments;
    RMT_IMAGE_TILING_TYPE     tilingType;
    RMT_IMAGE_TILING_OPT_MODE tilingOptMode;
    RMT_IMAGE_METADATA_MODE   metadataMode;
    uint64                    maxBaseAlignment;
    bool                      isPresentable;
    uint32                    imageSize;
    uint32                    metadataOffset;
    uint32                    metadataSize;
    uint32                    metadataHeaderOffset;
    uint32                    metadataHeaderSize;
    uint64                    imageAlignment;
    uint64                    metadataAlignment;
    uint64                    metadataHeaderAlignment;
    bool                      isFullscreen;
};

static const size_t RMT_IMAGE_BYTES_SIZE = 304 / 8; // 304-bits
struct RMT_RESOURCE_TYPE_IMAGE_TOKEN : RMT_TOKEN_DATA
{
    uint8 bytes[RMT_IMAGE_BYTES_SIZE];

    // Initializes the token fields
    RMT_RESOURCE_TYPE_IMAGE_TOKEN(RMT_IMAGE_DESC_CREATE_INFO createInfo)
    {
        sizeInBytes = sizeof(bytes);
        pByteData = &bytes[0];

        // FLAGS [19:0] Creation flags describing how the image was created.
        //                See RMT_IMAGE_CREATE_FLAGS.
        SetBits(createInfo.createFlags, 19, 0);

        // USAGE_FLAGS [34:20] Usage flags describing how the image is used by the application.
        //                       See RMT_IMAGE_USAGE_FLAGS.
        SetBits(createInfo.usageFlags, 34, 20);

        // TYPE [36:35] The type of the image encoded RMT_IMAGE_TYPE.
        SetBits(createInfo.imageType, 36, 35);

        // DIMENSION_X [50:37] The dimension of the image in the X dimension, minus 1.
        SetBits((createInfo.dimensions.dimension_X - 1), 50, 37);

        // DIMENSION_Y [64:51] The dimension of the image in the Y dimension, minus 1.
        SetBits((createInfo.dimensions.dimension_Y - 1), 64, 51);

        // DIMENSION_Z [78:65] The dimension of the image in the Z dimension, minus 1.
        SetBits((createInfo.dimensions.dimension_Z - 1), 78, 65);

        // FORMAT [98:79] The format of the image. Encoded as RMT_IMAGE_FORMAT
        SetBits(createInfo.format.dwordVal, 98, 79);

        // MIPS [102:99] The number of mip-map levels in the image.
        SetBits(createInfo.mips, 102, 99);

        // SLICES [113:103] The number of slices in the image minus one. The maximum this can be in the range [1..2048].
        SetBits((createInfo.slices - 1), 113, 103);

        // SAMPLES [116:114] The Log2(n) of the sample count for the image.
        SetBits(Log2(createInfo.samples), 116, 114);

        // FRAGMENTS [118:117] The Log2(n) of the fragment count for the image.
        SetBits(Log2(createInfo.fragments), 118, 117);

        // TILING_TYPE [120:119] The tiling type used for the image, encoded as RMT_IMAGE_TILING_TYPE.
        SetBits(createInfo.tilingType, 120, 119);

        // TILING_OPT_MODE [122:121] The tiling optimisation mode for the image, encoded as RMT_IMAGE_TILING_OPT_MODE.
        SetBits(createInfo.tilingOptMode, 122, 121);

        // METADATA_MODE [124:123] The metadata mode for the image, encoded as RMT_IMAGE_METADATA_MODE.
        SetBits(createInfo.metadataMode, 124, 123);

        // MAX_BASE_ALIGNMENT [129:125] The alignment of the image resource. This is stored as the Log2(n) of the
        //                                alignment, it is therefore possible to encode alignments from [1Byte..2MiB].
        SetBits(((createInfo.maxBaseAlignment == 0) ? 0: Log2(createInfo.maxBaseAlignment)), 129, 125);

        // PRESENTABLE [130] This bit is set to 1 if the image is presentable.
        SetBits((createInfo.isPresentable ? 1 : 0), 130, 130);

        // IMAGE_SIZE [162:131] The size of the core image data inside the resource.
        SetBits(createInfo.imageSize, 162, 131);

        // METADATA_OFFSET [194:163] The offset from the base virtual address of the resource to the metadata of
        //                             the image.
        SetBits(createInfo.metadataOffset, 194, 163);

        // METADATA_SIZE [226:195] The size of the metadata inside the resource.
        SetBits(createInfo.metadataSize, 226, 195);

        // METADATA_HEADER_OFFSET [258:227] The offset from the base virtual address of the resource to the
        //                                    metadata header.
        SetBits(createInfo.metadataHeaderOffset, 258, 227);

        // METADATA_HEADER_SIZE [290:259] The size of the metadata header inside the resource.
        SetBits(createInfo.metadataHeaderSize, 290, 259);

        // IMAGE_ALIGN [295:291] The alignment of the core image data within the resource's virtual address allocation.
        //                         This is stored as the Log2(n) of the alignment.
        SetBits(Log2(createInfo.imageAlignment), 295, 291);

        // METADATA_ALIGN [300:296] The alignment of the metadata within the resource's virtual address allocation.
        //                            This is stored as the Log2(n) of the alignment.
        SetBits(Log2(createInfo.metadataAlignment), 300, 296);

        // METADATA_HEADER_ALIGN [305:301] The alignment of the metadata header within the resource's virtual address
        //                                   allocation. This is stored as the Log2(n) of the alignment.
        SetBits(Log2(createInfo.metadataHeaderAlignment), 305, 301);

        // FULLSCREEN [306] This bit is set to 1 if the image is fullscreen presentable.
        SetBits((createInfo.isFullscreen ? 1 : 0), 306, 306);

        // RESERVED [311:307] Reserved for future expansion. Should be set to 0.
        SetBits(0, 311, 307);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// RMT_RESOURCE_TYPE_BUFFER
static const size_t RMT_BUFFER_BYTES_SIZE = 88 / 8; // 88-bits
struct RMT_RESOURCE_TYPE_BUFFER_TOKEN : RMT_TOKEN_DATA
{
    uint8 bytes[RMT_BUFFER_BYTES_SIZE];

    // Initializes the token fields
    RMT_RESOURCE_TYPE_BUFFER_TOKEN(uint8 createFlags, uint16 usageFlags, uint64 size)
    {
        sizeInBytes = sizeof(bytes);
        pByteData = &bytes[0];

        // CREATE_FLAGS [7:0] The create flags for a buffer. See RMT_BUFFER_CREATE_FLAGS.
        SetBits(createFlags, 7, 0);

        // USAGE_FLAGS [23:8] The usage flags for a buffer. See RMT_BUFFER_USAGE_FLAGS.
        SetBits(usageFlags, 23, 8);

        // SIZE [87:24] The size in bytes of the buffer.
        SetBits(size, 87, 24);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// RMT_RESOURCE_TYPE_GPU_EVENT
static const size_t RMT_GPU_EVENT_BYTES_SIZE = 8 / 8; // 8-bits
struct RMT_RESOURCE_TYPE_GPU_EVENT_TOKEN : RMT_TOKEN_DATA
{
    uint8 bytes[RMT_GPU_EVENT_BYTES_SIZE];

    // Initializes the token fields
    RMT_RESOURCE_TYPE_GPU_EVENT_TOKEN(bool isGpuOnly)
    {
        sizeInBytes = sizeof(bytes);
        pByteData = &bytes[0];

        // FLAGS [7:0] The flags used to create the GPU event. 0 - GPU-only event.
        SetBits((isGpuOnly ? 1 : 0), 7, 0);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// RMT_RESOURCE_TYPE_BORDER_COLOR_PALETTE
static const size_t RMT_BORDER_COLOR_PALETTE_BYTES_SIZE = 8 / 8; // 8-bits
struct RMT_RESOURCE_TYPE_BORDER_COLOR_PALETTE_TOKEN : RMT_TOKEN_DATA
{
    uint8 bytes[RMT_BORDER_COLOR_PALETTE_BYTES_SIZE];

    // Initializes the token fields
    RMT_RESOURCE_TYPE_BORDER_COLOR_PALETTE_TOKEN(uint8 numEntries)
    {
        sizeInBytes = sizeof(bytes);
        pByteData = &bytes[0];

        // NUM_ENTRIES [7:0] The number of entries in the border color palette.
        SetBits(numEntries, 7, 0);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// RMT_RESOURCE_TYPE_PERF_EXPERIMENT
static const size_t RMT_PERF_EXPERIMENT_BYTES_SIZE = 96 / 8; // 96-bits
struct RMT_RESOURCE_TYPE_PERF_EXPERIMENT_TOKEN : RMT_TOKEN_DATA
{
    uint8 bytes[RMT_PERF_EXPERIMENT_BYTES_SIZE];

    // Initializes the token fields
    RMT_RESOURCE_TYPE_PERF_EXPERIMENT_TOKEN(uint32 spmSize, uint32 sqttSize, uint32 counterSize)
    {
        sizeInBytes = sizeof(bytes);
        pByteData = &bytes[0];

        // SPM_SIZE [31:0] The size in bytes for the amount of memory allocated for SPM counter streaming.
        SetBits(spmSize, 31, 0);

        // SQTT_SIZE [63:32] The size in bytes for the amount of memory allocated for SQTT data streaming.
        SetBits(sqttSize, 63, 32);

        // COUNTER_SIZE [95:64] The size in bytes for the amount of memory allocated for per-draw counter data.
        SetBits(counterSize, 95, 64);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// RMT_RESOURCE_TYPE_QUERY_HEAP
static const size_t RMT_QUERY_HEAP_BYTES_SIZE = 8 / 8; // 8-bits
struct RMT_RESOURCE_TYPE_QUERY_HEAP_TOKEN : RMT_TOKEN_DATA
{
    uint8 bytes[RMT_QUERY_HEAP_BYTES_SIZE];

    // Initializes the token fields
    RMT_RESOURCE_TYPE_QUERY_HEAP_TOKEN(RMT_QUERY_HEAP_TYPE type, bool enableCpuAccess)
    {
        sizeInBytes = sizeof(bytes);
        pByteData = &bytes[0];

        // TYPE [1:0] The type of the query heap. See RMT_QUERY_HEAP_TYPE.
        SetBits(type, 1, 0);

        // ENABLE_CPU_ACCESS [2] Set to 1 if CPU access is enabled.
        SetBits((enableCpuAccess ? 1 :0), 2, 2);

        // RESERVED [7:3] Reserved for future expansion. Should be set to 0.
        SetBits(0, 7, 3);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// RMT_RESOURCE_TYPE_VIDEO_DECODER
static const size_t RMT_VIDEO_DECODER_BYTES_SIZE = 32 / 8; // 32-bits
struct RMT_RESOURCE_TYPE_VIDEO_DECODER_TOKEN : RMT_TOKEN_DATA
{
    uint8 bytes[RMT_VIDEO_DECODER_BYTES_SIZE];

    // Initializes the token fields
    RMT_RESOURCE_TYPE_VIDEO_DECODER_TOKEN(
        RMT_ENGINE_TYPE        engineType,
        RMT_VIDEO_DECODER_TYPE decoderType,
        uint32                 width,
        uint32                 height)
    {
        sizeInBytes = sizeof(bytes);
        pByteData = &bytes[0];

        // ENGINE_TYPE [3:0] The type of engine that the video decoder will run on. See RMT_ENGINE_TYPE
        SetBits(engineType, 3, 0);

        // VIDEO_DECODER_TYPE [7:4] The type of decoder being run. See RMT_VIDEO_DECODER_TYPE.
        SetBits(decoderType, 7, 4);

        // WIDTH [19:8] The width of the video minus one.
        SetBits((width - 1), 19, 8);

        // HEIGHT [31:20] The height of the video minus one.
        SetBits((height - 1), 31, 20);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// RMT_RESOURCE_TYPE_VIDEO_ENCODER
static const size_t RMT_VIDEO_ENCODER_BYTES_SIZE = 48 / 8; // 48-bits
struct RMT_RESOURCE_TYPE_VIDEO_ENCODER_TOKEN : RMT_TOKEN_DATA
{
    uint8 bytes[RMT_VIDEO_ENCODER_BYTES_SIZE];

    // Initializes the token fields
    RMT_RESOURCE_TYPE_VIDEO_ENCODER_TOKEN(
        RMT_ENGINE_TYPE        engineType,
        RMT_VIDEO_ENCODER_TYPE encoderType,
        uint16                 width,
        uint16                 height,
        RMT_IMAGE_FORMAT       format)
    {
        sizeInBytes = sizeof(bytes);
        pByteData = &bytes[0];

        // ENGINE_TYPE [3:0] The type of engine that the video encoder will run on. See RMT_ENGINE_TYPE.
        SetBits(engineType, 3, 0);

        // VIDEO_ENCODER_TYPE [4] The type of encoder being run. See RMT_VIDEO_ENCODER_TYPE
        SetBits(encoderType, 4, 4);

        // WIDTH [16:5] The width of the video minus one.
        SetBits((width - 1), 16, 5);

        // HEIGHT [28:17] The height of the video minus one.
        SetBits((height - 1), 28, 17);

        // IMAGE_FORMAT [47:29] Image format, see RMT_IMAGE_FORMAT.
        SetBits(format.dwordVal, 47, 29);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// RMT_RESOURCE_TYPE_HEAP
static const size_t RMT_HEAP_BYTES_SIZE = 80 / 8; // 80-bits
struct RMT_RESOURCE_TYPE_HEAP_TOKEN : RMT_TOKEN_DATA
{
    uint8 bytes[RMT_HEAP_BYTES_SIZE];

    // Initializes the token fields
    RMT_RESOURCE_TYPE_HEAP_TOKEN(RMT_HEAP_FLAGS flags, uint64 size, uint64 alignment, uint8 segmentIndex)
    {
        sizeInBytes = sizeof(bytes);
        pByteData = &bytes[0];

        // FLAGS [4:0] The flags used to create the heap. See RMT_HEAP_FLAGS
        SetBits(flags.byteVal, 4, 0);

        // SIZE [68:5] The size of the heap in bytes.
        SetBits(size, 68, 5);

        // ALIGNMENT [73:69] The alignment of the heap. This is stored as the Log2(n) of the alignment.
        SetBits(Log2(alignment), 73, 69);

        // SEGMENT_INDEX [77:74] The segment index where the heap was requested to be created.
        SetBits(segmentIndex, 77, 74);

        // RESERVED [79:78] This is reserved for future expansion. Should be set to 0.
        SetBits(0, 79, 78);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// RMT_RESOURCE_TYPE_PIPELINE
static const size_t RMT_PIPELINE_BYTES_SIZE = 152 / 8; // 152-bits
struct RMT_RESOURCE_TYPE_PIPELINE_TOKEN : RMT_TOKEN_DATA
{
    uint8 bytes[RMT_PIPELINE_BYTES_SIZE];

    // Initializes the token fields
    RMT_RESOURCE_TYPE_PIPELINE_TOKEN(
        RMT_PIPELINE_CREATE_FLAGS flags,
        RMT_PIPELINE_HASH         hash,
        RMT_PIPELINE_STAGES       stages,
        bool                      isNgg)
    {
        sizeInBytes = sizeof(bytes);
        pByteData = &bytes[0];

        // CREATE_FLAGS [7:0] Describes the creation flags for the pipeline.
        SetBits(flags.byteVal, 7, 0);

        // PIPELINE_HASH [135:8] The 128bit pipeline hash of the code object.
        SetBits(hash.hashUpper, 71, 8);
        SetBits(hash.hashLower, 135, 72);

        // PS_STAGE [136] The bit is set to true if one of the shaders in the pipeline will execute on the PS
        //                  hardware stage.
        SetBits(stages.PS_STAGE, 136, 136);

        // HS_STAGE [137] The bit is set to true if one of the shaders in the pipeline will execute on the HS
        //                  hardware stage.
        SetBits(stages.HS_STAGE, 137, 137);

        // DS_STAGE [138] The bit is set to true if one of the shaders in the pipeline will execute on the DS
        //                  hardware stage.
        SetBits(stages.DS_STAGE, 138, 138);

        // VS_STAGE [139] The bit is set to true if one of the shaders in the pipeline will execute on the VS
        //                  hardware stage.
        SetBits(stages.VS_STAGE, 139, 139);

        // GS_STAGE [140] The bit is set to true if one of the shaders in the pipeline will execute on the GS
        //                  hardware stage.
        SetBits(stages.GS_STAGE, 140, 140);

        // CS_STAGE [141] The bit is set to true if one of the shaders in the pipeline will execute on the CS
        //                  hardware stage.
        SetBits(stages.CS_STAGE, 141, 141);

        // TS_STAGE [142] The bit is set to true if one of the shaders in the pipeline will execute on the
        //                  task shader hardware stage.
        SetBits(stages.TS_STAGE, 142, 142);

        // MS_STAGE[143] The bit is set to true if one of the shaders in the pipeline will execute on the
        //                 surface shader hardware stage.
        SetBits(stages.MS_STAGE, 143, 143);

        // IS_NGG [144] The bit is set to true if the pipeline was compiled in NGG mode.
        SetBits((isNgg ? 1 : 0), 144, 144);

        // RESERVED [151:145] Reserved for future expansion. Should be set to 0.
        SetBits(0, 151, 145);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// RMT_RESOURCE_TYPE_DESCRIPTOR_HEAP
static const size_t RMT_DESCRIPTOR_HEAP_BYTES_SIZE = 32 / 8; // 32-bits
struct RMT_RESOURCE_TYPE_DESCRIPTOR_HEAP_TOKEN : RMT_TOKEN_DATA
{
    uint8 bytes[RMT_DESCRIPTOR_HEAP_BYTES_SIZE];

    // Initializes the token fields
    RMT_RESOURCE_TYPE_DESCRIPTOR_HEAP_TOKEN(
        RMT_DESCRIPTOR_TYPE type,
        bool                isShaderVisible,
        uint8               gpuMask,
        uint16              numDescriptors)
    {
        sizeInBytes = sizeof(bytes);
        pByteData = &bytes[0];

        // TYPE [3:0] The type of descriptors in the heap. See RMT_DESCRIPTOR_TYPE
        SetBits(type, 3, 0);

        // SHADER_VISIBLE [4] Flag indicating whether the heap is shader-visible.
        SetBits((isShaderVisible ? 1 : 0), 4, 4);

        // GPU_MASK [12:5] For single adapter this is set to zero, for multiple adapter mode this is a bitmask to
        //                   identify which adapters the heap applies to.
        SetBits(gpuMask, 12, 5);

        // NUM_DESCRIPTORS [28:13] The number of descriptors in the heap.
        SetBits(numDescriptors, 28, 13);

        // RESERVED [31:29] This is reserved for future expansion. Should be set to 0.
        SetBits(0, 31, 29);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// RMT_RESOURCE_TYPE_POOL_SIZE
static const size_t RMT_POOL_SIZE_BYTES_SIZE = 24 / 8; // 16-bits
struct RMT_RESOURCE_TYPE_POOL_SIZE_TOKEN : RMT_TOKEN_DATA
{
    uint8 bytes[RMT_POOL_SIZE_BYTES_SIZE];

    // Initializes the token fields
    RMT_RESOURCE_TYPE_POOL_SIZE_TOKEN(uint16 maxSets, uint8 poolSizeCount)
    {
        sizeInBytes = sizeof(bytes);
        pByteData = &bytes[0];

        // MAX_SETS [15:0] Maximum number of descriptor sets that can be allocated from the pool.
        SetBits(maxSets, 15, 0);

        // POOL_SIZE_COUNT [23:16] The number of pool size structs.
        SetBits(poolSizeCount, 23, 16);
    }
};

static const size_t RMT_POOL_SIZE_DESC_BYTES_SIZE = 32 / 8; // 32-bits
// Structure describing a Pool Size
struct RMT_POOL_SIZE_DESC : RMT_TOKEN_DATA
{
    uint8 bytes[RMT_POOL_SIZE_DESC_BYTES_SIZE];

    RMT_POOL_SIZE_DESC(RMT_DESCRIPTOR_TYPE type, uint16 numDescriptors)
    {
        sizeInBytes = sizeof(bytes);
        pByteData = &bytes[0];

        // TYPE [15:0] Descriptor type this pool can hold. See RMT_DESCRIPTOR_TYPE.
        SetBits(type, 15, 0);

        // NUM_DESCRIPTORS [31:16] Number of descriptors to be allocated by this pool.
        SetBits(numDescriptors, 31, 16);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// RMT_RESOURCE_TYPE_CMD_ALLOCATOR
static const size_t RMT_CMD_ALLOCATOR_DESC_BYTES_SIZE = 352 / 8; // 352-bits
struct RMT_RESOURCE_TYPE_CMD_ALLOCATOR_TOKEN : RMT_TOKEN_DATA
{
    uint8 bytes[RMT_CMD_ALLOCATOR_DESC_BYTES_SIZE];

    // Initializes the token fields
    RMT_RESOURCE_TYPE_CMD_ALLOCATOR_TOKEN(
        RMT_CMD_ALLOCATOR_CREATE_FLAGS flags,
        RMT_HEAP_TYPE                  cmdDataPreferredHeap,
        uint64                         cmdDataAllocSize,
        uint64                         cmdDataSuballocSize,
        RMT_HEAP_TYPE                  embeddedDataPreferredHeap,
        uint64                         embeddedDataAllocSize,
        uint64                         embeddedDataSuballocSize,
        RMT_HEAP_TYPE                  gpuScratchMemPreferredHeap,
        uint64                         gpuScratchMemAllocSize,
        uint64                         gpuScratchMemSuballocSize)
    {
        sizeInBytes = sizeof(bytes);
        pByteData = &bytes[0];

        // FLAGS [3:0] Describes the creation flags for the command allocator. See RMT_CMD_ALLOCATOR_CREATE_FLAGS.
        SetBits(flags.byteVal, 3, 0);

        // CMD_DATA_PREFERRED_HEAP [7:4] The preferred allocation heap for executable command data
        SetBits(cmdDataPreferredHeap, 7, 4);

        // CMD_DATA_ALLOC_SIZE [63:8] Size of the base memory allocations the command allocator will make for executable
        //                              command data. Expressed as 4kB chunks
        SetBits(cmdDataAllocSize, 63, 8);

        // CMD_DATA_SUBALLOC_SIZE [119:64] Size, in bytes, of the chunks the command allocator will give to command
        //                                   buffers for executable command data. Expressed as 4kB chunks
        SetBits(cmdDataSuballocSize, 119, 64);

        // EMBEDDED_DATA_PREFERRED_HEAP [123:120] The preferred allocation heap for embedded command data
        SetBits(embeddedDataPreferredHeap, 123, 120);

        // EMBEDDED_DATA_ALLOC_SIZE [179:124] Size, in bytes, of the base memory allocations the command allocator will
        //                                      make for embedded command data. Expressed as 4kB chunks
        SetBits(embeddedDataAllocSize, 179, 124);

        // EMBEDDED_DATA_SUBALLOC_SIZE [235:180] Size, in bytes, of the chunks the command allocator will give to
        //                                         command buffers for embedded command data. Expressed as 4kB chunks
        SetBits(embeddedDataSuballocSize, 235, 180);

        // GPU_SCRATCH_MEM_PREFERRED_HEAP [239:236] The preferred allocation heap for GPU scratch memory.
        SetBits(gpuScratchMemPreferredHeap, 239, 236);

        // GPU_SCRATCH_MEM_ALLOC_SIZE [295:240] Size, in bytes, of the base memory allocations the command allocator
        //                                        will make for GPU scratch memory. Expressed as 4kB chunks
        SetBits(gpuScratchMemAllocSize, 295, 240);

        // GPU_SCRATCH_MEM_SUBALLOC_SIZE [351:296] Size, in bytes, of the chunks the command allocator will give to
        //                                           command buffers for GPU scratch memory. Expressed as 4kB chunks
        SetBits(gpuScratchMemSuballocSize, 351, 296);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// RMT_RESOURCE_TYPE_MISC_INTERNAL
static const size_t RMT_MISC_INTERNAL_DESC_BYTES_SIZE = 8 / 8; // 8-bits
struct RMT_RESOURCE_TYPE_MISC_INTERNAL_TOKEN : RMT_TOKEN_DATA
{
    uint8 bytes[RMT_MISC_INTERNAL_DESC_BYTES_SIZE];

    // Initializes the token fields
    RMT_RESOURCE_TYPE_MISC_INTERNAL_TOKEN(RMT_MISC_INTERNAL_TYPE type)
    {
        sizeInBytes = sizeof(bytes);
        pByteData = &bytes[0];

        // TYPE [7:0] The type of internal allocation.
        SetBits(type, 7, 0);
    }
};

} // namespace DevDriver
