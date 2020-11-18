/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "util/rmtTokens.h"
#include "util/rmtResourceDescriptions.h"
#include "core/eventDefs.h"
#include "palImage.h"
#include "pal.h"

namespace Pal
{

// =====================================================================================================================
static uint32 PalToRmtImgCreateFlags(
    ImageCreateFlags palFlags)
{
    DevDriver::RMT_IMAGE_CREATE_FLAGS rmtFlags = {};
    rmtFlags.INVARIANT = palFlags.invariant;
    rmtFlags.CLONEABLE = palFlags.cloneable;
    rmtFlags.SHAREABLE = palFlags.shareable;
    rmtFlags.FLIPPABLE = palFlags.flippable;
    rmtFlags.STEREO = palFlags.stereo;
    rmtFlags.CUBEMAP = palFlags.cubemap;
    rmtFlags.PRT = palFlags.prt;
    rmtFlags.RESERVED_0 = 0;
    rmtFlags.READ_SWIZZLE_EQUATIONS = palFlags.needSwizzleEqs;
    rmtFlags.PER_SUBRESOURCE_INIT = palFlags.perSubresInit;
    rmtFlags.SEPARATE_DEPTH_ASPECT_RATIO = palFlags.separateDepthAspectInit;
    rmtFlags.COPY_FORMATS_MATCH = 0;
    rmtFlags.REPETITIVE_RESOLVE = palFlags.repetitiveResolve;
    rmtFlags.PREFR_SWIZZLE_EQUATIONS = palFlags.preferSwizzleEqs;
    rmtFlags.FIXED_TILE_SWIZZLE = palFlags.fixedTileSwizzle;
    rmtFlags.VIDEO_REFERENCE_ONLY = palFlags.videoReferenceOnly;
    rmtFlags.OPTIMAL_SHAREABLE = palFlags.optimalShareable;
    rmtFlags.SAMPLE_LOCATIONS_ALWAYS_KNOWN = palFlags.sampleLocsAlwaysKnown;
    rmtFlags.FULL_RESOLVE_DESTINATION_ONLY = palFlags.fullResolveDstOnly;
    rmtFlags.RESERVED = 0;

    return rmtFlags.u32Val;
}

// =====================================================================================================================
static uint16 PalToRmtImgUsageFlags(
    ImageUsageFlags palUsageFlags)
{
    DevDriver::RMT_IMAGE_USAGE_FLAGS rmtUsageFlags = {};
    rmtUsageFlags.SHADER_READ = palUsageFlags.shaderRead;
    rmtUsageFlags.SHADER_WRITE = palUsageFlags.shaderWrite;
    rmtUsageFlags.RESOLVE_SOURCE = palUsageFlags.resolveSrc;
    rmtUsageFlags.RESOLVE_DESTINATION = palUsageFlags.resolveDst;
    rmtUsageFlags.COLOR_TARGET = palUsageFlags.colorTarget;
    rmtUsageFlags.DEPTH_STENCIL = palUsageFlags.depthStencil;
    rmtUsageFlags.NO_STENCIL_SHADER_READ = palUsageFlags.noStencilShaderRead;
    rmtUsageFlags.HI_Z_NEVER_INVALID  = palUsageFlags.hiZNeverInvalid;
    rmtUsageFlags.DEPTH_AS_Z24 = palUsageFlags.depthAsZ24;
    rmtUsageFlags.FIRST_SHADER_WRITABLE_MIP = palUsageFlags.firstShaderWritableMip;
    rmtUsageFlags.CORNER_SAMPLING = palUsageFlags.cornerSampling;
#if GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION >= GPUOPEN_RMV_1_1_VERSION
    rmtUsageFlags.VRS_DEPTH = palUsageFlags.vrsDepth;
#endif
    rmtUsageFlags.RESERVED = 0;

    return rmtUsageFlags.u16Val;
}

// =====================================================================================================================
static DevDriver::RMT_IMAGE_TYPE PalToRmtImageType(
    ImageType palType)
{
    DevDriver::RMT_IMAGE_TYPE retType = DevDriver::RMT_IMAGE_TYPE_1D;
    switch(palType)
    {
    case ImageType::Tex1d:
        retType = DevDriver::RMT_IMAGE_TYPE_1D;
        break;

    case ImageType::Tex2d:
        retType = DevDriver::RMT_IMAGE_TYPE_2D;
        break;

    case ImageType::Tex3d:
        retType = DevDriver::RMT_IMAGE_TYPE_3D;
        break;

    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    return retType;
}

// =====================================================================================================================
static DevDriver::RMT_SWIZZLE PalToRmtSwizzle(
    ChannelSwizzle palSwizzle)
{
    DevDriver::RMT_SWIZZLE retSwizzle = DevDriver::RMT_SWIZZLE_0;

    switch(palSwizzle)
    {
    case ChannelSwizzle::Zero:
        retSwizzle = DevDriver::RMT_SWIZZLE_0;
        break;

    case ChannelSwizzle::One:
        retSwizzle = DevDriver::RMT_SWIZZLE_1;
        break;

    case ChannelSwizzle::X:
        retSwizzle = DevDriver::RMT_SWIZZLE_X;
        break;

    case ChannelSwizzle::Y:
        retSwizzle = DevDriver::RMT_SWIZZLE_Y;
        break;

    case ChannelSwizzle::Z:
        retSwizzle = DevDriver::RMT_SWIZZLE_Z;
        break;

    case ChannelSwizzle::W:
        retSwizzle = DevDriver::RMT_SWIZZLE_W;
        break;

    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    return retSwizzle;
}

// =====================================================================================================================
static DevDriver::RMT_NUM_FORMAT PalToRmtNumFormat(
    ChNumFormat palFormat)
{
    DevDriver::RMT_NUM_FORMAT retFormat = DevDriver::RMT_NUM_FORMAT::RMT_FORMAT_UNDEFINED;

    // In most cases the value is the same between PAL and RMT, we will only look for the exceptions
    // @TODO - Add static asserts to make sure this remains true.
    switch(palFormat)
    {

    case ChNumFormat::AstcLdr10x8_Srgb:
        // @TODO - This format is not in the current RMT spec, setting to the closest match for now.
        retFormat = DevDriver::RMT_FORMAT_ASTCLDR10X8_UNORM;
        break;

    case ChNumFormat::AstcLdr10x10_Srgb:
        // @TODO - This format is not in the current RMT spec, setting to the closest match for now.
        retFormat = DevDriver::RMT_FORMAT_ASTCLDR10X10_UNORM;
        break;

    default:
        retFormat = static_cast<DevDriver::RMT_NUM_FORMAT>(palFormat);
        break;
    }

    return retFormat;
}

// =====================================================================================================================
static DevDriver::RMT_IMAGE_FORMAT PalToRmtImageFormat(
    SwizzledFormat palFormat)
{
    DevDriver::RMT_IMAGE_FORMAT retFormat = {};

    retFormat.SWIZZLE_X = PalToRmtSwizzle(palFormat.swizzle.r);
    retFormat.SWIZZLE_Y = PalToRmtSwizzle(palFormat.swizzle.g);
    retFormat.SWIZZLE_Z = PalToRmtSwizzle(palFormat.swizzle.b);
    retFormat.SWIZZLE_W = PalToRmtSwizzle(palFormat.swizzle.a);
    retFormat.NUM_FORMAT = PalToRmtNumFormat(palFormat.format);

    return retFormat;
}

// =====================================================================================================================
static DevDriver::RMT_IMAGE_TILING_TYPE PalToRmtTilingType(
    ImageTiling palTiling)
{
    DevDriver::RMT_IMAGE_TILING_TYPE retType = DevDriver::RMT_IMAGE_TILING_TYPE_LINEAR;

    switch(palTiling)
    {
    case ImageTiling::Linear:
        retType = DevDriver::RMT_IMAGE_TILING_TYPE_LINEAR;
        break;

    case ImageTiling::Optimal:
        retType = DevDriver::RMT_IMAGE_TILING_TYPE_OPTIMAL;
        break;

    case ImageTiling::Standard64Kb:
        retType = DevDriver::RMT_IMAGE_TILING_TYPE_STANDARD_SWIZZLE;
        break;

    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    return retType;
}

// =====================================================================================================================
static DevDriver::RMT_IMAGE_TILING_OPT_MODE PalToRmtTilingOptMode(
    TilingOptMode palTilingOptMode)
{
    DevDriver::RMT_IMAGE_TILING_OPT_MODE retMode = DevDriver::RMT_IMAGE_TILING_OPT_MODE_BALANCED;

    switch(palTilingOptMode)
    {
    case TilingOptMode::Balanced:
        retMode = DevDriver::RMT_IMAGE_TILING_OPT_MODE_BALANCED;
        break;

    case TilingOptMode::OptForSpace:
        retMode = DevDriver::RMT_IMAGE_TILING_OPT_MODE_OPT_FOR_SPACE;
        break;

    case TilingOptMode::OptForSpeed:
        retMode = DevDriver::RMT_IMAGE_TILING_OPT_MODE_OPT_FOR_SPEED;
        break;

    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    return retMode;
}

// =====================================================================================================================
static DevDriver::RMT_IMAGE_METADATA_MODE PalToRmtMetadataMode(
    MetadataMode palMetadataMode)
{
    DevDriver::RMT_IMAGE_METADATA_MODE retMode = DevDriver::RMT_IMAGE_METADATA_MODE_DEFAULT;

    switch(palMetadataMode)
    {
    case MetadataMode::Default:
        retMode = DevDriver::RMT_IMAGE_METADATA_MODE_DEFAULT;
        break;

    case MetadataMode::ForceEnabled:
        retMode = DevDriver::RMT_IMAGE_METADATA_MODE_OPT_FOR_TEX_PREFETCH;
        break;

    case MetadataMode::Disabled:
        retMode = DevDriver::RMT_IMAGE_METADATA_MODE_DISABLED;
        break;

    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    return retMode;
}

// =====================================================================================================================
static DevDriver::RMT_QUERY_HEAP_TYPE PalToRmtQueryHeapType(
    QueryPoolType palType)
{
    DevDriver::RMT_QUERY_HEAP_TYPE retType = DevDriver::RMT_QUERY_HEAP_TYPE_OCCLUSION;

    switch(palType)
    {
    case QueryPoolType::Occlusion:
        retType = DevDriver::RMT_QUERY_HEAP_TYPE_OCCLUSION;
        break;

    case QueryPoolType::PipelineStats:
        retType = DevDriver::RMT_QUERY_HEAP_TYPE_PIPELINE_STATS;
        break;

    case QueryPoolType::StreamoutStats:
        retType = DevDriver::RMT_QUERY_HEAP_TYPE_STREAMOUT_STATS;
        break;
    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    return retType;
}

// =====================================================================================================================
static DevDriver::RMT_VIDEO_ENCODER_TYPE PalToRmtEncoderType(
    VideoEncodeCodec palType)
{
    DevDriver::RMT_VIDEO_ENCODER_TYPE retType = DevDriver::RMT_VIDEO_ENCODER_TYPE_H264;

    switch(palType)
    {
    case VideoEncodeCodec::H264:
        retType = DevDriver::RMT_VIDEO_ENCODER_TYPE_H264;
        break;

    case VideoEncodeCodec::H265:
        retType = DevDriver::RMT_VIDEO_ENCODER_TYPE_H265;
        break;

    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    return retType;
}

// =====================================================================================================================
static DevDriver::RMT_VIDEO_DECODER_TYPE PalToRmtDecoderType(
    VideoDecodeType palType)
{
    DevDriver::RMT_VIDEO_DECODER_TYPE retType = DevDriver::RMT_VIDEO_DECODER_TYPE_H264;

    switch(palType)
    {
    case VideoDecodeType::H264:
        retType = DevDriver::RMT_VIDEO_DECODER_TYPE_H264;
        break;

    case VideoDecodeType::Vc1:
        retType = DevDriver::RMT_VIDEO_DECODER_TYPE_VC1;
        break;

    case VideoDecodeType::Mpeg2Idct:
        retType = DevDriver::RMT_VIDEO_DECODER_TYPE_MPEG2IDCT;
        break;

    case VideoDecodeType::Mpeg2Vld:
        retType = DevDriver::RMT_VIDEO_DECODER_TYPE_MPEG2VLD;
        break;

    case VideoDecodeType::Mpeg4:
        retType = DevDriver::RMT_VIDEO_DECODER_TYPE_MPEG4;
        break;

    case VideoDecodeType::Wmv9:
        retType = DevDriver::RMT_VIDEO_DECODER_TYPE_WMV9;
        break;

    case VideoDecodeType::Mjpeg:
        retType = DevDriver::RMT_VIDEO_DECODER_TYPE_MJPEG;
        break;

    case VideoDecodeType::Hevc:
        retType = DevDriver::RMT_VIDEO_DECODER_TYPE_HVEC;
        break;

    case VideoDecodeType::Vp9:
        retType = DevDriver::RMT_VIDEO_DECODER_TYPE_VP9;
        break;

    case VideoDecodeType::Hevc10Bit:
        retType = DevDriver::RMT_VIDEO_DECODER_TYPE_HEVC10BIT;
        break;

    case VideoDecodeType::Vp910Bit:
        retType = DevDriver::RMT_VIDEO_DECODER_TYPE_VP910BIT;
        break;

    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    return retType;
}

// =====================================================================================================================
static DevDriver::RMT_DESCRIPTOR_TYPE PalToRmtDescriptorType(
    ResourceDescriptionDescriptorType palType)
{
    DevDriver::RMT_DESCRIPTOR_TYPE retType = DevDriver::RMT_DESCRIPTOR_TYPE_CSV_SRV_UAV;

    switch(palType)
    {
    case ResourceDescriptionDescriptorType::ConstantBufferShaderResourceUAV:
        retType = DevDriver::RMT_DESCRIPTOR_TYPE_CSV_SRV_UAV;
        break;

    case ResourceDescriptionDescriptorType::Sampler:
        retType = DevDriver::RMT_DESCRIPTOR_TYPE_SAMPLER;
        break;

    case ResourceDescriptionDescriptorType::RenderTargetView:
        retType = DevDriver::RMT_DESCRIPTOR_TYPE_RTV;
        break;

    case ResourceDescriptionDescriptorType::DepthStencilView:
        retType = DevDriver::RMT_DESCRIPTOR_TYPE_DSV;
        break;

    case ResourceDescriptionDescriptorType::CombinedImageSampler:
        retType = DevDriver::RMT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        break;

    case ResourceDescriptionDescriptorType::SampledImage:
        retType = DevDriver::RMT_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        break;

    case ResourceDescriptionDescriptorType::StorageImage:
        retType = DevDriver::RMT_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        break;

    case ResourceDescriptionDescriptorType::UniformTexelBuffer:
        retType = DevDriver::RMT_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
        break;

    case ResourceDescriptionDescriptorType::StorageTexelBuffer:
        retType = DevDriver::RMT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
        break;

    case ResourceDescriptionDescriptorType::UniformBuffer:
        retType = DevDriver::RMT_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        break;

    case ResourceDescriptionDescriptorType::StorageBuffer:
        retType = DevDriver::RMT_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        break;

    case ResourceDescriptionDescriptorType::UniformBufferDynamic:
        retType = DevDriver::RMT_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        break;

    case ResourceDescriptionDescriptorType::StorageBufferDynamic:
        retType = DevDriver::RMT_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
        break;

    case ResourceDescriptionDescriptorType::InputAttachment:
        retType = DevDriver::RMT_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        break;

    case ResourceDescriptionDescriptorType::InlineUniformBlock:
        retType = DevDriver::RMT_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK;
        break;

    case ResourceDescriptionDescriptorType::AccelerationStructure:
        retType = DevDriver::RMT_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE;
        break;

    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    return retType;
}

// =====================================================================================================================
static DevDriver::RMT_CMD_ALLOCATOR_CREATE_FLAGS PalToRmtCmdAllocatorCreateFlags(
    CmdAllocatorCreateFlags palFlags)
{
    DevDriver::RMT_CMD_ALLOCATOR_CREATE_FLAGS retFlags;

    retFlags.AUTO_MEMORY_REUSE = palFlags.autoMemoryReuse;
    retFlags.DISABLE_BUSY_CHUNK_TRACKING = palFlags.disableBusyChunkTracking;
    retFlags.THREAD_SAFE = palFlags.threadSafe;
    retFlags.reserved = 0;

    return retFlags;
}

// =====================================================================================================================
static DevDriver::RMT_HEAP_TYPE PalToRmtHeapType(
    GpuHeap palType)
{
    DevDriver::RMT_HEAP_TYPE retType = DevDriver::RMT_HEAP_TYPE_LOCAL;

    switch(palType)
    {
    case GpuHeap::GpuHeapLocal:
        retType = DevDriver::RMT_HEAP_TYPE_LOCAL;
        break;

    case GpuHeap::GpuHeapInvisible:
        retType = DevDriver::RMT_HEAP_TYPE_INVISIBLE;
        break;

    case GpuHeap::GpuHeapGartUswc:
        retType = DevDriver::RMT_HEAP_TYPE_GART_USWC;
        break;

    case GpuHeap::GpuHeapGartCacheable:
        retType = DevDriver::RMT_HEAP_TYPE_GART_CACHEABLE;
        break;

    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    return retType;
}

// =====================================================================================================================
static DevDriver::RMT_MISC_INTERNAL_TYPE PalToRmtMiscInternalType(
    MiscInternalAllocType palType)
{
    // The Pal type matches the RMT type
    // @TODO - Add static asserts
    return static_cast<DevDriver::RMT_MISC_INTERNAL_TYPE>(palType);
}

// =====================================================================================================================
static DevDriver::RMT_MISC_EVENT_TYPE PalToRmtMiscEventType(
    MiscEventType palType)
{
     DevDriver::RMT_MISC_EVENT_TYPE retType = DevDriver::RMT_MISC_EVENT_TYPE_SUBMIT_GFX;

    switch(palType)
    {
    case MiscEventType::SubmitGfx:
        retType = DevDriver::RMT_MISC_EVENT_TYPE_SUBMIT_GFX;
        break;

    case MiscEventType::SubmitCompute:
        retType = DevDriver::RMT_MISC_EVENT_TYPE_SUBMIT_COMPUTE;
        break;

    case MiscEventType::Present:
        retType = DevDriver::RMT_MISC_EVENT_TYPE_PRESENT;
        break;

    case MiscEventType::InvalidateRanges:
        retType = DevDriver::RMT_MISC_EVENT_TYPE_INVALIDATE_RANGES;
        break;

    case MiscEventType::FlushMappedMemoryRanges:
        retType = DevDriver::RMT_MISC_EVENT_TYPE_FLUSH_MAPPED_MEMORY_RANGED;
        break;

    case MiscEventType::Trim:
        retType = DevDriver::RMT_MISC_EVENT_TYPE_TRIM_MEMORY;
        break;

    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    return retType;
}

// =====================================================================================================================
static DevDriver::RMT_RESOURCE_TYPE PalToRmtResourceType(
    ResourceType palType)
{
    DevDriver::RMT_RESOURCE_TYPE retType = DevDriver::RMT_RESOURCE_TYPE_IMAGE;

    switch(palType)
    {
    case ResourceType::Image:
        retType = DevDriver::RMT_RESOURCE_TYPE_IMAGE;
        break;
    case ResourceType::Buffer:
        retType = DevDriver::RMT_RESOURCE_TYPE_BUFFER;
        break;
    case ResourceType::Pipeline:
        retType = DevDriver::RMT_RESOURCE_TYPE_PIPELINE;
        break;
    case ResourceType::Heap:
        retType = DevDriver::RMT_RESOURCE_TYPE_HEAP;
        break;
    case ResourceType::GpuEvent:
        retType = DevDriver::RMT_RESOURCE_TYPE_GPU_EVENT;
        break;
    case ResourceType::BorderColorPalette:
        retType = DevDriver::RMT_RESOURCE_TYPE_BORDER_COLOR_PALETTE;
        break;
    case ResourceType::IndirectCmdGenerator:
        retType = DevDriver::RMT_RESOURCE_TYPE_INDIRECT_CMD_GENERATOR;
        break;
    case ResourceType::MotionEstimator:
        retType = DevDriver::RMT_RESOURCE_TYPE_MOTION_ESTIMATOR;
        break;
    case ResourceType::PerfExperiment:
        retType = DevDriver::RMT_RESOURCE_TYPE_PERF_EXPERIMENT;
        break;
    case ResourceType::QueryPool:
        retType = DevDriver::RMT_RESOURCE_TYPE_QUERY_HEAP;
        break;
    case ResourceType::VideoEncoder:
        retType = DevDriver::RMT_RESOURCE_TYPE_VIDEO_ENCODER;
        break;
    case ResourceType::VideoDecoder:
        retType = DevDriver::RMT_RESOURCE_TYPE_VIDEO_DECODER;
        break;
    case ResourceType::Timestamp:
        retType = DevDriver::RMT_RESOURCE_TYPE_TIMESTAMP;
        break;
    case ResourceType::DescriptorHeap:
        retType = DevDriver::RMT_RESOURCE_TYPE_DESCRIPTOR_HEAP;
        break;
    case ResourceType::DescriptorPool:
        retType = DevDriver::RMT_RESOURCE_TYPE_DESCRIPTOR_POOL;
        break;
    case ResourceType::CmdAllocator:
        retType = DevDriver::RMT_RESOURCE_TYPE_CMD_ALLOCATOR;
        break;
    case ResourceType::MiscInternal:
        retType = DevDriver::RMT_RESOURCE_TYPE_MISC_INTERNAL;
        break;

    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    return retType;
}

// =====================================================================================================================
static DevDriver::RMT_ENGINE_TYPE PalToRmtEngineType(
    EngineType palType)
{
    DevDriver::RMT_ENGINE_TYPE retType = DevDriver::RMT_ENGINE_TYPE_UNIVERSAL;

    switch(palType)
    {
    case EngineTypeUniversal:
    retType = DevDriver::RMT_ENGINE_TYPE_UNIVERSAL;
    break;

    case EngineTypeCompute:
        retType = DevDriver::RMT_ENGINE_TYPE_COMPUTE;
        break;

    case EngineTypeDma:
        retType = DevDriver::RMT_ENGINE_TYPE_DMA;
        break;

    case EngineTypeTimer:
        retType = DevDriver::RMT_ENGINE_TYPE_TIMER;
        break;

    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    return retType;
}

} // Pal

